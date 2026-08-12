#include "actuator_task.h"

#include "FreeRTOS.h"
#include "task.h"

#include "../config.h"
#include "../shared_state.h"
#include "../plant_sim.h"

void actuator_task(void *pvParameters) {
    (void)pvParameters;

    plant_sim_t plant;
    plant_sim_init(&plant);
    g_plant_throttle_pct = plant.angle_pct;

    const float dt_s = CONTROL_PERIOD_MS / 1000.0f;

    TickType_t last_wake = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(CONTROL_PERIOD_MS);

    for (;;) {
        vTaskDelayUntil(&last_wake, period);

        float duty = 0.0f;
        if (xQueueReceive(g_control_to_actuator_queue, &duty, 0) != pdTRUE) {
            duty = 0.0f; /* no command yet: let the spring pull toward idle */
        }

        float angle = plant_sim_step(&plant, duty, dt_s);
        g_plant_throttle_pct = angle;
    }
}
