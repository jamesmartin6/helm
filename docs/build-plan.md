# Helm — Embedded Throttle-by-Wire Control Unit with HiL/SiL Test Harness

**Stack:** Embedded C, FreeRTOS, ARM Cortex-M (QEMU-emulated, optional real STM32 Nucleo), Python, FastAPI, PostgreSQL, React, TypeScript, Docker
**Status:** Planned — not yet built. Proposed project plan, written for handoff to Claude Code or for manual implementation.

## Why this project

Your existing portfolio (Signal, Quorum, TaskFlow, TaskRunner, Cadence, Gridiron) is all backend/web full-stack — strong general software engineering signal, but none of it touches embedded C, RTOS, or control-systems work, which is the core of the Bosch Powertrain Controls Software Engineering Intern JD. This project is designed specifically to close that gap: it's a simulated **Electronic Throttle Control (ETC)** unit — the kind of closed-loop actuator control problem that sits at the heart of VCU/ECU/TCU work — built with the same rigor Bosch's own posting asks for: rapid prototyping, model-based control logic, hardware-software interface debugging, and SiL/HiL-style automated test validation.

It's "full stack" in a real sense: embedded firmware at the bottom, a backend telemetry/logging service in the middle, and a live React dashboard on top — plus a fault-injection test harness that plays the role of a HiL/SiL rig.

## Project Overview

**Goal:** A closed-loop throttle-by-wire controller running as embedded C firmware on FreeRTOS (emulated via QEMU, with an optional real STM32 Nucleo board as a stretch goal) that reads a simulated accelerator pedal position, drives a simulated throttle-plate actuator via a PID control loop, performs dual-sensor plausibility checks and fail-safe fallback the way a real automotive ECU would, and streams telemetry over a serial link to a backend service and a live web dashboard. A separate SiL test harness feeds the firmware realistic plant physics in closed loop and systematically injects sensor/communication faults to verify the firmware detects and responds correctly — this is the automated test software and fault-mode validation the JD calls out directly.

## Tech Stack

