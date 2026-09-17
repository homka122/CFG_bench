#include "../src/parser.h"
#include <GraphBLAS.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>

static void assert_matrix_has(GrB_Matrix matrix, GrB_Index row, GrB_Index col) {
    bool value = false;
    GrB_Info info = GrB_Matrix_extractElement_BOOL(&value, matrix, row, col);
    assert(info == GrB_SUCCESS);
    assert(value);
}

static void test_explode_indices_metadata(void) {
    SymbolList list = symbol_list_create();
    int a = symbol_list_add_str(&list, "a", false);
    int op = symbol_list_add_str(&list, "op_i", false);
    assert(a != -1);
    assert(op != -1);

    ExplodedIndices indices = explode_indices(&list, 3);

    assert(indices.count == 4);
    assert(indices.items[0].symbol_index == (size_t)a);
    assert(indices.items[0].block_index == 0);
    assert(indices.items[1].symbol_index == (size_t)op);
    assert(indices.items[1].block_index == 0);
    assert(indices.items[2].symbol_index == (size_t)op);
    assert(indices.items[2].block_index == 1);
    assert(indices.items[3].symbol_index == (size_t)op);
    assert(indices.items[3].block_index == 2);
    assert(strcmp(symbol_list_get_str(&list, (size_t)op), "op_i") == 0);

    exploded_indices_free(&indices);
    symbol_list_free(&list);
}

static void test_graph_matrices_metadata(void) {
    SymbolList list = symbol_list_create();
    int a = symbol_list_add_str(&list, "a", false);
    int op = symbol_list_add_str(&list, "op_i", false);
    assert(a != -1);
    assert(op != -1);

    GraphEdge edges[] = {
        {.u = 0, .v = 1, .term_index = (size_t)op, .index = 0},
        {.u = 1, .v = 2, .term_index = (size_t)op, .index = 1},
        {.u = 2, .v = 0, .term_index = (size_t)a, .index = 1},
    };
    Graph graph = {.edges = edges, .edge_count = 3, .node_count = 3, .block_count = 2};

    GraphMatrices result = get_grb_matrices_from_graph(graph, &list);

    assert(result.count == 3);
    assert(result.matrix_symbols[0].symbol_index == (size_t)a);
    assert(result.matrix_symbols[0].block_index == 0);
    assert(result.matrix_symbols[1].symbol_index == (size_t)op);
    assert(result.matrix_symbols[1].block_index == 0);
    assert(result.matrix_symbols[2].symbol_index == (size_t)op);
    assert(result.matrix_symbols[2].block_index == 1);

    assert_matrix_has(result.matrices[0], 2, 0);
    assert_matrix_has(result.matrices[1], 0, 1);
    assert_matrix_has(result.matrices[2], 1, 2);

    assert(strcmp(symbol_list_get_str(&list, (size_t)op), "op_i") == 0);

    graph_matrices_free(&result);
    symbol_list_free(&list);
}

static void test_parser_result_copy_is_deep(void) {
    ParserResult original = {0};
    original.symbols = symbol_list_create();
    int nonterm = symbol_list_add_str(&original.symbols, "S", true);
    int term = symbol_list_add_str(&original.symbols, "a", false);
    grammar_add_rule(&original.grammar, nonterm, term, -1);
    original.grammar.start_nonterm = nonterm;

    original.graph.edges = malloc(sizeof(GraphEdge));
    original.graph.edges[0] = (GraphEdge){.u = 0, .v = 1, .term_index = (size_t)term, .index = 0};
    original.graph.edge_count = 1;
    original.graph.node_count = 2;
    original.graph.block_count = 1;
    original.node_count = 2;
    original.block_count = 1;

    ParserResult copy = parser_result_copy(&original);

    assert(copy.grammar.rules != original.grammar.rules);
    assert(copy.symbols.symbols != original.symbols.symbols);
    assert(copy.symbols.symbols[0].label != original.symbols.symbols[0].label);
    assert(copy.graph.edges != original.graph.edges);

    copy.grammar.rules[0].first = -1;
    copy.symbols.symbols[0].label[0] = 'X';
    copy.graph.edges[0].u = 42;

    assert(original.grammar.rules[0].first == nonterm);
    assert(strcmp(original.symbols.symbols[0].label, "S") == 0);
    assert(original.graph.edges[0].u == 0);

    free_parser_result(&copy);
    free_parser_result(&original);
}

int main(void) {
    test_explode_indices_metadata();
    test_parser_result_copy_is_deep();

    GrB_init(GrB_NONBLOCKING);
    test_graph_matrices_metadata();
    GrB_finalize();

    return 0;
}
