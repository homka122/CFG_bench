#include "rsm.h"
#include <stdio.h>

int main(void) {
    CFG_RSM *rsm = rsm_init();

    rsm_add_nonterm(rsm, "S");
    rsm_add_term(rsm, "a");

    char name[64];

    for (int i = 0; i < 1000; ++i) {
        snprintf(name, sizeof(name), "q%d", i);
        rsm_add_state(rsm, "S", name);
    }

    rsm_set_start_state(rsm, "S", "q0");
    rsm_set_final_state(rsm, "S", "q999");

    for (int i = 0; i < 999; ++i) {
        char from[64];
        char to[64];

        snprintf(from, sizeof(from), "q%d", i);
        snprintf(to, sizeof(to), "q%d", i + 1);

        rsm_add_edge(rsm, "S", from, to, "a");
    }

    rsm_free(rsm);
    return 0;
}
