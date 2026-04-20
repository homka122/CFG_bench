#pragma once

#include "LAGraph.h"
#include "LAGraphX.h"

typedef struct CFG_RSM CFG_RSM;

typedef enum RSM_Template {
    RSM_TEMPLATE_AA,
    RSM_TEMPLATE_VF,
    RSM_TEMPLATE_C_ALIAS,
    RSM_TEMPLATE_JAVA_POINTS_TO,
    RSM_TEMPLATE_RDF_HIERARCHY,
} RSM_Template;

CFG_RSM *rsm_create_template(RSM_Template template);

CFG_RSM *rsm_init(void);
void rsm_add_nonterm(CFG_RSM *rsm, const char *nonterm);
void rsm_add_term(CFG_RSM *rsm, const char *term);
void rsm_add_state(CFG_RSM *rsm, const char *nonterm, const char *state);
void rsm_add_edge(CFG_RSM *rsm, const char *nonterm, const char *state_v, const char *state_u, const char *label);
void rsm_set_start_state(CFG_RSM *rsm, const char *nonterm, const char *state);
void rsm_add_final_state(CFG_RSM *rsm, const char *nonterm, const char *state);
RSM rsm_convert_to_lagraph(CFG_RSM *rsm);
void rsm_free(CFG_RSM *rsm);
