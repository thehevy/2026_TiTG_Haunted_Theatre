# Acebott ESP32-Max v1.0 Pinout Guide

**Created:** 2026-08-14
**Last Updated:** 2026-08-14

Target sketch:

- `components/acebott_esp32_max/AcebottESP32Max_Node/AcebottESP32Max_Node.ino`

## Board Overview

The Acebott ESP32-Max v1.0 uses an ESP32-WROOM-DA module.
It supports WiFi/Bluetooth and provides a standard ESP32 devkit GPIO header.

- **3.3V logic** on all GPIO pins.
- Relay modules must accept 3.3V control input (most optocoupler relay boards do).
- KY-022 IR receiver must be powered from 3.3V, not 5V.

## IDE Setup

- Board: **ESP32 Dev Module** in Arduino IDE 2.x
- Baud: `115200`
- Required libraries (via Library Manager):
  - **PubSubClient** by Nick O'Leary
  - **IRremote** by Arduino-IRremote

## Function Map

| Function               | GPIO | Notes                                                                    |
| ---------------------- | ---- | ------------------------------------------------------------------------ |
| Relay 1 output         | 4    | Pulse. MQTT `relay1:pulse`. IR A1. Serial `1` or `a` toggle.             |
| Relay 2 output         | 5    | Pulse+lockout. MQTT `relay2:pulse`. IR A2. Serial `2` or `b` toggle.     |
| Relay 3 output         | 18   | Toggle. MQTT `relay3:toggle`. IR A3/A6. Serial `3` or `c` toggle.        |
| Positive trigger input | 16   | Active HIGH input.                                                       |
| Negative trigger input | 17   | Active LOW with pull-up. Also reprints header text to Serial.            |
| IR receiver input      | 19   | KY-022 / TL1838 / VS1838B signal pin. Power from 3.3V only.              |

## Power and Ground

| Device                         | ESP32-Max Connection                          |
| ------------------------------ | --------------------------------------------- |
| Relay module VCC               | External 5V recommended                       |
| Relay module GND               | Common GND with ESP32-Max                     |
| Relay module IN signal         | GPIO4 / GPIO5 / GPIO18 (3.3V signal)          |
| Positive trigger source GND    | Common GND with ESP32-Max                     |
| Negative trigger switch/sensor | Connect between GPIO17 and GND                |
| KY-022 VCC                     | 3.3V only (not 5V)                            |
| KY-022 GND                     | GND                                           |
| KY-022 signal                  | GPIO19                                        |

## Board Power Input (Onboard Connector)

| Input Path   | Connector Available         | Minimum Input | Maximum Input | Notes                                              |
| ------------ | --------------------------- | ------------- | ------------- | -------------------------------------------------- |
| USB power    | Yes (Micro-USB or USB-C)    | 4.75V         | 5.25V         | Preferred for programming and serial monitoring.   |
| VIN pin      | Yes (header pin)            | 4.8V          | 5.5V          | Supply regulated 5V only.                          |

## WiFi and MQTT

- Device connects to configured WiFi network on boot.
- Subscribes to `haunt/<device-id>/trigger` for relay commands.
- Publishes `online` to `haunt/<device-id>/status` on connect.
- MQTT commands: `relay1:pulse`, `relay2:pulse`, `relay3:toggle`.

## AP Fallback Configuration Mode

- If WiFi connection times out, device starts its own setup AP.
- AP SSID format: `HauntSetup-XXXXXX`
- AP password: `hauntsetup`
- Connect to AP and open `http://192.168.4.1` in a browser.
- Configure: WiFi SSID, password, MQTT host, MQTT port, Device ID.
- Settings are saved to NVS and device reboots automatically.

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> GPIO19
- KY-022 GND (-) -> GND
- KY-022 VCC (+) -> 3.3V (not 5V)

### IR Command Map

| Remote Button | IR Code | Action                         |
| ------------- | ------- | ------------------------------ |
| A1            | `0x0C`  | Pulse GPIO4                    |
| A2            | `0x18`  | Pulse GPIO5                    |
| A3            | `0x5E`  | Pulse GPIO18                   |
| A4            | `0x08`  | Toggle GPIO4                   |
| A5            | `0x1C`  | Toggle GPIO5                   |
| A6            | `0x5A`  | Toggle GPIO18                  |
| A7            | `0x42`  | Pulse GPIO4, GPIO5, GPIO18     |
| M1            | `0x07`  | Set lockout mode to none       |
| M2            | `0x15`  | Set lockout mode to 5 seconds  |
| M3            | `0x09`  | Set lockout mode to 15 seconds |
| Power         | `0x45`  | Reprint header text to Serial  |

Lockout behavior:

- Lockout applies to IR pulse actions (A1, A2, A3, A7).
- Toggle actions (A4, A5, A6) always execute regardless of lockout.

## Relay Logic

- Sketch configured for **active LOW** relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false`.

## Serial Control

- Baud: `115200`
- Commands: `1`/`2`/`3` pulse, `a`/`b`/`c` toggle, `i` print last IR code.
