"""Download graphs and their grammars into a directory."""

import argparse
import logging
import os
import shutil
import sys
import tempfile
from pathlib import Path

import cfpq_data

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


def mtx_dir_to_txt(source_dir: Path, destination: Path) -> None:
    """Stream labeled MatrixMarket edges to TXT without building a graph."""
    destination.parent.mkdir(parents=True, exist_ok=True)
    fd, temporary_name = tempfile.mkstemp(dir=destination.parent, prefix=f".{destination.name}.")
    try:
        with os.fdopen(fd, "w") as output:
            for mtx_file in sorted(source_dir.glob("*.mtx")):
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
                        output.write(f"{u} {mtx_file.stem} {v}\n")
                        count += 1
                    if count != expected:
                        raise ValueError(
                            f"{mtx_file} declares {expected} entries but has {count}"
                        )
        os.replace(temporary_name, destination)
    finally:
        Path(temporary_name).unlink(missing_ok=True)


def download_graph(directory: Path, graph_name: str, progress: str = "", verbose: bool = False) -> None:
    """Save one dataset graph as TXT and copy its entire grammar directory."""
    dataset = next(name for name, graphs in DATASETS.items() if graph_name in graphs)
    interactive = sys.stdout.isatty() and not verbose

    def show(stage: str, done: bool = False) -> None:
        message = f"{progress}{graph_name}: {stage}"
        print(f"\r{message:<72}" if interactive else message,
              end="\n" if done or not interactive else "", flush=True)

    show("downloading")
    source = cfpq_data.download(graph_name)
    graph_dir = directory / "graphs" / dataset
    cfpq_data.graph_from_mtx_dir
    grammar_dir = directory / "grammars" / dataset / graph_name
    graph_dir.mkdir(parents=True, exist_ok=True)

    show("saving graph")
    mtx_dir_to_txt(source / "graph", graph_dir / f"{graph_name}.txt")
    if (source / "grammar").is_dir():
        show("copying grammar")
        shutil.copytree(source / "grammar", grammar_dir, dirs_exist_ok=True)
    show("done", done=True)


def main() -> None:
    """Download one graph or a dataset category from CLI arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    dataset_names = (*DATASETS, "all")
    parser.add_argument("--out-dir", required=True, type=Path,
                        help="output directory")
    parser.add_argument("--graph", choices=cfpq_data.DATASET,
                        metavar="graph", help="graph name, e.g. bzip")
    parser.add_argument("--dataset", nargs="?", const="", metavar="dataset",
                        help=f"one of: {', '.join(dataset_names)}")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="show all log messages, including DEBUG")
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


if __name__ == "__main__":
    main()
