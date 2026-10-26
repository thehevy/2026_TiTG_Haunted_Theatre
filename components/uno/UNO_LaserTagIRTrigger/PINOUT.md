# UNO Laser Tag IR Trigger Pinout

**Created:** 2026-10-26
**Last Updated:** 2026-10-26

Target sketch: `components/uno/UNO_LaserTagIRTrigger/UNO_LaserTagIRTrigger.ino`

Same source, pins, and behavior as the Arduino 101 version. Full pinout, duration table, IR formats, wiring, and
validation: `components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md`.

## UNO Differences

- Board: `arduino:avr:uno`. UNO R3 clones may show as "Unknown" in `arduino.ps1 ports`; always pass `-Port`.
- 5 V logic: D5 drives 5 V when active. D4 and D11 are driven HIGH (5 V) when idle and LOW when active.
- Power the IR receiver from 5V.

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

**Rewiring from `UNO_LaserTagIRNegativeTrigger` / `UNO_SimpleIRNegativeTrigger`:** IR D2 -> D3, relay D8 -> D4, IR LED
D5 -> D7. **From `UNO_LaserTagIRDualTrigger`:** IR D2 -> D3, negative relay D8 -> D4. D2 is now the optional wired trigger
input; leave it empty unless `wired_input` is set.

Upload: `.\tools\arduino\arduino.ps1 upload UNO_LaserTagIRTrigger -Port COMx`

Optional DFPlayer Mini (sound) uses A0/A1 on the UNO; see the **MP3 Sound** section of the Arduino 101 `PINOUT.md` for
wiring, the SD card layout, and line-out to an amp.
