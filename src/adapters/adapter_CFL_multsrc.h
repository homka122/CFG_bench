#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <stdbool.h>

typedef struct {
    bool use_start_nodes;
} CFL_multsrc_PrepareData;

AdapterMethods adapter_CFL_multsrc_get_methods(void);
