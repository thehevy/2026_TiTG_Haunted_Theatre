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

Only GPIO0 and GPIO2 are free, so this build is **one input, one output**: no positive output, duration jumpers, LEDs,
aux trigger, or DFPlayer. There is no button, so force the setup AP by **pulsing RST to GND 3 times within 10 s each**
(power cycles do not count), or with the serial command `AP`. Each reset boots the board, and every boot clicks the relay
(see below), so disconnect the prop's load first.

**Out of the box it behaves like the old `ESP01_Node`** and uses the same wiring:

- IR **sensor** on GPIO2, fires when the sensor output goes **HIGH** (`wired_input` = 2).
- **GPIO0 goes HIGH for 1 s** to drive the relay through the NPN transistor (`main_ms` = 1000).
- **15 s cooldown**: 1 s pulse + 14 s re-arm (`rearm_ms` = 14000). Sensor edges during the cooldown are ignored.

All three can be changed on the settings page.

**GPIO2 input choices** (settings page **Wired trigger input**, or `SET wired_input=`):

| `wired_input`        | GPIO2 is                                       | Use for                                              |
| -------------------- | ---------------------------------------------- | ---------------------------------------------------- |
| 2 positive (default) | Digital input; fires when the sensor goes HIGH | The existing IR sensor (active-HIGH output)          |
| 1 negative           | Digital input; fires when the sensor goes LOW  | IR obstacle / break-beam modules that pull LOW       |
| Off                  | IR receiver; decoded codes fire the output     | KY-022 / VS1838B receiver with the laser tag guns    |

While the wired input is on, IR code decoding is off (both use GPIO2). **Do not add a pull-down to GPIO2:** it must be
HIGH at power-up or the ESP-01 will not boot.

## Pin Map

| Function              | ESP-01 pin | Notes                                                                       |
| --------------------- | ---------- | --------------------------------------------------------------------------- |
| IR sensor (or receiver) OUT | GPIO2 | Must be HIGH at power-up                                                  |
| Relay driver output   | GPIO0      | **Active HIGH**: HIGH for `main_ms` on a trigger, LOW when idle             |
| Serial TX / RX        | GPIO1 / 3  | USB-serial adapter for upload and the serial commands                       |
| VCC / CH_PD (EN)      | 3.3 V      | 3.3 V only, 300 mA or more                                                  |

**Relay on GPIO0 (unchanged from `ESP01_Node`):** drive the relay through an NPN transistor (1 kOhm to the base) and keep
a **10 kOhm pull-up from GPIO0 to 3.3 V** so GPIO0 is HIGH at reset. The sketch drives GPIO0 LOW as its first step, but
during the ~0.1-0.3 s boot GPIO0 is HIGH, so the relay can click briefly at power-up (the old sketch did the same). Do
not connect a relay input that pulls GPIO0 LOW at rest; the ESP-01 will not boot.

**Check GPIO0 at power-up (once per board):** the 10 kOhm pull-up feeds the NPN base through 1 kOhm, which can hold
GPIO0 near 0.9 V. Below about 2.5 V the ESP-01 may read LOW and start in flash mode instead of running (serial shows
`boot mode:(1,x)` and nothing else). With the ESP-01 removed (or held in reset with RST to GND), measure GPIO0 to GND:

- **2.5 V or more:** fine.
- **Lower:** use a 2N7000 / BS170 logic-level N-MOSFET instead of the NPN (gate to GPIO0, source to GND, drain to
  relay IN; a gate draws no current, so GPIO0 stays at 3.3 V), or change the resistors to a 2.2 kOhm pull-up and a
  22 kOhm base resistor.

Either way the relay still clicks for the ~0.2 s boot, because GPIO0 must be HIGH at reset. Use a prop that tolerates
a short pulse at power-up, or a relay with a longer pull-in time.

## Setup and Upload

- First upload over USB with an ESP-01 programmer (hold GPIO0 LOW at reset to enter flash mode):
  `.\tools\arduino\arduino.ps1 upload ESP01_LaserTagIRTrigger -Port COMx`
- After that, update over WiFi: `.\tools\arduino\arduino.ps1 upload ESP01_LaserTagIRTrigger -Port <ip>`, or the settings
  page **Firmware** upload.

## Wiring

```text
ESP-01                                    IR sensor module (3.3 V)
3V3 ------------------------------------> VCC
GPIO2 <---------------------------------- OUT (HIGH = triggered)
GND ------------------------------------> GND

ESP-01                                    Relay driver (same as ESP01_Node)
3V3 -- 10 kOhm --+
GPIO0 -----------+-- 1 kOhm --> NPN base ; emitter -> GND ; collector -> relay module IN
                                          Relay module VCC from a separate 5 V supply; common GND
3V3 -> VCC and CH_PD (EN) ; GND -> GND
```

**Replacing `ESP01_Node`:** no rewiring. Flash this sketch, then set the WiFi and MQTT settings on the settings page.

Compile verified 2026-10-26: 345,660 B code in flash (33%; OTA fits in the 1 MB layout), IRAM 93%. Not yet
bench-tested.
