from __future__ import annotations

import secrets
from contextlib import asynccontextmanager
from typing import Any

from fastapi import Depends, FastAPI, HTTPException, Path, Request
from fastapi.responses import HTMLResponse
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from fastapi.templating import Jinja2Templates

from .config import settings
from .db import storage_service
from .models import COMMANDS, DEVICE_ID_PATTERN, TriggerRequest, TriggerResponse
from .mqtt_client import mqtt_service


@asynccontextmanager
async def lifespan(app: FastAPI):
    storage_service.init()
    mqtt_service.start()
    yield
    mqtt_service.stop()


app = FastAPI(title="Haunted House Central Controller", version="0.2.0", lifespan=lifespan)
templates = Jinja2Templates(directory="templates")
basic_auth = HTTPBasic(realm="Haunt controller")


def require_admin(credentials: HTTPBasicCredentials = Depends(basic_auth)) -> None:
    if not settings.admin_password:
        raise HTTPException(status_code=503, detail="ADMIN_PASSWORD is not set in .env")
    user_ok = secrets.compare_digest(credentials.username.encode(), settings.admin_username.encode())
    password_ok = secrets.compare_digest(credentials.password.encode(), settings.admin_password.encode())
    if not (user_ok and password_ok):
        raise HTTPException(status_code=401, detail="Invalid login", headers={"WWW-Authenticate": "Basic"})


@app.middleware("http")
async def security_headers(request: Request, call_next):
    response = await call_next(request)
    response.headers["X-Frame-Options"] = "DENY"
    response.headers["X-Content-Type-Options"] = "nosniff"
    response.headers["Referrer-Policy"] = "no-referrer"
    response.headers["Cache-Control"] = "no-store"
    return response


def collect_devices() -> dict[str, dict[str, Any]]:
    stored = storage_service.list_device_statuses() if storage_service.is_ready() else {}
    live = mqtt_service.device_statuses()
    devices: dict[str, dict[str, Any]] = {}
    for device_id in sorted(set(stored) | set(live)):
        row = stored.get(device_id, {})
        status = live.get(device_id)
        devices[device_id] = {
            "payload": (status.payload if status and status.payload else row.get("payload", "")),
            "updated_at": (status.updated_at if status and status.payload else row.get("updated_at", "")),
            "heartbeat": status.heartbeat if status else {},
            "heartbeat_at": status.heartbeat_at if status else "",
            "last_event": status.last_event if status else {},
            "last_event_at": status.last_event_at if status else "",
        }
    return devices


@app.get("/", response_class=HTMLResponse, dependencies=[Depends(require_admin)])
def dashboard(request: Request) -> HTMLResponse:
    return templates.TemplateResponse(
        request=request,
        name="index.html",
        context={
            "statuses": collect_devices(),
            "mqtt_connected": mqtt_service.is_connected(),
            "commands": COMMANDS,
        },
    )


@app.post("/api/devices/{device_id}/trigger", response_model=TriggerResponse, dependencies=[Depends(require_admin)])
def trigger_device(
    request: Request,
    payload: TriggerRequest,
    device_id: str = Path(pattern=DEVICE_ID_PATTERN),
) -> TriggerResponse:
    # A JSON body cannot be sent cross-site without a CORS preflight, which this app never allows.
    if not request.headers.get("content-type", "").startswith("application/json"):
        raise HTTPException(status_code=415, detail="Content-Type must be application/json")
    if not mqtt_service.is_connected():
        raise HTTPException(status_code=503, detail="MQTT broker is not connected")

    topic, published = mqtt_service.publish_trigger(device_id=device_id, command=payload.command)
    return TriggerResponse(
        device_id=device_id,
        topic=topic,
        command=payload.command,
        published=published,
    )


@app.get("/api/devices/status", dependencies=[Depends(require_admin)])
def list_device_statuses() -> dict:
    return {"devices": collect_devices()}


@app.get("/api/health")
def health() -> dict:
    return {
        "ok": True,
        "database_ready": storage_service.is_ready(),
        "mqtt_connected": mqtt_service.is_connected(),
        "admin_password_set": bool(settings.admin_password),
    }
