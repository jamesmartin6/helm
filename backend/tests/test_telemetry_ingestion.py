from app.protocol import TelemetryFrame
from app.serial_ingest import ingest_service


def make_frame(cycle_id: int, dtc_mask: int = 0, failsafe: int = 0) -> TelemetryFrame:
    return TelemetryFrame(
        cycle_id=cycle_id,
        timestamp_ms=cycle_id * 10,
        pedal_raw_a=2048,
        pedal_raw_b=2051,
        throttle_raw_a=2000,
        throttle_raw_b=2003,
        pedal_pct=50.0,
        throttle_pct=48.9,
        control_error=1.1,
        actuator_duty_pct=5.0,
        active_dtc_mask=dtc_mask,
        failsafe_active=failsafe,
    )


async def test_telemetry_latest_after_ingest(client):
    resp = await client.get("/telemetry/latest")
    assert resp.status_code == 404  # nothing ingested yet

    await ingest_service._on_telemetry(make_frame(cycle_id=1))

    resp = await client.get("/telemetry/latest")
    assert resp.status_code == 200
    body = resp.json()
    assert body["cycle_id"] == 1
    assert body["pedal_pct"] == 50.0
    assert body["failsafe_active"] is False


async def test_telemetry_history_most_recent_first(client):
    for cycle_id in range(1, 6):
        await ingest_service._on_telemetry(make_frame(cycle_id=cycle_id))

    resp = await client.get("/telemetry?limit=3")
    assert resp.status_code == 200
    rows = resp.json()
    assert len(rows) == 3
    assert [r["cycle_id"] for r in rows] == [5, 4, 3]


async def test_telemetry_since_filters_by_time(client):
    import datetime

    await ingest_service._on_telemetry(make_frame(cycle_id=1))

    future = (datetime.datetime.now(datetime.timezone.utc) + datetime.timedelta(seconds=10)).isoformat()
    resp = await client.get("/telemetry", params={"since": future})
    assert resp.status_code == 200
    assert resp.json() == []
