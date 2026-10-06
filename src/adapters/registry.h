#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define DEFAULT_ALGORITHM "CFL_adv"

// CLI options an algorithm may need in prepare
typedef struct {
    int8_t optimizations;
    bool use_start_nodes;
    bool use_cfpq;
} AlgorithmOptions;

// builds the adapter's PrepareData from the options and calls adapter->prepare
typedef GrB_Info (*AlgorithmPrepare)(const AdapterMethods *adapter, const ParserResult *parser_result,
                                     const AlgorithmOptions *options);

typedef struct {
    const char *name;
    AdapterMethods (*get_methods)(void);
    AlgorithmPrepare prepare;
    // returns the vertices reachable from any start vertex instead of reachable pairs,
    // so the expected result from the config doesn't fit it
    bool is_multiple_source;
} AlgorithmEntry;

// returns NULL if there is no algorithm with this name
const AlgorithmEntry *registry_find(const char *name);

// prints the algorithm names separated by ", "
void registry_print_names(FILE *out);
