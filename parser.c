#include <LAGraph.h>
#include <LAGraphX.h>
#include <ctype.h>
#include <parser.h>
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

Symbol symbol_create(char *str) {
    Symbol result;

    size_t len = strlen(str);
    if (len >= 2) {
        result.is_indexed = strcmp(str + len - 2, "_i") == 0;
    } else {
        result.is_indexed = false;
    }

    result.label = malloc((len + 1) * sizeof(char));
    strcpy(result.label, str);
    result.is_nonterm = false;

    return result;
}

void symbol_free(Symbol sym) { free(sym.label); }

SymbolList symbol_list_create() {
    SymbolList result;

    result.symbols = NULL;
    result.count = 0;

    return result;
}

void symbol_list_print(SymbolList list) {
    printf("Symbols list:\n");

    for (size_t i = 0; i < list.count; i++) {
        Symbol *sym = &list.symbols[i];
        char *indexed_str = sym->is_indexed ? " [I]" : "";
        char *nonterm_str = sym->is_nonterm ? " [N]" : " [T]";
        printf("%s%ld:\t%s\t%s\n", nonterm_str, i, sym->label, indexed_str);
    }
}

int symbol_list_add_str(SymbolList *list, char *str, bool is_nonterm) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i].label, str) == 0) {
            if (is_nonterm)
                list->symbols[i].is_nonterm = true;
            return i;
        }
    }

    Symbol new_sym = symbol_create(str);
    list->symbols = realloc(list->symbols, (list->count + 1) * sizeof(Symbol));
    if (is_nonterm)
        new_sym.is_nonterm = is_nonterm;
    list->symbols[list->count] = new_sym;

    return list->count++;
}

int symbol_list_add_str_start_nonterm(SymbolList *list, char *str) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->symbols[i].label, str) == 0) {
            list->symbols[i].is_nonterm = true;
            Symbol temp = list->symbols[0];
            list->symbols[0] = list->symbols[i];
            list->symbols[i] = temp;

            return i;
        }
    }

    Symbol new_sym = symbol_create(str);
    list->symbols = realloc(list->symbols, (list->count + 1) * sizeof(Symbol));
    list->symbols[list->count] = list->symbols[0];
    list->symbols[0] = new_sym;
    new_sym.is_nonterm = true;
    list->count++;

    return (list->count - 1);
}

void symbol_list_free(SymbolList *list) {
    for (size_t i = 0; i < list->count; i++) {
        symbol_free(list->symbols[i]);
    }

    free(list->symbols);
}

bool is_non_terminal(const Symbol symbol, SymbolList non_terms) {
    for (size_t i = 0; i < non_terms.count; ++i) {
        if (strcmp(non_terms.symbols[i].label, symbol.label) == 0) {
            return true;
        }
    }

    return false;
}

bool is_number(const char *s) {
    if (s == NULL || *s == '\0')
        return false;
    while (*s) {
        if (!isdigit(*s))
            return false;
        s++;
    }
    return true;
}

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
Grammar process_grammar(char *grammar_text, SymbolList *symbol_list) {
    Rule *rules = NULL;
    size_t rules_count = 0;

    char *line = grammar_text;
    while (*line) {
        // don't blame me for this code, i know
        char *end = strchr(line, '\n');
        if (end)
            *end = '\0';

        char *first = strtok(line, " \t");
        char *second = strtok(NULL, " \t");
        char *third = strtok(NULL, " \t");

        if (first == NULL) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        if (strcmp(first, "Count:") == 0) {
            line = end ? end + 1 : line + strlen(line);
            char *end = strchr(line, '\n');
            if (end)
                *end = '\0';

            int replaced_symbol = symbol_list_add_str_start_nonterm(symbol_list, line);

            for (size_t rule_i = 0; rule_i < rules_count; rule_i++) {
                int *first = &rules[rule_i].first;
                int *second = &rules[rule_i].second;
                int *third = &rules[rule_i].third;

                if (*first == 0) {
                    *first = replaced_symbol;
                } else if (*first == replaced_symbol) {
                    *first = 0;
                }

                if (*second == 0) {
                    *second = replaced_symbol;
                } else if (*second == replaced_symbol) {
                    *second = 0;
                }

                if (*third == 0) {
                    *third = replaced_symbol;
                } else if (*third == replaced_symbol) {
                    *third = 0;
                }
            }

            break;
        }

        if (first == NULL) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        };

        Rule rule;
        rule.first = symbol_list_add_str(symbol_list, first, true);
        rule.second = second == NULL ? -1 : symbol_list_add_str(symbol_list, second, false);
        rule.third = third == NULL ? -1 : symbol_list_add_str(symbol_list, third, false);

        rules = realloc(rules, (rules_count + 1) * sizeof(Rule));
        rules[rules_count] = rule;
        rules_count++;

        line = end ? end + 1 : line + strlen(line);
    }

    return (Grammar){0, rules, rules_count};
}

// Accept edges NULL prointer and create new array
// Input: graphText
// Output: edges array (init with malloc), count
Graph process_graph(char *graph_text, SymbolList *symbol_list) {
    Graph result = {.edges = NULL, .edge_count = 0, .node_count = 0, .block_count = 1};

    char *line = graph_text;
    size_t capacity = 128;
    result.edges = malloc(capacity * sizeof(GraphEdge));
    while (*line) {
        char *end = strchr(line, '\n');
        if (end)
            *end = '\0';

        char *saveptr;
        char *u_str = strtok_r(line, " \t", &saveptr);
        char *v_str = strtok_r(NULL, " \t", &saveptr);
        char *a_str = strtok_r(NULL, " \t", &saveptr);
        char *index_str = strtok_r(NULL, " \t", &saveptr);
        char *rest = strtok_r(NULL, " \t", &saveptr);

        if (!u_str || !a_str || !v_str || rest != NULL) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        if (!is_number(u_str) || !is_number(v_str) || (index_str != NULL && !is_number(index_str))) {
            fprintf(stderr, "error: %s, %s, %s", u_str, v_str, index_str);
            exit(-1);
        }

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

        line = end ? end + 1 : line + strlen(line);
    }

    return result;
}

ParserResult parser(config_row config_i) {
    char *config_graph = strdup(config_i.graph);
    char *config_grammar = strdup(config_i.grammar);

    // printf("Reading graph file...");
    char *graph_buf = read_entire_file(config_graph);
    // printf("OK\n");
    fflush(stdout);

    // printf("Reading grammar file...");
    char *grammar_buf = read_entire_file(config_grammar);
    // printf("OK\n");
    fflush(stdout);

    SymbolList list = symbol_list_create();

    // printf("Process grammar...");
    Grammar _grammar = process_grammar(grammar_buf, &list);
    // printf("OK\n");
    fflush(stdout);

    // printf("Process graph...");
    Graph graph = process_graph(graph_buf, &list);
    // printf("OK\n");
    fflush(stdout);

#if false
    symbol_list_print(list);
    printf("\n");
    grammar_print(_grammar, list);

    printf("\nGraph Info:\n");
    printf("Number of nodes: %d\n", graph.node_count);
    printf("Edges: %d\n", graph.edge_count);
#endif

    free(graph_buf);
    free(grammar_buf);
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