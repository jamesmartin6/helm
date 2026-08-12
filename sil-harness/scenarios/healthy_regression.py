"""
Regression scenario: no faults injected at all. Drives a randomized
driving-style pedal profile for an extended period and asserts zero
false-positive DTCs the entire time -- the counterpart to the fault
scenarios, proving the diagnostic layer doesn't cry wolf during ordinary
operation.

Duration defaults to the full 5 minutes the build plan's Definition of Done
calls for. Override with HELM_HEALTHY_REGRESSION_DURATION_S for a faster
smoke-test pass during development (e.g. 20) -- the default is what runs in
the full/overnight suite.
"""

import os
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))

from harness import profile_random_driving  # noqa: E402

DURATION_S = float(os.environ.get("HELM_HEALTHY_REGRESSION_DURATION_S", "300"))


def test_healthy_regression_zero_false_positive_dtcs(firmware):
    h = firmware
    pedal_fn = profile_random_driving(seed=12345, segment_duration_s=3.0)

    frames = h.run_for(DURATION_S, pedal_fn=pedal_fn)

    assert len(frames) > 0, "no telemetry observed during the regression run"

    # Boot-time transients (comms-timeout / OOR-at-zero before the first
    # pedal command lands) are expected and don't count -- only look at
    # frames from after the pedal profile has clearly taken effect.
    settled = [f for f in frames if f.cycle_id > frames[0].cycle_id + 100]
    assert len(settled) > 0, "not enough frames captured after the startup transient"

    dtc_frames = [f for f in settled if f.active_dtc_mask != 0]
    failsafe_frames = [f for f in settled if f.failsafe_active]

    if dtc_frames:
        first = dtc_frames[0]
        print(f"\nFIRST false-positive DTC at cycle={first.cycle_id}: "
              f"mask=0x{first.active_dtc_mask:04x} pedal={first.pedal_pct:.1f}% "
              f"throttle={first.throttle_pct:.1f}%")

    print(f"\nhealthy_regression: {len(settled)} settled frames over {DURATION_S:.0f}s, "
          f"{len(dtc_frames)} with a nonzero DTC mask, {len(failsafe_frames)} in fail-safe")

    assert len(dtc_frames) == 0, \
        f"{len(dtc_frames)}/{len(settled)} frames had a false-positive DTC during healthy operation"
    assert len(failsafe_frames) == 0, \
        f"{len(failsafe_frames)}/{len(settled)} frames were in fail-safe during healthy operation"
