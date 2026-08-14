# 2026 TiTG Haunted Theatre

**Created:** 2026-07-30
**Last Updated:** 2026-08-07

Centralized haunted-house control project for mixed hardware:

- Arduino 101 (Intel Curie)
- ESP32
- ESP8266
- Arduino UNO
- Rocky Linux / Raspberry Pi server controllers

This repository contains:

- Device firmware sketches for multiple board families
- A central web controller scaffold (FastAPI + MQTT)
- Rocky Linux deployment and hardening scripts
- Setup and architecture documentation

## IDE Support Matrix

Use this matrix when selecting Arduino IDE versions for each hardware family.

| Component | Preferred IDE | Status | Notes |
| --- | --- | --- | --- |
| Arduino 101 (Intel Curie) | Arduino IDE 1.8.19 | Required | Use 1.8.19 for firmware updates and uploads. Curie tooling is legacy. |
| ESP32 nodes | Arduino IDE 2.x (latest stable) | Recommended | Keep ESP32 board package current and use Library Manager for dependencies. |
| ESP8266 nodes | Arduino IDE 2.x (latest stable) | Recommended | Keep ESP8266 board package current and use Library Manager for dependencies. |
| UNO and Nano class nodes | Arduino IDE 2.x (latest stable) | Recommended | Standard AVR workflows are stable in IDE 2.x. |

Recommended local setup:

- Install Arduino IDE 1.8.19 and Arduino IDE 2.x side-by-side.
- Use IDE 1.8.19 only for Arduino 101 tasks.
- Use IDE 2.x for ESP32, ESP8266, UNO, and Nano tasks.

## Repository Layout

- `components/arduino101/Arduino101_Node/Arduino101_Node.ino`
  - Arduino 101 RF-driven relay node (pulse, lockout pulse, toggle)
  - Pinout: `components/arduino101/PINOUT.md`
- `components/esp32/ESP32_Node.ino`
  - ESP32 MQTT relay node
  - Pinout: `components/esp32/PINOUT.md`
- `components/esp8266/ESP8266_Node.ino`
  - ESP8266 MQTT relay node
  - Pinout: `components/esp8266/PINOUT.md`
- `components/uno/UNO_Node.ino`
  - UNO serial relay node
  - Pinout: `components/uno/PINOUT.md`
- `central-controller/`
  - Web controller app and deployment assets
- `SETUP_GUIDE.md`
  - Arduino 101 setup, firmware update, and troubleshooting
- `CENTRAL_CONTROLLER_PLAN.md`
  - Full architecture and rollout plan

## Quick Start

### 1. Arduino 101 Node

1. Open `components/arduino101/Arduino101_Node/Arduino101_Node.ino` in Arduino IDE 1.8.19.
2. Review wiring in `components/arduino101/PINOUT.md`.
3. Set board to Arduino/Genuino 101.
4. Set port to COM5 (or your active board port).
5. Upload and open Serial Monitor at 115200.
6. Verify heartbeat and RF receive output.

### 2. Central Controller (Dev Mode)

1. Change directory:
   - `cd central-controller`
2. Create venv:
   - `python -m venv .venv`
3. Activate venv (PowerShell):
   - `.\.venv\Scripts\Activate.ps1`
4. Install deps:
   - `pip install -r requirements.txt`
5. Configure env:
   - `copy .env.example .env`
6. Run:
   - `uvicorn app.main:app --host 0.0.0.0 --port 8080`

### 3. Rocky Linux Deployment

From `central-controller/` on Rocky Linux:

1. `sudo bash deploy/rocky-install.sh`
2. `sudo bash deploy/postgres-bootstrap.sh haunt haunt`
3. `sudo bash deploy/mqtt-bootstrap.sh haunt-device`
4. `sudo bash deploy/rocky-harden.sh`
5. Update `/opt/haunt-controller/.env`
6. `sudo systemctl restart haunt-controller`

## MQTT Topic Convention

- Trigger command to device:
  - `haunt/<device-id>/trigger`
- Device status publish:
  - `haunt/<device-id>/status`

Current node command strings:

- `relay1:pulse`
- `relay2:pulse`
- `relay3:toggle`

## Required Libraries

Install these with Arduino IDE Library Manager before compiling the related sketches.

| Board/Sketch | Required library | Notes |
| --- | --- | --- |
| Arduino 101 node | `rc-switch` by sui77 | Used for 433 MHz RF receive/decode in the Arduino 101 sketch. |
| UNO node | `IRremote` by Arduino-IRremote | Required for KY-022/TL1838/VS1838B IR decode (`IRremote.hpp`). |
| ESP32 node | `PubSubClient` by Nick O'Leary | Required for MQTT client support (`PubSubClient.h`). |
| ESP32 node | `IRremote` by Arduino-IRremote | Required for KY-022/TL1838/VS1838B IR decode (`IRremote.hpp`). |
| ESP8266 node | `PubSubClient` by Nick O'Leary | Required for MQTT client support (`PubSubClient.h`). |
| ESP8266 node | `IRremote` by Arduino-IRremote | Required for KY-022/TL1838/VS1838B IR decode (`IRremote.hpp`). |

Included from board cores (no Library Manager install needed):

- `WiFi.h` for ESP32 comes from the ESP32 board package.
- `ESP8266WiFi.h` for ESP8266 comes from the ESP8266 board package.

## Notes

- Arduino 101 is a retired board and works best with Arduino IDE 1.8.19.
- `rc-switch` on Arduino 101 may warn about architecture compatibility; test reliability in your environment.
- Keep shell scripts LF-only for Rocky compatibility (enforced by `.gitattributes`).
