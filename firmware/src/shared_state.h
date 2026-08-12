#ifndef HELM_SHARED_STATE_H
#define HELM_SHARED_STATE_H

#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"

/*
 * Cross-task shared state for the control pipeline:
 *
 *   sensor_task --(sensor_reading_t)--> control_task --(float duty)--> actuator_task
 *        \--(sensor_reading_t)--> diag_task
 *
 *   control_task --(telemetry_frame_t)--> comms_task --(command_frame_t)--> [sensor_task/control_task via globals below]
 *
 * All queues are depth-1 "mailbox" queues used with xQueueOverwrite/
 * xQueueReceive: each stage only ever cares about the latest value, matching
 * a fixed-rate control loop where stale data should just be replaced, not
 * queued up.
 */

typedef struct {
    uint16_t pedal_raw_a;
    uint16_t pedal_raw_b;
    uint16_t throttle_raw_a;
    uint16_t throttle_raw_b;
    float pedal_pct;
    float throttle_pct;
    int pedal_plausibility_fault;
    int throttle_plausibility_fault;
    int any_sensor_out_of_range; /* true if any of the 4 raw values this cycle is outside spec range */
} sensor_reading_t;

typedef enum {
    FAULT_OVERRIDE_NONE = 0,
    FAULT_OVERRIDE_STUCK,
    FAULT_OVERRIDE_OUT_OF_RANGE,
    FAULT_OVERRIDE_MISMATCH,
} fault_override_kind_t;

typedef struct {
    fault_override_kind_t kind;
    float value_pct; /* meaning depends on kind; see sensor_task.c */
} fault_override_t;

/* Indexed by TARGET_SENSOR_* from protocol.h (0=PEDAL_A,1=PEDAL_B,2=THROTTLE_A,3=THROTTLE_B). */
#define NUM_SENSOR_CHANNELS 4

typedef struct {
    uint16_t active_dtc_mask;
    uint8_t failsafe_active;
} diag_state_t;

extern QueueHandle_t g_sensor_to_control_queue; /* sensor_reading_t */
extern QueueHandle_t g_sensor_to_diag_queue;    /* sensor_reading_t */
extern QueueHandle_t g_control_to_actuator_queue; /* float (duty_pct) */
extern QueueHandle_t g_control_to_comms_queue;  /* telemetry_frame_t (from protocol.h) */

/* Fault injection state written by comms_task on INJECT_FAULT/CLEAR_FAULT,
 * read by sensor_task each cycle when synthesizing raw sensor values. */
extern volatile fault_override_t g_fault_overrides[NUM_SENSOR_CHANNELS];

/* Effective commanded pedal position (0-100%), written by comms_task on
 * SET_PEDAL_OVERRIDE, read by sensor_task. Stands in for a real accelerator
 * pedal sensor input in this fully-simulated SiL setup. */
extern volatile float g_pedal_override_pct;

/* Actual plant throttle-plate angle, written by actuator_task after each
 * plant_sim_step, read by sensor_task to synthesize throttle raw sensors.
 * Single-word float; Cortex-M3 word access is atomic enough for this
 * simulation scale, so no mutex is used here. */
extern volatile float g_plant_throttle_pct;

/* Latest diagnostic state, written by diag_task, read by control_task
 * (failsafe override) and comms_task (telemetry active_dtc_mask). */
extern volatile diag_state_t g_diag_state;

/* Watchdog bookkeeping. control_task increments the heartbeat every cycle;
 * watchdog_task checks it advanced within WATCHDOG_DEADLINE_MS. Trip is
 * latched -- matches real ECU behavior (not self-clearing without reset). */
extern volatile uint32_t g_control_heartbeat_count;
extern volatile uint8_t g_watchdog_tripped;

/* Comms liveness, for DTC_COMMS_TIMEOUT: comms_task stamps the cycle number
 * of the last CRC-valid command frame it received; diag_task compares
 * against the current cycle. */
extern volatile uint32_t g_last_valid_cmd_cycle;
extern volatile uint32_t g_current_cycle;

#endif /* HELM_SHARED_STATE_H */
