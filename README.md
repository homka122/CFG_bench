# CFG_bench

**CFG_bench** is a tool for benchmarking the CFL algorithm from LAGraph.

## CLI Help

```text
Usage: ./build/cfg_bench -c <config file> [options]

Required:
  -c <config file>  Path to benchmark config file

Benchmark options:
  -r <rounds>       Number of benchmark rounds (default: 10)
  --hot             Enable HOT launch (warm-up run before measurements)
  -a <algorithm>    Algorithm to use (default: CFL_adv; options: CFL_adv, CFL, CFL_single_path, CFL_all_path, CFL_CFPQ_RSM, CFL_multsrc)

Optimization flags:
  -e                Enable empty optimization
  -f                Enable format optimization
  -l                Enable lazy optimization
  -b                Enable block optimization

Other:
  -t                Enable test mode: run each config once and check the result
                    (-r and --hot are ignored)
  -h                Print this help message

Example:
  ./build/cfg_bench -c configs/configs_my.csv -r 10 --hot
```

## Usage

1. Install **GraphBLAS**:
    ```bash
    git clone https://github.com/DrTimothyAldenDavis/GraphBLAS.git
    cd GraphBLAS
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

With [uv](https://docs.astral.sh/uv/) installed, download a dataset and run the
benchmark with the generated config:

```bash
uv run --locked python tools/download_graph.py --out-dir data --dataset c_alias
./build/cfg_bench -c data/configs/c_alias.csv -efbl -a CFL_adv
```

To download just one graph and its grammars:

```bash
uv run --locked python tools/download_graph.py --out-dir data --graph bzip
```

Dataset choices are `c_alias`, `rdf`, `java`, `field_sensitive_alias`,
`context_sensitive_data_flow`, `data_provenance`, `name_resolution`, `uniprot`,
or `all` (every dataset).

Graphs are saved to `<out-dir>/graphs/<dataset>/<graph>.g`; all grammars for
each graph are copied to `<out-dir>/grammars/<dataset>/<graph>/`. For example,
`--graph bzip --out-dir data` creates `data/graphs/c_alias/bzip.g` and
`data/grammars/c_alias/bzip/`.

With `--dataset`, the downloader also writes a headerless config to
`<out-dir>/configs/<dataset>.csv`. It uses the default grammar for the dataset
and published reachable-pair counts, omitting graphs without a published count.
`--graph` does not generate a config.

The converter adds missing reverse edges. Indexed labels are written with an
`_i` suffix and a separate index, as described in [Graph Format](#graph-format).

The selected Java grammar uses indexed labels (`_i`), while the grammars
bundled with 14 Java graphs use unindexed labels. Their published counts have
not been verified against the selected grammar.

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

| Flag | Optimization |
| ---- | ------------ |
| `-e` | empty |
| `-f` | format |
| `-l` | lazy |
| `-b` | block |

These flags can be combined. For example, to enable all optimizations, run:

```bash
./build/cfg_bench -efbl -c configs/configs_my.csv -r 10 --hot
```

Each row in the config file has this format:

```text
<graph path>,<grammar path>,<expected result>
```

Example from `configs/configs_my.csv`:

```text
data/graphs/vf/xz.g,data/grammars/vf.cnf,358834
```

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
