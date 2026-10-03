# Acebott ESP32-Max v1.0 Pinout Guide

**Created:** 2026-08-14  
**Last Updated:** 2026-10-03

Target sketch:

- `components/acebott_esp32_max/AcebottESP32Max_Node/AcebottESP32Max_Node.ino`

## Board Overview

The Acebott ESP32-Max v1.0 uses an ESP32-WROOM-DA module.
It supports WiFi/Bluetooth and provides a standard ESP32 devkit GPIO header.

- 3.3V logic on all GPIO pins.
- Relay modules must accept 3.3V control input (most optocoupler relay boards do).
- KY-022 IR receiver must be powered from 3.3V, not 5V.

## IDE Setup

- Board: ESP32 Dev Module in Arduino IDE 2.x
- Baud: `115200`
- Required libraries (via Library Manager):
  - PubSubClient by Nick O'Leary
  - IRremote by Arduino-IRremote

## Function Map

| Function               | GPIO | Board Notes                                                                                 |
| ---------------------- | ---- | ------------------------------------------------------------------------------------------ |
| IR programming input    | 19   | Used for IR programming and option selection. Code `0x00` triggers Relay 1 only.            |
| Positive trigger input | 16   | Active HIGH. Keeps Relay 2 active only while GPIO16 remains HIGH.                          |
| Negative trigger input | 17   | Active LOW. Keeps Relay 3 active only while GPIO17 remains LOW. Use pull-up or internal pull-up. |
| Relay 1 output         | 4    | Pulse output for the IR `0x00` trigger.                                                   |
| Relay 2 output         | 5    | Held active while GPIO16 stays HIGH.                                                      |
| Relay 3 output         | 18   | Held active while GPIO17 stays LOW.                                                       |

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

| Input Path   | Connector Available      | Minimum Input | Maximum Input | Notes                                            |
| ------------ | ------------------------ | ------------- | ------------- | ------------------------------------------------ |
| USB power    | Yes (Micro-USB or USB-C) | 4.75V         | 5.25V         | Preferred for programming and serial monitoring. |
| VIN pin      | Yes (header pin)         | 4.8V          | 5.5V          | Supply regulated 5V only.                        |

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

| Remote Button | IR Code | Action       |
| ------------- | ------- | ------------ |
| `0`           | `0x00`  | Pulse GPIO4  |

## Required Components and Resistor Guidance

- 1x Acebott ESP32-Max v1.0
- 1x 3-channel relay module (or three single-relay modules)
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 1x 10k pull-down resistor for GPIO16 so the positive trigger line stays LOW by default
- 1x 10k pull-up resistor for GPIO17 if needed
- Jumper wires and a stable common ground

### Wiring notes for safe signal conditioning

1. Connect the positive trigger source to GPIO16 through a proper signal path, and add a 10k resistor from GPIO16 to GND so the line stays LOW until a valid HIGH signal is present.
2. Do not tie GPIO16 directly to 3.3V without a resistor path to ground; this can leave the line floating during reset and can stress the signal source.
3. For the negative trigger, wire GPIO17 to a pull-up or use the internal pull-up, and switch the sensor to ground only when active.
4. Keep the relay coil supply and the Arduino ground common, but do not power the relay coil from the same pin supply without appropriate flyback protection.

## Relay Logic

- The sketch is configured for active LOW relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false` in the sketch.
- For the current trigger model, GPIO16 and GPIO17 behave as held-state triggers rather than momentary pulses.

## Validation

1. Open the Serial Monitor at `115200`.
2. Press reset and confirm the startup banner.
3. Drive GPIO16 HIGH and verify the sketch reports `POSITIVE -> Relay 2 active while GPIO16 stays HIGH`.
4. Release GPIO16 and verify Relay 2 turns off.
5. Pull GPIO17 LOW and verify the sketch reports `NEGATIVE -> Relay 3 active while GPIO17 stays LOW`.
6. Release GPIO17 and verify Relay 3 turns off.
7. Transmit IR code `0x00` and verify Relay 1 pulses once.
8. Confirm the startup banner shows `Default timeout: none`.

## Serial Control

- Baud: `115200`
- Commands: `1` pulses Relay 1, `2` holds Relay 2, and `3` holds Relay 3 for test use.
- `i` prints the last IR command and raw code.
