# 2026 TiTG Haunted Theatre

**Created:** 2026-07-30
**Last Updated:** 2026-10-20

Centralized haunted-house control project for mixed hardware:

- Arduino 101 (Intel Curie)
- ESP-01 bare module
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
| ESP-01 bare module nodes | Arduino IDE 2.x (latest stable) | Recommended | Use a 3.3V serial adapter and keep GPIO0/GPIO2 constraints in mind when wiring. |
| ESP32 nodes | Arduino IDE 2.x (latest stable) | Recommended | Keep ESP32 board package current and use Library Manager for dependencies. |
| ESP8266 nodes | Arduino IDE 2.x (latest stable) | Recommended | Keep ESP8266 board package current and use Library Manager for dependencies. |
| UNO and Nano class nodes | Arduino IDE 2.x (latest stable) | Recommended | Standard AVR workflows are stable in IDE 2.x. |

Recommended local setup:

- Install Arduino IDE 1.8.19 and Arduino IDE 2.x side-by-side.
- Use IDE 1.8.19 only for Arduino 101 tasks.
- Use IDE 2.x for ESP32, ESP8266, UNO, and Nano tasks.
- Or use arduino-cli from VS Code for every board. See [Command-Line Compile and Upload](#command-line-compile-and-upload).

## Repository Layout

- `components/arduino101/Arduino101_LaserTagIRTrigger/Arduino101_LaserTagIRTrigger.ino`
  - IR trigger for laser tag guns, TV remotes, and LED-strip remotes (ignores phones and sunlight): D4 LOW + D5 HIGH for
    0.5/5/10/15 s selected by D8/D9 jumpers, an optional D2 wired trigger (negative or positive, off by default), plus an
    aux D10 -> D11 negative trigger. Optional DFPlayer Mini sound (auto-detected). Same source as the UNO and Nano versions.
  - Pinout: `components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`
- `components/esp01/ESP01_Node.ino`
  - ESP-01 IR-trigger relay node with 1s pulse and 15s cooldown
  - Pinout: `components/esp01/PINOUT.md`
- `components/esp32/ESP32_Node.ino`
  - ESP32 MQTT relay node
  - Pinout: `components/esp32/PINOUT.md`
- `components/esp8266/ESP8266_Node.ino`
  - ESP8266 MQTT relay node
  - Pinout: `components/esp8266/PINOUT.md`
- `components/uno/UNO_LaserTagIRTrigger/` and `components/nano/Nano_LaserTagIRTrigger/`
  - UNO and Nano builds of the laser tag IR trigger (same pins and behavior, 5 V logic)
- `components/nano/Nano_MP3TriggerPlayer/Nano_MP3TriggerPlayer.ino`
  - Arduino Nano + DIYables Serial MP3 Player: 4 negative triggers (D2-D5) + PIR (D8), each plays its own file once; other triggers ignored until it finishes
  - Pinout: `components/nano/Nano_MP3TriggerPlayer/PINOUT.md`
- `central-controller/`
  - Web controller app and deployment assets
- `archive/`
  - Retired sketches (earlier laser tag and simple IR triggers, the RF/UNO/Nano node sketches, CH9311, WeMos D1 R32). Each `.ino` header lists why it was retired.
- `LASER_TAG_TRANSITION_GUIDE.md`
  - Printable per-board rewiring checklist for moving deployed props to the `*_LaserTagIRTrigger` sketches
- `tools/arduino/`
  - `arduino.ps1` arduino-cli helper (setup, compile, upload, monitor)
  - `sketches.json` sketch-to-board (FQBN) map, required cores, and libraries
- `tools/web-config/index.html`
  - USB (Web Serial) settings page for the `*_LaserTagIRTrigger` sketches; open in Chrome or Edge
- `SETUP_GUIDE.md`
  - Arduino 101 setup, firmware update, and troubleshooting
- `CENTRAL_CONTROLLER_PLAN.md`
  - Full architecture and rollout plan
- `SESSION_NOTES.md`
  - Current status, next steps, and resume-on-a-new-machine setup

## Quick Start

### 0. ESP-01 IR Trigger Node

1. Open `components/esp01/ESP01_Node.ino` in Arduino IDE 2.x.
2. Review wiring in `components/esp01/PINOUT.md`.
3. Set board to Generic ESP8266 Module or the matching ESP-01 board package profile.
4. Select the correct serial adapter port for the ESP-01 programmer.
5. Upload and confirm the relay pulses for 1 second after each valid IR edge.
6. Verify the 15 second cooldown by triggering the sensor twice in rapid succession.

### 1. Arduino 101 Laser Tag IR Trigger

1. Review wiring in `components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`.
2. Confirm the port: `.\tools\arduino\arduino.ps1 ports` (COM5 or your active board port).
3. Upload: `.\tools\arduino\arduino.ps1 upload Arduino101_LaserTagIRTrigger -Port COM5`
   (or Arduino IDE 1.8.19 with board Arduino/Genuino 101).
4. Open Serial Monitor at 115200 and verify the banner and heartbeat.
5. Set the prop name and durations with `tools/web-config/index.html`.

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

## Command-Line Compile and Upload

All sketches can be compiled and uploaded with arduino-cli from PowerShell or VS Code tasks (**Terminal > Run Task > Arduino: ...**). Full install steps and per-board notes are in [SETUP_GUIDE.md](SETUP_GUIDE.md#command-line-compile-and-upload-arduino-cli).

```powershell
.\tools\arduino\arduino.ps1 setup                              # one time: board URLs, proxy, cores, libraries
.\tools\arduino\arduino.ps1 ports                              # confirm the board's COM port
.\tools\arduino\arduino.ps1 compile all
.\tools\arduino\arduino.ps1 upload Arduino101_LaserTagIRTrigger -Port COM5
.\tools\arduino\arduino.ps1 monitor -Port COM5
.\tools\arduino\arduino.ps1 intellisense                       # VS Code C/C++ IntelliSense for all sketches
```

Arduino 101 firmware updates still require Arduino IDE 1.8.19 (Tools > Firmware Updater).

## MQTT Topic Convention

- Trigger command to device:
  - `haunt/<device-id>/trigger`
- Device status publish:
  - `haunt/<device-id>/status`

Current node command strings:

- `relay1:pulse`
- `relay2:pulse`
- `relay3:toggle`

## Quick Wiring Checklist for ESP-01

- Sensor signal -> GPIO2 (active HIGH)
- Sensor GND -> ESP-01 GND
- Sensor VCC -> ESP-01 3.3V
- Relay control -> GPIO0
- Relay module VCC -> external 5V supply
- Relay module GND -> common GND with ESP-01
- Ensure the relay board is compatible with active LOW output logic
- Verify GPIO0 is not being held low during reset by the relay board or wiring
- Use a 3.3V USB serial adapter for programming the bare ESP-01 module

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
