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

1. Enable the Rocky package prerequisites the first time on a fresh host:
   - `sudo dnf install -y epel-release`
   - `sudo dnf install -y python3 python3-pip python3-virtualenv git curl NetworkManager NetworkManager-wifi NetworkManager-config-server mosquitto nginx`
2. Validate the network stack before continuing:
   - `nmcli radio`
   - `nmcli device status`
   - `nmtui` if the Wi-Fi interface is not active
3. Create a virtual environment:
   - `python3 -m venv .venv`
   - `source .venv/bin/activate`
4. Install Python packages:
   - `pip install -r requirements.txt`
5. Copy environment file:
   - `cp .env.example .env`
6. Update `.env` values for your MQTT broker and PostgreSQL database.
   - Keep `DATABASE_URL` using password auth such as `scram-sha-256`.
7. Run the app:
   - `uvicorn app.main:app --host 0.0.0.0 --port 8080`
8. Open:
   - `http://<server-ip>:8080`

## Rocky Pre-Flight Checks

On a fresh Rocky Linux host, confirm the OS is ready before running the service installer:

- `sudo dnf install -y epel-release`
- `sudo dnf install -y NetworkManager NetworkManager-wifi NetworkManager-config-server`
- `nmcli radio`
- `nmcli device status`
- `hostnamectl`
- `systemctl status NetworkManager`

If Wi-Fi is not available, repair the connection first and ensure the interface is managed by NetworkManager before starting the app.

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

1. Enable Rocky EPEL and the required base packages on a fresh host:
   - `sudo dnf install -y epel-release`
   - `sudo dnf install -y NetworkManager NetworkManager-wifi NetworkManager-config-server`
2. Copy this project to your Rocky server.
3. From the `central-controller` directory run:
   - `sudo bash deploy/rocky-install.sh`
4. Edit app settings:
   - `/opt/haunt-controller/.env`
5. Restart app after changing `.env`:
   - `sudo systemctl restart haunt-controller`
6. Verify the service is healthy:
   - `systemctl status haunt-controller`
   - `curl http://127.0.0.1:8080/api/health`
7. Open:
   - `http://<server-ip>/`

Installer behavior:

- Installs EPEL, Python, Mosquitto, Nginx, and NetworkManager Wi-Fi support
- Creates app user `haunt`
- Copies app to `/opt/haunt-controller`
- Builds `.venv` and installs Python requirements
- Installs and starts `haunt-controller` systemd service
- Installs Nginx proxy config from `nginx/haunt-controller.conf`
- Performs a final health check at the end of the install

## Verify Installation

After the script completes, confirm the platform is up before continuing:

```bash
systemctl status haunt-controller
systemctl status mosquitto
systemctl status nginx
curl http://127.0.0.1:8080/api/health
```

If the health check fails, review the app logs:

```bash
journalctl -u haunt-controller -n 100 --no-pager
```

If PostgreSQL login fails, confirm `pg_hba.conf` allows password auth:

```text
host all all 127.0.0.1/32 scram-sha-256
```

and reload PostgreSQL before restarting the app.

## Rocky Ops Scripts

Use these helper scripts after install:

- `sudo bash deploy/postgres-bootstrap.sh haunt haunt 'yourStrongPassword'`
- `sudo bash deploy/rocky-harden.sh`
- `sudo bash deploy/rocky-harden.sh --allow-mqtt`
- `sudo bash deploy/mqtt-bootstrap.sh haunt-device 'yourStrongPassword'`

Both bootstrap scripts require an explicit password on the command line. If the password is omitted, the script exits with an error and prints the exact corrective command to rerun.

## PostgreSQL Example

Example local PostgreSQL URL in `.env`:

`DATABASE_URL=postgresql+psycopg://haunt:yourStrongPassword@127.0.0.1:5432/haunt`

The app auto-creates tables on startup:

- `device_status`
- `event_log`

Suggested Rocky setup order:

1. `sudo bash deploy/rocky-install.sh`
2. `sudo bash deploy/postgres-bootstrap.sh haunt haunt 'yourStrongPassword'`
3. Update `/opt/haunt-controller/.env` with the matching PostgreSQL password
4. `sudo bash deploy/mqtt-bootstrap.sh haunt-device 'yourStrongPassword'`
5. `sudo bash deploy/rocky-harden.sh`
6. `sudo systemctl restart haunt-controller`
