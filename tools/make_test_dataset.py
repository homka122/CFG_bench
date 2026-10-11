"""Build the small test dataset in tests/data: connected samples of a few FLPQ_Data graphs per category.

Edges whose label the grammar does not use are dropped first. A sample is grown by BFS from random
vertices of the undirected graph until the edges between the visited vertices reach the budget of the
graph; the vertices are renumbered in BFS order and the indices of indexed labels (`load_N`, `call_N`)
are renumbered from 0 with one map for all labels. The sampling is deterministic for a fixed seed, so
the expected reachable-pair counts in GRAPHS stay valid.

Start vertices are written by `pairs_extractor --start-only` into `<out-dir>/start_nodes`; a config row
gets its file as the fourth column when it exists, so run the generator again after the extractor.
"""

import argparse
import random
import re
import shutil
import sys
from dataclasses import dataclass
from pathlib import Path

import flpq_data
import numpy as np

sys.path.insert(0, str(Path(__file__).parent))
from download_graph import INDEXED_LABEL, g_lines, graph_query, indexed_bases

SEED = 2026


@dataclass(frozen=True)
class Sample:
    edges: int  # budget of sampled edges without the reverse ones
    pairs: int | None = None  # expected reachable pairs, None until measured


# budgets: the largest of 500 * 2^k (up to 32000) where every algorithm finishes in 3 s
GRAPHS: dict[str, dict[str, Sample]] = {
    "c_alias_analysis": {
        "init": Sample(32_000, 58_570),
        "mm": Sample(32_000, 78_606),
        "block": Sample(32_000, 73_886),
    },
    "rdf": {
        "taxonomy": Sample(32_000, 6_375),
        "go": Sample(32_000, 177_811),
        "eclass": Sample(32_000, 19_016),
    },
    "java_points_to": {
        "lusearch": Sample(2_000, 362),
        "eclipse": Sample(2_000, 549),
        "gson": Sample(2_000, 808),
    },
    "context_sensitive_data_flow": {
        "xz": Sample(4_000, 28_156),
        "nab": Sample(4_000, 266_875),
        "leela": Sample(4_000, 18_828),
    },
    "field_sensitive_alias": {
        "xz_field_sensitive_alias": Sample(4_000, 205_164),
        "nab_field_sensitive_alias": Sample(2_000, 62_241),
        "leela_field_sensitive_alias": Sample(2_000, 113_744),
    },
}


def read_edges(graph: Path) -> tuple[np.ndarray, np.ndarray, np.ndarray, list[str]]:
    """Edges of an MTX directory as arrays `u`, `v`, `label_id` and the label names."""
    us, vs, label_ids, labels = [], [], [], []
    for file in sorted(graph.glob("*.mtx")):
        with file.open() as source:
            for _ in range(2):
                source.readline()
            nnz = int(source.readline().split()[2])
            data = np.fromstring(source.read(), dtype=np.int64, sep=" ").reshape(nnz, 2)
        us.append(data[:, 0])
        vs.append(data[:, 1])
        label_ids.append(np.full(nnz, len(labels), dtype=np.int32))
        labels.append(flpq_data.filename_to_label(file.name))
    return np.concatenate(us), np.concatenate(vs), np.concatenate(label_ids), labels


def bfs_order(u: np.ndarray, v: np.ndarray, budget: int, rng: random.Random, fanout: int) -> np.ndarray:
    """Visit rank of every vertex (-1 if not visited); visits until the induced edges reach the budget.

    A vertex enqueues at most `fanout` random unvisited neighbors, so a hub (an RDF class with thousands
    of `type` edges) does not fill the whole sample with its star.
    """
    node_count = int(max(u.max(), v.max())) + 1 if len(u) else 0
    # CSR of the undirected graph; every edge appears once from each endpoint
    source = np.concatenate([u, v])
    target = np.concatenate([v, u])
    neighbors = target[np.argsort(source, kind="stable")]
    offsets = np.zeros(node_count + 1, dtype=np.int64)
    np.cumsum(np.bincount(source, minlength=node_count), out=offsets[1:])

    rank = np.full(node_count, -1, dtype=np.int64)
    visited = 0
    induced = 0

    def visit(x: int) -> None:
        nonlocal visited, induced
        rank[x] = visited
        visited += 1
        # every edge is counted once, when its second endpoint is visited
        induced += int(np.count_nonzero(rank[neighbors[offsets[x] : offsets[x + 1]]] != -1))

    candidates = np.flatnonzero(offsets[1:] > offsets[:-1]).tolist()
    rng.shuffle(candidates)
    while induced < budget and candidates:
        seed = candidates.pop()
        if rank[seed] != -1:
            continue
        visit(seed)
        queue = [seed]
        head = 0
        while head < len(queue) and induced < budget:
            near = neighbors[offsets[queue[head]] : offsets[queue[head] + 1]]
            head += 1
            new = np.unique(near[rank[near] == -1]).tolist()
            rng.shuffle(new)
            for x in new[:fanout]:
                if induced >= budget:
                    break
                visit(x)
                queue.append(x)
    return rank


