#include "adapter_CFL_adv.h"
#include "adapter_CFL.h"
#include "parser.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <LAGraphX.h>
#include <getopt.h>
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
    GrB_Info retval;
    int8_t optimizations = 0;
    int opt;
    bool is_test = false;
    bool is_config = false;
    bool is_algo_choosed = false;
    char *input_config = NULL;

    AdapterMethods adapter = {0};

    while ((opt = getopt(argc, argv, "eflbtc:a:")) != -1) {
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
        case 'a':
            is_algo_choosed = true;
            char *algo = optarg;
            printf("Choosen algorithm: %s\n", algo);

            if (strcmp(algo, "CFL_adv") == 0) {
                adapter = adapter_CFL_adv_get_methods();
            } else if (strcmp(algo, "CFL") == 0) {
                adapter = adapter_CFL_get_methods();
            } else {
                fprintf(stderr, "Unknown algorithm: %s\n", algo);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            fprintf(stderr, "Usage: %s [-e] [-f] [-l]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (!is_algo_choosed) {
        adapter = adapter_CFL_adv_get_methods();
        printf("No algorithm choosed, using CFL_adv by default\n");
    }

    TRY(adapter.setup());

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
        double start[COUNT];
        double end[COUNT];

        config_row config = configs[i];
        printf("CONFIG: grammar: %s, graph: %s\n", config.grammar, config.graph);
        fflush(stdout);

        ParserResult parser_result = parser(config);
        adapter.prepare(parser_result, &(CFL_adv_PrepareData){.optimizations = optimizations});

        bool is_hot = HOT;

        for (size_t i = 0; i < COUNT; i++) {
            TRY(adapter.init_outputs());

            start[i] = LAGraph_WallClockTime();
#ifndef CI
            retval = adapter.run();
#endif
            end[i] = LAGraph_WallClockTime();

            if (is_test) {
                size_t result = adapter.get_result();
                if (adapter.is_result_valid(config.valid_result)) {
                    printf("\tResult: %ld (Return code: %d)" GREEN " [OK]" RESET, result, retval);
                } else {
                    printf("\tResult: %ld (Return code: %d)" RED " [Wrong]" RESET " (Result must be %ld)", result,
                           retval, config.valid_result);
                }

                if (retval != 0) {
                    printf("\t(MSG: %s)", msg);
                }
                printf(" (%.4f sec)", end[i] - start[i]);

                TRY(adapter.free_outputs());
                break;
            }

            if (is_hot) {
                is_hot = false;
                i--;
                TRY(adapter.free_outputs());
                continue;
            }

            printf("\t%.3fs", end[i] - start[i]);
            fflush(stdout);

            TRY(adapter.free_outputs());
        }
        printf("\n");

        if (is_test) {
            adapter.cleanup();

            fflush(stdout);
            continue;
        }

        double sum = 0;
        for (size_t i = 0; i < COUNT; i++) {
            sum += end[i] - start[i];
        }
        size_t result = adapter.get_result();
        printf("\tTime elapsed (avg): %.6f seconds. Result: %ld (return code "
               "%d) (%s)\n\n",
               sum / COUNT, result, retval, msg);

        TRY(adapter.cleanup());

        fflush(stdout);
    }

    free(configs);
    TRY(adapter.teardown());
    return 0;
}
