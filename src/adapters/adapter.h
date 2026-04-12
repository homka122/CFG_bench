/**
 * @file adapter.h
 * @brief Adapter interface for CFL algorithm implementations.
 *
 * This file defines the adapter pattern used to support multiple CFL
 * (Context-Free Language) reachability algorithm implementations.
 * Each adapter provides a consistent interface for running algorithms
 * and managing their internal state.
 *
 * == Adapter Lifecycle ==
 *
 * An adapter must be used in the following order:
 *
 * 1. setup()         - Initialize GraphBLAS/LAGraph and internal state
 * 2. prepare()       - Load grammar and graph, convert to internal format
 * 3. init_outputs()  - Allocate output matrices (called once per benchmark run)
 * 4. run()           - Execute the algorithm
 * 5. get_result()    - Retrieve the result
 * 6. free_outputs()  - Release output matrices (called after each run)
 * 7. cleanup()       - Release grammar/graph resources (called after all runs)
 * 8. teardown()      - Shutdown GraphBLAS/LAGraph (called at program end)
 *
 * Example:
 * @code
 *     AdapterMethods adapter = adapter_CFL_adv_get_methods();
 *
 *     adapter.setup();
 *     adapter.prepare(parser_result, prepare_data);
 *
 *     for (int i = 0; i < COUNT; i++) {
 *         adapter.init_outputs();
 *         adapter.run();
 *         size_t result = adapter.get_result();
 *         adapter.free_outputs();
 *     }
 *
 *     adapter.cleanup();
 *     adapter.teardown();
 * @endcode
 *
 * == Implementing a New Adapter ==
 *
 * To implement a new CFL algorithm adapter:
 *
 * 1. Create adapter_<name>.h and adapter_<name>.c
 * 2. Define a prepare data struct if needed (passed via prepare()'s prepare_data)
 * 3. Implement all AdapterMethods functions
 * 4. Provide a getter function: AdapterMethods adapter_<name>_get_methods(void)
 *
 * Required function implementations:
 *
 * - setup(): Initialize any global state, call LAGr_Init()
 * - teardown(): Cleanup global state, call LAGr_Finalize()
 * - prepare(): Convert ParserResult to internal format, store in state
 * - init_outputs(): Allocate output matrices for the algorithm
 * - run(): Execute the CFL algorithm
 * - free_outputs(): Release output matrices
 * - cleanup(): Release grammar/graph resources
 * - is_result_valid(): Check if result matches expected value
 * - get_result(): Return the algorithm result (typically nnz count)
 *
 * == Important Notes ==
 *
 * - prepare() is called once per config (graph + grammar combination)
 * - init_outputs()/free_outputs() are called once per benchmark iteration
 * - cleanup() is called once after all iterations for a config
 * - All functions return GrB_Info (GrB_SUCCESS on success, error code otherwise)
 */

#pragma once

#include "parser.h"
#include <GraphBLAS.h>
#include <LAGraph.h>

/**
 * @brief Function pointer type for adapter setup function.
 *
 * Initializes the adapter and underlying libraries (GraphBLAS, LAGraph).
 * Called once at the beginning of the program.
 *
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterSetup)(void);

/**
 * @brief Function pointer type for adapter teardown function.
 *
 * Shuts down the adapter and underlying libraries.
 * Called once at the end of the program.
 *
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterTeardown)(void);

/**
 * @brief Function pointer type for output initialization.
 *
 * Allocates output matrices required by the algorithm.
 * Called before each run of the algorithm.
 *
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterInitOutputs)(void);

/**
 * @brief Function pointer type for output cleanup.
 *
 * Releases output matrices after each algorithm run.
 *
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterFreeOutputs)(void);

/**
 * @brief Function pointer type for algorithm execution.
 *
 * Runs the CFL algorithm.
 * Must be called after init_outputs() and before get_result().
 *
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterRun)(void);

/**
 * @brief Function pointer type for data preparation.
 *
 * Converts the parser result (grammar, graph, symbols) into the
 * internal format required by the algorithm.
 * Called once per config (graph + grammar combination) before any runs.
 *
 * @param parser_result Parsed grammar and graph data
 * @param prepare_data Optional adapter-specific configuration data
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterPrepare)(ParserResult parser_result, void *prepare_data);

/**
 * @brief Function pointer type for resource cleanup.
 *
 * Releases resources allocated during prepare() but not outputs.
 * Called after all benchmark iterations for a config are complete.
 *
 * @return GrB_SUCCESS on success, error code otherwise
 */
typedef GrB_Info (*AdapterCleanup)(void);

/**
 * @brief Function pointer type for result validation.
 *
 * Validates the algorithm output against an expected result.
 *
 * @param valid_result Expected result value (typically expected nnz count)
 * @return true if result is valid, false otherwise
 */
typedef bool (*AdapterIsResultValid)(size_t valid_result);

/**
 * @brief Function pointer type for result retrieval.
 *
 * Returns the result of the algorithm execution.
 * Typically returns the number of non-zero elements in the output matrix.
 *
 * @return The algorithm result value
 */
typedef size_t (*AdapterGetResult)(void);

/**
 * @brief Adapter method table.
 *
 * This struct contains function pointers for all adapter operations.
 * Each adapter implementation provides a getter function that returns
 * a populated AdapterMethods struct with pointers to its implementations.
 *
 * Usage:
 * @code
 *     AdapterMethods methods = adapter_<name>_get_methods();
 *     methods.setup();
 * @endcode
 */
typedef struct {
    /** Initialize adapter and underlying libraries */
    AdapterSetup setup;

    /** Shutdown adapter and underlying libraries */
    AdapterTeardown teardown;

    /** Allocate output matrices before algorithm run */
    AdapterInitOutputs init_outputs;

    /** Release output matrices after algorithm run */
    AdapterFreeOutputs free_outputs;

    /** Execute the CFL algorithm */
    AdapterRun run;

    /** Prepare adapter with grammar and graph data */
    AdapterPrepare prepare;

    /** Release prepared data (grammar, graph matrices) */
    AdapterCleanup cleanup;

    /** Validate algorithm output */
    AdapterIsResultValid is_result_valid;

    /** Get algorithm result */
    AdapterGetResult get_result;
} AdapterMethods;
