#include "rsm.h"
#include "symbol_list.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RSM_NO_STATE ((size_t)-1)

#define DA_INITIAL_CAPACITY 8

#define DA_PUSH(da, value)                                                                                             \
    do {                                                                                                               \
        if ((da)->count >= (da)->capacity) {                                                                           \
            size_t old_capacity = (da)->capacity;                                                                      \
            size_t new_capacity = old_capacity == 0 ? DA_INITIAL_CAPACITY : old_capacity * 2;                          \
                                                                                                                       \
            void *new_data = realloc((da)->data, new_capacity * sizeof(*(da)->data));                                  \
                                                                                                                       \
            if (new_data == NULL) {                                                                                    \
                fprintf(stderr, "out of memory\n");                                                                    \
                abort();                                                                                               \
            }                                                                                                          \
                                                                                                                       \
            (da)->data = new_data;                                                                                     \
            (da)->capacity = new_capacity;                                                                             \
        }                                                                                                              \
                                                                                                                       \
        (da)->data[(da)->count++] = (value);                                                                           \
    } while (0)

#define DA_FREE(da)                                                                                                    \
    do {                                                                                                               \
        free((da)->data);                                                                                              \
        (da)->data = NULL;                                                                                             \
        (da)->count = 0;                                                                                               \
        (da)->capacity = 0;                                                                                            \
    } while (0)

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

static CFG_RSM_Box rsm_box_init(size_t nonterm) {
    CFG_RSM_Box result = {0};

    result.nonterm = nonterm;
    result.states = symbol_list_create();
    result.start_state = RSM_NO_STATE;

    return result;
}

static void rsm_box_free(CFG_RSM_Box *box) {
    if (box == NULL) {
        return;
    }

    symbol_list_free(&box->states);
    DA_FREE(&box->edges);
    DA_FREE(&box->final_states);

    *box = (CFG_RSM_Box){0};
    box->start_state = RSM_NO_STATE;

    return;
}

typedef struct CFG_RSM_Boxes {
    CFG_RSM_Box *data;
    size_t count;
    size_t capacity;
} CFG_RSM_Boxes;

typedef struct CFG_RSM {
    SymbolList nonterms;
    SymbolList terms;

    CFG_RSM_Boxes boxes;
} CFG_RSM;

static void rsm_check_not_null(const CFG_RSM *rsm) {
    if (rsm == NULL) {
        fprintf(stderr, "rsm is NULL\n");
        abort();
    }
}

static CFG_RSM_Box *rsm_get_box(CFG_RSM *rsm, const char *nonterm) {
    int nonterm_i = symbol_list_get_index_str(&rsm->nonterms, nonterm);

    if (nonterm_i == -1) {
        fprintf(stderr, "there is no nonterm %s\n", nonterm);
        abort();
    }

    return &rsm->boxes.data[nonterm_i];
}

CFG_RSM *rsm_init(void) {
    CFG_RSM *result = calloc(1, sizeof(*result));

    if (result == NULL) {
        fprintf(stderr, "out of memory\n");
        abort();
    }

    result->nonterms = symbol_list_create();
    result->terms = symbol_list_create();
    result->boxes = (CFG_RSM_Boxes){NULL, 0, 0};

    return result;
}

void rsm_add_nonterm(CFG_RSM *rsm, const char *nonterm) {
    rsm_check_not_null(rsm);

    if (symbol_list_get_index_str(&rsm->nonterms, nonterm) != -1) {
        fprintf(stderr, "nonterm %s already exists\n", nonterm);
        abort();
    }

    if (symbol_list_get_index_str(&rsm->terms, nonterm) != -1) {
        fprintf(stderr, "symbol %s already exists as term\n", nonterm);
        abort();
    }

    int nonterm_i = symbol_list_add_str(&rsm->nonterms, nonterm, true);

    if (nonterm_i < 0 || (size_t)nonterm_i != rsm->boxes.count) {
        fprintf(stderr, "nonterm index does not match box index\n");
        abort();
    }

    CFG_RSM_Box box = rsm_box_init((size_t)nonterm_i);
    DA_PUSH(&rsm->boxes, box);

    return;
}

void rsm_add_term(CFG_RSM *rsm, const char *term) {
    rsm_check_not_null(rsm);

    if (symbol_list_get_index_str(&rsm->terms, term) != -1) {
        fprintf(stderr, "term %s already exists\n", term);
        abort();
    }

    if (symbol_list_get_index_str(&rsm->nonterms, term) != -1) {
        fprintf(stderr, "symbol %s already exists as nonterm\n", term);
        abort();
    }

    symbol_list_add_str(&rsm->terms, term, false);

    return;
}

