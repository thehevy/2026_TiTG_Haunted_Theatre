# Laser Tag IR Trigger Transition Guide

**Created:** 2026-10-26
**Last Updated:** 2026-10-26

How to move each deployed IR trigger board from its old sketch to the new `*_LaserTagIRTrigger` sketch. Print this
guide, find each prop's old sketch in the [Quick Lookup](#quick-lookup), and follow that card.

## Table of Contents

- [What Changes](#what-changes)
- [New Standard Pinout](#new-standard-pinout)
- [Before You Start](#before-you-start)
- [Quick Lookup](#quick-lookup)
- [Card A: Arduino 101 Negative-Only Sketches](#card-a-arduino-101-negative-only-sketches)
- [Card B: Arduino 101 Dual Trigger Sketches](#card-b-arduino-101-dual-trigger-sketches)
- [Card C: UNO Negative-Only Sketches](#card-c-uno-negative-only-sketches)
- [Card D: UNO Dual Trigger Sketch](#card-d-uno-dual-trigger-sketch)
- [Card E: Nano Sketches](#card-e-nano-sketches)
- [Card F: Arduino 101 Wired-Input Sketches](#card-f-arduino-101-wired-input-sketches)
- [Card G: WiFi Boards (ESP32, Acebott, ESP8266, ESP-01)](#card-g-wifi-boards-esp32-acebott-esp8266-esp-01)
- [60-Second Props: Add a Timer Relay](#60-second-props-add-a-timer-relay)
- [Flash, Configure, and Test (Every Board)](#flash-configure-and-test-every-board)
- [Deployed Board Worksheet](#deployed-board-worksheet)

## What Changes

- **One sketch per board type:** `Arduino101_LaserTagIRTrigger`, `UNO_LaserTagIRTrigger`, or `Nano_LaserTagIRTrigger`.
- **More IR sources trigger:** all laser tag guns, TV remotes, and LED-strip remotes. Phones and sunlight are still ignored.
- **No retrigger during a pulse:** shots during a pulse are ignored and do not extend it. Each pulse is followed by a
  short re-arm gap, so edge-triggered props fire once per pulse.
- **Duration is set on the board,** not by choosing a sketch: jumpers on D8/D9, or the saved setting on the config page.
- **60 s is retired.** Use a 0.5 s pulse to start a programmable timer relay.
- **New optional aux trigger:** D10 input to D11 output. It can stay unconnected.
- **New optional wired trigger:** D2 fires the main outputs from a switch or sensor. It is off until enabled on the
  config page, so an empty D2 can never fire.

## New Standard Pinout

Same pins on the Arduino 101, UNO, and Nano.

| Pin | Function                          | Connect                                         |
| --- | --------------------------------- | ----------------------------------------------- |
| D2  | Wired trigger input (off by default) | Optional: switch / sensor; enable with `wired_input` |
| D3  | IR receiver OUT                   | IR receiver signal                              |
| D4  | Main negative trigger (active LOW) | Active-LOW relay module IN                     |
| D5  | Main positive trigger (active HIGH) | Optional: active-HIGH relay / MOSFET / prop input |
| D6  | Relay status LED                  | 330 Ohm resistor -> LED -> GND                  |
| D7  | IR status LED                     | 330 Ohm resistor -> LED -> GND                  |
| D8  | Duration jumper A                 | Wire to GND for 5 s (see table)                 |
| D9  | Duration jumper B                 | Wire to GND for 10 s (see table)                |
| D10 | Aux trigger input                 | Optional: button / contact to GND               |
| D11 | Aux trigger output (active LOW)   | Optional: second active-LOW relay module IN     |
| D0 / D1 (Arduino 101) | DFPlayer serial         | Optional sound: D0 <- DFPlayer TX, D1 -> 1 kOhm -> DFPlayer RX |
| A0 / A1 (UNO, Nano)   | DFPlayer serial         | Optional sound: A0 <- DFPlayer TX, A1 -> 1 kOhm -> DFPlayer RX |

**Duration jumpers** (a short wire from the pin to any GND pin):

| D8 to GND | D9 to GND | Main pulse                                  |
| --------- | --------- | ------------------------------------------- |
| no        | no        | 0.5 s (or the saved `main_ms` setting)      |
| yes       | no        | 5 s                                         |
| no        | yes       | 10 s                                        |
| yes       | yes       | 15 s                                        |

**IR receiver power is unchanged:** 3.3V on the Arduino 101, 5V on the UNO and Nano.

> **Warning: D5, D6, D7, and D11 are now outputs.** At idle, D5, D6, and D7 are LOW. An **active-LOW relay input** on any
> of them turns that relay **ON** as soon as the board boots. Only LEDs (through a resistor) or active-HIGH loads may be
> on D5-D7. Only an active-LOW relay may be on D11.
>
> The sketch also sends DFPlayer commands on **D1 (Arduino 101)** or **A1 (UNO/Nano)**. Leave those pins empty unless a
> DFPlayer is connected.

## Before You Start

Parts and tools:

- [ ] Laptop with the repo, arduino-cli, and Chrome or Edge (for `tools/web-config/index.html`)
- [ ] USB cables (Arduino 101 / UNO: USB-B or micro-USB as fitted; Nano: mini-USB)
- [ ] Female-female and male-female Dupont jumper wires (for duration jumpers and moved wires)
- [ ] Hot glue or tape to secure moved wires and jumpers against vibration
- [ ] Nano only: 2 LEDs and 2 x 330 Ohm resistors if you want status LEDs (optional)
- [ ] Multimeter (optional, to confirm D4 idles HIGH and pulses LOW)
- [ ] A laser tag gun and a TV or LED remote for testing

Safety:

1. **Unplug the board's USB and power, and the relay supply, before moving any wire.**
2. Leave the prop's mains or high-voltage side alone. Only the low-voltage control wires on the board header change.
3. Label each moved wire with tape before pulling it.

## Quick Lookup

Read the old sketch name from the board's startup banner, the [worksheet](#deployed-board-worksheet), or `SESSION_NOTES.md`.

| Old sketch                                  | Card | Duration setting       |
| ------------------------------------------- | ---- | ---------------------- |
| `Arduino101_LaserTagIRNegativeTrigger`      | A    | No jumpers (0.5 s)     |
| `Arduino101_LaserTagIRNegativeTrigger5s`    | A    | D8 to GND (5 s)        |
| `Arduino101_LaserTagIRNegativeTrigger10s`   | A    | D9 to GND (10 s)       |
| `Arduino101_LaserTagIRNegativeTrigger60s`   | A    | No jumpers + [timer relay](#60-second-props-add-a-timer-relay) |
| `Arduino101_SimpleIRNegativeTrigger`        | A    | No jumpers (0.5 s)     |
| `Arduino101_SimpleIRNegativeTrigger15s`     | A    | D8 + D9 to GND (15 s)  |
| `Arduino101_SimpleIRNegativeTrigger60s`     | A    | No jumpers + [timer relay](#60-second-props-add-a-timer-relay) |
| `Arduino101_LaserTagIRDualTrigger`          | B    | No jumpers (0.5 s)     |
| `Arduino101_LaserTagIRDualTrigger5s`        | B    | D8 to GND (5 s)        |
| `UNO_LaserTagIRNegativeTrigger`             | C    | No jumpers (0.5 s)     |
| `UNO_SimpleIRNegativeTrigger`               | C    | No jumpers (0.5 s)     |
| `UNO_LaserTagIRDualTrigger`                 | D    | No jumpers (0.5 s)     |
| `Nano_LaserTagIRNegativeTrigger`            | E    | No jumpers (0.5 s)     |
| `Nano_LaserTagIRNegativeTrigger5s`          | E    | D8 to GND (5 s)        |
| `Arduino101_TimedTriggerRelay`              | F    | Match the old selector (0.5/5/10/15 s); 30/60 s use a timer relay |
| `Arduino101_Node`                           | F    | No jumpers (0.5 s)     |
| `ESP32_Node`, `AcebottESP32Max_Node`        | G    | No jumpers (0.5 s)     |
| `ESP8266_Node`, `ESP12F_SimpleIRPositiveTrigger` | G | No jumpers (0.5 s)   |
| `ESP01_Node`                                | G    | Defaults match (1 s pulse, 15 s cooldown) |

Any other duration from 0.1 to 15 s: leave D8/D9 open and set `main_ms` on the config page.

## Card A: Arduino 101 Negative-Only Sketches

Old sketches: `Arduino101_LaserTagIRNegativeTrigger` (500 ms, 5 s, 10 s, 60 s) and `Arduino101_SimpleIRNegativeTrigger`
(500 ms, 15 s, 60 s). New sketch: **`Arduino101_LaserTagIRTrigger`**.

| Wire                    | Old pin | New pin | Action        |
| ----------------------- | ------- | ------- | ------------- |
| IR receiver OUT         | D3      | D3      | No change     |
| IR receiver VCC / GND   | 3.3V / GND | 3.3V / GND | No change |
| Relay module IN         | D4      | D4      | No change     |
| Relay module VCC / GND  | 5V / GND | 5V / GND | No change    |
| Relay status LED        | D6      | D6      | No change     |
| **IR status LED**       | **D5**  | **D7**  | **Move**      |

Steps:

- [ ] Unplug USB and power.
- [ ] Move the IR status LED wire (with its 330 Ohm resistor) from **D5** to **D7**.
- [ ] Confirm nothing is left on D5 (or add an active-HIGH load there if wanted).
- [ ] Add duration jumpers from the [Quick Lookup](#quick-lookup) (5 s: D8 to GND; 10 s: D9 to GND; 15 s: both).
- [ ] 60 s props: also add a [timer relay](#60-second-props-add-a-timer-relay).
- [ ] Flash, configure, and test ([checklist](#flash-configure-and-test-every-board)).

If the LED is not moved, it still works but lights with the relay instead of with IR activity.

## Card B: Arduino 101 Dual Trigger Sketches

Old sketches: `Arduino101_LaserTagIRDualTrigger` (500 ms) and `Arduino101_LaserTagIRDualTrigger5s`.
New sketch: **`Arduino101_LaserTagIRTrigger`**.

| Wire                    | Old pin | New pin | Action        |
| ----------------------- | ------- | ------- | ------------- |
| IR receiver OUT         | D3      | D3      | No change     |
| Negative relay IN       | D4      | D4      | No change     |
| Positive load           | D5      | D5      | No change     |
| Relay status LED        | D6      | D6      | No change     |
| IR status LED           | D7      | D7      | No change     |

Steps:

- [ ] **No rewiring.**
- [ ] 5 s boards: add a jumper from **D8 to GND**.
- [ ] Flash, configure, and test ([checklist](#flash-configure-and-test-every-board)).

## Card C: UNO Negative-Only Sketches

Old sketches: `UNO_LaserTagIRNegativeTrigger` and `UNO_SimpleIRNegativeTrigger` (both 500 ms).
New sketch: **`UNO_LaserTagIRTrigger`**.

| Wire                    | Old pin | New pin | Action        |
| ----------------------- | ------- | ------- | ------------- |
| **IR receiver OUT**     | **D2**  | **D3**  | **Move**      |
| IR receiver VCC / GND   | 5V / GND | 5V / GND | No change    |
| **Relay module IN**     | **D8**  | **D4**  | **Move**      |
| Relay module VCC / GND  | 5V / GND | 5V / GND | No change    |
| **IR status LED**       | **D5**  | **D7**  | **Move**      |
| Relay status LED        | D6      | D6      | No change     |

Steps:

- [ ] Unplug USB and power.
- [ ] Move the IR receiver signal wire from **D2** to **D3**.
- [ ] Move the relay module IN wire from **D8** to **D4**.
- [ ] Move the IR status LED wire from **D5** to **D7**.
- [ ] Confirm **D2 and D8 are now empty**. D8 is a duration jumper input; a wire left on it can change the duration.
- [ ] Flash, configure, and test ([checklist](#flash-configure-and-test-every-board)).

## Card D: UNO Dual Trigger Sketch

Old sketch: `UNO_LaserTagIRDualTrigger` (500 ms). New sketch: **`UNO_LaserTagIRTrigger`**.

| Wire                    | Old pin | New pin | Action        |
| ----------------------- | ------- | ------- | ------------- |
| **IR receiver OUT**     | **D2**  | **D3**  | **Move**      |
| **Negative relay IN**   | **D8**  | **D4**  | **Move**      |
| Positive load (5 V)     | D5      | D5      | No change     |
| Relay status LED        | D6      | D6      | No change     |
| IR status LED           | D7      | D7      | No change     |

Steps:

- [ ] Unplug USB and power.
- [ ] Move the IR receiver signal wire from **D2** to **D3**.
- [ ] Move the negative relay IN wire from **D8** to **D4**.
- [ ] Confirm **D2 and D8 are now empty**.
- [ ] Flash, configure, and test ([checklist](#flash-configure-and-test-every-board)).

## Card E: Nano Sketches

Old sketches: `Nano_LaserTagIRNegativeTrigger` (500 ms) and `Nano_LaserTagIRNegativeTrigger5s`.
New sketch: **`Nano_LaserTagIRTrigger`**.

| Wire                    | Old pin       | New pin | Action                    |
| ----------------------- | ------------- | ------- | ------------------------- |
| **IR receiver OUT**     | **D2**        | **D3**  | **Move**                  |
| IR receiver VCC / GND   | 5V / GND      | 5V / GND | No change                |
| Relay module IN         | D4            | D4      | No change                 |
| Relay status LED        | D13 (onboard) | D6      | Optional: add LED on D6   |
| IR status LED           | (none)        | D7      | Optional: add LED on D7   |

> **Check for a 3-channel relay board (old `Nano_Node` wiring).** If relay inputs IN2/IN3 are wired to **D5/D6**, disconnect
> them. Those pins are now outputs that sit LOW at idle, which would turn relays 2 and 3 **ON** permanently.

Steps:

- [ ] Unplug USB and power.
- [ ] Move the IR receiver signal wire from **D2** to **D3**.
- [ ] If a multi-channel relay board is connected, remove any wires on **D5** and **D6** (keep IN1 on D4).
- [ ] Optional: add status LEDs. D6 -> 330 Ohm -> LED (+), LED (-) -> GND for relay. Same on D7 for IR.
- [ ] 5 s boards: add a jumper from **D8 to GND**.
- [ ] Flash, configure, and test ([checklist](#flash-configure-and-test-every-board)). If upload fails with `not in sync`,
  add `-Fqbn arduino:avr:nano:cpu=atmega328old`.

The onboard D13 LED is no longer used, because it blinks during bootloader startup.

## Card F: Arduino 101 Wired-Input Sketches

Old sketches: `Arduino101_TimedTriggerRelay` (never deployed) and `Arduino101_Node` (RF node). New sketch:
**`Arduino101_LaserTagIRTrigger`** with its **D2 wired input**. RF (433 MHz) receive is not carried over.

| Wire                         | Old pin                         | New pin | Action                                 |
| ---------------------------- | ------------------------------- | ------- | -------------------------------------- |
| Relay module IN              | D4                              | D4      | No change                              |
| **Positive trigger source**  | **D7**                          | **D2**  | **Move**; add 10 kOhm D2-to-GND; `wired_input` = 2 |
| **Negative trigger source**  | **D8**                          | **D2**  | **Move**; `wired_input` = 1            |
| TimedTriggerRelay trigger LED | D5                             | -       | **Remove** (D5 is now the positive output) |
| **Node relays 2 / 3 IN**     | **D5 / D6**                     | -       | **Disconnect** (would turn ON at idle) |
| Relay status LED             | D6 (TimedTriggerRelay)          | D6      | No change                              |
| Duration selector            | A0-A4 (TimedTriggerRelay)       | D8/D9   | **Move** to D8/D9 jumpers              |
| RF receiver data             | D2 (Node)                       | -       | **Remove** before enabling the wired input |

Steps:

- [ ] Unplug USB and power.
- [ ] Remove anything on D5 and D6 that is not an LED or active-HIGH load (Node relay 2/3 inputs, TimedTriggerRelay
  trigger LED). Node: remove the RF receiver from D2.
- [ ] D2 accepts **one** trigger source. Move the positive source (from D7) **or** the negative source (from D8) to D2.
  If both were used, wire the second (negative) source to D10 instead; it fires D11, so it needs its own relay.
- [ ] Positive source: add a 10 kOhm resistor from D2 to GND.
- [ ] Remove the A0-A4 selector wiring. Add D8/D9 jumpers for 5/10/15 s. 30/60 s: add a
  [timer relay](#60-second-props-add-a-timer-relay).
- [ ] Flash, then on the config page set **Wired trigger input** to Negative or Positive and click **Apply & Save**.
- [ ] Test ([checklist](#flash-configure-and-test-every-board)), plus: trigger D2 and confirm the relay pulse.

## Card G: WiFi Boards (ESP32, Acebott, ESP8266, ESP-01)

New sketches: **`ESP32_LaserTagIRTrigger`** (ESP32 DevKit and Acebott ESP32-Max), **`ESP8266_LaserTagIRTrigger`**
(NodeMCU / ESP-12F), **`ESP01_LaserTagIRTrigger`**. Pins are GPIO numbers (NodeMCU labels in brackets). The old node
sketches **held** a relay while an input stayed active; the new sketch fires a timed pulse instead.

**ESP32 DevKit / Acebott ESP32-Max (old `ESP32_Node`, `AcebottESP32Max_Node`):**

| Wire                        | Old GPIO | New GPIO | Action                                                        |
| --------------------------- | -------- | -------- | ------------------------------------------------------------- |
| IR receiver OUT             | 19       | 19       | No change                                                     |
| Relay 1 IN (IR pulse)       | 4        | 4        | No change (main negative output)                              |
| **Relay 2 IN**              | **5**    | 25       | **Move** to GPIO25 (aux output) or disconnect                 |
| **Relay 3 IN**              | **18**   | -        | **Disconnect**: GPIO18 is now the positive output and idles LOW, which turns an active-LOW relay ON |
| **Positive trigger source** | **16**   | 27       | **Move** to GPIO27 with its 10 kOhm pull-down; `wired_input` = positive |
| **Negative trigger source** | **17**   | 27 or 26 | **Move** to GPIO27 (`wired_input` = negative, fires the main output) or GPIO26 (fires GPIO25) |

GPIO16/17 are now the DFPlayer serial pins, so nothing else may stay on them.

**ESP8266 NodeMCU (old `ESP8266_Node`):**

| Wire                  | Old pin   | New pin   | Action                                                              |
| --------------------- | --------- | --------- | ------------------------------------------------------------------- |
| **IR receiver OUT**   | **D4 (2)** | D5 (14)  | **Move**                                                            |
| Relay 1 IN            | D1 (5)    | D1 (5)    | No change (main negative output)                                    |
| **Relay 2 IN**        | **D2 (4)** | -        | **Disconnect**: D2 is now the positive output and idles LOW (an active-LOW relay would turn ON) |
| **Relay 3 IN**        | **D5 (14)** | -       | **Disconnect**: D5 is now the IR input                              |
| **Trigger inputs**    | **D6, D7** | -        | **Disconnect**: D6/D7 are now the duration jumpers                  |

**ESP-12F (old `ESP12F_SimpleIRPositiveTrigger`):**

| Wire                  | Old GPIO | New GPIO      | Action                                                         |
| --------------------- | -------- | ------------- | -------------------------------------------------------------- |
| **IR receiver OUT**   | **4**    | 14            | **Move**                                                       |
| **Positive load**     | **5**    | 4             | **Move** (GPIO5 is now the negative output)                    |
| **IR status LED**     | **12**   | 16            | **Move**: GPIO12 is now duration jumper A; an LED there reads as a jumper |
| **Relay status LED**  | **14**   | 2 (onboard)   | **Remove** (GPIO14 is now the IR input); the onboard LED is used |

**ESP-01 (old `ESP01_Node`, IR sensor on GPIO2):**

| Wire              | Old GPIO | New GPIO | Action                                                                   |
| ----------------- | -------- | -------- | ------------------------------------------------------------------------ |
| IR sensor OUT     | 2        | 2        | No change (default `wired_input` = 2, positive, as before)               |
| Relay driver      | 0        | 0        | No change: GPIO0 still goes **HIGH** for 1 s (NPN driver + 10 kOhm pull-up) |

Steps (every WiFi board):

- [ ] Unplug power, make the wiring changes above.
- [ ] Flash over USB: `.\tools\arduino\arduino.ps1 upload <Board>_LaserTagIRTrigger -Port COMx`.
- [ ] Join the setup AP `Haunt-<device_id>` (password `hauntsetup`), open `http://192.168.4.1`, log in `admin` / `hauntadmin`.
- [ ] Change the admin and AP passwords. Set the router SSID/password and the MQTT username/password.
- [ ] Optional: set **Device ID** to the old MQTT ID (for example `2026-ht-node-01`) so the controller keeps the same topic.
- [ ] ESP-01: the defaults already match the old node (sensor HIGH, 1 s pulse, 15 s cooldown); change them only if needed.
- [ ] **Save**, **Reboot**, and confirm the board joins the router and MQTT shows connected.
- [ ] Test with the gun (or the ESP-01 sensor) and with **Test main** on the settings page.

## 60-Second Props: Add a Timer Relay

The new sketch tops out at 15 s. For a 60 s (or any long or adjustable) on-time, put a programmable timer relay module
between the trigger board and the prop:

```text
Board D4 -> active-LOW relay module (0.5 s click) -> relay COM/NO -> timer relay TRIGGER input
Timer relay output (set to 60 s, "one-shot on trigger" mode) -> prop
```

- [ ] Leave D8/D9 open (0.5 s pulse).
- [ ] Wire the relay's COM/NO contacts to the timer's trigger input, or follow the timer module's manual for a dry-contact trigger.
- [ ] Set the timer mode to a single on-pulse per trigger, and set the time on the timer.
- [ ] Confirm the timer ignores or restarts on extra triggers the way you want. The board itself ignores shots during a pulse.

## Flash, Configure, and Test (Every Board)

Flash (board on the bench or at the prop with a laptop):

```powershell
cd C:\GitHub\2026_TiTG_Haunted_Theatre
.\tools\arduino\arduino.ps1 ports
.\tools\arduino\arduino.ps1 upload Arduino101_LaserTagIRTrigger -Port COMx
```

Use `UNO_LaserTagIRTrigger` or `Nano_LaserTagIRTrigger` for those boards. Arduino 101: if the upload stalls, press
MASTER RESET as soon as the upload starts.

Configure:

- [ ] Open `tools/web-config/index.html` in Chrome or Edge and click **Connect** (close any other serial monitor first).
- [ ] Set **Prop name** to the prop's location (for example `Coffin Lid`).
- [ ] Confirm **Duration jumpers** and **Main pulse now** match the [Quick Lookup](#quick-lookup).
- [ ] Click **Apply & Save**.

Test:

- [ ] **Test main** clicks the relay for the expected time.
- [ ] A laser tag shot triggers (IR LED on D7 flashes, relay LED on D6 lights, relay clicks).
- [ ] Shots during the pulse are ignored, and the relay turns off on time.
- [ ] A phone filming the receiver does **not** trigger.
- [ ] Power-cycle: no relay clicks on during boot.

If a gun does not trigger: on the config page, check **Print raw timings of unrecognized IR**, click **Apply**, fire the
gun a few times, and save the serial log for review.

## Deployed Board Worksheet

Last known sketch per board, from `SESSION_NOTES.md`. COM numbers vary by USB port and machine, so match boards by
serial number (shown by `arduino.ps1 ports`). Fill in the prop location as you go.

| Board serial           | Old sketch                               | Card / duration      | Prop location | Done |
| ---------------------- | ---------------------------------------- | -------------------- | ------------- | ---- |
| 101 `...610005K`       | `Arduino101_LaserTagIRNegativeTrigger5s` | A / D8 (5 s)         |               | [ ]  |
| 101 `...61000Q4`       | `Arduino101_LaserTagIRNegativeTrigger`   | A / none (0.5 s)     |               | [ ]  |
| 101 `...61000V7`       | `Arduino101_LaserTagIRNegativeTrigger60s` | A / none + timer    |               | [ ]  |
| 101 `...61000R4`       | `Arduino101_LaserTagIRNegativeTrigger5s` | A / D8 (5 s)         |               | [ ]  |
| 101 `...61000V8`       | `Arduino101_LaserTagIRNegativeTrigger`   | A / none (0.5 s)     |               | [ ]  |
| 101 `...61000LS`       | `Arduino101_LaserTagIRNegativeTrigger`   | A / none (0.5 s)     |               | [ ]  |
| 101 `...61000GH`       | `Arduino101_LaserTagIRNegativeTrigger`   | A / none (0.5 s)     |               | [ ]  |
| 101 `...6100019`       | `Arduino101_LaserTagIRNegativeTrigger5s` | A / D8 (5 s)         |               | [ ]  |
| 101 `...61000QP`       | `Arduino101_LaserTagIRNegativeTrigger`   | A / none (0.5 s)     |               | [ ]  |
| 101 `...61000RS`       | `Arduino101_LaserTagIRNegativeTrigger`   | A / none (0.5 s)     |               | [ ]  |
| 101 `...6100017`       | `Arduino101_LaserTagIRDualTrigger5s`     | B / D8 (5 s)         |               | [ ]  |
| 101 `...61000V9`       | `Arduino101_LaserTagIRDualTrigger`       | B / none (0.5 s)     |               | [ ]  |
| 101 `...61000HZ`       | `Arduino101_LaserTagIRDualTrigger`       | B / none (0.5 s)     |               | [ ]  |
| UNO `...18204`         | `UNO_LaserTagIRNegativeTrigger`          | C / none (0.5 s)     |               | [ ]  |
| Elegoo UNO (COM21)     | `UNO_LaserTagIRDualTrigger` (upload failed 2026-10-21) | D / none (0.5 s) |       | [ ]  |
| Nano (CH340, no serial) | `Nano_LaserTagIRNegativeTrigger` / `...5s` | E / check banner  |               | [ ]  |
| ______________________ | ________________________________________ | ____________________ |               | [ ]  |
| ______________________ | ________________________________________ | ____________________ |               | [ ]  |

Notes:

- `...6100017` received `Arduino101_LaserTagIRDualTrigger5s` on 2026-10-23 (it was previously `...NegativeTrigger`).
  If it was installed on a negative-only prop with the IR LED on D5, use Card A instead.
- `...61000RS` was left in DFU mode after its last upload; confirm it boots before rewiring.
- The Nano entries cannot be told apart by serial number. Check each Nano's startup banner (old 500 ms vs 5 s) before
  choosing the jumper.
- Boards not listed (for example any running `UNO_SimpleIRNegativeTrigger` or `Arduino101_SimpleIRNegativeTrigger*`):
  use the [Quick Lookup](#quick-lookup) and add them to the blank rows.
