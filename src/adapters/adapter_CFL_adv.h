#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>

typedef struct {
    int8_t optimizations;
} CFL_adv_PrepareData;

AdapterMethods adapter_CFL_adv_get_methods(void);

/**
 * Extract reachable vertex pairs from the start-symbol result matrix.
 *
 * On success, sources[i] and destinations[i] form one reachable pair. The
 * caller owns both returned arrays and must free them. Empty results are
 * returned as two NULL arrays with pair_count set to zero.
 */
GrB_Info adapter_CFL_adv_get_reachable_pairs(GrB_Index **sources, GrB_Index **destinations, GrB_Index *pair_count);

/**
 * Count vertices reachable from any of the given sources in the start-symbol
 * result matrix, the same result the multiple-source algorithms return.
 *
 * If sources is NULL, all vertices are used as sources.
 */
GrB_Info adapter_CFL_adv_count_reachable(const GrB_Index *sources, size_t sources_count, size_t *result);
