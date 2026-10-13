#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int8_t optimizations;
    bool use_start_nodes;
} CFL_multsrc_adv_PrepareData;

AdapterMethods adapter_CFL_multsrc_adv_get_methods(void);
