# ESP-01 Laser Tag IR Trigger (WiFi) Pinout

**Created:** 2026-10-26
**Last Updated:** 2026-10-26

Target sketch:

- `components/esp01/ESP01_LaserTagIRTrigger/ESP01_LaserTagIRTrigger.ino` (pins in `board_config.h`)

Boards: **ESP-01S** or **ESP-01 with 1 MB flash** (FQBN `esp8266:esp8266:generic:eesz=1M64`). Older 512 KB ESP-01
modules are too small for this sketch.

WiFi features (router or setup AP, settings page, MQTT, OTA, passwords) are the same as the ESP32 build: see
`components/esp32/ESP32_LaserTagIRTrigger/PINOUT.md`. Trigger behavior: see
`components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`.

## Differences From the ESP32 Build

Only GPIO0 and GPIO2 are free, so this build is **one input, one negative output**: no positive output, duration
jumpers, LEDs, aux trigger, or DFPlayer. Set the pulse length with `main_ms` on the settings page (100-15000 ms).
There is no button, so force the setup AP by **resetting 3 times within 10 s each** (or the serial command `AP`).

**GPIO2 input, two choices** (settings page **Wired trigger input**, or `SET wired_input=`):

| `wired_input`        | GPIO2 is                                      | Use for                                               |
| -------------------- | --------------------------------------------- | ----------------------------------------------------- |
| Off (default)        | IR receiver; decoded codes fire the output    | KY-022 / VS1838B receiver with the laser tag guns     |
| 1 negative           | Digital input; fires when the sensor goes LOW | IR obstacle / break-beam modules that pull LOW        |
| 2 positive           | Digital input; fires when the sensor goes HIGH | Sensors with an active-HIGH push-pull output         |

While the wired input is on, IR code decoding is off (both use GPIO2). **Do not add a pull-down to GPIO2:** it must be
HIGH at power-up or the ESP-01 will not boot, so a positive sensor must drive its output itself and idle LOW only after
the board has started.

**Replacing `ESP01_Node`** (IR sensor on GPIO2, 1 s pulse, 15 s cooldown): set `wired_input` = 2 (positive),
`main_ms` = 1000, `rearm_ms` = 14000. The relay output on GPIO0 is now **active LOW** (see below).

## Pin Map

| Function              | ESP-01 pin | Notes                                                                    |
| --------------------- | ---------- | ------------------------------------------------------------------------ |
| IR receiver OUT or IR sensor OUT | GPIO2 | Must be HIGH at power-up (receivers and most sensor modules idle HIGH) |
| Main negative trigger | GPIO0      | **Active LOW**; released when idle. Must not be pulled LOW at power-up   |
| Serial TX / RX        | GPIO1 / 3  | USB-serial adapter for upload and the serial commands                    |
| VCC / CH_PD (EN)      | 3.3 V      | 3.3 V only, 300 mA or more                                               |

**Relay on GPIO0:** use an active-LOW relay module or an opto/transistor input whose idle state leaves GPIO0 HIGH. A
module that pulls its input LOW at rest stops the ESP-01 from booting. The archived `ESP01_Node` drove an **active-HIGH**
relay on GPIO0; that relay module will not work with this sketch without changing it to an active-LOW input.

## Setup and Upload

- First upload over USB with an ESP-01 programmer (hold GPIO0 LOW at reset to enter flash mode):
  `.\tools\arduino\arduino.ps1 upload ESP01_LaserTagIRTrigger -Port COMx`
- After that, update over WiFi: `.\tools\arduino\arduino.ps1 upload ESP01_LaserTagIRTrigger -Port <ip>`, or the settings
  page **Firmware** upload.

## Wiring

```text
ESP-01                                    IR receiver or IR sensor module (3.3 V)
3V3 ------------------------------------> VCC
GPIO2 <---------------------------------- OUT
GND ------------------------------------> GND

ESP-01                                    Relay module (active LOW)
GPIO0 ----------------------------------> IN
                                          VCC from a separate 5 V supply; common GND
3V3 -> VCC and CH_PD (EN) ; GND -> GND
```

Compile verified 2026-10-26: 343,180 B code in flash (32%; OTA fits in the 1 MB layout), 31,636 B RAM (39%). Not yet
bench-tested.
