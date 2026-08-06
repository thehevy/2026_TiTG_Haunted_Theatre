from __future__ import annotations

from fastapi import FastAPI, HTTPException, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates

from .db import storage_service
from .models import TriggerRequest, TriggerResponse
from .mqtt_client import mqtt_service


app = FastAPI(title="Haunted House Central Controller", version="0.1.0")
templates = Jinja2Templates(directory="templates")


@app.on_event("startup")
def startup_event() -> None:
    storage_service.init()
    mqtt_service.start()


@app.on_event("shutdown")
def shutdown_event() -> None:
    mqtt_service.stop()


@app.get("/", response_class=HTMLResponse)
def dashboard(request: Request) -> HTMLResponse:
    statuses = storage_service.list_device_statuses() if storage_service.is_ready() else {}
    if not statuses:
        statuses = {
            device_id: {
                "payload": status.payload,
                "updated_at": status.updated_at,
            }
            for device_id, status in mqtt_service.device_statuses().items()
        }

    return templates.TemplateResponse(
        request=request,
        name="index.html",
        context={
            "statuses": statuses,
            "mqtt_connected": mqtt_service.is_connected(),
            "commands": ["relay1:pulse", "relay2:pulse", "relay3:toggle"],
        },
    )


@app.post("/api/devices/{device_id}/trigger", response_model=TriggerResponse)
def trigger_device(device_id: str, payload: TriggerRequest) -> TriggerResponse:
    if not mqtt_service.is_connected():
        raise HTTPException(status_code=503, detail="MQTT broker is not connected")

    topic, published = mqtt_service.publish_trigger(device_id=device_id, command=payload.command)
    return TriggerResponse(
        device_id=device_id,
        topic=topic,
        command=payload.command,
        published=published,
    )


@app.get("/api/devices/status")
def list_device_statuses() -> dict:
    data = storage_service.list_device_statuses() if storage_service.is_ready() else {}
    if not data:
        data = {
            device_id: {
                "payload": status.payload,
                "updated_at": status.updated_at,
            }
            for device_id, status in mqtt_service.device_statuses().items()
        }

    return {"devices": data}


@app.get("/api/health")
def health() -> dict:
    known_devices = len(storage_service.list_device_statuses()) if storage_service.is_ready() else len(mqtt_service.device_statuses())
    return {
        "ok": True,
        "database_ready": storage_service.is_ready(),
        "mqtt_connected": mqtt_service.is_connected(),
        "known_devices": known_devices,
    }
