#include "control_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "../config.h"
#include "../shared_state.h"
#include "../pid.h"
#include "../protocol.h"

#ifdef HELM_TEST_WATCHDOG_STALL
#include "../watchdog.h" /* only pulled in for the debug stall-injection build */
#endif

void control_task(void *pvParameters) {
    (void)pvParameters;

    pid_t pid;
    pid_init(&pid, PID_KP, PID_KI, PID_KD, PID_OUTPUT_MIN, PID_OUTPUT_MAX);

    const float dt_s = CONTROL_PERIOD_MS / 1000.0f;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        sensor_reading_t reading;
        if (xQueueReceive(g_sensor_to_control_queue, &reading, 0) != pdTRUE) {
            /* No fresh sensor data yet (startup race) -- skip this tick. */
            continue;
        }

        diag_state_t diag = g_diag_state; /* snapshot; single-writer/reader, plain struct copy */

        float setpoint = diag.failsafe_active ? FAILSAFE_THROTTLE_PCT : reading.pedal_pct;
        float duty = pid_update(&pid, setpoint, reading.throttle_pct, dt_s);

#ifdef HELM_TEST_WATCHDOG_STALL
        /* Debug-only build: intentionally blow the control loop deadline
         * once, to prove the watchdog forces fail-safe on a stalled task.
         * Never compiled into the default firmware build. */
        helm_debug_maybe_stall_control_task();
#endif

        g_current_cycle++;
        g_control_heartbeat_count++;

        xQueueOverwrite(g_control_to_actuator_queue, &duty);

        telemetry_frame_t frame;
        frame.cycle_id = g_current_cycle;
        frame.timestamp_ms = (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
        frame.pedal_raw_a = reading.pedal_raw_a;
        frame.pedal_raw_b = reading.pedal_raw_b;
        frame.throttle_raw_a = reading.throttle_raw_a;
        frame.throttle_raw_b = reading.throttle_raw_b;
        frame.pedal_pct = reading.pedal_pct;
        frame.throttle_pct = reading.throttle_pct;
        frame.control_error = setpoint - reading.throttle_pct;
        frame.actuator_duty_pct = duty;
        frame.active_dtc_mask = diag.active_dtc_mask;
        frame.failsafe_active = diag.failsafe_active;

        xQueueOverwrite(g_control_to_comms_queue, &frame);
    }
}
