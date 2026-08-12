import asyncio
import struct

from app import serial_link as serial_link_module
from app.protocol import (
    FRAME_TYPE_TELEMETRY,
    TELEMETRY_PAYLOAD_STRUCT,
    TelemetryFrame,
    crc16_ccitt_false,
)
from app.serial_link import SerialLink


def _encode_telemetry_frame(f: TelemetryFrame) -> bytes:
    """Local test-only encoder (the backend only ever decodes telemetry in
    production -- encoding it is the firmware's job -- so this doesn't
    belong in app/protocol.py)."""
    payload = struct.pack(
        TELEMETRY_PAYLOAD_STRUCT,
        f.cycle_id, f.timestamp_ms, f.pedal_raw_a, f.pedal_raw_b,
        f.throttle_raw_a, f.throttle_raw_b, f.pedal_pct, f.throttle_pct,
        f.control_error, f.actuator_duty_pct, f.active_dtc_mask, f.failsafe_active,
    )
    type_and_payload = bytes([FRAME_TYPE_TELEMETRY]) + payload
    crc = crc16_ccitt_false(type_and_payload)
    return bytes([0xAA, 0x55, len(type_and_payload)]) + type_and_payload + struct.pack("<H", crc)


class FakeStreamReader:
    def __init__(self, chunks: list[bytes]):
        self._chunks = list(chunks)

    async def read(self, n: int) -> bytes:
        if not self._chunks:
            return b""  # EOF -- ends the connection loop
        return self._chunks.pop(0)


class FakeStreamWriter:
    def write(self, data: bytes) -> None:
        pass

    async def drain(self) -> None:
        pass

    def close(self) -> None:
        pass


async def test_decode_error_on_one_frame_does_not_stop_ingestion_of_the_next(monkeypatch):
    """A frame that passes CRC but somehow fails to decode (corrupted in a
    way the CRC alone doesn't catch, or a bug in the decoder) must be logged
    and skipped -- not take down the whole ingest connection, per the
    Phase 4 spec: 'a malformed/CRC-failed frame is logged and skipped
    without interrupting ingestion of subsequent frames.'"""
    good_frame = TelemetryFrame(
        cycle_id=1, timestamp_ms=10, pedal_raw_a=100, pedal_raw_b=100, throttle_raw_a=100,
        throttle_raw_b=100, pedal_pct=1.0, throttle_pct=1.0, control_error=0.0,
        actuator_duty_pct=0.0, active_dtc_mask=0, failsafe_active=0,
    )
    frame_bytes = _encode_telemetry_frame(good_frame)

    call_count = {"n": 0}
    real_decode = serial_link_module.decode_telemetry

    def flaky_decode(payload):
        call_count["n"] += 1
        if call_count["n"] == 1:
            raise ValueError("simulated decode failure on first frame")
        return real_decode(payload)

    monkeypatch.setattr(serial_link_module, "decode_telemetry", flaky_decode)

    reader = FakeStreamReader([frame_bytes, frame_bytes])  # two identical frames back to back

    async def fake_open_connection(host, port):
        return reader, FakeStreamWriter()

    monkeypatch.setattr(serial_link_module.asyncio, "open_connection", fake_open_connection)

    received = []

    async def on_telemetry(frame):
        received.append(frame)

    async def on_ack(ack):
        pass

    link = SerialLink(host="127.0.0.1", port=0)

    async def run_until_eof():
        try:
            await asyncio.wait_for(link.run(on_telemetry, on_ack), timeout=1.0)
        except asyncio.TimeoutError:
            pass  # run() loops forever (reconnect); timing it out after EOF is expected

    await run_until_eof()

    # First frame's decode raised and was swallowed; second frame still got through.
    assert call_count["n"] == 2
    assert len(received) == 1
    assert received[0].cycle_id == 1
