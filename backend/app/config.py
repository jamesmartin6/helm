from pydantic_settings import BaseSettings, SettingsConfigDict


class Settings(BaseSettings):
    model_config = SettingsConfigDict(env_prefix="HELM_", env_file=".env", extra="ignore")

    # SQLAlchemy async URL. Defaults to an on-disk SQLite file for zero-setup
    # local dev; docker-compose overrides this to the Postgres service.
    database_url: str = "sqlite+aiosqlite:///./helm.db"

    # TCP address of the firmware's UART bridge (see firmware/... and
    # sil-harness/qemu_runner.py for why this is a socket, not a real serial
    # port, on this project).
    firmware_host: str = "127.0.0.1"
    firmware_port: int = 5678

    # If the firmware isn't reachable at startup, keep retrying at this
    # interval rather than crashing the whole service.
    firmware_reconnect_interval_s: float = 2.0

    # How many recent telemetry rows to keep in the in-memory latest-frame
    # cache (used to answer /telemetry/latest and seed new WS clients).
    telemetry_history_limit: int = 1000


settings = Settings()
