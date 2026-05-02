#include "GraphBLAS.h"

#define TRY(GrB_method)                                                                                                \
    {                                                                                                                  \
        GrB_Info LG_GrB_Info = GrB_method;                                                                             \
        if (LG_GrB_Info < GrB_SUCCESS) {                                                                               \
            fprintf(stderr, "LAGraph failure (file %s, line %d): (%d) \n", __FILE__, __LINE__, LG_GrB_Info);           \
            return (LG_GrB_Info);                                                                                      \
        }                                                                                                              \
    }

GrB_Info adapter_CFL_init_src_nodes_common(GrB_Index **srcs, size_t *source_count, size_t graph_size) {
    if (graph_size == 0) {
        *source_count = 5;
        *srcs = calloc(*source_count, sizeof(GrB_Index));
    } else {
        *source_count = graph_size;
        *srcs = calloc(*source_count, sizeof(GrB_Index));
    }

    if (srcs == NULL) {
        fprintf(stderr, "out of memory\n");
        abort();
    }

    for (size_t i = 0; i < *source_count; i++) {
        (*srcs)[i] = i;
    }

    return GrB_SUCCESS;
}
