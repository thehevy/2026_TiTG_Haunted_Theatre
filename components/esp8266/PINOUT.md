# ESP8266 Pinout Guide

Target sketch:

- `components/esp8266/ESP8266_Node.ino`

## Function Map

| Function | ESP8266 Pin | Typical NodeMCU Label |
| --- | --- | --- |
| Relay 1 output | GPIO5 | D1 |
| Relay 2 output | GPIO4 | D2 |
| Relay 3 output | GPIO14 | D5 |
| Positive trigger input | GPIO12 | D6, active HIGH |
| Negative trigger input | GPIO13 | D7, active LOW with pull-up. Also reprints header text to Serial. |

## Serial Output Path (Important)

- Header and log text is printed on USB Serial (`Serial`), not on a GPIO pin.
- Open Serial Monitor at `115200` baud.
- Pulling the negative trigger input LOW reprints the header.

## Power and Ground

| Device | ESP8266 Connection |
| --- | --- |
| Relay module VCC | External 5V recommended |
| Relay module GND | Common GND with ESP8266 |
| Positive trigger source GND | Common GND with ESP8266 |
| Negative trigger switch/sensor | Connect between D7 and GND |

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
