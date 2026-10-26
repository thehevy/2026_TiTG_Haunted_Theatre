import os
import sys
import tempfile
from pathlib import Path
from types import SimpleNamespace

import pytest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
os.chdir(ROOT)  # the app loads templates/ relative to the working directory
os.environ.update(
    {
        "DATABASE_URL": "sqlite:///" + str(Path(tempfile.mkdtemp()) / "test.db").replace("\\", "/"),
        "MQTT_HOST": "127.0.0.1",
        "MQTT_PORT": "1",  # nothing listens here, so the broker stays disconnected
        "ADMIN_USERNAME": "admin",
        "ADMIN_PASSWORD": "test-pass",
    }
)

from fastapi.testclient import TestClient  # noqa: E402

from app.main import app  # noqa: E402
from app.mqtt_client import mqtt_service  # noqa: E402

AUTH = ("admin", "test-pass")


@pytest.fixture(scope="module")
def client():
    with TestClient(app) as c:
        yield c


def deliver(topic: str, payload: str) -> None:
    mqtt_service._on_message(None, None, SimpleNamespace(topic=topic, payload=payload.encode()))


def test_health_is_open(client):
    r = client.get("/api/health")
    assert r.status_code == 200
    assert r.json()["admin_password_set"] is True


@pytest.mark.parametrize("path", ["/", "/api/devices/status"])
def test_pages_need_login(client, path):
    assert client.get(path).status_code == 401
    assert client.get(path, auth=("admin", "wrong")).status_code == 401
    assert client.get(path, auth=AUTH).status_code == 200


def test_trigger_needs_login(client):
    assert client.post("/api/devices/haunt-1a2b3c/trigger", json={"command": "main"}).status_code == 401


@pytest.mark.parametrize("device_id", ["bad_id", "-lead", "trail-", "a" * 33, "%2B", "%23"])
def test_trigger_rejects_bad_device_id(client, device_id):
    r = client.post(f"/api/devices/{device_id}/trigger", json={"command": "main"}, auth=AUTH)
    assert r.status_code in (404, 422)


@pytest.mark.parametrize("command", ["relay1:pulse", "MAIN", "", "main; aux"])
def test_trigger_rejects_unknown_command(client, command):
    r = client.post("/api/devices/haunt-1a2b3c/trigger", json={"command": command}, auth=AUTH)
    assert r.status_code == 422


def test_trigger_requires_json(client):
    r = client.post(
        "/api/devices/haunt-1a2b3c/trigger",
        content='{"command":"main"}',
        headers={"Content-Type": "text/plain"},
        auth=AUTH,
    )
    assert r.status_code in (415, 422)


def test_trigger_reports_broker_down(client):
    r = client.post("/api/devices/haunt-1a2b3c/trigger", json={"command": "main"}, auth=AUTH)
    assert r.status_code == 503


def test_mqtt_messages_update_dashboard(client):
    deliver("haunt/prop-01/status", "online")
    deliver("haunt/prop-01/heartbeat", '{"name":"Coffin","ir_fired":3,"ir_faults":1}')
    deliver("haunt/prop-01/event", '{"src":"ir","fired":true,"ms":500,"detail":"laser tag"}')
    deliver("haunt/bad_id/status", "online")
    deliver("haunt/prop-01/extra/status", "online")
    deliver("other/prop-02/status", "online")

    devices = client.get("/api/devices/status", auth=AUTH).json()["devices"]
    assert set(devices) == {"prop-01"}
    prop = devices["prop-01"]
    assert prop["payload"] == "online"
    assert prop["heartbeat"]["ir_faults"] == 1
    assert prop["last_event"]["src"] == "ir"

    page = client.get("/", auth=AUTH).text
    assert "Coffin" in page and "1 faults" in page


def test_dashboard_escapes_device_data(client):
    deliver("haunt/prop-03/heartbeat", '{"name":"<script>alert(1)</script>"}')
    page = client.get("/", auth=AUTH).text
    assert "<script>alert(1)</script>" not in page
    assert "&lt;script&gt;" in page
