"""Download graphs, grammars, and category configs from FLPQ_Data into a directory."""

import argparse
import logging
import re
import shutil
import sys
from collections.abc import Iterable, Iterator
from pathlib import Path

import flpq_data

# Query used in the config of a category whose graphs carry several queries.
DEFAULT_QUERIES = {"rdf": "nested_parentheses_subClassOf_type"}
INDEXED_LABEL = re.compile(r"^(.+)_(\d+)$")


def graph_query(category: str, graph_name: str) -> str:
    """The CFPQ query of the graph used in the category config."""
    queries = {query.name for query in flpq_data.graph_info(graph_name).queries if query.query_class == "cfpq"}
    if len(queries) == 1:
        return queries.pop()
    if DEFAULT_QUERIES.get(category) in queries:
        return DEFAULT_QUERIES[category]
    raise ValueError(f"{graph_name} needs an entry in DEFAULT_QUERIES[{category!r}], queries: {sorted(queries)}")


def indexed_bases(grammars: Path) -> set[str]:
    """Terminal bases the grammars of a graph use as indexed (`<base>_i`)."""
    bases = set()
    for file in grammars.iterdir():
        bases.update(re.findall(r"\b(\w+)_i\b", file.read_text()))
    return bases


def g_lines(edges: Iterable[tuple[int, str, int]], bases: set[str]) -> Iterator[str]:
    """.g lines with reverse edges; `X_N` is written as indexed `X_i N` only when a grammar uses `X_i`."""
    for u, label, v in edges:
        match = INDEXED_LABEL.match(label)
        if match and match[1] in bases:
            base, index = match.groups()
            yield f"{u} {v} {base}_i {index}\n{v} {u} {base}_r_i {index}\n"
        else:
            yield f"{u} {v} {label}\n{v} {u} {label}_r\n"


def graph_path(directory: Path, category: str, graph_name: str) -> Path:
    return directory / "graphs" / category / f"{graph_name}.g"


def grammar_dir(directory: Path, category: str, graph_name: str) -> Path:
    return directory / "grammars" / category / graph_name


def download_graph(directory: Path, graph_name: str, progress: str, verbose: bool) -> None:
    """Save one graph as .g and copy the grammars of all its queries."""
    category = flpq_data.graph_info(graph_name).category
    same_line = sys.stdout.isatty() and not verbose

    def show(stage: str) -> None:
        line = f"{progress}{graph_name}: {stage}"
        print(f"\r{line:<72}" if same_line else line, end="" if same_line and stage != "done" else "\n", flush=True)

    show("downloading")
    source = flpq_data.graph_dir(graph_name)

    show("copying grammars")
    grammars = grammar_dir(directory, category, graph_name)
    grammars.mkdir(parents=True, exist_ok=True)
    for file in (source / "queries" / "cfpq").glob("*/*"):
        if file.suffix in {".cnf", ".rsm"}:
            shutil.copy(file, grammars)

    show("converting")
    graph = graph_path(directory, category, graph_name)
    graph.parent.mkdir(parents=True, exist_ok=True)
    temporary = graph.with_name(f".{graph.name}.part")
    try:
        with temporary.open("w") as output:
            output.writelines(g_lines(flpq_data.iter_edges_from_mtx_dir(source / "graph"), indexed_bases(grammars)))
        temporary.replace(graph)
    finally:
        temporary.unlink(missing_ok=True)
    show("done")


def write_category_config(directory: Path, category: str) -> Path:
    """Write a benchmark config with the FLPQ_Data reachable-pair counts (-1 when not published yet)."""
    lines = []
    for graph_name in flpq_data.categories()[category]:
        graph = graph_path(directory, category, graph_name)
        grammar = grammar_dir(directory, category, graph_name) / f"{graph_query(category, graph_name)}.cnf"
        for path in (graph, grammar):
            if not path.is_file():
                raise FileNotFoundError(path)
        rows = flpq_data.reachable_pairs(graph=graph_name, grammar=grammar.name, query_class="cfpq")
        count = rows[0]["num_reachable_pairs"] if rows else None
        lines.append(f"{graph},{grammar},{-1 if count is None else count}\n")
    destination = directory / "configs" / f"{category}.csv"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text("".join(lines))
    return destination


def main() -> None:
    """Download one graph or whole categories from CLI arguments."""
    categories = flpq_data.categories()
    category_names = (*categories, "all")
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out-dir", required=True, type=Path, help="output directory")
    target = parser.add_mutually_exclusive_group(required=True)
    target.add_argument("--graph", choices=flpq_data.graph_names(), metavar="graph", help="graph name, e.g. bzip")
    target.add_argument(
        "--category", choices=category_names, metavar="category", help=f"one of: {', '.join(category_names)}"
    )
    parser.add_argument("-v", "--verbose", action="store_true", help="show all log messages, including DEBUG")
    args = parser.parse_args()
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.WARNING)

    if args.graph:
        download_graph(args.out_dir, args.graph, "", args.verbose)
        return
    selected = list(categories) if args.category == "all" else [args.category]
    graph_names = [name for category in selected for name in categories[category]]
    print(f"Downloading {args.category}: {len(graph_names)} graphs")
    for index, graph_name in enumerate(graph_names, 1):
        download_graph(args.out_dir, graph_name, f"[{index}/{len(graph_names)}] ", args.verbose)
    for category in selected:
        print(f"Wrote {write_category_config(args.out_dir, category)}")


if __name__ == "__main__":
    main()
