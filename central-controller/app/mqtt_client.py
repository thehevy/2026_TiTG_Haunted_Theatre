from __future__ import annotations

import json
import logging
import re
import threading
from dataclasses import dataclass, field, replace
from datetime import datetime, timezone
from typing import Any, Dict

import paho.mqtt.client as mqtt

from .config import settings
from .db import storage_service
from .models import DEVICE_ID_PATTERN

log = logging.getLogger(__name__)

DEVICE_ID_RE = re.compile(DEVICE_ID_PATTERN)
MAX_PAYLOAD_BYTES = 2048
STATUS_MAX_CHARS = 255


@dataclass
class DeviceStatus:
    device_id: str
    payload: str
    updated_at: str
    heartbeat: Dict[str, Any] = field(default_factory=dict)
    heartbeat_at: str = ""
    last_event: Dict[str, Any] = field(default_factory=dict)
    last_event_at: str = ""


def parse_json_object(payload: str) -> Dict[str, Any]:
    try:
        data = json.loads(payload)
    except ValueError:
        return {}
    return data if isinstance(data, dict) else {}


class MQTTService:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._connected = False
        self._status_by_device: Dict[str, DeviceStatus] = {}

        self.client = mqtt.Client(mqtt.CallbackAPIVersion.VERSION2)
        if settings.mqtt_username:
            self.client.username_pw_set(
                settings.mqtt_username,
                password=settings.mqtt_password,
            )
        self.client.reconnect_delay_set(min_delay=1, max_delay=30)

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def start(self) -> None:
        # connect_async + loop_start keeps retrying in the background, so a broker that is down at boot is not fatal.
        self.client.connect_async(settings.mqtt_host, settings.mqtt_port, keepalive=30)
        self.client.loop_start()

    def stop(self) -> None:
        self.client.disconnect()
        self.client.loop_stop()

    def publish_trigger(self, device_id: str, command: str) -> tuple[str, bool]:
        if not DEVICE_ID_RE.match(device_id):
            raise ValueError("invalid device_id")
        topic = f"{settings.mqtt_topic_prefix}/{device_id}/trigger"
        result = self.client.publish(topic, command, qos=1)
        if storage_service.is_ready():
            try:
                storage_service.log_event("trigger_command", command, device_id=device_id)
            except Exception:
                log.exception("Could not log trigger command")
        return topic, result.rc == mqtt.MQTT_ERR_SUCCESS

    def device_statuses(self) -> Dict[str, DeviceStatus]:
        with self._lock:
            return {device_id: replace(status) for device_id, status in self._status_by_device.items()}

    def is_connected(self) -> bool:
        return self._connected

    def _on_connect(self, client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
        self._connected = not reason_code.is_failure
        if reason_code.is_failure:
            log.warning("MQTT connect refused: %s", reason_code)
            return
        prefix = settings.mqtt_topic_prefix
        client.subscribe([(f"{prefix}/+/status", 1), (f"{prefix}/+/event", 1), (f"{prefix}/+/heartbeat", 0)])

    def _on_disconnect(self, client: mqtt.Client, userdata, disconnect_flags, reason_code, properties) -> None:
        self._connected = False

    def _on_message(self, client: mqtt.Client, userdata, message: mqtt.MQTTMessage) -> None:
        parts = message.topic.split("/")
        if len(parts) != 3 or parts[0] != settings.mqtt_topic_prefix or len(message.payload) > MAX_PAYLOAD_BYTES:
            return
        _, device_id, kind = parts
        if not DEVICE_ID_RE.match(device_id) or kind not in ("status", "event", "heartbeat"):
            return

        payload = message.payload.decode("utf-8", errors="replace")
        now = datetime.now(timezone.utc).isoformat()

        with self._lock:
            status = self._status_by_device.get(device_id)
            if status is None:
                status = DeviceStatus(device_id=device_id, payload="", updated_at=now)
                self._status_by_device[device_id] = status
            if kind == "status":
                status.payload = payload[:STATUS_MAX_CHARS]
                status.updated_at = now
            elif kind == "heartbeat":
                status.heartbeat = parse_json_object(payload)
                status.heartbeat_at = now
            else:
                status.last_event = parse_json_object(payload)
                status.last_event_at = now

        if not storage_service.is_ready():
            return
        try:
            if kind == "status":
                storage_service.upsert_device_status(device_id=device_id, payload=payload[:STATUS_MAX_CHARS])
                storage_service.log_event("status_update", payload, device_id=device_id)
            elif kind == "event":
                storage_service.log_event("device_event", payload, device_id=device_id)
        except Exception:
            log.exception("Could not store MQTT %s from %s", kind, device_id)


mqtt_service = MQTTService()
