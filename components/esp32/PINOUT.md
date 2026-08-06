# ESP32 Pinout Guide

Target sketch:

- `components/esp32/ESP32_Node.ino`

## Function Map

| Function | ESP32 Pin | Notes |
| --- | --- | --- |
| Relay 1 output | GPIO4 | Command: `relay1:pulse` |
| Relay 2 output | GPIO5 | Command: `relay2:pulse` |
| Relay 3 output | GPIO18 | Command: `relay3:toggle` |
| Positive trigger input | GPIO16 | Active HIGH input. |
| Negative trigger input | GPIO17 | Active LOW input with pull-up enabled. Also reprints header text to Serial. |

## Serial Output Path (Important)

- Header and log text is printed on USB Serial (`Serial`), not on a GPIO pin.
- Open Serial Monitor at `115200` baud.
- Pulling the negative trigger input LOW reprints the header.

## Power and Ground

| Device | ESP32 Connection |
| --- | --- |
| Relay module VCC | External 5V recommended |
| Relay module GND | Common GND with ESP32 |
| Positive trigger source GND | Common GND with ESP32 |
| Negative trigger switch/sensor | Connect between GPIO17 and GND |

## Relay Logic

- The sketch is configured for **active LOW** relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false`.

## Network Requirements

- Configure `WIFI_SSID` and `WIFI_PASSWORD`.
- Set `MQTT_HOST`, `MQTT_PORT`, and `DEVICE_ID`.
- Device subscribes to `haunt/<device-id>/trigger`.
