# Arduino UNO Pinout Guide

**Created:** 2026-08-07
**Last Updated:** 2026-08-14

Target sketch:

- `components/uno/UNO_Node.ino`

## Function Map

| Function               | UNO Pin | Trigger                                                              |
| ---------------------- | ------- | -------------------------------------------------------------------- |
| Relay 1 output         | D4      | Serial command `1` (pulse) or `a` (toggle)                           |
| Relay 2 output         | D5      | Serial command `2` (pulse) or `b` (toggle)                           |
| Relay 3 output         | D6      | Serial command `3` (pulse) or `c` (toggle)                           |
| Positive trigger input | D7      | Active HIGH input                                                    |
| Negative trigger input | D8      | Active LOW with pull-up. Also reprints header text to Serial.        |

## Power and Ground

| Device                         | UNO Connection               |
| ------------------------------ | ---------------------------- |
| Relay module VCC               | External 5V recommended      |
| Relay module GND               | Common GND with UNO          |
| Positive trigger source GND    | Common GND with UNO          |
| Negative trigger switch/sensor | Connect between D8 and GND   |

## Board Power Input (Onboard Connector)

| Input Path        | Connector Available                 | Minimum Input            | Maximum Input             | Notes                                                                             |
| ----------------- | ----------------------------------- | ------------------------ | ------------------------- | --------------------------------------------------------------------------------- |
| USB power         | Yes (USB-B on UNO R3 class boards)  | 4.75V                    | 5.25V                     | Preferred for bench setup and programming.                                        |
| Barrel jack / VIN | Yes                                 | 7V (recommended minimum) | 12V (recommended maximum) | Board family limit is wider, but 7V to 12V is the practical operating range.      |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> UNO D2
- KY-022 GND (-) -> UNO GND
- KY-022 VCC (+) -> UNO 5V

### Required Components

- 1x Arduino UNO or Nano (ATmega328P)
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 3x female-to-female jumper wires

### Wiring

1. Connect KY-022 GND to UNO GND.
2. Connect KY-022 VCC to UNO 5V.
3. Connect KY-022 signal to UNO D2.
4. Keep relay and trigger wiring unchanged.

### IR Command Map

| Remote Button | IR Code | Action                         |
| ------------- | ------- | ------------------------------ |
| A1            | `0x0C`  | Pulse D4                       |
| A2            | `0x18`  | Pulse D5                       |
| A3            | `0x5E`  | Pulse D6                       |
| A4            | `0x08`  | Toggle D4                      |
| A5            | `0x1C`  | Toggle D5                      |
| A6            | `0x5A`  | Toggle D6                      |
| A7            | `0x42`  | Pulse D4, D5, and D6           |
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
- Header and log text is printed on USB Serial (`Serial`), not on a GPIO pin.
- Pulling D8 LOW reprints the header.
- Send one character commands from Serial Monitor:
  - `1`, `2`, `3` for pulse
  - `a`, `b`, `c` for toggle
