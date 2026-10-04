"""Download graphs, grammars, and dataset configs into a directory."""

import argparse
import logging
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

import cfpq_data

# fmt: off
DATASETS = {
    "c_alias": (
        "wc", "bzip", "pr", "ls", "gzip", "apache", "init", "mm", "ipc", "lib",
        "block", "arch", "crypto", "security", "sound", "net", "fs", "drivers",
        "postgre", "kernel",
    ),
    "rdf": (
        "generations", "travel", "skos", "univ", "foaf", "atom", "people",
        "biomedical", "pizza", "wine", "funding", "core", "pathways",
        "go_hierarchy", "enzyme", "geospecies", "go", "eclass",
        "taxonomy_hierarchy", "taxonomy",
    ),
    "java": (
        "gson", "sunflow", "lusearch", "luindex", "avrora", "mockito",
        "commons_io", "commons_lang3", "eclipse", "h2", "pmd", "xalan",
        "junit5", "batik", "fop", "tomcat", "guava", "jackson", "jython",
        "tradebeans", "tradesoap",
    ),
    "field_sensitive_alias": (
        "xz_field_sensitive_alias", "nab_field_sensitive_alias",
        "leela_field_sensitive_alias", "povray_field_sensitive_alias",
        "x264_field_sensitive_alias", "cactus_field_sensitive_alias",
        "parest_field_sensitive_alias", "perlbench_field_sensitive_alias",
        "imagick_field_sensitive_alias", "omnetpp_field_sensitive_alias",
    ),
    "context_sensitive_data_flow": (
        "xz", "nab", "leela", "x264", "parest", "imagick", "povray",
        "cactus", "omnetpp", "perlbench",
    ),
    "data_provenance": (
        "sampleproject", "wikipedia-provenance", "pluggy", "itsdangerous",
        "requests", "httpx", "click", "jinja", "flask", "fastapi",
        "celery", "scikit-learn", "sphinx", "pandas", "django", "zulip",
        "superset", "airflow",
    ),
    "name_resolution": (
        "jiaozi", "jsonpath", "shattered_pixel_dungeon", "libgdx",
    ),
    "uniprot": tuple(f"unigraph_{i}" for i in range(1, 11)),
}
# fmt: on

# Paths are relative to grammars/<dataset>/.
DEFAULT_GRAMMARS = {
    "c_alias": Path("wc/c_alias.cnf"),
    "context_sensitive_data_flow": Path("xz/vf.cnf"),
    "data_provenance": Path("sampleproject/prov_derivation.cnf"),
    "field_sensitive_alias": Path("xz_field_sensitive_alias/aa.cnf"),
    "java": Path("gson/java_points_to.cnf"),
    "name_resolution": Path("jiaozi/name_resolution.cnf"),
    "rdf": Path("atom/nested_parentheses_subClassOf_type.cnf"),
    "uniprot": Path("unigraph_1/unigraph_1.cnf"),
}

