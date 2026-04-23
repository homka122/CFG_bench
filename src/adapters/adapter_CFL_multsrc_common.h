#pragma once

#include "../parser.h"
#include "adapter.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <stdbool.h>

// single method for sets source nodes
GrB_Info adapter_CFL_init_src_nodes_common(GrB_Index **srcs, size_t *source_count);
