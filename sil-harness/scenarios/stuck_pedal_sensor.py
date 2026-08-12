"""
Scenario: pedal sensor A freezes at a fixed value while sensor B keeps
tracking the real commanded pedal position. Asserts the firmware raises
DTC_PEDAL_PLAUSIBILITY within PLAUSIBILITY_FAULT_CYCLES, enters fail-safe
within one further control cycle, and clears the DTC within
PLAUSIBILITY_CLEAR_CYCLES once the fault is removed.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config  # noqa: E402
from fault_injection import (  # noqa: E402
    LatencyMeasurement,
    establish_clean_baseline,
    inject_stuck_sensor,
    clear_fault,
    measure_fault_onset_to_dtc_latency,
    measure_fault_removed_to_dtc_clear_latency,
    measure_failsafe_transition,
)
from protocol import TARGET_SENSOR_PEDAL_A  # noqa: E402

STUCK_VALUE_PCT = 90.0
STUCK_VALUE_RAW = round(STUCK_VALUE_PCT / 100.0 * config.SENSOR_RAW_FULL_SCALE)


def test_stuck_pedal_sensor(firmware, latency_report):
    h = firmware
    assert establish_clean_baseline(h, pedal_pct=50.0), \
        "firmware never reached a clean (no-DTC) baseline before fault injection"

    # Freeze pedal A far enough from the commanded 50% (tracked by pedal B)
    # to clear SENSOR_PLAUSIBILITY_TOLERANCE_PCT.
    inject_stuck_sensor(h, TARGET_SENSOR_PEDAL_A, value_pct=STUCK_VALUE_PCT)

    raise_m = measure_fault_onset_to_dtc_latency(
        h,
        fault_visible=lambda tel: abs(tel.pedal_raw_a - STUCK_VALUE_RAW) < 5,
        dtc_bit=config.DTC_PEDAL_PLAUSIBILITY,
        budget_ms=config.PLAUSIBILITY_FAULT_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=2.0, label="stuck_pedal_sensor: fault -> DTC_PEDAL_PLAUSIBILITY raised",
    )
    failsafe_m = measure_failsafe_transition(
        h, active=True, budget_ms=config.FAILSAFE_TRANSITION_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=1.0, label="stuck_pedal_sensor: DTC -> failsafe_active",
    )

    clear_fault(h, TARGET_SENSOR_PEDAL_A)

    clear_m = measure_fault_removed_to_dtc_clear_latency(
        h,
        fault_gone=lambda tel: abs(tel.pedal_raw_a - tel.pedal_raw_b) < 10,
        dtc_bit=config.DTC_PEDAL_PLAUSIBILITY,
        budget_ms=config.PLAUSIBILITY_CLEAR_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=2.0, label="stuck_pedal_sensor: fault cleared -> DTC clears",
    )

    latency_report.extend([raise_m, failsafe_m, clear_m])

    print(f"\n{raise_m}\n{failsafe_m}\n{clear_m}")

    assert raise_m.within_budget, str(raise_m)
    assert failsafe_m.within_budget, str(failsafe_m)
    assert clear_m.within_budget, str(clear_m)