# Reachable vertex-pair counts for the selected grammar languages in CFPQ_Data 5.0.0.
# Source: https://formallanguageconstrainedpathquerying.github.io/CFPQ_Data/graphs/index.html
# None means the count is not available in the source tables.
REACHABLE_PAIR_COUNTS = {
    "c_alias": {
        "wc": 156,
        "bzip": 315,
        "pr": 385,
        "ls": 854,
        "gzip": 1458,
        "apache": 92806768,
        "init": 3783769,
        "mm": 3990305,
        "ipc": 5249389,
        "lib": 5276303,
        "block": 5351409,
        "arch": 5339563,
        "crypto": 5428237,
        "security": 5593387,
        "sound": 6085269,
        "net": 8833403,
        "fs": 9646475,
        "drivers": 18825025,
        "postgre": 90661446,
        "kernel": 16747731,
    },
    "rdf": {
        "generations": 12,
        "travel": 52,
        "skos": 30,
        "univ": 25,
        "foaf": 36,
        "atom": 6,
        "people": 51,
        "biomedical": 47,
        "pizza": 1356,
        "wine": 565,
        "funding": 58,
        "core": 204,
        "pathways": 884,
        "go_hierarchy": 588976,
        "enzyme": 396,
        "geospecies": 85,
        "go": 640316,
        "eclass": 90994,
        "taxonomy_hierarchy": 5351657,
        "taxonomy": 151706,
    },
    "java": {
        "gson": 56325,
        "sunflow": 35209,
        "lusearch": 43719,
        "luindex": 176051,
        "avrora": 192790,
        "mockito": 16169,
        "commons_io": 24020,
        "commons_lang3": 27553,
        "eclipse": 378989,
        "h2": 2611022,
        "pmd": 137120,
        "xalan": 1138776,
        "junit5": 129598,
        "batik": 868368,
        "fop": 1984072,
        "tomcat": 3792543,
        "guava": 26384496,
        "jackson": 3108775,
        "jython": 561720,
        "tradebeans": 34370090,
        "tradesoap": 34451130,
    },
    "field_sensitive_alias": {
        "xz_field_sensitive_alias": 205164,
        "nab_field_sensitive_alias": 262566,
        "leela_field_sensitive_alias": 3968276,
        "povray_field_sensitive_alias": 27219043,
        "x264_field_sensitive_alias": 5246565,
        "cactus_field_sensitive_alias": 37625324,
        "parest_field_sensitive_alias": 49415038,
        "perlbench_field_sensitive_alias": 851737865,
        "imagick_field_sensitive_alias": 369956094,
        "omnetpp_field_sensitive_alias": 158255766,
    },
    "context_sensitive_data_flow": {
        "xz": 358834,
        "nab": 739646,
        "leela": 662466,
        "x264": 20259480,
        "parest": 1342540,
        "imagick": 12687034,
        "povray": 34599413,
        "cactus": 47806209,
        "omnetpp": 8424500,
        "perlbench": 297504186,
    },
    "data_provenance": {
        "sampleproject": 342,
        "wikipedia-provenance": 1732,
        "pluggy": 880,
        "itsdangerous": 2912,
        "requests": 2524,
        "httpx": 4408,
        "click": 2710,
        "jinja": 7436,
        "flask": 8062,
        "fastapi": 8814,
        "celery": 25306,
        "scikit-learn": 217728,
        "sphinx": 304084,
        "pandas": 154646,
        "django": 615946,
        "zulip": 209908,
        "superset": 97460,
        "airflow": 91350,
    },
    "name_resolution": {
        "jiaozi": 14435,
        "jsonpath": 59764,
        "shattered_pixel_dungeon": 971998,
        "libgdx": None,
    },
    "uniprot": {
        "unigraph_1": 376578,
        "unigraph_2": 39888347,
        "unigraph_3": 52471840,
        "unigraph_4": 808091802,
        "unigraph_5": 2303590109,
        "unigraph_6": 1722963921,
        "unigraph_7": 2320964134,
        "unigraph_8": None,
        "unigraph_9": None,
        "unigraph_10": None,
    },
}


def indexed_bases(grammar_dir: Path) -> set[str]:
    """Terminal bases the grammars use as indexed (`<base>_i`)."""
    bases = set()
    for cnf in grammar_dir.glob("*.cnf"):
        bases.update(re.findall(r"\b(\w+)_i\b", cnf.read_text()))
    return bases


