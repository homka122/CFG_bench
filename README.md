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
2. Install **LAGraph v1.2**:
    ```bash
    git clone https://github.com/GraphBLAS/LAGraph.git
    cd LAGraph
    git switch v1.2
    make
    sudo make install
    cd ..
    ```
3. Start the benchmark:
    ```bash
    git clone https://github.com/homka122/CFG_bench.git
    cd CFG_bench
    make bench
    ```
4. _(Optional)_ To use different graphs and grammars, upload the required files to the `data` folder.

## Benchmark Configuration

The test configuration is set in `test.c` using three macros:

```c
#define COUNT 10    // Number of benchmark runs on a single graph
#define HOT true    // If true, the first run is done without measuring time (warm-up)
#define configs configs_my  // Use your custom configuration for the benchmark (default is the xz.g graph and vf.cnf grammar)
```

## Adding a New Configuration

To add a custom configuration for the benchmark, follow these steps:

1. **Prepare the Graph and Grammar Files**:  
   Ensure that you have the graph and grammar files ready. The paths should be relative to the current directory.

2. **Add the Configuration to the Array**:  
   Find the appropriate configuration array (e.g., `configs_rdf`, `configs_java`, `configs_c_alias`, etc.) and add your new config. The configuration should be a string containing the path to the graph and the grammar, separated by a comma.  
   Example:

    ```c
    char *configs_new[] = {
        "data/graphs/new_graph.g,data/grammars/new_grammar.cnf",
        NULL
    };
    ```

3. **Update the `configs` Macro**:  
   Modify the `configs` macro to use your new configuration array. For example, if you added a config to `configs_new`, change the line:

    ```c
    #define configs configs_my
    ```

    to:

    ```c
    #define configs configs_new
    ```

4. **Run the Benchmark**:  
   After adding the new configuration, run the benchmark to ensure that it works correctly with your graph and grammar.

Now the benchmark will use your custom configuration.
