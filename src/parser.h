#pragma once

#include "symbol_list.h"
#include <LAGraph.h>
#include <LAGraphX.h>

#define LG_ERROR_MSG(...)                                                                                              \
    {                                                                                                                  \
        if (msg != NULL && msg[0] == '\0') {                                                                           \
            snprintf(msg, LAGRAPH_MSG_LEN, __VA_ARGS__);                                                               \
        }                                                                                                              \
    }

#ifndef GRB_CATCH
#define GRB_CATCH(info)                                                                                                \
    {                                                                                                                  \
        LG_ERROR_MSG("GraphBLAS failure (file %s, line %d): info: %d", __FILE__, __LINE__, info);                      \
        fprintf(stderr, "%s\n", msg);                                                                                  \
        fflush(stderr);                                                                                                \
        exit(info);                                                                                                    \
    }
#endif

#define MY_GRB_TRY(GrB_method)                                                                                         \
    {                                                                                                                  \
        GrB_Info LG_GrB_Info = GrB_method;                                                                             \
        if (LG_GrB_Info < GrB_SUCCESS) {                                                                               \
            GRB_CATCH(LG_GrB_Info);                                                                                    \
        }                                                                                                              \
    }

typedef struct {
    size_t nonterms_count;
    size_t rules_count;
    LAGraph_rule_EWCNF *rules;
} grammar_t;

typedef struct {
    int first;
    int second;
    int third;
} Rule;

typedef struct {
    int start_nonterm;
    Rule *rules;
    size_t rules_count;
} Grammar;

// Edge is a tuple (u, v, a, index)
// "term_index" is index of terminal in symbol_list
// "index" is index for indexed terms
typedef struct {
    size_t u;
    size_t v;
    size_t term_index;
    size_t index;
} GraphEdge;

// Graph is a set of edges
// Nodes must be enumerate with zero based indexing
//
// "node_count" is max(v) + 1 for all v in graph
// "block_count" is max(i) + 1 for all indecies in graph.
//      if there are no indicies "block_count" is equal to "1"
typedef struct {
    GraphEdge *edges;
    size_t edge_count;
    size_t node_count;
    size_t block_count;
} Graph;

typedef struct {
    size_t node_count;
    size_t block_count;
    Grammar grammar;
    SymbolList symbols;
    Graph graph;
} ParserResult;

typedef struct {
    char *grammar;
    char *graph;
    size_t valid_result;
} config_row;

// parse graph via "graph_file" desctriptor
//
// you must initilized symbol_list with symbol_list_create() function
//
// graph format:
// [u v a index], where is index is optional
// 
// output is Graph structure and modified symbol_list
Graph process_graph(FILE *graph_file, SymbolList *symbol_list);

ParserResult parser(config_row config_i);

void grammar_print(Grammar grammar, SymbolList list);
void get_configs_from_file(char *path, size_t *configs_count, config_row *configs);