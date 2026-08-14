# ESP8266 Pinout Guide

**Created:** 2026-08-07
**Last Updated:** 2026-08-14

Target sketch:

- `components/esp8266/ESP8266_Node.ino`

## Function Map

| Function               | ESP8266 Pin | Typical NodeMCU Label                                             |
| ---------------------- | ----------- | ----------------------------------------------------------------- |
| Relay 1 output         | GPIO5       | D1                                                                |
| Relay 2 output         | GPIO4       | D2                                                                |
| Relay 3 output         | GPIO14      | D5                                                                |
| Positive trigger input | GPIO12      | D6, active HIGH                                                   |
| Negative trigger input | GPIO13      | D7, active LOW with pull-up. Also reprints header text to Serial. |

## Serial Output Path (Important)

- Header and log text is printed on USB Serial (`Serial`), not on a GPIO pin.
- Open Serial Monitor at `115200` baud.
- Pulling the negative trigger input LOW reprints the header.

## Power and Ground

| Device                         | ESP8266 Connection             |
| ------------------------------ | ------------------------------ |
| Relay module VCC               | External 5V recommended        |
| Relay module GND               | Common GND with ESP8266        |
| Positive trigger source GND    | Common GND with ESP8266        |
| Negative trigger switch/sensor | Connect between D7 and GND     |

## Board Power Input (Onboard Connector)

| Input Path           | Connector Available                                  | Minimum Input | Maximum Input | Notes                                                                            |
| -------------------- | ---------------------------------------------------- | ------------- | ------------- | -------------------------------------------------------------------------------- |
| USB power            | Yes (typically Micro-USB on NodeMCU class boards)    | 4.75V         | 5.25V         | Preferred for programming and stable operation.                                  |
| 5V or VIN header pin | Usually available on dev boards                      | 4.8V          | 5.5V          | Use regulated 5V input unless your board vendor documents a different VIN range. |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> ESP8266 GPIO2 (NodeMCU D4)
- KY-022 GND (-) -> ESP8266 GND
- KY-022 VCC (+) -> ESP8266 3.3V

### Required Components

- 1x ESP8266 development board (NodeMCU style)
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 3x female-to-female jumper wires

### Wiring

1. Connect KY-022 GND to ESP8266 GND.
2. Connect KY-022 VCC to ESP8266 3.3V.
3. Connect KY-022 signal to GPIO2 (D4).
4. Keep relay and trigger wiring unchanged.

Note: GPIO2 is a boot-strapping pin and must be HIGH at boot. The KY-022 idle output is normally HIGH, but avoid noisy wiring or forced LOW on this line during power-up.

Note: The current ESP8266 node sketch does not decode IR input yet. Add IR decode library support and command mapping if you want IR-triggered actions.

## Relay Logic

- The sketch is configured for **active LOW** relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false`.

## Network Requirements

- Configure `WIFI_SSID` and `WIFI_PASSWORD`.
- Set `MQTT_HOST`, `MQTT_PORT`, and `DEVICE_ID`.
- Device subscribes to `haunt/<device-id>/trigger`.

## Command Map

- `relay1:pulse`
- `relay2:pulse`
- `relay3:toggle`
