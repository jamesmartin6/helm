"""
Higher-level fault-injection actions and latency-measurement helpers, built
on top of harness.py. Scenarios use these to inject a fault, measure how
long the firmware takes to raise the corresponding DTC and enter fail-safe,
clear the fault, and measure how long it takes to recover -- then assert
those latencies against the budgets in config.py.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import Optional

from harness import HelmHarness
from protocol import (
    FAULT_TYPE_COMMS_DROPOUT,
    FAULT_TYPE_SENSOR_MISMATCH,
    FAULT_TYPE_SENSOR_OUT_OF_RANGE,
    FAULT_TYPE_SENSOR_STUCK,
)


@dataclass
class LatencyMeasurement:
    label: str
    latency_ms: Optional[float]  # None if it never happened within the timeout
    budget_ms: float

    @property
    def within_budget(self) -> bool:
        return self.latency_ms is not None and self.latency_ms <= self.budget_ms

    def __str__(self) -> str:
        if self.latency_ms is None:
            return f"{self.label}: TIMED OUT (budget {self.budget_ms:.0f}ms)"
        verdict = "OK" if self.within_budget else "OVER BUDGET"
        return f"{self.label}: {self.latency_ms:.1f}ms (budget {self.budget_ms:.0f}ms) [{verdict}]"


def inject_stuck_sensor(harness: HelmHarness, target_sensor: int, value_pct: float) -> None:
    harness.inject_fault(FAULT_TYPE_SENSOR_STUCK, target_sensor, value_pct)


def inject_out_of_range(harness: HelmHarness, target_sensor: int) -> None:
    harness.inject_fault(FAULT_TYPE_SENSOR_OUT_OF_RANGE, target_sensor)


def inject_mismatch(harness: HelmHarness, target_sensor: int) -> None:
    harness.inject_fault(FAULT_TYPE_SENSOR_MISMATCH, target_sensor)


def notify_comms_dropout(harness: HelmHarness, target_sensor: int = 0) -> None:
    """Sends the (no-op-on-firmware) COMMS_DROPOUT marker command, for
    symmetry/logging on the harness side. The actual dropout is caused by the
    caller simply not sending anything for a while afterward."""
    harness.inject_fault(FAULT_TYPE_COMMS_DROPOUT, target_sensor)


def clear_fault(harness: HelmHarness, target_sensor: int) -> None:
    harness.clear_fault(target_sensor)


def establish_clean_baseline(harness: HelmHarness, pedal_pct: float = 50.0,
                              timeout_s: float = 3.0) -> bool:
    """Sets the pedal to a normal in-range value and waits for the
    boot-time DTCs (comms-timeout, out-of-range at the zero rail) to clear,
    so a fault scenario starts from a known-healthy state rather than
    measuring against boot transients. Returns True if a clean baseline was
    reached before the timeout."""
    harness.set_pedal(pedal_pct)
    frame, _elapsed = harness.wait_until(
        lambda tel: tel.active_dtc_mask == 0 and not tel.failsafe_active,
        timeout_s=timeout_s,
    )
    return frame is not None


def measure_dtc_raise_latency(harness: HelmHarness, dtc_bit: int, budget_ms: float,
                               timeout_s: float, label: str, keep_alive: bool = True) -> LatencyMeasurement:
    _frame, elapsed_s = harness.wait_until(
        lambda tel: (tel.active_dtc_mask & dtc_bit) != 0, timeout_s=timeout_s, keep_alive=keep_alive
    )
    latency_ms = elapsed_s * 1000.0 if _frame is not None else None
    return LatencyMeasurement(label=label, latency_ms=latency_ms, budget_ms=budget_ms)


def measure_fault_onset_to_dtc_latency(harness: HelmHarness, fault_visible, dtc_bit: int,
                                        budget_ms: float, timeout_s: float, label: str,
                                        keep_alive: bool = True) -> LatencyMeasurement:
    """Like measure_dtc_raise_latency, but the clock starts when the fault
    actually becomes observable in telemetry (`fault_visible(tel) -> bool`),
    not when the INJECT_FAULT command was sent.

    QEMU's CMSDK UART model on this project's dev target only transfers
    roughly one byte per ~10ms of simulated time regardless of firmware
    drain speed, so a 13-byte command frame takes ~130ms to fully arrive --
    that's a QEMU transport artifact, not firmware behavior. The build
    plan's latency budgets (PLAUSIBILITY_FAULT_CYCLES * CONTROL_PERIOD_MS,
    etc.) are about the firmware's own debounce timing once a fault is
    actually present, the way it would be instantaneously on real/HiL
    hardware -- so this measures from fault-onset-in-telemetry, not
    command-sent, to get an apples-to-apples comparison against those
    budgets rather than accidentally benchmarking QEMU's UART emulation
    speed instead of the firmware."""
    onset_frame, _onset_elapsed = harness.wait_until(fault_visible, timeout_s=timeout_s, keep_alive=keep_alive)
    if onset_frame is None:
        return LatencyMeasurement(label=label, latency_ms=None, budget_ms=budget_ms)

    raised_frame, elapsed_s = harness.wait_until(
        lambda tel: (tel.active_dtc_mask & dtc_bit) != 0, timeout_s=timeout_s, keep_alive=keep_alive
    )
    latency_ms = elapsed_s * 1000.0 if raised_frame is not None else None
    return LatencyMeasurement(label=label, latency_ms=latency_ms, budget_ms=budget_ms)


def measure_fault_removed_to_dtc_clear_latency(harness: HelmHarness, fault_gone, dtc_bit: int,
                                                budget_ms: float, timeout_s: float, label: str,
                                                keep_alive: bool = True) -> LatencyMeasurement:
    """Symmetric counterpart to measure_fault_onset_to_dtc_latency: the clock
    starts when the fault actually disappears from telemetry (`fault_gone`),
    not when CLEAR_FAULT was sent -- for the same reason (QEMU's ~130ms
    command transit time would otherwise dominate the measurement)."""
    gone_frame, _elapsed = harness.wait_until(fault_gone, timeout_s=timeout_s, keep_alive=keep_alive)
    if gone_frame is None:
        return LatencyMeasurement(label=label, latency_ms=None, budget_ms=budget_ms)

    cleared_frame, elapsed_s = harness.wait_until(
        lambda tel: (tel.active_dtc_mask & dtc_bit) == 0, timeout_s=timeout_s, keep_alive=keep_alive
    )
    latency_ms = elapsed_s * 1000.0 if cleared_frame is not None else None
    return LatencyMeasurement(label=label, latency_ms=latency_ms, budget_ms=budget_ms)


def measure_dtc_clear_latency(harness: HelmHarness, dtc_bit: int, budget_ms: float,
                               timeout_s: float, label: str) -> LatencyMeasurement:
    _frame, elapsed_s = harness.wait_until(
        lambda tel: (tel.active_dtc_mask & dtc_bit) == 0, timeout_s=timeout_s
    )
    latency_ms = elapsed_s * 1000.0 if _frame is not None else None
    return LatencyMeasurement(label=label, latency_ms=latency_ms, budget_ms=budget_ms)


def measure_failsafe_transition(harness: HelmHarness, active: bool, budget_ms: float,
                                 timeout_s: float, label: str, keep_alive: bool = True) -> LatencyMeasurement:
    _frame, elapsed_s = harness.wait_until(
        lambda tel: bool(tel.failsafe_active) == active, timeout_s=timeout_s, keep_alive=keep_alive
    )
    latency_ms = elapsed_s * 1000.0 if _frame is not None else None
    return LatencyMeasurement(label=label, latency_ms=latency_ms, budget_ms=budget_ms)
