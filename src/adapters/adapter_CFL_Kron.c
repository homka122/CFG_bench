#include "adapter_CFL_Kron.h"
#include "GraphBLAS.h"
#include "LAGraph.h"
#include "adapter_CFL_common.h"
#include "parser.h"
#include "rsm.h"
#include <stdlib.h>

#define TRY(GrB_method)                                                                                                \
    {                                                                                                                  \
        GrB_Info LG_GrB_Info = GrB_method;                                                                             \
        if (LG_GrB_Info < GrB_SUCCESS) {                                                                               \
            fprintf(stderr, "LAGraph failure (file %s, line %d): (%d, msg: %s) \n", __FILE__, __LINE__, LG_GrB_Info,   \
                    state.msg);                                                                                        \
            return (LG_GrB_Info);                                                                                      \
        }                                                                                                              \
    }

typedef struct {
    RSM rsm;
    GrB_Matrix *adj_matrices;
    GrB_Matrix *outputs;      
    GrB_Matrix M_intersect;   
    size_t terms_count;      
    size_t nonterms_count;    
    size_t start_nonterm;     
    char msg[LAGRAPH_MSG_LEN];
} state_t;

static state_t state;

static GrB_Info adapter_CFL_setup(void) {
    TRY(LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, state.msg));

    return GrB_SUCCESS;
}

static GrB_Info adapter_CFL_prepare(ParserResult parser_result, void *prepare_data) {
    (void)prepare_data; 

    Grammar grammar = parser_result.grammar;
    Graph graph = parser_result.graph;
    SymbolList list = parser_result.symbols;

    if (parser_result.rsm_template == RSM_NO_TEMPLATE) {
        fprintf(stderr, "RSM template not found\n");
        abort();
    }

    grammar_to_WCNF(&grammar, &list);
    SymbolList terms = symbol_list_create();
    SymbolList nonterms = symbol_list_create();

    grammar_split_terms_nonterms(&grammar, &list, &terms, &nonterms);

    for (size_t i = 0; i < graph.edge_count; i++) {
        graph.edges[i].term_index = symbol_list_get_index_str(&terms, list.symbols[graph.edges[i].term_index].label);
    }

    explode_indices_CFL(&grammar, &graph, &nonterms, &terms);

    CFG_RSM *rsm = rsm_create_template(parser_result.rsm_template, true, parser_result.block_count, &terms);

    GrB_Matrix *prepared_adj_matrices = calloc(rsm->terms.count, sizeof(GrB_Matrix));
    if (prepared_adj_matrices == NULL) {
        fprintf(stderr, "out of memory\n");
        abort();
    }
    for (size_t i = 0; i < rsm->terms.count; i++) {
        TRY(GrB_Matrix_new(prepared_adj_matrices + i, GrB_BOOL, graph.node_count, graph.node_count));
    }

    GrB_Scalar true_scalar;
    TRY(GrB_Scalar_new(&true_scalar, GrB_BOOL));
    TRY(GrB_Scalar_setElement_BOOL(true_scalar, true));

    GrB_Index *row = malloc(sizeof(GrB_Index) * graph.edge_count);
    GrB_Index *col = malloc(sizeof(GrB_Index) * graph.edge_count);
    if ((row == NULL || col == NULL) && graph.edge_count > 0) {
        fprintf(stderr, "out of memory\n");
        abort();
    }
    for (size_t i = 0; i < rsm->terms.count; i++) {
        size_t count = 0;

        for (size_t j = 0; j < graph.edge_count; j++) {
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
    state.terms_count = state.rsm.terminal_count;
    state.nonterms_count = state.rsm.nonterminal_count;
    state.start_nonterm = state.rsm.start_nonterminal;
    state.outputs = NULL;
    state.M_intersect = NULL;

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

static GrB_Info adapter_CFL_init_outputs(void) {
    TRY(adapter_CFL_init_outputs_common(&state.outputs, state.nonterms_count, 0, state.msg));
    state.M_intersect = NULL;

    return GrB_SUCCESS;
}

static GrB_Info adapter_CFL_run(void) {
    TRY(LAGraph_CFL_AllPaths_Kronecker(&state.M_intersect, state.outputs, state.adj_matrices,
                                       (int64_t)state.terms_count, &state.rsm, state.msg));

    return GrB_SUCCESS;
}

static ResultType adapter_CFL_is_result_valid(size_t valid_result) {
    ResultType is_valid = RESULT_UNKNOWN;
    GrB_Info info = adapter_CFL_is_result_valid_common(state.outputs[state.start_nonterm], valid_result, &is_valid);
    if (info < GrB_SUCCESS) {
        fprintf(stderr, "LAGraph failure (file %s, line %d): (%d, msg: %s) \n", __FILE__, __LINE__, info, state.msg);
        return RESULT_UNKNOWN;
    }
    return is_valid;
}

static size_t adapter_CFL_get_result(void) {
    size_t result = 0;
    TRY(adapter_CFL_get_result_common(state.outputs[state.start_nonterm], &result));
    return result;
}

static GrB_Info adapter_CFL_free_outputs(void) {
    TRY(GrB_free(&state.M_intersect));
    TRY(adapter_CFL_free_outputs_common(&state.outputs, state.nonterms_count, state.msg));

    return GrB_SUCCESS;
}

static GrB_Info adapter_CFL_cleanup(void) {
    TRY(adapter_CFL_cleanup_common(&state.adj_matrices, state.terms_count, (void **)NULL));
    rsm_lagraph_rsm_free(&state.rsm);

    return GrB_SUCCESS;
}

static GrB_Info adapter_CFL_teardown(void) {
    TRY(LAGraph_Finalize(state.msg));
    return GrB_SUCCESS;
}

AdapterMethods adapter_CFL_Kron_get_methods(void) {
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
