#include "plausibility.h"

void plausibility_init(plausibility_t *p) {
    p->fault_active = 0;
    p->consecutive_bad = 0;
    p->consecutive_good = 0;
}

static float fabsf_local(float v) {
    return v < 0.0f ? -v : v;
}

int plausibility_update(plausibility_t *p, float value_a, float value_b,
                         float tolerance_pct, int fault_cycles, int clear_cycles) {
    float disagreement = fabsf_local(value_a - value_b);

    if (disagreement > tolerance_pct) {
        p->consecutive_bad++;
        p->consecutive_good = 0;
        if (!p->fault_active && p->consecutive_bad >= fault_cycles) {
            p->fault_active = 1;
        }
    } else {
        p->consecutive_good++;
        p->consecutive_bad = 0;
        if (p->fault_active && p->consecutive_good >= clear_cycles) {
            p->fault_active = 0;
        }
    }

    return p->fault_active;
}
