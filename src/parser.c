#include "parser.h"
#include <LAGraph.h>
#include <LAGraphX.h>
#include <ctype.h>
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

static int rule_new_index(size_t *map, Symbol *sym, int index, size_t offset) {
    if (index == -1) {
        return -1;
    }

    if (sym->is_indexed) {
        return map[index] + offset;
    } else {
        return map[index];
    }
}

static bool is_rule_has_indexed_symb(Rule rule, SymbolList list) {
    bool first = rule.first != -1 && (list.symbols[rule.first].is_indexed);
    bool second = rule.second != -1 && (list.symbols[rule.second].is_indexed);
    bool third = rule.third != -1 && (list.symbols[rule.third].is_indexed);

    return first || second || third;
}

// before: homka_i
// after: homka_1
//        homka_2
//        homka_3
void explode_indices(Grammar *grammar, Graph *graph, SymbolList *list) {
    size_t *map = NULL;
    size_t map_size = 0;
    SymbolList new_list = symbol_list_create();
    SymbolList old_list = *list;

    for (size_t i = 0; i < list->count; i++) {
        Symbol sym = list->symbols[i];

        if (map_size == i) {
            map_size = map_size * 2 + 1;
            map = realloc(map, map_size * sizeof(size_t));
        }

        // just add to new list and map to it
        if (!sym.is_indexed) {
            map[i] = symbol_list_add_str(&new_list, sym.label, sym.is_nonterm);
            continue;
        }

        map[i] = symbol_list_add_str(&new_list, symbol_numerate(&sym, 0), sym.is_nonterm);
        for (size_t j = 1; j < graph->block_count; j++) {
            char *new_label = symbol_numerate(&sym, j);
            symbol_list_add_str(&new_list, new_label, sym.is_nonterm);
        }
    }
    *list = new_list;

    Rule *new_rules = NULL;
    size_t rules_size = 0;
    size_t rules_count = 0;
    for (size_t i = 0; i < grammar->rules_count; i++) {
        if (rules_size + graph->block_count + 2 >= rules_count) {
            rules_size = rules_size * 2 + graph->block_count + 2;
            new_rules = realloc(new_rules, rules_size * sizeof(Rule));
        }

        Rule rule = grammar->rules[i];
        Symbol *first = rule.first == -1 ? NULL : &old_list.symbols[rule.first];
        Symbol *second = rule.second == -1 ? NULL : &old_list.symbols[rule.second];
        Symbol *third = rule.third == -1 ? NULL : &old_list.symbols[rule.third];
        if (!is_rule_has_indexed_symb(rule, old_list)) {
            new_rules[rules_count] =
                (Rule){rule_new_index(map, first, rule.first, 0), rule_new_index(map, second, rule.second, 0),
                       rule_new_index(map, third, rule.third, 0)};
            rules_count++;
            continue;
        }

        for (size_t j = 0; j < graph->block_count; j++) {
            new_rules[rules_count] =
                (Rule){rule_new_index(map, first, rule.first, j), rule_new_index(map, second, rule.second, j),
                       rule_new_index(map, third, rule.third, j)};
            rules_count++;
        }
    }
    grammar->rules = new_rules;
    grammar->rules_count = rules_count;

    for (size_t i = 0; i < graph->edge_count; i++) {
        GraphEdge *edge = &graph->edges[i];

        edge->term_index = map[edge->term_index] + edge->index;
        edge->index = 0;
    }
    graph->block_count = 1;

    free(map);

    return;
}

GrB_Matrix *get_grb_matrices_from_graph(Graph graph, SymbolList *list) {
    explode_indices(&(Grammar){.rules = NULL, .rules_count = 0}, &graph, list);
    SymbolData *symbol_datas = calloc(list->count, sizeof(SymbolData));
    for (size_t i = 0; i < list->count; i++) {
        symbol_datas[i] = symbol_data_create();
    }

    for (size_t i = 0; i < graph.edge_count; i++) {
        GraphEdge edge = graph.edges[i];
        symbol_data_add(&symbol_datas[edge.term_index], edge.u, edge.v, edge.index);
    }

    GrB_Matrix *matrices = malloc(sizeof(GrB_Matrix) * list->count);
    for (size_t i = 0; i < list->count; i++) {
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

    for (size_t i = 0; i < list->count; i++) {
        symbol_data_free(&symbol_datas[i]);
    }

    free(symbol_datas);

    return matrices;
}

ParserResult parser(config_row config_i) {
    char *config_graph = strdup(config_i.graph);
    char *config_grammar = strdup(config_i.grammar);

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

    return (ParserResult){.block_count = graph.block_count,
                          .node_count = graph.node_count,
                          .grammar = _grammar,
                          .symbols = list,
                          .graph = graph};
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
