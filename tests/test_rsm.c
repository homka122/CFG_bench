#include "rsm.h"

int main(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "S");
    rsm_add_nonterm(rsm, "A");

    rsm_add_term(rsm, "a");
    rsm_add_term(rsm, "b");

    rsm_add_state(rsm, "S", "s0");
    rsm_add_state(rsm, "S", "s1");
    rsm_add_state(rsm, "S", "s2");

    rsm_add_state(rsm, "A", "a0");
    rsm_add_state(rsm, "A", "a1");

    rsm_set_start_state(rsm, "S", "s0");
    rsm_set_final_state(rsm, "S", "s2");

    rsm_set_start_state(rsm, "A", "a0");
    rsm_set_final_state(rsm, "A", "a1");

    rsm_add_edge(rsm, "S", "s0", "s1", "a");
    rsm_add_edge(rsm, "S", "s1", "s2", "A");
    rsm_add_edge(rsm, "A", "a0", "a1", "b");

    rsm_free(rsm);

    return 0;
}
