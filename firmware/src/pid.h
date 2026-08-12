#ifndef HELM_PID_H
#define HELM_PID_H

/*
 * PID controller with clamped-integrator anti-windup and output clamping.
 * Used to close the loop between commanded pedal position and actual
 * throttle-plate position.
 */

typedef struct {
    float kp, ki, kd;
    float out_min, out_max;
    float integral;
    float prev_error;
    int has_prev_error; /* 0 until first update() call, so kd doesn't kick on cycle 1 */
} pid_t;

/* Initializes the controller with the given gains and output clamp range. */
void pid_init(pid_t *pid, float kp, float ki, float kd, float out_min, float out_max);

/* Resets internal state (integral, derivative history) without changing gains. */
void pid_reset(pid_t *pid);

/*
 * Runs one control-loop update.
 * setpoint, measurement: same units (throttle-plate percent).
 * dt_s: elapsed time in seconds since the previous update.
 * Returns the clamped controller output.
 */
float pid_update(pid_t *pid, float setpoint, float measurement, float dt_s);

#endif /* HELM_PID_H */
