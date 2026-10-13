#include "adapter_CFL_multsrc_adv.h"
#include "GraphBLAS.h"
#include "LAGraph.h"
#include "adapter_CFL_common.h"
#include "adapter_CFL_multsrc_common.h"
#include "parser.h"
#include <stdlib.h>
#include <string.h>

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
    GrB_Matrix output; // created by the algorithm: output[s, v] <=> v is reachable from the source s
    LAGraph_rule_EWCNF *rules;
    GrB_Index *srcs;
    size_t source_count;
    size_t rules_count;
    size_t symbols_amount;
    size_t graph_size;
    int8_t optimizations;
    char msg[LAGRAPH_MSG_LEN];
} state_t;

static state_t state;

// the same graph conversion as in the CFL_adv adapter
static GrB_Matrix *get_matrices_from_graph(Graph graph, size_t *map_base_indecies, size_t symbols_amount) {
    SymbolData *symbol_datas = calloc(symbols_amount, sizeof(SymbolData));
    for (size_t i = 0; i < symbols_amount; i++) {
        symbol_datas[i] = symbol_data_create();
    }

    for (size_t i = 0; i < graph.edge_count; i++) {
        GraphEdge edge = graph.edges[i];
        symbol_data_add(&symbol_datas[map_base_indecies[edge.term_index] + edge.index], edge.u, edge.v, edge.index);
    }

    GrB_Matrix *matrices = malloc(sizeof(GrB_Matrix) * symbols_amount);
    for (size_t i = 0; i < symbols_amount; i++) {
        SymbolData data = symbol_datas[i];
        GrB_Index nrows = graph.node_count;

        char msg[LAGRAPH_MSG_LEN];
        MY_GRB_TRY(GrB_Matrix_new(&matrices[i], GrB_BOOL, nrows, graph.node_count));

        if (data.size == 0) {
            continue;
        }

        GrB_Scalar true_scalar;
        MY_GRB_TRY(GrB_Scalar_new(&true_scalar, GrB_BOOL));
        MY_GRB_TRY(GrB_Scalar_setElement_BOOL(true_scalar, true));
        MY_GRB_TRY(GxB_Matrix_build_Scalar(matrices[i], data.rows, data.cols, true_scalar, data.size));
        MY_GRB_TRY(GrB_free(&true_scalar));
    }

    for (size_t i = 0; i < symbols_amount; i++) {
        symbol_data_free(&symbol_datas[i]);
    }

    free(symbol_datas);

    return matrices;
}

// initialize LAGraph\GraphBLAS
static GrB_Info adapter_CFL_multsrc_adv_setup(void) {
    TRY(LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, state.msg));
    return GrB_SUCCESS;
}

// prepare the adapter for use with the given parser result
// this may include converting the grammar and graph into a format suitable for the algorithm
//
// this modify ther inner state of the adapter
//
// adapter_CFL_multsrc_adv_prepare should be called just once for each config
static GrB_Info adapter_CFL_multsrc_adv_prepare(const ParserResult *parser_result, void *prepare_data) {
    CFL_multsrc_adv_PrepareData *data = (CFL_multsrc_adv_PrepareData *)prepare_data;
    Grammar grammar = parser_result->grammar;
    Graph graph = parser_result->graph;
    SymbolList list = parser_result->symbols;

    // indexed symbols must be each enumerate
    size_t *map = calloc(list.count * graph.block_count, sizeof(size_t));
    size_t offset = 0;
    for (size_t i = 0; i < list.count; i++) {
        Symbol sym = list.symbols[i];
        if (!sym.is_indexed) {
            map[i] = i + offset;
            continue;
        }

        map[i] = i + offset;
        offset += graph.block_count - 1;
    }
    state.symbols_amount = list.count + offset;

    GrB_Matrix *matrices = get_matrices_from_graph(graph, map, state.symbols_amount);

    LAGraph_rule_EWCNF *rules_EWCNF = calloc(grammar.rules_count, sizeof(LAGraph_rule_EWCNF));
    for (size_t i = 0; i < grammar.rules_count; i++) {
        Rule rule = grammar.rules[i];

        rules_EWCNF[i] = (LAGraph_rule_EWCNF){.nonterm = rule.first == -1 ? -1 : (int32_t)map[rule.first],
                                              .prod_A = rule.second == -1 ? -1 : (int32_t)map[rule.second],
                                              .prod_B = rule.third == -1 ? -1 : (int32_t)map[rule.third],
                                              .indexed = 0,
                                              .indexed_count = 0};
        if (rule.first != -1 && list.symbols[rule.first].is_indexed) {
            rules_EWCNF[i].indexed |= LAGraph_EWNCF_INDEX_NONTERM;
        }
        if (rule.second != -1 && list.symbols[rule.second].is_indexed)
            rules_EWCNF[i].indexed |= LAGraph_EWNCF_INDEX_PROD_A;
        if (rule.third != -1 && list.symbols[rule.third].is_indexed)
            rules_EWCNF[i].indexed |= LAGraph_EWNCF_INDEX_PROD_B;
        if (rules_EWCNF[i].indexed != 0) {
            rules_EWCNF[i].indexed_count = graph.block_count;
        }
    }
    free(map);

    state.adj_matrices = matrices;
    state.rules = rules_EWCNF;
    state.rules_count = grammar.rules_count;
    state.optimizations = data->optimizations;
    state.graph_size = graph.node_count;

    if (data->use_start_nodes) {
        state.source_count = parser_result->start_nodes_count;
        state.srcs = NULL;

        if (state.source_count > 0) {
            state.srcs = malloc(state.source_count * sizeof(*state.srcs));
            if (state.srcs == NULL) {
                fprintf(stderr, "out of memory\n");
                abort();
            }
            memcpy(state.srcs, parser_result->start_nodes, state.source_count * sizeof(*state.srcs));
        }
    } else {
        TRY(adapter_CFL_init_src_nodes_common(&state.srcs, &state.source_count, state.graph_size));
    }

    return GrB_SUCCESS;
}

