from datetime import datetime, timezone

from sqlalchemy import Boolean, DateTime, Float, Integer, String
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column


def _utcnow() -> datetime:
    return datetime.now(timezone.utc)


class Base(DeclarativeBase):
    pass


class Telemetry(Base):
    __tablename__ = "telemetry"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    cycle_id: Mapped[int] = mapped_column(Integer, index=True)
    timestamp_ms: Mapped[int] = mapped_column(Integer)
    pedal_pct: Mapped[float] = mapped_column(Float)
    throttle_pct: Mapped[float] = mapped_column(Float)
    control_error: Mapped[float] = mapped_column(Float)
    actuator_duty_pct: Mapped[float] = mapped_column(Float)
    active_dtc_mask: Mapped[int] = mapped_column(Integer)
    failsafe_active: Mapped[bool] = mapped_column(Boolean)
    recorded_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow, index=True)


class DtcEvent(Base):
    __tablename__ = "dtc_events"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    code: Mapped[int] = mapped_column(Integer, index=True)
    name: Mapped[str] = mapped_column(String(64))
    raised_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)
    cleared_at: Mapped[datetime | None] = mapped_column(DateTime(timezone=True), nullable=True, default=None)
    cause: Mapped[str | None] = mapped_column(String(256), nullable=True, default=None)


class FaultInjection(Base):
    __tablename__ = "fault_injections"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    fault_type: Mapped[str] = mapped_column(String(32))
    target_sensor: Mapped[str | None] = mapped_column(String(32), nullable=True)
    triggered_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), default=_utcnow)
    source: Mapped[str] = mapped_column(String(16))  # "harness" or "ui"
