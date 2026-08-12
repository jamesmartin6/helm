"""
Python implementation of the Helm wire protocol, mirroring firmware/src/protocol.c/.h.

Frame format: [0xAA 0x55] [LEN:u8] [TYPE:u8] [PAYLOAD...] [CRC16:u16]
LEN = 1 (TYPE byte) + payload length. All multi-byte fields little-endian.
CRC16 is CRC-16/CCITT-FALSE over TYPE + PAYLOAD.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass

SYNC0 = 0xAA
SYNC1 = 0x55

FRAME_TYPE_TELEMETRY = 0x01
FRAME_TYPE_COMMAND = 0x10
FRAME_TYPE_ACK = 0x11

CMD_SET_PEDAL_OVERRIDE = 0x01
CMD_INJECT_FAULT = 0x02
CMD_CLEAR_FAULT = 0x03
CMD_PING = 0x04

FAULT_TYPE_NONE = 0x00
FAULT_TYPE_SENSOR_STUCK = 0x01
FAULT_TYPE_SENSOR_OUT_OF_RANGE = 0x02
FAULT_TYPE_SENSOR_MISMATCH = 0x03
FAULT_TYPE_COMMS_DROPOUT = 0x04

TARGET_SENSOR_PEDAL_A = 0x00
TARGET_SENSOR_PEDAL_B = 0x01
TARGET_SENSOR_THROTTLE_A = 0x02
TARGET_SENSOR_THROTTLE_B = 0x03

ACK_STATUS_OK = 0x00
ACK_STATUS_REJECTED = 0x01

TELEMETRY_PAYLOAD_STRUCT = "<IIHHHHffffHB"  # cycle_id..failsafe_active
COMMAND_PAYLOAD_STRUCT = "<BBBf"
ACK_PAYLOAD_STRUCT = "<IBB"


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


def _build_frame(frame_type: int, payload: bytes) -> bytes:
    type_and_payload = bytes([frame_type]) + payload
    crc = crc16_ccitt_false(type_and_payload)
    return bytes([SYNC0, SYNC1, len(type_and_payload)]) + type_and_payload + struct.pack("<H", crc)


def encode_command(cmd: int, fault_type: int = 0, target_sensor: int = 0, value: float = 0.0) -> bytes:
    payload = struct.pack(COMMAND_PAYLOAD_STRUCT, cmd, fault_type, target_sensor, value)
    return _build_frame(FRAME_TYPE_COMMAND, payload)


@dataclass
class TelemetryFrame:
    cycle_id: int
    timestamp_ms: int
    pedal_raw_a: int
    pedal_raw_b: int
    throttle_raw_a: int
    throttle_raw_b: int
    pedal_pct: float
    throttle_pct: float
    control_error: float
    actuator_duty_pct: float
    active_dtc_mask: int
    failsafe_active: int


@dataclass
class AckFrame:
    cycle_id: int
    cmd_echo: int
    status: int


class FrameReceiver:
    """Streaming byte-at-a-time frame receiver, mirroring protocol_rx_t in
    firmware/src/protocol.c. Feed it bytes as they arrive from the socket;
    call `.frames()` to drain any complete, CRC-valid frames found so far."""

    def __init__(self) -> None:
        self._buf = bytearray()
        self.crc_error_count = 0

    def feed(self, data: bytes) -> None:
        self._buf += data

    def frames(self):
        """Generator yielding (frame_type, payload_bytes) for each complete,
        CRC-valid frame currently buffered. Resyncs past corrupt frames."""
        while True:
            idx = self._buf.find(bytes([SYNC0, SYNC1]))
            if idx == -1:
                if len(self._buf) > 1:
                    del self._buf[: len(self._buf) - 1]
                return
            if idx > 0:
                del self._buf[:idx]
            if len(self._buf) < 3:
                return
            length = self._buf[2]
            total = 2 + 1 + length + 2
            if len(self._buf) < total:
                return

            frame = bytes(self._buf[:total])
            type_and_payload = frame[3 : 3 + length]
            (crc_recv,) = struct.unpack_from("<H", frame, 3 + length)
            crc_calc = crc16_ccitt_false(type_and_payload)

            del self._buf[:total]

            if crc_calc != crc_recv:
                self.crc_error_count += 1
                continue

            frame_type = type_and_payload[0]
            payload = type_and_payload[1:]
            yield frame_type, payload


def decode_telemetry(payload: bytes) -> TelemetryFrame:
    fields = struct.unpack(TELEMETRY_PAYLOAD_STRUCT, payload)
    return TelemetryFrame(*fields)


def decode_ack(payload: bytes) -> AckFrame:
    fields = struct.unpack(ACK_PAYLOAD_STRUCT, payload)
    return AckFrame(*fields)
