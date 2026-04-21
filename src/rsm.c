#include "rsm.h"
#include "symbol_list.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define RSM_NO_STATE ((size_t)-1)
#define RSM_NO_NONTERM ((size_t)-1)

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

CFG_RSM *rsm_init(SymbolList *terms) {
    CFG_RSM *result = calloc(1, sizeof(*result));

    if (result == NULL) {
        fprintf(stderr, "out of memory\n");
        abort();
    }

    result->nonterms = symbol_list_create();
    if (terms == NULL) {
        result->terms = symbol_list_create();
    } else {
        result->terms = *terms;
        result->with_intial_terms = true;
    }
    result->boxes = (CFG_RSM_Boxes){NULL, 0, 0};
    result->start_nonterm = RSM_NO_NONTERM;

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

    if (rsm->start_nonterm == RSM_NO_NONTERM) {
        rsm->start_nonterm = (size_t)nonterm_i;
    }

    return;
}

void rsm_set_start_nonterm(CFG_RSM *rsm, const char *nonterm) {
    rsm_check_not_null(rsm);

    int nonterm_i = symbol_list_get_index_str(&rsm->nonterms, nonterm);

    if (nonterm_i == -1) {
        fprintf(stderr, "there is no nonterm %s\n", nonterm);
        abort();
    }

    rsm->start_nonterm = (size_t)nonterm_i;

    return;
}

