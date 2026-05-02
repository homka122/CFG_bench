# CFG_bench

**CFG_bench** is a tool for benchmarking the CFL algorithm from LAGraph.

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
    gdown 12Qhc6XNXYbpPbZGp-lo30NsywFELAFhu
    unzip CFPQ_eval.zip -d .
    make
    ./build/cfg_bench -c configs/configs_my.csv -r 10 --hot
    ```
    If `gdown` hangs or fails to download the archive, use the direct Google Drive link instead:
    ```bash
    curl -L "https://drive.usercontent.google.com/download?id=12Qhc6XNXYbpPbZGp-lo30NsywFELAFhu&export=download&confirm=t" -o CFPQ_eval.zip
    ```
    After unpacking, the benchmark data will be available in the `data` folder.
    Run `./build/cfg_bench -h` to print the CLI help message with descriptions of all available options.
4. _(Optional)_ To use different graphs and grammars, upload the required files to the `data` folder.

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
data/graphs/c_alias/init.g,data/grammars/c_alias.cnf,3783769
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
