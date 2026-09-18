#include "adapter_CFL_adv.h"
#include "parser.h"
#include <GraphBLAS.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define OPT_EMPTY (1 << 0)
#define OPT_FORMAT (1 << 1)
#define OPT_LAZY (1 << 2)
#define OPT_BLOCK (1 << 3)
#define ALL_OPTIMIZATIONS (OPT_EMPTY | OPT_FORMAT | OPT_LAZY | OPT_BLOCK)

enum { START_ONLY_OPTION = 1000 };

static void print_usage(const char *program_name) {
    fprintf(stderr,
            "Usage: %s -c <config file> -o <output directory> [--start-only]\n"
            "\n"
            "  --start-only  Write unique start vertices instead of reachable pairs\n",
            program_name);
}

static int check_grb(GrB_Info info, const char *operation) {
    if (info >= GrB_SUCCESS) {
        return 0;
    }

    fprintf(stderr, "%s failed with GraphBLAS error %d\n", operation, info);
    return 1;
}

static const char *path_basename(const char *path) {
    const char *slash = strrchr(path, '/');
    return slash == NULL ? path : slash + 1;
}

static size_t stem_length(const char *basename) {
    const char *dot = strrchr(basename, '.');
    return dot == NULL || dot == basename ? strlen(basename) : (size_t)(dot - basename);
}

static char *make_output_path(const char *directory, const config_row *config, bool start_only) {
    const char *grammar = path_basename(config->grammar);
    const char *graph = path_basename(config->graph);
    const char *suffix = start_only ? "_start.result" : ".result";
    size_t grammar_length = stem_length(grammar);
    size_t graph_length = stem_length(graph);
    size_t directory_length = strlen(directory);
    const char *separator = directory_length > 0 && directory[directory_length - 1] == '/' ? "" : "/";
    int path_length = snprintf(NULL, 0, "%s%s%.*s_%.*s%s", directory, separator, (int)grammar_length, grammar,
                               (int)graph_length, graph, suffix);

    if (path_length < 0) {
        return NULL;
    }

    char *path = malloc((size_t)path_length + 1);
    if (path == NULL) {
        return NULL;
    }

    snprintf(path, (size_t)path_length + 1, "%s%s%.*s_%.*s%s", directory, separator, (int)grammar_length, grammar,
             (int)graph_length, graph, suffix);
    return path;
}

static int compare_indices(const void *left, const void *right) {
    GrB_Index a = *(const GrB_Index *)left;
    GrB_Index b = *(const GrB_Index *)right;
    return (a > b) - (a < b);
}

static int write_result(FILE *output, GrB_Index *sources, const GrB_Index *destinations, GrB_Index pair_count,
                        bool start_only, GrB_Index *written_count) {
    *written_count = 0;

    if (start_only && pair_count > 1) {
        qsort(sources, (size_t)pair_count, sizeof(*sources), compare_indices);
    }

    for (GrB_Index i = 0; i < pair_count; i++) {
        if (start_only && i > 0 && sources[i] == sources[i - 1]) {
            continue;
        }

        int result =
            start_only ? fprintf(output, "%" PRIu64 "\n", (uint64_t)sources[i])
                       : fprintf(output, "%" PRIu64 " %" PRIu64 "\n", (uint64_t)sources[i], (uint64_t)destinations[i]);
        if (result < 0) {
            return -1;
        }
        (*written_count)++;
    }

    return 0;
}

