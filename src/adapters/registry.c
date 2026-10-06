#include "registry.h"
#include "adapter_CFL.h"
#include "adapter_CFL_CFPQ_RSM.h"
#include "adapter_CFL_adv.h"
#include "adapter_CFL_all_path.h"
#include "adapter_CFL_all_path_adv.h"
#include "adapter_CFL_multsrc.h"
#include "adapter_CFL_single_path.h"
#include <string.h>

// for adapters without PrepareData
static GrB_Info prepare_no_data(const AdapterMethods *adapter, const ParserResult *parser_result,
                                const AlgorithmOptions *options) {
    (void)options;
    return adapter->prepare(parser_result, NULL);
}

static GrB_Info prepare_CFL_adv(const AdapterMethods *adapter, const ParserResult *parser_result,
                                const AlgorithmOptions *options) {
    CFL_adv_PrepareData data = {.optimizations = options->optimizations};
    return adapter->prepare(parser_result, &data);
}

static GrB_Info prepare_CFL_all_path(const AdapterMethods *adapter, const ParserResult *parser_result,
                                     const AlgorithmOptions *options) {
    CFL_all_path_PrepareData data = {.use_cfpq = options->use_cfpq};
    return adapter->prepare(parser_result, &data);
}

static GrB_Info prepare_CFL_all_path_adv(const AdapterMethods *adapter, const ParserResult *parser_result,
                                         const AlgorithmOptions *options) {
    CFL_all_path_adv_PrepareData data = {.optimizations = options->optimizations};
    return adapter->prepare(parser_result, &data);
}

static GrB_Info prepare_CFL_CFPQ_RSM(const AdapterMethods *adapter, const ParserResult *parser_result,
                                     const AlgorithmOptions *options) {
    CFL_CFPQ_RSM_PrepareData data = {.use_start_nodes = options->use_start_nodes};
    return adapter->prepare(parser_result, &data);
}

static GrB_Info prepare_CFL_multsrc(const AdapterMethods *adapter, const ParserResult *parser_result,
                                    const AlgorithmOptions *options) {
    CFL_multsrc_PrepareData data = {.use_start_nodes = options->use_start_nodes};
    return adapter->prepare(parser_result, &data);
}

// the order is used in the help message
static const AlgorithmEntry algorithms[] = {
    {"CFL_adv", adapter_CFL_adv_get_methods, prepare_CFL_adv, false},
    {"CFL", adapter_CFL_get_methods, prepare_no_data, false},
    {"CFL_single_path", adapter_CFL_single_path_get_methods, prepare_no_data, false},
    {"CFL_all_path", adapter_CFL_all_paths_get_methods, prepare_CFL_all_path, false},
    {"CFL_all_path_adv", adapter_CFL_all_path_adv_get_methods, prepare_CFL_all_path_adv, false},
    {"CFL_CFPQ_RSM", adapter_CFL_CFPQ_RSM_get_methods, prepare_CFL_CFPQ_RSM, true},
    {"CFL_multsrc", adapter_CFL_multsrc_get_methods, prepare_CFL_multsrc, true},
};

#define ALGORITHMS_COUNT (sizeof(algorithms) / sizeof(algorithms[0]))

const AlgorithmEntry *registry_find(const char *name) {
    for (size_t i = 0; i < ALGORITHMS_COUNT; i++) {
        if (strcmp(algorithms[i].name, name) == 0) {
            return &algorithms[i];
        }
    }
    return NULL;
}

void registry_print_names(FILE *out) {
    for (size_t i = 0; i < ALGORITHMS_COUNT; i++) {
        fprintf(out, "%s%s", i == 0 ? "" : ", ", algorithms[i].name);
    }
}
