import asyncio
import logging
from datetime import datetime, timezone

from sqlalchemy import select

from .config import settings
from .database import async_session_maker
from .models import DtcEvent, FaultInjection, Telemetry
from .protocol import (
    DTC_ACTUATOR_FAULT,
    DTC_COMMS_TIMEOUT,
    DTC_PEDAL_PLAUSIBILITY,
    DTC_SENSOR_OUT_OF_RANGE,
    DTC_THROTTLE_PLAUSIBILITY,
    DTC_WATCHDOG_TRIP,
    AckFrame,
    TelemetryFrame,
)
from .serial_link import SerialLink
from .ws import manager

logger = logging.getLogger("helm.serial_ingest")

DTC_NAMES = {
    DTC_PEDAL_PLAUSIBILITY: "DTC_PEDAL_PLAUSIBILITY",
    DTC_THROTTLE_PLAUSIBILITY: "DTC_THROTTLE_PLAUSIBILITY",
    DTC_SENSOR_OUT_OF_RANGE: "DTC_SENSOR_OUT_OF_RANGE",
    DTC_COMMS_TIMEOUT: "DTC_COMMS_TIMEOUT",
    DTC_WATCHDOG_TRIP: "DTC_WATCHDOG_TRIP",
    DTC_ACTUATOR_FAULT: "DTC_ACTUATOR_FAULT",
}


class SerialIngestService:
    """Owns the connection to the firmware, persists every telemetry frame,
    tracks DTC raise/clear transitions as DtcEvent rows, and broadcasts live
    updates to WebSocket clients. Also the single place fault-injection
    commands get sent to the firmware from (shared with the REST endpoints),
    so there's exactly one writer on the UART connection."""

    def __init__(self) -> None:
        self.link = SerialLink(
            settings.firmware_host, settings.firmware_port, settings.firmware_reconnect_interval_s
        )
        self._last_dtc_mask = 0
        self._latest: dict | None = None
        self._task: asyncio.Task | None = None

    @property
    def firmware_connected(self) -> bool:
        return self.link.connected

    @property
    def latest_telemetry(self) -> dict | None:
        return self._latest

    def start(self) -> None:
        self._task = asyncio.create_task(self.link.run(self._on_telemetry, self._on_ack))

    async def send_command(self, cmd: int, fault_type: int = 0, target_sensor: int = 0, value: float = 0.0) -> bool:
        return await self.link.send_command(cmd, fault_type=fault_type, target_sensor=target_sensor, value=value)

    async def record_fault_injection(self, fault_type: str, target_sensor: str | None, source: str) -> FaultInjection:
        async with async_session_maker() as session:
            row = FaultInjection(fault_type=fault_type, target_sensor=target_sensor, source=source)
            session.add(row)
            await session.commit()
            await session.refresh(row)
            return row

    async def _on_telemetry(self, frame: TelemetryFrame) -> None:
        recorded_at = datetime.now(timezone.utc)

        try:
            async with async_session_maker() as session:
                row = Telemetry(
                    cycle_id=frame.cycle_id,
                    timestamp_ms=frame.timestamp_ms,
                    pedal_pct=frame.pedal_pct,
                    throttle_pct=frame.throttle_pct,
                    control_error=frame.control_error,
                    actuator_duty_pct=frame.actuator_duty_pct,
                    active_dtc_mask=frame.active_dtc_mask,
                    failsafe_active=bool(frame.failsafe_active),
                    recorded_at=recorded_at,
                )
                session.add(row)
                await self._update_dtc_events(session, frame.active_dtc_mask)
                await session.commit()
        except Exception:
            # A DB hiccup must not take down the ingest loop -- log and keep
            # decoding subsequent frames rather than crashing the service.
            logger.exception("failed to persist telemetry frame cycle_id=%d", frame.cycle_id)

        self._latest = {
            "cycle_id": frame.cycle_id,
            "timestamp_ms": frame.timestamp_ms,
            "pedal_pct": frame.pedal_pct,
            "throttle_pct": frame.throttle_pct,
            "control_error": frame.control_error,
            "actuator_duty_pct": frame.actuator_duty_pct,
            "active_dtc_mask": frame.active_dtc_mask,
            "failsafe_active": bool(frame.failsafe_active),
            "recorded_at": recorded_at.isoformat(),
        }
        await manager.broadcast({"type": "telemetry", **self._latest})

    async def _update_dtc_events(self, session, new_mask: int) -> None:
        old_mask = self._last_dtc_mask
        if new_mask == old_mask:
            return
        changed = old_mask ^ new_mask

        for code, name in DTC_NAMES.items():
            if not (changed & code):
                continue
            if new_mask & code:
                session.add(DtcEvent(code=code, name=name))
            else:
                result = await session.execute(
                    select(DtcEvent)
                    .where(DtcEvent.code == code, DtcEvent.cleared_at.is_(None))
                    .order_by(DtcEvent.raised_at.desc())
                )
                open_event = result.scalars().first()
                if open_event is not None:
                    open_event.cleared_at = datetime.now(timezone.utc)

        self._last_dtc_mask = new_mask
        await manager.broadcast({"type": "dtc_change", "active_dtc_mask": new_mask})

    async def _on_ack(self, ack: AckFrame) -> None:
        logger.debug("ack: cycle_id=%d cmd_echo=0x%02x status=%d", ack.cycle_id, ack.cmd_echo, ack.status)


ingest_service = SerialIngestService()
