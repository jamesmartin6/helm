"""
Scenario: the harness goes silent (no command frames at all, simulating a
lost serial/CAN link) for longer than COMMS_TIMEOUT_CYCLES control cycles.
Asserts DTC_COMMS_TIMEOUT raises within budget and fail-safe engages, then
that resuming communication clears the DTC immediately (per the DTC table,
comms-timeout clears on the very next valid frame, unlike the debounced
plausibility/OOR clears).
"""

import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config  # noqa: E402
from fault_injection import (  # noqa: E402
    establish_clean_baseline,
    measure_dtc_raise_latency,
    measure_dtc_clear_latency,
    measure_failsafe_transition,
)

# Comms-timeout clear has no multi-cycle debounce in the firmware (DTC
# table: "Cleared when: A valid frame received") -- but unlike the other
# scenarios' clear measurements, there's no telemetry-visible "fault
# removed" moment to measure from other than the resuming command itself
# actually arriving, so this budget has to include one command's QEMU UART
# transit time (~130-150ms observed for a 13-byte frame) on top of the
# firmware's effectively-instant clear.
COMMS_CLEAR_LATENCY_MS = 150 + config.MEASUREMENT_MARGIN_MS


def test_comm_timeout_recovery(firmware, latency_report):
    h = firmware
    assert establish_clean_baseline(h, pedal_pct=25.0), \
        "firmware never reached a clean (no-DTC) baseline before the dropout"

    # NOTE: deliberately not sending notify_comms_dropout() (or anything
    # else) here before going silent -- any command sent now would itself
    # update g_last_valid_cmd_cycle once it finally transits (QEMU's UART
    # model takes ~130ms per 13-byte frame), which would reset the very
    # timeout counter this scenario is trying to measure. The "dropout" is
    # simply the harness going silent starting now.

    # establish_clean_baseline's own keep-alive sends can leave a couple of
    # commands still in flight (each takes ~130ms to transit), which would
    # keep resetting g_last_valid_cmd_cycle for a while even after we stop
    # sending. Settle quietly (no new sends) until that backlog has fully
    # drained, so the silence measurement below starts from a truly
    # quiescent point rather than being reset out from under it.
    time.sleep(0.5)
    h.poll()

    # Go silent: no keep-alive commands at all while we wait for the timeout.
    raise_m = measure_dtc_raise_latency(
        h, config.DTC_COMMS_TIMEOUT,
        budget_ms=config.COMMS_TIMEOUT_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=2.0, label="comm_timeout_recovery: silence -> DTC_COMMS_TIMEOUT raised",
        keep_alive=False,
    )
    failsafe_m = measure_failsafe_transition(
        h, active=True, budget_ms=config.FAILSAFE_TRANSITION_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=1.0, label="comm_timeout_recovery: DTC -> failsafe_active",
        keep_alive=False,
    )

    # Resume communication -- the very next valid frame should clear the DTC.
    # (FAULT_TYPE_COMMS_DROPOUT itself is a documented no-op on the firmware
    # side -- not sent here since queuing it ahead of the ping below would
    # just add another ~130ms of QEMU UART transit time before the ping that
    # actually matters for this measurement gets through.)
    h.ping()
    clear_m = measure_dtc_clear_latency(
        h, config.DTC_COMMS_TIMEOUT,
        budget_ms=COMMS_CLEAR_LATENCY_MS,
        timeout_s=1.0, label="comm_timeout_recovery: comms resumed -> DTC clears",
    )

    latency_report.extend([raise_m, failsafe_m, clear_m])
    print(f"\n{raise_m}\n{failsafe_m}\n{clear_m}")

    assert raise_m.within_budget, str(raise_m)
    assert failsafe_m.within_budget, str(failsafe_m)
    assert clear_m.within_budget, str(clear_m)
