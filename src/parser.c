#include "parser.h"
#include <LAGraph.h>
#include <LAGraphX.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

char *read_entire_file(char *path) {

    FILE *file = fopen(path, "rb");
    if (file == NULL) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m Problem with opening file by %s path\n", path);
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);
    if (fread(buffer, 1, file_size, file) == 0) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m Problem with reading file by %s path\n", path);
        exit(-1);
    };
    buffer[file_size] = '\0';
    fclose(file);

    return buffer;
}

size_t get_text_lines(char *text, char ***lines_arg) {
    size_t lines_count = 0;
    size_t capacity = 1024;
    char **lines = calloc(capacity, sizeof(char *));
    lines[lines_count++] = text;
    size_t text_len = strlen(text);

    for (size_t i = 0; i < text_len; i++) {
        if (text[i] == '\n' && i + 1 < text_len) {
            if (i > 0 && text[i - 1] == '\n') {
                continue;
            }

            if (lines_count == capacity) {
                capacity *= 2;
                lines = realloc(lines, capacity * sizeof(char *));
            }

            lines[lines_count++] = &text[i + 1];
        }
    }

    *lines_arg = lines;
    return lines_count;
}

Graph process_graph(FILE *graph_file, SymbolList *symbol_list) {
    if (symbol_list == NULL) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m symbol_list is NULL\n");
        exit(-1);
    }

    Graph result = {.edges = NULL, .edge_count = 0, .node_count = 0, .block_count = 1};

    char line[1024];
    size_t capacity = 128;
    result.edges = calloc(capacity, sizeof(GraphEdge));

    while (fgets(line, sizeof(line), graph_file)) {
        if (line[0] == '\n') {
            continue;
        }

        char *saveptr;
        char *u_str = strtok_r(line, " \t\n", &saveptr);
        char *v_str = strtok_r(NULL, " \t\n", &saveptr);
        char *a_str = strtok_r(NULL, " \t\n", &saveptr);
        char *index_str = strtok_r(NULL, " \t\n", &saveptr);
        char *rest = strtok_r(NULL, " \t\n", &saveptr);

        if (!u_str || !a_str || !v_str || rest != NULL) {
            continue;
        }

        // JUST BELIEVE THAT USER DON'T USE INSTEAD OF NUMBERS SOMETHING ELSE
        // cause this check is really expensive
        //
        // if (!is_number(u_str) || !is_number(v_str) || (index_str != NULL && !is_number(index_str))) {
        //     fprintf(stderr, "error: %s, %s, %s", u_str, v_str, index_str);
        //     exit(-1);
        // }

        size_t u = (size_t)atoi(u_str);
        size_t v = (size_t)atoi(v_str);
        size_t index = index_str == NULL ? 0 : (size_t)atoi(index_str);

        int term_index = symbol_list_add_str(symbol_list, a_str, false);

        result.node_count = MAX(result.node_count, v + 1);
        result.node_count = MAX(result.node_count, u + 1);
        result.block_count = MAX(result.block_count, index + 1);

        if (result.edge_count >= capacity) {
            result.edges = realloc(result.edges, capacity * 2 * sizeof(GraphEdge));
            capacity *= 2;
        }

        result.edges[result.edge_count++] = (GraphEdge){u, v, term_index, index};
    }

    return result;
}

/*
 * build the matrix-space view of a SymbolList.
 *
 * public metadata is stored as matrix_id -> {symbol_index, block_index}
 *
 * when first_matrix_by_symbol_arg is not NULL, this helper also builds the
 * private forward index symbol_index -> first matrix_id for that symbol:
 *
 *   matrix_id = first_matrix_by_symbol[edge.term_index]
 *   if symbol is indexed, matrix_id += edge.index
 */
