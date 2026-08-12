from app.protocol import DTC_COMMS_TIMEOUT, DTC_PEDAL_PLAUSIBILITY
from app.serial_ingest import ingest_service
from test_telemetry_ingestion import make_frame


async def test_dtc_event_raised_and_cleared(client):
    await ingest_service._on_telemetry(make_frame(cycle_id=1, dtc_mask=0))
    await ingest_service._on_telemetry(make_frame(cycle_id=2, dtc_mask=DTC_PEDAL_PLAUSIBILITY, failsafe=1))

    resp = await client.get("/dtcs")
    assert resp.status_code == 200
    events = resp.json()
    assert len(events) == 1
    assert events[0]["code"] == DTC_PEDAL_PLAUSIBILITY
    assert events[0]["name"] == "DTC_PEDAL_PLAUSIBILITY"
    assert events[0]["cleared_at"] is None

    resp = await client.get("/dtcs?active_only=true")
    assert len(resp.json()) == 1

    await ingest_service._on_telemetry(make_frame(cycle_id=3, dtc_mask=0))

    resp = await client.get("/dtcs?active_only=true")
    assert resp.json() == []

    resp = await client.get("/dtcs")
    events = resp.json()
    assert len(events) == 1
    assert events[0]["cleared_at"] is not None


async def test_multiple_simultaneous_dtcs_tracked_independently(client):
    await ingest_service._on_telemetry(make_frame(cycle_id=1, dtc_mask=0))
    await ingest_service._on_telemetry(
        make_frame(cycle_id=2, dtc_mask=DTC_PEDAL_PLAUSIBILITY | DTC_COMMS_TIMEOUT, failsafe=1)
    )

    resp = await client.get("/dtcs?active_only=true")
    codes = {e["code"] for e in resp.json()}
    assert codes == {DTC_PEDAL_PLAUSIBILITY, DTC_COMMS_TIMEOUT}

    # Clear only the comms-timeout bit -- pedal plausibility should stay open.
    await ingest_service._on_telemetry(make_frame(cycle_id=3, dtc_mask=DTC_PEDAL_PLAUSIBILITY, failsafe=1))

    resp = await client.get("/dtcs?active_only=true")
    codes = {e["code"] for e in resp.json()}
    assert codes == {DTC_PEDAL_PLAUSIBILITY}
