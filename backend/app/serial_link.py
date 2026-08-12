import asyncio
import logging
from typing import Awaitable, Callable

from .protocol import (
    FRAME_TYPE_ACK,
    FRAME_TYPE_TELEMETRY,
    AckFrame,
    FrameReceiver,
    TelemetryFrame,
    decode_ack,
    decode_telemetry,
    encode_command,
)

logger = logging.getLogger("helm.serial_link")


class SerialLink:
    """Maintains a TCP connection to the firmware's UART bridge (see
    sil-harness/qemu_runner.py and progress.md for why this is a socket, not
    a real serial port, on this project). Reconnects automatically if the
    firmware isn't up yet or the connection drops -- this must never crash
    the whole service just because the firmware container isn't ready."""

    def __init__(self, host: str, port: int, reconnect_interval_s: float = 2.0):
        self.host = host
        self.port = port
        self.reconnect_interval_s = reconnect_interval_s
        self._reader: asyncio.StreamReader | None = None
        self._writer: asyncio.StreamWriter | None = None
        self.connected = False

    async def run(
        self,
        on_telemetry: Callable[[TelemetryFrame], Awaitable[None]],
        on_ack: Callable[[AckFrame], Awaitable[None]],
    ) -> None:
        """Runs forever: connects, decodes frames, dispatches to callbacks,
        reconnects on any disconnect or connection failure."""
        rx = FrameReceiver()
        while True:
            try:
                self._reader, self._writer = await asyncio.open_connection(self.host, self.port)
                self.connected = True
                logger.info("connected to firmware at %s:%d", self.host, self.port)
                while True:
                    data = await self._reader.read(4096)
                    if not data:
                        break
                    rx.feed(data)
                    for frame_type, payload in rx.frames():
                        try:
                            if frame_type == FRAME_TYPE_TELEMETRY:
                                await on_telemetry(decode_telemetry(payload))
                            elif frame_type == FRAME_TYPE_ACK:
                                await on_ack(decode_ack(payload))
                        except Exception:
                            # A CRC-valid-but-otherwise-malformed frame (or a
                            # bug in a callback) must not kill the whole
                            # ingest connection -- log it and keep reading
                            # subsequent frames.
                            logger.exception("failed to decode/handle frame type=0x%02x", frame_type)
            except (ConnectionRefusedError, OSError) as exc:
                logger.info("firmware not reachable at %s:%d (%s), retrying...", self.host, self.port, exc)
            finally:
                self.connected = False
                self._writer = None
                self._reader = None
            await asyncio.sleep(self.reconnect_interval_s)

    async def send_command(self, cmd: int, fault_type: int = 0, target_sensor: int = 0, value: float = 0.0) -> bool:
        """Returns False (rather than raising) if not currently connected --
        callers turn that into a proper HTTP error response."""
        if self._writer is None:
            return False
        self._writer.write(encode_command(cmd, fault_type, target_sensor, value))
        await self._writer.drain()
        return True
