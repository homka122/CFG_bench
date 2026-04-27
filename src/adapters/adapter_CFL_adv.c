#include "adapter_CFL_adv.h"
#include "GraphBLAS.h"
#include "LAGraph.h"
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
    GrB_Matrix *adj_matrices;
    GrB_Matrix *outputs;
    LAGraph_rule_EWCNF *rules;
    size_t rules_count;
    char msg[LAGRAPH_MSG_LEN];
    size_t symbols_amount;
    size_t graph_size;
    int8_t optimizations;
} state_t;

static state_t state;

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
static GrB_Info adapter_CFL_adv_setup() {
    TRY(LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, state.msg));
    return GrB_SUCCESS;
}

// inner structure to pass data to prepare function
typedef CFL_adv_PrepareData PrepareData;

// prepare the adapter for use with the given parser result
// this may include converting the grammar and graph into a format suitable for the algorithm
//
// this modify ther inner state of the adapter
//
// adapter_CFL_adv_prepare should be called just once for each config
static GrB_Info adapter_CFL_adv_prepare(ParserResult parser_result, void *prepare_data) {
    PrepareData *data = (PrepareData *)prepare_data;
    int8_t optimizations = data->optimizations;
    Grammar grammar = parser_result.grammar;
    Graph graph = parser_result.graph;
    SymbolList list = parser_result.symbols;

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
    state.optimizations = optimizations;
    state.graph_size = graph.node_count;

    free(graph.edges);
    free(grammar.rules);
    for (size_t i = 0; i < list.count; i++) {
        free(list.symbols[i].label);
    }
    free(list.symbols);

    return GrB_SUCCESS;
}

// initialize output matrices
//
// this should be called before each run of the algorithm
static GrB_Info adapter_CFL_adv_init_outputs() {
    TRY(adapter_CFL_init_outputs_common(&state.outputs, state.symbols_amount, state.graph_size, state.msg));

    return GrB_SUCCESS;
}

// run the algorithm
//
// this should be called after adapter_CFL_adv_init_outputs
static GrB_Info adapter_CFL_adv_run() {
    TRY(LAGraph_CFL_reachability_adv(state.outputs, state.adj_matrices, state.symbols_amount, state.rules,
                                     state.rules_count, state.msg, state.optimizations));

    return GrB_SUCCESS;
}

// check if the result of the algorithm is valid
//
// TODO: now check only count of reachibility pairs, make this more generic for other adapters
static ResultType adapter_CFL_adv_is_result_valid(size_t valid_result) {
    ResultType is_valid = RESULT_UNKNOWN;
    GrB_Info info = adapter_CFL_is_result_valid_common(state.outputs[0], valid_result, &is_valid);
    if (info < GrB_SUCCESS) {
        fprintf(stderr, "LAGraph failure (file %s, line %d): (%d, msg: %s) \n", __FILE__, __LINE__, info, state.msg);
        return RESULT_UNKNOWN;
    }
    return is_valid;
}

// TODO: now this is the same as adapter_CFL_adv_is_result_valid, make this more generic for other adapters
static size_t adapter_CFL_adv_get_result() {
    size_t result = 0;
    TRY(adapter_CFL_get_result_common(state.outputs[0], &result));
    return result;
}

// free output matrices
//
// this should be called after each run of the algorithm
static GrB_Info adapter_CFL_adv_free_outputs() {
    TRY(adapter_CFL_free_outputs_common(&state.outputs, state.symbols_amount, state.msg));

    return GrB_SUCCESS;
}

// free all resources allocated by the adapter except outputs
//
// this should be called after all runs of the algorithm for the given config
static GrB_Info adapter_CFL_adv_cleanup() {
    TRY(adapter_CFL_cleanup_common(&state.adj_matrices, state.symbols_amount, (void **)&state.rules));

    return GrB_SUCCESS;
}

// free LAGraph\GraphBLAS resources
static GrB_Info adapter_CFL_adv_teardown() {
    TRY(LAGraph_Finalize(state.msg));
    return GrB_SUCCESS;
}

// get the methods of the adapter
AdapterMethods adapter_CFL_adv_get_methods() {
    AdapterMethods methods = {.setup = adapter_CFL_adv_setup,
                              .teardown = adapter_CFL_adv_teardown,
                              .init_outputs = adapter_CFL_adv_init_outputs,
                              .free_outputs = adapter_CFL_adv_free_outputs,
                              .run = adapter_CFL_adv_run,
                              .prepare = adapter_CFL_adv_prepare,
                              .cleanup = adapter_CFL_adv_cleanup,
                              .is_result_valid = adapter_CFL_adv_is_result_valid,
                              .get_result = adapter_CFL_adv_get_result};
    return methods;
}
