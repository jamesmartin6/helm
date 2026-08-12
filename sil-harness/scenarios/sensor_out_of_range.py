"""
Scenario: a throttle sensor reading goes outside the valid raw ADC range
(simulating a rail fault). Unlike the plausibility checks, this DTC raises
immediately (no multi-cycle debounce) but still clears with the standard
PLAUSIBILITY_CLEAR_CYCLES debounce once the fault is removed.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

import config  # noqa: E402
from fault_injection import (  # noqa: E402
    establish_clean_baseline,
    inject_out_of_range,
    clear_fault,
    measure_fault_onset_to_dtc_latency,
    measure_fault_removed_to_dtc_clear_latency,
    measure_failsafe_transition,
)
from protocol import TARGET_SENSOR_THROTTLE_A  # noqa: E402


def test_sensor_out_of_range(firmware, latency_report):
    h = firmware
    assert establish_clean_baseline(h, pedal_pct=20.0), \
        "firmware never reached a clean (no-DTC) baseline before fault injection"

    inject_out_of_range(h, TARGET_SENSOR_THROTTLE_A)

    raise_m = measure_fault_onset_to_dtc_latency(
        h,
        fault_visible=lambda tel: tel.throttle_raw_a > config.SENSOR_RAW_MAX or tel.throttle_raw_a < config.SENSOR_RAW_MIN,
        dtc_bit=config.DTC_SENSOR_OUT_OF_RANGE,
        budget_ms=config.OUT_OF_RANGE_RAISE_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=1.0, label="sensor_out_of_range: fault -> DTC_SENSOR_OUT_OF_RANGE raised",
    )
    failsafe_m = measure_failsafe_transition(
        h, active=True, budget_ms=config.FAILSAFE_TRANSITION_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=1.0, label="sensor_out_of_range: DTC -> failsafe_active",
    )

    clear_fault(h, TARGET_SENSOR_THROTTLE_A)

    clear_m = measure_fault_removed_to_dtc_clear_latency(
        h,
        fault_gone=lambda tel: config.SENSOR_RAW_MIN <= tel.throttle_raw_a <= config.SENSOR_RAW_MAX,
        dtc_bit=config.DTC_SENSOR_OUT_OF_RANGE,
        budget_ms=config.OUT_OF_RANGE_CLEAR_LATENCY_MS + config.MEASUREMENT_MARGIN_MS,
        timeout_s=2.0, label="sensor_out_of_range: fault cleared -> DTC clears",
    )

    latency_report.extend([raise_m, failsafe_m, clear_m])
    print(f"\n{raise_m}\n{failsafe_m}\n{clear_m}")

    assert raise_m.within_budget, str(raise_m)
    assert failsafe_m.within_budget, str(failsafe_m)
    assert clear_m.within_budget, str(clear_m)
