# Arduino Nano Pinout Guide

**Created:** 2026-10-02  
**Last Updated:** 2026-10-03

Target sketch:

- `components/nano/Nano_Node/Nano_Node.ino`

## Function Map

| Function               | Nano Pin | Notes                                                                                       |
| ---------------------- | -------- | ------------------------------------------------------------------------------------------- |
| IR programming input    | D2       | Used for IR programming and option selection. Code `0x00` triggers Relay 1 only.            |
| Positive trigger input | D7       | Active HIGH. Keeps Relay 2 active only while D7 remains HIGH.                               |
| Negative trigger input | D8       | Active LOW. Keeps Relay 3 active only while D8 remains LOW. Use pull-up or internal pull-up. |
| Relay 1 output         | D4       | Pulse output for the IR `0x00` trigger.                                                    |
| Relay 2 output         | D5       | Held active while D7 stays HIGH.                                                            |
| Relay 3 output         | D6       | Held active while D8 stays LOW.                                                             |

## Power and Ground

| Device                         | Nano Connection             |
| ------------------------------ | --------------------------- |
| Relay module VCC               | External 5V recommended      |
| Relay module GND               | Common GND with Nano        |
| Positive trigger source GND    | Common GND with Nano        |
| Negative trigger switch/sensor | Connect between D8 and GND |

## Board Power Input (Onboard Connector)

| Input Path                   | Connector Available                         | Minimum Input            | Maximum Input             | Notes                                                                                 |
| ---------------------------- | ------------------------------------------- | ------------------------ | ------------------------- | ------------------------------------------------------------------------------------- |
| USB power                    | Yes, USB Micro on the base Nano board      | 4.75V                    | 5.25V                     | Preferred for bench setup and programming.                                            |
| Barrel jack on breakout board | Yes, via the external power breakout board | 7V (recommended minimum) | 12V (recommended maximum) | Use this when powering the Nano from a wall adapter; the board regulates it to 5V. |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> Nano D2
- KY-022 GND (-) -> Nano GND
- KY-022 VCC (+) -> Nano 5V

### IR Command Map

| Remote Button | IR Code | Action                    |
| ------------- | ------- | ------------------------- |
| `0`           | `0x00`  | Pulse D4                  |

## Required Components and Resistor Guidance

- 1x Arduino Nano
- 1x 3-channel relay module (or three single-relay modules)
- 1x 5V DC power supply sized for relay coil current
- 1x 10k pull-down resistor for D7 so the positive trigger line stays LOW by default
- 1x 10k pull-up resistor for D8 if you are not relying on the internal pull-up or if the trigger source is open collector
- Jumper wires, terminal blocks, and a stable common ground

### Wiring notes for safe signal conditioning

1. Connect the positive trigger source to D7 through a proper signal path, and add a 10k resistor from D7 to GND so the line stays LOW until a valid HIGH signal is present.
2. Do not tie D7 directly to 5V without a resistor path to ground; this can short the signal source or leave the line floating during reset.
3. For the negative trigger, wire D8 to a pull-up or use the internal pull-up, and switch the sensor to ground only when active.
4. Keep the relay coil supply and the Arduino ground common, but do not power the relay coil from the same pin supply without appropriate flyback protection.

## Relay Logic

- The sketch is configured for active LOW relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false` in the sketch.
- For the current trigger model, D7 and D8 behave as held-state triggers rather than momentary pulses.

## Validation

1. Open the Serial Monitor at `115200`.
2. Press reset and confirm the startup banner.
3. Drive D7 HIGH and verify the sketch reports `POSITIVE -> Relay 2 active while D7 stays HIGH`.
4. Release D7 and verify Relay 2 turns off.
5. Pull D8 LOW and verify the sketch reports `NEGATIVE -> Relay 3 active while D8 stays LOW`.
6. Release D8 and verify Relay 3 turns off.
7. Transmit IR code `0x00` and verify Relay 1 pulses once.
8. Confirm the startup banner shows `Default timeout: none`.