void rsm_add_state(CFG_RSM *rsm, const char *nonterm, const char *state) {
    rsm_check_not_null(rsm);

    CFG_RSM_Box *box = rsm_get_box(rsm, nonterm);

    if (symbol_list_get_index_str(&box->states, state) != -1) {
        fprintf(stderr, "state %s already exists\n", state);
        abort();
    }

    symbol_list_add_str(&box->states, state, false);

    return;
}

void rsm_add_edge(CFG_RSM *rsm, const char *nonterm, const char *state_v, const char *state_u, const char *label) {
    rsm_check_not_null(rsm);

    CFG_RSM_Box *box = rsm_get_box(rsm, nonterm);

    int state_v_i = symbol_list_get_index_str(&box->states, state_v);

    if (state_v_i == -1) {
        fprintf(stderr, "there is no state %s\n", state_v);
        abort();
    }

    int state_u_i = symbol_list_get_index_str(&box->states, state_u);

    if (state_u_i == -1) {
        fprintf(stderr, "there is no state %s\n", state_u);
        abort();
    }

    int label_i = 0;
    int label_term_i = symbol_list_get_index_str(&rsm->terms, label);
    int label_nonterm_i = symbol_list_get_index_str(&rsm->nonterms, label);

    bool is_term = false;

    if (label_term_i != -1) {
        label_i = label_term_i;
        is_term = true;
    } else if (label_nonterm_i != -1) {
        label_i = label_nonterm_i;
        is_term = false;
    } else {
        fprintf(stderr, "there is no term or nonterm with label %s\n", label);
        abort();
    }

    CFG_Edge edge = {
        .start = (size_t)state_v_i,
        .label = (size_t)label_i,
        .end = (size_t)state_u_i,
        .is_term = is_term,
    };
    DA_PUSH(&box->edges, edge);

    return;
}

void rsm_set_start_state(CFG_RSM *rsm, const char *nonterm, const char *state) {
    rsm_check_not_null(rsm);

    CFG_RSM_Box *box = rsm_get_box(rsm, nonterm);

    int state_i = symbol_list_get_index_str(&box->states, state);

    if (state_i == -1) {
        fprintf(stderr, "there is no state %s\n", state);
        abort();
    }

    box->start_state = (size_t)state_i;

    return;
}

void rsm_add_final_state(CFG_RSM *rsm, const char *nonterm, const char *state) {
    rsm_check_not_null(rsm);

    CFG_RSM_Box *box = rsm_get_box(rsm, nonterm);

    int state_i = symbol_list_get_index_str(&box->states, state);

    if (state_i == -1) {
        fprintf(stderr, "there is no state %s\n", state);
        abort();
    }

    for (size_t i = 0; i < box->final_states.count; ++i) {
        if (box->final_states.data[i] == (size_t)state_i) {
            fprintf(stderr, "state %s is already final\n", state);
            abort();
        }
    }

    DA_PUSH(&box->final_states, (size_t)state_i);

    return;
}

void rsm_free(CFG_RSM *rsm) {
    if (rsm == NULL) {
        return;
    }

    symbol_list_free(&rsm->nonterms);
    symbol_list_free(&rsm->terms);
    for (size_t i = 0; i < rsm->boxes.count; i++) {
        rsm_box_free(&rsm->boxes.data[i]);
    }
    DA_FREE(&rsm->boxes);

    free(rsm);

    return;
}

static CFG_RSM *rsm_create_aa_template(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "A");

    rsm_add_term(rsm, "a");
    rsm_add_term(rsm, "call_i");
    rsm_add_term(rsm, "return_i");

    rsm_add_state(rsm, "A", "0");
    rsm_add_state(rsm, "A", "p_i");
    rsm_add_state(rsm, "A", "q_i");

    rsm_set_start_state(rsm, "A", "0");
    rsm_add_final_state(rsm, "A", "0");

    rsm_add_edge(rsm, "A", "0", "0", "a");
    rsm_add_edge(rsm, "A", "0", "p_i", "call_i");
    rsm_add_edge(rsm, "A", "p_i", "q_i", "A");
    rsm_add_edge(rsm, "A", "q_i", "0", "return_i");

    return rsm;
}

