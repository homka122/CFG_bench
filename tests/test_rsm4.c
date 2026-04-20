#include "rsm.h"

int main(void) {
    for (int i = RSM_TEMPLATE_AA; i <= RSM_TEMPLATE_VF; ++i) {
        CFG_RSM *rsm = rsm_create_template((RSM_Template)i);
        rsm_free(rsm);
    }

    return 0;
}
