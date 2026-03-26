#pragma once

#include "parser.h"
#include <GraphBLAS.h>
#include <LAGraph.h>

typedef struct {
    GrB_Info (*setup)(void);
    GrB_Info (*teardown)(void);
    GrB_Info (*init_outputs)(void);
    GrB_Info (*free_outputs)(void);
    GrB_Info (*run)(void);
    GrB_Info (*prepare)(ParserResult, void *);
    GrB_Info (*cleanup)(void);
    bool (*is_result_valid)(size_t);
    size_t (*get_result)(void);
} AdapterMethods;