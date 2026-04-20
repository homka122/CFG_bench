#include "rsm.h"

int main(void) {
    GrB_init(GrB_NONBLOCKING);

    for (int i = RSM_TEMPLATE_AA; i <= RSM_TEMPLATE_VF; ++i) {
        CFG_RSM *cfg_rsm = rsm_create_template((RSM_Template)i);
        RSM rsm = rsm_convert_to_lagraph(cfg_rsm);

        lagraph_rsm_free(&rsm);
        rsm_free(cfg_rsm);
    }

    GrB_finalize();

    return 0;
}
