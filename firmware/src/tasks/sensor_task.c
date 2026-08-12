#include "sensor_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "../config.h"
#include "../shared_state.h"
#include "../plausibility.h"
#include "../protocol.h" /* TARGET_SENSOR_* channel indices */

/* Small fixed offset the "B" sensor of each healthy pair carries, simulating
 * ordinary real-world sensor tolerance -- comfortably inside
 * SENSOR_PLAUSIBILITY_TOLERANCE_PCT so it never trips the plausibility check
 * on its own. */
#define HEALTHY_SENSOR_B_OFFSET_RAW 3

/* Offset applied to a channel under FAULT_OVERRIDE_MISMATCH, large enough to
 * clear SENSOR_PLAUSIBILITY_TOLERANCE_PCT (5% of 4095 =~ 205 counts) while
 * usually staying inside the valid sensor range. */
#define MISMATCH_FAULT_OFFSET_RAW 400

static uint16_t pct_to_raw(float pct) {
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    long v = (long)(pct / 100.0f * (float)SENSOR_RAW_FULL_SCALE + 0.5f);
    if (v < 0) v = 0;
    if (v > 65535) v = 65535;
    return (uint16_t)v;
}

static float raw_to_pct(uint16_t raw) {
    float pct = (float)raw / (float)SENSOR_RAW_FULL_SCALE * 100.0f;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    return pct;
}

/* Applies any active fault override for this channel on top of its normal
 * synthesized raw value. */
static uint16_t apply_fault_override(int channel, uint16_t normal_raw) {
    fault_override_t ov = g_fault_overrides[channel];
    switch (ov.kind) {
    case FAULT_OVERRIDE_STUCK:
        return pct_to_raw(ov.value_pct);
    case FAULT_OVERRIDE_OUT_OF_RANGE:
        return (uint16_t)(SENSOR_RAW_MAX + 50);
    case FAULT_OVERRIDE_MISMATCH: {
        long v = (long)normal_raw + MISMATCH_FAULT_OFFSET_RAW;
        if (v > SENSOR_RAW_MAX) {
            v = (long)normal_raw - MISMATCH_FAULT_OFFSET_RAW;
        }
        if (v < 0) v = 0;
        return (uint16_t)v;
    }
    case FAULT_OVERRIDE_NONE:
    default:
        return normal_raw;
    }
}

static plausibility_t s_pedal_plausibility;
static plausibility_t s_throttle_plausibility;

void sensor_task(void *pvParameters) {
    (void)pvParameters;

    plausibility_init(&s_pedal_plausibility);
    plausibility_init(&s_throttle_plausibility);

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        float pedal_source_pct = g_pedal_override_pct;
        float throttle_source_pct = g_plant_throttle_pct;

        uint16_t pedal_a = pct_to_raw(pedal_source_pct);
        uint16_t pedal_b = (uint16_t)(pedal_a + HEALTHY_SENSOR_B_OFFSET_RAW);
        uint16_t throttle_a = pct_to_raw(throttle_source_pct);
        uint16_t throttle_b = (uint16_t)(throttle_a + HEALTHY_SENSOR_B_OFFSET_RAW);

        pedal_a = apply_fault_override(TARGET_SENSOR_PEDAL_A, pedal_a);
        pedal_b = apply_fault_override(TARGET_SENSOR_PEDAL_B, pedal_b);
        throttle_a = apply_fault_override(TARGET_SENSOR_THROTTLE_A, throttle_a);
        throttle_b = apply_fault_override(TARGET_SENSOR_THROTTLE_B, throttle_b);

        int any_oor = (pedal_a < SENSOR_RAW_MIN || pedal_a > SENSOR_RAW_MAX) ||
                      (pedal_b < SENSOR_RAW_MIN || pedal_b > SENSOR_RAW_MAX) ||
                      (throttle_a < SENSOR_RAW_MIN || throttle_a > SENSOR_RAW_MAX) ||
                      (throttle_b < SENSOR_RAW_MIN || throttle_b > SENSOR_RAW_MAX);

        float pedal_a_pct = raw_to_pct(pedal_a);
        float pedal_b_pct = raw_to_pct(pedal_b);
        float throttle_a_pct = raw_to_pct(throttle_a);
        float throttle_b_pct = raw_to_pct(throttle_b);

        int pedal_fault = plausibility_update(&s_pedal_plausibility, pedal_a_pct, pedal_b_pct,
                                               SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                               PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);
        int throttle_fault = plausibility_update(&s_throttle_plausibility, throttle_a_pct, throttle_b_pct,
                                                  SENSOR_PLAUSIBILITY_TOLERANCE_PCT,
                                                  PLAUSIBILITY_FAULT_CYCLES, PLAUSIBILITY_CLEAR_CYCLES);

        sensor_reading_t reading;
        reading.pedal_raw_a = pedal_a;
        reading.pedal_raw_b = pedal_b;
        reading.throttle_raw_a = throttle_a;
        reading.throttle_raw_b = throttle_b;
        reading.pedal_pct = (pedal_a_pct + pedal_b_pct) * 0.5f;
        reading.throttle_pct = (throttle_a_pct + throttle_b_pct) * 0.5f;
        reading.pedal_plausibility_fault = pedal_fault;
        reading.throttle_plausibility_fault = throttle_fault;
        reading.any_sensor_out_of_range = any_oor;

        xQueueOverwrite(g_sensor_to_control_queue, &reading);
        xQueueOverwrite(g_sensor_to_diag_queue, &reading);
    }
}
