#pragma once

#include "LAGraph.h"
#include "LAGraphX.h"
#include "symbol_list.h"

typedef enum RSM_Template {
    RSM_NO_TEMPLATE,
    RSM_TEMPLATE_AA,
    RSM_TEMPLATE_VF,
    RSM_TEMPLATE_C_ALIAS,
    RSM_TEMPLATE_JAVA_POINTS_TO,
    RSM_TEMPLATE_RDF_HIERARCHY,
} RSM_Template;

typedef struct CFG_Edge {
    size_t start;
    size_t label;
    size_t end;
    bool is_term;
} CFG_Edge;

typedef struct CFG_Edges {
    CFG_Edge *data;
    size_t count;
    size_t capacity;
} CFG_Edges;

typedef struct Final_States {
    size_t *data;
    size_t count;
    size_t capacity;
} Final_States;

typedef struct CFG_RSM_Box {
    size_t nonterm;

    SymbolList states;
    CFG_Edges edges;
    size_t start_state;
    Final_States final_states;
} CFG_RSM_Box;

typedef struct CFG_RSM_Boxes {
    CFG_RSM_Box *data;
    size_t count;
    size_t capacity;
} CFG_RSM_Boxes;

typedef struct CFG_RSM {
    SymbolList nonterms;
    SymbolList terms;
    size_t start_nonterm;

    CFG_RSM_Boxes boxes;
    bool with_intial_terms;
} CFG_RSM;

CFG_RSM *rsm_create_template(RSM_Template template, bool exploded, size_t n, SymbolList *terms);

CFG_RSM *rsm_init(SymbolList *terms);
void rsm_add_nonterm(CFG_RSM *rsm, const char *nonterm);
void rsm_set_start_nonterm(CFG_RSM *rsm, const char *nonterm);
void rsm_add_term(CFG_RSM *rsm, const char *term);
void rsm_add_state(CFG_RSM *rsm, const char *nonterm, const char *state);
void rsm_add_edge(CFG_RSM *rsm, const char *nonterm, const char *state_v, const char *state_u, const char *label);
void rsm_set_start_state(CFG_RSM *rsm, const char *nonterm, const char *state);
void rsm_add_final_state(CFG_RSM *rsm, const char *nonterm, const char *state);
void rsm_free(CFG_RSM *rsm);
void rsm_print(CFG_RSM *rsm);
RSM rsm_convert_to_lagraph(CFG_RSM *rsm);
void rsm_lagraph_rsm_free(RSM *rsm);
