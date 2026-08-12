#include "test_framework.h"
#include "../../src/config.h"
#include "../../src/pid.h"
#include "../../src/plant_sim.h"

#include <math.h>
#include <stdio.h>

/* Runs a single step-response test: plant starts at rest (angle=0), setpoint
 * jumps to `step_pct`, PID + plant run in closed loop for a generous window,
 * and we check settling time + overshoot against the spec thresholds. */
static void run_step_response_case(float step_pct) {
    pid_t pid;
    plant_sim_t plant;

    pid_init(&pid, PID_KP, PID_KI, PID_KD, PID_OUTPUT_MIN, PID_OUTPUT_MAX);
    plant_sim_init(&plant);

    const float dt_s = CONTROL_PERIOD_MS / 1000.0f;
    /* Simulate a generous window past the max allowed settling time so we can
     * confirm the response doesn't just touch the band once and drift back out. */
    const int window_ms = SETTLING_TIME_MAX_MS * 4;
    const int total_cycles = window_ms / CONTROL_PERIOD_MS;

    float tolerance_band = SETTLING_TOLERANCE_PCT;
    float max_angle = 0.0f;
    int last_out_of_band_cycle = -1;

    for (int i = 0; i < total_cycles; i++) {
        float duty = pid_update(&pid, step_pct, plant.angle_pct, dt_s);
        float angle = plant_sim_step(&plant, duty, dt_s);

        if (angle > max_angle) {
            max_angle = angle;
        }
        if (fabsf(angle - step_pct) > tolerance_band) {
            last_out_of_band_cycle = i;
        }
    }

    int settling_cycle = last_out_of_band_cycle + 1; /* 0 if never left the band */
    float settling_time_ms = (float)settling_cycle * CONTROL_PERIOD_MS;

    float overshoot_pct = max_angle - step_pct;
    if (overshoot_pct < 0.0f) {
        overshoot_pct = 0.0f;
    }

    printf("  step=%.1f%% -> settling_time=%.0fms max_angle=%.2f%% overshoot=%.2f%%\n",
           step_pct, settling_time_ms, max_angle, overshoot_pct);

    char msg[128];
    snprintf(msg, sizeof(msg), "settling time for step=%.1f%%", step_pct);
    HELM_CHECK_FLOAT_LE(settling_time_ms, (float)SETTLING_TIME_MAX_MS, msg);

    snprintf(msg, sizeof(msg), "overshoot for step=%.1f%%", step_pct);
    HELM_CHECK_FLOAT_LE(overshoot_pct, OVERSHOOT_MAX_PCT, msg);
}

void test_pid_step_response(void) {
    printf("test_pid_step_response:\n");
    /* Five step sizes spanning the pedal range, per Phase 1 definition of done. */
    static const float steps[] = {10.0f, 25.0f, 50.0f, 75.0f, 95.0f};
    for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]); i++) {
        run_step_response_case(steps[i]);
    }
}

void test_pid_output_clamped(void) {
    printf("test_pid_output_clamped:\n");
    pid_t pid;
    pid_init(&pid, PID_KP, PID_KI, PID_KD, PID_OUTPUT_MIN, PID_OUTPUT_MAX);

    /* A huge instantaneous error should never produce an output outside the
     * configured clamp range, even before the plant has had time to respond. */
    float out = pid_update(&pid, 100.0f, 0.0f, CONTROL_PERIOD_MS / 1000.0f);
    HELM_CHECK(out <= PID_OUTPUT_MAX, "PID output must not exceed out_max");
    HELM_CHECK(out >= PID_OUTPUT_MIN, "PID output must not exceed out_min");

    out = pid_update(&pid, -100.0f, 100.0f, CONTROL_PERIOD_MS / 1000.0f);
    HELM_CHECK(out <= PID_OUTPUT_MAX, "PID output must not exceed out_max (negative case)");
    HELM_CHECK(out >= PID_OUTPUT_MIN, "PID output must not exceed out_min (negative case)");
}
