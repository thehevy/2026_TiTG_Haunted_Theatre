# Session Notes

**Created:** 2026-10-20
**Last Updated:** 2026-10-26

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

As of 2026-10-26:

- **New 2026-10-26: WiFi builds `ESP32_`, `ESP8266_`, `ESP01_LaserTagIRTrigger`** (compiled, not yet uploaded). Same shared
  `.ino` as the 101/UNO/Nano builds; each folder's `board_config.h` holds pins/features; `arduino.ps1 sync` copies the
  Arduino 101 `.ino` to all six folders. WiFi: joins the router (default `TITG2026HT`) or runs a WPA2 setup AP
  `Haunt-<device_id>` (no SSID, router unreachable for 20 s, BOOT/FLASH held 3 s, 3 quick resets, or serial `AP`); settings
  page with basic auth (`admin`), per-boot form token, passwords never shown; MQTT `haunt/<id>/status|trigger|event|heartbeat`
  with broker login; OTA via page upload or `arduino.ps1 upload <sketch> -Port <ip>`. ESP IR capture is interrupt-driven.
  ESP32 build also covers the Acebott ESP32-Max (header is GPIO-labeled). ESP8266: no wired/aux/DFPlayer. ESP-01: one input
  (GPIO2 = IR receiver, or a digital IR sensor via `wired_input`) + one active-LOW output, 1 MB flash. Defaults
  `hauntsetup` / `hauntadmin` must be changed on first setup. The old WiFi node sketches are archived.
- **New 2026-10-26: consolidated `*_LaserTagIRTrigger`** for Arduino 101, UNO, and Nano (one identical source, compiled, not
  yet uploaded). Any valid IR code (laser tag, TV, LED-strip remotes) -> D4 LOW + D5 HIGH for 0.5/5/10/15 s via D8/D9 jumpers
  (live) or the saved `main_ms`; optional D2 wired input (off by default); aux D10 edge -> D11 LOW 500 ms; triggers during a
  pulse are ignored (250 ms re-arm); optional DFPlayer Mini sound; settings over USB with `tools/web-config/index.html`.
  All earlier laser tag, simple IR, node, RF, timed relay, CH9311, and WeMos sketches are in `archive/` with issue notes in
  each header. **Every board in the inventory below still runs an archived sketch** and needs rewiring/reflashing: use
  `LASER_TAG_TRANSITION_GUIDE.md`.
- **New 2026-10-25: `Nano_MP3TriggerPlayer`** (uploaded to **COM10**, CH340 Nano, new bootloader; banner + heartbeat confirmed;
  not yet tested with the MP3 module): Nano + DIYables Serial MP3
  Player (YX5300, SoftwareSerial D6 TX / D7 RX). Negative triggers D2-D5 + PIR D8 each play their own file (`01/001`-`01/005`)
  once; all triggers are ignored until the file finishes (0x3D finished reply, status poll, `MAX_PLAY_MS` backup).
- **New 2026-10-23: `Arduino101_PIRLaserSoundTrigger`** (compiled, not yet uploaded or bench-tested): 3 PIR (D2-D4) + 1 laser
  (D5) each play their own **DFPlayer Mini** track (`Serial1` on D0/D1, BUSY on D10). Each sensor's negative trigger output
  (D6-D9) stays LOW while its sound plays. One sound at a time (other triggers ignored, not queued), 15 s cooldown per
  sensor, 10 s warm-up, high-Z idle outputs.
