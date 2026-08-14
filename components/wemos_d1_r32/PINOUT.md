# WeMos D1 R32 Pinout Guide

**Created:** 2026-08-14
**Last Updated:** 2026-08-14

Target sketch:

- `components/wemos_d1_r32/WemosD1R32_Node/WemosD1R32_Node.ino`

## Board Overview

The WeMos D1 R32 is an ESP32-based board in an Arduino UNO R3 form factor.
It runs the same UNO-style sketch logic but with ESP32 GPIO numbers.

Key differences from a standard UNO:

- **3.3V logic** - all GPIO pins are 3.3V. Do not connect 5V signals directly.
- Relay modules must accept 3.3V control input (most optocoupler relay boards do).
- KY-022 IR receiver must be powered from 3.3V, not 5V.
- Silk-screen labels D0-D13 are printed on the board but map to ESP32 GPIO numbers.

## IDE Setup

- Board: **ESP32 Dev Module** in Arduino IDE 2.x
- Baud: `115200`
- Required library: **IRremote** by Arduino-IRremote (install via Library Manager)

## Function Map

| Function               | GPIO | Board Silk Label | Notes                                                             |
| ---------------------- | ---- | ---------------- | ----------------------------------------------------------------- |
| Relay 1 output         | 16   | D4               | Pulse. Serial command `1` or `a` toggle.                          |
| Relay 2 output         | 17   | D3               | Pulse+lockout. Serial command `2` or `b` toggle.                  |
| Relay 3 output         | 18   | D5               | Toggle. Serial command `3` or `c` toggle.                         |
| Positive trigger input | 23   | D7               | Active HIGH input.                                                |
| Negative trigger input | 27   | D9               | Active LOW with pull-up. Also reprints header text to Serial.     |
| IR receiver input      | 19   | D6               | KY-022 / TL1838 / VS1838B signal pin. Power from 3.3V only.       |

## Serial Output Path

- All header and log text is printed on USB Serial at `115200` baud.
- Pulling GPIO27 LOW reprints the header.

## Power and Ground

| Device                         | D1 R32 Connection                             |
| ------------------------------ | --------------------------------------------- |
| Relay module VCC               | External 5V recommended                       |
| Relay module GND               | Common GND with D1 R32                        |
| Relay module IN signal         | GPIO16/17/18 (3.3V - verify module tolerance) |
| Positive trigger source GND    | Common GND with D1 R32                        |
| Negative trigger switch/sensor | Connect between GPIO27 and GND                |
| KY-022 VCC                     | 3.3V (not 5V)                                 |
| KY-022 GND                     | GND                                           |
| KY-022 signal                  | GPIO19                                        |

## Board Power Input (Onboard Connector)

| Input Path        | Connector Available  | Minimum Input | Maximum Input | Notes                                                   |
| ----------------- | -------------------- | ------------- | ------------- | ------------------------------------------------------- |
| USB power         | Yes (Micro-USB)      | 4.75V         | 5.25V         | Preferred for programming and serial monitoring.        |
| Barrel jack / VIN | Yes (UNO-style jack) | 7V            | 12V           | Regulated on-board. 7-9V recommended to reduce heat.    |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> GPIO19 (D6 label)
- KY-022 GND (-) -> GND
- KY-022 VCC (+) -> 3.3V (not 5V)

### Required Components

- 1x WeMos D1 R32
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 3x female-to-female jumper wires

### IR Command Map

| Remote Button | IR Code | Action                         |
| ------------- | ------- | ------------------------------ |
| A1            | `0x0C`  | Pulse GPIO16                   |
| A2            | `0x18`  | Pulse GPIO17                   |
| A3            | `0x5E`  | Pulse GPIO18                   |
| A4            | `0x08`  | Toggle GPIO16                  |
| A5            | `0x1C`  | Toggle GPIO17                  |
| A6            | `0x5A`  | Toggle GPIO18                  |
| A7            | `0x42`  | Pulse GPIO16, GPIO17, GPIO18   |
| M1            | `0x07`  | Set lockout mode to none       |
| M2            | `0x15`  | Set lockout mode to 5 seconds  |
| M3            | `0x09`  | Set lockout mode to 15 seconds |
| Power         | `0x45`  | Reprint header text to Serial  |

Lockout behavior:

- Lockout mode applies to IR pulse actions (A1, A2, A3, A7).
- Toggle actions (A4, A5, A6) ignore lockout and always execute.

## Relay Logic

- The sketch is configured for **active LOW** relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false`.

## Serial Control

- Baud: `115200`
- Send one character commands from Serial Monitor:
  - `1`, `2`, `3` for pulse
  - `a`, `b`, `c` for toggle
  - `i` to print last IR command and raw code

## GPIO Safety Notes

- Avoid GPIO0, GPIO2, GPIO12, GPIO15 for outputs. These are boot-strapping pins.
- GPIO34, 35, 36, 39 are input-only on ESP32 and are not used in this sketch.
- All pins used in this sketch (GPIO16, 17, 18, 19, 23, 27) are safe for general I/O.