void rsm_add_term(CFG_RSM *rsm, const char *term) {
    rsm_check_not_null(rsm);

    if (symbol_list_get_index_str(&rsm->terms, term) != -1 && (!rsm->with_intial_terms)) {
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

static CFG_RSM *rsm_create_aa_template(bool exploded, size_t n, SymbolList *terms) {
    if (exploded && n == 0) {
        fprintf(stderr, "rsm_template: n should be positive when exploded = true\n");
        abort();
    }

    CFG_RSM *rsm = rsm_init(terms);

    rsm_add_nonterm(rsm, "A");

    rsm_add_term(rsm, "a");
    if (exploded) {
        for (size_t i = 0; i < n; i++) {
            char label[256];
            snprintf(label, sizeof(label), "call_%zu", i);
            rsm_add_term(rsm, label);
        }

        for (size_t i = 0; i < n; i++) {
            char label[256];
            snprintf(label, sizeof(label), "return_%zu", i);
            rsm_add_term(rsm, label);
        }
    } else {
        rsm_add_term(rsm, "call_i");
        rsm_add_term(rsm, "return_i");
    }

    rsm_add_state(rsm, "A", "0");
    if (exploded) {
        for (size_t i = 0; i < n; i++) {
            char label[256];
            snprintf(label, sizeof(label), "p_%zu", i);
            rsm_add_state(rsm, "A", label);
        }

        for (size_t i = 0; i < n; i++) {
            char label[256];
            snprintf(label, sizeof(label), "q_%zu", i);
            rsm_add_state(rsm, "A", label);
        }
    } else {
        rsm_add_state(rsm, "A", "p_i");
        rsm_add_state(rsm, "A", "q_i");
    }

    rsm_set_start_state(rsm, "A", "0");
    rsm_add_final_state(rsm, "A", "0");

    rsm_add_edge(rsm, "A", "0", "0", "a");
    if (exploded) {
        for (size_t i = 0; i < n; i++) {
            char label_first[256];
            char label_second[256];
            snprintf(label_first, sizeof(label_first), "p_%zu", i);
            snprintf(label_second, sizeof(label_second), "call_%zu", i);
            rsm_add_edge(rsm, "A", "0", label_first, label_second);
        }
    } else {
        rsm_add_edge(rsm, "A", "0", "p_i", "call_i");
    }
    if (exploded) {
        for (size_t i = 0; i < n; i++) {
            char label_first[256];
            char label_second[256];
            snprintf(label_first, sizeof(label_first), "p_%zu", i);
            snprintf(label_second, sizeof(label_second), "q_%zu", i);
            rsm_add_edge(rsm, "A", label_first, label_second, "A");
        }
    } else {
        rsm_add_edge(rsm, "A", "p_i", "q_i", "A");
    }
    if (exploded) {
        for (size_t i = 0; i < n; i++) {
            char label_first[256];
            char label_second[256];
            snprintf(label_first, sizeof(label_first), "q_%zu", i);
            snprintf(label_second, sizeof(label_second), "return_%zu", i);
            rsm_add_edge(rsm, "A", label_first, "0", label_second);
        }
    } else {
        rsm_add_edge(rsm, "A", "q_i", "0", "return_i");
    }

    return rsm;
}

static CFG_RSM *rsm_create_c_alias_template(void) {
    CFG_RSM *rsm = rsm_init(NULL);

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

static CFG_RSM *rsm_create_java_points_to_template(bool exploded, size_t n, SymbolList *terms) {
    CFG_RSM *rsm = rsm_init(terms);

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
    CFG_RSM *rsm = rsm_init(NULL);

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

static CFG_RSM *rsm_create_vf_template(bool exploded, size_t n, SymbolList *terms) {
    CFG_RSM *rsm = rsm_init(terms);

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

CFG_RSM *rsm_create_template(RSM_Template template, bool exploded, size_t n, SymbolList *terms) {
    switch (template) {
    case RSM_TEMPLATE_AA:
        return rsm_create_aa_template(exploded, n, terms);

    case RSM_TEMPLATE_VF:
        return rsm_create_vf_template(exploded, n, terms);

    case RSM_TEMPLATE_C_ALIAS:
        return rsm_create_c_alias_template();

    case RSM_TEMPLATE_JAVA_POINTS_TO:
        return rsm_create_java_points_to_template(exploded, n, terms);

    case RSM_TEMPLATE_RDF_HIERARCHY:
        return rsm_create_rdf_hierarchy_template();

    default:
        fprintf(stderr, "unknown RSM template: %d\n", (int)template);
        abort();
    }
}

static void rsm_validate_before_convert(const CFG_RSM *rsm) {
    rsm_check_not_null(rsm);

    if (rsm->boxes.count != rsm->nonterms.count) {
        fprintf(stderr, "invalid RSM: boxes count (%zu) does not match nonterms count (%zu)\n", rsm->boxes.count,
                rsm->nonterms.count);
        abort();
    }

    if (rsm->nonterms.count == 0) {
        fprintf(stderr, "invalid RSM: no nonterms\n");
        abort();
    }

    if (rsm->start_nonterm == RSM_NO_NONTERM) {
        fprintf(stderr, "invalid RSM: start nonterm is not set\n");
        abort();
    }

    if (rsm->start_nonterm >= rsm->boxes.count) {
        fprintf(stderr, "invalid RSM: start nonterm index %zu is out of bounds, boxes count is %zu\n",
                rsm->start_nonterm, rsm->boxes.count);
        abort();
    }

    for (size_t box_i = 0; box_i < rsm->boxes.count; ++box_i) {
        const CFG_RSM_Box *box = &rsm->boxes.data[box_i];

        if (box->nonterm != box_i) {
            fprintf(stderr, "invalid RSM: box %zu has nonterm index %zu\n", box_i, box->nonterm);
            abort();
        }

        if (box->nonterm >= rsm->nonterms.count) {
            fprintf(stderr, "invalid RSM: box %zu nonterm index %zu is out of bounds, nonterms count is %zu\n", box_i,
                    box->nonterm, rsm->nonterms.count);
            abort();
        }

        if (box->states.count == 0) {
            fprintf(stderr, "invalid RSM: box %zu has no states\n", box_i);
            abort();
        }

        if (box->start_state == RSM_NO_STATE) {
            fprintf(stderr, "invalid RSM: box %zu start state is not set\n", box_i);
            abort();
        }

        if (box->start_state >= box->states.count) {
            fprintf(stderr, "invalid RSM: box %zu start state index %zu is out of bounds, states count is %zu\n", box_i,
                    box->start_state, box->states.count);
            abort();
        }

        if (box->final_states.count == 0) {
            fprintf(stderr, "invalid RSM: box %zu has no final states\n", box_i);
            abort();
        }

        for (size_t final_i = 0; final_i < box->final_states.count; ++final_i) {
            size_t state = box->final_states.data[final_i];

            if (state >= box->states.count) {
                fprintf(stderr, "invalid RSM: box %zu final state index %zu is out of bounds, states count is %zu\n",
                        box_i, state, box->states.count);
                abort();
            }

            for (size_t j = final_i + 1; j < box->final_states.count; ++j) {
                if (box->final_states.data[j] == state) {
                    fprintf(stderr, "invalid RSM: box %zu has duplicate final state index %zu\n", box_i, state);
                    abort();
                }
            }
        }

        for (size_t edge_i = 0; edge_i < box->edges.count; ++edge_i) {
            const CFG_Edge *edge = &box->edges.data[edge_i];

            if (edge->start >= box->states.count) {
                fprintf(stderr,
                        "invalid RSM: box %zu edge %zu start state index %zu is out of bounds, states count is %zu\n",
                        box_i, edge_i, edge->start, box->states.count);
                abort();
            }

            if (edge->end >= box->states.count) {
                fprintf(stderr,
                        "invalid RSM: box %zu edge %zu end state index %zu is out of bounds, states count is %zu\n",
                        box_i, edge_i, edge->end, box->states.count);
                abort();
            }

            if (edge->is_term) {
                if (edge->label >= rsm->terms.count) {
                    fprintf(
                        stderr,
                        "invalid RSM: box %zu edge %zu terminal label index %zu is out of bounds, terms count is %zu\n",
                        box_i, edge_i, edge->label, rsm->terms.count);
                    abort();
                }
            } else {
                if (edge->label >= rsm->nonterms.count) {
                    fprintf(stderr,
                            "invalid RSM: box %zu edge %zu nonterminal label index %zu is out of bounds, nonterms "
                            "count is %zu\n",
                            box_i, edge_i, edge->label, rsm->nonterms.count);
                    abort();
                }

                if (edge->label >= rsm->boxes.count) {
                    fprintf(stderr,
                            "invalid RSM: box %zu edge %zu nonterminal label index %zu has no corresponding box\n",
                            box_i, edge_i, edge->label);
                    abort();
                }
            }
        }
    }
}

#define RSM_ABORT(...)                                                                                                 \
    do {                                                                                                               \
        fprintf(stderr, __VA_ARGS__);                                                                                  \
        fputc('\n', stderr);                                                                                           \
        abort();                                                                                                       \
    } while (0)

#define RSM_CHECK(cond, ...)                                                                                           \
    do {                                                                                                               \
        if (!(cond)) {                                                                                                 \
            RSM_ABORT(__VA_ARGS__);                                                                                    \
        }                                                                                                              \
    } while (0)

#define RSM_GRB_CHECK(expr)                                                                                            \
    do {                                                                                                               \
        GrB_Info info = (expr);                                                                                        \
        if (info != GrB_SUCCESS) {                                                                                     \
            RSM_ABORT("GraphBLAS error %d at %s:%d", (int)info, __FILE__, __LINE__);                                   \
        }                                                                                                              \
    } while (0)

RSM rsm_convert_to_lagraph(CFG_RSM *cfg) {
    rsm_check_not_null(cfg);
    rsm_validate_before_convert(cfg);

    RSM result = {0};

    // code by https://github.com/Zestria
    // ispired by
    // https://github.com/SparseLinearAlgebra/LAGraph/pull/7/changes#diff-a7b0227939d51efc141437082399c0b7449c85b36691f507b3ca8c26ae1c5ce3R152

    GrB_Index total_states = 0;

    for (size_t i = 0; i < cfg->boxes.count; i++) {
        total_states += (GrB_Index)cfg->boxes.data[i].states.count;
    }

    result.state_count = total_states;
    result.terminal_count = (GrB_Index)cfg->terms.count;
    result.nonterminal_count = (GrB_Index)cfg->nonterms.count;
    result.start_nonterminal = (GrB_Index)cfg->start_nonterm;

    result.terminal_matrices = calloc(cfg->terms.count, sizeof(*result.terminal_matrices));
    result.nonterminal_matrices = calloc(cfg->nonterms.count, sizeof(*result.nonterminal_matrices));
    result.start_states = calloc(cfg->nonterms.count, sizeof(*result.start_states));
    result.final_states = calloc(cfg->nonterms.count, sizeof(*result.final_states));

    RSM_CHECK(result.terminal_matrices != NULL || cfg->terms.count == 0, "out of memory");

    RSM_CHECK(result.nonterminal_matrices != NULL || cfg->nonterms.count == 0, "out of memory");

    RSM_CHECK(result.start_states != NULL || cfg->nonterms.count == 0, "out of memory");

    RSM_CHECK(result.final_states != NULL || cfg->nonterms.count == 0, "out of memory");

    for (size_t i = 0; i < cfg->terms.count; i++) {
        RSM_GRB_CHECK(GrB_Matrix_new(&result.terminal_matrices[i], GrB_BOOL, total_states, total_states));
    }

    for (size_t i = 0; i < cfg->nonterms.count; i++) {
        RSM_GRB_CHECK(GrB_Matrix_new(&result.nonterminal_matrices[i], GrB_BOOL, total_states, total_states));

        RSM_GRB_CHECK(GrB_Vector_new(&result.final_states[i], GrB_BOOL, total_states));
    }

    GrB_Index *state_offsets = calloc(cfg->boxes.count, sizeof(*state_offsets));

    RSM_CHECK(state_offsets != NULL || cfg->boxes.count == 0, "out of memory");

    GrB_Index offset = 0;

    for (size_t box_i = 0; box_i < cfg->boxes.count; box_i++) {
        state_offsets[box_i] = offset;
        offset += (GrB_Index)cfg->boxes.data[box_i].states.count;
    }

    for (size_t box_i = 0; box_i < cfg->boxes.count; box_i++) {
        const CFG_RSM_Box *box = &cfg->boxes.data[box_i];

        GrB_Index box_offset = state_offsets[box_i];

        result.start_states[box_i] = box_offset + (GrB_Index)box->start_state;

        for (size_t final_i = 0; final_i < box->final_states.count; ++final_i) {
            GrB_Index global_final_state = box_offset + (GrB_Index)box->final_states.data[final_i];

            RSM_GRB_CHECK(GrB_Vector_setElement_BOOL(result.final_states[box_i], true, global_final_state));
        }

        for (size_t edge_i = 0; edge_i < box->edges.count; ++edge_i) {
            const CFG_Edge *edge = &box->edges.data[edge_i];

            GrB_Index from = box_offset + (GrB_Index)edge->start;
            GrB_Index to = box_offset + (GrB_Index)edge->end;

            if (edge->is_term) {
                RSM_GRB_CHECK(GrB_Matrix_setElement_BOOL(result.terminal_matrices[edge->label], true, from, to));
            } else {
                RSM_GRB_CHECK(GrB_Matrix_setElement_BOOL(result.nonterminal_matrices[edge->label], true, from, to));
            }
        }
    }

    free(state_offsets);

    return result;
}

void rsm_lagraph_rsm_free(RSM *rsm) {
    if (rsm == NULL) {
        return;
    }

    if (rsm->terminal_matrices != NULL) {
        for (GrB_Index i = 0; i < rsm->terminal_count; i++) {
            if (rsm->terminal_matrices[i] != NULL) {
                GrB_Matrix_free(&rsm->terminal_matrices[i]);
            }
        }

        free(rsm->terminal_matrices);
    }

    if (rsm->nonterminal_matrices != NULL) {
        for (GrB_Index i = 0; i < rsm->nonterminal_count; i++) {
            if (rsm->nonterminal_matrices[i] != NULL) {
                GrB_Matrix_free(&rsm->nonterminal_matrices[i]);
            }
        }

        free(rsm->nonterminal_matrices);
    }

    if (rsm->final_states != NULL) {
        for (GrB_Index i = 0; i < rsm->nonterminal_count; i++) {
            if (rsm->final_states[i] != NULL) {
                GrB_Vector_free(&rsm->final_states[i]);
            }
        }

        free(rsm->final_states);
    }

    free(rsm->start_states);

    *rsm = (RSM){0};
}
