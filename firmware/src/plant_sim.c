#include "plant_sim.h"
#include "config.h"

void plant_sim_init(plant_sim_t *plant) {
    plant->angle_pct = 0.0f;
    plant->motor_vel_pct_s = 0.0f;
}

static float clampf(float v, float lo, float hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

float plant_sim_step(plant_sim_t *plant, float duty_pct, float dt_s) {
    duty_pct = clampf(duty_pct, -100.0f, 100.0f);

    /* Motor velocity target from commanded duty, reached with first-order lag
     * (electrical + mechanical time constant of the actuator). */
    float target_vel_pct_s = PLANT_MOTOR_GAIN * duty_pct;
    float alpha = dt_s / (PLANT_MOTOR_TIME_CONSTANT_S + dt_s);
    plant->motor_vel_pct_s += alpha * (target_vel_pct_s - plant->motor_vel_pct_s);

    /* Spring-return force pulls the plate toward idle (0%), proportional to
     * displacement from idle -- a real throttle body's return spring. */
    float spring_vel_pct_s = -(PLANT_SPRING_RETURN_RATE_PCT_S / 100.0f) * plant->angle_pct;

    float total_vel_pct_s = plant->motor_vel_pct_s + spring_vel_pct_s;

    plant->angle_pct += total_vel_pct_s * dt_s;
    plant->angle_pct = clampf(plant->angle_pct, 0.0f, 100.0f);

    return plant->angle_pct;
}
