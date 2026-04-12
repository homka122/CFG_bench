// code from b08lsoai's CFG_Bench fork

#include "GraphBLAS.h"
#include "LAGraph.h"
#include "adapter_CFL_adv.h"
#include "adapter_CFL_common.h"
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
    GrB_Type pi_type;
    GrB_Matrix *adj_matrices;
    GrB_Matrix *outputs;
    LAGraph_rule_WCNF *rules;
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
    TRY(LAGraph_CFL_single_path(state.outputs, &state.pi_type, state.adj_matrices, state.terms_count,
                                state.nonterms_count, state.rules, state.rules_count, state.msg));

    return GrB_SUCCESS;
}

// check if the result of the algorithm is valid
//
// TODO: now check only count of reachibility pairs, make this more generic for other adapters
static bool adapter_CFL_is_result_valid(size_t valid_result) {
    bool is_valid = false;
    TRY(adapter_CFL_is_result_valid_common(state.outputs[0], valid_result, &is_valid));
    return is_valid;
}

// TODO: now this is the same as adapter_CFL_adv_is_result_valid, make this more generic for other adapters
static size_t adapter_CFL_get_result() {
    size_t result = 0;
    TRY(adapter_CFL_get_result_common(state.outputs[0], &result));
    return result;
}

// free output matrices
//
// this should be called after each run of the algorithm
static GrB_Info adapter_CFL_free_outputs() {
    TRY(GrB_free(&state.pi_type));
    TRY(adapter_CFL_free_outputs_common(&state.outputs, state.nonterms_count, state.msg));

    return GrB_SUCCESS;
}

// free all resources allocated by the adapter except outputs
//
// this should be called after all runs of the algorithm for the given config
static GrB_Info adapter_CFL_cleanup() {
    TRY(adapter_CFL_cleanup_common(&state.adj_matrices, state.terms_count, (void **)&state.rules));

    return GrB_SUCCESS;
}

// free LAGraph\GraphBLAS resources
static GrB_Info adapter_CFL_teardown() { TRY(LAGraph_Finalize(state.msg)); }

// get the methods of the adapter
AdapterMethods adapter_CFL_single_path_get_methods() {
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
