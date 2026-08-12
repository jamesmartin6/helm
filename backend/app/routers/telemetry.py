from datetime import datetime

from fastapi import APIRouter, Depends, HTTPException, Query, status
from sqlalchemy import select
from sqlalchemy.ext.asyncio import AsyncSession

from ..database import get_session
from ..models import DtcEvent, Telemetry
from ..protocol import (
    CMD_CLEAR_FAULT,
    CMD_INJECT_FAULT,
    FAULT_TYPE_COMMS_DROPOUT,
    FAULT_TYPE_SENSOR_MISMATCH,
    FAULT_TYPE_SENSOR_OUT_OF_RANGE,
    FAULT_TYPE_SENSOR_STUCK,
    TARGET_SENSOR_PEDAL_A,
    TARGET_SENSOR_PEDAL_B,
    TARGET_SENSOR_THROTTLE_A,
    TARGET_SENSOR_THROTTLE_B,
)
from ..schemas import (
    DtcEventOut,
    FaultClearRequest,
    FaultInjectionOut,
    FaultInjectRequest,
    HealthOut,
    TelemetryOut,
)
from ..serial_ingest import ingest_service

router = APIRouter()

_FAULT_TYPE_MAP = {
    "stuck": FAULT_TYPE_SENSOR_STUCK,
    "out_of_range": FAULT_TYPE_SENSOR_OUT_OF_RANGE,
    "mismatch": FAULT_TYPE_SENSOR_MISMATCH,
    "comms_dropout": FAULT_TYPE_COMMS_DROPOUT,
}

_TARGET_SENSOR_MAP = {
    "pedal_a": TARGET_SENSOR_PEDAL_A,
    "pedal_b": TARGET_SENSOR_PEDAL_B,
    "throttle_a": TARGET_SENSOR_THROTTLE_A,
    "throttle_b": TARGET_SENSOR_THROTTLE_B,
}


@router.get("/telemetry", response_model=list[TelemetryOut])
async def get_telemetry(
    since: datetime | None = Query(default=None),
    limit: int = Query(default=100, ge=1, le=10000),
    session: AsyncSession = Depends(get_session),
):
    stmt = select(Telemetry).order_by(Telemetry.recorded_at.desc()).limit(limit)
    if since is not None:
        stmt = stmt.where(Telemetry.recorded_at >= since)
    result = await session.execute(stmt)
    return result.scalars().all()


@router.get("/telemetry/latest", response_model=TelemetryOut)
async def get_latest_telemetry(session: AsyncSession = Depends(get_session)):
    cached = ingest_service.latest_telemetry
    if cached is not None:
        return cached
    result = await session.execute(select(Telemetry).order_by(Telemetry.recorded_at.desc()).limit(1))
    row = result.scalars().first()
    if row is None:
        raise HTTPException(status_code=404, detail="no telemetry recorded yet")
    return row


@router.get("/dtcs", response_model=list[DtcEventOut])
async def get_dtcs(
    active_only: bool = Query(default=False),
    session: AsyncSession = Depends(get_session),
):
    stmt = select(DtcEvent).order_by(DtcEvent.raised_at.desc())
    if active_only:
        stmt = stmt.where(DtcEvent.cleared_at.is_(None))
    result = await session.execute(stmt)
    return result.scalars().all()


@router.post("/faults/inject", response_model=FaultInjectionOut, status_code=status.HTTP_201_CREATED)
async def inject_fault(req: FaultInjectRequest):
    fault_type_wire = _FAULT_TYPE_MAP[req.fault_type.value]
    target_sensor_wire = _TARGET_SENSOR_MAP[req.target_sensor.value]

    sent = await ingest_service.send_command(
        CMD_INJECT_FAULT, fault_type=fault_type_wire, target_sensor=target_sensor_wire, value=req.value
    )
    if not sent:
        raise HTTPException(status_code=503, detail="firmware not connected")

    row = await ingest_service.record_fault_injection(
        fault_type=req.fault_type.value, target_sensor=req.target_sensor.value, source="ui"
    )
    return row


@router.post("/faults/clear", status_code=status.HTTP_204_NO_CONTENT)
async def clear_fault(req: FaultClearRequest):
    target_sensor_wire = _TARGET_SENSOR_MAP[req.target_sensor.value]
    sent = await ingest_service.send_command(CMD_CLEAR_FAULT, target_sensor=target_sensor_wire)
    if not sent:
        raise HTTPException(status_code=503, detail="firmware not connected")
    return None


@router.get("/health", response_model=HealthOut)
async def health():
    return HealthOut(status="ok", firmware_connected=ingest_service.firmware_connected)
