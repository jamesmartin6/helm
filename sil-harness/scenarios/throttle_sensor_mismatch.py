"""
Scenario: throttle sensor B is forced to disagree with throttle sensor A
beyond tolerance (a wiring-fault-style mismatch, as opposed to a frozen
value). Asserts DTC_THROTTLE_PLAUSIBILITY raises/clears within spec latency
and fail-safe engages.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config  # noqa: E402
from fault_injection import (  # noqa: E402
    establish_clean_baseline,
    inject_mismatch,
    clear_fault,
    measure_fault_onset_to_dtc_latency,
    measure_fault_removed_to_dtc_clear_latency,
    measure_failsafe_transition,
)
from protocol import TARGET_SENSOR_THROTTLE_B  # noqa: E402

MISMATCH_RAW_THRESHOLD = (config.SENSOR_PLAUSIBILITY_TOLERANCE_PCT / 100.0) * config.SENSOR_RAW_FULL_SCALE


def test_throttle_sensor_mismatch(firmware, latency_report):
    h = firmware
    assert establish_clean_baseline(h, pedal_pct=35.0), \
        "firmware never reached a clean (no-DTC) baseline before fault injection"

    inject_mismatch(h, TARGET_SENSOR_THROTTLE_B)

    raise_m = measure_fault_onset_to_dtc_latency(
        h,
        fault_visible=lambda tel: abs(tel.throttle_raw_b - tel.throttle_raw_a) > MISMATCH_RAW_THRESHOLD,
        dtc_bit=config.DTC_THROTTLE_PLAUSIBILITY,
        budget_ms=config.PLAUSIBILITY_FAULT_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=2.0, label="throttle_sensor_mismatch: fault -> DTC_THROTTLE_PLAUSIBILITY raised",
    )
    failsafe_m = measure_failsafe_transition(
        h, active=True, budget_ms=config.FAILSAFE_TRANSITION_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=1.0, label="throttle_sensor_mismatch: DTC -> failsafe_active",
    )

    clear_fault(h, TARGET_SENSOR_THROTTLE_B)

    clear_m = measure_fault_removed_to_dtc_clear_latency(
        h,
        fault_gone=lambda tel: abs(tel.throttle_raw_b - tel.throttle_raw_a) < 10,
        dtc_bit=config.DTC_THROTTLE_PLAUSIBILITY,
        budget_ms=config.PLAUSIBILITY_CLEAR_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=2.0, label="throttle_sensor_mismatch: fault cleared -> DTC clears",
    )

    latency_report.extend([raise_m, failsafe_m, clear_m])
    print(f"\n{raise_m}\n{failsafe_m}\n{clear_m}")

    assert raise_m.within_budget, str(raise_m)
    assert failsafe_m.within_budget, str(failsafe_m)
    assert clear_m.within_budget, str(clear_m)
