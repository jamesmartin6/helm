#ifndef HELM_PLAUSIBILITY_H
#define HELM_PLAUSIBILITY_H

/*
 * Dual-sensor plausibility check state machine, reusable for any redundant
 * sensor pair (pedal A/B, throttle A/B). Debounces both the raise and the
 * clear so a single noisy sample doesn't flip the fault state:
 *   - raised after PLAUSIBILITY_FAULT_CYCLES consecutive out-of-tolerance cycles
 *   - cleared after PLAUSIBILITY_CLEAR_CYCLES consecutive back-in-tolerance cycles
 */

typedef struct {
    int fault_active;
    int consecutive_bad;
    int consecutive_good;
} plausibility_t;

void plausibility_init(plausibility_t *p);

/*
 * Feeds one cycle's worth of paired sensor readings (already resolved to
 * percent, 0-100) through the debounce state machine.
 * tolerance_pct: max allowed |value_a - value_b| before a cycle counts as bad.
 * fault_cycles: consecutive bad cycles required to raise the fault.
 * clear_cycles: consecutive good cycles required to clear an active fault.
 * Returns the fault state after this update (1 = active, 0 = clear).
 */
int plausibility_update(plausibility_t *p, float value_a, float value_b,
                         float tolerance_pct, int fault_cycles, int clear_cycles);

#endif /* HELM_PLAUSIBILITY_H */
