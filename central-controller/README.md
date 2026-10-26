# Haunted House Central Controller

**Created:** 2026-07-30
**Last Updated:** 2026-10-26

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

- Web dashboard with quick trigger buttons, last heartbeat, and last trigger event per prop
- MQTT command publishing for device nodes (`main` / `aux` only; device IDs validated)
- Database-backed live status board from device status messages; every trigger event is logged
- API endpoints for automation tools
- Login required (HTTP basic auth, `ADMIN_USERNAME` / `ADMIN_PASSWORD` in `.env`); only `/api/health` is open
- Starts even if the MQTT broker is down, and reconnects on its own

## Compatible Device Topic Pattern

This app sends commands to:

- `haunt/<device-id>/trigger`

It listens on:

- `haunt/+/status` (stored in `device_status`)
- `haunt/+/event` (logged to `event_log`; last one shown on the dashboard)
- `haunt/+/heartbeat` (last one kept in memory and shown on the dashboard, including IR faults)

Messages from device IDs that do not match the firmware rule (letters, digits, inner hyphens, 1-32 characters) are
ignored.

These match the WiFi laser tag trigger sketches:

- `components/esp32/ESP32_LaserTagIRTrigger/` (ESP32 DevKit, Acebott ESP32-Max)
- `components/esp8266/ESP8266_LaserTagIRTrigger/`
- `components/esp01/ESP01_LaserTagIRTrigger/`

The boards also publish `haunt/<device-id>/event` (one JSON message per trigger) and `haunt/<device-id>/heartbeat`
(JSON every 30 s). See `components/esp32/ESP32_LaserTagIRTrigger/PINOUT.md`.

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
6. Update `.env` values for your MQTT broker and PostgreSQL database, and set `ADMIN_PASSWORD` (the dashboard login).
   - Keep `DATABASE_URL` using password auth such as `scram-sha-256`.
7. Run the app (development only; production runs behind Nginx on `127.0.0.1`):
   - `uvicorn app.main:app --host 0.0.0.0 --port 8080`
8. Open and log in as `admin` with `ADMIN_PASSWORD`:
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

The laser tag trigger sketches respond to these command strings:

- `main` - fire the main outputs (and play the next IR sound if a DFPlayer is fitted)
- `aux` - fire the aux output (ESP32 build only)

`relay1:pulse` and `relay2:pulse` are still accepted as aliases for `main` and `aux`. Commands obey the same lockout and
re-arm gap as an IR shot.

Use the UI or API to publish those commands. Any other command is rejected with HTTP 422.

## API Examples

All endpoints except `/api/health` need the admin login. The trigger endpoint only accepts a JSON body.

### Publish trigger

- `POST /api/devices/{device_id}/trigger`

```bash
curl -u admin:"$ADMIN_PASSWORD" -H 'Content-Type: application/json' -d '{"command":"main"}' \
  http://127.0.0.1:8080/api/devices/haunt-1a2b3c/trigger
```

Body:

```json
{
  "command": "main"
}
```

### List known device statuses

- `GET /api/devices/status`

### Health check

- `GET /api/health` (no login): `ok`, `database_ready`, `mqtt_connected`, `admin_password_set`

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
4. Edit app settings (the installer generated `ADMIN_PASSWORD`; read it with `sudo grep ^ADMIN_PASSWORD= /opt/haunt-controller/.env`):
   - `/opt/haunt-controller/.env`
5. Restart app after changing `.env`:
   - `sudo systemctl restart haunt-controller`
6. Verify the service is healthy:
   - `systemctl status haunt-controller`
   - `curl http://127.0.0.1:8080/api/health`
7. Open and log in as `admin`:
   - `http://<server-ip>/`

Installer behavior:

- Installs EPEL, Python, Mosquitto, Nginx, and NetworkManager Wi-Fi support
- Creates app user `haunt`
- Copies app to `/opt/haunt-controller`
- Builds `.venv` and installs Python requirements
- Installs and starts `haunt-controller` systemd service (Uvicorn listens on `127.0.0.1:8080` only)
- Installs Nginx proxy config from `nginx/haunt-controller.conf`
- Creates `.env` with a random `ADMIN_PASSWORD` on first install; reinstalling keeps the existing `.env`
- Performs a final health check at the end of the install

The dashboard is plain HTTP through Nginx. Basic auth sends the password with every request, so keep the controller on
the haunt's private network (or add TLS to the Nginx site).

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

- `sudo bash deploy/postgres-bootstrap.sh haunt haunt`
- `sudo bash deploy/rocky-harden.sh`
- `sudo bash deploy/rocky-harden.sh --allow-mqtt`
- `sudo bash deploy/mqtt-bootstrap.sh haunt-device`

Both bootstrap scripts prompt for the password (typed twice, not echoed). Passwords are not accepted as arguments, so
they stay out of shell history, and the scripts never print them.

## PostgreSQL Example

Example local PostgreSQL URL in `.env`:

`DATABASE_URL=postgresql+psycopg://haunt:yourStrongPassword@127.0.0.1:5432/haunt`

The app auto-creates tables on startup:

- `device_status`
- `event_log`

Suggested Rocky setup order:

1. `sudo bash deploy/rocky-install.sh`
2. `sudo bash deploy/postgres-bootstrap.sh haunt haunt` (type the password at the prompt)
3. Update `/opt/haunt-controller/.env` with the matching PostgreSQL password
4. `sudo bash deploy/mqtt-bootstrap.sh haunt-device` (type the password at the prompt), then set `MQTT_USERNAME` /
   `MQTT_PASSWORD` in `.env`
5. `sudo bash deploy/rocky-harden.sh`
6. `sudo systemctl restart haunt-controller`