def mtx_dir_to_g(source_dir: Path, destination: Path, indexed_labels: set[str]) -> None:
    """Stream MatrixMarket edges into the benchmark's graph format."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(dir=destination.parent, prefix=f".{destination.name}.")
    try:
        with os.fdopen(fd, "w") as output:
            mtx_files = sorted(source_dir.glob("*.mtx"))
            if not mtx_files:
                raise ValueError(f"No MatrixMarket files in {source_dir}")
            labels = {mtx_file.stem for mtx_file in mtx_files}
            for mtx_file in mtx_files:
                label = mtx_file.stem
                base, separator, suffix = label.rpartition("_")
                indexed = bool(separator and suffix.isdecimal() and base in indexed_labels)
                label_base = base if indexed else label
                reverse_base = label_base[:-2] if label_base.endswith("_r") else f"{label_base}_r"
                terminal = f"{label_base}_i" if indexed else label_base
                reverse_terminal = f"{reverse_base}_i" if indexed else reverse_base
                reverse_label = f"{reverse_base}_{suffix}" if indexed else reverse_base
                index = f"\t{suffix}" if indexed else ""
                add_reverse = reverse_label not in labels
                with mtx_file.open() as source:
                    lines = (line.strip() for line in source if line.strip())
                    header = (next(lines, None), next(lines, None))
                    if header != (
                        "%%MatrixMarket matrix coordinate pattern general",
                        "%%GraphBLAS type bool",
                    ):
                        raise ValueError(f"Unexpected header in {mtx_file}")

                    dimensions = next(lines, "").split()
                    if len(dimensions) != 3:
                        raise ValueError(f"Invalid dimensions in {mtx_file}")
                    _rows, _cols, expected = map(int, dimensions)
                    count = 0
                    for line in lines:
                        u, v = map(int, line.split())
                        output.write(f"{u}\t{v}\t{terminal}{index}\n")
                        if add_reverse:
                            output.write(f"{v}\t{u}\t{reverse_terminal}{index}\n")
                        count += 1
                    if count != expected:
                        raise ValueError(f"{mtx_file} declares {expected} entries but has {count}")
        os.replace(temporary_name, destination)
    finally:
        Path(temporary_name).unlink(missing_ok=True)


def download_graph(directory: Path, graph_name: str, progress: str = "", verbose: bool = False) -> None:
    """Save one dataset graph as .g and copy its entire grammar directory."""
    dataset = next(name for name, graphs in DATASETS.items() if graph_name in graphs)
    interactive = sys.stdout.isatty() and not verbose

    def show(stage: str, done: bool = False) -> None:
        message = f"{progress}{graph_name}: {stage}"
        print(f"\r{message:<72}" if interactive else message, end="\n" if done or not interactive else "", flush=True)

    show("downloading")
    source = cfpq_data.download(graph_name)
    graph_dir = directory / "graphs" / dataset
    grammar_dir = directory / "grammars" / dataset / graph_name
    graph_dir.mkdir(parents=True, exist_ok=True)

    show("saving graph")
    mtx_dir_to_g(source / "graph", graph_dir / f"{graph_name}.g", indexed_bases(source / "grammar"))
    if (source / "grammar").is_dir():
        show("copying grammar")
        shutil.copytree(source / "grammar", grammar_dir, dirs_exist_ok=True)
    show("done", done=True)


def write_dataset_config(directory: Path, dataset: str) -> Path:
    """Write a benchmark config for graphs with published reachable-pair counts."""
    config_dir = directory / "configs"
    config_dir.mkdir(parents=True, exist_ok=True)
    destination = config_dir / f"{dataset}.csv"
    grammar = directory / "grammars" / dataset / DEFAULT_GRAMMARS[dataset]
    if not grammar.is_file():
        raise FileNotFoundError(f"Default grammar is missing: {grammar}")
    fd, temporary_name = tempfile.mkstemp(dir=config_dir, prefix=f".{destination.name}.")
    try:
        with os.fdopen(fd, "w") as output:
            for graph_name in DATASETS[dataset]:
                count = REACHABLE_PAIR_COUNTS[dataset][graph_name]
                if count is None:
                    continue
                graph = directory / "graphs" / dataset / f"{graph_name}.g"
                if not graph.is_file():
                    raise FileNotFoundError(f"Graph is missing: {graph}")
                output.write(f"{graph},{grammar},{count}\n")
        os.replace(temporary_name, destination)
    finally:
        Path(temporary_name).unlink(missing_ok=True)
    return destination


def main() -> None:
    """Download one graph or a dataset category from CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    dataset_names = (*DATASETS, "all")
    parser.add_argument("--out-dir", required=True, type=Path, help="output directory")
    parser.add_argument("--graph", choices=cfpq_data.DATASET, metavar="graph", help="graph name, e.g. bzip")
    parser.add_argument("--dataset", nargs="?", const="", metavar="dataset", help=f"one of: {', '.join(dataset_names)}")
    parser.add_argument("-v", "--verbose", action="store_true", help="show all log messages, including DEBUG")
    args = parser.parse_args()
    logging.getLogger().setLevel(logging.DEBUG if args.verbose else logging.WARNING)
    if args.dataset == "":
        parser.error(f"--dataset requires a value; available: {', '.join(dataset_names)}")
    if args.dataset is not None and args.dataset not in dataset_names:
        parser.error(f"unknown dataset {args.dataset!r}; available: {', '.join(dataset_names)}")
    if (args.graph is None) == (args.dataset is None):
        parser.error("specify either --graph or --dataset")

    if args.dataset == "all":
        graph_names = tuple(name for graphs in DATASETS.values() for name in graphs)
    elif args.dataset:
        graph_names = DATASETS[args.dataset]
    else:
        graph_names = (args.graph,)
    if args.dataset:
        print(f"Downloading dataset {args.dataset}: {len(graph_names)} graphs")
    for index, graph_name in enumerate(graph_names, 1):
        download_graph(args.out_dir, graph_name, f"[{index}/{len(graph_names)}] ", args.verbose)
    if args.dataset:
        datasets = DATASETS if args.dataset == "all" else {args.dataset: DATASETS[args.dataset]}
        for dataset in datasets:
            print(f"Wrote {write_dataset_config(args.out_dir, dataset)}")


if __name__ == "__main__":
    main()
