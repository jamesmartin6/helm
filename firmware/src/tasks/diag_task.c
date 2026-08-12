#include "diag_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "../config.h"
#include "../shared_state.h"
#include "../protocol.h"

/* DTC_SENSOR_OUT_OF_RANGE has an asymmetric debounce vs. the plausibility
 * checks: raised immediately on the first out-of-range sample, cleared only
 * after PLAUSIBILITY_CLEAR_CYCLES consecutive in-range samples (see DTC
 * table in docs/build-plan.md). Owned here rather than in plausibility.c
 * since that module's raise/clear debounce is symmetric. */
static int s_oor_active = 0;
static int s_oor_good_streak = 0;

/* DTC_ACTUATOR_FAULT (0x0020) is explicitly a HiL-only stretch-goal DTC per
 * the build plan ("only meaningful once real-HiL actuator feedback
 * exists") -- there is no independent actuator feedback sensor in SiL mode
 * to compare against, so this bit is intentionally always 0 here. */

void diag_task(void *pvParameters) {
    (void)pvParameters;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        sensor_reading_t reading;
        if (xQueueReceive(g_sensor_to_diag_queue, &reading, 0) != pdTRUE) {
            continue;
        }

        if (reading.any_sensor_out_of_range) {
            s_oor_active = 1;
            s_oor_good_streak = 0;
        } else {
            s_oor_good_streak++;
            if (s_oor_good_streak >= PLAUSIBILITY_CLEAR_CYCLES) {
                s_oor_active = 0;
            }
        }

        uint32_t cycle_now = g_current_cycle;
        uint32_t cycles_since_valid_cmd = cycle_now - g_last_valid_cmd_cycle;
        int comms_timeout_active = (cycles_since_valid_cmd >= (uint32_t)COMMS_TIMEOUT_CYCLES);

        uint16_t mask = 0;
        if (reading.pedal_plausibility_fault) mask |= DTC_PEDAL_PLAUSIBILITY;
        if (reading.throttle_plausibility_fault) mask |= DTC_THROTTLE_PLAUSIBILITY;
        if (s_oor_active) mask |= DTC_SENSOR_OUT_OF_RANGE;
        if (comms_timeout_active) mask |= DTC_COMMS_TIMEOUT;
        if (g_watchdog_tripped) mask |= DTC_WATCHDOG_TRIP;

        diag_state_t new_state;
        new_state.active_dtc_mask = mask;
        /* Any active DTC forces fail-safe -- matches the Core Concept
         * Primer: fall back to limp-home "the instant something doesn't
         * add up," not just on a watchdog trip. */
        new_state.failsafe_active = (mask != 0) ? 1u : 0u;

        g_diag_state = new_state;
    }
}
