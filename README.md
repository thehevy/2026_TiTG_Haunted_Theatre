# 2026 TiTG Haunted Theatre

**Created:** 2026-07-30
**Last Updated:** 2026-10-26

Centralized haunted-house control project for mixed hardware:

- Arduino 101 (Intel Curie)
- Arduino UNO and Nano
- ESP32 (DevKit, Acebott ESP32-Max), ESP8266 (NodeMCU, ESP-12F), and ESP-01
- Rocky Linux / Raspberry Pi server controllers

Most props run one shared **laser tag IR trigger** sketch, built for every board from the same source. Each build has the
same pins where the board allows, settings saved on the board, and a settings page: over USB for the Arduino boards,
and over WiFi for the ESP boards.

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
  - **Shared laser tag IR trigger source** (edit this copy, then run `.\tools\arduino\arduino.ps1 sync`). Any valid IR
    code (laser tag guns, TV and LED-strip remotes; phones and sunlight ignored) pulls the negative output LOW and drives
    the positive output HIGH for 0.5/5/10/15 s (jumpers) or the saved `main_ms`. Optional wired trigger (off by default),
    aux trigger, and DFPlayer Mini sound. Each sketch folder's `board_config.h` holds that board's pins and features.
  - Pinout and behavior: `components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`
- `components/uno/UNO_LaserTagIRTrigger/`, `components/nano/Nano_LaserTagIRTrigger/`
  - UNO and Nano builds (same pins, 5 V logic)
- `components/esp32/ESP32_LaserTagIRTrigger/`, `components/esp8266/ESP8266_LaserTagIRTrigger/`, `components/esp01/ESP01_LaserTagIRTrigger/`
  - WiFi builds (ESP32 DevKit / Acebott ESP32-Max, ESP8266 NodeMCU / ESP-12F, ESP-01): settings web page, router or
    setup access point, MQTT to the central controller, OTA updates. Guide: `components/esp32/ESP32_LaserTagIRTrigger/PINOUT.md`
- `components/arduino101/Arduino101_PIRLaserSoundTrigger/`
  - Arduino 101: 3 PIR + 1 laser sensor, each plays its own DFPlayer track and holds its own negative output
- `components/nano/Nano_MP3TriggerPlayer/Nano_MP3TriggerPlayer.ino`
  - Arduino Nano + DIYables Serial MP3 Player: 4 negative triggers (D2-D5) + PIR (D8), each plays its own file once; other triggers ignored until it finishes
  - Pinout: `components/nano/Nano_MP3TriggerPlayer/PINOUT.md`
- `components/esp32/ESP32_SmartTracking_Node/`, `components/esp32/ESP32S3_SmartTracking_Node/`
  - Tracking skull (ToF sensors, servo, LED eyes, DFPlayer). Pinouts: `components/esp32/SMART_TRACKING_PINOUT.md`,
    `components/esp32/ESP32S3_SMART_TRACKING_PINOUT.md`
- `central-controller/`
  - Web controller app and deployment assets
- `archive/`
  - Retired sketches (earlier laser tag and simple IR triggers; the RF, UNO, Nano, ESP32, Acebott, ESP8266, and ESP-01
    node sketches; CH9311; WeMos D1 R32). Each `.ino` header lists why it was retired.
- `LASER_TAG_TRANSITION_GUIDE.md`
  - Printable per-board rewiring checklist for moving deployed props to the `*_LaserTagIRTrigger` sketches
- `tools/arduino/`
  - `arduino.ps1` arduino-cli helper (setup, compile, upload over USB or WiFi, monitor, sync shared sketch sources)
  - `sketches.json` sketch-to-board (FQBN) map, required cores, and libraries
- `tools/web-config/index.html`
  - USB (Web Serial) settings page for every `*_LaserTagIRTrigger` build; open in Chrome or Edge
- `SETUP_GUIDE.md`
  - Arduino 101 setup, firmware update, and troubleshooting
- `CENTRAL_CONTROLLER_PLAN.md`
  - Full architecture and rollout plan
- `SESSION_NOTES.md`
  - Current status, next steps, and resume-on-a-new-machine setup

## Quick Start

### 0. ESP32 / ESP8266 / ESP-01 Laser Tag IR Trigger (WiFi)

