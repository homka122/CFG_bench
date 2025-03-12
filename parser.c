#include <LAGraph.h>
#include <LAGraphX.h>
#include <ctype.h>
#include <parser.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE_LEN 256

typedef struct {
    char **symbols;
    int count;
} SymbolList;

typedef struct {
    int lhs;
    int rhs1;
    int rhs2;
    int index;
} ProductionRule;

typedef struct {
    int u;
    int term_index;
    int v;
} GraphEdge;

void init_symbol_list(SymbolList *list) {
    list->symbols = NULL;
    list->count = 0;
}

int add_symbol(SymbolList *list, const char *symbol) {
    for (int i = 0; i < list->count; ++i) {
        if (strcmp(list->symbols[i], symbol) == 0) {
            return i;
        }
    }

    list->symbols = realloc(list->symbols, sizeof(char *) * (list->count + 1));
    list->symbols[list->count] = strdup(symbol);

    return list->count++;
}

void free_symbol_list(SymbolList *list) {
    for (int i = 0; i < list->count; ++i) {
        free(list->symbols[i]);
    }

    free(list->symbols);
    init_symbol_list(list);
}

bool is_non_terminal(const char *symbol, SymbolList non_terms) {
    for (int i = 0; i < non_terms.count; ++i) {
        if (strcmp(non_terms.symbols[i], symbol) == 0) {
            return true;
        }
    }

    return false;
}

void process_rule(const char *line, SymbolList *non_terms, SymbolList *terms,
                  ProductionRule *rules, int *rule_idx) {
    char *arrow = strstr(line, "->");
    if (!arrow)
        return;

    char lhs[MAX_LINE_LEN] = {0};
    strncpy(lhs, line, arrow - line);
    lhs[strcspn(lhs, " \t\n")] = '\0';
    int lhs_id = add_symbol(non_terms, lhs);

    char *rhs = arrow + 2;
    while (*rhs == ' ' || *rhs == '\t')
        rhs++;
    rhs[strcspn(rhs, "\n")] = '\0';

    ProductionRule rule = {lhs_id, -1, -1, 0};

    if (strlen(rhs) == 0) {
        rules[(*rule_idx)++] = rule;
        return;
    }

    char *symbols[2] = {NULL, NULL};
    int count = 0;
    char *token = strtok(rhs, " ");
    while (token && count < 2) {
        symbols[count++] = token;
        token = strtok(NULL, " ");
    }

    for (int i = 0; i < count; ++i) {
        SymbolList *arr =
            is_non_terminal(symbols[i], *non_terms) ? non_terms : terms;
        int id = add_symbol(arr, symbols[i]);
        if (i == 0)
            rule.rhs1 = id;
        else
            rule.rhs2 = id;
    }

    rules[(*rule_idx)++] = rule;
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
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    char *buffer = (char *)malloc(file_size + 1);
    fread(buffer, 1, file_size, file);
    buffer[file_size] = '\0';
    fclose(file);

    return buffer;
}