- All 10 active sketches compile with arduino-cli (`compile all`, 2026-10-26).
- arduino-cli compile/upload tooling is in the repo and working: `tools/arduino/arduino.ps1`,
  `tools/arduino/sketches.json`, and `.vscode/tasks.json`.
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
  | COM13 | `...61000GH`  | `Arduino101_LaserTagIRNegativeTrigger`     | 500 ms         | Was 10 s; reflashed to 500 ms 2026-10-20  |
  | COM14 | `...18204`    | `UNO_LaserTagIRNegativeTrigger`            | 500 ms         | **UNO R3 clone** (USB VID 3343 PID 0043; arduino-cli shows "Unknown") |
  | COM15 | CH340         | `Nano_LaserTagIRNegativeTrigger` / `...5s` | 500 ms / 5 s   | **Nano clone(s)** (CH340, new bootloader). CH340 has no serial number, so COM15 = that USB socket. 500 ms uploaded twice 2026-10-20; **5 s** uploaded 2026-10-23 (most recent Nano in that socket) |
  | COM16 | `...6100019`  | `Arduino101_LaserTagIRNegativeTrigger5s`   | 5 s            |                                            |
  | COM17 | `...61000QP`  | `Arduino101_LaserTagIRNegativeTrigger`     | 500 ms         | BLE firmware updated during first upload   |
  | COM18 | `...61000RS`  | `Arduino101_LaserTagIRNegativeTrigger`     | 500 ms         | Upload + BLE update succeeded 2026-10-21; stuck in DFU afterwards, heartbeat **not yet verified** |
  | COM19 | `...6100017`  | `Arduino101_LaserTagIRDualTrigger5s`       | 5 s            | D4 LOW + D5 HIGH; uploaded 2026-10-23 (was `...NegativeTrigger`) |
  | COM22 | `...61000V9`  | `Arduino101_LaserTagIRDualTrigger`         | 500 ms         | D4 LOW + D5 HIGH; BLE updated on first upload |
  | COM23 | `...61000HZ`  | `Arduino101_LaserTagIRDualTrigger`         | 500 ms         | D4 LOW + D5 HIGH; BLE updated on first upload |

## Next Steps

1. Bench-test `Arduino101_LaserTagIRTrigger` (see its `PINOUT.md` > Validation) with **every** laser tag gun, a TV remote,
   an LED-strip remote, and an iPhone. For any gun that does not fire, check **Print raw timings** on the config page (`SET timings=1`) and save the serial
   timings. Then reflash the inventory boards (use jumpers for 5/10 s instead of separate sketches).
1. Decide the config/OTA direction for non-WiFi boards (Web Serial page, ESP-01 esp-link bridge, or ESP32 replacement).
   **Chosen: Web Serial page** (`tools/web-config/index.html`, settings in EEPROM). Bench-test it with the sketch.
1. Bench-test `Nano_MP3TriggerPlayer` (see its `PINOUT.md` > Validation). SD card: folder `01`, files `001*.mp3`-`005*.mp3`.
   If playback cuts off ~2 s in, the module clone misreports status: set `STATUS_POLL_MS = 0`.
2. Bench-test `Arduino101_PIRLaserSoundTrigger` with a DFPlayer Mini (SD card: `/mp3/0001.mp3`-`0004.mp3`, BUSY to D10).
3. Bench-test the D2 wired input on `Arduino101_LaserTagIRTrigger` (`wired_input` = 1 with a switch to GND, then 2 with a
   10 kOhm pull-down and a 3.3 V/5 V signal). Confirm it is ignored while set to 0 (off) with D2 floating.
4. Bench-test one WiFi board (ESP32 first; see `components/esp32/ESP32_LaserTagIRTrigger/PINOUT.md` > Validation): setup
   AP, settings page, router join, MQTT with a broker login, page and `-Port <ip>` OTA. Then the ESP-01 with its IR sensor
   (`wired_input` = 2, `main_ms` = 1000, `rearm_ms` = 14000) and an active-LOW relay input on GPIO0.
6. Upload other sketches the same way as needed:

   ```powershell
   .\tools\arduino\arduino.ps1 ports
   .\tools\arduino\arduino.ps1 upload <SketchName> -Port COMx
   .\tools\arduino\arduino.ps1 monitor -Port COMx
   ```

