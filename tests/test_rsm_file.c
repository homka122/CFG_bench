#include "../src/rsm_file.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t state(const CFG_RSM_Box *box, const char *name) {
    int index = symbol_list_get_index_str((SymbolList *)&box->states, name);
    assert(index != -1);
    return (size_t)index;
}

static bool has_edge(const CFG_RSM *rsm, const CFG_RSM_Box *box, const char *from, const char *label, const char *to) {
    for (size_t i = 0; i < box->edges.count; i++) {
        CFG_Edge edge = box->edges.data[i];
        const char *edge_label =
            symbol_list_get_str(edge.is_term ? (SymbolList *)&rsm->terms : (SymbolList *)&rsm->nonterms, edge.label);
        if (edge.start == state(box, from) && edge.end == state(box, to) && strcmp(edge_label, label) == 0) {
            return true;
        }
    }
    return false;
}

static bool is_final(const CFG_RSM_Box *box, const char *name) {
    for (size_t i = 0; i < box->final_states.count; i++) {
        if (box->final_states.data[i] == state(box, name)) {
            return true;
        }
    }
    return false;
}

static SymbolList graph_terms(void) {
    SymbolList terms = symbol_list_create();
    symbol_list_add_str(&terms, "alloc", false);
    symbol_list_add_str(&terms, "load_0", false);
    symbol_list_add_str(&terms, "load_1", false);
    return terms;
}

static void test_indexed_boxes(void) {
    const char *text = "# java points-to, cut down\n"
                       "start: PointsTo\n"
                       "[box PointsTo]\n"
                       "start: 0\n"
                       "final: 1\n"
                       "0 --alloc--> 1\n"
                       "0 --load_i--> p_i\n"
                       "p_i --Alias--> q_i\n"
                       "q_i --store_i--> 0\n"
                       "\n"
                       "[box Alias]\n"
                       "start: 0\n"
                       "final: 1, 2\n"
                       "0 --PointsTo--> 1\n"
                       "1 --PointsTo--> 2\n";
    SymbolList terms = graph_terms();
    CFG_RSM *rsm = rsm_from_text(text, "test", 2, &terms);
    assert(rsm != NULL);

    assert(rsm->nonterms.count == 2);
    assert(strcmp(symbol_list_get_str(&rsm->nonterms, rsm->start_nonterm), "PointsTo") == 0);
    // graph terminals keep their indices, store_0 and store_1 are appended
    assert(rsm->terms.count == 5);
    assert(symbol_list_get_index_str(&rsm->terms, "load_0") == 1);
    assert(symbol_list_get_index_str(&rsm->terms, "store_1") != -1);
    assert(symbol_list_get_index_str(&rsm->terms, "load_i") == -1);

    const CFG_RSM_Box *points_to = &rsm->boxes.data[0];
    assert(points_to->states.count == 6); // 0, 1, p_0, p_1, q_0, q_1
    assert(points_to->start_state == state(points_to, "0"));
    assert(points_to->final_states.count == 1 && is_final(points_to, "1"));
    assert(points_to->edges.count == 7);
    assert(has_edge(rsm, points_to, "0", "alloc", "1"));
    assert(has_edge(rsm, points_to, "0", "load_0", "p_0"));
    assert(has_edge(rsm, points_to, "0", "load_1", "p_1"));
    assert(has_edge(rsm, points_to, "p_0", "Alias", "q_0"));
    assert(has_edge(rsm, points_to, "p_1", "Alias", "q_1"));
    assert(has_edge(rsm, points_to, "q_0", "store_0", "0"));
    assert(has_edge(rsm, points_to, "q_1", "store_1", "0"));
    assert(!has_edge(rsm, points_to, "p_0", "Alias", "q_1"));

    const CFG_RSM_Box *alias = &rsm->boxes.data[1];
    assert(alias->states.count == 3);
    assert(alias->final_states.count == 2 && is_final(alias, "1") && is_final(alias, "2"));
    assert(has_edge(rsm, alias, "0", "PointsTo", "1"));

    rsm_free(rsm);
}

static void test_default_start_box(void) {
    SymbolList terms = symbol_list_create();
    CFG_RSM *rsm =
        rsm_from_text("[box A]\nstart: 0\nfinal: 0\n[box S]\nstart: 0\nfinal: 1\n0 --A--> 1\n", "test", 1, &terms);
    assert(rsm != NULL);
    assert(strcmp(symbol_list_get_str(&rsm->nonterms, rsm->start_nonterm), "S") == 0);
    rsm_free(rsm);
}

static void test_errors(void) {
    const char *invalid[] = {
        "",                                            // no boxes
        "start: S\n",                                  // start box without boxes
        "[box S]\nfinal: 0\n",                         // no start state
        "[box S]\nstart: 0\nstart: 1\n",               // two start states
        "[box S]\nstart: 0\n0 -a-> 1\n",               // bad transition syntax
        "[box S]\nstart: 0\n0 --a--> 1\n0 --a--> 2\n", // nondeterministic
        "[box S]\nstart: 0\n[box S]\nstart: 0\n",      // duplicate box
        "start: T\n[box S]\nstart: 0\n",               // unknown start box
        "[box S]\nstart: p_i\n",                       // indexed start state
        "0 --a--> 1\n[box S]\nstart: 0\n",             // transition before the first box
    };
    for (size_t i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        SymbolList terms = symbol_list_create();
        fprintf(stderr, "expected error %zu: ", i);
        assert(rsm_from_text(invalid[i], "test", 1, &terms) == NULL);
    }

    SymbolList terms = symbol_list_create();
    assert(rsm_from_file("/nonexistent/grammar.rsm", 1, &terms) == NULL);
    symbol_list_free(&terms);
}

int main(void) {
    test_indexed_boxes();
    test_default_start_box();
    test_errors();
    printf("test_rsm_file: OK\n");
    return 0;
}
