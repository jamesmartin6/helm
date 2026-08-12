# Helm Backend

FastAPI telemetry service: ingests the firmware's live UART telemetry
stream (over a TCP socket bridge, see below), persists it, and exposes it
to the frontend over REST + WebSocket.

## Setup

```
python -m venv .venv
.venv/Scripts/pip install -r requirements.txt   # .venv/bin/pip on POSIX
```

Requires a *standard* CPython build with prebuilt PyPI wheels available
(`pydantic-core` and `uvicorn[standard]`'s `watchfiles` are Rust extensions
with no source-build path on unusual interpreter builds). If you're on this
project's dev machine: don't use the MSYS2 `ucrt64` Python for this --
its nonstandard wheel tag forces source builds that fail to link. Use a
normal python.org / Microsoft Store / pyenv-managed CPython instead.

Copy `.env.example` to `.env` and adjust if you want Postgres instead of
the SQLite default, or a non-default firmware host/port.

## Running

Build and start the firmware first (see `../firmware/README` via
`progress.md`), then:

```
.venv/Scripts/python -m uvicorn app.main:app --reload
```

`GET /health` reports `firmware_connected` -- the service starts fine even
if the firmware isn't up yet, and reconnects automatically once it is.

## Testing

```
.venv/Scripts/python -m pytest -v
```

Unit tests run against an in-memory SQLite DB and simulate telemetry frames
directly (`ingest_service._on_telemetry(...)`) rather than a real socket, so
they're fast and don't need the firmware running. For real end-to-end
verification (real Postgres + live QEMU firmware + WebSocket streaming),
this project's `progress.md` documents a manual integration pass -- see the
Phase 4 section there for exact commands and results.

## API

| Method | Path | Notes |
|---|---|---|
| GET | `/telemetry?since=&limit=` | paginated, most recent first |
| GET | `/telemetry/latest` | current state |
| GET | `/dtcs?active_only=` | DTC event history |
| POST | `/faults/inject` | `{fault_type, target_sensor, value}` -> 201 |
| POST | `/faults/clear` | `{target_sensor}` -> 204 |
| GET | `/health` | `{status, firmware_connected}` |
| WS | `/ws/telemetry` | pushes `{type:"telemetry",...}` and `{type:"dtc_change",...}` |

`fault_type`: `stuck` \| `out_of_range` \| `mismatch` \| `comms_dropout`.
`target_sensor`: `pedal_a` \| `pedal_b` \| `throttle_a` \| `throttle_b`.
