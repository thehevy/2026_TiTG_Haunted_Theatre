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

Note: This node sketch currently does not decode IR. Add an IR decode library and action mapping before expecting IR-triggered outputs.

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
