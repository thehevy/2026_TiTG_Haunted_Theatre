# Haunted House Central Controller

**Created:** 2026-07-30
**Last Updated:** 2026-08-07

## Table of Contents

- [Features](#features)
- [Compatible Device Topic Pattern](#compatible-device-topic-pattern)
- [Quick Start (Rocky Linux)](#quick-start-rocky-linux)
- [Trigger Commands](#trigger-commands)
- [API Examples](#api-examples)
- [Deploy with systemd](#deploy-with-systemd)
- [One-Pass Rocky Linux Deployment](#one-pass-rocky-linux-deployment)
- [Rocky Ops Scripts](#rocky-ops-scripts)
- [PostgreSQL Example](#postgresql-example)

This project is a Rocky Linux friendly controller web app for managing haunted-house device triggers.

## Features

- Web dashboard with quick trigger buttons
- MQTT command publishing for device nodes
- Database-backed live status board from device status messages
- API endpoints for automation tools

## Compatible Device Topic Pattern

This app sends commands to:

- `haunt/<device-id>/trigger`

It listens for status updates on:

- `haunt/+/status`

These match the scaffold sketches in:

- `components/esp32/ESP32_Node.ino`
- `components/esp8266/ESP8266_Node.ino`

## Quick Start (Rocky Linux)

1. Install dependencies:
   - `sudo dnf install -y python3 python3-pip`
2. Create a virtual environment:
   - `python3 -m venv .venv`
   - `source .venv/bin/activate`
3. Install Python packages:
   - `pip install -r requirements.txt`
4. Copy environment file:
   - `cp .env.example .env`
5. Update `.env` values for your MQTT broker.
   - Set `DATABASE_URL` for PostgreSQL or use sqlite default.
6. Run the app:
   - `uvicorn app.main:app --host 0.0.0.0 --port 8080`
7. Open:
   - `http://<server-ip>:8080`

## Trigger Commands

The current node sketches respond to these command strings:

- `relay1:pulse`
- `relay2:pulse`
- `relay3:toggle`

Use the UI or API to publish those commands.

## API Examples

### Publish trigger

- `POST /api/devices/{device_id}/trigger`

Body:

```json
{
  "command": "relay1:pulse"
}
```

### List known device statuses

- `GET /api/devices/status`

### Health check

- `GET /api/health`

## Deploy with systemd

An example service file is included in:

- `systemd/haunt-controller.service`

Copy it to `/etc/systemd/system/`, edit paths, then:

- `sudo systemctl daemon-reload`
- `sudo systemctl enable --now haunt-controller`

## One-Pass Rocky Linux Deployment

Use the included installer to configure dependencies, MQTT broker, app service, and Nginx.

1. Copy this project to your Rocky server.
2. From the `central-controller` directory run:
   - `sudo bash deploy/rocky-install.sh`
3. Edit app settings:
   - `/opt/haunt-controller/.env`
4. Restart app after changing `.env`:
   - `sudo systemctl restart haunt-controller`
5. Open:
   - `http://<server-ip>/`

Installer behavior:

- Installs `python3`, `mosquitto`, and `nginx`
- Creates app user `haunt`
- Copies app to `/opt/haunt-controller`
- Builds `.venv` and installs Python requirements
- Installs and starts `haunt-controller` systemd service
- Installs Nginx proxy config from `nginx/haunt-controller.conf`

## Rocky Ops Scripts

Use these helper scripts after install:

- `sudo bash deploy/postgres-bootstrap.sh haunt haunt`
- `sudo bash deploy/rocky-harden.sh`
- `sudo bash deploy/rocky-harden.sh --allow-mqtt`
- `sudo bash deploy/mqtt-bootstrap.sh haunt-device`

The MQTT bootstrap script prints generated credentials if no password is provided.
The PostgreSQL bootstrap script prints generated credentials if no password is provided.

## PostgreSQL Example

Example local PostgreSQL URL in `.env`:

`DATABASE_URL=postgresql+psycopg://haunt:change-me@127.0.0.1:5432/haunt`

The app auto-creates tables on startup:

- `device_status`
- `event_log`

Suggested Rocky setup order:

1. `sudo bash deploy/rocky-install.sh`
2. `sudo bash deploy/postgres-bootstrap.sh haunt haunt`
3. `sudo bash deploy/mqtt-bootstrap.sh haunt-device`
4. `sudo bash deploy/rocky-harden.sh`
5. Update `/opt/haunt-controller/.env`
6. `sudo systemctl restart haunt-controller`
