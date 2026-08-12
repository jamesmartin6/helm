"""
Drives the QEMU-hosted firmware in closed loop over its (TCP-bridged) UART:
decodes the live telemetry stream, sends pedal-position / fault-injection
commands, and provides polling helpers scenarios use to assert on
fault-to-DTC and DTC-to-failsafe latency.
"""

from __future__ import annotations

import math
import random
import socket
import time
from dataclasses import dataclass, field
from typing import Callable, Optional

from protocol import (
    ACK_STATUS_OK,
    CMD_CLEAR_FAULT,
    CMD_INJECT_FAULT,
    CMD_PING,
    CMD_SET_PEDAL_OVERRIDE,
    FRAME_TYPE_ACK,
    FRAME_TYPE_TELEMETRY,
    AckFrame,
    FrameReceiver,
    TelemetryFrame,
    decode_ack,
    decode_telemetry,
    encode_command,
)


# QEMU's CMSDK UART model on this machine only refills its single-byte RX
# register at roughly one byte per ~10ms regardless of how fast the firmware
# drains it -- so a 13-byte command frame takes ~130ms to fully arrive.
# Sending keep-alives faster than that (e.g. every 50ms) queues commands
# faster than they can transit, building an ever-growing backlog that stalls
# real fault-injection commands behind it. 150ms keeps each send comfortably
# clear of the previous one while staying well under the 500ms
# (COMMS_TIMEOUT_CYCLES * CONTROL_PERIOD_MS) comms-timeout budget.
PEDAL_KEEPALIVE_INTERVAL_S = 0.15


@dataclass
class HarnessStats:
    telemetry_frames_seen: int = 0
    acks_seen: int = 0
    crc_errors_seen: int = 0


class HelmHarness:
    def __init__(self, port: int, host: str = "127.0.0.1", connect_timeout: float = 10.0):
        self.sock = socket.create_connection((host, port), timeout=connect_timeout)
        self.sock.settimeout(0.05)
        self.rx = FrameReceiver()
        self.latest_telemetry: Optional[TelemetryFrame] = None
        self.last_ack: Optional[AckFrame] = None
        self.stats = HarnessStats()
        self._current_pedal_pct = 0.0

    def close(self) -> None:
        self.sock.close()

    # --- low-level frame I/O -------------------------------------------------

    def send_command(self, cmd: int, fault_type: int = 0, target_sensor: int = 0, value: float = 0.0) -> None:
        self.sock.sendall(encode_command(cmd, fault_type, target_sensor, value))

    def _drain_socket(self) -> None:
        try:
            data = self.sock.recv(4096)
        except socket.timeout:
            return
        except (BlockingIOError, OSError):
            return
        if not data:
            return
        self.rx.feed(data)
        for frame_type, payload in self.rx.frames():
            if frame_type == FRAME_TYPE_TELEMETRY:
                self.latest_telemetry = decode_telemetry(payload)
                self.stats.telemetry_frames_seen += 1
            elif frame_type == FRAME_TYPE_ACK:
                self.last_ack = decode_ack(payload)
                self.stats.acks_seen += 1
        self.stats.crc_errors_seen = self.rx.crc_error_count

    def poll(self) -> None:
        """Drains any currently-available socket data. Call frequently from a
        loop when you need up-to-date telemetry without blocking long."""
        self._drain_socket()

    # --- commands --------------------------------------------------------

    def set_pedal(self, pct: float) -> None:
        pct = max(-100.0, min(100.0, pct))
        self._current_pedal_pct = pct
        self.send_command(CMD_SET_PEDAL_OVERRIDE, value=pct)

    def inject_fault(self, fault_type: int, target_sensor: int, value: float = 0.0) -> None:
        self.send_command(CMD_INJECT_FAULT, fault_type=fault_type, target_sensor=target_sensor, value=value)

    def clear_fault(self, target_sensor: int) -> None:
        self.send_command(CMD_CLEAR_FAULT, target_sensor=target_sensor)

    def ping(self) -> None:
        self.send_command(CMD_PING)

    # --- waiting / assertions ---------------------------------------------

    def wait_until(self, predicate: Callable[[TelemetryFrame], bool], timeout_s: float,
                    keep_alive: bool = True) -> tuple[Optional[TelemetryFrame], float]:
        """Polls telemetry until predicate(latest_telemetry) is True or the
        timeout elapses. Returns (matching_frame_or_None, elapsed_seconds).
        If keep_alive, re-sends the current pedal command periodically so
        DTC_COMMS_TIMEOUT doesn't fire as a side effect of waiting itself."""
        start = time.monotonic()
        last_keepalive = start
        while True:
            self._drain_socket()
            now = time.monotonic()
            if self.latest_telemetry is not None and predicate(self.latest_telemetry):
                return self.latest_telemetry, now - start
            if now - start >= timeout_s:
                return None, now - start
            if keep_alive and (now - last_keepalive) >= PEDAL_KEEPALIVE_INTERVAL_S:
                self.set_pedal(self._current_pedal_pct)
                last_keepalive = now

    def run_for(self, duration_s: float, pedal_fn: Optional[Callable[[float], float]] = None,
                sample_interval_s: float = PEDAL_KEEPALIVE_INTERVAL_S) -> list[TelemetryFrame]:
        """Runs for duration_s wall-clock seconds, optionally driving the
        pedal from pedal_fn(elapsed_seconds) -> percent, sent every
        sample_interval_s. Returns every telemetry frame observed."""
        start = time.monotonic()
        last_send = 0.0
        observed: list[TelemetryFrame] = []
        seen_cycles: set[int] = set()
        while True:
            now = time.monotonic()
            elapsed = now - start
            if elapsed >= duration_s:
                break
            if pedal_fn is not None and (elapsed - last_send) >= sample_interval_s:
                self.set_pedal(pedal_fn(elapsed))
                last_send = elapsed
            elif pedal_fn is None and (elapsed - last_send) >= PEDAL_KEEPALIVE_INTERVAL_S:
                self.ping()
                last_send = elapsed
            self._drain_socket()
            if self.latest_telemetry is not None and self.latest_telemetry.cycle_id not in seen_cycles:
                seen_cycles.add(self.latest_telemetry.cycle_id)
                observed.append(self.latest_telemetry)
        return observed


