# Helm Firmware

Embedded C11 firmware on FreeRTOS: dual-sensor plausibility checking, a PID
throttle control loop, a DTC/fail-safe state machine, and a binary UART
protocol for telemetry and fault-injection commands. Targets the Cortex-M3
core QEMU emulates for its `mps2-an385` machine.

Two independent build targets share the same CMakeLists.txt:

1. **Host unit tests** (`HELM_BUILD_HOST_TESTS`, on by default when not
   cross-compiling) — the pure control-math library (`plant_sim.c`,
   `pid.c`, `plausibility.c`, `protocol.c`) compiled with the system C
   compiler, no RTOS, no QEMU. Fast, deterministic.
2. **The actual firmware** — cross-compiled with `arm-none-eabi-gcc` via
   the toolchain file, links FreeRTOS and runs under QEMU.

## Prerequisites

- `arm-none-eabi-gcc`, `cmake`, `ninja`, `qemu-system-arm` on PATH
- A native C compiler for the host unit tests (any `gcc`/`clang`/MSVC works)

On this project's dev machine (Windows), all of the above came from
MSYS2's `ucrt64` package set via `pacman`.

## Host unit tests

```
mkdir build && cd build
cmake -G Ninja ..
ninja
./helm_host_tests.exe
```

Verifies: PID settling time/overshoot across 5 step sizes, plausibility
fault raise/clear at exactly the configured cycle counts with zero false
positives, and wire-protocol encode/decode roundtrips including a CRC
mismatch/resync case.

## Building and running the firmware

```
mkdir build-arm && cd build-arm
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-toolchain.cmake ..
ninja
```

Produces `helm.elf` (production) and `helm_watchdog_test.elf` (a debug
build with `HELM_TEST_WATCHDOG_STALL` defined -- intentionally stalls
`control_task` once to prove the watchdog trips and forces fail-safe; not
part of the normal firmware).

Run under QEMU with its UART bridged to a TCP socket (see the repo root
`progress.md` for why this is a socket, not `-serial pty`):

```
qemu-system-arm -M mps2-an385 -nographic -kernel helm.elf \
  -chardev socket,id=serial0,host=127.0.0.1,port=5678,server=on,wait=off \
  -serial chardev:serial0 -monitor none
```

Something else (the SiL harness, the backend, or your own script) then
connects to `127.0.0.1:5678` and speaks the wire protocol documented in
`src/protocol.h` and `docs/build-plan.md`.

## Layout

```
src/
  config.h              named constants (control period, PID gains, DTC thresholds)
  plant_sim.c/.h         discrete motor + spring-return plate model
  pid.c/.h                PID with clamped-integrator anti-windup
  plausibility.c/.h       dual-sensor plausibility debounce state machine
  protocol.c/.h           wire framing, CRC-16/CCITT-FALSE, encode/decode
  uart.c/.h                CMSDK APB UART0 register driver
  shared_state.h           cross-task queues and shared volatiles
  watchdog.c/.h             independent deadline-monitor task
  startup_mps2_m3.c        vector table, reset handler
  FreeRTOSConfig.h
  main.c                    task creation, priorities, RTOS hooks
  tasks/
    sensor_task.c           dual raw sensor synthesis + fault-override application
    control_task.c          PID loop, failsafe setpoint override, telemetry assembly
    actuator_task.c          drives plant_sim
    diag_task.c              DTC state machine
    comms_task.c              UART TX (telemetry) / RX (commands) + ACK
third_party/FreeRTOS-Kernel/  vendored, pruned to the ARM_CM3 port (~1.4MB)
linker/mps2_m3.ld
cmake/arm-none-eabi-toolchain.cmake
test/unit/                    host-side test suite
```
