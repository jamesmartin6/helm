"""
Spawns and tears down a qemu-system-arm instance running the Helm firmware,
bridging its UART to a TCP socket so the harness can connect like it would
to a real serial port.

Windows note: QEMU's `-serial pty` chardev is POSIX-only. This project uses
a TCP socket chardev instead (`-chardev socket,...`), which is functionally
identical (a byte stream) and works on every host QEMU supports -- see
progress.md for the full rationale. `-serial pty` remains available as an
alternative on POSIX hosts if you'd rather use a real PTY; this runner
always uses the socket approach for portability.
"""

from __future__ import annotations

import shutil
import socket
import subprocess
import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_ELF = REPO_ROOT / "firmware" / "build-arm" / "helm.elf"


def _find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def _qemu_binary() -> str:
    for candidate in ("qemu-system-arm", "qemu-system-arm.exe"):
        path = shutil.which(candidate)
        if path:
            return path
    raise RuntimeError(
        "qemu-system-arm not found on PATH. On this project's dev machine it "
        "lives under C:\\msys64\\ucrt64\\bin -- make sure that's on PATH."
    )


class QemuFirmware:
    """Context manager: starts QEMU running the firmware ELF with its UART
    bridged to a fresh TCP port, and tears it down on exit."""

    def __init__(self, elf_path: Path | str = DEFAULT_ELF, boot_wait_s: float = 0.5):
        self.elf_path = Path(elf_path)
        self.boot_wait_s = boot_wait_s
        self.port = _find_free_port()
        self._proc: subprocess.Popen | None = None

    def start(self) -> "QemuFirmware":
        if not self.elf_path.exists():
            raise FileNotFoundError(
                f"Firmware ELF not found at {self.elf_path}. Build it first:\n"
                f"  cd firmware && mkdir -p build-arm && cd build-arm && "
                f"cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=../cmake/arm-none-eabi-toolchain.cmake .. && ninja"
            )
        qemu = _qemu_binary()
        args = [
            qemu,
            "-M", "mps2-an385",
            "-nographic",
            "-kernel", str(self.elf_path),
            "-chardev", f"socket,id=serial0,host=127.0.0.1,port={self.port},server=on,wait=off",
            "-serial", "chardev:serial0",
            "-monitor", "none",
        ]
        self._proc = subprocess.Popen(
            args, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL
        )
        # Give QEMU a moment to bind the listening socket before anyone tries
        # to connect; a hard sleep is simplest and reliable enough here.
        time.sleep(self.boot_wait_s)
        if self._proc.poll() is not None:
            raise RuntimeError(f"qemu-system-arm exited immediately (code {self._proc.returncode})")
        return self

    def stop(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            self._proc.terminate()
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait(timeout=5)
        self._proc = None

    def __enter__(self) -> "QemuFirmware":
        return self.start()

    def __exit__(self, exc_type, exc, tb) -> None:
        self.stop()