# --- pedal-position profile generators, fn(elapsed_seconds) -> percent --------

def profile_step(target_pct: float) -> Callable[[float], float]:
    return lambda t: target_pct


def profile_ramp(start_pct: float, end_pct: float, duration_s: float) -> Callable[[float], float]:
    def fn(t: float) -> float:
        if duration_s <= 0:
            return end_pct
        frac = max(0.0, min(1.0, t / duration_s))
        return start_pct + (end_pct - start_pct) * frac
    return fn


def profile_sine(center_pct: float, amplitude_pct: float, period_s: float) -> Callable[[float], float]:
    def fn(t: float) -> float:
        v = center_pct + amplitude_pct * math.sin(2 * math.pi * t / period_s)
        return max(0.0, min(100.0, v))
    return fn


def profile_random_driving(seed: int, segment_duration_s: float = 3.0,
                            min_pct: float = 5.0, max_pct: float = 95.0) -> Callable[[float], float]:
    """Piecewise-random driving-style profile: holds and ramps between random
    pedal positions, for the healthy-operation regression scenario.

    min_pct/max_pct default to a safe margin inside [0, 100]: raw sensor
    values are only considered valid in [SENSOR_RAW_MIN, SENSOR_RAW_MAX]
    (roughly [1.2%, 98.8%] in percent), which deliberately treats readings
    near the absolute rails as suspicious (a real ADC has headroom there).
    A driving profile that dips into that zone would trip
    DTC_SENSOR_OUT_OF_RANGE correctly, not falsely -- that's what
    sensor_out_of_range.py exercises on purpose. This profile stays inside
    the normal operating envelope so it actually tests for *false*
    positives during ordinary driving, not the rail-fault detector."""
    rng = random.Random(seed)
    breakpoints = [0.0]
    targets = [rng.uniform(min_pct, max_pct)]

    def _ensure_covered(t: float) -> None:
        while breakpoints[-1] < t + segment_duration_s:
            breakpoints.append(breakpoints[-1] + segment_duration_s)
            targets.append(rng.uniform(min_pct, max_pct))

    def fn(t: float) -> float:
        _ensure_covered(t)
        idx = 0
        while idx + 1 < len(breakpoints) and breakpoints[idx + 1] < t:
            idx += 1
        t0, t1 = breakpoints[idx], breakpoints[idx + 1]
        v0, v1 = targets[idx], targets[idx + 1]
        frac = 0.0 if t1 <= t0 else max(0.0, min(1.0, (t - t0) / (t1 - t0)))
        return v0 + (v1 - v0) * frac

    return fn
