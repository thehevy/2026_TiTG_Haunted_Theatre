# ESP8266 Pinout Guide

**Created:** 2026-08-07  
**Last Updated:** 2026-10-03

Target sketch:

- `components/esp8266/ESP8266_Node/ESP8266_Node.ino`

Package Download Location:
<https://arduino.esp8266.com/stable/package_esp8266com_index.json>

## IDE Setup

- Board: ESP8266 NodeMCU 1.0 (ESP-12E Module)
- Upload speed: 115200 or 921600 depending on board and USB cable quality
- Serial baud: `115200`
- Required libraries:
  - PubSubClient by Nick O'Leary
  - IRremote by Arduino-IRremote

## Function Map

| Function               | ESP8266 Pin | NodeMCU Label                 | Notes                                                   |
| ---------------------- | ----------- | ---------------------------- | ------------------------------------------------------- |
| IR receiver input      | GPIO2       | D4                           | KY-022 signal line, 3.3V-powered                        |
| Positive trigger input | GPIO12      | D6                           | Active HIGH; holds Relay 2 while HIGH                   |
| Negative trigger input | GPIO13      | D7                           | Active LOW; holds Relay 3 while LOW                      |
| Relay 1 output         | GPIO5       | D1                           | IR `0x00` pulse output                                   |
| Relay 2 output         | GPIO4       | D2                           | Held active while D6 stays HIGH                          |
| Relay 3 output         | GPIO14      | D5                           | Held active while D7 stays LOW                          |

## Serial Output Path

- Header and log text are printed on USB Serial (`Serial`), not on a GPIO pin.
- Open the Serial Monitor at `115200` baud.
- The sketch reports the trigger state transitions for the positive and negative inputs.

## Power and Ground

| Device                         | ESP8266 Connection          |
| ------------------------------ | --------------------------- |
| Relay module VCC               | External 5V recommended     |
| Relay module GND               | Common GND with ESP8266     |
| Positive trigger source GND    | Common GND with ESP8266     |
| Negative trigger switch/sensor | Connect between D7 and GND  |
| IR receiver VCC                | ESP8266 3.3V               |
| IR receiver GND                | ESP8266 GND                |
| IR receiver signal            | GPIO2 (D4)                 |

## Board Power Input

The acceptable ESP8266 board input power range is approximately 4.75V to 5.5V, depending on the power path used.

| Input Path           | Connector Available                               | Minimum Input Voltage | Maximum Input Voltage | Notes                                                       |
| -------------------- | ------------------------------------------------- | --------------------- | --------------------- | ----------------------------------------------------------- |
| USB power            | Yes, typically Micro-USB on NodeMCU class boards | 4.75V                 | 5.25V                 | Preferred for programming and stable operation.             |
| 5V or VIN header pin | Usually available on dev boards                   | 4.8V                  | 5.5V                  | Use regulated 5V unless the board vendor specifies another range. |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> GPIO2 (D4)
- KY-022 GND (-) -> GND
- KY-022 VCC (+) -> 3.3V

### IR Command Map

| Remote Button | IR Code | Action          |
| ------------- | ------- | --------------- |
| `0`           | `0x00`  | Pulse Relay 1   |

## Required Components and Resistor Guidance

- 1x ESP8266 NodeMCU class board
- 1x 3-channel relay module or three single-relay modules
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 1x 10k pull-down resistor for D6 so the positive trigger line stays LOW by default
- 1x 10k pull-up resistor for D7 if needed
- Female-to-female jumper wires and common ground wiring

### Wiring notes

1. Connect the positive trigger signal to D6 through a proper interface so the line defaults LOW and rises HIGH only when active.
2. Keep the negative trigger line on D7 pulled HIGH by default and switch it to GND only when active.
3. Maintain common ground between the relay board, the ESP8266, and any trigger sources.
4. Do not drive relay modules directly from a GPIO pin without a proper driver or board-level transistor/optocoupler arrangement if the board requires it.

## Relay Logic

- The sketch is configured for active LOW relays.
- If the relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false` in the sketch.
- The positive and negative trigger inputs are held-state inputs rather than momentary pulse triggers.
- The default timeout is none; shutdown occurs only when the triggering input drops back to its inactive state.

## Network Requirements

- Configure `WIFI_SSID` and `WIFI_PASSWORD`.
- Set `MQTT_HOST`, `MQTT_PORT`, and `DEVICE_ID`.
- Device subscribes to `haunt/<device-id>/trigger`.

## MQTT/Serial Command Map

- `relay1:pulse` -> pulse Relay 1
- `relay2:pulse` -> hold Relay 2 active
- `relay3:toggle` -> toggle Relay 3
- `1` on Serial -> pulse Relay 1
- `2` on Serial -> hold Relay 2 active
- `3` on Serial -> hold Relay 3 active
- `i` on Serial -> print the last IR command and raw value
