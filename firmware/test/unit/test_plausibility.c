#include "test_framework.h"
#include "../../src/config.h"
#include "../../src/plausibility.h"

#include <stdio.h>

void test_plausibility_raises_at_exact_fault_cycle_count(void) {
    printf("test_plausibility_raises_at_exact_fault_cycle_count:\n");
    plausibility_t p;
    plausibility_init(&p);

    /* Disagreement well past tolerance every cycle. */
    const float a = 50.0f;
    const float b = 50.0f + SENSOR_PLAUSIBILITY_TOLERANCE_PCT + 1.0f;

    int fault = 0;
    for (int i = 1; i <= PLAUSIBILITY_FAULT_CYCLES; i++) {
        fault = plausibility_update(&p, a, b, SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                     PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);
        if (i < PLAUSIBILITY_FAULT_CYCLES) {
            char msg[96];
            snprintf(msg, sizeof(msg), "must not raise before cycle %d (currently cycle %d)",
                     PLAUSIBILITY_FAULT_CYCLES, i);
            HELM_CHECK_INT_EQ(fault, 0, msg);
        }
    }
    HELM_CHECK_INT_EQ(fault, 1, "must raise at exactly PLAUSIBILITY_FAULT_CYCLES");
}

void test_plausibility_clears_at_exact_clear_cycle_count(void) {
    printf("test_plausibility_clears_at_exact_clear_cycle_count:\n");
    plausibility_t p;
    plausibility_init(&p);

    const float bad_a = 50.0f;
    const float bad_b = 50.0f + SENSOR_PLAUSIBILITY_TOLERANCE_PCT + 1.0f;
    const float good_a = 50.0f;
    const float good_b = 50.0f + SENSOR_PLAUSIBILITY_TOLERANCE_PCT - 1.0f;

    int fault = 0;
    for (int i = 0; i < PLAUSIBILITY_FAULT_CYCLES; i++) {
        fault = plausibility_update(&p, bad_a, bad_b, SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                     PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);
    }
    HELM_CHECK_INT_EQ(fault, 1, "precondition: fault must be active before testing clear");

    for (int i = 1; i <= PLAUSIBILITY_CLEAR_CYCLES; i++) {
        fault = plausibility_update(&p, good_a, good_b, SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                     PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);
        if (i < PLAUSIBILITY_CLEAR_CYCLES) {
            char msg[96];
            snprintf(msg, sizeof(msg), "must not clear before cycle %d (currently cycle %d)",
                     PLAUSIBILITY_CLEAR_CYCLES, i);
            HELM_CHECK_INT_EQ(fault, 1, msg);
        }
    }
    HELM_CHECK_INT_EQ(fault, 0, "must clear at exactly PLAUSIBILITY_CLEAR_CYCLES");
}

void test_plausibility_no_false_positives_on_healthy_pair(void) {
    printf("test_plausibility_no_false_positives_on_healthy_pair:\n");
    plausibility_t p;
    plausibility_init(&p);

    /* Simulate a long run of a healthy, well-matched sensor pair with small
     * jitter safely inside tolerance. */
    int fault = 0;
    float a = 20.0f;
    for (int i = 0; i < 10000; i++) {
        /* Deterministic pseudo-jitter, always within tolerance. */
        float jitter = ((i * 37) % 100) / 100.0f; /* 0.0 - 0.99 */
        float b = a + jitter * (SENSOR_PLAUSIBILITY_TOLERANCE_PCT * 0.5f);
        fault = plausibility_update(&p, a, b, SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                     PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);
        HELM_CHECK_INT_EQ(fault, 0, "healthy matched pair must never raise a fault");
        if (fault) break; /* stop spamming failures once we know it's broken */
    }
}

void test_plausibility_intermittent_bad_cycles_do_not_accumulate(void) {
    printf("test_plausibility_intermittent_bad_cycles_do_not_accumulate:\n");
    plausibility_t p;
    plausibility_init(&p);

    const float a = 50.0f;
    const float bad_b = 50.0f + SENSOR_PLAUSIBILITY_TOLERANCE_PCT + 1.0f;
    const float good_b = 50.0f;

    /* Alternate bad/good -- consecutive-bad count should reset each time a
     * good cycle appears, so the fault should never raise. */
    int fault = 0;
    for (int i = 0; i < 100; i++) {
        float b = (i % 2 == 0) ? bad_b : good_b;
        fault = plausibility_update(&p, a, b, SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                     PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);
    }
    HELM_CHECK_INT_EQ(fault, 0, "alternating good/bad cycles must not accumulate into a fault");
}