static CFG_RSM *rsm_create_c_alias_template(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "S");
    rsm_add_nonterm(rsm, "V");

    rsm_add_term(rsm, "d_rev");
    rsm_add_term(rsm, "d");
    rsm_add_term(rsm, "a");
    rsm_add_term(rsm, "a_rev");

    rsm_add_state(rsm, "S", "0");
    rsm_add_state(rsm, "S", "1");
    rsm_add_state(rsm, "S", "2");
    rsm_add_state(rsm, "S", "3");

    rsm_set_start_state(rsm, "S", "0");
    rsm_add_final_state(rsm, "S", "3");

    rsm_add_edge(rsm, "S", "0", "1", "d_rev");
    rsm_add_edge(rsm, "S", "1", "2", "V");
    rsm_add_edge(rsm, "S", "2", "3", "d");

    rsm_add_state(rsm, "V", "0");
    rsm_add_state(rsm, "V", "1");
    rsm_add_state(rsm, "V", "2");
    rsm_add_state(rsm, "V", "3");

    rsm_set_start_state(rsm, "V", "0");
    rsm_add_final_state(rsm, "V", "0");
    rsm_add_final_state(rsm, "V", "1");
    rsm_add_final_state(rsm, "V", "2");
    rsm_add_final_state(rsm, "V", "3");

    rsm_add_edge(rsm, "V", "0", "2", "a");
    rsm_add_edge(rsm, "V", "0", "0", "a_rev");
    rsm_add_edge(rsm, "V", "0", "1", "S");
    rsm_add_edge(rsm, "V", "1", "0", "a_rev");
    rsm_add_edge(rsm, "V", "1", "2", "a");
    rsm_add_edge(rsm, "V", "2", "2", "a");
    rsm_add_edge(rsm, "V", "2", "3", "S");
    rsm_add_edge(rsm, "V", "3", "2", "a");

    return rsm;
}

static CFG_RSM *rsm_create_java_points_to_template(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "Alias");
    rsm_add_nonterm(rsm, "PointsTo");
    rsm_add_nonterm(rsm, "FlowsTo");

    rsm_add_term(rsm, "assign");
    rsm_add_term(rsm, "alloc");
    rsm_add_term(rsm, "load_i");
    rsm_add_term(rsm, "store_i");
    rsm_add_term(rsm, "alloc_rev");
    rsm_add_term(rsm, "store_i_rev");
    rsm_add_term(rsm, "load_i_rev");
    rsm_add_term(rsm, "assign_rev");

    rsm_add_state(rsm, "Alias", "0");
    rsm_add_state(rsm, "Alias", "1");
    rsm_add_state(rsm, "Alias", "2");

    rsm_set_start_state(rsm, "Alias", "0");
    rsm_add_final_state(rsm, "Alias", "2");

    rsm_add_edge(rsm, "Alias", "0", "1", "PointsTo");
    rsm_add_edge(rsm, "Alias", "1", "2", "FlowsTo");

    rsm_add_state(rsm, "PointsTo", "0");
    rsm_add_state(rsm, "PointsTo", "p_i");
    rsm_add_state(rsm, "PointsTo", "q_i");
    rsm_add_state(rsm, "PointsTo", "1");

    rsm_set_start_state(rsm, "PointsTo", "0");
    rsm_add_final_state(rsm, "PointsTo", "1");

    rsm_add_edge(rsm, "PointsTo", "0", "0", "assign");
    rsm_add_edge(rsm, "PointsTo", "0", "1", "alloc");
    rsm_add_edge(rsm, "PointsTo", "0", "p_i", "load_i");
    rsm_add_edge(rsm, "PointsTo", "p_i", "q_i", "Alias");
    rsm_add_edge(rsm, "PointsTo", "q_i", "0", "store_i");

    rsm_add_state(rsm, "FlowsTo", "0");
    rsm_add_state(rsm, "FlowsTo", "p_i");
    rsm_add_state(rsm, "FlowsTo", "q_i");
    rsm_add_state(rsm, "FlowsTo", "1");

    rsm_set_start_state(rsm, "FlowsTo", "0");
    rsm_add_final_state(rsm, "FlowsTo", "1");

    rsm_add_edge(rsm, "FlowsTo", "0", "1", "alloc_rev");
    rsm_add_edge(rsm, "FlowsTo", "1", "p_i", "store_i_rev");
    rsm_add_edge(rsm, "FlowsTo", "p_i", "q_i", "Alias");
    rsm_add_edge(rsm, "FlowsTo", "q_i", "1", "load_i_rev");
    rsm_add_edge(rsm, "FlowsTo", "1", "1", "assign_rev");

    return rsm;
}

