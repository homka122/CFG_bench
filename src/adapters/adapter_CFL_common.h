#pragma once

#include "../parser.h"
#include <GraphBLAS.h>
#include <LAGraph.h>
#include <stdbool.h>

GrB_Info adapter_CFL_prepare_common(ParserResult parser_result, GrB_Matrix **adj_matrices, size_t *terms_count,
                                    size_t *nonterms_count, LAGraph_rule_WCNF **rules, size_t *rules_count,
                                    size_t *graph_size);

void explode_indices_CFL(Grammar *grammar, Graph *graph, SymbolList *nonterms, SymbolList *terms);

GrB_Info adapter_CFL_init_outputs_common(GrB_Matrix **outputs, size_t nonterms_count, size_t graph_size, char *msg);
GrB_Info adapter_CFL_get_result_common(GrB_Matrix output, size_t *result);
GrB_Info adapter_CFL_is_result_valid_common(GrB_Matrix output, size_t valid_result, bool *is_valid);
GrB_Info adapter_CFL_free_outputs_common(GrB_Matrix **outputs, size_t nonterms_count, char *msg);
GrB_Info adapter_CFL_cleanup_common(GrB_Matrix **adj_matrices, size_t terms_count, void **rules);
