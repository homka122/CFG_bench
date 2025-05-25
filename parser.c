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

    return list->count++;
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

// void process_rule(const char *line, SymbolList *non_terms, SymbolList *terms,
//                   ProductionRule *rules, int *rule_idx) {
//     char *arrow = strstr(line, "->");
//     if (!arrow)
//         return;

//     char lhs[4] = {0};
//     strncpy(lhs, line, arrow - line);
//     lhs[strcspn(lhs, " \t\n")] = '\0';

//     char *rhs = arrow + 2;
//     while (*rhs == ' ' || *rhs == '\t')
//         rhs++;
//     rhs[strcspn(rhs, "\n")] = '\0';

//     ProductionRule rule = {lhs_id, -1, -1, 0};

//     if (strlen(rhs) == 0) {
//         rules[(*rule_idx)++] = rule;
//         return;
//     }

//     char *symbols[2] = {NULL, NULL};
//     int count = 0;
//     char *token = strtok(rhs, " ");
//     while (token && count < 2) {
//         symbols[count++] = token;
//         token = strtok(NULL, " ");
//     }

//     for (int i = 0; i < count; ++i) {
//         SymbolList *arr = is_non_terminal(symbol_create(symbols[i]),
//         *non_terms)
//                               ? non_terms
//                               : terms;
//         int id = add_symbol(arr, symbol_create(symbols[i]));
//         if (i == 0)
//             rule.rhs1 = id;
//         else
//             rule.rhs2 = id;
//     }

//     rules[(*rule_idx)++] = rule;
// }

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
        fprintf(stderr,
                "\x1B[31m[ERROR]\033[0m Problem with opening file by %s path\n",
                path);
        exit(-1);
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *buffer = (char *)malloc(file_size + 1);
    if (fread(buffer, 1, file_size, file) == 0) {
        fprintf(stderr,
                "\x1B[31m[ERROR]\033[0m Problem with reading file by %s path\n",
                path);
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
            printf("%s -> %s %s\n", list.symbols[first].label,
                   list.symbols[second].label, list.symbols[third].label);
            continue;
        }

        if (second != -1) {
            printf("%s -> %s\n", list.symbols[first].label,
                   list.symbols[second].label);
            continue;
        }

        printf("%s ->\n", list.symbols[first].label);
    }

    printf("\n");
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

            int replaced_symbol =
                symbol_list_add_str_start_nonterm(symbol_list, line);

            for (size_t rule_i = 0; rule_i < rules_count; rule_i++) {
                int *first = &rules[rule_i].first;
                int *second = &rules[rule_i].second;
                int *third = &rules[rule_i].third;

                *first = *first == 0 ? replaced_symbol : *first;
                *second = *second == 0 ? replaced_symbol : *second;
                *third = *third == 0 ? replaced_symbol : *third;
            }

            break;
        }

        if (first == NULL) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        };

        Rule rule;
        rule.first = symbol_list_add_str(symbol_list, first, true);
        rule.second = second == NULL
                          ? -1
                          : symbol_list_add_str(symbol_list, second, false);
        rule.third =
            third == NULL ? -1 : symbol_list_add_str(symbol_list, third, false);

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
    Graph result = {
        .edges = NULL, .edge_count = 0, .node_count = 0, .block_count = 1};

    char *line = graph_text;
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

        if (!is_number(u_str) || !is_number(v_str) ||
            (index_str != NULL && !is_number(index_str))) {
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

        result.edges =
            realloc(result.edges, (result.edge_count + 1) * sizeof(GraphEdge));
        result.edges[result.edge_count++] =
            (GraphEdge){u, v, term_index, index};

        line = end ? end + 1 : line + strlen(line);
    }

    return result;
}

