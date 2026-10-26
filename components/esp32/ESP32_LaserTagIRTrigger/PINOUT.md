# ESP32 Laser Tag IR Trigger (WiFi) Pinout

**Created:** 2026-10-26
**Last Updated:** 2026-10-26

Target sketch:

- `components/esp32/ESP32_LaserTagIRTrigger/ESP32_LaserTagIRTrigger.ino` (pins in `board_config.h`)

Boards: **ESP32 DevKit** (ESP32-WROOM-32, 30- or 38-pin) and **Acebott ESP32-Max V1.0** (UNO-size, header labeled with
GPIO numbers). Both use the same build and the same GPIO numbers.

Same trigger behavior as the Arduino 101 / UNO / Nano build (any valid IR code, jumper or saved duration, no retrigger
during a pulse, wired input, aux trigger, DFPlayer). See
`components/arduino101/Arduino101_LaserTagIRTrigger/PINOUT.md` for trigger details, IR formats, and the DFPlayer SD
layout. This page covers the ESP32 pins and the WiFi features.

## Board Setup

- FQBN: `esp32:esp32:esp32` (Arduino IDE: **ESP32 Dev Module**, default partition scheme, which supports OTA).
- Libraries: `PubSubClient` (installed by `.\tools\arduino\arduino.ps1 setup`).
- USB upload: `.\tools\arduino\arduino.ps1 upload ESP32_LaserTagIRTrigger -Port COMx`. If it stops at `Connecting...`,
  hold **BOOT** until writing starts. Acebott boards use a CH340 USB chip (install the CH340 driver if no COM port appears).
- Serial Monitor: `115200` baud. The same serial commands and `tools/web-config/index.html` work over USB.
- All I/O is **3.3 V**. Power the IR receiver from **3.3V**.

## Pin Map

| Function                     | GPIO | Notes                                                                 |
| ---------------------------- | ---- | --------------------------------------------------------------------- |
| Wired trigger input          | 27   | `wired_input`: off (default) / negative / positive (10 kOhm pull-down) |
| IR receiver OUT              | 19   | 3.3 V receiver                                                        |
| Main negative trigger        | 4    | **Active LOW**; released (high-Z) when idle                           |
| Main positive trigger        | 18   | **Active HIGH** (3.3 V)                                               |
| Relay status LED             | 21   | 330 Ohm -> LED -> GND                                                 |
| IR status LED                | 22   | 330 Ohm -> LED -> GND                                                 |
| Duration jumper A            | 32   | To GND = 5 s                                                          |
| Duration jumper B            | 33   | To GND = 10 s (both = 15 s)                                           |
| Aux trigger input            | 26   | Button / contact to GND                                               |
| Aux trigger output           | 25   | **Active LOW**, 500 ms default                                        |
| DFPlayer TX -> board RX      | 16   | `Serial2`                                                             |
| Board TX -> DFPlayer RX      | 17   | Through a **1 kOhm** resistor                                         |
| Setup AP button              | 0    | **BOOT** button: hold 3 s while running                               |

Avoid adding anything to GPIO 0, 2, 5, 12, 15 (boot pins) and 34-39 (input only, no pull-ups). The relay outputs are
released at boot until the sketch starts, so active-LOW relay modules stay off.

**Check a 5 V relay module once (ESP pins are not 5 V tolerant):** idle outputs are released (input), so the module's
own circuit sets the IN voltage. With the relay off, measure IN (GPIO4 / GPIO25) to GND:

