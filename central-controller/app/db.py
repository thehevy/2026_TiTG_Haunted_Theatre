from __future__ import annotations

from datetime import datetime, timezone
from typing import Any

from sqlalchemy import DateTime, Integer, String, Text, create_engine, select
from sqlalchemy.orm import DeclarativeBase, Mapped, Session, mapped_column, sessionmaker

from .config import settings


class Base(DeclarativeBase):
    pass


class DeviceStatusRecord(Base):
    __tablename__ = "device_status"

    device_id: Mapped[str] = mapped_column(String(128), primary_key=True)
    payload: Mapped[str] = mapped_column(String(255), nullable=False)
    updated_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)


class EventLogRecord(Base):
    __tablename__ = "event_log"

    id: Mapped[int] = mapped_column(Integer, primary_key=True, autoincrement=True)
    event_type: Mapped[str] = mapped_column(String(64), nullable=False)
    device_id: Mapped[str | None] = mapped_column(String(128), nullable=True)
    payload: Mapped[str] = mapped_column(Text, nullable=False)
    created_at: Mapped[datetime] = mapped_column(DateTime(timezone=True), nullable=False)


class StorageService:
    def __init__(self) -> None:
        self._ready = False
        self._engine = create_engine(settings.database_url, pool_pre_ping=True)
        self._session_factory = sessionmaker(bind=self._engine, expire_on_commit=False)

    def init(self) -> None:
        Base.metadata.create_all(self._engine)
        self._ready = True

    def is_ready(self) -> bool:
        return self._ready

    def upsert_device_status(self, device_id: str, payload: str) -> None:
        now = datetime.now(timezone.utc)
        with self._session_factory() as session:
            row = session.get(DeviceStatusRecord, device_id)
            if row is None:
                row = DeviceStatusRecord(device_id=device_id, payload=payload, updated_at=now)
                session.add(row)
            else:
                row.payload = payload
                row.updated_at = now
            session.commit()

    def list_device_statuses(self) -> dict[str, dict[str, Any]]:
        with self._session_factory() as session:
            rows = session.execute(select(DeviceStatusRecord)).scalars().all()
            return {
                row.device_id: {
                    "payload": row.payload,
                    "updated_at": row.updated_at.isoformat(),
                }
                for row in rows
            }

    def log_event(self, event_type: str, payload: str, device_id: str | None = None) -> None:
        now = datetime.now(timezone.utc)
        with self._session_factory() as session:
            session.add(
                EventLogRecord(
                    event_type=event_type,
                    device_id=device_id,
                    payload=payload,
                    created_at=now,
                )
            )
            session.commit()


storage_service = StorageService()