static CFG_RSM *rsm_create_rdf_hierarchy_template(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "S");

    rsm_add_term(rsm, "type_rev");
    rsm_add_term(rsm, "type");
    rsm_add_term(rsm, "subClassOf_rev");
    rsm_add_term(rsm, "subClassOf");

    rsm_add_state(rsm, "S", "0");
    rsm_add_state(rsm, "S", "1");
    rsm_add_state(rsm, "S", "2");
    rsm_add_state(rsm, "S", "3");
    rsm_add_state(rsm, "S", "4");
    rsm_add_state(rsm, "S", "5");

    rsm_set_start_state(rsm, "S", "0");
    rsm_add_final_state(rsm, "S", "2");

    rsm_add_edge(rsm, "S", "0", "1", "type_rev");
    rsm_add_edge(rsm, "S", "1", "2", "type");

    rsm_add_edge(rsm, "S", "0", "3", "subClassOf_rev");
    rsm_add_edge(rsm, "S", "3", "2", "subClassOf");

    rsm_add_edge(rsm, "S", "3", "5", "S");
    rsm_add_edge(rsm, "S", "5", "2", "subClassOf");

    rsm_add_edge(rsm, "S", "1", "4", "S");
    rsm_add_edge(rsm, "S", "4", "2", "type");

    return rsm;
}

static CFG_RSM *rsm_create_vf_template(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "M");
    rsm_add_nonterm(rsm, "V");

    rsm_add_term(rsm, "d_rev");
    rsm_add_term(rsm, "d");
    rsm_add_term(rsm, "a_rev");
    rsm_add_term(rsm, "a");
    rsm_add_term(rsm, "f_i_rev");
    rsm_add_term(rsm, "f_i");

    rsm_add_state(rsm, "M", "0");
    rsm_add_state(rsm, "M", "1");
    rsm_add_state(rsm, "M", "2");
    rsm_add_state(rsm, "M", "3");

    rsm_set_start_state(rsm, "M", "0");
    rsm_add_final_state(rsm, "M", "3");

    rsm_add_edge(rsm, "M", "0", "1", "d_rev");
    rsm_add_edge(rsm, "M", "1", "2", "V");
    rsm_add_edge(rsm, "M", "2", "3", "d");

    rsm_add_state(rsm, "V", "0");
    rsm_add_state(rsm, "V", "1");
    rsm_add_state(rsm, "V", "2");
    rsm_add_state(rsm, "V", "3");
    rsm_add_state(rsm, "V", "4");
    rsm_add_state(rsm, "V", "5");
    rsm_add_state(rsm, "V", "p_i");
    rsm_add_state(rsm, "V", "q_i");

    rsm_set_start_state(rsm, "V", "0");

    rsm_add_final_state(rsm, "V", "0");
    rsm_add_final_state(rsm, "V", "1");
    rsm_add_final_state(rsm, "V", "2");
    rsm_add_final_state(rsm, "V", "3");
    rsm_add_final_state(rsm, "V", "4");

    rsm_add_edge(rsm, "V", "0", "1", "V");
    rsm_add_edge(rsm, "V", "0", "4", "M");
    rsm_add_edge(rsm, "V", "0", "5", "a_rev");
    rsm_add_edge(rsm, "V", "4", "5", "a_rev");
    rsm_add_edge(rsm, "V", "5", "1", "V");
    rsm_add_edge(rsm, "V", "1", "2", "a");
    rsm_add_edge(rsm, "V", "2", "3", "M");
    rsm_add_edge(rsm, "V", "0", "p_i", "f_i_rev");
    rsm_add_edge(rsm, "V", "p_i", "q_i", "V");
    rsm_add_edge(rsm, "V", "q_i", "3", "f_i");

    return rsm;
}

CFG_RSM *rsm_create_template(RSM_Template template) {
    switch (template) {
    case RSM_TEMPLATE_AA:
        return rsm_create_aa_template();

    case RSM_TEMPLATE_VF:
        return rsm_create_vf_template();

    case RSM_TEMPLATE_C_ALIAS:
        return rsm_create_c_alias_template();

    case RSM_TEMPLATE_JAVA_POINTS_TO:
        return rsm_create_java_points_to_template();

    case RSM_TEMPLATE_RDF_HIERARCHY:
        return rsm_create_rdf_hierarchy_template();

    default:
        fprintf(stderr, "unknown RSM template: %d\n", (int)template);
        abort();
    }
}

// TODO
RSM rsm_convert_to_lagraph(CFG_RSM *rsm);
