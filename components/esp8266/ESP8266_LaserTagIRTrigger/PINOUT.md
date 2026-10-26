# ESP8266 Laser Tag IR Trigger (WiFi) Pinout

**Created:** 2026-10-26
**Last Updated:** 2026-10-26

Target sketch:

- `components/esp8266/ESP8266_LaserTagIRTrigger/ESP8266_LaserTagIRTrigger.ino` (pins in `board_config.h`)

Boards: **NodeMCU v2/v3** and **ESP-12F** modules (FQBN `esp8266:esp8266:nodemcuv2`).

WiFi features (router or setup AP, settings page, MQTT, OTA, passwords) are the same as the ESP32 build: see
`components/esp32/ESP32_LaserTagIRTrigger/PINOUT.md`. Trigger behavior: see
`components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`.

## Differences From the ESP32 Build

The ESP8266 has only five GPIOs that are safe at boot, so this build has **no wired input, no aux trigger, and no
DFPlayer**. IR, both main outputs, the duration jumpers, both status LEDs, WiFi, MQTT, and OTA all work.

## Pin Map

| Function              | NodeMCU label | GPIO | Notes                                                       |
| --------------------- | ------------- | ---- | ----------------------------------------------------------- |
| IR receiver OUT       | D5            | 14   | 3.3 V receiver                                              |
| Main negative trigger | D1            | 5    | **Active LOW**; released (high-Z) when idle                 |
| Main positive trigger | D2            | 4    | **Active HIGH** (3.3 V)                                     |
| Duration jumper A     | D6            | 12   | To GND = 5 s                                                |
| Duration jumper B     | D7            | 13   | To GND = 10 s (both = 15 s)                                 |
| Relay status LED      | D4            | 2    | Onboard blue LED (lights when LOW); boot pin, add nothing   |
| IR status LED         | D0            | 16   | 330 Ohm -> LED -> GND                                       |
| Setup AP button       | D3            | 0    | **FLASH** button: hold 3 s while running                    |

With a 5 V relay module, measure its IN (D1) to GND once with the relay off; it must be 3.6 V or less (see the
ESP32 `PINOUT.md`, *Check a 5 V relay module*).

Do not connect anything to D3, D4, or D8 (boot pins). The setup AP can also be forced by pressing **RST** 3 times within
10 s each (power cycles do not count), or with the serial command `AP`.

## Setup and Upload

- USB: `.\tools\arduino\arduino.ps1 upload ESP8266_LaserTagIRTrigger -Port COMx`
- WiFi: `.\tools\arduino\arduino.ps1 upload ESP8266_LaserTagIRTrigger -Port <ip>` (asks for the admin password), or upload
  the `.bin` from `arduino-cli compile --export-binaries` on the settings page.
- Serial Monitor: `115200` baud. Power the IR receiver from **3V3**.

## Wiring

```text
NodeMCU                                   IR receiver (3.3 V)
3V3 ------------------------------------> VCC
D5 <------------------------------------- OUT
GND ------------------------------------> GND

NodeMCU                                   Main relay module (active LOW, 5 V)
D1 -------------------------------------> IN
VIN (5 V from USB) ---------------------> VCC
GND ------------------------------------> GND

NodeMCU                                   Optional
D2 -------------------------------------> active-HIGH load (3.3 V logic)
D6 / D7 --[jumper]--> GND                 5 s / 10 s (both = 15 s)
D0 -- 330 Ohm --> LED --> GND             IR status LED
```

Compile verified 2026-10-26: 345,192 B code in flash (33%), IRAM 93%. Not yet bench-tested.
