#ifndef HELM_WATCHDOG_H
#define HELM_WATCHDOG_H

void watchdog_task(void *pvParameters);

#ifdef HELM_TEST_WATCHDOG_STALL
/* Debug-only: intentionally stalls control_task once, well past
 * WATCHDOG_DEADLINE_MS, to prove the watchdog trips and forces fail-safe.
 * Only linked into the `helm_watchdog_test` firmware target, never the
 * default build. */
void helm_debug_maybe_stall_control_task(void);
#endif

#endif /* HELM_WATCHDOG_H */
