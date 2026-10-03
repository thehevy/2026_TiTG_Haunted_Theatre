# ESP32 Pinout Guide

**Created:** 2026-08-07  
**Last Updated:** 2026-10-03

Target sketch:

- `components/esp32/ESP32_Node/ESP32_Node.ino`

## IDE Setup

- Board: ESP32 Dev Module
- Serial baud: `115200`
- Required libraries:
  - PubSubClient by Nick O'Leary
  - IRremote by Arduino-IRremote

## Function Map

| Function               | ESP32 Pin | Notes                                                                 |
| ---------------------- | --------- | --------------------------------------------------------------------- |
| IR receiver input      | GPIO19    | KY-022 signal line. IR `0x00` pulses Relay 1                          |
| Positive trigger input | GPIO16    | Active HIGH; holds Relay 2 while HIGH                                 |
| Negative trigger input | GPIO17    | Active LOW; holds Relay 3 while LOW                                    |
| Relay 1 output         | GPIO4     | Pulse output for the IR `0x00` command                                |
| Relay 2 output         | GPIO5     | Held active while GPIO16 remains HIGH                                 |
| Relay 3 output         | GPIO18    | Held active while GPIO17 remains LOW                                  |

## Serial Output Path

- Header and log text are printed on USB Serial (`Serial`), not on a GPIO pin.
- Open the Serial Monitor at `115200` baud.
- The sketch logs trigger transitions for the positive and negative inputs.

## Power and Ground

| Device                         | ESP32 Connection               |
| ------------------------------ | ------------------------------ |
| Relay module VCC               | External 5V recommended        |
| Relay module GND               | Common GND with ESP32          |
| Positive trigger source GND    | Common GND with ESP32          |
| Negative trigger switch/sensor | Connect between GPIO17 and GND |
| IR receiver VCC                | ESP32 3.3V                     |
| IR receiver GND                | ESP32 GND                     |
| IR receiver signal            | GPIO19                        |

## Board Power Input

| Input Path           | Connector Available                       | Minimum Input | Maximum Input | Notes                                                      |
| -------------------- | ----------------------------------------- | ------------- | ------------- | ---------------------------------------------------------- |
| USB power            | Yes (board dependent: USB-C or Micro-USB) | 4.75V         | 5.25V         | Preferred for programming and normal operation.            |
| 5V or VIN header pin | Usually available on dev boards           | 4.8V          | 5.5V          | Feed regulated 5V only unless the board datasheet says otherwise. |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> ESP32 GPIO19
- KY-022 GND (-) -> ESP32 GND
- KY-022 VCC (+) -> ESP32 3.3V

### IR Command Map

| Remote Button | IR Code | Action       |
| ------------- | ------- | ------------ |
| `0`           | `0x00`  | Pulse Relay 1 |

## Required Components and Resistor Guidance

- 1x ESP32 development board
- 1x 3-channel relay module or three single-relay modules
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 1x 10k pull-down resistor for GPIO16 so the positive trigger line defaults LOW
- 1x 10k pull-up resistor for GPIO17 if needed
- Female-to-female jumper wires and common ground wiring

### Wiring notes

1. Connect the positive trigger signal to GPIO16 through a proper interface so it defaults LOW and rises HIGH only when active.
2. Keep the negative trigger line on GPIO17 pulled HIGH by default and switch it to GND only when active.
3. Maintain common ground between the relay board, the ESP32, and any trigger sources.
4. Do not drive relay modules directly from the ESP32 GPIO pins without a proper driver or board-level transistor/optocoupler arrangement if your relay board requires it.

## Relay Logic

- The sketch is configured for active LOW relays.
- If the relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false` in the sketch.
- The positive and negative trigger inputs are held-state inputs rather than momentary pulse triggers.
- The default timeout is none; turnout ends only when the input returns to its inactive state.

## Network Requirements

- Configure `WIFI_SSID` and `WIFI_PASSWORD`.
- Set `MQTT_HOST`, `MQTT_PORT`, and `DEVICE_ID`.
- Device subscribes to `haunt/<device-id>/trigger`.

## AP Fallback Configuration Mode

- If the node cannot connect to the configured WiFi network, it starts its own setup AP.
- AP SSID format: `HauntSetup-XXXXXX`
- AP password: `hauntsetup`
- Connect to the AP and browse to `http://192.168.4.1`.
- Save WiFi SSID/password and MQTT settings in the web form.
- The node stores settings in NVS and restarts automatically.

## MQTT/Serial Command Map

- `relay1:pulse` -> pulse Relay 1
- `relay2:pulse` -> hold Relay 2 active
- `relay3:toggle` -> toggle Relay 3
- `1` on Serial -> pulse Relay 1
- `2` on Serial -> hold Relay 2 active
- `3` on Serial -> hold Relay 3 active
- `i` on Serial -> print the last IR command and raw value
