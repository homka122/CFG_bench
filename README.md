# CFG_bench

[![Build](https://github.com/homka122/CFG_bench/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/homka122/CFG_bench/actions/workflows/build.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**CFG_bench** is a benchmark for the context-free language (CFL) reachability
algorithms implemented in [LAGraph](https://github.com/SparseLinearAlgebra/LAGraph).

CFL reachability asks which pairs of graph vertices are connected by a path whose
edge labels form a word of a given context-free grammar. It is the core of many
static analyses, such as alias and points-to analysis of C and Java programs and
value-flow analysis, and of path queries over RDF graphs. LAGraph solves it with
sparse linear algebra on top of SuiteSparse:GraphBLAS: the graph and the grammar
nonterminals become sparse Boolean matrices, and the algorithm multiplies them
until no new paths appear.

CFG_bench runs these algorithms on real graphs and grammars, measures the running
time and the peak memory, checks the result against the expected number of
reachable pairs and saves the measurements to CSV files in `results/`. It supports:

- `CFL` and `CFL_adv`, which find all reachable pairs; `CFL_adv` adds four optional optimizations
- `CFL_single_path`, `CFL_all_path` and `CFL_all_path_adv`, which also keep the paths:
  one path or all paths for every reachable pair
- `CFL_multsrc` and `CFL_CFPQ_RSM`, which search only from given start vertices;
  `CFL_CFPQ_RSM` takes the grammar as a recursive state machine

The graphs and grammars come from the [FLPQ_Data](https://github.com/FormalLanguageConstrainedPathQuerying/FLPQ_Data)
collection.

## CLI Help

```text
Usage: ./build/cfg_bench -c <config file> [options]

Required:
  -c <config file>  Path to benchmark config file

Benchmark options:
  -r <rounds>       Number of benchmark rounds (default: 10)
  --hot             Enable HOT launch (warm-up run before measurements)
  --bench-parse     Print grammar and graph parsing times only
  --use-start-nodes Use start vertices from the path specified in the config
  -a <algorithm>    Algorithm to use (default: CFL_adv; options: CFL_adv, CFL, CFL_single_path, CFL_all_path, CFL_all_path_adv, CFL_CFPQ_RSM, CFL_multsrc)

Optimization flags:
  -e                Enable empty optimization
  -f                Enable format optimization
  -l                Enable lazy optimization
  -b                Enable block optimization

Other:
  -t                Enable test mode: run each config once and check the result
                    (-r and --hot are ignored)
  --compute-results Check the result against CFL_adv -efbl on the same data instead of
                    the config value (only with -t); CFL_multsrc and CFL_CFPQ_RSM need it,
                    values are cached in .cache when run from the project root
  -h                Print this help message
  --CFL-all-path-use-CFPQ-Core      Use CFPQ_Core in CFL_all_path algorithm

Example:
  ./build/cfg_bench -c configs/configs_my.csv -r 10 --hot
```

## Requirements

- Linux: the benchmark reads peak memory from `/proc/self/status` and uses `malloc_trim`
- GCC 13 or newer, GNU Make and CMake 3.20 or newer (CI uses Ubuntu 24.04)
- [SuiteSparse:GraphBLAS](https://github.com/DrTimothyAldenDavis/GraphBLAS) v10.5.1
- [LAGraph](https://github.com/SparseLinearAlgebra/LAGraph) from the `homka122/all_algorithms_benchmark` branch
- [uv](https://docs.astral.sh/uv/) with Python 3.11 to 3.13, to download graphs and run the linters

## Usage

1. Install **GraphBLAS**:
    ```bash
    git clone https://github.com/DrTimothyAldenDavis/GraphBLAS.git
    cd GraphBLAS
    git checkout v10.5.1
    make
    sudo make install
    cd ..
    ```
2. Install **LAGraph** from the benchmark branch:
    ```bash
    git clone https://github.com/SparseLinearAlgebra/LAGraph.git
    cd LAGraph
    git switch homka122/all_algorithms_benchmark
    make
    sudo make install
    cd ..
    ```
3. Start the benchmark:
    ```bash
    git clone https://github.com/homka122/CFG_bench.git
    cd CFG_bench
    make
    ./build/cfg_bench -c configs/configs_my.csv -efbl -a CFL_adv
    ```
    Run `./build/cfg_bench -h` to print the CLI help message with descriptions of all available options.

## Downloading new graphs

With [uv](https://docs.astral.sh/uv/) installed, download a graph category and run
the benchmark with the generated config:

```bash
uv run --locked python tools/download_graph.py --out-dir data --category c_alias_analysis
./build/cfg_bench -c data/configs/c_alias_analysis.csv -efbl -a CFL_adv
```

To download just one graph and its grammars:

```bash
uv run --locked python tools/download_graph.py --out-dir data --graph bzip
```

Category and graph names are those of FLPQ_Data: `c_alias_analysis`, `rdf`,
`java_points_to`, `field_sensitive_alias`, `context_sensitive_data_flow`,
`data_provenance`, `name_resolution`, `biological_uniprot`, or `all` (every
category). FLPQ_Data keeps the downloaded archives in a per-user cache
(`FLPQ_DATA_CACHE` or the OS cache directory), so repeated runs do not download
again.

Graphs are saved to `<out-dir>/graphs/<category>/<graph>.g`; the grammars (`.cnf`)
and recursive state machines (`.rsm`) of all queries of each graph are copied to
`<out-dir>/grammars/<category>/<graph>/`. For example, `--graph bzip --out-dir data`
creates `data/graphs/c_alias_analysis/bzip.g` and
`data/grammars/c_alias_analysis/bzip/`.

With `--category`, the downloader also writes a headerless config to
`<out-dir>/configs/<category>.csv`: one line per graph with the graph's own copy
of the category grammar (`nested_parentheses_subClassOf_type` for `rdf`, the
only query otherwise) and the reachable-pair count from the FLPQ_Data
`reachable_pairs` table, or `-1` when the table has no count yet. `--graph` does
not generate a config.

The converter adds reverse edges. Indexed labels are written with an `_i` suffix
and a separate index, as described in [Graph Format](#graph-format).

### Legacy dataset archive

As a second way to obtain benchmark data, download and unpack the existing
`CFPQ_eval` archive into the repository root:

```bash
gdown 12Qhc6XNXYbpPbZGp-lo30NsywFELAFhu
unzip CFPQ_eval.zip -d .
```

If `gdown` fails, download the same archive directly:

```bash
curl -L "https://drive.usercontent.google.com/download?id=12Qhc6XNXYbpPbZGp-lo30NsywFELAFhu&export=download&confirm=t" -o CFPQ_eval.zip
unzip CFPQ_eval.zip -d .
```

The archive places its benchmark data in `data/`. It is not needed to run the
included `configs/configs_my.csv` example.

## Benchmark Configuration

The benchmark reads its input set from a CSV file passed with `-c`:

```bash
./build/cfg_bench -c configs/configs_my.csv
```

Use `-r` to set the number of benchmark rounds and `--hot` to enable the HOT launch warm-up run.

The `CFL_adv` algorithm also supports optimization flags:

| Flag | Optimization | What it does |
| ---- | ------------ | ------------ |
| `-e` | empty  | skips operations on empty matrices |
| `-f` | format | stores each matrix by rows or by columns, whichever suits the operation |
| `-l` | lazy   | keeps a nonterminal matrix as a sum of parts and merges only parts of similar size |
| `-b` | block  | multiplies the matrices of all indices of an indexed symbol at once |

These flags can be combined. For example, to enable all optimizations, run:

```bash
./build/cfg_bench -efbl -c configs/configs_my.csv -r 10 --hot
```

Each row in the config file has this format:

```text
<graph path>,<grammar path>,<expected result>[,<start vertices path>]
```

Example from `configs/configs_my.csv`:

```text
data/graphs/vf/xz.g,data/grammars/vf.cnf,358834
```

The expected result is the number of reachable pairs, or `-1` when it is unknown:
in test mode such rows are reported as `[Unknown]` and do not fail the run.
`CFL_multsrc` and `CFL_CFPQ_RSM` ignore it, see
[Multiple-Source Algorithms](#multiple-source-algorithms).

## Grammar Format

Grammar files contain one production rule per line:

```text
<LEFT_SYMBOL>	[RIGHT_SYMBOL_1]	[RIGHT_SYMBOL_2]
```

- `<LEFT_SYMBOL>` is the nonterminal on the left-hand side of the rule.
- `[RIGHT_SYMBOL_1]` and `[RIGHT_SYMBOL_2]` are optional right-hand side symbols.
- Symbols are separated by tabs.
- The `Count:` line is required. The start symbol must be placed on the next line.
- Indexed symbols must end with `_i`, for example `a_i` or `AS_i`.

Example:

```text
S	AS_i	b_i
AS_i	a_i	S
S	c

Count:
S
```

## Graph Format

Graph files contain one edge per line:

```text
<EDGE_SOURCE>	<EDGE_DESTINATION>	<EDGE_LABEL>	[LABEL_INDEX]
```

- `<EDGE_SOURCE>` and `<EDGE_DESTINATION>` are zero-based vertex ids.
- `<EDGE_LABEL>` is the terminal label on the edge.
- `[LABEL_INDEX]` is optional and specifies the concrete index for labels ending with `_i`.
- Values are separated by tabs.
- For example, an edge labeled `x_9` is written as `x_i 9`.

Example:

```text
1	2	a_i	1
2	3	b_i	1
2	4	b_i	2
1	5	c
```

## Adding a New Configuration

To add a custom benchmark configuration:

1. **Prepare the graph and grammar files**  
   Put the required files into the `data` directory (or use existing files there).

2. **Create a config file in `configs/` or extend an existing one**  
   Use the existing files in `configs/` as examples, such as `configs/configs_my.csv` or `configs/configs_java.csv`.

3. **Add one row per benchmark case**  
   Each row must contain the graph path, grammar path, and expected result separated by commas.

   ```text
   data/graphs/new_graph.g,data/grammars/new_grammar.cnf,12345
   ```

4. **Run the benchmark with your config file**  
   Pass the file with `-c`:

   ```bash
   ./build/cfg_bench -c configs/your_config.csv
   ```

No source code changes are required.

## Multiple-Source Algorithms

`CFL_multsrc` and `CFL_CFPQ_RSM` search paths only from the start vertices.
Their result is the number of vertices reachable from at least one start
vertex, not the number of reachable pairs, so they ignore the expected result
from the config and `-t` prints a warning and `[Unknown]`.

1. **Add the start vertices file as the fourth column**  
   The file contains one vertex per line. It can be produced with
   `pairs_extractor --start-only`, see
   [Reachable Pair Extraction](#reachable-pair-extraction). The same config
   works for `CFL_adv`.

   ```text
   data/graphs/c_alias/wc.g,data/grammars/c_alias.cnf,156,data/start_nodes/c_alias_wc_start.result
   ```

   Without `--use-start-nodes` the fourth column is ignored and every vertex is
   a start vertex. `CFL_CFPQ_RSM` also needs an RSM template for the grammar.

2. **Check the result**  
   `--compute-results` runs `CFL_adv -efbl` on the same data, counts the
   vertices reachable from the start vertices and checks the algorithm against
   this count:

   ```bash
   ./build/cfg_bench -t -c configs/your_config.csv -a CFL_multsrc --use-start-nodes --compute-results
   ```

   ```text
   	Result: 156 (Return code: 0) [OK] (Computed: 156) (0.0033 sec)
   ```

   When the benchmark runs from the project root, computed values are cached
   in `.cache/computed_results.csv`, and the next runs print `Cached` instead
   of running `CFL_adv` again. A value is recomputed after the graph, grammar
   or start vertices file changes. Delete the file to clear the cache.

3. **Run the benchmark**

   ```bash
   ./build/cfg_bench -c configs/your_config.csv -a CFL_multsrc --use-start-nodes -r 10 --hot
   ```

## Reachable Pair Extraction

Build and run the standalone extractor. The output directory must already
exist:

    make pairs-extractor
    ./build/pairs_extractor -c configs/configs_my.csv -o results

The extractor runs CFL_adv with all four optimizations enabled and writes the
start-symbol matrix tuples without a header, one pair per line. It creates one
file per config row named `<grammar>_<graph>.result`; for example,
`aa_x264.result`:

    0 1
    2 5

Use `--start-only` to write only the unique vertices from the left side of
reachable pairs, one vertex per line:

    ./build/pairs_extractor -c configs/configs_my.csv -o results --start-only

In this mode, files are named `<grammar>_<graph>_start.result`.

## Development

- `make CI` builds the benchmark and runs every algorithm on `configs/for_test.csv`, as CI does
- `make lint` checks the formatting with clang-format and ruff, `make format` fixes it
- `make hooks` installs pre-commit hooks that run the same tools on every commit

See [CONTRIBUTING.md](CONTRIBUTING.md) for how to report bugs and contribute.
