#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>

typedef struct {
    int8_t optimizations;
} CFL_all_path_adv_PrepareData;

AdapterMethods adapter_CFL_all_path_adv_get_methods(void);
