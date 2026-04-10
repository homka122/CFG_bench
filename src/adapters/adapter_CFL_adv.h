#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>

typedef struct {
    int8_t optimizations;
} CFL_adv_PrepareData;

AdapterMethods adapter_CFL_adv_get_methods(void);
