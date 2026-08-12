async def test_health_reports_firmware_disconnected_by_default(client):
    resp = await client.get("/health")
    assert resp.status_code == 200
    body = resp.json()
    assert body["status"] == "ok"
    assert body["firmware_connected"] is False
