#pragma once

#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>

typedef struct {
    bool use_cfpq;
} CFL_all_path_PrepareData;

AdapterMethods adapter_CFL_all_paths_get_methods(void);
