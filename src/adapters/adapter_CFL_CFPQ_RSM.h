#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <stdbool.h>

typedef struct {
    bool use_start_nodes;
} CFL_CFPQ_RSM_PrepareData;

AdapterMethods adapter_CFL_CFPQ_RSM_get_methods(void);
