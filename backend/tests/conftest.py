import sys
from pathlib import Path

BACKEND_DIR = Path(__file__).resolve().parent.parent
if str(BACKEND_DIR) not in sys.path:
    sys.path.insert(0, str(BACKEND_DIR))

import pytest_asyncio  # noqa: E402
from httpx import ASGITransport, AsyncClient  # noqa: E402
from sqlalchemy.ext.asyncio import async_sessionmaker, create_async_engine  # noqa: E402
from sqlalchemy.pool import StaticPool  # noqa: E402

from app.database import get_session  # noqa: E402
from app.main import app  # noqa: E402
from app.models import Base  # noqa: E402
from app.serial_ingest import ingest_service  # noqa: E402


@pytest_asyncio.fixture
async def db_engine():
    """A fresh in-memory SQLite DB per test. StaticPool keeps the same
    in-memory connection alive across the multiple sessions FastAPI's
    dependency injection opens during a test (a plain NullPool would give
    each connection its own throwaway :memory: database)."""
    engine = create_async_engine(
        "sqlite+aiosqlite:///:memory:",
        poolclass=StaticPool,
        connect_args={"check_same_thread": False},
    )
    async with engine.begin() as conn:
        await conn.run_sync(Base.metadata.create_all)
    yield engine
    await engine.dispose()


@pytest_asyncio.fixture
async def client(db_engine):
    """An AsyncClient wired to the real FastAPI app, with get_session
    overridden to use the per-test in-memory DB. Deliberately does NOT run
    the app's lifespan (which would try to open a real socket to the
    firmware) -- ingest_service state is reset directly instead, and tests
    that need to simulate a decoded frame call ingest_service._on_telemetry
    directly rather than going through a real UART connection."""
    session_maker = async_sessionmaker(db_engine, expire_on_commit=False)

    async def override_get_session():
        async with session_maker() as session:
            yield session

    app.dependency_overrides[get_session] = override_get_session

    # Reset shared ingest_service state between tests.
    ingest_service._last_dtc_mask = 0
    ingest_service._latest = None
    ingest_service.link._writer = None
    ingest_service.link.connected = False

    # Patch the session maker ingest_service itself uses (it normally imports
    # the module-level one tied to the real database URL) so a simulated
    # _on_telemetry call writes into this test's in-memory DB too.
    import app.serial_ingest as serial_ingest_module

    original_session_maker = serial_ingest_module.async_session_maker
    serial_ingest_module.async_session_maker = session_maker

    transport = ASGITransport(app=app)
    async with AsyncClient(transport=transport, base_url="http://test") as ac:
        yield ac

    serial_ingest_module.async_session_maker = original_session_maker
    app.dependency_overrides.clear()
