from datetime import datetime
from enum import Enum

from pydantic import BaseModel, ConfigDict


class TelemetryOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    cycle_id: int
    timestamp_ms: int
    pedal_pct: float
    throttle_pct: float
    control_error: float
    actuator_duty_pct: float
    active_dtc_mask: int
    failsafe_active: bool
    recorded_at: datetime


class DtcEventOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    code: int
    name: str
    raised_at: datetime
    cleared_at: datetime | None
    cause: str | None


class FaultTypeName(str, Enum):
    stuck = "stuck"
    out_of_range = "out_of_range"
    mismatch = "mismatch"
    comms_dropout = "comms_dropout"


class TargetSensorName(str, Enum):
    pedal_a = "pedal_a"
    pedal_b = "pedal_b"
    throttle_a = "throttle_a"
    throttle_b = "throttle_b"


class FaultInjectRequest(BaseModel):
    fault_type: FaultTypeName
    target_sensor: TargetSensorName
    value: float = 0.0


class FaultClearRequest(BaseModel):
    target_sensor: TargetSensorName


class FaultInjectionOut(BaseModel):
    model_config = ConfigDict(from_attributes=True)

    id: int
    fault_type: str
    target_sensor: str | None
    triggered_at: datetime
    source: str


class HealthOut(BaseModel):
    status: str
    firmware_connected: bool
