#include "adapter_CFL.h"
#include "adapter_CFL_CFPQ_RSM.h"
#include "adapter_CFL_adv.h"
#include "adapter_CFL_all_path.h"
#include "adapter_CFL_multsrc.h"
#include "adapter_CFL_single_path.h"
#include "adapter_CFL_all_path_adv.h"
#include "memory.h"
#include "parser.h"
#include "result_manager.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <LAGraphX.h>
#include <getopt.h>
#include <malloc.h>
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
            printf("[%2ld] %s [%s] %s\n", i, sym.label, sym.is_nonterm ? "N" : "T", sym.is_indexed ? "[I]" : "");
        }
    } else {
        for (size_t i = 0; i < list.count; i++) {
            Symbol sym = list.symbols[i];
            printf("[%2ld] %s [%s] %s\n", map[i], sym.label, sym.is_nonterm ? "N" : "T", sym.is_indexed ? "[I]" : "");
        }
    }
}

#define RESET "\033[0m"
#define BLACK "\033[30m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define WHITE "\033[37m"

// Use your custom configuration for the benchmark (default is the xz.g graph
// and vf.cnf grammar)
#define configs_macro configs_java

#define OPT_EMPTY (1 << 0)
#define OPT_FORMAT (1 << 1)
#define OPT_LAZY (1 << 2)
#define OPT_BLOCK (1 << 3)

enum { HOT_OPTION = 1000 };

static void print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage: %s -c <config file> [options]\n"
            "\n"
            "Required:\n"
            "  -c <config file>  Path to benchmark config file\n"
            "\n"
            "Benchmark options:\n"
            "  -r <rounds>       Number of benchmark rounds (default: 10)\n"
            "  --hot             Enable HOT launch (warm-up run before measurements)\n"
            "  -a <algorithm>    Algorithm to use "
            "(default: CFL_adv; options: CFL_adv, CFL, CFL_single_path, CFL_all_path, CFL_all_path_adv)\n"
            "\n"
            "Optimization flags:\n"
            "  -e                Enable empty optimization\n"
            "  -f                Enable format optimization\n"
            "  -l                Enable lazy optimization\n"
            "  -b                Enable block optimization\n"
            "\n"
            "Other:\n"
            "  -t                Enable test mode\n"
            "  -h                Print this help message\n"
            "\n"
            "Example:\n"
            "  %s -c configs/configs_my.csv -r 10 --hot\n",
            program_name, program_name);
}

