# ESP32 Pinout Guide

**Created:** 2026-08-07
**Last Updated:** 2026-08-14

Target sketch:

- `components/esp32/ESP32_Node.ino`

## Function Map

| Function               | ESP32 Pin | Notes                                                                       |
| ---------------------- | --------- | --------------------------------------------------------------------------- |
| Relay 1 output         | GPIO4     | Command: `relay1:pulse`                                                     |
| Relay 2 output         | GPIO5     | Command: `relay2:pulse`                                                     |
| Relay 3 output         | GPIO18    | Command: `relay3:toggle`                                                    |
| Positive trigger input | GPIO16    | Active HIGH input.                                                          |
| Negative trigger input | GPIO17    | Active LOW input with pull-up enabled. Also reprints header text to Serial. |

## Serial Output Path (Important)

- Header and log text is printed on USB Serial (`Serial`), not on a GPIO pin.
- Open Serial Monitor at `115200` baud.
- Pulling the negative trigger input LOW reprints the header.

## Power and Ground

| Device                         | ESP32 Connection                 |
| ------------------------------ | -------------------------------- |
| Relay module VCC               | External 5V recommended          |
| Relay module GND               | Common GND with ESP32            |
| Positive trigger source GND    | Common GND with ESP32            |
| Negative trigger switch/sensor | Connect between GPIO17 and GND   |

## Board Power Input (Onboard Connector)

| Input Path           | Connector Available                          | Minimum Input | Maximum Input | Notes                                                                         |
| -------------------- | -------------------------------------------- | ------------- | ------------- | ----------------------------------------------------------------------------- |
| USB power            | Yes (board dependent: USB-C or Micro-USB)    | 4.75V         | 5.25V         | Preferred for programming and normal operation.                               |
| 5V or VIN header pin | Usually available on dev boards              | 4.8V          | 5.5V          | Feed regulated 5V only unless your specific board datasheet states otherwise. |

## Optional IR Receiver (KY-022 / TL1838 / VS1838B)

### Pinout

- KY-022 signal (S/OUT) -> ESP32 GPIO19
- KY-022 GND (-) -> ESP32 GND
- KY-022 VCC (+) -> ESP32 3.3V

### Required Components

- 1x ESP32 development board
- 1x KY-022 (TL1838/VS1838B) IR receiver module
- 1x IR remote transmitter
- 3x female-to-female jumper wires

### Wiring

1. Connect KY-022 GND to ESP32 GND.
2. Connect KY-022 VCC to ESP32 3.3V.
3. Connect KY-022 signal to ESP32 GPIO19.
4. Keep relay and trigger wiring unchanged.

Note: The current ESP32 node sketch does not decode IR input yet. Add IR decode library support and command mapping if you want IR-triggered actions.

## Relay Logic

- The sketch is configured for **active LOW** relays.
- If your relay board is active HIGH, set `RELAY_ACTIVE_LOW` to `false`.

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
