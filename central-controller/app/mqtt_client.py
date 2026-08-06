from __future__ import annotations

import threading
from dataclasses import dataclass
from datetime import datetime, timezone
from typing import Dict

import paho.mqtt.client as mqtt

from .config import settings
from .db import storage_service


@dataclass
class DeviceStatus:
    device_id: str
    payload: str
    updated_at: str


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

        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message

    def start(self) -> None:
        self.client.connect(settings.mqtt_host, settings.mqtt_port, keepalive=30)
        self.client.loop_start()

    def stop(self) -> None:
        self.client.loop_stop()
        self.client.disconnect()

    def publish_trigger(self, device_id: str, command: str) -> tuple[str, bool]:
        topic = f"{settings.mqtt_topic_prefix}/{device_id}/trigger"
        result = self.client.publish(topic, command, qos=1)
        if storage_service.is_ready():
            try:
                storage_service.log_event("trigger_command", command, device_id=device_id)
            except Exception:
                pass
        return topic, result.rc == mqtt.MQTT_ERR_SUCCESS

    def device_statuses(self) -> Dict[str, DeviceStatus]:
        with self._lock:
            return dict(self._status_by_device)

    def is_connected(self) -> bool:
        return self._connected

    def _on_connect(self, client: mqtt.Client, userdata, flags, reason_code, properties) -> None:
        self._connected = reason_code == 0
        status_topic = f"{settings.mqtt_topic_prefix}/+/status"
        client.subscribe(status_topic, qos=1)

    def _on_disconnect(self, client: mqtt.Client, userdata, disconnect_flags, reason_code, properties) -> None:
        self._connected = False

    def _on_message(self, client: mqtt.Client, userdata, message: mqtt.MQTTMessage) -> None:
        topic = message.topic
        payload = message.payload.decode("utf-8", errors="replace")

        parts = topic.split("/")
        if len(parts) < 3:
            return

        device_id = parts[1]
        now = datetime.now(timezone.utc).isoformat()

        with self._lock:
            self._status_by_device[device_id] = DeviceStatus(
                device_id=device_id,
                payload=payload,
                updated_at=now,
            )

        if storage_service.is_ready():
            try:
                storage_service.upsert_device_status(device_id=device_id, payload=payload)
                storage_service.log_event("status_update", payload, device_id=device_id)
            except Exception:
                pass


mqtt_service = MQTTService()