- **Firmware:** C11, FreeRTOS kernel v10.6.x (vendored under `firmware/third_party/FreeRTOS-Kernel`), ARM Cortex-M4 target
- **Toolchain:** `arm-none-eabi-gcc` (13.x), CMake 3.25+ with the `arm-none-eabi` toolchain file, `arm-none-eabi-gdb` for debugging
- **SiL emulation:** QEMU `qemu-system-arm`, machine `mps2-an385` (Cortex-M3; close enough to M4 for this project without an FPU dependency — note this substitution explicitly in the README), firmware loaded via `-kernel build/helm.elf`
- **Plant/physics simulation:** A small discrete-time motor + throttle-plate model (first-order motor dynamics + spring-return plate), implemented in C, swappable between "simulated plant" (SiL) and "real potentiometer + DC motor" (HiL stretch goal)
- **Communication:** Custom lightweight binary framing protocol over UART (QEMU's `-serial pty` maps the emulated UART to a host PTY the backend reads like a real serial port), analogous in spirit to how an ECU exports diagnostics over a serial/CAN-like link
- **Test harness:** Python 3.11+, `pyserial` for the PTY/UART link, `pytest` as the scenario runner
- **Backend:** Python, FastAPI, PostgreSQL, SQLAlchemy — ingests the serial telemetry stream, decodes frames, persists telemetry + DTCs (diagnostic trouble codes), exposes REST + WebSocket API
- **Frontend:** React + TypeScript + Vite — live telemetry dashboard (pedal position, throttle position, control error, actuator duty cycle, active DTCs) plus a fault-injection control panel
- **Orchestration:** Docker Compose (QEMU+firmware container, backend, Postgres, frontend)

## Repo Structure

```
helm/
├── docker-compose.yml
├── README.md
├── firmware/                    # Embedded C, FreeRTOS
│   ├── CMakeLists.txt
│   ├── src/
│   │   ├── main.c
│   │   ├── config.h                # tunable constants (control period, PID gains, DTC thresholds)
│   │   ├── tasks/
│   │   │   ├── sensor_task.c       # reads dual APP/TPS signals, plausibility check
│   │   │   ├── control_task.c      # PID control loop, fixed control-period tick
│   │   │   ├── actuator_task.c     # drives throttle-plate actuator (PWM)
│   │   │   ├── diag_task.c         # DTC state machine, fail-safe/limp-home logic
│   │   │   └── comms_task.c        # UART framing, telemetry TX, command RX
│   │   ├── plant_sim.c             # discrete-time motor + throttle-plate model (SiL mode)
│   │   ├── protocol.c/.h           # binary frame encode/decode (shared with backend)
│   │   └── watchdog.c
│   └── test/
│       └── unit/                   # host-side unit tests for control math, plausibility logic
├── sil-harness/                 # Python SiL test harness
│   ├── config.py                   # mirrors firmware config.h constants
│   ├── harness.py                  # drives closed-loop sim, talks to firmware over serial/pty
│   ├── fault_injection.py          # sensor stuck/out-of-range/cross-check-mismatch/comm-loss faults
│   └── scenarios/                  # named test scenarios with pass/fail assertions
├── backend/                     # Python, FastAPI
│   ├── app/
│   │   ├── main.py
│   │   ├── serial_ingest.py        # reads the telemetry stream, decodes frames
│   │   ├── models.py               # Telemetry, DTC, FaultInjectionEvent
│   │   ├── routers/telemetry.py
│   │   └── ws.py                   # live telemetry over WebSocket
│   └── tests/
└── frontend/                    # React + TypeScript
    ├── src/
    │   ├── App.tsx
    │   ├── components/
    │   │   ├── TelemetryChart.tsx  # pedal vs. throttle position, control error, live
    │   │   ├── DtcPanel.tsx        # active/historical diagnostic trouble codes
    │   │   └── FaultInjectionPanel.tsx  # trigger SiL faults from the UI
    │   └── hooks/useTelemetrySocket.ts
    └── public/
```

## Core Concept Primer

A real electronic throttle body doesn't just read one pedal sensor and drive one motor — it reads **two independent pedal-position sensors** and **two independent throttle-position sensors**, cross-checks them against each other every control cycle for plausibility, and falls back to a safe default (a spring-loaded limp-home throttle angle) the instant something doesn't add up — a stuck sensor, an out-of-range reading, a communication timeout. This project implements that pattern in miniature: dual simulated sensors, a plausibility-check state machine, a PID control loop closing the loop between commanded and actual throttle position, and a diagnostic layer that raises DTCs and forces fail-safe behavior on fault — the same category of safety-critical control logic the JD's VCU/ECU/TCU language is pointing at.

## Control Loop Constants

These are starting defaults — Claude Code should implement them as named constants (`config.h` on firmware, `config.py` on the harness/backend), not magic numbers, so they're tunable in one place:

| Constant | Value | Notes |
|---|---|---|
| `CONTROL_PERIOD_MS` | 10 | Control loop tick rate (100 Hz) |
| `PID_KP` / `PID_KI` / `PID_KD` | 2.5 / 0.8 / 0.05 | Starting gains; tune against Phase 1 settling-time tests |
| `SETTLING_TIME_MAX_MS` | 300 | Max time to settle within tolerance band after a step input |
| `SETTLING_TOLERANCE_PCT` | 2.0 | ± throttle-position error band counted as "settled" |
| `OVERSHOOT_MAX_PCT` | 10.0 | Max allowed overshoot past commanded position |
| `SENSOR_PLAUSIBILITY_TOLERANCE_PCT` | 5.0 | Max allowed disagreement between dual sensor pair |
| `PLAUSIBILITY_FAULT_CYCLES` | 5 | Consecutive out-of-tolerance cycles before a fault is raised |
| `PLAUSIBILITY_CLEAR_CYCLES` | 20 | Consecutive healthy cycles before a fault clears |
| `FAILSAFE_THROTTLE_PCT` | 8.0 | Limp-home throttle angle (idle-plus, not fully closed) |
| `COMMS_TIMEOUT_CYCLES` | 50 | Missed telemetry-ack cycles before comms-loss DTC |
| `WATCHDOG_DEADLINE_MS` | 15 | Max allowed control-loop tick overrun before watchdog trips |
| `TELEMETRY_RATE_HZ` | 100 | One telemetry frame per control cycle |

## Wire Protocol (`protocol.c/.h`, shared spec between firmware and backend)

All multi-byte fields little-endian. Every frame is wrapped: `[0xAA 0x55] [LEN:u8] [TYPE:u8] [PAYLOAD...] [CRC16:u16]`. CRC16 is CRC-16/CCITT-FALSE over `TYPE + PAYLOAD`. Receiver drops and resyncs on CRC mismatch (log a `comms_crc_error` counter, don't crash).

**Telemetry frame** (`TYPE = 0x01`, firmware → backend, one per control cycle):

| Field | Type | Description |
|---|---|---|
| `cycle_id` | u32 | Monotonic control-cycle counter |
| `timestamp_ms` | u32 | Firmware uptime in ms |
| `pedal_raw_a`, `pedal_raw_b` | u16, u16 | Dual APP sensor raw values (0-4095, simulated 12-bit ADC) |
| `throttle_raw_a`, `throttle_raw_b` | u16, u16 | Dual TPS sensor raw values (0-4095) |
| `pedal_pct` | float32 | Resolved commanded pedal position, 0.0-100.0 |
| `throttle_pct` | float32 | Resolved actual throttle position, 0.0-100.0 |
| `control_error` | float32 | `pedal_pct - throttle_pct` (post fail-safe override if active) |
| `actuator_duty_pct` | float32 | PWM duty applied to actuator, -100.0-100.0 |
| `active_dtc_mask` | u16 | Bitmask of currently active DTCs (see table below) |
| `failsafe_active` | u8 | 0/1 |

**Command frame** (`TYPE = 0x10`, backend/harness → firmware):

| Field | Type | Description |
|---|---|---|
| `cmd` | u8 | `0x01=SET_PEDAL_OVERRIDE, 0x02=INJECT_FAULT, 0x03=CLEAR_FAULT, 0x04=PING` |
| `fault_type` | u8 | `0x00=NONE, 0x01=SENSOR_STUCK, 0x02=SENSOR_OUT_OF_RANGE, 0x03=SENSOR_MISMATCH, 0x04=COMMS_DROPOUT` (only meaningful for `INJECT_FAULT`) |
| `target_sensor` | u8 | `0x00=PEDAL_A, 0x01=PEDAL_B, 0x02=THROTTLE_A, 0x03=THROTTLE_B` |
| `value_f32` | float32 | Override value (pedal override %, or stuck-sensor freeze value) |

**Ack frame** (`TYPE = 0x11`, firmware → backend, sent in response to every command frame): `cycle_id:u32`, `cmd_echo:u8`, `status:u8` (`0x00=OK, 0x01=REJECTED`).

## DTC Table

| Code | Name | Raised when | Cleared when |
|---|---|---|---|
| `0x0001` | `DTC_PEDAL_PLAUSIBILITY` | Pedal sensor pair disagreement > `SENSOR_PLAUSIBILITY_TOLERANCE_PCT` for `PLAUSIBILITY_FAULT_CYCLES` | Agreement restored for `PLAUSIBILITY_CLEAR_CYCLES` |
| `0x0002` | `DTC_THROTTLE_PLAUSIBILITY` | Same, for the throttle sensor pair | Same |
| `0x0004` | `DTC_SENSOR_OUT_OF_RANGE` | Any raw sensor value outside `[50, 4045]` (allows headroom below/above rail for a real ADC) | Value back in range for `PLAUSIBILITY_CLEAR_CYCLES` |
| `0x0008` | `DTC_COMMS_TIMEOUT` | No valid command-frame CRC received for `COMMS_TIMEOUT_CYCLES` | A valid frame received |
| `0x0010` | `DTC_WATCHDOG_TRIP` | A task misses `WATCHDOG_DEADLINE_MS` | Requires full firmware reset (matches real ECU behavior — a watchdog trip is not self-clearing) |
| `0x0020` | `DTC_ACTUATOR_FAULT` | Commanded duty and estimated actual plate response diverge beyond tolerance for N cycles (stretch: only meaningful once real-HiL actuator feedback exists) | Divergence resolved |

`active_dtc_mask` in the telemetry frame is the OR of all currently-active codes above. Any nonzero `DTC_WATCHDOG_TRIP` bit implies `failsafe_active = 1` and is latched until reset.



Deliverable: The physics model and PID control math as a pure-C library, unit-testable on the host machine before touching FreeRTOS or QEMU.

Spec:
- `plant_sim.c`: a discrete-time model — pedal position (0-100%) as the commanded input, a first-order-lag DC motor model driving a throttle plate with spring-return-to-idle dynamics, output is actual throttle-plate angle. Model runs at `CONTROL_PERIOD_MS` resolution.
- PID controller using `PID_KP`/`PID_KI`/`PID_KD` as starting gains, with anti-windup on the integral term and output clamped to `[-100.0, 100.0]` duty percent.
- Dual-sensor plausibility check per the DTC table above: `SENSOR_PLAUSIBILITY_TOLERANCE_PCT` / `PLAUSIBILITY_FAULT_CYCLES` / `PLAUSIBILITY_CLEAR_CYCLES`.

Definition of done: host-side unit tests (no RTOS) verify the PID loop settles within `SETTLING_TOLERANCE_PCT` of a commanded step input within `SETTLING_TIME_MAX_MS`, with overshoot under `OVERSHOOT_MAX_PCT`, across at least 5 step sizes spanning the pedal range; plausibility-check tests confirm fault raised at exactly `PLAUSIBILITY_FAULT_CYCLES` and cleared at exactly `PLAUSIBILITY_CLEAR_CYCLES`, with no false positives on matched healthy sensor pairs.

## Phase 2 — FreeRTOS Firmware (SiL, running under QEMU)

Deliverable: The control loop, sensor, actuator, diagnostic, and comms logic running as FreeRTOS tasks under QEMU, with `plant_sim.c` standing in for real hardware.

Spec:
- `sensor_task`: samples the (simulated) dual pedal/throttle sensors on a fixed period, runs the plausibility check, publishes clean values to the control task via a queue.
- `control_task`: runs the PID loop against the plant model at a fixed control-period tick using a hardware timer/RTOS periodic task, publishes commanded actuator duty cycle.
- `actuator_task`: applies the commanded PWM duty cycle to the (simulated) motor.
- `diag_task`: owns the DTC state machine — raises/clears diagnostic codes, forces fail-safe (limp-home throttle angle) when a fault is active, clears fail-safe only after N consecutive healthy cycles.
- `watchdog.c`: an independent watchdog task that verifies the control loop is executing on schedule and forces fail-safe if a task misses its deadline (a real embedded-systems concern: a hung or overrunning task in a control loop is a safety issue, not just a bug).
- `comms_task`: encodes a telemetry frame (timestamps, sensor values, control error, actuator output, active DTCs) every control cycle and writes it out over UART (bridged to a PTY when running under QEMU so the backend can read it like a real serial port); also decodes inbound command frames (used by the SiL harness for fault injection and by the backend for mode commands).

Definition of done: firmware builds and runs under QEMU; with the plant simulation driving inputs, the control loop holds commanded throttle position within tolerance in steady state; the watchdog demonstrably forces fail-safe if a task is artificially stalled; telemetry frames are readable on the PTY end at the expected rate.

## Phase 3 — SiL Test Harness & Fault Injection

Deliverable: A Python harness that closes the loop around the QEMU-hosted firmware and systematically exercises fault modes — this is the project's centerpiece, directly answering the JD's "developing automated test software for software testing" and HiL/SiL validation language.

Spec:
- `harness.py` opens the firmware's PTY, decodes telemetry frames in real time, and can inject step/ramp/sinusoidal pedal-position commands to exercise the control loop under different driving-style profiles.
- `fault_injection.py` sends command frames simulating: a stuck sensor (value frozen), an out-of-range sensor reading, a dual-sensor plausibility mismatch, and a communications dropout (no telemetry for N cycles) — then asserts, from the telemetry stream, that the firmware raises the correct DTC and enters fail-safe within a specified maximum number of control cycles, and clears it correctly once the fault is removed.
- `scenarios/` holds a set of named, repeatable test scenarios (e.g. `stuck_pedal_sensor.py`, `throttle_sensor_mismatch.py`, `comm_timeout_recovery.py`) each producing a pass/fail result and a timing report (fault-to-detection latency, detection-to-failsafe latency).

Definition of done: every fault scenario passes reliably, with measured fault-to-DTC-raised latency under `PLAUSIBILITY_FAULT_CYCLES × CONTROL_PERIOD_MS` (50ms for plausibility faults) or `COMMS_TIMEOUT_CYCLES × CONTROL_PERIOD_MS` (500ms for comms dropout) as applicable, and DTC-to-failsafe-active latency under one additional control cycle; a healthy-operation regression scenario confirms zero false-positive DTCs across a 5-minute run of randomized driving-style pedal inputs; the full scenario suite is runnable as a single `pytest` command and produces a pass/fail report with per-scenario latency numbers, suitable for CI.

## Phase 4 — Backend Telemetry Service

Deliverable: A FastAPI service that ingests the live telemetry stream, persists it, and exposes it to the frontend.

Spec:
- `serial_ingest.py` reads and decodes telemetry frames (per the wire protocol above) from the firmware's serial/PTY output continuously, in a background asyncio task; validates CRC16 and drops/logs malformed frames rather than crashing.
- SQLAlchemy models:
  - `Telemetry`: `id (PK)`, `cycle_id (int, indexed)`, `timestamp_ms (int)`, `pedal_pct (float)`, `throttle_pct (float)`, `control_error (float)`, `actuator_duty_pct (float)`, `active_dtc_mask (int)`, `failsafe_active (bool)`, `recorded_at (datetime, indexed)`
  - `DtcEvent`: `id (PK)`, `code (int)`, `name (str)`, `raised_at (datetime)`, `cleared_at (datetime, nullable)`, `cause (str, nullable)`
  - `FaultInjection`: `id (PK)`, `fault_type (str)`, `target_sensor (str, nullable)`, `triggered_at (datetime)`, `source (str)` (`"harness"` or `"ui"`)
- REST endpoints:

| Method | Path | Query/Body | Response | Notes |
|---|---|---|---|---|
| GET | `/telemetry` | `?since=<ts>&limit=` | `list[TelemetryOut]` | paginated, most recent first |
| GET | `/telemetry/latest` | — | `TelemetryOut` | current state, for initial dashboard load |
| GET | `/dtcs` | `?active_only=bool` | `list[DtcEventOut]` | |
| POST | `/faults/inject` | `{fault_type, target_sensor, value}` | `FaultInjectionOut`, 201 | forwards a command frame to the firmware over the serial link |
| POST | `/faults/clear` | `{target_sensor}` | 204 | |
| GET | `/health` | — | `{status, firmware_connected: bool}` | |

- WebSocket: `GET /ws/telemetry` — pushes a `TelemetryOut`-shaped JSON message on every decoded frame, plus a `{type: "dtc_change", ...}` message whenever `active_dtc_mask` changes between frames.

Definition of done: telemetry appears in Postgres in real time while the firmware runs; the WebSocket stream delivers updates to a connected client with no perceptible lag; historical queries return correct data for a given time range; a malformed/CRC-failed frame is logged and skipped without interrupting ingestion of subsequent frames.

## Phase 5 — Frontend Dashboard

Deliverable: A React/TypeScript dashboard giving a live view into the control loop, the way a bench engineer would monitor an ECU during validation testing.

Spec:
- `TelemetryChart.tsx`: live-scrolling chart of commanded vs. actual throttle position and control error, updating from the WebSocket stream.
- `DtcPanel.tsx`: shows currently active DTCs and a history log with raised/cleared timestamps and cause.
- `FaultInjectionPanel.tsx`: buttons to trigger each fault scenario against the running firmware and watch the dashboard respond live — stuck sensor, sensor mismatch, comm dropout — with the resulting fault-to-failsafe transition visible on the chart in real time.

Definition of done: a demo consists of opening the dashboard, triggering a fault from the UI, and watching the telemetry chart show the control loop detect the fault and transition to fail-safe throttle position within the expected latency, with the DTC panel updating to match.

## Phase 6 — Dockerization, Docs, and (Stretch) Real Hardware-in-the-Loop

Deliverable: One-command startup for the full SiL stack, plus an optional real-HiL extension.

Spec:
- `docker-compose.yml` brings up the QEMU+firmware container, backend, Postgres, and frontend together.
- README documents the architecture, how to run the SiL fault-injection suite standalone, and includes a walkthrough of the fault-injection demo (this is the single best thing to show in an interview).
- **Stretch goal — real HiL:** swap `plant_sim.c` for real hardware — an STM32 Nucleo board, a potentiometer standing in for the pedal sensor, and a small DC motor (with an L298N driver) standing in for the throttle actuator. The control/diagnostic/comms firmware logic is unchanged; only the sensor-read and actuator-write functions swap from simulated to real GPIO/ADC/PWM calls. This turns the project from SiL into genuine HiL testing and is a strong, concrete answer to interview questions about hands-on hardware experience.

Definition of done: `docker compose up` runs the full SiL demo with zero manual steps; a stranger can follow the README to reproduce the fault-injection demo end to end.

## Testing Requirements (applies across phases)

Host-side unit tests for the PID control math and plausibility-check logic (Phase 1) — fast, deterministic, no hardware or RTOS involved. The SiL fault-injection suite (Phase 3) is the single highest-signal test in the project, since it directly demonstrates the kind of automated test software and fault-mode validation the JD asks for, rather than just checking a few hand-picked scenarios. If the real-HiL stretch goal is built, the same fault-injection scenarios should be re-run against real hardware to confirm SiL results transfer.

## Notes for the Resume / Interview Story

Once built, the concrete things to point to:
- "Implemented a closed-loop throttle-by-wire controller in embedded C on FreeRTOS, including dual-sensor plausibility checking and fail-safe fallback logic consistent with automotive functional-safety patterns"
- "Built a SiL test harness that systematically injects sensor and communication faults and asserts fault-to-failsafe response within a defined latency bound — full automated regression coverage of the firmware's failure modes"
- "Full-stack delivery: firmware, backend telemetry ingestion, and a live React dashboard, including UI-triggered fault injection for demoing control-loop behavior in real time"
- If the real-HiL stretch goal is completed: concrete hands-on experience with real sensor/actuator hardware, not just simulation — directly answers the JD's HiL requirement