1. Review wiring in the board's `PINOUT.md` (`components/esp32/ESP32_LaserTagIRTrigger/PINOUT.md` has the WiFi guide).
2. First upload over USB: `.\tools\arduino\arduino.ps1 upload ESP32_LaserTagIRTrigger -Port COMx`
   (or `ESP8266_LaserTagIRTrigger`, `ESP01_LaserTagIRTrigger`).
3. Serial (115200) prints a generated setup AP password and admin password; write them down. With no router set, the
   board starts the setup access point `Haunt-<device_id>`. Join it with the AP password and log in at
   `http://192.168.4.1` as `admin` with the admin password.
4. Set your own passwords, the router SSID/password (show router: `TITG2026HT`) and the MQTT login, **Save**, then
   **Reboot**.
5. Later updates can go over WiFi: `.\tools\arduino\arduino.ps1 upload ESP32_LaserTagIRTrigger -Port <board-ip>`.

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
.\tools\arduino\arduino.ps1 upload ESP32_LaserTagIRTrigger -Port 192.168.1.50   # WiFi (OTA); asks for the admin password
.\tools\arduino\arduino.ps1 sync                               # copy the shared laser tag .ino to every board folder
.\tools\arduino\arduino.ps1 monitor -Port COM5
.\tools\arduino\arduino.ps1 intellisense                       # VS Code C/C++ IntelliSense for all sketches
```

Arduino 101 firmware updates still require Arduino IDE 1.8.19 (Tools > Firmware Updater).

## MQTT Topic Convention

Used by the WiFi `*_LaserTagIRTrigger` builds. `<device-id>` is set on each board's settings page (default
`haunt-<6 hex digits>`).

| Topic                          | Direction        | Payload                                                       |
| ------------------------------ | ---------------- | ------------------------------------------------------------- |
| `haunt/<device-id>/trigger`    | controller -> board | `main` or `aux` (`relay1:pulse` / `relay2:pulse` also accepted) |
| `haunt/<device-id>/status`     | board -> controller | `online` (retained), `offline` as the last will            |
| `haunt/<device-id>/event`      | board -> controller | JSON for each trigger: source, fired, pulse ms, IR code     |
| `haunt/<device-id>/heartbeat`  | board -> controller | JSON every 30 s: name, board, firmware, IP, RSSI, counts    |

The broker requires a login (`allow_anonymous false`); enter the MQTT username and password on each board.

## Quick Wiring Checklist for ESP-01

- IR sensor OUT -> GPIO2 (fires on HIGH by default; must be HIGH at power-up, no pull-down)
- Sensor VCC -> 3.3V, GND -> GND
- Relay driver -> GPIO0 (goes HIGH to fire): NPN transistor with 1k base resistor, plus a 10k pull-up from GPIO0 to 3.3V
- Relay module VCC -> external 5V supply, GND common with the ESP-01
- For an IR receiver (laser tag guns) instead of a sensor, set `wired_input` to off on the settings page
- Use a 3.3V USB serial adapter for the first upload; later updates can go over WiFi

## Required Libraries

`.\tools\arduino\arduino.ps1 setup` installs everything in `tools/arduino/sketches.json`. For Arduino IDE, install with
Library Manager:

| Board/Sketch | Required library | Notes |
| --- | --- | --- |
| ESP32 / ESP8266 / ESP-01 `*_LaserTagIRTrigger` | `PubSubClient` by Nick O'Leary | MQTT client (`PubSubClient.h`). |
| `Arduino101_PIRLaserSoundTrigger`, ESP32 smart tracking | `DFRobotDFPlayerMini` by DFRobot | DFPlayer Mini control. |
| ESP32 smart tracking | `VL53L1X` by Pololu, `ESP32Servo` | ToF sensors and servo. |

The Arduino 101 / UNO / Nano `*_LaserTagIRTrigger` builds and `Nano_MP3TriggerPlayer` need no extra libraries. WiFi,
web server, OTA, and mDNS come from the ESP32 and ESP8266 board packages.

## Notes

- Arduino 101 is a retired board and works best with Arduino IDE 1.8.19.
- Keep shell scripts LF-only for Rocky compatibility (enforced by `.gitattributes`).
