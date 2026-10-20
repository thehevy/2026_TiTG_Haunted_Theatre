# Arduino 101 Setup Guide (Windows, COM5)

**Created:** 2026-07-30
**Last Updated:** 2026-10-20

## Table of Contents

- [Goal](#goal)
- [Current Known Device](#current-known-device)
- [Recommended IDE](#recommended-ide)
- [IDE Support Matrix for This Project](#ide-support-matrix-for-this-project)
- [Step 1: Install Arduino IDE 1.8.19](#step-1-install-arduino-ide-1819)
- [Step 2: Configure Boards Manager Download Access](#step-2-configure-boards-manager-download-access)
- [Step 3: Install Intel Curie Boards Core](#step-3-install-intel-curie-boards-core)
- [Step 4: Install/Fix Arduino 101 Drivers (Windows)](#step-4-installfix-arduino-101-drivers-windows)
- [Step 5: Select Board and Port](#step-5-select-board-and-port)
- [Step 6: Update Firmware](#step-6-update-firmware)
- [Step 7: Upload First Example (Blink)](#step-7-upload-first-example-blink)
- [Step 8: Try Arduino 101 Feature Examples](#step-8-try-arduino-101-feature-examples)
- [Required Libraries for Project Sketches](#required-libraries-for-project-sketches)
- [Troubleshooting](#troubleshooting)
- [Using VS Code For Sketch Editing](#using-vs-code-for-sketch-editing)
- [Command-Line Compile and Upload (arduino-cli)](#command-line-compile-and-upload-arduino-cli)
- [Optional: IDE 2.x Usage](#optional-ide-2x-usage)

## Goal

Set up an Arduino 101 board, update firmware, and upload example sketches.

## Current Known Device

- Port: COM5
- USB Device: USB Serial Device (VID_8087, PID_0AB6)
- This matches Intel Curie based hardware used by Arduino 101.

## Recommended IDE

Use Arduino IDE 1.8.19 for initial setup and firmware update.

Why:

- Arduino 101 is a retired board.
- Intel Curie tooling is deprecated and most reliable in IDE 1.8.x.
- Firmware updater flow is more consistent in IDE 1.8.x.

## IDE Support Matrix for This Project

Use Arduino IDE 1.8.19 for Arduino 101. Use Arduino IDE 2.x for the other boards.

| Hardware family | Preferred IDE | Policy |
| --- | --- | --- |
| Arduino 101 (Intel Curie) | Arduino IDE 1.8.19 | Required for setup, firmware updates, and normal uploads. |
| ESP32 | Arduino IDE 2.x (latest stable) | Recommended default workflow. |
| ESP8266 | Arduino IDE 2.x (latest stable) | Recommended default workflow. |
| UNO and Nano class boards | Arduino IDE 2.x (latest stable) | Recommended default workflow. |

Practical recommendation:

- Keep both IDE versions installed side-by-side.
- Reserve IDE 1.8.19 for Arduino 101 only.
- Use IDE 2.x for all non-101 development in this repo.

## Step 1: Install Arduino IDE 1.8.19

1. Download Arduino IDE 1.8.19 for Windows from the official Arduino software page.
2. Install with default options.
3. Launch the IDE.

## Step 2: Configure Boards Manager Download Access

If you are on a corporate network, configure proxy first.

1. Open File > Preferences.
2. In Additional Boards Manager URLs, add:
   [Arduino package index](https://downloads.arduino.cc/packages/package_index.json)

3. If downloads fail, enable Use proxy server and set:
   - Host: proxy-dmz.intel.com
   - Port: 912
4. Restart the IDE.

Notes:

- The package index URL itself is valid.
- If proxy auth is required, set proxy user and password in preferences.

## Step 3: Install Intel Curie Boards Core

1. Open Tools > Board > Boards Manager.
2. Search for: curie
3. Install: [DEPRECATED] Intel Curie Boards
4. Preferred version: 2.0.4 (or 2.0.6 if 2.0.4 is not available).

## Step 4: Install/Fix Arduino 101 Drivers (Windows)

If the board is not recognized correctly, install drivers manually.

Driver folder:
C:/Users/[your-user]/AppData/Local/Arduino15/packages/Intel/hardware/arc32/2.0.4/drivers

Run:

- dpinst-amd64.exe on 64-bit Windows
- dpinst-x86.exe on 32-bit Windows

Then unplug/replug the board.

### Windows 11: Intel drivers rejected, use Zadig instead

On Windows 11 (confirmed on Windows 11 Enterprise build 26200), `dpinst` fails with exit code `0x80030000`. The Intel driver catalogs are unsigned, and Windows enforces driver signing.
The cause appears in `C:\Windows\INF\setupapi.dev.log` as `Error 0x800B0100: No signature was present`.

Symptoms:

- Normal mode works. The board shows as **USB Serial Device (COMx)** with VID `8087` PID `0AB6`, using the built-in Windows driver. This is expected; no fix needed for it.
- Uploads hang at `Waiting for device...` and end with `ERROR: Timed out waiting for Arduino 101`. Pressing MASTER RESET does not help.
  The board's flashing (DFU) mode device, **ARDUINO 101** (VID `8087` PID `0ABA`), has no driver.

Fix (one time per PC, needs admin):

1. Download Zadig from the official [libwdi releases](https://github.com/pbatard/libwdi/releases/latest) (for example `zadig-2.9.exe`).
2. Run Zadig and accept the UAC prompt. The board can be unplugged.
3. Choose **Device > Create New Device**, name it `Arduino 101 DFU`, and set the USB ID to `8087` / `0ABA` (leave MI blank).
4. Set the target driver to **WinUSB** and click **Install Driver**.
5. Plug the board in. It still shows as **USB Serial Device** in normal mode. **ARDUINO 101** appears under *Universal Serial Bus devices* only for a few seconds during uploads.

Zadig adds a self-signed certificate for the driver it generates. Check corporate policy before using it on managed machines.

If an upload reports `Serial port busy` or `Access to the port is denied`, another program has the port open. Close any serial monitors, then unplug/replug the board.

## Step 5: Select Board and Port

1. Tools > Board > Arduino/Genuino 101
2. Tools > Port > COM5

## Step 6: Update Firmware

1. Open Tools > Firmware Updater (or Curie Firmware Updater).
2. Select COM5.
3. Click Update and wait for success.
4. Unplug/replug the board when done.

If updater fails:

1. Press MASTER RESET right before clicking Update.
2. Retry update.
3. On slow driver setup, press MASTER RESET every second while install completes.

## Step 7: Upload First Example (Blink)

1. File > Examples > 01.Basics > Blink
2. Click Upload
3. Confirm Done uploading
4. Confirm LED blinks

## Step 8: Try Arduino 101 Feature Examples

Suggested sketches:

- File > Examples > CurieIMU > RawIMUDataSerial
- File > Examples > CurieBLE > CallbackLED

For serial output reliability, use this in setup():

```cpp
Serial.begin(115200);
while (!Serial) {
}
```

## Required Libraries for Project Sketches

Install these in Arduino IDE using Sketch > Include Library > Manage Libraries.

| Sketch | Library | Why |
| --- | --- | --- |
| ESP32 node (`components/esp32/ESP32_Node/ESP32_Node.ino`) | `PubSubClient` by Nick O'Leary | Provides `PubSubClient.h` for MQTT. |
| ESP32 node (`components/esp32/ESP32_Node/ESP32_Node.ino`) | `IRremote` by Arduino-IRremote | Provides `IRremote.hpp` for KY-022/TL1838/VS1838B IR decode. |
| ESP8266 node (`components/esp8266/ESP8266_Node.ino`) | `PubSubClient` by Nick O'Leary | Provides `PubSubClient.h` for MQTT. |
| ESP8266 node (`components/esp8266/ESP8266_Node.ino`) | `IRremote` by Arduino-IRremote | Provides `IRremote.hpp` for KY-022/TL1838/VS1838B IR decode. |
| UNO node (`components/uno/UNO_Node/UNO_Node.ino`) | `IRremote` by Arduino-IRremote | Provides `IRremote.hpp` for KY-022/TL1838/VS1838B IR decode. |
| Arduino 101 RF node (`components/arduino101/Arduino101_Node/Arduino101_Node.ino`) | `rc-switch` by sui77 | Provides RF receive/decode support. |

Notes:

- `WiFi.h` on ESP32 is from the ESP32 board package.
- `ESP8266WiFi.h` on ESP8266 is from the ESP8266 board package.
- If compile output says `PubSubClient.h: No such file or directory`, install `PubSubClient` first.
- If compile output says `IRremote.hpp: No such file or directory`, install `IRremote` first.

## Troubleshooting

### Board not listed in Ports

- Try another USB data cable.
- Try another USB port.
- Reinstall Curie driver package.

### Upload timeout or device not found

- Verify board is Arduino/Genuino 101.
- Verify port is COM5.
- Close Serial Monitor before upload.
- Press MASTER RESET when upload starts.

### Boards Manager download errors

- Verify package URL and proxy settings.
- Retry from non-corporate network if proxy blocks package install.

## Using VS Code For Sketch Editing

Use VS Code to write and manage the sketch files, then use Arduino IDE 1.8.19 for firmware update and upload.

Recommended workflow:

1. Create or open the sketch folder in VS Code.
2. Edit and save the `.ino` file in VS Code.
3. Open the same sketch in Arduino IDE 1.8.19 when you are ready to build.
4. Confirm Tools > Board is set to Arduino/Genuino 101.
5. Confirm Tools > Port is set to COM5.
6. Update firmware first if needed, then upload from Arduino IDE 1.8.19.

This keeps the editor flexible while preserving the most reliable upload path for Arduino 101.

To compile and upload without leaving VS Code, use the arduino-cli workflow below. Keep IDE 1.8.19 for the Curie Firmware Updater.

## Command-Line Compile and Upload (arduino-cli)

arduino-cli shares the board cores (`%LOCALAPPDATA%\Arduino15`) and sketchbook libraries (`Documents\Arduino\libraries`) with Arduino IDE 1.8.19, so anything installed by one is visible to the other.

Sketch names, board FQBNs, required cores, and required libraries are defined in `tools/arduino/sketches.json`. The helper script is `tools/arduino/arduino.ps1`.

### One-time install (Windows)

1. Download the latest `arduino-cli_<version>_Windows_64bit.zip` from the [arduino-cli releases page](https://github.com/arduino/arduino-cli/releases/latest).
2. Extract `arduino-cli.exe` to `%LOCALAPPDATA%\Programs\arduino-cli` and add that folder to your user `Path`.
3. Open a new PowerShell window in the repo root and run:

   ```powershell
   arduino-cli config init
   .\tools\arduino\arduino.ps1 setup
   ```

`setup` adds the ESP32/ESP8266 Boards Manager URLs, sets the arduino-cli proxy from `$env:HTTPS_PROXY` (for example `http://proxy-dmz.intel.com:912`), installs `Intel:arc32@2.0.4`, `arduino:avr`, `esp32:esp32`, `esp8266:esp8266`, and installs every library in `sketches.json`. To set a different proxy, pass `-Proxy http://host:port`. To clear it, pass `-Proxy ''`.

### Daily use

```powershell
.\tools\arduino\arduino.ps1 list                               # sketch names and FQBNs
.\tools\arduino\arduino.ps1 ports                              # connected boards and COM ports
.\tools\arduino\arduino.ps1 compile Arduino101_Node            # compile one sketch
.\tools\arduino\arduino.ps1 compile all                        # compile every sketch
.\tools\arduino\arduino.ps1 upload Arduino101_Node -Port COM5  # compile + upload
.\tools\arduino\arduino.ps1 monitor -Port COM5                 # serial monitor at 115200
```

If `-Port` is omitted, `upload` auto-detects the port only when exactly one connected board matches the sketch's board type. ESP boards that use CH340/CP210x USB bridges are not auto-identified, so pass `-Port` for them.

In VS Code, the same actions are available from **Terminal > Run Task** (`Arduino: Compile sketch`, `Arduino: Upload sketch`, `Arduino: Compile all sketches`, `Arduino: Serial monitor`, `Arduino: List ports`). `Ctrl+Shift+B` runs `Arduino: Compile sketch`.

### Board-specific upload notes

| Board | FQBN | Upload note |
| --- | --- | --- |
| Arduino 101 | `Intel:arc32:arduino_101` | Confirm the port shows VID `8087` PID `0AB6` in `ports`. Close the serial monitor first. If upload stalls, press MASTER RESET right as the upload starts. Use IDE 1.8.19 for firmware updates. |
| UNO | `arduino:avr:uno` | Standard upload. |
| Nano | `arduino:avr:nano:cpu=atmega328` | For old-bootloader clones, add `-Fqbn arduino:avr:nano:cpu=atmega328old`. |
| ESP32, WeMos D1 R32, Acebott ESP32-Max | `esp32:esp32:esp32` | If upload stops at `Connecting...`, hold BOOT until writing starts. |
| ESP32-S3 | `esp32:esp32:esp32s3` | Same BOOT-button workaround. |
| ESP8266 NodeMCU / ESP-12F | `esp8266:esp8266:nodemcuv2` | Standard upload. |
| ESP-01 | `esp8266:esp8266:generic` | Hold GPIO0 LOW at reset to enter flash mode. Use a 3.3V serial adapter. |

COM5 can also be taken by other devices, such as a Bluetooth serial link. Always run `ports` and confirm the board before you upload.

### VS Code IntelliSense for sketches

The C/C++ extension (`ms-vscode.cpptools`) needs each sketch's board-specific compiler, include paths, and defines.
The repo's `.vscode/c_cpp_properties.json` points it at `.build/compile_commands.json`, and `.vscode/settings.json` makes VS Code treat `*.ino` files as C++.
`.build/` is machine-specific and gitignored, so generate it on each machine:

```powershell
.\tools\arduino\arduino.ps1 intellisense                  # all sketches (several minutes the first time)
.\tools\arduino\arduino.ps1 intellisense ESP8266_Node     # refresh one sketch
```

Or run **Terminal > Run Task > Arduino: Refresh IntelliSense (all sketches)**.

Re-run it after you add a sketch, add an `#include` for a new library, or upgrade a board core. If squiggles don't update, run **C/C++: Reset IntelliSense Database** from the Command Palette.
If VS Code still shows the **Configure IntelliSense** prompt, select the `Arduino (arduino-cli)` configuration.

Limitation: the Arduino build auto-generates function prototypes, but IntelliSense reads the raw `.ino`. If a function is called before its definition in the file, IntelliSense may flag it even though the sketch compiles.

## Optional: IDE 2.x Usage

After successful setup and firmware update in IDE 1.8.19, you can try IDE 2.x for editing/uploading.
If uploads fail in 2.x, return to IDE 1.8.19 for this board.

For this project overall, IDE 2.x is the default for ESP32, ESP8266, UNO, and Nano class nodes.
