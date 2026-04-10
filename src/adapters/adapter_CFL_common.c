#include "adapter_CFL_common.h"

#define TRY(GrB_method)                                                                                                \
    {                                                                                                                  \
        GrB_Info LG_GrB_Info = GrB_method;                                                                             \
        if (LG_GrB_Info < GrB_SUCCESS) {                                                                               \
            fprintf(stderr, "LAGraph failure (file %s, line %d): (%d) \n", __FILE__, __LINE__, LG_GrB_Info);           \
            return (LG_GrB_Info);                                                                                      \
        }                                                                                                              \
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

static bool is_rule_has_indexed_symb(Rule rule, SymbolList nonterms, SymbolList terms) {
    bool first = rule.first != -1 && (nonterms.symbols[rule.first].is_indexed);

    bool second = false;
    bool third = false;
    if (rule.second != -1 && rule.third != -1) {
        // N -> A B
        second = nonterms.symbols[rule.second].is_indexed;
        third = nonterms.symbols[rule.third].is_indexed;
    } else if (rule.second != -1) {
        // N -> t
        second = terms.symbols[rule.second].is_indexed;
    } else {
        // N -> eps
    }

    return first || second || third;
}

// before: homka_i
// after: homka_1
//        homka_2
//        homka_3
static void explode_indices_CFL(Grammar *grammar, Graph *graph, SymbolList *nonterms, SymbolList *terms) {
    size_t *map_nonterms = NULL;
    size_t *map_terms = NULL;
    size_t map_nonterms_size = 0;
    size_t map_terms_size = 0;
    SymbolList new_list_nonterms = symbol_list_create();
    SymbolList new_list_terms = symbol_list_create();
    SymbolList old_list_nonterms = *nonterms;
    SymbolList old_list_terms = *terms;

    for (size_t list_i = 0; list_i < 2; list_i++) {
        SymbolList *list = list_i == 0 ? &old_list_terms : &old_list_nonterms;
        SymbolList *new_list = list_i == 0 ? &new_list_terms : &new_list_nonterms;
        size_t **map = list_i == 0 ? &map_terms : &map_nonterms;
        size_t *map_size = list_i == 0 ? &map_terms_size : &map_nonterms_size;
        for (size_t i = 0; i < list->count; i++) {
            Symbol sym = list->symbols[i];

            if (*map_size == i) {
                *map_size = *map_size * 2 + 1;
                *map = realloc(*map, *map_size * sizeof(size_t));
            }

            // just add to new list and map to it
            if (!sym.is_indexed) {
                (*map)[i] = symbol_list_add_str(new_list, sym.label, sym.is_nonterm);
                continue;
            }

            (*map)[i] = symbol_list_add_str(new_list, symbol_numerate(&sym, 0), sym.is_nonterm);
            for (size_t j = 1; j < graph->block_count; j++) {
                char *new_label = symbol_numerate(&sym, j);
                symbol_list_add_str(new_list, new_label, sym.is_nonterm);
            }
        }
    }
    *nonterms = new_list_nonterms;
    *terms = new_list_terms;

    Rule *new_rules = NULL;
    size_t rules_capacity = 0;
    size_t rules_count = 0;
    for (size_t i = 0; i < grammar->rules_count; i++) {
        if (rules_count + graph->block_count + 2 >= rules_capacity) {
            rules_capacity = rules_capacity * 2 + graph->block_count + 2;
            new_rules = realloc(new_rules, rules_capacity * sizeof(Rule));
        }

        Rule rule = grammar->rules[i];
        Symbol *first = rule.first == -1 ? NULL : &old_list_nonterms.symbols[rule.first];

        if (rule.second != -1 && rule.third != -1) {
            // N -> A B
            Symbol *second = &old_list_nonterms.symbols[rule.second];
            Symbol *third = &old_list_nonterms.symbols[rule.third];
            if (!is_rule_has_indexed_symb(rule, old_list_nonterms, old_list_terms)) {
                new_rules[rules_count] = (Rule){rule_new_index(map_nonterms, first, rule.first, 0),
                                                rule_new_index(map_nonterms, second, rule.second, 0),
                                                rule_new_index(map_nonterms, third, rule.third, 0)};
                rules_count++;
                continue;
            }

            for (size_t j = 0; j < graph->block_count; j++) {
                new_rules[rules_count] = (Rule){rule_new_index(map_nonterms, first, rule.first, j),
                                                rule_new_index(map_nonterms, second, rule.second, j),
                                                rule_new_index(map_nonterms, third, rule.third, j)};
                rules_count++;
            }
        } else if (rule.second != -1) {
            // N -> t
            Symbol *second = &old_list_terms.symbols[rule.second];
            if (!is_rule_has_indexed_symb(rule, old_list_nonterms, old_list_terms)) {
                new_rules[rules_count] = (Rule){rule_new_index(map_nonterms, first, rule.first, 0),
                                                rule_new_index(map_terms, second, rule.second, 0), -1};
                rules_count++;
                continue;
            }

            for (size_t j = 0; j < graph->block_count; j++) {
                new_rules[rules_count] = (Rule){rule_new_index(map_nonterms, first, rule.first, j),
                                                rule_new_index(map_terms, second, rule.second, j), -1};
                rules_count++;
            }
        } else {
            // N -> eps
            if (!is_rule_has_indexed_symb(rule, old_list_nonterms, old_list_terms)) {
                new_rules[rules_count] = (Rule){rule_new_index(map_nonterms, first, rule.first, 0), -1, -1};
                rules_count++;
                continue;
            }

            for (size_t j = 0; j < graph->block_count; j++) {
                new_rules[rules_count] = (Rule){rule_new_index(map_nonterms, first, rule.first, j), -1, -1};
                rules_count++;
            }
        }
    }
    grammar->rules = new_rules;
    grammar->rules_count = rules_count;

    for (size_t i = 0; i < graph->edge_count; i++) {
        GraphEdge *edge = &graph->edges[i];

        edge->term_index = map_terms[edge->term_index] + edge->index;
        edge->index = 0;
    }
    graph->block_count = 1;

    free(map_terms);
    free(map_nonterms);
}

GrB_Info adapter_CFL_prepare_common(ParserResult parser_result, GrB_Matrix **adj_matrices, size_t *terms_count,
                                    size_t *nonterms_count, LAGraph_rule_WCNF **rules, size_t *rules_count, size_t *graph_size) {
    Grammar grammar = parser_result.grammar;
    Graph graph = parser_result.graph;
    SymbolList list = parser_result.symbols;

    grammar_to_WCNF(&grammar, &list);
    SymbolList terms = symbol_list_create();
    SymbolList nonterms = symbol_list_create();

    grammar_split_terms_nonterms(&grammar, &list, &terms, &nonterms);

    // change term indecies in graph
    for (size_t i = 0; i < graph.edge_count; i++) {
        graph.edges[i].term_index = symbol_list_get_index_str(&terms, list.symbols[graph.edges[i].term_index].label);
    }

    explode_indices_CFL(&grammar, &graph, &nonterms, &terms);

    LAGraph_rule_WCNF *rules_WCNF = calloc(grammar.rules_count, sizeof(LAGraph_rule_WCNF));
    for (int i = 0; i < grammar.rules_count; ++i) {
        Rule r = grammar.rules[i];
        rules_WCNF[i] = (LAGraph_rule_WCNF){r.first, r.second, r.third, 0};
    }

    GrB_Matrix *prepared_adj_matrices = calloc(terms.count, sizeof(GrB_Matrix));
    for (size_t i = 0; i < terms.count; i++) {
        GrB_Matrix_new(prepared_adj_matrices + i, GrB_BOOL, graph.node_count, graph.node_count);
    }

    GrB_Scalar true_scalar;
    GrB_Scalar_new(&true_scalar, GrB_BOOL);
    GrB_Scalar_setElement_BOOL(true_scalar, true);

    GrB_Index *row = malloc(sizeof(GrB_Index) * graph.edge_count);
    GrB_Index *col = malloc(sizeof(GrB_Index) * graph.edge_count);
    for (size_t i = 0; i < terms.count; i++) {
        int count = 0;

        for (int j = 0; j < graph.edge_count; j++) {
            if (i == graph.edges[j].term_index) {
                row[count] = graph.edges[j].u;
                col[count] = graph.edges[j].v;
                count++;
            }
        }

        GxB_Matrix_build_Scalar(prepared_adj_matrices[i], row, col, true_scalar, count);
#ifdef DEBUG_parser
        GxB_print(prepared_adj_matrices[i], 1);
#endif
    }

    free(row);
    free(col);
    GrB_free(&true_scalar);

    *adj_matrices = prepared_adj_matrices;
    *terms_count = terms.count;
    *nonterms_count = nonterms.count;
    *rules = rules_WCNF;
    *rules_count = grammar.rules_count;
    *graph_size = graph.node_count;

    free(graph.edges);
    free(grammar.rules);
    for (size_t i = 0; i < list.count; i++) {
        free(list.symbols[i].label);
    }
    free(list.symbols);

    return GrB_SUCCESS;
}

GrB_Info adapter_CFL_init_outputs_common(GrB_Matrix **outputs, size_t nonterms_count, size_t graph_size, char *msg) {
    TRY(LAGraph_Calloc((void **)outputs, nonterms_count, sizeof(GrB_Matrix), msg));

    for (size_t i = 0; i < nonterms_count; i++) {
        TRY(GrB_Matrix_new(&(*outputs)[i], GrB_BOOL, graph_size, graph_size));
    }

    return GrB_SUCCESS;
}

GrB_Info adapter_CFL_get_result_common(GrB_Matrix output, size_t *result) {
    GrB_Index nnz = 0;
    TRY(GrB_Matrix_nvals(&nnz, output));

    *result = nnz;
    return GrB_SUCCESS;
}

GrB_Info adapter_CFL_is_result_valid_common(GrB_Matrix output, size_t valid_result, bool *is_valid) {
    size_t result = 0;
    TRY(adapter_CFL_get_result_common(output, &result));

    *is_valid = result == valid_result;
    return GrB_SUCCESS;
}

GrB_Info adapter_CFL_free_outputs_common(GrB_Matrix **outputs, size_t nonterms_count, char *msg) {
    if (*outputs == NULL) {
        return GrB_SUCCESS;
    }

    for (size_t i = 0; i < nonterms_count; i++) {
        if ((*outputs)[i] == NULL)
            continue;

        GrB_Info info = GrB_free(&(*outputs)[i]);
        if (info != GrB_SUCCESS) {
            return info;
        }
    }

    return LAGraph_Free((void **)outputs, msg);
}

GrB_Info adapter_CFL_cleanup_common(GrB_Matrix **adj_matrices, size_t terms_count, void **rules) {
    for (size_t i = 0; i < terms_count; i++) {
        if ((*adj_matrices)[i] == NULL)
            continue;

        GrB_Info info = GrB_free(&(*adj_matrices)[i]);
        if (info != GrB_SUCCESS) {
            return info;
        }
    }

    free(*adj_matrices);
    *adj_matrices = NULL;
    free(*rules);
    *rules = NULL;

    return GrB_SUCCESS;
}
