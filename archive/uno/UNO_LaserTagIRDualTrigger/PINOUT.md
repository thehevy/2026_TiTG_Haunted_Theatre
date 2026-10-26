# Arduino UNO Laser Tag IR Dual Trigger Pinout

**Created:** 2026-10-21
**Last Updated:** 2026-10-21

Target sketch:

- `components/uno/UNO_LaserTagIRDualTrigger/UNO_LaserTagIRDualTrigger.ino`

Any shot from the Sharper Image laser tag gun fires **two outputs at the same time for 500 ms**:

- **D8 negative trigger:** pulled LOW (for an active-LOW 5 V relay module)
- **D5 positive trigger:** driven HIGH at **5 V** (for an active-HIGH relay, a MOSFET/transistor, or a prop controller input)

This is `UNO_LaserTagIRNegativeTrigger` with a positive output added on D5; the IR activity LED moved from D5 to D7. Phones and TV
remotes are ignored. The Arduino 101 equivalent is `Arduino101_LaserTagIRDualTrigger`, which uses D4/D5 at 3.3 V.

## Board Setup

- Arduino board target: `Arduino Uno` (`arduino:avr:uno`)
- Port: confirm with `.\tools\arduino\arduino.ps1 ports`
- Serial Monitor: `115200` baud
- No external libraries.
- Close the Serial Monitor before uploading. If an upload fails with `not in sync: resp=0x00`, check that nothing is on D0/D1 and that the RESET button makes the "L" LED flicker (bootloader present). See [Troubleshooting](#troubleshooting).

## Function Map

| Function                | UNO pin | Signal / behavior                                            |
| ----------------------- | ------- | ------------------------------------------------------------ |
| IR receiver signal      | D2      | Idles HIGH; LOW while IR is received                         |
| Negative trigger output | D8      | Idle 5 V HIGH; **LOW** for 500 ms after a laser tag shot     |
| Positive trigger output | D5      | Idle LOW (0 V); **HIGH (5 V)** for 500 ms after a laser tag shot |
| Relay status LED        | D6      | On while the triggers are active                             |
| IR status LED           | D7      | On for 100 ms after any IR activity (including ignored IR)   |
| IR receiver power       | 5V      | KY-022 runs from 5 V on the UNO                              |
| Common ground           | GND     | Shared by board, receiver, loads, and LEDs                   |

**Rewiring from `UNO_LaserTagIRNegativeTrigger`:** move the IR status LED from D5 to D7, then connect the positive-trigger load to D5.

## Serial Output Path

- At startup: `Arduino UNO laser tag IR dual trigger ready`
- `Trigger: any laser tag shot (9-bit frame) -> negative D8 LOW + positive D5 HIGH for 500 ms`
- `Status LEDs: IR D7, relay D6`
- Every 5 seconds: `Heartbeat: IR input D2, negative trigger output D8, positive trigger output D5, trigger: any laser tag shot, shots 0, other codes 0`
- Each gun shot: `Laser tag shot: code 0x147 (9 bits 101000111, 5 frames) -> TRIGGER: negative D8 LOW + positive D5 HIGH for 500 ms`

## Positive Trigger Output (D5) Electrical Limits

- D5 drives **5 V** when active and **0 V** when idle. It is a logic signal: keep the load under **20 mA** (40 mA absolute maximum).
- Suitable loads: a 5 V active-HIGH relay **module** (with its own driver transistor), a logic-level MOSFET gate, an NPN transistor base through 1 kOhm, or the trigger input of a prop controller / sound board (common ground).
- Do **not** drive a bare relay coil, motor, solenoid, or LED strip directly from D5.
- D5 floats during reset and in the bootloader (about 1 s after power-up or upload). Add a 10 kOhm pull-down from D5 to GND if the load must never see a glitch.

## Wiring

| UNO pin | Connect to                                      | Notes                                  |
| ------- | ----------------------------------------------- | -------------------------------------- |
| D2      | IR receiver OUT/S                               | IR signal input                        |
| 5V      | IR receiver VCC, relay module VCC               | 5 V supply                             |
| D8      | Active-LOW relay module IN                      | Idle 5 V HIGH                          |
| D5      | Active-HIGH relay IN / MOSFET gate / prop input | 5 V logic HIGH when active             |
| D6      | 330 Ohm -> relay LED anode                      | LED cathode to GND                     |
| D7      | 330 Ohm -> IR status LED anode                  | LED cathode to GND                     |
| GND     | Receiver, relays, loads, LEDs                   | Grounds must be common                 |

```text
Arduino UNO                               IR receiver (KY-022 / VS1838B / TL1838)
5V -------------------------------------> VCC
D2 <------------------------------------- OUT / S
GND ------------------------------------> GND

Arduino UNO                               5 V relay module (active LOW)
D8 -------------------------------------> IN
5V -------------------------------------> VCC
GND ------------------------------------> GND

Arduino UNO                               Positive-trigger load (active HIGH, 5 V logic)
D5 -------------------------------------> IN / gate / trigger input
GND ------------------------------------> GND
(optional) D5 -- 10 kOhm --> GND          keeps D5 LOW during reset/boot

Arduino UNO                               Status LEDs
D6 -------------------------------------> 330 Ohm -> relay LED (+) ; LED (-) -> GND
D7 -------------------------------------> 330 Ohm -> IR LED (+) ; LED (-) -> GND
```

## Output Logic

| State                         | D8 (negative) | D5 (positive) | D6 LED |
| ----------------------------- | ------------- | ------------- | ------ |
| Idle / no shot                | HIGH (5 V)    | LOW (0 V)     | Off    |
| 500 ms after a laser tag shot | LOW (0 V)     | HIGH (5 V)    | On     |

- Both outputs switch together and share `TRIGGER_PULSE_MS`. Each new shot restarts the 500 ms timer.
- Both outputs are set to their idle levels before they become outputs, so neither load blips at boot.
- Signal format and tuning constants (`GUN_FRAME_BITS`, `GUN_CODE_MASK`, `GUN_CODE_VALUE`, `TRIGGER_PULSE_MS`) are the same as in `UNO_LaserTagIRNegativeTrigger`; see its `PINOUT.md`.

## Troubleshooting

| Symptom                                         | Likely cause / what to check                                                      |
| ----------------------------------------------- | --------------------------------------------------------------------------------- |
| Upload: `not in sync: resp=0x00`, "L" LED never flickers on RESET | ATmega328P not running its bootloader: reseat the DIP chip (notch toward the analog pins), check for bent legs, disconnect D0/D1 and shields; otherwise reburn the bootloader (Arduino as ISP) or replace the chip |
| IR LED (D7) flashes, outputs do not switch      | IR is not a gun shot (phone/remote): read the serial line                         |
| Relay LED (D6) on, D8 relay does not click      | Check D8 -> IN, relay 5V/GND, active-LOW module                                   |
| Relay LED (D6) on, D5 load does not switch      | Check D5 wiring and common GND; confirm the load is active HIGH                   |

## Validation

1. Upload: `.\tools\arduino\arduino.ps1 upload UNO_LaserTagIRDualTrigger -Port COMx` (close the Serial Monitor first).
2. Open the Serial Monitor at `115200`. Confirm the banner shows `negative D8 LOW + positive D5 HIGH` and a heartbeat every 5 seconds.
3. With a meter to GND, confirm D5 reads about 0 V and D8 about 5 V at idle.
4. Fire the gun. Confirm `-> TRIGGER`, D5 at about 5 V and D8 at about 0 V for 0.5 s, and both loads switch.
5. Hold a phone near the receiver and unlock it with Face ID. Confirm neither output fires.

Compiles clean (5,182 B flash, 628 B RAM). First upload attempt (Elegoo UNO R3 on COM21, 2026-10-21) failed with
`not in sync: resp=0x00` because that board's bootloader was not responding (hardware issue, not the sketch). Not yet tested on hardware.
