# Arduino UNO Pinout Guide

Target sketch:

- `components/uno/UNO_Node.ino`

## Function Map

| Function | UNO Pin | Trigger |
| --- | --- | --- |
| Relay 1 output | D4 | Serial command `1` (pulse) or `a` (toggle) |
| Relay 2 output | D5 | Serial command `2` (pulse) or `b` (toggle) |
| Relay 3 output | D6 | Serial command `3` (pulse) or `c` (toggle) |
| Positive trigger input | D7 | Active HIGH input |
| Negative trigger input | D8 | Active LOW with pull-up. Also reprints header text to Serial. |

## Power and Ground

| Device | UNO Connection |
| --- | --- |
| Relay module VCC | External 5V recommended |
| Relay module GND | Common GND with UNO |
| Positive trigger source GND | Common GND with UNO |
| Negative trigger switch/sensor | Connect between D8 and GND |

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
