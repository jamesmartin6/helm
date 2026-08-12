import sys
from pathlib import Path

SIL_HARNESS_DIR = Path(__file__).resolve().parent
if str(SIL_HARNESS_DIR) not in sys.path:
    sys.path.insert(0, str(SIL_HARNESS_DIR))

import pytest  # noqa: E402

from harness import HelmHarness  # noqa: E402
from qemu_runner import QemuFirmware  # noqa: E402

_latency_records: list = []


@pytest.fixture
def firmware():
    """Boots a fresh QEMU instance running the firmware for this test, yields
    a connected HelmHarness, and tears both down afterward. Function-scoped
    (one QEMU boot per test) so fault-injection state from one scenario can
    never leak into another."""
    with QemuFirmware() as qemu:
        h = HelmHarness(port=qemu.port)
        try:
            yield h
        finally:
            h.close()


@pytest.fixture
def latency_report():
    """Scenarios append LatencyMeasurement objects to this list; they're
    aggregated into results/latency_report.txt at the end of the run."""
    records: list = []
    yield records
    _latency_records.extend(records)


def pytest_sessionfinish(session, exitstatus):
    if not _latency_records:
        return
    results_dir = SIL_HARNESS_DIR / "results"
    results_dir.mkdir(exist_ok=True)
    report_path = results_dir / "latency_report.txt"
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("Helm SiL Fault-Injection Latency Report\n")
        f.write("=" * 70 + "\n")
        for m in _latency_records:
            f.write(str(m) + "\n")
        f.write("=" * 70 + "\n")
        n_ok = sum(1 for m in _latency_records if m.within_budget)
        f.write(f"{n_ok}/{len(_latency_records)} measurements within budget\n")
    print(f"\nLatency report written to {report_path}")
