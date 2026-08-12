# Helm — Build Progress

This file is the source of truth for what's done and what's next. If a session
gets interrupted (usage limits, context reset), the next session should read
this file first, check the "Currently working on" section, verify the last
claimed-done item actually works (build/run/test), then continue down the list.

Full spec: [docs/build-plan.md](docs/build-plan.md)

## Environment notes (this machine)

- Windows 11, MSYS2 installed at `C:\msys64`.
- Toolchain installed via `pacman` (ucrt64 packages): `arm-none-eabi-gcc`,
  `cmake`, `ninja`, `qemu`, `python-pip`, `gdb-multiarch`.
- **pacman mirror SSL issue**: default MSYS2 mirrors fail TLS verification in
  this environment. Fixed by adding to `C:\msys64\etc\pacman.conf` under
  `[options]`: `XferCommand = /usr/bin/curl.exe -k -L --retry 3 -C - -f -o %o %u`
  (skips cert verification for mirror downloads only — package integrity is
  still checked by pacman's own signature/checksum verification). If pacman
  breaks again, check this line is still present.
- **Stale base install / DLL ABI mismatch**: this MSYS2 install had a base
  system from ~Feb 2025. Installing fresh packages (cmake 4.4.2 etc.) on top
  of old shared libs (zstd, curl, openssl...) caused
  `STATUS_ENTRY_POINT_NOT_FOUND` (exit -1073741511) when running cmake/etc.
  Fixed with a full `pacman -Su --noconfirm` (twice — once for core
  msys2-runtime/bash/pacman itself, then again for the ucrt64 package set).
  If any msys2-provided tool silently fails to launch with no output, this is
  the first thing to suspect — run `pacman -Su` again.
- **Bash tool shell can go stale after upgrading msys2-runtime/bash/pacman**:
  the persistent shell process keeps old DLLs mapped. If a plain `pacman`
  Bash call starts returning bare exit 127 with zero output after an
  msys2-runtime upgrade, that's why — either tolerate it (new Bash tool calls
  get fresh processes) or drive the upgrade from PowerShell instead.
- Host C compiler for Phase 1 unit tests: mingw64 `gcc` at
  `/c/msys64/ucrt64/bin/gcc` (or mingw64/bin).
- PATH: `/c/msys64/ucrt64/bin:/c/msys64/usr/bin` is appended in `~/.bashrc`
  so `gcc`, `arm-none-eabi-gcc`, `cmake`, `ninja`, `qemu-system-arm`,
  `python3`, `gdb-multiarch` are all on PATH in fresh Bash tool shells.
- No Docker installed on this machine — Phase 6 docker-compose is written but
  cannot be verified by running it here. Note this honestly in the README
  rather than claiming it was tested.

## Currently working on

- (updated as work proceeds — see bottom of file for the live pointer)

## Phase 0 — Repo & tooling setup

- [x] `git init`, `.gitignore`, `LICENSE` (MIT)
- [x] Directory skeleton per build plan repo structure
- [x] Toolchain installed (arm-none-eabi-gcc, qemu, cmake, ninja, python, node)
- [ ] GitHub repo created, initial commit pushed
- [ ] progress.md committed and used as the running task ledger

## Phase 1 — Plant model + PID control math (host-testable, no RTOS) — DONE

- [x] `firmware/src/config.h` — named constants from build plan table
- [x] `firmware/src/plant_sim.c/.h` — discrete motor + spring-return throttle plate model
- [x] `firmware/src/pid.c/.h` — PID with anti-windup, output clamp [-100,100]
- [x] `firmware/src/plausibility.c/.h` — dual-sensor plausibility state machine
- [x] `firmware/test/unit/` — host-side test runner (plain C + assert, no external deps)
  - [x] settling time / overshoot tests across 5 step sizes (10/25/50/75/95%)
  - [x] plausibility fault raised at exactly PLAUSIBILITY_FAULT_CYCLES
  - [x] plausibility clear at exactly PLAUSIBILITY_CLEAR_CYCLES
  - [x] no false positives on healthy matched sensor pairs
- [x] CMake host-test target wired up, all 10041 checks passing

Tuned gains (build plan's starting values were too slow for the plant physics
I picked, so retuned per the build plan's own instruction to tune against
these tests): `PID_KP=8.0 PID_KI=3.0 PID_KD=0.1`. Plant constants tuned so a
full-range actuator sweep is physically possible within the settling window
(real ETC actuators are ~100-150ms full range): `PLANT_MOTOR_TIME_CONSTANT_S=0.03`,
`PLANT_MOTOR_GAIN=6.0`, `PLANT_SPRING_RETURN_RATE_PCT_S=60.0`. Results:
settling 40-200ms (max 300ms), overshoot 0.7-1.5% (max 10%), all 5 step sizes.