// the algorithm creates the output matrix itself
//
// this should be called before each run of the algorithm
static GrB_Info adapter_CFL_multsrc_adv_init_outputs(void) {
    state.output = NULL;

    return GrB_SUCCESS;
}

// run the algorithm
//
// this should be called after adapter_CFL_multsrc_adv_init_outputs
static GrB_Info adapter_CFL_multsrc_adv_run(void) {
    TRY(LAGraph_CFL_reachability_multsrc_adv(&state.output, state.adj_matrices, state.srcs, state.source_count,
                                             state.symbols_amount, state.rules, state.rules_count, state.msg,
                                             state.optimizations));

    return GrB_SUCCESS;
}

// the config holds the number of reachable pairs, which is not the result of this algorithm
static ResultType adapter_CFL_multsrc_adv_is_result_valid(size_t valid_result) {
    (void)valid_result;
    return RESULT_UNKNOWN;
}

// the number of vertices reachable from at least one source
static size_t adapter_CFL_multsrc_adv_get_result(void) {
    GrB_Index size = 0;
    TRY(GrB_Matrix_ncols(&size, state.output));

    // reachable(v) = true <=> output(s, v) for some source s
    GrB_Vector reachable = NULL;
    TRY(GrB_Vector_new(&reachable, GrB_BOOL, size));
    TRY(GrB_Matrix_reduce_Monoid(reachable, NULL, NULL, GrB_LOR_MONOID_BOOL, state.output, GrB_DESC_T0));

    GrB_Index result = 0;
    TRY(GrB_Vector_nvals(&result, reachable));
    TRY(GrB_Vector_free(&reachable));

    return result;
}

// free the output matrix
//
// this should be called after each run of the algorithm
static GrB_Info adapter_CFL_multsrc_adv_free_outputs(void) {
    TRY(GrB_Matrix_free(&state.output));

    return GrB_SUCCESS;
}

// free all resources allocated by the adapter except outputs
//
// this should be called after all runs of the algorithm for the given config
static GrB_Info adapter_CFL_multsrc_adv_cleanup(void) {
    TRY(adapter_CFL_cleanup_common(&state.adj_matrices, state.symbols_amount, (void **)&state.rules));
    free(state.srcs);
    state.srcs = NULL;

    return GrB_SUCCESS;
}

// free LAGraph\GraphBLAS resources
static GrB_Info adapter_CFL_multsrc_adv_teardown(void) {
    TRY(LAGraph_Finalize(state.msg));
    return GrB_SUCCESS;
}

// get the methods of the adapter
AdapterMethods adapter_CFL_multsrc_adv_get_methods(void) {
    AdapterMethods methods = {.setup = adapter_CFL_multsrc_adv_setup,
                              .teardown = adapter_CFL_multsrc_adv_teardown,
                              .init_outputs = adapter_CFL_multsrc_adv_init_outputs,
                              .free_outputs = adapter_CFL_multsrc_adv_free_outputs,
                              .run = adapter_CFL_multsrc_adv_run,
                              .prepare = adapter_CFL_multsrc_adv_prepare,
                              .cleanup = adapter_CFL_multsrc_adv_cleanup,
                              .is_result_valid = adapter_CFL_multsrc_adv_is_result_valid,
                              .get_result = adapter_CFL_multsrc_adv_get_result};
    return methods;
}
