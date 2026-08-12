#ifndef HELM_PLANT_SIM_H
#define HELM_PLANT_SIM_H

/*
 * Discrete-time throttle-plate plant model: a first-order-lag DC motor
 * driving a spring-return throttle plate. Stands in for real hardware in
 * SiL mode; swappable for real sensor/actuator I/O in the HiL stretch goal.
 *
 * All angles/positions are throttle-plate percent open, 0.0 (closed/idle)
 * to 100.0 (wide open).
 */

typedef struct {
    float angle_pct;    /* current throttle-plate position */
    float motor_vel_pct_s; /* internal lagged motor velocity state */
} plant_sim_t;

/* Resets the plant to a closed-throttle rest state. */
void plant_sim_init(plant_sim_t *plant);

/*
 * Advances the plant by one control period.
 * duty_pct: commanded actuator duty cycle, [-100.0, 100.0].
 * dt_s: elapsed simulated time in seconds (normally CONTROL_PERIOD_MS / 1000).
 * Returns the resulting throttle-plate angle in percent, clamped [0, 100].
 */
float plant_sim_step(plant_sim_t *plant, float duty_pct, float dt_s);

#endif /* HELM_PLANT_SIM_H */