static ExplodedIndices build_exploded_indices(const SymbolList *list, size_t block_count,
                                              size_t **first_matrix_by_symbol_p) {
    if (list == NULL) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m symbol_list is NULL\n");
        exit(-1);
    }

    if (block_count == 0) {
        block_count = 1;
    }

    size_t count = 0;
    for (size_t i = 0; i < list->count; i++) {
        count += list->symbols[i].is_indexed ? block_count : 1;
    }

    ExplodedIndices result = {.items = calloc(count, sizeof(MatrixSymbolInfo)), .count = count};
    size_t *first_matrix_by_symbol = NULL;
    if (first_matrix_by_symbol_p != NULL) {
        first_matrix_by_symbol = calloc(list->count, sizeof(size_t));
    }

    size_t matrix_index = 0;
    for (size_t i = 0; i < list->count; i++) {
        Symbol sym = list->symbols[i];
        size_t symbol_matrix_count = sym.is_indexed ? block_count : 1;

        if (first_matrix_by_symbol != NULL) {
            first_matrix_by_symbol[i] = matrix_index;
        }

        for (size_t block_index = 0; block_index < symbol_matrix_count; block_index++) {
            result.items[matrix_index++] = (MatrixSymbolInfo){.symbol_index = i, .block_index = block_index};
        }
    }

    if (first_matrix_by_symbol_p != NULL) {
        *first_matrix_by_symbol_p = first_matrix_by_symbol;
    }

    return result;
}

ExplodedIndices explode_indices(const SymbolList *list, size_t block_count) {
    return build_exploded_indices(list, block_count, NULL);
}

void exploded_indices_free(ExplodedIndices *indices) {
    if (indices == NULL) {
        return;
    }

    free(indices->items);
    indices->items = NULL;
    indices->count = 0;
}

// minimize graph nodes
//
// for example with 1000x1000 graph and only edge 0 -> 600 it makes 2x2 graph with edge 0 -> 1 with same label
void *minimize_graph(Graph *graph) {
    if (graph == NULL) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m graph is NULL\n");
        exit(-1);
    }

    size_t old_node_count = graph->node_count;
    for (size_t i = 0; i < graph->edge_count; i++) {
        old_node_count = MAX(old_node_count, graph->edges[i].u + 1);
        old_node_count = MAX(old_node_count, graph->edges[i].v + 1);
    }

    if (old_node_count == 0) {
        graph->node_count = 0;
        return NULL;
    }

    size_t *map = malloc(old_node_count * sizeof(*map));
    if (map == NULL) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m out of memory\n");
        abort();
    }

    for (size_t i = 0; i < old_node_count; i++) {
        map[i] = (size_t)-1;
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        map[graph->edges[i].u] = 0;
        map[graph->edges[i].v] = 0;
    }

    size_t new_node_count = 0;
    for (size_t i = 0; i < old_node_count; i++) {
        if (map[i] != (size_t)-1) {
            map[i] = new_node_count++;
        }
    }

    for (size_t i = 0; i < graph->edge_count; i++) {
        graph->edges[i].u = map[graph->edges[i].u];
        graph->edges[i].v = map[graph->edges[i].v];
    }

    graph->node_count = new_node_count;

    return map;
}

