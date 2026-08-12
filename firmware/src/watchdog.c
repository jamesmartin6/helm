#include "watchdog.h"

#include "FreeRTOS.h"
#include "task.h"

#include "config.h"
#include "shared_state.h"

/* Independent watchdog task: verifies control_task's heartbeat counter has
 * advanced at least once every WATCHDOG_DEADLINE_MS. A hung or overrunning
 * control loop is a safety issue, not just a bug -- so a missed deadline
 * latches DTC_WATCHDOG_TRIP permanently (diag_task forces fail-safe as long
 * as it's set; only a full reset clears it, matching real ECU behavior). */
void watchdog_task(void *pvParameters) {
    (void)pvParameters;

    /* Startup grace period: give the pipeline (sensor -> control -> actuator
     * -> diag) a few control cycles to actually start producing heartbeats
     * before the first deadline check, so task-creation/first-schedule
     * ordering can't read as a missed deadline. */
    vTaskDelay(pdMS_TO_TICKS(CONTROL_PERIOD_MS * 3));

    uint32_t last_seen_heartbeat = g_control_heartbeat_count;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(WATCHDOG_DEADLINE_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        uint32_t current = g_control_heartbeat_count;
        if (current == last_seen_heartbeat) {
            g_watchdog_tripped = 1u;
        }
        last_seen_heartbeat = current;
    }
}

#ifdef HELM_TEST_WATCHDOG_STALL
void helm_debug_maybe_stall_control_task(void) {
    static int done = 0;
    /* Wait long enough for a few cycles of normal telemetry to be visible
     * first, then stall once for well over WATCHDOG_DEADLINE_MS. */
    if (!done && g_current_cycle == 200u) {
        done = 1;
        vTaskDelay(pdMS_TO_TICKS(WATCHDOG_DEADLINE_MS * 5));
    }
}
#endif
