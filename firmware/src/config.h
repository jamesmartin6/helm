#ifndef HELM_CONFIG_H
#define HELM_CONFIG_H

/*
 * Tunable constants for the control loop, diagnostics, and comms.
 * Mirrored in sil-harness/config.py — keep both in sync.
 */

/* --- Control loop timing --- */
#define CONTROL_PERIOD_MS      10      /* 100 Hz control loop tick */
#define TELEMETRY_RATE_HZ      100     /* one telemetry frame per control cycle */

/* --- PID gains (starting values, tuned against Phase 1 settling-time tests) --- */
#define PID_KP                  8.0f
#define PID_KI                  3.0f
#define PID_KD                  0.1f
#define PID_OUTPUT_MIN        (-100.0f)
#define PID_OUTPUT_MAX          100.0f

/* --- Step-response acceptance criteria --- */
#define SETTLING_TIME_MAX_MS    300
#define SETTLING_TOLERANCE_PCT    2.0f
#define OVERSHOOT_MAX_PCT        10.0f

/* --- Dual-sensor plausibility check --- */
#define SENSOR_PLAUSIBILITY_TOLERANCE_PCT   5.0f
#define PLAUSIBILITY_FAULT_CYCLES           5
#define PLAUSIBILITY_CLEAR_CYCLES           20

/* --- Fail-safe --- */
#define FAILSAFE_THROTTLE_PCT     8.0f  /* limp-home angle: idle-plus, not fully closed */

/* --- Comms --- */
#define COMMS_TIMEOUT_CYCLES      50

/* --- Watchdog --- */
#define WATCHDOG_DEADLINE_MS      15

/* --- Raw sensor range (simulated 12-bit ADC, 0-4095) --- */
#define SENSOR_RAW_MIN            50
#define SENSOR_RAW_MAX          4045
#define SENSOR_RAW_FULL_SCALE   4095

/* --- Plant model (first-order motor + spring-return throttle plate) --- */
/* Motor electrical+mechanical time constant, in seconds. Smaller = faster motor.
 * A small automotive throttle-body actuator is a fast, low-inertia servo --
 * full-range sweep well under 300ms is realistic (real ETC units are ~100-150ms). */
#define PLANT_MOTOR_TIME_CONSTANT_S   0.03f
/* Spring-return rate toward idle (0%) when no duty is applied, %/s of plate angle. */
#define PLANT_SPRING_RETURN_RATE_PCT_S 60.0f
/* Motor gain: plate angle %/s per 1% commanded duty, before time-constant lag. */
#define PLANT_MOTOR_GAIN               6.0f

#endif /* HELM_CONFIG_H */