void parser(char *config_i, grammar_t *grammar, GrB_Matrix **adj_matrices) {
    char *config = strdup(config_i);
    char *graph_buf = read_entire_file(strtok(config, ","));
    char *grammar_buf = read_entire_file(strtok(NULL, ","));

    SymbolList non_terms, terms;
    init_symbol_list(&non_terms);
    init_symbol_list(&terms);

    ProductionRule *rules = NULL;
    int rule_count = 0;
    int capacity = 0;

    int graph_line_count = 0;

    char *line = grammar_buf;

    char **grammar_lines = NULL;
    int grammar_lines_count = 0;
    int grammar_lines_capacity = 0;

    bool first = true;
    while (*line) {
        char *end = strchr(line, '\n');
        if (end)
            *end = '\0';

        if (first) {
            add_symbol(&non_terms, line);
            first = false;
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        if (strlen(line) < 3) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        if (grammar_lines_count >= grammar_lines_capacity) {
            grammar_lines_capacity =
                grammar_lines_capacity == 0 ? 1 : grammar_lines_capacity * 2;
            grammar_lines =
                realloc(grammar_lines, sizeof(char *) * grammar_lines_capacity);
        }

        grammar_lines[grammar_lines_count++] = line;

        char *arrow = strstr(line, "->");
        if (!arrow) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        char lhs[MAX_LINE_LEN] = {0};
        strncpy(lhs, line, arrow - line);
        lhs[strcspn(lhs, " \t\n")] = '\0';
        int lhs_id = add_symbol(&non_terms, lhs);

        line = end ? end + 1 : line + strlen(line);
    }

    for (int i = 0; i < grammar_lines_count; i++) {
        char *line = grammar_lines[i];

        if (strlen(line) < 3)
            continue;

        if (strstr(line, "->") != NULL) {
            if (rule_count >= capacity) {
                capacity = capacity == 0 ? 1 : capacity * 2;
                rules = realloc(rules, sizeof(ProductionRule) * capacity);
            }
            process_rule(line, &non_terms, &terms, rules, &rule_count);
        }
    }
    free(grammar_lines);

    GraphEdge *edges = NULL;
    int edge_count = 0;
    int max_node = -1;
    line = graph_buf;

    while (*line) {
        char *end = strchr(line, '\n');
        if (end)
            *end = '\0';

        graph_line_count++;

        char *saveptr;
        char *u_str = strtok_r(line, " ", &saveptr);
        char *a_str = strtok_r(NULL, " ", &saveptr);
        char *v_str = strtok_r(NULL, " ", &saveptr);
        char *rest = strtok_r(NULL, " ", &saveptr);

        if (!u_str || !a_str || !v_str || rest != NULL) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        if (!is_number(u_str) || !is_number(v_str)) {
            fprintf(stderr, "error: %s, %s", u_str, v_str);
            exit(-1);
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        int u = atoi(u_str);
        int v = atoi(v_str);

        int term_index = -1;
        for (int j = 0; j < terms.count; ++j) {
            if (strcmp(terms.symbols[j], a_str) == 0) {
                term_index = j;
                break;
            }
        }

        if (u > max_node)
            max_node = u;
        if (v > max_node)
            max_node = v;

        if (term_index == -1) {
            line = end ? end + 1 : line + strlen(line);
            continue;
        }

        edges = realloc(edges, (edge_count + 1) * sizeof(GraphEdge));
        edges[edge_count++] = (GraphEdge){u, term_index, v};

        line = end ? end + 1 : line + strlen(line);
    }

    LAGraph_rule_WCNF *rules_WCNF =
        calloc(rule_count, sizeof(LAGraph_rule_WCNF));

    for (int i = 0; i < rule_count; ++i) {
        ProductionRule r = rules[i];
        rules_WCNF[i] = (LAGraph_rule_WCNF){r.lhs, r.rhs1, r.rhs2, r.index};
    }
    *grammar = (grammar_t){.nonterms_count = non_terms.count,
                           .terms_count = terms.count,
                           .rules_count = rule_count,
                           .rules = rules_WCNF};

    *adj_matrices = calloc(grammar->terms_count, sizeof(GrB_Matrix));
    for (size_t i = 0; i < terms.count; i++) {
        GrB_Matrix_new((*adj_matrices) + i, GrB_BOOL, max_node + 1,
                       max_node + 1);
    }

    GrB_Scalar true_scalar;
    GrB_Scalar_new(&true_scalar, GrB_BOOL);
    GrB_Scalar_setElement_BOOL(true_scalar, true);

    GrB_Index *row = malloc(sizeof(GrB_Index) * edge_count);
    GrB_Index *col = malloc(sizeof(GrB_Index) * edge_count);
    for (size_t i = 0; i < terms.count; i++) {
        int count = 0;

        for (int j = 0; j < edge_count; ++j) {
            if (i == edges[j].term_index) {
                row[count] = edges[j].u;
                col[count] = edges[j].v;
                count++;
            }
        }

        GxB_Matrix_build_Scalar((*adj_matrices)[i], row, col, true_scalar,
                                count);
#ifdef DEBUG_parser
        GxB_print((*adj_matrices)[i], 1);
#endif
    }

    free(row);
    free(col);
    GrB_free(&true_scalar);

#ifdef DEBUG_parser
    printf("Non-terminals:\n");
    for (int i = 0; i < non_terms.count; ++i)
        printf("%d: %s\n", i, non_terms.symbols[i]);

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
    printf("Number of nodes: %d\n", max_node + 1);
    printf("Edges: %d\n", edge_count);
    printf("Graph lines: %d\n", graph_line_count);
#endif

    free_symbol_list(&non_terms);
    free_symbol_list(&terms);
    free(rules);

    free(edges);
    free(graph_buf);
    free(grammar_buf);
    free(config);

    return;
}