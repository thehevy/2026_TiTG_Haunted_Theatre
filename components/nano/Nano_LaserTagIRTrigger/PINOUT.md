# Nano Laser Tag IR Trigger Pinout

**Created:** 2026-10-26
**Last Updated:** 2026-10-26

Target sketch: `components/nano/Nano_LaserTagIRTrigger/Nano_LaserTagIRTrigger.ino`

Same source, pins, and behavior as the Arduino 101 version. Full pinout, duration table, IR formats, wiring, and
validation: `components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`.

## Nano Differences

- Board: `arduino:avr:nano:cpu=atmega328`. CH340 clones show as "USB-SERIAL CH340"; always pass `-Port`. If upload fails
  with `not in sync`, add `-Fqbn arduino:avr:nano:cpu=atmega328old`.
- 5 V logic: D5 drives 5 V when active. D4 and D11 are driven HIGH (5 V) when idle and LOW when active.
- Power the IR receiver from 5V.
- The onboard D13 LED is not used (it blinks during bootloader startup). Status LEDs are external on D6/D7.

| Pin | Function                                   |
| --- | ------------------------------------------ |
| D2  | Wired trigger input (off by default; `wired_input`) |
| D3  | IR receiver OUT                            |
| D4  | Main negative trigger (active LOW)         |
| D5  | Main positive trigger (active HIGH, 5 V)   |
| D6  | Relay status LED                           |
| D7  | IR status LED                              |
| D8  | Duration select A (jumper to GND)          |
| D9  | Duration select B (jumper to GND)          |
| D10 | Aux negative input                         |
| D11 | Aux negative output (active LOW, 500 ms)   |
| A0  | DFPlayer TX (optional MP3)                 |
| A1  | DFPlayer RX via 1 kOhm (optional MP3)      |

**Rewiring from `Nano_LaserTagIRNegativeTrigger` (500 ms / 5 s):** IR D2 -> D3 (relay stays on D4). Add LEDs on D6/D7.
For 5 s, jumper D8 to GND.

Upload: `.\tools\arduino\arduino.ps1 upload Nano_LaserTagIRTrigger -Port COMx`

Optional DFPlayer Mini (sound) uses A0/A1 on the Nano; see the **MP3 Sound** section of the Arduino 101 `PINOUT.md` for
wiring, the SD card layout, and line-out to an amp.