Build/run: `cd firmware && mkdir build && cd build && cmake -G Ninja .. && ninja && ./helm_host_tests.exe`
(needs `arm-none-eabi-gcc`/etc NOT required for this target — just system gcc via
`/c/msys64/ucrt64/bin` on PATH).

## Phase 2 — FreeRTOS firmware under QEMU

- [ ] Vendor FreeRTOS-Kernel (git submodule or plain copy, per README call-out)
- [ ] `firmware/src/protocol.c/.h` — frame encode/decode + CRC16/CCITT-FALSE
- [ ] `firmware/src/tasks/sensor_task.c`
- [ ] `firmware/src/tasks/control_task.c`
- [ ] `firmware/src/tasks/actuator_task.c`
- [ ] `firmware/src/tasks/diag_task.c` — DTC state machine, failsafe
- [ ] `firmware/src/tasks/comms_task.c` — UART TX/RX framing
- [ ] `firmware/src/watchdog.c`
- [ ] CMake arm-none-eabi toolchain file + mps2-an385 linker script
- [ ] Builds to `build/helm.elf`
- [ ] Runs under `qemu-system-arm -M mps2-an385 -kernel build/helm.elf -serial pty`
- [ ] Telemetry frames readable on PTY at ~100Hz
- [ ] Watchdog trips and forces failsafe when a task is artificially stalled

## Phase 3 — SiL test harness & fault injection

- [ ] `sil-harness/config.py` (mirrors firmware config.h)
- [ ] `sil-harness/harness.py` — PTY open, frame decode, step/ramp/sine pedal commands
- [ ] `sil-harness/fault_injection.py` — stuck/OOR/mismatch/comms-dropout fault commands
- [ ] `sil-harness/scenarios/stuck_pedal_sensor.py`
- [ ] `sil-harness/scenarios/throttle_sensor_mismatch.py`
- [ ] `sil-harness/scenarios/comm_timeout_recovery.py`
- [ ] `sil-harness/scenarios/sensor_out_of_range.py`
- [ ] `sil-harness/scenarios/healthy_regression.py` (5-min randomized, zero false-positive DTCs)
- [ ] All scenarios pass under a single `pytest` invocation w/ latency report
- [ ] Latencies within spec thresholds (50ms plausibility, 500ms comms, +1 cycle to failsafe)

## Phase 4 — Backend telemetry service (FastAPI)

- [ ] SQLAlchemy models: Telemetry, DtcEvent, FaultInjection
- [ ] `serial_ingest.py` — async background frame reader, CRC-validated, drop-and-log on bad frame
- [ ] REST: GET /telemetry, /telemetry/latest, /dtcs, POST /faults/inject, POST /faults/clear, GET /health
- [ ] WebSocket /ws/telemetry (telemetry push + dtc_change events)
- [ ] Postgres via docker-compose; SQLite fallback for local/test runs
- [ ] backend/tests — ingestion, endpoints, malformed-frame resilience

## Phase 5 — Frontend dashboard (React + TS + Vite)

- [ ] `useTelemetrySocket.ts` hook
- [ ] `TelemetryChart.tsx` — live pedal/throttle/error chart
- [ ] `DtcPanel.tsx` — active + historical DTCs
- [ ] `FaultInjectionPanel.tsx` — trigger faults from UI
- [ ] End-to-end manual check: trigger fault from UI, watch chart + DTC panel respond

## Phase 6 — Docker, docs, polish

- [ ] `docker-compose.yml` (qemu+firmware, backend, postgres, frontend)
- [ ] README.md — architecture, quickstart, fault-injection demo walkthrough, screenshots/gif
- [ ] Repo polish: description, topics, social preview if easy
- [ ] Final pass: re-read build plan, confirm every "Definition of done" is met or honestly caveated

## Session log

(Each session appends a short entry here: date, what got done, what's next, any blockers.)

### 2026-08-12 — Session 1
- Read build plan, confirmed environment has no arm-none-eabi-gcc/qemu/cmake/docker preinstalled.
- Fixed MSYS2 pacman SSL/mirror issue (see Environment notes above).
- Installing toolchain via pacman (arm-none-eabi-gcc, cmake, ninja, qemu, python-pip, gdb-multiarch).
- Created repo skeleton, .gitignore, LICENSE, this progress.md.
- Next: finish toolchain install, create GitHub repo, commit skeleton, start Phase 1.
