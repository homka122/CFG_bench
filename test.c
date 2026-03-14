#include <GraphBLAS.h>
#include <LAGraph.h>
#include <LAGraphX.h>
#include <getopt.h>
#include <parser.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define run_algorithm()                                                                                                \
    LAGraph_CFL_reachability_adv(outputs, adj_matrices, symbols_amount, grammar.rules, grammar.rules_count, msg,       \
                                 optimizations)

#define check_error(error)                                                                                             \
    {                                                                                                                  \
        retval = run_algorithm();                                                                                      \
        TEST_CHECK(retval == error);                                                                                   \
        TEST_MSG("retval = %d (%s)", retval, msg);                                                                     \
    }

#define check_result(result)                                                                                           \
    {                                                                                                                  \
        char *expected = output_to_str(0);                                                                             \
        TEST_CHECK(strcmp(result, expected) == 0);                                                                     \
        TEST_MSG("Wrong result. Actual: %s", expected);                                                                \
    }

#define TRY(GrB_method)                                                                                                \
    {                                                                                                                  \
        GrB_Info LG_GrB_Info = GrB_method;                                                                             \
        if (LG_GrB_Info < GrB_SUCCESS) {                                                                               \
            fprintf(stderr, "LAGraph failure (file %s, line %d): (%d) \n", __FILE__, __LINE__, LG_GrB_Info);           \
            return (LG_GrB_Info);                                                                                      \
        }                                                                                                              \
    }

GrB_Matrix *adj_matrices = NULL;
GrB_Matrix *outputs = NULL;
grammar_t grammar = {0, 0, NULL};
char msg[LAGRAPH_MSG_LEN];
size_t symbols_amount = 0;

GrB_Info setup() { TRY(LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, msg)); }

GrB_Info teardown(void) { TRY(LAGraph_Finalize(msg)); }

GrB_Info init_outputs() { TRY(LAGraph_Calloc((void **)&outputs, symbols_amount, sizeof(GrB_Matrix), msg)); }

GrB_Info free_outputs(size_t symbols_amount) {
    for (size_t i = 0; i < (symbols_amount); i++) {
        if (outputs == NULL)
            break;

        if (outputs[i] == NULL)
            continue;

        TRY(GrB_free(&outputs[i]));
    }
    TRY(LAGraph_Free((void **)&outputs, msg));
    outputs = NULL;
}

char *symbol_numerate(Symbol *sym, size_t num) {
    char *original_str = sym->label;
    size_t len = strlen(original_str);

    if (len >= 2 && strcmp(original_str + len - 2, "_i") == 0) {
        size_t num_len = snprintf(NULL, 0, "%zu", num);
        size_t new_len = len - 1 + num_len;

        char *new_str = malloc(new_len + 1);
        strncpy(new_str, original_str, len - 1);
        snprintf(new_str + len - 1, num_len + 1, "%zu", num);
        new_str[new_len] = '\0';

        return new_str;
    } else {
        return sym->label;
    }
}

int rule_new_index(size_t *map, Symbol *sym, int index, size_t offset) {
    if (index == -1) {
        return -1;
    }

    if (sym->is_indexed) {
        return map[index] + offset;
    } else {
        return map[index];
    }
}

