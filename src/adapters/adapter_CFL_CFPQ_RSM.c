// code from DanyaLitva's CFG_Bench fork
// https://github.com/DanyaLitva/CFG_bench/blob/main/testAP.c

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
    RSM rsm;
    GrB_Vector *reachable;
    GrB_Matrix *adj_matrices;
    GrB_Index *sources;
    size_t sources_num;
    GrB_Index V; // Graph size
    char msg[LAGRAPH_MSG_LEN];
} state_t;

static state_t state;

static GrB_Info adapter_CFL_setup() {
    TRY(LAGr_Init(GrB_BLOCKING, malloc, NULL, NULL, free, state.msg));

    return GrB_SUCCESS;
}

// prepare the adapter for use with the given parser result
// this may include converting the grammar and graph into a format suitable for the algorithm
//
// this modify ther inner state of the adapter
//
// adapter_CFL_prepare should be called just once for each config
static GrB_Info adapter_CFL_prepare(ParserResult parser_result, void *prepare_data) {
    (void)prepare_data;
    Grammar grammar = parser_result.grammar;
    Graph graph = parser_result.graph;
    SymbolList list = parser_result.symbols;

    grammar_to_WCNF(&grammar, &list);
    SymbolList terms = symbol_list_create();
    SymbolList nonterms = symbol_list_create();

    grammar_split_terms_nonterms(&grammar, &list, &terms, &nonterms);

    // change term indecies in graph
    for (size_t i = 0; i < graph.edge_count; i++) {
        graph.edges[i].term_index = symbol_list_get_index_str(&terms, list.symbols[graph.edges[i].term_index].label);
    }

    explode_indices_CFL(&grammar, &graph, &nonterms, &terms);

    if (parser_result.rsm_template == RSM_NO_TEMPLATE) {
        fprintf(stderr, "RSM template not found\n");
        abort();
    }

    CFG_RSM *rsm = rsm_create_template(parser_result.rsm_template, true, parser_result.block_count, &terms);

    GrB_Matrix *prepared_adj_matrices = calloc(rsm->terms.count, sizeof(GrB_Matrix));
    for (size_t i = 0; i < rsm->terms.count; i++) {
        TRY(GrB_Matrix_new(prepared_adj_matrices + i, GrB_BOOL, graph.node_count, graph.node_count));
    }

    GrB_Scalar true_scalar;
    TRY(GrB_Scalar_new(&true_scalar, GrB_BOOL));
    TRY(GrB_Scalar_setElement_BOOL(true_scalar, true));

    GrB_Index *row = malloc(sizeof(GrB_Index) * graph.edge_count);
    GrB_Index *col = malloc(sizeof(GrB_Index) * graph.edge_count);
    for (size_t i = 0; i < rsm->terms.count; i++) {
        int count = 0;

        for (int j = 0; j < graph.edge_count; j++) {
            if (i == graph.edges[j].term_index) {
                row[count] = graph.edges[j].u;
                col[count] = graph.edges[j].v;
                count++;
            }
        }

        TRY(GxB_Matrix_build_Scalar(prepared_adj_matrices[i], row, col, true_scalar, count));
#ifdef DEBUG_parser
        GxB_print(prepared_adj_matrices[i], 1);
#endif
    }

    state.rsm = rsm_convert_to_lagraph(rsm);
    state.adj_matrices = prepared_adj_matrices;
    state.V = graph.node_count;

    adapter_CFL_init_src_nodes_common(&state.sources, &state.sources_num, 0);

    free(row);
    free(col);
    GrB_free(&true_scalar);

    free(graph.edges);
    free(grammar.rules);
    symbol_list_free(&nonterms);
    symbol_list_free(&list);

    rsm_free(rsm);

    return GrB_SUCCESS;
}

// initialize output matrices
//
// this should be called before each run of the algorithm
static GrB_Info adapter_CFL_init_outputs() {
    state.reachable = calloc(1, sizeof(GrB_Vector));
    return GrB_SUCCESS;
}

// run the algorithm
//
// this should be called after adapter_CFL_adv_init_outputs
static GrB_Info adapter_CFL_run() {
    TRY(LAGraph_CFPQ_RSM(state.reachable, &state.rsm, state.adj_matrices, state.sources, state.sources_num, state.V,
                         state.msg));

    return GrB_SUCCESS;
}

// check if the result of the algorithm is valid
//
// TODO: now check only count of reachibility pairs, make this more generic for other adapters
static ResultType adapter_CFL_is_result_valid(size_t valid_result) {
    (void)valid_result;
    return RESULT_UNKNOWN;
    // bool is_valid = false;
    // TRY(adapter_CFL_is_result_valid_common(state.outputs[0], valid_result, &is_valid));
    // return is_valid;
}

// TODO: now this is the same as adapter_CFL_adv_is_result_valid, make this more generic for other adapters
static size_t adapter_CFL_get_result() {
    GrB_Index result = 0;
    TRY(GrB_Vector_nvals(&result, *state.reachable));
    return result;
}

// free output matrices
//
// this should be called after each run of the algorithm
static GrB_Info adapter_CFL_free_outputs() {
    TRY(GrB_free(state.reachable));
    free(state.reachable);

    return GrB_SUCCESS;
}

// free all resources allocated by the adapter except outputs
//
// this should be called after all runs of the algorithm for the given config
static GrB_Info adapter_CFL_cleanup() {
    TRY(adapter_CFL_cleanup_common(&state.adj_matrices, state.rsm.terminal_count, (void **)NULL));

    return GrB_SUCCESS;
}

// free LAGraph\GraphBLAS resources
static GrB_Info adapter_CFL_teardown() {
    TRY(LAGraph_Finalize(state.msg));
    return GrB_SUCCESS;
}

// get the methods of the adapter
AdapterMethods adapter_CFL_CFPQ_RSM_get_methods() {
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
