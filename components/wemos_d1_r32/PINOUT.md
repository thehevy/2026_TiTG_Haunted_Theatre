# WeMos D1 R32 Pinout Guide

**Created:** 2026-08-14  
**Last Updated:** 2026-10-03

Target sketch:

- `components/wemos_d1_r32/WemosD1R32_Node/WemosD1R32_Node.ino`

## Board Overview

The WeMos D1 R32 is an ESP32-based board in an Arduino UNO R3 form factor.
It uses the same held-trigger behavior as the Nano and Arduino 101 variants, but with ESP32 GPIO numbers.

Key differences from a standard UNO:

- 3.3V logic - all GPIO pins are 3.3V. Do not connect 5V signals directly.
- Relay modules must accept 3.3V control input (most optocoupler relay boards do).
- KY-022 IR receiver must be powered from 3.3V, not 5V.
- Silk-screen labels D0-D13 are printed on the board but map to ESP32 GPIO numbers.

## IDE Setup

- Board: ESP32 Dev Module in Arduino IDE 2.x
- Baud: `115200`
- Required library: IRremote by Arduino-IRremote (install via Library Manager)

## Function Map

| Function               | GPIO | Board Silk Label | Notes                                                                                     |
| ---------------------- | ---- | ---------------- | ----------------------------------------------------------------------------------------- |
| IR programming input    | 19   | D6               | Used for IR programming and option selection. Code `0x00` triggers Relay 1 only.          |
| Positive trigger input | 23   | D7               | Active HIGH. Keeps Relay 2 active only while D7 remains HIGH.                             |
| Negative trigger input | 27   | D9               | Active LOW. Keeps Relay 3 active only while D9 remains LOW. Use pull-up or internal pull-up. |
| Relay 1 output         | 16   | D4               | Pulse output for the IR `0x00` trigger.                                                  |
| Relay 2 output         | 17   | D3               | Held active while D7 stays HIGH.                                                          |
| Relay 3 output         | 18   | D5               | Held active while D9 stays LOW.                                                           |

## Serial Output Path

- All header and log text is printed on USB Serial at `115200` baud.
- D9 is pulled low to show the negative trigger state.

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

| Input Path        | Connector Available  | Minimum Input | Maximum Input | Notes                                                    |
| ----------------- | -------------------- | ------------- | ------------- | -------------------------------------------------------- |
| USB power         | Yes (Micro-USB)      | 4.75V         | 5.25V         | Preferred for programming and serial monitoring.         |
| Barrel jack / VIN | Yes (UNO-style jack) | 7V            | 12V           | Regulated on-board. 7-9V recommended to reduce heat.     |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> GPIO19 (D6 label)
- KY-022 GND (-) -> GND
- KY-022 VCC (+) -> 3.3V (not 5V)

### Required Components

- 1x WeMos D1 R32
- 1x 3-channel relay module or three single-relay modules
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 1x 10k pull-down resistor for the positive trigger line so D7 stays LOW by default
- 1x 10k pull-up resistor for the negative trigger line if needed
- Jumper wires and a common ground

### IR Command Map

| Remote Button | IR Code | Action             |
| ------------- | ------- | ------------------ |
| `0`           | `0x00`  | Pulse GPIO16       |

## Required Components and Resistor Guidance

- 1x WeMos D1 R32
- 1x 3-channel relay module (or three single-relay modules)
- 1x 5V DC power supply sized for relay coil current
- 1x 10k pull-down resistor for D7 so the positive trigger line stays LOW by default
- 1x 10k pull-up resistor for D9 if you are not relying on the internal pull-up or if the trigger source is open collector
- Jumper wires, terminal blocks, and a stable common ground

### Wiring notes for safe signal conditioning

1. Connect the positive trigger source to D7 through a proper signal path, and add a 10k resistor from D7 to GND so the line stays LOW until a valid HIGH signal is present.
2. Do not tie D7 directly to 3.3V without a resistor path to ground; this can leave the line floating during reset and can stress the signal source.
3. For the negative trigger, wire D9 to a pull-up or use the internal pull-up, and switch the sensor to ground only when active.
4. Keep the relay coil supply and the Arduino ground common, but do not power the relay coil from the same pin supply without appropriate flyback protection.

## Relay Logic

- The sketch is configured for active LOW relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false` in the sketch.
- For the current trigger model, D7 and D9 behave as held-state triggers rather than momentary pulses.

## Validation

1. Open the Serial Monitor at `115200`.
2. Press reset and confirm the startup banner.
3. Drive D7 HIGH and verify the sketch reports `POSITIVE -> Relay 2 active while D7 stays HIGH`.
4. Release D7 and verify Relay 2 turns off.
5. Pull D9 LOW and verify the sketch reports `NEGATIVE -> Relay 3 active while D9 stays LOW`.
6. Release D9 and verify Relay 3 turns off.
7. Transmit IR code `0x00` and verify Relay 1 pulses once.
8. Confirm the startup banner shows `Default timeout: none`.

## GPIO Safety Notes

- Avoid GPIO0, GPIO2, GPIO12, GPIO15 for outputs. These are boot-strapping pins.
- GPIO34, 35, 36, 39 are input-only on ESP32 and are not used in this sketch.
- All pins used in this sketch (GPIO16, 17, 18, 19, 23, 27) are safe for general I/O.
