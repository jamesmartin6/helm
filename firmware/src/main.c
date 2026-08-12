#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "shared_state.h"
#include "protocol.h"
#include "watchdog.h"
#include "tasks/sensor_task.h"
#include "tasks/control_task.h"
#include "tasks/actuator_task.h"
#include "tasks/diag_task.h"
#include "tasks/comms_task.h"

/* --- Definitions for the extern declarations in shared_state.h --- */
QueueHandle_t g_sensor_to_control_queue;
QueueHandle_t g_sensor_to_diag_queue;
QueueHandle_t g_control_to_actuator_queue;
QueueHandle_t g_control_to_comms_queue;

volatile fault_override_t g_fault_overrides[NUM_SENSOR_CHANNELS];
volatile float g_pedal_override_pct = 0.0f;
volatile float g_plant_throttle_pct = 0.0f;
volatile diag_state_t g_diag_state = { 0, 0 };
volatile uint32_t g_control_heartbeat_count = 0;
volatile uint8_t g_watchdog_tripped = 0;
volatile uint32_t g_last_valid_cmd_cycle = 0;
volatile uint32_t g_current_cycle = 0;

/* Task priorities: watchdog highest (must never be starved), control_task
 * next (hard real-time loop), sensor feeds it, actuator/diag/comms trail. */
#define PRIO_WATCHDOG   (tskIDLE_PRIORITY + 6)
#define PRIO_CONTROL    (tskIDLE_PRIORITY + 5)
#define PRIO_SENSOR     (tskIDLE_PRIORITY + 4)
#define PRIO_ACTUATOR   (tskIDLE_PRIORITY + 4)
#define PRIO_DIAG       (tskIDLE_PRIORITY + 3)
#define PRIO_COMMS      (tskIDLE_PRIORITY + 2)

#define STACK_WATCHDOG  configMINIMAL_STACK_SIZE
#define STACK_CONTROL   (configMINIMAL_STACK_SIZE * 2)
#define STACK_SENSOR    (configMINIMAL_STACK_SIZE * 2)
#define STACK_ACTUATOR  configMINIMAL_STACK_SIZE
#define STACK_DIAG      configMINIMAL_STACK_SIZE
#define STACK_COMMS     (configMINIMAL_STACK_SIZE * 2)

int main(void) {
    g_sensor_to_control_queue = xQueueCreate(1, sizeof(sensor_reading_t));
    g_sensor_to_diag_queue = xQueueCreate(1, sizeof(sensor_reading_t));
    g_control_to_actuator_queue = xQueueCreate(1, sizeof(float));
    g_control_to_comms_queue = xQueueCreate(1, sizeof(telemetry_frame_t));

    xTaskCreate(watchdog_task, "watchdog", STACK_WATCHDOG, NULL, PRIO_WATCHDOG, NULL);
    xTaskCreate(control_task, "control", STACK_CONTROL, NULL, PRIO_CONTROL, NULL);
    xTaskCreate(sensor_task, "sensor", STACK_SENSOR, NULL, PRIO_SENSOR, NULL);
    xTaskCreate(actuator_task, "actuator", STACK_ACTUATOR, NULL, PRIO_ACTUATOR, NULL);
    xTaskCreate(diag_task, "diag", STACK_DIAG, NULL, PRIO_DIAG, NULL);
    xTaskCreate(comms_task, "comms", STACK_COMMS, NULL, PRIO_COMMS, NULL);

    vTaskStartScheduler();

    /* Only reached if there wasn't enough heap to start the scheduler. */
    for (;;) {
    }
    return 0;
}

void vApplicationMallocFailedHook(void) {
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    (void)pcTaskName;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}

void vAssertCalled(const char *pcFileName, unsigned long ulLine) {
    (void)pcFileName;
    (void)ulLine;
    taskDISABLE_INTERRUPTS();
    for (;;) {
    }
}
