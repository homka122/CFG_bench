#include <GraphBLAS.h>
#include <LAGraph.h>
#include <LAGraphX.h>
#include <parser.h>
#include <stdlib.h>
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

GrB_Matrix *adj_matrices = NULL;
GrB_Matrix *outputs = NULL;
grammar_t grammar = {0, 0, NULL};
char msg[LAGRAPH_MSG_LEN];
size_t symbols_amount = 0;

void setup() { LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, msg); }

void teardown(void) { LAGraph_Finalize(msg); }

void init_outputs() { outputs = calloc(symbols_amount, sizeof(GrB_Matrix)); }

void free_outputs(size_t symbols_amount) {
    for (size_t i = 0; i < (symbols_amount); i++) {
        if (outputs == NULL)
            break;

        if (outputs[i] == NULL)
            continue;

        GrB_free(&outputs[i]);
    }
    free(outputs);
    outputs = NULL;
}

void free_workspace() {

    // for (size_t i = 0; i < grammar.terms_count; i++) {
    //     if (adj_matrices == NULL)
    //         break;

    //     if (adj_matrices[i] == NULL)
    //         continue;

    //     GrB_free(&adj_matrices[i]);
    // }
    // free(adj_matrices);
    // adj_matrices = NULL;

    // free_outputs();

    // free(grammar.rules);
    // grammar = (grammar_t){0, 0, 0, NULL};
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

GrB_Matrix *get_matrices_from_graph(Graph graph, SymbolList list, size_t *map_base_indecies, size_t symbols_amount) {
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
        // nrows *= list.symbols[i].is_indexed ? graph.block_count : 1;

        char msg[LAGRAPH_MSG_LEN];
        MY_GRB_TRY(GrB_Matrix_new(&matrices[i], GrB_BOOL, nrows, graph.node_count));

        if (data.size == 0) {
            continue;
        }

        GrB_Scalar true_scalar;
        MY_GRB_TRY(GrB_Scalar_new(&true_scalar, GrB_BOOL));
        MY_GRB_TRY(GrB_Scalar_setElement_BOOL(true_scalar, true));
        MY_GRB_TRY(GxB_Matrix_build_Scalar(matrices[i], data.rows, data.cols, true_scalar, data.size));
        GrB_free(&true_scalar);
    }

    for (size_t i = 0; i < symbols_amount; i++) {
        symbol_data_free(&symbol_datas[i]);
    }

    free(symbol_datas);

    return matrices;
}

char *configs_rdf[] = {"data/graphs/rdf/go_hierarchy.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/taxonomy.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/eclass.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/go.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       "data/graphs/rdf/taxonomy_hierarchy.g,data/grammars/"
                       "nested_parentheses_subClassOf_type.cnf",
                       NULL};

char *configs_java[] = {"data/graphs/java/lusearch.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/sunflow.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/gson.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/commons_io.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/luindex.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/eclipse.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/avrora.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/batik.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/commons_lang3.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/h2.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/mockito.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/fop.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/tomcat.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/xalan.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/pmd.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/junit5.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/jython.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/tradesoap.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/tradebeans.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/jackson.g,data/grammars/java_points_to.cnf",
                        "data/graphs/java/guava.g,data/grammars/java_points_to.cnf",
                        NULL};

char *configs_c_alias[] = {"data/graphs/c_alias/init.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/mm.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/ipc.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/lib.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/block.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/arch.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/crypto.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/security.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/sound.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/fs.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/net.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/drivers.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/kernel.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/apache.g,data/grammars/c_alias.cnf",
                           "data/graphs/c_alias/postgre.g,data/grammars/c_alias.cnf",
                           NULL};

char *configs_aa[] = {"data/graphs/aa/xz.g,data/grammars/aa.cnf",
                      "data/graphs/aa/nab.g,data/grammars/aa.cnf",
                      "data/graphs/aa/leela.g,data/grammars/aa.cnf",
                      "data/graphs/aa/x264.g,data/grammars/aa.cnf",
                      "data/graphs/aa/povray.g,data/grammars/aa.cnf",
                      "data/graphs/aa/cactus.g,data/grammars/aa.cnf",
                      "data/graphs/aa/parest.g,data/grammars/aa.cnf",
                      "data/graphs/aa/imagick.g,data/grammars/aa.cnf",
                      "data/graphs/aa/perlbench.g,data/grammars/aa.cnf",
                      "data/graphs/aa/omnetpp.g,data/grammars/aa.cnf",
                      NULL};

char *configs_vf[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf", "data/graphs/vf/nab.g,data/grammars/vf.cnf",
                      "data/graphs/vf/leela.g,data/grammars/vf.cnf", NULL};

