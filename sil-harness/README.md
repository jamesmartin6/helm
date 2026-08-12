# Helm SiL Test Harness

Software-in-the-loop fault-injection test suite for the Helm firmware. Boots
the firmware under QEMU, drives it in closed loop over its UART, injects
sensor and communication faults, and asserts the firmware detects and
responds to each one within the latency budgets in `config.py` (mirrored
from `firmware/src/config.h`).

## Setup

```
python3 -m venv .venv
.venv/bin/pip install -r requirements.txt   # .venv/Scripts/pip on native Windows Python
```

Build the firmware first if you haven't:

```
cd ../firmware
mkdir -p build-arm && cd build-arm
cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-toolchain.cmake ..
ninja
```

## Running

```
.venv/bin/python -m pytest -v -s
```

Each scenario boots a fresh QEMU instance (function-scoped `firmware`
fixture in `conftest.py`), so fault-injection state never leaks between
tests. A pass/fail-with-latency-numbers report is written to
`results/latency_report.txt` after the run.

`healthy_regression.py` defaults to the full 5-minute randomized-driving
run the build plan's Definition of Done calls for. For a faster smoke-test
pass during development:

```
HELM_HEALTHY_REGRESSION_DURATION_S=20 .venv/bin/python -m pytest -v -s
```

## Windows note: TCP socket instead of PTY

QEMU's `-serial pty` chardev is POSIX-only. `qemu_runner.py` bridges the
firmware's UART to a TCP socket instead (`-chardev socket,...`) --
functionally identical (a byte stream) and portable to every host QEMU
supports. See `progress.md` at the repo root for the full rationale.

## A note on latency measurement methodology

QEMU's CMSDK UART model on this target transfers roughly one byte per
~10ms of simulated time regardless of how fast the firmware drains it, so a
13-byte command frame takes ~130ms to fully arrive. That's a QEMU
UART-emulation artifact, not firmware behavior -- so scenario latency
measurements start their clock from when a fault (or its removal) actually
becomes *observable in telemetry*, not from when the command was sent. See
`fault_injection.measure_fault_onset_to_dtc_latency`'s docstring for the
full explanation. `config.MEASUREMENT_MARGIN_MS` adds a further, smaller
allowance for the harness's own socket-polling/observation jitter -- the
raw measured latency is always reported in full regardless, so a firmware
regression that blows the real budget still fails the test.
