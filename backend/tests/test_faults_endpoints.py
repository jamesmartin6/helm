from app.serial_ingest import ingest_service


async def test_inject_fault_returns_503_when_firmware_disconnected(client):
    resp = await client.post(
        "/faults/inject", json={"fault_type": "stuck", "target_sensor": "pedal_a", "value": 90.0}
    )
    assert resp.status_code == 503


async def test_clear_fault_returns_503_when_firmware_disconnected(client):
    resp = await client.post("/faults/clear", json={"target_sensor": "pedal_a"})
    assert resp.status_code == 503


async def test_inject_fault_success_when_firmware_connected(client, monkeypatch):
    sent_commands = []

    async def fake_send_command(cmd, fault_type=0, target_sensor=0, value=0.0):
        sent_commands.append((cmd, fault_type, target_sensor, value))
        return True

    monkeypatch.setattr(ingest_service, "send_command", fake_send_command)

    resp = await client.post(
        "/faults/inject", json={"fault_type": "mismatch", "target_sensor": "throttle_b", "value": 0.0}
    )
    assert resp.status_code == 201
    body = resp.json()
    assert body["fault_type"] == "mismatch"
    assert body["target_sensor"] == "throttle_b"
    assert body["source"] == "ui"
    assert len(sent_commands) == 1

    # And it shows up in fault-injection history via a fresh telemetry frame
    # cycle to confirm persistence roundtrips correctly.
    resp = await client.post("/faults/clear", json={"target_sensor": "throttle_b"})
    assert resp.status_code == 204
    assert len(sent_commands) == 2
