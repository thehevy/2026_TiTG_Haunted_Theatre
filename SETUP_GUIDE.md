# Arduino 101 Setup Guide (Windows, COM5)

**Created:** 2026-07-30
**Last Updated:** 2026-08-07

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

## Optional: IDE 2.x Usage

After successful setup and firmware update in IDE 1.8.19, you can try IDE 2.x for editing/uploading.
If uploads fail in 2.x, return to IDE 1.8.19 for this board.

For this project overall, IDE 2.x is the default for ESP32, ESP8266, UNO, and Nano class nodes.
