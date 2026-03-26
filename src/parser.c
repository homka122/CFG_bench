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

typedef struct {
    int lhs;
    int rhs1;
    int rhs2;
    int index;
} ProductionRule;

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

void grammar_print(Grammar grammar, SymbolList list) {
    printf("Grammar:\n");

    printf("Start nonterm: %s\n", list.symbols[grammar.start_nonterm].label);

    for (size_t i = 0; i < grammar.rules_count; i++) {
        int first = grammar.rules[i].first;
        int second = grammar.rules[i].second;
        int third = grammar.rules[i].third;

        if (third != -1) {
            printf("%s -> %s %s\n", list.symbols[first].label, list.symbols[second].label, list.symbols[third].label);
            continue;
        }

        if (second != -1) {
            printf("%s -> %s\n", list.symbols[first].label, list.symbols[second].label);
            continue;
        }

        printf("%s ->\n", list.symbols[first].label);
    }

    printf("\n");
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

// TODO: if there is no Count line
Grammar process_grammar(FILE *grammar_file, SymbolList *symbol_list) {
    size_t rules_count = 0;
    size_t rules_capacity = 1024;
    Rule *rules = calloc(rules_capacity, sizeof(Rule));

    int start_nonterm = -1;
    char line[1024];

    while (fgets(line, sizeof(line), grammar_file)) {
        if (line[0] == '\n') {
            continue;
        }

        if (rules_count == rules_capacity) {
            rules_capacity *= 2;
            rules = realloc(rules, rules_capacity * sizeof(Rule));
        }

        char *first = strtok(line, " \t\n");
        char *second = strtok(NULL, " \t\n");
        char *third = strtok(NULL, " \t\n");

        if (strcmp(first, "Count:") == 0) {
            fgets(line, sizeof(line), grammar_file);
            char *first = strtok(line, " \t\n");
            start_nonterm = symbol_list_add_str(symbol_list, first, true);

            break;
        }

        int first_symbol = symbol_list_add_str(symbol_list, first, true);
        int second_symbol = second ? symbol_list_add_str(symbol_list, second, false) : -1;
        int third_symbol = third ? symbol_list_add_str(symbol_list, third, false) : -1;

        rules[rules_count++] = (Rule){first_symbol, second_symbol, third_symbol};
    }

    if (start_nonterm == -1) {
        fprintf(stderr, "\x1B[31m[ERROR]\033[0m no start nonterminal in grammar file\n");
        exit(-1);
    }

    return (Grammar){start_nonterm, rules, rules_count};
}

void grammar_swap_symbols(Grammar *grammar, int sym1, int sym2) {
    for (size_t i = 0; i < grammar->rules_count; i++) {
        if (grammar->rules[i].first == sym1) {
            grammar->rules[i].first = sym2;
        } else if (grammar->rules[i].first == sym2) {
            grammar->rules[i].first = sym1;
        }

        if (grammar->rules[i].second == sym1) {
            grammar->rules[i].second = sym2;
        } else if (grammar->rules[i].second == sym2) {
            grammar->rules[i].second = sym1;
        }

        if (grammar->rules[i].third == sym1) {
            grammar->rules[i].third = sym2;
        } else if (grammar->rules[i].third == sym2) {
            grammar->rules[i].third = sym1;
        }
    }
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

void get_configs_from_file(char *path, size_t *configs_count, config_row *configs) {
    *configs_count = 0;
    char *config_text = read_entire_file(path);

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