int main(int argc, char **argv) {
    char *config_path = NULL;
    char *output_directory = NULL;
    bool start_only = false;
    int opt;

    static struct option long_options[] = {
        {"start-only", no_argument, NULL, START_ONLY_OPTION},
        {0, 0, 0, 0},
    };

    while ((opt = getopt_long(argc, argv, "c:o:h", long_options, NULL)) != -1) {
        switch (opt) {
        case 'c':
            config_path = optarg;
            break;
        case 'o':
            output_directory = optarg;
            break;
        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        case START_ONLY_OPTION:
            start_only = true;
            break;
        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (config_path == NULL || output_directory == NULL || optind != argc) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    int status = EXIT_FAILURE;
    bool adapter_is_setup = false;
    bool adapter_is_prepared = false;
    bool outputs_are_initialized = false;
    AdapterMethods adapter = adapter_CFL_adv_get_methods();
    ParserResult parser_result = {0};
    bool parser_result_is_initialized = false;
    GrB_Index *sources = NULL;
    GrB_Index *destinations = NULL;
    GrB_Index pair_count = 0;
    config_row *configs = calloc(1000, sizeof(*configs));
    char *config_text = NULL;
    size_t configs_count = 0;
    FILE *output = NULL;
    char *output_path = NULL;
    struct stat output_directory_stat;

    if (stat(output_directory, &output_directory_stat) != 0) {
        perror("Failed to access output directory");
        goto cleanup;
    }
    if (!S_ISDIR(output_directory_stat.st_mode)) {
        fprintf(stderr, "Output path is not a directory: %s\n", output_directory);
        goto cleanup;
    }

    if (configs == NULL) {
        fprintf(stderr, "Failed to allocate config storage\n");
        goto cleanup;
    }

    get_configs_from_file(config_path, &configs_count, configs, &config_text);
    if (configs_count == 0) {
        fprintf(stderr, "Config does not contain any rows\n");
        goto cleanup;
    }

    if (check_grb(adapter.setup(), "CFL_adv setup")) {
        goto cleanup;
    }
    adapter_is_setup = true;

    for (size_t config_index = 0; config_index < configs_count; config_index++) {
        parser_result = parser(configs[config_index], false);
        parser_result_is_initialized = true;

        CFL_adv_PrepareData prepare_data = {.optimizations = ALL_OPTIMIZATIONS};
        if (check_grb(adapter.prepare(&parser_result, &prepare_data), "CFL_adv prepare")) {
            goto cleanup;
        }
        adapter_is_prepared = true;

        free_parser_result(&parser_result);
        parser_result_is_initialized = false;

        if (check_grb(adapter.init_outputs(), "CFL_adv output initialization")) {
            goto cleanup;
        }
        outputs_are_initialized = true;

        if (check_grb(adapter.run(), "CFL_adv run")) {
            goto cleanup;
        }

        if (check_grb(adapter_CFL_adv_get_reachable_pairs(&sources, &destinations, &pair_count),
                      "reachable-pair extraction")) {
            goto cleanup;
        }

        output_path = make_output_path(output_directory, &configs[config_index], start_only);
        if (output_path == NULL) {
            fprintf(stderr, "Failed to allocate output path\n");
            goto cleanup;
        }

        output = fopen(output_path, "w");
        if (output == NULL) {
            perror("Failed to open output file");
            goto cleanup;
        }

        GrB_Index written_count;
        if (write_result(output, sources, destinations, pair_count, start_only, &written_count) != 0) {
            perror("Failed to write result");
            goto cleanup;
        }

        if (fclose(output) != 0) {
            output = NULL;
            perror("Failed to close output file");
            goto cleanup;
        }
        output = NULL;

        printf("Wrote %" PRIu64 " %s to %s\n", (uint64_t)written_count,
               start_only ? "start vertices" : "reachable pairs", output_path);

        free(output_path);
        output_path = NULL;
        free(sources);
        sources = NULL;
        free(destinations);
        destinations = NULL;
        pair_count = 0;

        if (check_grb(adapter.free_outputs(), "CFL_adv output cleanup")) {
            goto cleanup;
        }
        outputs_are_initialized = false;
        if (check_grb(adapter.cleanup(), "CFL_adv cleanup")) {
            goto cleanup;
        }
        adapter_is_prepared = false;
    }
    status = EXIT_SUCCESS;

cleanup:
    if (output != NULL && fclose(output) != 0) {
        perror("Failed to close output file");
        status = EXIT_FAILURE;
    }
    free(output_path);
    free(sources);
    free(destinations);
    if (outputs_are_initialized && check_grb(adapter.free_outputs(), "CFL_adv output cleanup")) {
        status = EXIT_FAILURE;
    }
    if (adapter_is_prepared && check_grb(adapter.cleanup(), "CFL_adv cleanup")) {
        status = EXIT_FAILURE;
    }
    if (parser_result_is_initialized) {
        free_parser_result(&parser_result);
    }
    if (adapter_is_setup && check_grb(adapter.teardown(), "CFL_adv teardown")) {
        status = EXIT_FAILURE;
    }
    free(configs);
    free(config_text);
    return status;
}
