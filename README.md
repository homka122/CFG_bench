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
    git clone git@github.com:SparseLinearAlgebra/LAGraph.git
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
    make bench
    ./build/test -c configs/configs_my.csv
    ```
    After unpacking, the benchmark data will be available in the `data` folder.
    Run `./build/test -h` or any other invalid flag to print the CLI help message with all available options.
4. _(Optional)_ To use different graphs and grammars, upload the required files to the `data` folder.

## Benchmark Configuration

The benchmark reads its input set from a CSV file passed with `-c`:

```bash
./build/test -c configs/configs_my.csv
```

Each row in the config file has this format:

```text
<graph path>,<grammar path>,<expected result>
```

Example from `configs/configs_my.csv`:

```text
data/graphs/c_alias/init.g,data/grammars/c_alias.cnf,3783769
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
   ./build/test -c configs/your_config.csv
   ```

No source code changes are required.