ParserResult parser(char *config_i) {
    char *config = strdup(config_i);
    char *graph_buf = read_entire_file(strtok(config, ","));
    char *grammar_buf = read_entire_file(strtok(NULL, ","));
    SymbolList list = symbol_list_create();

    Grammar _grammar = process_grammar(grammar_buf, &list);
    Graph graph = process_graph(graph_buf, &list);

    // LAGraph_rule_WCNF *rules_WCNF =
    //     calloc(rule_count, sizeof(LAGraph_rule_WCNF));

    // for (int i = 0; i < rule_count; ++i) {
    //     ProductionRule r = rules[i];
    //     rules_WCNF[i] = (LAGraph_rule_WCNF){r.lhs, r.rhs1, r.rhs2, r.index};
    // }
    // *grammar = (grammar_t){.nonterms_count = non_terms.count,
    //                        .terms_count = terms.count,
    //                        .rules_count = rule_count,
    //                        .rules = rules_WCNF};

    // *adj_matrices = calloc(grammar->terms_count, sizeof(GrB_Matrix));
    // for (size_t i = 0; i < terms.count; i++) {
    //     GrB_Matrix_new((*adj_matrices) + i, GrB_BOOL, max_node + 1,
    //                    max_node + 1);
    // }

    // GrB_Scalar true_scalar;
    // GrB_Scalar_new(&true_scalar, GrB_BOOL);
    // GrB_Scalar_setElement_BOOL(true_scalar, true);

    // GrB_Index *row = malloc(sizeof(GrB_Index) * edge_count);
    // GrB_Index *col = malloc(sizeof(GrB_Index) * edge_count);
    // for (size_t i = 0; i < terms.count; i++) {
    //     int count = 0;

    //     for (int j = 0; j < edge_count; ++j) {
    //         // if (i == edges[j].term_index) {
    //         //     row[count] = edges[j].u;
    //         //     col[count] = edges[j].v;
    //         //     count++;
    //         // }
    //     }

    //     GxB_Matrix_build_Scalar((*adj_matrices)[i], row, col, true_scalar,
    //                             count);
    // #ifdef DEBUG_parser
    //     GxB_print((*adj_matrices)[i], 1);
    // #endif
    // }

    // free(row);
    // free(col);
    // GrB_free(&true_scalar);

#if false
    symbol_list_print(list);
    printf("\n");
    grammar_print(_grammar, list);
    // printf("Non-terminals:\n");
    // for (int i = 0; i < non_terms.count; ++i)
    //     printf("%d: %s\n", i, non_terms.symbols[i]);

    // printf("\nTerminals:\n");
    // for (int i = 0; i < terms.count; ++i)
    //     printf("%d: %s\n", i, terms.symbols[i]);

    // printf("\nProductions:\n");
    // for (int i = 0; i < rule_count; ++i) {
    //     ProductionRule r = rules[i];
    //     printf("[%d, %d, %d, %d]\n", r.lhs, r.rhs1, r.rhs2, r.index);
    // }

    // printf("\nProductions (With names):\n");
    // for (int i = 0; i < rule_count; ++i) {
    //     ProductionRule r = rules[i];
    //     printf("[%d] %s -> %s %s\n", r.index, non_terms.symbols[r.lhs],
    //            r.rhs1 == -1   ? ""
    //            : r.rhs2 == -1 ? terms.symbols[r.rhs1]
    //                           : non_terms.symbols[r.rhs1],
    //            r.rhs2 == -1 ? "" : non_terms.symbols[r.rhs2]);
    // }

    printf("\nGraph Info:\n");
    // printf("Number of nodes: %d\n", max_node + 1);
    // printf("Edges: %d\n", edge_count);
    // printf("Graph lines: %d\n", graph_line_count);
#endif

    // free(rules);

    // free(edges);
    free(graph_buf);
    free(grammar_buf);
    free(config);

    return (ParserResult){.block_count = graph.block_count,
                          .node_count = graph.node_count,
                          .grammar = _grammar,
                          .symbols = list,
                          .graph = graph};
}