- **3.6 V or less:** fine (most opto-isolated modules sit near 3.3 V through the board's input clamp).
- **Higher:** use a module with a 3.3 V trigger option (or a JD-VCC jumper), or a small NPN/MOSFET driver as on the
  ESP-01. Setting `RELEASE_NEGATIVE_WHEN_IDLE 0` in `board_config.h` drives idle HIGH at 3.3 V instead, but some 5 V
  modules then stay faintly on.

## WiFi Modes

| Situation                                            | What the board does                                                       |
| ---------------------------------------------------- | ------------------------------------------------------------------------- |
| Router SSID set and reachable                        | Joins the router. Settings page at `http://<device_id>.local` or its IP |
| Router SSID empty (the default)                      | Runs only the setup access point                                          |
| Router not reachable within 20 s (or lost)           | Starts the setup access point; retries the router every 2 min while nobody is connected to the AP; turns the AP off once the router connects |
| BOOT held 3 s, **or** reset 3 times within 10 s each, **or** serial command `AP` | Forces the setup access point on until the next reboot |

On the ESP8266 and ESP-01 only presses of **RST** count toward the 3 resets; power cycles do not (the count is kept in
RTC memory so it never writes flash). On the ESP32 any reset or power cycle counts.

**Setup access point:** SSID `Haunt-<device_id>` (for example `Haunt-haunt-1a2b3c`), WPA2 password `ap_pass`. Join it
from a phone or laptop; the settings page opens automatically (captive portal) or browse to `http://192.168.4.1`.

**Passwords:** there are no built-in passwords. On first boot (and after `DEFAULTS`) the board makes a random 10-character
setup AP password and admin password and prints them on USB serial at every boot until you change them:

```text
Setup AP password (generated): k7m2pq9xra
Settings page / OTA password for user admin (generated): 4tzh8wdn3e
```

Connect the USB serial monitor (115200) to read them. They are never shown on the WiFi settings page.

The serial log shows the current address at boot and in every heartbeat. Type `WIFI` for full status.

## Settings Page

Log in as user **`admin`** with `admin_pass` (the generated one until you change it). The page has:

- **Status:** WiFi mode, IP, signal, MQTT state, pulse length, trigger counts, DFPlayer. **Test main / Test aux** buttons.
- **Trigger:** the same settings as the USB config page (name, durations, re-arm, IR types, wired input, DFPlayer).
- **WiFi:** device ID, router SSID and password, setup AP password.
- **MQTT:** enable, broker host/port, username/password.
- **Admin password**, **Firmware upload**, **Reboot**.

**Save** stores everything in EEPROM. WiFi, MQTT, and password changes take effect after **Reboot** (the page says so).
If any field is invalid, nothing is saved and the page lists the errors.

### Security

- **Set your own AP and admin passwords on first setup.** Until then the page shows a warning and serial prints the
  generated ones at boot (`GET` shows `generated_passwords=1`).
- Saved passwords are never shown or sent to the browser. Leave a password field blank to keep it.
- Every form includes a per-boot token, so another web page cannot submit changes through your logged-in browser.
- The page is plain HTTP. On the router network, anyone who can capture traffic could see the admin login; keep props
  on the haunt's private network, and use the setup AP (WPA2) for changes in doubtful places.
- Pulses end on a hardware timer, so a slow page load, MQTT reconnect, or firmware upload cannot hold a relay on.

## IR Input Faults

If the IR input stays LOW for more than 15 ms (shorted or unpowered receiver) or never goes quiet for 400 ms (noise,
failing receiver), the capture is dropped and serial prints (at most every 10 s):

```text
IR input GPIO19 stuck LOW; capture dropped (1 so far). Check the IR receiver wiring and power.
```

The heartbeat shows `faults N` and the MQTT heartbeat has `"ir_faults":N`. Wired, aux, MQTT, and test triggers keep
working.

## MQTT (Central Controller)

Defaults: enabled, broker `192.168.1.10:1883`, no username. The repo's Mosquitto config requires a login, so set the
broker **username and password** on the settings page.

| Topic                          | Direction    | Payload                                                                 |
| ------------------------------ | ------------ | ----------------------------------------------------------------------- |
| `haunt/<device_id>/status`     | board -> broker | `online` (retained); `offline` as last will                          |
| `haunt/<device_id>/trigger`    | broker -> board | `main` (or `main:pulse`, `relay1:pulse`), `aux` (or `aux:pulse`, `relay2:pulse`) |
| `haunt/<device_id>/event`      | board -> broker | `{"src":"ir","fired":true,"ms":5000,"detail":"laser tag 0x147 9 bits"}` for every IR, wired, aux, MQTT, or test trigger |
| `haunt/<device_id>/heartbeat`  | board -> broker | Every 30 s: name, board, firmware, IP, RSSI, uptime, pulse length, IR counts and faults, DFPlayer |

An MQTT command fires the output the same way an IR shot does (lockout and re-arm apply) and plays the sound.

If the broker is down, the board retries with a growing delay (5 s up to 60 s) and never while an output is pulsing.
Each attempt can stall the board for about 2 s, so **turn MQTT off on props that do not use it**.

## Firmware Updates Over WiFi

Either method needs the admin password. Outputs are released during the update and the board reboots when done.

1. **From the settings page:** build a `.bin`, then use **Firmware > Upload firmware**:

   ```powershell
   arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries components\esp32\ESP32_LaserTagIRTrigger
   # upload components\esp32\ESP32_LaserTagIRTrigger\build\esp32.esp32.esp32\ESP32_LaserTagIRTrigger.ino.bin
   ```

2. **ArduinoOTA from VS Code / PowerShell** (board and PC on the same network, or the PC joined to the setup AP):

   ```powershell
   .\tools\arduino\arduino.ps1 upload ESP32_LaserTagIRTrigger -Port 192.168.1.50
   ```

   Use the board's IP (or `<device_id>.local`). arduino-cli asks for the admin password. Arduino IDE also lists the
   board under **Tools > Port > Network ports**.

## Wiring

```text
ESP32                                     IR receiver (3.3 V)
3V3 ------------------------------------> VCC
GPIO19 <--------------------------------- OUT
GND ------------------------------------> GND

ESP32                                     Main relay module (active LOW, 5 V)
GPIO4 ----------------------------------> IN
5V / VIN -------------------------------> VCC
GND ------------------------------------> GND

ESP32                                     Optional
GPIO18 ---------------------------------> active-HIGH load (3.3 V logic)
GPIO32 / GPIO33 --[jumper]--> GND         5 s / 10 s (both = 15 s)
GPIO27 --[switch]--> GND                  wired input, wired_input = negative
GPIO26 --[button]--> GND ; GPIO25 ------> aux relay IN (active LOW)
GPIO21 / GPIO22 -- 330 Ohm --> LED --> GND   relay / IR status LEDs
GPIO16 <-- DFPlayer TX ; GPIO17 -- 1 kOhm --> DFPlayer RX ; 5V/GND -> DFPlayer VCC/GND
```

On the Acebott ESP32-Max, the 5V and GND pins next to the GPIO header and the blue/red/black 3-pin rows can power
sensors and relay modules. Use an external 5 V supply for more than one relay module or a DFPlayer with a speaker.

## Validation

1. Upload over USB. Serial shows `ESP32 laser tag IR trigger ready`, the generated AP and admin passwords, the WiFi
   status, and a heartbeat every 5 s. Write the passwords down.
2. Confirm the setup AP appears (no router is set yet). Join it with the generated AP password and log in at
   `http://192.168.4.1` as `admin` with the generated admin password.
3. Set your own admin and AP passwords, the router SSID/password and MQTT login, **Save**, then **Reboot**. Confirm
   serial no longer prints generated passwords.
4. Confirm the board joins the router (`WIFI`), the page loads at `http://<device_id>.local`, and MQTT shows connected.
5. Fire the gun: confirm the trigger, the event on `haunt/<device_id>/event`, and the DFPlayer sound if fitted.
6. Publish `main` to `haunt/<device_id>/trigger` and confirm the outputs fire.
7. Hold BOOT for 3 s: confirm the setup AP starts. Reset 3 times quickly: confirm the AP starts after boot.
8. Upload new firmware once from the page and once with `arduino.ps1 upload ... -Port <ip>`.
9. Unplug the IR receiver OUT and short it to GND: confirm the `stuck LOW` message, then reconnect and confirm the gun
   still fires.
10. Fire a 15 s pulse and reload the settings page repeatedly: confirm the relay still drops on time.

Compile verified 2026-10-26: 1,047,771 B flash (79% of the OTA app partition), 54,376 B RAM (16%). Not yet bench-tested.