GraphMatrices get_grb_matrices_from_graph(Graph graph, const SymbolList *list) {
    size_t block_count = graph.block_count == 0 ? 1 : graph.block_count;
    size_t *first_matrix_by_symbol = NULL;
    ExplodedIndices indices = build_exploded_indices(list, block_count, &first_matrix_by_symbol);
    SymbolData *symbol_datas = calloc(indices.count, sizeof(SymbolData));
    for (size_t i = 0; i < indices.count; i++) {
        symbol_datas[i] = symbol_data_create();
    }

    for (size_t i = 0; i < graph.edge_count; i++) {
        GraphEdge edge = graph.edges[i];
        if (edge.term_index >= list->count) {
            fprintf(stderr, "\x1B[31m[ERROR]\033[0m graph edge term index out of bounds: index=%zu, count=%zu\n",
                    edge.term_index, list->count);
            exit(-1);
        }

        Symbol symbol = list->symbols[edge.term_index];
        if (symbol.is_indexed && edge.index >= block_count) {
            fprintf(stderr, "\x1B[31m[ERROR]\033[0m graph edge block index out of bounds: index=%zu, count=%zu\n",
                    edge.index, block_count);
            exit(-1);
        }

        size_t matrix_index = first_matrix_by_symbol[edge.term_index];
        if (symbol.is_indexed) {
            matrix_index += edge.index;
        }

        symbol_data_add(&symbol_datas[matrix_index], edge.u, edge.v, edge.index);
    }

    GrB_Matrix *matrices = malloc(sizeof(GrB_Matrix) * indices.count);
    for (size_t i = 0; i < indices.count; i++) {
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

    for (size_t i = 0; i < indices.count; i++) {
        symbol_data_free(&symbol_datas[i]);
    }

    free(symbol_datas);
    free(first_matrix_by_symbol);

    return (GraphMatrices){.matrices = matrices, .matrix_symbols = indices.items, .count = indices.count};
}

void graph_matrices_free(GraphMatrices *result) {
    if (result == NULL) {
        return;
    }

    if (result->matrices != NULL) {
        for (size_t i = 0; i < result->count; i++) {
            GrB_free(&result->matrices[i]);
        }
    }

    free(result->matrices);
    free(result->matrix_symbols);
    result->matrices = NULL;
    result->matrix_symbols = NULL;
    result->count = 0;
}

static const char *basename(const char *path) {
    const char *last_slash = strrchr(path, '/');

    if (last_slash == NULL) {
        return path;
    }

    return last_slash + 1;
}

bool rsm_template_from_string(const char *name, RSM_Template *out) {
    if (name == NULL || out == NULL) {
        return false;
    }

    const char *base = basename(name);

    if (strcmp(base, "aa.cnf") == 0) {
        *out = RSM_TEMPLATE_AA;
        return true;
    }

    if (strcmp(base, "c_alias.cnf") == 0) {
        *out = RSM_TEMPLATE_C_ALIAS;
        return true;
    }

    if (strcmp(base, "java_points_to.cnf") == 0) {
        *out = RSM_TEMPLATE_JAVA_POINTS_TO;
        return true;
    }

    if (strcmp(base, "vf.cnf") == 0) {
        *out = RSM_TEMPLATE_VF;
        return true;
    }

    if (strcmp(base, "nested_parentheses_subClassOf_type.cnf") == 0 || strcmp(base, "rdf_hierarchy.dot") == 0) {
        *out = RSM_TEMPLATE_RDF_HIERARCHY;
        return true;
    }

    *out = RSM_NO_TEMPLATE;

    return false;
}

ParserResult parser(config_row config_i) {
    char *config_graph = strdup(config_i.graph);
    char *config_grammar = strdup(config_i.grammar);

    RSM_Template template = RSM_NO_TEMPLATE;
    rsm_template_from_string(config_grammar, &template);

    // printf("Reading graph file...");
    // char *graph_buf = read_entire_file(config_graph);
    // printf("OK\n");
    // fflush(stdout);

    SymbolList list = symbol_list_create();

    // printf("Process grammar...");
    FILE *grammar_file = fopen(config_grammar, "r");
    Grammar _grammar = process_grammar(grammar_file, &list);
    grammar_swap_symbols(&_grammar, 0, _grammar.start_nonterm);
    symbol_list_swap(&list, 0, _grammar.start_nonterm);
    _grammar.start_nonterm = 0;
    // printf("OK\n");

    // printf("Process graph...");
    FILE *graph_file = fopen(config_graph, "r");
    Graph graph = process_graph(graph_file, &list);
    // printf("OK\n");

#if false
    symbol_list_print(list);
    printf("\n");
    grammar_print(_grammar, list);

    printf("\nGraph Info:\n");
    printf("Number of nodes: %d\n", graph.node_count);
    printf("Edges: %d\n", graph.edge_count);
#endif

    fclose(graph_file);
    fclose(grammar_file);
    free(config_grammar);
    free(config_graph);

    return (ParserResult){
        .block_count = graph.block_count,
        .node_count = graph.node_count,
        .grammar = _grammar,
        .symbols = list,
        .graph = graph,
        .rsm_template = template,
    };
}

void get_configs_from_file(char *path, size_t *configs_count, config_row *configs, char **text_p) {
    *configs_count = 0;
    char *config_text = read_entire_file(path);
    *text_p = config_text;

    char *line = config_text;
    bool last = false;
    while (!last) {
        char *end = strchrnul(line, '\n');
        if (*end == '\0')
            last = true;
        *end = '\0';

        char *graph = strtok(line, ",");
        char *grammar = strtok(NULL, ",");
        char *valid_result_str = strtok(NULL, ",");

        if (graph == NULL || grammar == NULL || valid_result_str == NULL)
            break;

        size_t valid_result = atoi(valid_result_str);

        configs[(*configs_count)++] = (config_row){.grammar = grammar, .graph = graph, .valid_result = valid_result};
        line = end + 1;
    }
}