def sample_graph(data: tuple, budget: int, rng: random.Random, fanout: int = 4):
    """Edges `(new_u, label, new_v)` of the sample and the vertex map `rank` (-1 for dropped vertices)."""
    u, v, label_id, labels = data
    rank = bfs_order(u, v, budget, rng, fanout)
    edge_rank = np.maximum(rank[u], rank[v])
    edge_rank[(rank[u] == -1) | (rank[v] == -1)] = np.iinfo(np.int64).max
    # the induced edges of the first k BFS vertices are those with edge_rank < k; take the largest k within budget
    k = int(np.sort(edge_rank)[budget]) if budget < len(edge_rank) else int(rank.max()) + 1
    keep = edge_rank < k
    rank[rank >= k] = -1
    edges = [(int(a), labels[c], int(b)) for a, c, b in zip(rank[u][keep], label_id[keep], rank[v][keep])]
    return edges, rank


def grammar_labels(data: tuple, grammar: Path, bases: set[str]) -> tuple:
    """Only the edges whose label (or its reverse) the grammar uses."""
    u, v, label_id, labels = data
    tokens = set(re.findall(r"\w+", grammar.read_text()))

    def used(label: str) -> bool:
        match = INDEXED_LABEL.match(label)
        if match and match[1] in bases:
            return True
        return label in tokens or f"{label}_r" in tokens

    keep = np.isin(label_id, [i for i, label in enumerate(labels) if used(label)])
    return u[keep], v[keep], label_id[keep], labels


def compact_indices(edges: list[tuple[int, str, int]], bases: set[str]) -> list[tuple[int, str, int]]:
    """Renumber the indices of indexed labels from 0, one map for all labels, so `load_N` and `store_N` agree."""
    indices = sorted(
        {int(match[2]) for _, label, _ in edges if (match := INDEXED_LABEL.match(label)) and match[1] in bases}
    )
    new_index = {index: position for position, index in enumerate(indices)}
    result = []
    for u, label, v in edges:
        match = INDEXED_LABEL.match(label)
        if match and match[1] in bases:
            label = f"{match[1]}_{new_index[int(match[2])]}"
        result.append((u, label, v))
    return result


def write_sample(out_dir: Path, category: str, graph_name: str, sample: Sample, data: tuple | None = None) -> str:
    """Write the sample graph and its grammars; return its config row."""
    source = flpq_data.graph_dir(graph_name)
    grammars = out_dir / "grammars" / category / graph_name
    grammars.mkdir(parents=True, exist_ok=True)
    for file in (source / "queries" / "cfpq").glob("*/*"):
        if file.suffix in {".cnf", ".rsm"}:
            shutil.copy(file, grammars)

    grammar = grammars / f"{graph_query(category, graph_name)}.cnf"
    bases = indexed_bases(grammars)
    data = grammar_labels(data or read_edges(source / "graph"), grammar, bases)
    rng = random.Random(f"{SEED}:{graph_name}")
    edges, rank = sample_graph(data, sample.edges, rng)
    graph = out_dir / "graphs" / category / f"{graph_name}.g"
    graph.parent.mkdir(parents=True, exist_ok=True)
    with graph.open("w") as output:
        output.writelines(g_lines(compact_indices(edges, bases), bases))
    print(f"{graph_name}: {len(edges)} edges, {int(rank.max()) + 1} vertices", flush=True)

    row = f"{graph},{grammar},{-1 if sample.pairs is None else sample.pairs}"
    # named like the output of `pairs_extractor --start-only`
    start_nodes = out_dir / "start_nodes" / f"{grammar.stem}_{graph_name}_start.result"
    if start_nodes.is_file():
        row += f",{start_nodes}"
    return row + "\n"


def build(out_dir: Path, categories: list[str]) -> None:
    for category in categories:
        rows = [write_sample(out_dir, category, graph_name, sample) for graph_name, sample in GRAPHS[category].items()]
        destination = out_dir / "configs" / f"{category}.csv"
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_text("".join(rows))
    configs = out_dir / "configs"
    if all((configs / f"{category}.csv").is_file() for category in GRAPHS):
        (configs / "all.csv").write_text("".join((configs / f"{category}.csv").read_text() for category in GRAPHS))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", type=Path, default=Path("tests/data"), help="output directory")
    parser.add_argument("--category", choices=(*GRAPHS, "all"), default="all", metavar="category")
    args = parser.parse_args()
    build(args.out_dir, list(GRAPHS) if args.category == "all" else [args.category])


if __name__ == "__main__":
    main()
