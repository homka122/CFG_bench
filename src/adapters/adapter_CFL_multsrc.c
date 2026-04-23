#include "GraphBLAS.h"
#include "LAGraph.h"
#include "adapter_CFL_adv.h"
#include "adapter_CFL_common.h"
#include "adapter_CFL_multsrc_common.h"
#include "parser.h"

#define TRY(GrB_method)                                                                                                \
    {                                                                                                                  \
        GrB_Info LG_GrB_Info = GrB_method;                                                                             \
        if (LG_GrB_Info < GrB_SUCCESS) {                                                                               \
            fprintf(stderr, "LAGraph failure (file %s, line %d): (%d, msg: %s) \n", __FILE__, __LINE__, LG_GrB_Info,   \
                    state.msg);                                                                                        \
            return (LG_GrB_Info);                                                                                      \
        }                                                                                                              \
    }

// inner state of the adapter
typedef struct {
    GrB_Matrix *adj_matrices;
    GrB_Matrix *outputs;
    LAGraph_rule_WCNF *rules;
    GrB_Index *srcs;
    size_t source_count;
    size_t rules_count;
    size_t terms_count;
    size_t nonterms_count;
    size_t graph_size;
    char msg[LAGRAPH_MSG_LEN];
} state_t;

static state_t state;

static GrB_Info adapter_CFL_setup() {
    TRY(LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, state.msg));

    return GrB_SUCCESS;
}

// prepare the adapter for use with the given parser result
// this may include converting the grammar and graph into a format suitable for the algorithm
//
// this modify ther inner state of the adapter
//
// adapter_CFL_prepare should be called just once for each config
static GrB_Info adapter_CFL_prepare(ParserResult parser_result, void *prepare_data) {
    TRY(adapter_CFL_prepare_common(parser_result, &state.adj_matrices, &state.terms_count, &state.nonterms_count,
                                   &state.rules, &state.rules_count, &state.graph_size));

    adapter_CFL_init_src_nodes_common(&state.srcs, &state.source_count);

    return GrB_SUCCESS;
}

// initialize output matrices
//
// this should be called before each run of the algorithm
static GrB_Info adapter_CFL_init_outputs() {
    TRY(adapter_CFL_init_outputs_common(&state.outputs, state.nonterms_count, state.graph_size, state.msg));

    return GrB_SUCCESS;
}

// run the algorithm
//
// this should be called after adapter_CFL_adv_init_outputs
static GrB_Info adapter_CFL_run() {
    TRY(LAGraph_CFL_reachability_multsrc(state.outputs, state.adj_matrices, state.srcs, state.source_count,
                                         state.terms_count, state.nonterms_count, state.rules, state.rules_count,
                                         state.msg));

    return GrB_SUCCESS;
}

// check if the result of the algorithm is valid
//
// TODO: now check only count of reachibility pairs, make this more generic for other adapters
static ResultType adapter_CFL_is_result_valid(size_t valid_result) {
    // TODO: combine indexed matrices if initial nonterm is indexed
    return RESULT_UNKNOWN;
}

// TODO: now this is the same as adapter_CFL_adv_is_result_valid, make this more generic for other adapters
static size_t adapter_CFL_get_result() {
    // reduce output matrix to vector for counting reachable nodes
    GrB_Index result = 0;

    GrB_Index size = 0;
    TRY(GrB_Matrix_ncols(&size, state.outputs[0]));
    GrB_Vector result_vec = NULL;
    TRY(GrB_Vector_new(&result_vec, GrB_BOOL, size));

    GrB_Index nnz = 0;
    TRY(GrB_Matrix_nvals(&nnz, state.outputs[0]));

    GrB_Index *rows = calloc(nnz, sizeof(GrB_Index));
    GrB_Index *cols = calloc(nnz, sizeof(GrB_Index));
    bool *values = calloc(nnz, sizeof(GrB_Index));
    TRY(GrB_Matrix_extractTuples_BOOL(rows, cols, values, &nnz, state.outputs[0]));

    for (size_t i = 0; i < nnz; i++) {
        rows[i] = 0;
    }

    TRY(GrB_Vector_build_BOOL(result_vec, cols, values, nnz, GxB_PAIR_BOOL));

    TRY(GrB_Vector_nvals(&result, result_vec));

    free(rows);
    free(cols);
    free(values);
    TRY(GrB_Vector_free(&result_vec));

    return result;
}

// free output matrices
//
// this should be called after each run of the algorithm
static GrB_Info adapter_CFL_free_outputs() {
    TRY(adapter_CFL_free_outputs_common(&state.outputs, state.nonterms_count, state.msg));

    return GrB_SUCCESS;
}

// free all resources allocated by the adapter except outputs
//
// this should be called after all runs of the algorithm for the given config
static GrB_Info adapter_CFL_cleanup() {
    TRY(adapter_CFL_cleanup_common(&state.adj_matrices, state.terms_count, (void **)&state.rules));
    free(state.srcs);

    return GrB_SUCCESS;
}

// free LAGraph\GraphBLAS resources
static GrB_Info adapter_CFL_teardown() { TRY(LAGraph_Finalize(state.msg)); }

// get the methods of the adapter
AdapterMethods adapter_CFL_multsrc_get_methods() {
    AdapterMethods methods = {.setup = adapter_CFL_setup,
                              .teardown = adapter_CFL_teardown,
                              .init_outputs = adapter_CFL_init_outputs,
                              .free_outputs = adapter_CFL_free_outputs,
                              .run = adapter_CFL_run,
                              .prepare = adapter_CFL_prepare,
                              .cleanup = adapter_CFL_cleanup,
                              .is_result_valid = adapter_CFL_is_result_valid,
                              .get_result = adapter_CFL_get_result};
    return methods;
}
