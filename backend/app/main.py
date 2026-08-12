import logging
from contextlib import asynccontextmanager

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.middleware.cors import CORSMiddleware

from .database import init_db
from .routers.telemetry import router as telemetry_router
from .serial_ingest import ingest_service
from .ws import manager

logging.basicConfig(level=logging.INFO)


@asynccontextmanager
async def lifespan(app: FastAPI):
    await init_db()
    ingest_service.start()
    yield


app = FastAPI(title="Helm Telemetry Service", lifespan=lifespan)

# The frontend dev server runs on a different origin (Vite default :5173);
# in production both are served from the same docker-compose network but
# still typically different origins (browser -> host-mapped ports), so keep
# this permissive rather than hardcoding a specific origin.
app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(telemetry_router)


@app.websocket("/ws/telemetry")
async def ws_telemetry(websocket: WebSocket):
    await manager.connect(websocket)
    try:
        while True:
            # This endpoint is push-only; just keep the connection open and
            # drain (and ignore) anything the client sends.
            await websocket.receive_text()
    except WebSocketDisconnect:
        pass
    finally:
        manager.disconnect(websocket)
