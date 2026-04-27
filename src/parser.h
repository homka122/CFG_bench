#pragma once

#include "grammar.h"
#include "rsm.h"
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
    RSM_Template rsm_template;
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
GrB_Matrix *get_grb_matrices_from_graph(Graph graph, SymbolList *list);

// minimize graph nodes
//
// for example with 1000x1000 graph and only edge 0 -> 600 it makes 2x2 graph with edge 0 -> 1 with same label
void *minimize_graph(Graph *graph);

// before: homka_i
// after: homka_1
//        homka_2
//        homka_3
void explode_indices(Grammar *grammar, Graph *graph, SymbolList *list);

ParserResult parser(config_row config_i);

void grammar_print(Grammar grammar, SymbolList list);
void get_configs_from_file(char *path, size_t *configs_count, config_row *configs, char **text_p);
