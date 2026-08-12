#include "pid.h"

void pid_init(pid_t *pid, float kp, float ki, float kd, float out_min, float out_max) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->out_min = out_min;
    pid->out_max = out_max;
    pid_reset(pid);
}

void pid_reset(pid_t *pid) {
    pid->integral = 0.0f;
    pid->prev_error = 0.0f;
    pid->has_prev_error = 0;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float pid_update(pid_t *pid, float setpoint, float measurement, float dt_s) {
    float error = setpoint - measurement;

    float derivative = 0.0f;
    if (pid->has_prev_error && dt_s > 0.0f) {
        derivative = (error - pid->prev_error) / dt_s;
    }

    /* Tentative integral, used only to check whether accumulating would push
     * the output further into saturation (clamped-integrator anti-windup). */
    float tentative_integral = pid->integral + error * dt_s;
    float unclamped_out = pid->kp * error + pid->ki * tentative_integral + pid->kd * derivative;

    if (unclamped_out > pid->out_max) {
        /* Output wants to saturate high: only keep accumulating if the error
         * itself is negative (would pull the output back down). */
        if (error < 0.0f) {
            pid->integral = tentative_integral;
        }
    } else if (unclamped_out < pid->out_min) {
        if (error > 0.0f) {
            pid->integral = tentative_integral;
        }
    } else {
        pid->integral = tentative_integral;
    }

    float out = pid->kp * error + pid->ki * pid->integral + pid->kd * derivative;
    out = clampf(out, pid->out_min, pid->out_max);

    pid->prev_error = error;
    pid->has_prev_error = 1;

    return out;
}