7. Do a first upload for one board of each other family (UNO/Nano, ESP32, ESP8266) and record results in the
   [Session Log](#session-log).
8. On any **other** Windows 11 PC used for Arduino 101 uploads, install the DFU driver with Zadig first
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
| `upload <name> -Port COMx` | Compile and upload. Add `-Fqbn` to override the board. `-Port <ip>` uploads to a WiFi board over the network. |
| `sync` | Copy the shared laser tag `.ino` (Arduino 101 copy) into the other five `*_LaserTagIRTrigger` folders |
| `monitor -Port COMx` | Serial monitor (default 115200) |
| `intellisense [name\|all]` | Generate `.build/compile_commands.json` for VS Code C/C++ IntelliSense |

VS Code: **Terminal > Run Task > Arduino: ...**. `Ctrl+Shift+B` compiles the selected sketch.

When you add a new sketch, add it to `tools/arduino/sketches.json` **and** to the `sketch` input list in `.vscode/tasks.json`.

## Compile Verification Baseline

Result of `.\tools\arduino\arduino.ps1 compile all` on 2026-10-26 (active sketches only; archived sketches are not built):

| Sketch | FQBN | Flash used |
| --- | --- | --- |
| Arduino101_LaserTagIRTrigger | `Intel:arc32:arduino_101` | 30,060 B (19%) |
| Arduino101_PIRLaserSoundTrigger | `Intel:arc32:arduino_101` | 17,968 B (11%) |
| UNO_LaserTagIRTrigger | `arduino:avr:uno` | 15,558 B (48%), RAM 1,169 B (57%) |
| Nano_LaserTagIRTrigger | `arduino:avr:nano:cpu=atmega328` | 15,560 B (50%), RAM 1,169 B (57%) |
| Nano_MP3TriggerPlayer | `arduino:avr:nano:cpu=atmega328` | 7,748 B (25%), RAM 469 B (22%) |
| ESP32_LaserTagIRTrigger | `esp32:esp32:esp32` | 1,045,463 B (79%), RAM 54,288 B (16%) |
| ESP8266_LaserTagIRTrigger | `esp8266:esp8266:nodemcuv2` | 342,704 B IROM (32%) |
| ESP01_LaserTagIRTrigger | `esp8266:esp8266:generic:eesz=1M64` | 343,180 B IROM (32%) |
| ESP32_SmartTracking_Node | `esp32:esp32:esp32` | 309,263 B (23%) |
| ESP32S3_SmartTracking_Node | `esp32:esp32:esp32s3` | 351,721 B (26%) |

## Known Gotchas

- **COM5 is not guaranteed to be the Arduino 101.** On the setup machine it was a Bluetooth serial link, and the board was COM6. Always run `ports` first.
- **Arduino 101 on Windows 11:** Intel `dpinst` drivers fail (`0x80030000`, unsigned catalogs). Install WinUSB for VID `8087` PID `0ABA`
  with Zadig (Device > Create New Device). Without it, uploads time out at `Waiting for device...` even with MASTER RESET.
- **`Serial port busy` / `Access denied` on upload:** another program holds the COM port. Close serial monitors and unplug/replug the board.
- **Arduino 101 interrupts:** `attachInterrupt(..., CHANGE)` only works on SoC GPIO pins D2, D4, D7, D8, and D10-D13. On ARC GPIO
  pins (D0, D1, D3, D5, D6, D9, A0-A5), the core silently turns it into falling-edge only. IR receivers on D3 must be
  polled (as `Arduino101_LaserTagIRTrigger` does).
- **IR "phone false triggers":** sketches that fire on any IR activity (or on IRremote command 0x00, which unknown IR
  decodes to) are triggered by iPhone Face ID/camera IR. `*_LaserTagIRTrigger` only accepts fully decoded frames.
- **COM port locked by a serial monitor:** uploads fail with `Serial port busy` / time out. Close the monitor before uploading.
- **Nano (CH340) upload hangs / port stays locked:** a stuck `avrdude` can't be killed while blocked in the CH340 driver.
  Unplug and replug the Nano. Bootloaders vary per clone; the default is `cpu=atmega328` (new), so use `-Fqbn ...:cpu=atmega328old` for old ones.
- **Port auto-detect:** `upload` without `-Port` only works when exactly one connected board matches the sketch's board type.
  Boards with CH340/CP210x USB bridges (most ESP boards) are not identified. Pass `-Port` for them.
- **PowerShell 5.1 stderr:** ESP cores write size reports to stderr. `arduino.ps1` merges stderr so these aren't treated as failures. Keep that behavior if you edit `Invoke-Cli`.
- **Proxy:** `proxy-dmz.intel.com:912` applies only on the Intel network. Off-network, run `setup -Proxy ''` and clear the IDE 1.8.19 proxy setting.
- **Arduino 101 uploads:** close the serial monitor first. If an upload or firmware update stalls, use the MASTER RESET timing workaround.
- **Arduino 101 BLE firmware update fails with `exit status 74`:** the `arduino101load` 2.3.0 package ships an empty `firmwares/`
  folder, so boards whose BLE firmware needs updating fail and drop into DFU mode (recoverable). Fix: copy `firmwares/2.0.0/`
  (`ble_core.bin`, `quark.bin`) from the cached `staging/packages/arduino101load-2.0.1-windows_386.tar.bz2` into
  `%LOCALAPPDATA%\Arduino15\packages\Intel\tools\arduino101load\2.3.0\firmwares\`. Reinstalling the core may remove it.
  Even with the files, the first BLE flash occasionally fails or logs `Error during download` yet ends in `SUCCESS`; re-upload
  until the log shows `BLE version: verified`.
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

### 2026-10-20: Nano laser tag port

- Added `components/nano/Nano_LaserTagIRNegativeTrigger` (500 ms): same decoder as the UNO port. Pins follow `Nano_Node`
  (user choice): IR on D2, relay on D4 (Nano_Node Relay 1). D5/D6 are left untouched, so a Nano_Node 3-channel relay board is
  safe. The onboard D13 LED shows relay status; there are no external status LEDs.
- FQBN was first set to `arduino:avr:nano:cpu=atmega328old` (the user expected the old bootloader).
- Uploaded to **COM15** (CH340 Nano clone). The old-bootloader upload failed (`not in sync: resp=0x8c`); the new-bootloader
  `cpu=atmega328` upload succeeded, and the banner and heartbeat were confirmed. The default FQBN is now `cpu=atmega328`.
- An old-bootloader retry hung and left an orphaned `avrdude` that `Stop-Process -Force` could not kill (blocked in the CH340
  driver), locking COM15. Unplugging and replugging the Nano released it. A brief automatic bootloader-fallback in `arduino.ps1`
  was removed because the failing attempt can hang instead of failing. `upload` now prints a Nano bootloader/replug hint instead.

### 2026-10-21: Laser tag dual-trigger variant

- Added `Arduino101_LaserTagIRDualTrigger`: the 500 ms laser tag sketch with an added **positive trigger output on D5**
  (HIGH 3.3 V while triggered, LOW idle) alongside the existing negative D4 output (LOW while triggered, high-Z idle). Both
  share `TRIGGER_PULSE_MS`. The IR activity LED moved D5 -> D7; the relay LED stays on D6.
- D5 is a 3.3 V logic signal only: drive a 3.3 V-compatible active-HIGH relay, a logic-level MOSFET, or a prop input.
  Optional 10 kOhm pull-down on D5 for a glitch-free boot.
- Compiles clean with `--warnings all` (17,684 B). Upload to COM19 (`...6100017`) was attempted twice on 2026-10-21 but failed
  before flashing with `Serial port busy` (port held, likely by a serial monitor); the user unplugged the board and deferred the
  upload. **COM19 still runs `Arduino101_LaserTagIRNegativeTrigger` (500 ms).**

### 2026-10-21: WeMos D1 R32 upload on COM15 blocked (CH340)

- Tried `WemosD1R32_SimpleIRPositiveTrigger` (`esp32:esp32:esp32`) on **COM15** (CH340, VID 1A86 PID 7523). The sketch compiles
  (292,667 B); the upload fails before connecting: `PermissionError(13, 'A device attached to the system is not functioning')`.
- Windows reports the device "working properly", but the port can't be opened at any baud rate (9600-921600). Not fixed by
  replugging, disconnecting the board wiring, or `pnputil /restart-device` (elevated). CH340 driver is 3.9.2024.9, the same one
  that worked for the Nano on COM15.
- Next to try: a different USB port (direct, not a hub), a different **data** cable, then another PC. If those fail, suspect the
  board's CH340 chip, or try an older WCH CH340 driver (3.5/3.8).
- Retried later: moved to another USB socket (**COM20**, instance `...&0&5`). Same `device not functioning` open failure, so the
  PC socket is ruled out. The Nano's CH340 works with the same driver, so the fault is this WeMos's CH340 or its cable. A likely cause is a
  counterfeit CH340 rejected by WCH driver 3.8/3.9 (fix: roll back to driver 3.5, which affects all CH340 devices on the PC).
  The user stopped here. Next: swap the cable, then try another PC or a driver rollback.

### 2026-10-21: Dual trigger uploads (COM21 UNO, COM22 Arduino 101)

- **COM22** (Arduino 101 `...61000V9`): `Arduino101_LaserTagIRDualTrigger` uploaded (BLE firmware also flashed). Heartbeat confirms
  negative D4 + positive D5. This is the first board running the dual trigger.
- **COM21** turned out to be an **Elegoo UNO R3** (USB `2341:0043`), not an Arduino 101. Created `components/uno/UNO_LaserTagIRDualTrigger`
  (UNO laser tag sketch plus positive D5 output at 5 V; IR LED moved D5 -> D7; negative stays on D8). Compiles (5,182 B).
- The COM21 upload failed: `not in sync: resp=0x00`, no serial output, and the "L" LED does not flicker on RESET. The ATmega328P (socketed
  DIP) is not running its bootloader. Next: reseat or check the chip and anything on D0/D1, reburn the bootloader (Arduino as ISP), or replace the chip.

### 2026-10-23: Nano 5 s laser tag

- Pulled `f5a8b80` (user): new `Arduino101_PIRLaserSoundTrigger` and `Arduino101_PIRLaserCH9311Trigger`. Both compile here; neither is uploaded yet.
- The user created `components/nano/Nano_LaserTagIRNegativeTrigger5s` (copy of the Nano 500 ms sketch with `TRIGGER_PULSE_MS = 5000`).
  Registered it in `sketches.json` and the task list, and updated its `PINOUT.md` timings. Uploaded to **COM15** (CH340 Nano, new bootloader);
  the banner confirms `LOW for 5000 ms`.

### 2026-10-23: PIR/laser sound trigger sketches (DFPlayer and CH9311)

Work on the second machine (`D:\GitHub\Arduino101`). This entry also covers 2026-10-19 work on that machine, done before these notes existed.

- **2026-10-19 recap** (commits `5d40b38`, `6b024df`): created the `*_SimpleIR*Trigger` sketches with IR/relay status LEDs
  and a `PINOUT.md` for each (Arduino 101 and UNO negative trigger; ESP-12F and D1 R32 positive trigger). Added the Arduino 101
  15 s/60 s variants, moved `HauntedHouseRF.ino` into its own folder, committed the ESP32-S3 smart tracking skull, and found and
  fixed the empty `arduino101load` BLE firmware folder (see [Known Gotchas](#known-gotchas)). Arduino 101 uploads from that day
  were later reflashed with laser tag sketches on the other machine, so they are not in the board inventory.
- Pulled 31 commits from the other machine (laser tag sketches, tooling, these notes).
- Added `Arduino101_PIRLaserSoundTrigger` (DFPlayer Mini). User requirements: up to 3 PIRs + 1 laser, each with its own negative
  trigger output; each sound must finish before another can start; 15 s cooldown per sensor. User choices: the output stays LOW
  for the whole sound; triggers during playback are ignored, not queued; the laser module is HIGH on beam-break; the PIRs are AM312 (3.3 V).
  The DFPlayer uses `Serial1` (D0/D1) instead of the UNO-style SoftwareSerial on 10/11, and BUSY on D10 detects end of sound.
- Added `Arduino101_PIRLaserCH9311Trigger`. The **CH9311-FL** is a 6-35 V, 20 W voice playback module with 8 MB flash, not a
  microcontroller. A first version used serial commands (`7E 04 <track> EF`) taken from a summary of seller listings. The seller
  manual showed the module has **no RX/TX**, only K1-K10 key inputs (CH9311 = low-level, pull Kn to GND; CH9312 = high-level/VIN),
  so the sketch was rewritten to press K1-K4. "Busy" is only an LED, so sound length comes from `SENSOR_SOUND_MS`.
  The K pins' idle voltage is undocumented: measure before wiring (direct if 5 V or less, else NPN transistors).
- Both sketches are registered in `sketches.json` and `tasks.json`. Commit `f5a8b80` (the first two pushes got GitHub
  `Internal Server Error`; the third succeeded).
- IntelliSense showed `cannot open source file "Arduino.h"` because `.build/compile_commands.json` had never been generated on
  this machine. `arduino.ps1 intellisense` regenerated all 28 entries.
- Untracked leftovers on this machine: per-sketch `.vscode/c_cpp_properties.json` files, including three folders under old sketch
  names (`Arduino101_SimpleIRPositiveTrigger`, `ESP12F_SimpleIRNegativeTrigger`, `WemosD1R32_SimpleIRNegativeTrigger`) that
  contain only that file. Safe to delete.

### 2026-10-25: Nano MP3 trigger player

- Added `components/nano/Nano_MP3TriggerPlayer` for the DIYables Serial MP3 Player (YX5300; newbiely/arduinogetstarted tutorial
  wiring, raw `7E FF 06 cmd 00 dh dl EF` frames, no library). Plays by folder/file (`0x0F`), so SD copy order does not matter.
- Triggers: D2-D5 active LOW (`INPUT_PULLUP`, edge on press), D8 PIR active HIGH (edge on rise, 30 s warm-up). One file at a
  time; triggers during playback are ignored, not queued; 1 s post-play lockout. The YX5300 has no BUSY pin, so the end is detected
  from the `0x3D` reply, a `0x42` status poll, and a 5 min `MAX_PLAY_MS` safety unlock.
- Registered in `sketches.json` and `tasks.json`. Compiles clean (7,748 B flash, 469 B RAM).
- Uploaded to **COM10** on this machine (CH340 Nano, `cpu=atmega328` new bootloader). Startup banner and heartbeat confirmed.
  The first upload attempt gave no output within 90 s but did not leave `avrdude` running; a direct re-run succeeded.

### 2026-10-26: Laser tag consolidation after the haunt run

- Feedback from three haunt nights: some laser tag guns reached the receiver but never fired (old rule: exactly 9 bits, 2
  matching frames); continuous fire restarted the timer so edge-triggered props fired only once; too many fixed-duration variants.
- New `Arduino101_/UNO_/Nano_LaserTagIRTrigger` (identical source; board block selects high-Z idle on the 101). Decoders: pulse
  width 6-24 bits (guns, Sony), pulse distance 12-64 bits (NEC, Samsung, Panasonic, LED strips). One clean frame is enough
  (`PW_MIN_FRAMES = 1`). Pins: IR D3, D4 neg, D5 pos, LEDs D6/D7, duration D8/D9, aux D10 -> D11.
- 60 s retired (programmable timer relay instead). Archived 14 sketches under `archive/<board>/`; removed from `sketches.json`
  and `tasks.json`. Compiles: 101 18,872 B; UNO/Nano 6,552 B flash, 677 B RAM.
- Config direction chosen: **Web Serial** (option A). Added EEPROM settings (`name`, `main_ms` used when no jumper,
  `aux_ms`, `rearm_ms`, `ir_formats`, `pw_min_frames`, `timings`) and serial commands (`GET`/`SET`/`SAVE`/`DEFAULTS`/`TEST`/
  `HELP`) to the sketch, plus a self-contained page `tools/web-config/index.html` (Chrome/Edge, no network access, CSP locked).
  Jumpers override `main_ms`. Compiles: 101 25,920 B; UNO/Nano 10,822 B flash, 871 B RAM (42%). Browser-side flashing
  (USB OTA) not started.
- Archived `Arduino101_Node` (and its board-level `components/arduino101/PINOUT.md`) to `archive/arduino101/Arduino101_Node/`:
  iPhone false triggers, floating positive input D7 (no pull-down) holding Relay 2 on when unwired, and RF "any code" default.
  `Arduino101_TimedTriggerRelay` has the same `INPUT` (no pull-down) on D7 and relies on an external 10 kOhm pull-down.
- Archived `Arduino101_TimedTriggerRelay` (never deployed). Its job moved into the unified sketch as a **D2 wired input**
  (`wired_input`: 0 off default, 1 negative/INPUT_PULLUP, 2 positive/INPUT + external pull-down) that fires D4/D5 with the
  same duration, lockout, and re-arm. 30/60 s dropped. Config struct is now version 2 (older saved settings reset to
  defaults). Compiles: 101 26,864 B; UNO/Nano 11,800 B flash, 898 B RAM (43%).
- Added optional **DFPlayer Mini** sound to the unified sketch (raw 10-byte frames with checksum, no library). Arduino 101:
  `Serial1` D0/D1; UNO/Nano: SoftwareSerial A0 RX / A1 TX; 1 kOhm in series on the module RX. Auto-detected (status probe
  at 1.5/4/6.5 s, or any module message). IR -> next file in `01/` (count via 0x4E query, or end found by a file-not-found
  error), restarting at `001` after `mp3_reset_s` (default 120 s) with no IR sound; D2 -> `02/001`; D10 -> `02/002`.
  New settings `mp3_volume`, `mp3_reset_s` (config version 3). Compiles: 101 29,628 B; UNO/Nano 15,498 B flash, 1,089 B RAM
  (53%). `Nano_MP3TriggerPlayer` (YX5300, 4 inputs + PIR) stays separate.
- WiFi builds: shared `.ino` now includes `board_config.h` per folder (101/UNO/Nano pin maps unchanged). Compiles: 101
  30,048 B; UNO/Nano 15,548 B flash, 1,169 B RAM (57%); ESP32 1,044,799 B (79% of OTA app partition); ESP8266 342,640 B
  IROM; ESP-01 341,832 B IROM (`eesz=1M64`).
- ESP-01 runs an IR **sensor** on GPIO2 (user confirmed), so its build has `WIRED_SHARES_IR_PIN`: with `wired_input` on,
  GPIO2 is a digital input and IR decoding is off. `rearm_ms` limit raised to 60000 (cooldowns like the old 15 s).
- Archived the old WiFi sketches: `ESP32_Node` (+ `components/esp32/PINOUT.md`), the whole `components/acebott_esp32_max`,
  `ESP8266_Node` (+ `components/esp8266/PINOUT.md`), `ESP12F_SimpleIRPositiveTrigger`, `ESP01_Node`
  (+ `components/esp01/PINOUT.md`). Removed `IRremote` from the library list (no active sketch uses it). Central controller
  command list is now `main` / `aux` (boards still accept `relay1:pulse` / `relay2:pulse`).
- Archived (none ever deployed): `HauntedHouseRF` (RF prototype), `Arduino101_PIRLaserCH9311Trigger` (no serial control,
  separate 6-35 V supply), and the whole `components/wemos_d1_r32` folder (`WemosD1R32_Node`, `WemosD1R32_SimpleIRPositiveTrigger`;
  CH340 upload failure, any-IR triggering). Removed `rc-switch` from the library list (only archived sketches used it).
- Archived `UNO_Node` and `Nano_Node` with their board-level `components/uno|nano/PINOUT.md`: IR fired on command 0x00
  with no protocol check (IRremote reports unknown IR, e.g. phones, as 0x00), and D7 positive input floats.
- Added `LASER_TAG_TRANSITION_GUIDE.md` (printable per-board rewiring cards and a deployed-board worksheet).
