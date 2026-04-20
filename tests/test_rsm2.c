#include "rsm.h"
#include <stdio.h>
#include <string.h>

static void add_from_buffer(CFG_RSM *rsm) {
    char nonterm[16];
    char state0[16];
    char state1[16];
    char term[16];

    strcpy(nonterm, "S");
    strcpy(state0, "q0");
    strcpy(state1, "q1");
    strcpy(term, "a");

    rsm_add_nonterm(rsm, nonterm);
    rsm_add_term(rsm, term);
    rsm_add_state(rsm, nonterm, state0);
    rsm_add_state(rsm, nonterm, state1);
    rsm_set_start_state(rsm, nonterm, state0);
    rsm_set_final_state(rsm, nonterm, state1);
    rsm_add_edge(rsm, nonterm, state0, state1, term);
}

int main(void) {
    CFG_RSM *rsm = rsm_init();

    add_from_buffer(rsm);
    rsm_add_edge(rsm, "S", "q0", "q1", "a");

    rsm_free(rsm);
    return 0;
}
