#include <GraphBLAS.h>
#include <LAGraph.h>
#include <LAGraphX.h>
#include <parser.h>
#include <stdlib.h>
#include <unistd.h>

#define run_algorithm()                                                        \
    LAGraph_CFL_reachability_adv(outputs, adj_matrices, nonterms_count,        \
                                 grammar.rules, grammar.rules_count, msg,      \
                                 optimizations)

#define check_error(error)                                                     \
    {                                                                          \
        retval = run_algorithm();                                              \
        TEST_CHECK(retval == error);                                           \
        TEST_MSG("retval = %d (%s)", retval, msg);                             \
    }

#define check_result(result)                                                   \
    {                                                                          \
        char *expected = output_to_str(0);                                     \
        TEST_CHECK(strcmp(result, expected) == 0);                             \
        TEST_MSG("Wrong result. Actual: %s", expected);                        \
    }

GrB_Matrix *adj_matrices = NULL;
GrB_Matrix *outputs = NULL;
grammar_t grammar = {0, 0, NULL};
char msg[LAGRAPH_MSG_LEN];

void setup() { LAGr_Init(GrB_NONBLOCKING, malloc, NULL, NULL, free, msg); }

void teardown(void) { LAGraph_Finalize(msg); }

void init_outputs() {
    outputs = calloc(grammar.nonterms_count, sizeof(GrB_Matrix));
}

void free_outputs() {
    for (size_t i = 0; i < grammar.nonterms_count; i++) {
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

char *configs_java[] = {
    "data/graphs/java/eclipse.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/lusearch.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/luindex.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/commons_io.g,data/grammars/java_points_to.cnf",
    "data/graphs/java/sunflow.g,data/grammars/java_points_to.cnf",
    NULL};

char *configs_c_alias[] = {
    "data/graphs/c_alias/init.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/block.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/fs.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/ipc.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/lib.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/mm.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/net.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/security.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/sound.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/arch.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/crypto.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/drivers.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/kernel.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/postgre.g,data/grammars/c_alias.cnf",
    "data/graphs/c_alias/apache.g,data/grammars/c_alias.cnf",
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

char *configs_vf[] = {"data/graphs/vf/xz.g,data/grammars/vf.cnf",
                      "data/graphs/vf/nab.g,data/grammars/vf.cnf",
                      "data/graphs/vf/leela.g,data/grammars/vf.cnf", NULL};

char *configs_my[] = {
    "data/graphs/c_alias/init_new.g,data/grammars/c_alias_new.cnf", NULL};

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

int main(int argc, char **argv) {
    int8_t optimizations = 0;
    int opt;

    while ((opt = getopt(argc, argv, "efl")) != -1) {
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

        LAGraph_rule_WCNF *rules_WCNF =
            calloc(_grammar.rules_count, sizeof(LAGraph_rule_WCNF));

        for (size_t i = 0; i < _grammar.rules_count; i++) {
            Rule rule = _grammar.rules[i];
            rules_WCNF[i] = (LAGraph_rule_WCNF){
                rule.first, rule.second, rule.third,
                parser_result.symbols.symbols[rule.second].is_nonterm};
        }

        int32_t nonterms_count = 0;
        for (size_t i = 0; i < parser_result.symbols.count; i++) {
            if (parser_result.symbols.symbols[i].is_nonterm)
                nonterms_count++;
        }

        grammar = (grammar_t){.nonterms_count = nonterms_count,
                              .rules_count = _grammar.rules_count,
                              .rules = rules_WCNF};

        adj_matrices = parser_result.matrices;

        double start[COUNT];
        double end[COUNT];

        if (HOT) {
            run_algorithm();
        }

        GrB_Index nnz = 0;
        for (size_t i = 0; i < COUNT; i++) {
            init_outputs();

            start[i] = LAGraph_WallClockTime();
#ifndef CI
            retval = run_algorithm();
#endif
            end[i] = LAGraph_WallClockTime();

            GrB_Matrix_nvals(&nnz, outputs[0]);
            // GxB_print(outputs[0], 1);
            printf("\t%.3fs", end[i] - start[i]);
            fflush(stdout);
            free_outputs();
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
        config = configs[++config_index];
        fflush(stdout);
    }

    teardown();
}
