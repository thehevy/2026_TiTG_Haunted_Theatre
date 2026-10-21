# Session Notes

**Created:** 2026-10-20
**Last Updated:** 2026-10-20

Running handoff notes for resuming work on this repo from any machine. Update the
[Current Status](#current-status) and [Next Steps](#next-steps) sections at the end of each session.

## Table of Contents

- [Current Status](#current-status)
- [Next Steps](#next-steps)
- [Resume on This Machine](#resume-on-this-machine)
- [Resume on a New Machine](#resume-on-a-new-machine)
- [Toolchain Reference](#toolchain-reference)
- [Compile Verification Baseline](#compile-verification-baseline)
- [Known Gotchas](#known-gotchas)
- [Session Log](#session-log)

## Current Status

As of 2026-10-20:

- arduino-cli compile/upload tooling is in the repo and working: `tools/arduino/arduino.ps1`,
  `tools/arduino/sketches.json`, and `.vscode/tasks.json`.
- All 17 sketches compile with arduino-cli (see [baseline](#compile-verification-baseline)).
- `ESP8266_Node.ino` build break is fixed (removed an orphaned block after `loop()`).
- **First Arduino 101 upload succeeded** (2026-10-20): `Arduino101_SimpleIRNegativeTrigger60s` on **COM6** via arduino-cli.
  The uploader also flashed the board's BLE firmware, and serial heartbeat output is confirmed. Other board families are not yet upload-tested.
- The Arduino 101 DFU driver (WinUSB via Zadig) is installed on the setup machine. The Intel `dpinst` drivers do not install on
  Windows 11 (unsigned catalogs). See `SETUP_GUIDE.md` > Step 4 > *Windows 11: Intel drivers rejected, use Zadig instead*.
- VS Code C/C++ IntelliSense is configured via `.build/compile_commands.json`. That file is machine-specific; generate it with
  `.\tools\arduino\arduino.ps1 intellisense` (already generated on the setup machine).
- On the setup machine, COM3 is Intel AMT SOL and COM4/COM5 are Bluetooth serial links. Always confirm board ports with
  `ports` after plugging in; a board can get a new COM number on a different USB port, so match by serial number.
- Board inventory (all running laser tag sketches, heartbeat confirmed after upload). Arduino 101 boards unless noted:

  | Port  | Board serial  | Sketch                                     | Relay per shot | Notes                                      |
  | ----- | ------------- | ------------------------------------------ | -------------- | ------------------------------------------ |
  | COM6  | `...610005K`  | `Arduino101_LaserTagIRNegativeTrigger5s`   | 5 s            |                                            |
  | COM7  | `...61000Q4`  | `Arduino101_LaserTagIRNegativeTrigger`     | 500 ms         | Bench-verified: gun triggers, iPhone ignored |
  | COM8  | `...61000V7`  | `Arduino101_LaserTagIRNegativeTrigger60s`  | 60 s           |                                            |
  | COM9  | `...61000R4`  | `Arduino101_LaserTagIRNegativeTrigger5s`   | 5 s            |                                            |
  | COM10 | `...61000V8`  | `Arduino101_LaserTagIRNegativeTrigger`     | 500 ms         |                                            |
  | COM12 | `...61000LS`  | `Arduino101_LaserTagIRNegativeTrigger`     | 500 ms         |                                            |
  | COM13 | `...61000GH`  | `Arduino101_LaserTagIRNegativeTrigger10s`  | 10 s           |                                            |
  | COM14 | `...18204`    | `UNO_LaserTagIRNegativeTrigger`            | 500 ms         | **UNO R3 clone** (USB VID 3343 PID 0043; arduino-cli shows "Unknown") |

## Next Steps

1. Install the COM7 laser tag board in the prop and test hits at real range and angles. If needed, restrict team/mode with
   `GUN_CODE_MASK`/`GUN_CODE_VALUE` (see its `PINOUT.md`). Flash the same sketch to other IR boards that should respond only to the gun.
   For a 5, 10, or 60-second relay pulse, use `Arduino101_LaserTagIRNegativeTrigger5s`, `...10s`, or `...60s`.
2. Upload and bench-test `Arduino101_TimedTriggerRelay` (see its `PINOUT.md` > Validation):
   `.\tools\arduino\arduino.ps1 upload Arduino101_TimedTriggerRelay -Port COM6`.
3. Wire and bench-test the 60 s IR negative trigger on the Arduino 101 (COM6): confirm D4 goes LOW for 60 s on an IR signal
   and check the D5/D6 status LEDs. Pinout: `components/arduino101/Arduino101_SimpleIRNegativeTrigger60s/PINOUT.md`.
4. Upload other sketches the same way as needed:

   ```powershell
   .\tools\arduino\arduino.ps1 ports
   .\tools\arduino\arduino.ps1 upload <SketchName> -Port COMx
   .\tools\arduino\arduino.ps1 monitor -Port COMx
   ```

5. Do a first upload for one board of each other family (UNO/Nano, ESP32, ESP8266) and record results in the
   [Session Log](#session-log).
6. On any **other** Windows 11 PC used for Arduino 101 uploads, install the DFU driver with Zadig first
   (`SETUP_GUIDE.md` > Step 4 > *Windows 11: Intel drivers rejected, use Zadig instead*).

## Resume on This Machine

The toolchain is already installed. Open a **new** terminal (or restart VS Code) so the PATH change is picked up, then:

```powershell
cd C:\GitHub\2026_TiTG_Haunted_Theatre
git pull
arduino-cli version
.\tools\arduino\arduino.ps1 list
.\tools\arduino\arduino.ps1 ports
```

Continue with [Next Steps](#next-steps).

## Resume on a New Machine

Windows, PowerShell 5.1 or later.

1. Clone the repo:

   ```powershell
   git clone https://github.com/thehevy/2026_TiTG_Haunted_Theatre.git C:\GitHub\2026_TiTG_Haunted_Theatre
   cd C:\GitHub\2026_TiTG_Haunted_Theatre
   ```

2. Proxy (Intel network only). Skip this step on networks with no proxy:

   ```powershell
   $env:HTTPS_PROXY = 'http://proxy-dmz.intel.com:912'
   $env:HTTP_PROXY  = 'http://proxy-dmz.intel.com:912'
   ```

3. Install arduino-cli (latest release) and add it to the user PATH:

   ```powershell
   [Net.ServicePointManager]::SecurityProtocol = 'Tls12'
   $proxyArgs = @{}; if ($env:HTTPS_PROXY) { $proxyArgs.Proxy = $env:HTTPS_PROXY }
   $rel   = Invoke-RestMethod https://api.github.com/repos/arduino/arduino-cli/releases/latest @proxyArgs
   $asset = $rel.assets | Where-Object name -like '*Windows_64bit.zip' | Select-Object -First 1
   $dest  = "$env:LOCALAPPDATA\Programs\arduino-cli"
   New-Item -ItemType Directory -Force $dest | Out-Null
   $zip = "$env:TEMP\$($asset.name)"
   Invoke-WebRequest $asset.browser_download_url -OutFile $zip -UseBasicParsing @proxyArgs
   Expand-Archive $zip -DestinationPath $dest -Force; Remove-Item $zip
   $userPath = [Environment]::GetEnvironmentVariable('Path', 'User')
   if ($userPath -notlike "*$dest*") { [Environment]::SetEnvironmentVariable('Path', ($userPath.TrimEnd(';') + ";$dest"), 'User') }
   & "$dest\arduino-cli.exe" version
   ```

4. Initialize arduino-cli and install all cores and libraries from `tools/arduino/sketches.json`:

   ```powershell
   & "$env:LOCALAPPDATA\Programs\arduino-cli\arduino-cli.exe" config init
   .\tools\arduino\arduino.ps1 setup
   ```

   `setup` uses `$env:HTTPS_PROXY` as the arduino-cli proxy. To clear a proxy that is no longer needed, run `.\tools\arduino\arduino.ps1 setup -Proxy ''`.

5. Install Arduino IDE 1.8.19, which is needed for the Arduino 101 Firmware Updater:

   ```powershell
   $proxyArgs = @{}; if ($env:HTTPS_PROXY) { $proxyArgs.Proxy = $env:HTTPS_PROXY }
   $zip = "$env:TEMP\arduino-1.8.19-windows.zip"
   Invoke-WebRequest https://downloads.arduino.cc/arduino-1.8.19-windows.zip -OutFile $zip -UseBasicParsing @proxyArgs
   Expand-Archive $zip -DestinationPath "$env:LOCALAPPDATA\Programs" -Force; Remove-Item $zip
   ```

   Launch `%LOCALAPPDATA%\Programs\arduino-1.8.19\arduino.exe`. If you are behind a proxy, set it in File > Preferences > Network
   (host `proxy-dmz.intel.com`, port `912`).

6. Verify all sketches compile (the first ESP32 build takes several minutes):

   ```powershell
   .\tools\arduino\arduino.ps1 compile all
   ```

   Compare the results with the [baseline](#compile-verification-baseline).

7. Generate the IntelliSense compile database for VS Code, then reload the VS Code window:

   ```powershell
   .\tools\arduino\arduino.ps1 intellisense
   ```

   Continue with [Next Steps](#next-steps).

## Toolchain Reference

| Item | Version / location |
| --- | --- |
| arduino-cli | 1.5.1, `%LOCALAPPDATA%\Programs\arduino-cli` |
| Arduino IDE (Curie firmware updates) | 1.8.19, `%LOCALAPPDATA%\Programs\arduino-1.8.19` |
| arduino-cli config | `%LOCALAPPDATA%\Arduino15\arduino-cli.yaml` |
| Board cores (shared with IDE) | `%LOCALAPPDATA%\Arduino15\packages` |
| Sketchbook libraries (shared with IDE) | `Documents\Arduino\libraries` (OneDrive-redirected on the setup machine) |
| `Intel:arc32` | 2.0.4 (pinned; 2.0.6 is available but docs prefer 2.0.4) |
| `arduino:avr` | 1.8.8 |
| `esp32:esp32` | 3.3.12 |
| `esp8266:esp8266` | 3.1.2 |
| Libraries | rc-switch 2.6.4, IRremote 4.7.1, PubSubClient 2.8, VL53L1X (Pololu) 1.3.1, ESP32Servo 3.2.1, DFRobotDFPlayerMini 1.0.6 |

Helper commands (`.\tools\arduino\arduino.ps1 <action>`):

| Action | Purpose |
| --- | --- |
| `setup` | Board URLs, proxy, cores, libraries |
| `list` | Sketch names and FQBNs |
| `ports` | Connected boards and COM ports |
| `compile <name\|all>` | Compile one or all sketches |
| `upload <name> -Port COMx` | Compile and upload. Add `-Fqbn` to override the board. |
| `monitor -Port COMx` | Serial monitor (default 115200) |
| `intellisense [name\|all]` | Generate `.build/compile_commands.json` for VS Code C/C++ IntelliSense |

VS Code: **Terminal > Run Task > Arduino: ...**. `Ctrl+Shift+B` compiles the selected sketch.

When you add a new sketch, add it to `tools/arduino/sketches.json` **and** to the `sketch` input list in `.vscode/tasks.json`.

## Compile Verification Baseline

Result of `.\tools\arduino\arduino.ps1 compile all` on 2026-10-20:

| Sketch | FQBN | Flash used |
| --- | --- | --- |
| HauntedHouseRF | `Intel:arc32:arduino_101` | 23,300 B (14%) |
| Arduino101_Node | `Intel:arc32:arduino_101` | 19,476 B (12%) |
| Arduino101_SimpleIRNegativeTrigger | `Intel:arc32:arduino_101` | 14,960 B (9%) |
| Arduino101_SimpleIRNegativeTrigger15s | `Intel:arc32:arduino_101` | 14,964 B (9%) |
| Arduino101_SimpleIRNegativeTrigger60s | `Intel:arc32:arduino_101` | 14,964 B (9%) |
| Arduino101_TimedTriggerRelay | `Intel:arc32:arduino_101` | 16,792 B (10%) |
| Arduino101_LaserTagIRNegativeTrigger | `Intel:arc32:arduino_101` | 17,576 B (11%) |
| Arduino101_LaserTagIRNegativeTrigger5s | `Intel:arc32:arduino_101` | 17,588 B (11%) |
| Arduino101_LaserTagIRNegativeTrigger10s | `Intel:arc32:arduino_101` | 17,588 B (11%) |
| Arduino101_LaserTagIRNegativeTrigger60s | `Intel:arc32:arduino_101` | 17,588 B (11%) |
| UNO_Node | `arduino:avr:uno` | 10,522 B (32%) |
| UNO_SimpleIRNegativeTrigger | `arduino:avr:uno` | 9,414 B (29%) |
| UNO_LaserTagIRNegativeTrigger | `arduino:avr:uno` | 5,106 B (15%), RAM 628 B (30%) |
| Nano_Node | `arduino:avr:nano:cpu=atmega328` | 10,226 B (33%) |
| ESP32_Node | `esp32:esp32:esp32` | 957,243 B (73%) |
| ESP32_SmartTracking_Node | `esp32:esp32:esp32` | 309,263 B (23%) |
| ESP32S3_SmartTracking_Node | `esp32:esp32:esp32s3` | 351,721 B (26%) |
| AcebottESP32Max_Node | `esp32:esp32:esp32` | 956,675 B (72%) |
| WemosD1R32_Node | `esp32:esp32:esp32` | 293,775 B (22%) |
| WemosD1R32_SimpleIRPositiveTrigger | `esp32:esp32:esp32` | 292,667 B (22%) |
| ESP8266_Node | `esp8266:esp8266:nodemcuv2` | 259,748 B IROM (24%) |
| ESP12F_SimpleIRPositiveTrigger | `esp8266:esp8266:nodemcuv2` | 242,924 B IROM (23%) |
| ESP01_Node | `esp8266:esp8266:generic` | 236,804 B IROM (22%) |

Expected warning: on Arduino 101 sketches, `rc-switch` says it "may be incompatible" with the `arc32` architecture. It still compiles. Test RF receive reliability on hardware.

## Known Gotchas

- **COM5 is not guaranteed to be the Arduino 101.** On the setup machine it was a Bluetooth serial link, and the board was COM6. Always run `ports` first.
- **Arduino 101 on Windows 11:** Intel `dpinst` drivers fail (`0x80030000`, unsigned catalogs). Install WinUSB for VID `8087` PID `0ABA`
  with Zadig (Device > Create New Device). Without it, uploads time out at `Waiting for device...` even with MASTER RESET.
- **`Serial port busy` / `Access denied` on upload:** another program holds the COM port. Close serial monitors and unplug/replug the board.
- **Arduino 101 interrupts:** `attachInterrupt(..., CHANGE)` only works on SoC GPIO pins D2, D4, D7, D8, and D10-D13. On ARC GPIO
  pins (D0, D1, D3, D5, D6, D9, A0-A5), the core silently turns it into falling-edge only. rc-switch therefore needs D2, and IR/RF
  receivers on D3 must be polled (as `Arduino101_LaserTagIRNegativeTrigger` does).
- **rc-switch never reports code 0:** `available()` treats 0 as "nothing received".
- **IR "phone false triggers":** the simple IR sketches fire on any IR activity, including iPhone Face ID. Use
  `Arduino101_LaserTagIRNegativeTrigger`, which only accepts the laser tag gun's frame format.
- **COM port locked by a serial monitor:** uploads fail with `Serial port busy` / time out. Close the monitor before uploading.
- **Port auto-detect:** `upload` without `-Port` only works when exactly one connected board matches the sketch's board type.
  Boards with CH340/CP210x USB bridges (most ESP boards) are not identified. Pass `-Port` for them.
- **PowerShell 5.1 stderr:** ESP cores write size reports to stderr. `arduino.ps1` merges stderr so these aren't treated as failures. Keep that behavior if you edit `Invoke-Cli`.
- **Proxy:** `proxy-dmz.intel.com:912` applies only on the Intel network. Off-network, run `setup -Proxy ''` and clear the IDE 1.8.19 proxy setting.
- **Arduino 101 uploads:** close the serial monitor first. If an upload or firmware update stalls, use the MASTER RESET timing workaround.
- **Nano clones:** if an upload fails with sync errors, retry with `-Fqbn arduino:avr:nano:cpu=atmega328old`.
- **ESP32:** if an upload stops at `Connecting...`, hold BOOT until writing starts.
- **ESP-01:** GPIO0 must be held LOW at reset to enter flash mode. Use a 3.3V serial adapter.
- **IntelliSense "Configure IntelliSense" prompt:** run `.\tools\arduino\arduino.ps1 intellisense`, reload VS Code, and select the
  `Arduino (arduino-cli)` configuration. The Intel Curie core can't produce a compile database on its own (a post-link hook fails),
  so the script falls back to a full compile for Arduino 101 sketches.
- **IntelliSense and prototypes:** IntelliSense reads the raw `.ino`, without the prototypes Arduino generates. A call to a function
  defined later in the file may be flagged even though it compiles.

## Session Log

### 2026-10-20: arduino-cli toolchain setup

- Reviewed the repo docs and Copilot instructions. There were no prior session notes.
- Installed arduino-cli 1.5.1 and Arduino IDE 1.8.19 (portable zip, with a Start Menu shortcut). Pre-seeded the IDE
  `preferences.txt` with the proxy and the ESP32/ESP8266 board URLs.
- Installed the cores and libraries listed in [Toolchain Reference](#toolchain-reference).
- Added `tools/arduino/arduino.ps1`, `tools/arduino/sketches.json`, and `.vscode/tasks.json`. `.gitignore` now
  whitelists only `.vscode/tasks.json`.
- Fixed `ESP8266_Node.ino`: removed an orphaned block after `loop()` that referenced the undeclared `relay2LockoutUntil`.
- Documented the CLI workflow in `SETUP_GUIDE.md` and `README.md`.
- Commit `4a866bf` pushed to `main`.
- Hardware uploads are still untested. See [Next Steps](#next-steps).

### 2026-10-20: VS Code IntelliSense fix

- The C/C++ extension showed a **Configure IntelliSense** prompt because no configuration existed and `.ino` wasn't associated with C++.
- Added the `intellisense` action to `arduino.ps1`. It exports each sketch's real compile flags from arduino-cli, expands `@response`
  files and `-iprefix` includes (ESP32), force-includes `Arduino.h`, and maps the flags to each `.ino` in
  `.build/compile_commands.json`.
- Committed `.vscode/c_cpp_properties.json` and `.vscode/settings.json`, and added the **Arduino: Refresh IntelliSense** task.
  `.build/` is gitignored.

### 2026-10-20: First Arduino 101 upload (COM6)

- Target: `Arduino101_SimpleIRNegativeTrigger60s` to the Arduino 101 on COM6 (VID `8087` PID `0AB6`).
- First attempt failed. COM6 was busy (fixed by unplug/replug), and the DFU-mode device (PID `0ABA`) had no driver.
- Intel `dpinst-amd64.exe` was rejected on Windows 11 Enterprise 26200 (unsigned catalogs, `0x800B0100`).
- Installed WinUSB for `8087:0ABA` with Zadig 2.9 (official libwdi v1.5.1 release, Authenticode-signed by Akeo Consulting),
  saved to `%LOCALAPPDATA%\Programs\zadig`.
- Upload succeeded. The uploader also flashed the BLE firmware. Serial heartbeat confirmed:
  `Heartbeat: IR input D3, negative trigger output D4`.
- Documented the Zadig fix in `SETUP_GUIDE.md` Step 4.

### 2026-10-20: New Arduino101_TimedTriggerRelay sketch

- New folder `components/arduino101/Arduino101_TimedTriggerRelay/` with the sketch and `PINOUT.md`.
- Behavior: a positive (D7, active HIGH, external 10 kOhm pull-down) or negative (D8, active LOW, internal pull-up) trigger
  pulses an active-LOW relay on D4 (high-Z idle). The duration selector on A0-A4 (ground one) chooses 1/5/15/30/60 s; with
  nothing grounded it defaults to 1 s. The trigger is edge-based, triggers during a pulse are ignored, and an input held
  active at boot does not fire. Status LEDs: D5 trigger, D6 relay.
- Added to `sketches.json` and the VS Code task picker. Compiles with `--warnings all` clean. Not yet uploaded or bench-tested.
- Fixed an `arduino.ps1 intellisense <name>` bug: a partial refresh dropped the existing entries
  (Windows PowerShell 5.1 `ConvertFrom-Json` array handling).

### 2026-10-20: Laser tag IR trigger (Arduino101_LaserTagIRNegativeTrigger, COM7)

- Goal: the relay fires only when the **Sharper Image laser tag gun** hits the KY-022 IR receiver on D3. The simple IR sketches
  trigger on any IR, including an iPhone's Face ID/proximity IR.
- Detour: "code 0x0" was first read as a 433 MHz RF code (RF polling decoder, commit `97ee56f`, in git history). The hardware is
  actually IR (KY-022 + laser tag gun), so the sketch was rewritten as a polled IR decoder.
- Captured the gun's signal: a 6 ms leader mark + 0.5 ms space, then 9 pulse-width bits (1.5 ms mark = 1, 0.5 ms mark = 0, MSB
  first), 5 frames per shot, ~4.5 ms apart. Codes seen: `0x14F`, `0x143`, `0x14B`, `0x145`, `0x147`. The `10100` header is
  fixed; the low bits follow the team/mode setting.
- Sketch: triggers on any 9-bit gun frame received at least twice per shot (`GUN_FRAME_BITS`, `GUN_CODE_MASK`/`GUN_CODE_VALUE`
  to restrict). NEC remotes are printed but ignored; unrecognized IR is reported and ignored. Same D3/D4/D5/D6 wiring as the simple sketches.
- Bench test on the second Arduino 101 (**COM7**, serial `...61000Q4`): 10 of 10 shots triggered, and an iPhone IR burst was ignored. The user confirmed it works.
- Renamed the user's copy `Arduino101_SimpleIRNegativeTriggerCode0x0` to `Arduino101_LaserTagIRNegativeTrigger` (git mv).
- Fixed `Arduino101_Node`: removed the dead `received == 0x0` branch (rc-switch can't report 0), and corrected the header text and
  `components/arduino101/PINOUT.md`. Behavior is unchanged: any code pulses Relay 1 while `pulseCode` is unset.
- `Arduino101_Node` uploaded to COM7 but produced no serial output. Likely cause: its rc-switch `CHANGE` interrupt on an unconnected
  (floating) D2 (the boards' receiver is on D3). Not investigated further; the COM7 board now runs the laser tag sketch.
- `arduino.ps1 intellisense <name>` now also drops entries for sketch files that no longer exist.

### 2026-10-20: Laser tag 5 s variant

- Added `Arduino101_LaserTagIRNegativeTrigger5s`: identical to the laser tag sketch except `TRIGGER_PULSE_MS = 5000` and the
  `(5 s)` banner/title. Same wiring and decoder. Uploaded to **COM6** (heartbeat confirmed).
- Added `Arduino101_LaserTagIRNegativeTrigger60s`: same, with `TRIGGER_PULSE_MS = 60000` and the `(60 s)` banner/title.
  Uploaded to **COM8** (third Arduino 101, serial `...61000V7`); heartbeat confirmed.
- Uploaded `Arduino101_LaserTagIRNegativeTrigger5s` to **COM9** (fourth Arduino 101, serial `...61000R4`); heartbeat confirmed.
- Uploaded `Arduino101_LaserTagIRNegativeTrigger` (500 ms) to **COM10** (fifth Arduino 101, serial `...61000V8`); heartbeat confirmed.
- Uploaded `Arduino101_LaserTagIRNegativeTrigger` (500 ms) to **COM12** (sixth Arduino 101, serial `...61000LS`); heartbeat confirmed.
- Added `Arduino101_LaserTagIRNegativeTrigger10s`: same decoder, `TRIGGER_PULSE_MS = 10000`, `(10 s)` banner/title.
  Uploaded to **COM13** (seventh Arduino 101, serial `...61000GH`); heartbeat confirmed.

### 2026-10-20: UNO R3 laser tag port

- Added `components/uno/UNO_LaserTagIRNegativeTrigger` (500 ms): the same polled laser tag decoder as the Arduino 101 sketch,
  with UNO wiring matching `UNO_SimpleIRNegativeTrigger`: IR on D2 (receiver at 5 V), relay on D8, LEDs on D5/D6.
- The relay output is driven directly (HIGH = off, LOW = on) because the UNO is 5 V. D8 is set HIGH before it becomes an
  output, so the relay never blips at boot. No IRremote dependency.
- Compiles clean with `--warnings all` (5,106 B flash, 628 B RAM).
- Uploaded to **COM14**, an UNO R3 clone (USB VID 3343 / PID 0043, serial `...18204`). The upload worked with `arduino:avr:uno`
  even though arduino-cli lists the board as "Unknown". Startup banner and heartbeat confirmed; gun shots not yet tested on the UNO.