int main(int argc, char **argv) {
    GrB_Info retval = GrB_SUCCESS;
    int8_t optimizations = 0;
    int opt;
    bool is_test = false;
    bool is_hot_enabled = false;
    bool is_config = false;
    char *algo = NULL;
    bool is_algo_choosed = false;
    char *input_config = NULL;
    size_t rounds_count = 10;

    AdapterMethods adapter = {0};

    static struct option long_options[] = {{"hot", no_argument, 0, HOT_OPTION}, {0, 0, 0, 0}};

    while ((opt = getopt_long(argc, argv, "eflbthr:c:a:", long_options, NULL)) != -1) {
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
        case 'h':
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        case HOT_OPTION:
            is_hot_enabled = true;
            break;
        case 't':
            is_test = true;
            break;
        case 'r':
            rounds_count = strtoul(optarg, NULL, 10);
            if (rounds_count == 0) {
                fprintf(stderr, "Rounds count must be greater than 0\n");
                exit(EXIT_FAILURE);
            }
            printf("Choosen rounds count: %zu\n", rounds_count);
            break;
        case 'c':
            is_config = true;
            input_config = optarg;
            printf("Choosen config: %s\n", input_config);
            break;
        case 'a':
            is_algo_choosed = true;
            algo = optarg;
            printf("Choosen algorithm: %s\n", algo);

            if (strcmp(algo, "CFL_adv") == 0) {
                adapter = adapter_CFL_adv_get_methods();
            } else if (strcmp(algo, "CFL") == 0) {
                adapter = adapter_CFL_get_methods();
            } else if (strcmp(algo, "CFL_single_path") == 0) {
                adapter = adapter_CFL_single_path_get_methods();
            } else if (strcmp(algo, "CFL_all_path") == 0) {
                adapter = adapter_CFL_all_paths_get_methods();
            } else if (strcmp(algo, "CFL_CFPQ_RSM") == 0) {
                adapter = adapter_CFL_CFPQ_RSM_get_methods();
            } else if (strcmp(algo, "CFL_multsrc") == 0) {
                adapter = adapter_CFL_multsrc_get_methods();
            } else if (strcmp(algo, "CFL_all_path_adv") == 0) {
                adapter = adapter_CFL_all_path_adv_get_methods();
            } else {
                fprintf(stderr, "Unknown algorithm: %s\n", algo);
                exit(EXIT_FAILURE);
            }
            break;
        default:
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (!is_algo_choosed) {
        adapter = adapter_CFL_adv_get_methods();
        algo = "CFL_adv";
        printf("No algorithm choosed, using CFL_adv by default\n");
    }

    TRY(adapter.setup());

    if (!is_config) {
        fprintf(stderr, "Need to choose config by flag -c [config file]\n");
        exit(EXIT_FAILURE);
    }

    size_t configs_count = 0;
    config_row *configs = calloc(1000, sizeof(config_row));
    char *config_text;
    get_configs_from_file(input_config, &configs_count, configs, &config_text);

    printf("Start bench\n");
    fflush(stdout);

    for (size_t i = 0; i < configs_count; i++) {
        double *start = calloc(rounds_count, sizeof(double));
        double *end = calloc(rounds_count, sizeof(double));
        if (start == NULL || end == NULL) {
            fprintf(stderr, "Failed to allocate memory for benchmark rounds\n");
            free(start);
            free(end);
            exit(EXIT_FAILURE);
        }

        config_row config = configs[i];
        printf("CONFIG: grammar: %s, graph: %s\n", config.grammar, config.graph);
        fflush(stdout);

        ParserResult parser_result = parser(config);
        adapter.prepare(parser_result, &(CFL_adv_PrepareData){.optimizations = optimizations});

        bool is_hot = is_hot_enabled;

        size_t result = 0;
        ssize_t max_memory_kb = 0;
        for (size_t i = 0; i < rounds_count; i++) {
            TRY(adapter.init_outputs());

            // in some cases free don't change memory usage, so we need to reset it manually
            malloc_trim(0);
            if (mem_peak_reset() != 0) {
                fprintf(stderr, "Failed to reset memory peak\n");
                exit(EXIT_FAILURE);
            }

            start[i] = LAGraph_WallClockTime();
#ifndef CI
            retval = adapter.run();
#endif
            end[i] = LAGraph_WallClockTime();
            max_memory_kb = mem_get_peak_kb();

            if (is_test) {
                size_t result = adapter.get_result();
                ResultType result_type = adapter.is_result_valid(config.valid_result);
                char status[256];
                switch (result_type) {
                case RESULT_OK:
                    snprintf(status, sizeof(status), GREEN "[OK]" RESET);
                    break;
                case RESULT_ERROR:
                    snprintf(status, sizeof(status), RED "[Wrong] (Result must be %ld)" RESET, config.valid_result);
                    break;
                case RESULT_UNKNOWN:
                    snprintf(status, sizeof(status), YELLOW "[Unknown]" RESET);
                    break;
                default:
                    fprintf(stderr, "Unknown result type: %d\n", result_type);
                    abort();
                }

                printf("\tResult: %ld (Return code: %d) %s", result, retval, status);

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

            result = adapter.get_result();
            TRY(adapter.free_outputs());
            save_result(algo, config.grammar, config.graph, result, max_memory_kb,
                        (size_t)((end[i] - start[i]) * 1000));
            // in some cases free don't change memory usage, so we need to reset it manually
            malloc_trim(0);
        }
        printf("\n");

        if (is_test) {
            free(start);
            free(end);
            adapter.cleanup();

            fflush(stdout);
            continue;
        }

        double sum = 0;
        for (size_t i = 0; i < rounds_count; i++) {
            sum += end[i] - start[i];
        }
        printf("\tTime elapsed (avg): %.6f seconds. %zd KB max memory. Result: %ld (return code "
               "%d) (%s)\n\n",
               sum / rounds_count, max_memory_kb, result, retval, msg);

        free(start);
        free(end);

        TRY(adapter.cleanup());

        fflush(stdout);
    }

    free(configs);
    free(config_text);
    TRY(adapter.teardown());
    return 0;
}