char *configs_my[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf", NULL};

// Number of benchmark runs on a single graph
#define COUNT 1
// If true, the first run is done without measuring time (warm-up)
#define HOT false
// Use your custom configuration for the benchmark (default is the xz.g graph
// and vf.cnf grammar)
#define configs configs_my

#define OPT_EMPTY (1 << 0)
#define OPT_FORMAT (1 << 1)
#define OPT_LAZY (1 << 2)
#define OPT_BLOCK (1 << 3)

int main(int argc, char **argv) {
    int8_t optimizations = 0;
    int opt;
    bool is_test = false;

    while ((opt = getopt(argc, argv, "eflbt")) != -1) {
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
        default:
            fprintf(stderr, "Usage: %s [-e] [-f] [-l]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    setup();
    GrB_Info retval;

    // char *config = argv[1];
    printf("Start bench\n");
    fflush(stdout);
    int config_index = 0;
    char *config = configs[config_index];
    while (config) {
        printf("CONFIG: %s\n", config);
        fflush(stdout);
        ParserResult parser_result = parser(config);
        Grammar _grammar = parser_result.grammar;
        Graph graph = parser_result.graph;
        SymbolList list = parser_result.symbols;

        // indexed symbols must be each enumerate
        size_t *map = calloc(sizeof(size_t), list.count * graph.block_count);
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
        symbols_amount = map[list.count - 1] + (graph.block_count - 1) + 1;
        // printf("BLOCK_COUNT: %ld, SYMBOLS_AMOUNT: %ld\n", graph.block_count, symbols_amount);
        // symbol_list_print(list);
        // for (size_t i = 0; i < list.count; i++) {
        //     printf("%ld ", map[i]);
        // }
        // fflush(stdout);

        // expolode indecies
        // if (!(optimizations & OPT_BLOCK)) {
        //     explode_indices(&_grammar, &graph, &list);
        // }
        // symbol_list_print(list);
        // grammar_print(_grammar, list);

        GrB_Matrix *matrices = get_matrices_from_graph(graph, list, map, symbols_amount);

        // FILE *homka = fopen("./xzMatrixResult.mtx", "w");
        // if (homka == NULL) {
        //     exit(-1);
        // }

        LAGraph_rule_EWCNF *rules_EWCNF = calloc(_grammar.rules_count, sizeof(LAGraph_rule_EWCNF));

        for (size_t i = 0; i < _grammar.rules_count; i++) {
            Rule rule = _grammar.rules[i];
            rules_EWCNF[i] = (LAGraph_rule_EWCNF){.nonterm = rule.first == -1 ? -1 : map[rule.first],
                                                  .prod_A = rule.second == -1 ? -1 : map[rule.second],
                                                  .prod_B = rule.third == -1 ? -1 : map[rule.third],
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

            // printf("%d %d %d %d\n", rules_EWCNF[i].nonterm, rules_EWCNF[i].prod_A, rules_EWCNF[i].prod_B,
            //        rules_EWCNF[i].indexed);
        }

        int32_t nonterms_count = 0;
        for (size_t i = 0; i < list.count; i++) {
            if (list.symbols[i].is_nonterm)
                nonterms_count++;
        }

        grammar =
            (grammar_t){.nonterms_count = nonterms_count, .rules_count = _grammar.rules_count, .rules = rules_EWCNF};

        adj_matrices = matrices;

        double start[COUNT];
        double end[COUNT];

        GrB_Index nnz = 0;
        if (is_test) {
            init_outputs();
            retval = run_algorithm();
            GrB_Matrix_nvals(&nnz, outputs[0]);
            printf("\tResult: %ld (Return code: %d)", nnz, retval);
            if (retval != 0) {
                printf("\t(MSG: %s)", msg);
            }
            printf("\n");
            fflush(stdout);
            free_outputs(symbols_amount);
            config = configs[++config_index];
            continue;
        }

        if (HOT) {
            run_algorithm();
        }

        for (size_t i = 0; i < COUNT; i++) {
            init_outputs();

            start[i] = LAGraph_WallClockTime();
#ifndef CI
            retval = run_algorithm();
#endif
            end[i] = LAGraph_WallClockTime();

            GrB_Matrix_nvals(&nnz, outputs[0]);
            // LAGraph_MMWrite(outputs[0], homka, NULL, msg);

            // GxB_print(outputs[0], 1);
            printf("\t%.3fs", end[i] - start[i]);
            fflush(stdout);
            free_outputs(symbols_amount);
        }
        printf("\n");

        // printf("retval = %d (%s)\n", retval, msg);
        double sum = 0;
        for (size_t i = 0; i < COUNT; i++) {
            sum += end[i] - start[i];
        }

        printf("\tTime elapsed (avg): %.6f seconds. Result: %ld (return code "
               "%d) (%s)\n\n",
               sum / COUNT, nnz, retval, msg);
        // GxB_print(outputs[0], 1);
        free_workspace();

        free(graph.edges);
        free(grammar.rules);
        free(_grammar.rules);
        for (size_t i = 0; i < list.count; i++) {
            free(list.symbols[i].label);
        }

        free(list.symbols);
        for (size_t i = 0; i < list.count; i++) {
            GrB_free(&adj_matrices[i]);
        }
        free(adj_matrices);

        config = configs[++config_index];
        fflush(stdout);
    }

    teardown();
    return 0;
}