bool is_rule_has_indexed_symb(Rule rule, SymbolList list) {
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

typedef struct {
    size_t *rows;
    size_t *cols;
    size_t *indeces;
    size_t size;
    size_t capacity;
} SymbolData;

void symbol_data_init(SymbolData *data) {
    data->rows = NULL;
    data->cols = NULL;
    data->indeces = NULL;
    data->size = 0;
    data->capacity = 0;
}

void symbol_data_expand(SymbolData *data) {
    size_t new_capacity = data->capacity == 0 ? 10 : data->capacity * 2;
    data->rows = realloc(data->rows, new_capacity * sizeof(size_t));
    data->cols = realloc(data->cols, new_capacity * sizeof(size_t));
    data->indeces = realloc(data->indeces, new_capacity * sizeof(size_t));
    data->capacity = new_capacity;
}

void symbol_data_free(SymbolData *data) {
    free(data->rows);
    free(data->cols);
    free(data->indeces);
}

GrB_Matrix *get_matrices_from_graph(Graph graph, size_t *map_base_indecies, size_t symbols_amount) {
    SymbolData *symbol_datas = calloc(symbols_amount, sizeof(SymbolData));
    for (size_t i = 0; i < symbols_amount; i++) {
        symbol_data_init(&symbol_datas[i]);
    }

    for (size_t i = 0; i < graph.edge_count; i++) {
        GraphEdge edge = graph.edges[i];
        SymbolData *data = &symbol_datas[map_base_indecies[edge.term_index] + edge.index];

        if (data->size == data->capacity) {
            symbol_data_expand(data);
        }

        data->rows[data->size] = edge.u;
        data->cols[data->size] = edge.v;
        data->indeces[data->size] = edge.index;
        data->size++;
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

char *configs_vf[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf", "data/graphs/vf/nab.g,data/grammars/vf.cnf",
                      "data/graphs/vf/leela.g,data/grammars/vf.cnf", NULL};

char *configs_my[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf", NULL};

void print_rules(Grammar grammar, SymbolList list) {
    for (size_t i = 0; i < grammar.rules_count; i++) {
        Rule rule = grammar.rules[i];
        if (rule.first != -1) {
            printf("%s ->", list.symbols[rule.first].label);
        }
        if (rule.second != -1) {
            printf(" %s", list.symbols[rule.second].label);
        }
        if (rule.third != -1) {
            printf(" %s", list.symbols[rule.third].label);
        }
        printf("\n");
    }
}

void print_list(SymbolList list, size_t *map) {
    if (map == NULL) {
        for (size_t i = 0; i < list.count; i++) {
            Symbol sym = list.symbols[i];
            printf("[%2d] %s [%s] %s\n", i, sym.label, sym.is_nonterm ? "N" : "T", sym.is_indexed ? "[I]" : "");
        }
    } else {
        for (size_t i = 0; i < list.count; i++) {
            Symbol sym = list.symbols[i];
            printf("[%2d] %s [%s] %s\n", map[i], sym.label, sym.is_nonterm ? "N" : "T", sym.is_indexed ? "[I]" : "");
        }
    }
}

// Took path to graph, grammar and prepare for launching algorithm
void preparing_for_config(config_row config) {
    ParserResult parser_result = parser(config);
    Grammar _grammar = parser_result.grammar;
    Graph graph = parser_result.graph;
    SymbolList list = parser_result.symbols;

    // print_rules(_grammar, list);
    // print_list(list, NULL);

    // printf("BLOCK COUNT: %ld\n", graph.block_count);

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
    symbols_amount = list.count + offset;
    // print_list(list, map);

    GrB_Matrix *matrices = get_matrices_from_graph(graph, map, symbols_amount);

    LAGraph_rule_EWCNF *rules_EWCNF = calloc(_grammar.rules_count, sizeof(LAGraph_rule_EWCNF));

    for (size_t i = 0; i < _grammar.rules_count; i++) {
        Rule rule = _grammar.rules[i];
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

    // for (size_t i = 0; i < _grammar.rules_count; i++) {
    //     LAGraph_rule_EWCNF rule = rules_EWCNF[i];
    //     printf("%d %d %d %d %d\n", rule.nonterm, rule.prod_A, rule.prod_B, rule.indexed, rule.indexed_count);
    // }

    int32_t nonterms_count = 0;
    for (size_t i = 0; i < list.count; i++) {
        if (list.symbols[i].is_nonterm)
            nonterms_count++;
    }

    grammar = (grammar_t){.nonterms_count = nonterms_count, .rules_count = _grammar.rules_count, .rules = rules_EWCNF};

    adj_matrices = matrices;

    free(graph.edges);
    free(_grammar.rules);
    for (size_t i = 0; i < list.count; i++) {
        free(list.symbols[i].label);
    }
    free(list.symbols);
}

#define RESET "\033[0m"
#define BLACK "\033[30m" /* Black */
#define RED "\033[31m"   /* Red */
#define GREEN "\033[32m" /* Green */

// Number of benchmark runs on a single graph
#define COUNT 10
// If true, the first run is done without measuring time (warm-up)
#define HOT false
// Use your custom configuration for the benchmark (default is the xz.g graph
// and vf.cnf grammar)
#define configs_macro configs_java

#define OPT_EMPTY (1 << 0)
#define OPT_FORMAT (1 << 1)
#define OPT_LAZY (1 << 2)
#define OPT_BLOCK (1 << 3)

int main(int argc, char **argv) {
    int8_t optimizations = 0;
    int opt;
    bool is_test = false;
    bool is_config = false;
    char *input_config = NULL;

    while ((opt = getopt(argc, argv, "eflbtc:")) != -1) {
        switch (opt) {
        case 'e':
            optimizations |= OPT_EMPTY;
            break;
        case 'f':
            optimizations |= OPT_FORMAT;
            break;
        case 'l':
            optimizations |= OPT_LAZY;
            break;
        case 'b':
            optimizations |= OPT_BLOCK;
            break;
        case 't':
            is_test = true;
            break;
        case 'c':
            is_config = true;
            input_config = optarg;
            printf("Choosen config: %s\n", input_config);
            break;
        default:
            fprintf(stderr, "Usage: %s [-e] [-f] [-l]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    TRY(setup());
    GrB_Info retval;

    if (!is_config) {
        fprintf(stderr, "Need to choose config by flag -c [config file]\n");
        exit(EXIT_FAILURE);
    }

    size_t configs_count = 0;
    config_row *configs = calloc(1000, sizeof(config_row));
    get_configs_from_file(input_config, &configs_count, configs);

    printf("Start bench\n");
    fflush(stdout);

    for (size_t i = 0; i < configs_count; i++) {
        config_row config = configs[i];
        printf("CONFIG: grammar: %s, graph: %s\n", config.grammar, config.graph);
        fflush(stdout);

        preparing_for_config(config);

        double start[COUNT];
        double end[COUNT];

        GrB_Index nnz = 0;
        bool is_hot = HOT;

        for (size_t i = 0; i < COUNT; i++) {
            TRY(init_outputs());

            // printf("\n");
            // for (size_t j = 0; j < symbols_amount; j++) {
            //     GrB_Matrix_nvals(&nnz, adj_matrices[j]);
            //     printf("%ld ", nnz);
            // }
            // printf("\n");

            start[i] = LAGraph_WallClockTime();
#ifndef CI
            retval = run_algorithm();
#endif
            end[i] = LAGraph_WallClockTime();
            GrB_Matrix_nvals(&nnz, outputs[0]);

            if (is_test) {
                if (nnz == config.valid_result) {
                    printf("\tResult: %ld (Return code: %d)" GREEN " [OK]" RESET, nnz, retval);
                } else {
                    printf("\tResult: %ld (Return code: %d)" RED " [Wrong]" RESET " (Result must be %ld)", nnz, retval,
                           config.valid_result);
                }
                if (retval != 0) {
                    printf("\t(MSG: %s)", msg);
                }
                printf(" (%.4f sec)", end[i] - start[i]);

                // printf("\n");
                // for (size_t j = 0; j < symbols_amount; j++) {
                //     GrB_Matrix_nvals(&nnz, outputs[j]);
                //     printf("%ld ", nnz);
                // }
                // printf("\n");

                TRY(free_outputs(symbols_amount));
                break;
            }

            if (is_hot) {
                is_hot = false;
                i--;
                TRY(free_outputs(symbols_amount));
                continue;
            }

            printf("\t%.3fs", end[i] - start[i]);
            fflush(stdout);

            TRY(free_outputs(symbols_amount));
        }
        printf("\n");

        if (is_test) {
            free(grammar.rules);
            for (size_t i = 0; i < symbols_amount; i++) {
                GrB_free(&adj_matrices[i]);
            }
            free(adj_matrices);

            fflush(stdout);
            continue;
        }

        double sum = 0;
        for (size_t i = 0; i < COUNT; i++) {
            sum += end[i] - start[i];
        }

        printf("\tTime elapsed (avg): %.6f seconds. Result: %ld (return code "
               "%d) (%s)\n\n",
               sum / COUNT, nnz, retval, msg);

        free(grammar.rules);
        for (size_t i = 0; i < symbols_amount; i++) {
            GrB_free(&adj_matrices[i]);
        }
        free(adj_matrices);

        fflush(stdout);
    }

    free(configs);
    TRY(teardown());
    return 0;
}
