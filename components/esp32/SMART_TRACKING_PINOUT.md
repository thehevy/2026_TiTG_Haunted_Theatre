# ESP32 Smart Tracking Skull Pinout

**Created:** 2026-10-03  
**Last Updated:** 2026-10-03

Target sketch:

- `components/esp32/ESP32_SmartTracking_Node/ESP32_SmartTracking_Node.ino`

## Overview

This variant tracks a target with two VL53L1X time-of-flight sensors, rotates a skull servo toward the detected target, and triggers a scare event with blinking eye LEDs and a DFPlayer Mini sound effect.

## Recommended Bill of Materials

- Microcontroller: ESP32 Dev Module
- Sensors: 2x VL53L1X Time-of-Flight (ToF) long-range distance sensor modules
- Power regulator: 6V UBEC (minimum 3A output capability)
- Power source: 2S LiPo battery (7.4V) or standard 7.2V NiMH RC truck battery pack
- Actuator: 1/10-scale RC steering servo with 3-pin layout (signal, V+, ground)
- Hardware: breadboard, jumper wires, and 3D-printed or cardboard brackets
- Visuals: 2x 5V LEDs and 2x 220 ohm resistors
- Audio: DFPlayer Mini MP3 module, speaker, and MicroSD card formatted to FAT32

## ESP32 Board Setup

- Board: `ESP32 Dev Module`
- Serial monitor baud: `115200`
- Required libraries:
  - `VL53L1X` by Pololu
  - `ESP32Servo` by Kevin Harrington
  - `DFRobotDFPlayerMini`

## System Pinout Map

| Component | Component Pin | ESP32 / Connection | Notes |
| --------- | ------------- | ------------------ | ----- |
| Left ToF sensor | VIN | 5V rail | Powers the sensor logic board |
| Left ToF sensor | GND | GND | Shared system ground |
| Left ToF sensor | SDA | GPIO21 | Shared I2C data line |
| Left ToF sensor | SCL | GPIO22 | Shared I2C clock line |
| Left ToF sensor | XSHUT | GPIO4 | Dedicated shutdown control |
| Right ToF sensor | VIN | 5V rail | Powers the sensor logic board |
| Right ToF sensor | GND | GND | Shared system ground |
| Right ToF sensor | SDA | GPIO21 | Shared I2C data line |
| Right ToF sensor | SCL | GPIO22 | Shared I2C clock line |
| Right ToF sensor | XSHUT | GPIO5 | Dedicated shutdown control |
| Servo | Signal | GPIO13 | PWM servo control signal |
| Servo | Power | UBEC +6V output | Do not power from the ESP32 5V pin |
| Servo | Ground | UBEC ground and ESP32 GND | Must share a common ground |
| Left eye LED | Positive | GPIO25 | 220 ohm series resistor required |
| Left eye LED | Negative | GND | Shared system ground |
| Right eye LED | Positive | GPIO26 | 220 ohm series resistor required |
| Right eye LED | Negative | GND | Shared system ground |
| DFPlayer Mini | VCC | 5V | Powers the decoder and audio circuit |
| DFPlayer Mini | GND | GND | Shared system ground |
| DFPlayer Mini | RX | GPIO16 | Serial2 RX, add 1k resistor if noisy |
| DFPlayer Mini | TX | GPIO17 | Serial2 TX |
| UBEC | Input + | Battery + | 7.4V LiPo or 7.2V NiMH |
| UBEC | Input - | Battery - | Negative supply lead |
| UBEC | Output + | Servo V+ | Regulated 6V output |
| UBEC | Output - | ESP32 GND | Common return path |

## Pin Mapping

| Function | ESP32 Pin | Notes |
| -------- | --------- | ----- |
| Left ToF XSHUT | GPIO4 | Hard-reset line for the left sensor |
| Right ToF XSHUT | GPIO5 | Hard-reset line for the right sensor |
| Left eye LED | GPIO25 | Turned on while tracking, off during idle |
| Right eye LED | GPIO26 | Turned on while tracking, off during idle |
| Servo signal | GPIO13 | Skull rotation servo output |
| DFPlayer RX | GPIO16 | Serial2 RX for DFPlayer |
| DFPlayer TX | GPIO17 | Serial2 TX for DFPlayer |
| ToF I2C SDA | GPIO21 | Shared with both VL53L1X sensors |
| ToF I2C SCL | GPIO22 | Shared with both VL53L1X sensors |

## Power and Wiring Notes

- Keep all sensor grounds common with the ESP32 ground.
- Use a 5V rail for the DFPlayer and the ToF sensors, but do not feed the servo directly from the ESP32 5V output.
- Use the UBEC to provide a regulated 6V feed for the servo and tie the UBEC ground back to the ESP32 ground.
- The eye LEDs should each include a 220 ohm series resistor.
- The DFPlayer serial line should include a 1k resistor on the RX line if the board layout is noisy.
- The ToF sensors are started with XSHUT toggling, then re-addressed to `0x30` and `0x31` to allow dual-sensor operation.
- Sensor angle should be approximately 15 to 20 degrees outward from center to improve left-vs-right discrimination.

## Wiring Schematic

```text
+-------------------+
|  2S LiPo Battery  |
+---------+---------+
          |
          +--- (+) --------------------> UBEC Input +
          |
          +--- (-) --------------------> UBEC Input -

UBEC 6V Output + --------------------> Servo V+
UBEC 6V Output - --------------------> ESP32 GND

ESP32 GND ---------------------------> ToF GND
ESP32 GND ---------------------------> DFPlayer GND
ESP32 GND ---------------------------> Servo GND

ESP32 GPIO21 -----------------------> ToF SDA
ESP32 GPIO22 -----------------------> ToF SCL
ESP32 GPIO4 ------------------------> Left ToF XSHUT
ESP32 GPIO5 ------------------------> Right ToF XSHUT
ESP32 GPIO13 -----------------------> Servo Signal
ESP32 GPIO25 -----------------------> Left Eye LED (+)
ESP32 GPIO26 -----------------------> Right Eye LED (+)
ESP32 GPIO16 -----------------------> DFPlayer RX
ESP32 GPIO17 -----------------------> DFPlayer TX
```

## Behavior Summary

- If both sensors detect a target, the skull turns to the midpoint between them.
- If only the left sensor detects a target, the skull swings left.
- If only the right sensor detects a target, the skull swings right.
- If neither sensor detects a target for more than 3 seconds, the skull returns to center.
- When any tracking event occurs, the eye LEDs turn on and the DFPlayer plays folder `01`, track `001.mp3`.
- During the idle period, the eye LEDs are off and the sound is stopped.

## Sound Card Layout

Place the scare clip on a FAT32-formatted MicroSD card in this folder structure:

- `/01/001.mp3`

## Safety and Setup Notes

- Keep the servo limits constrained to the skull range, typically around `30` to `150` degrees.
- Verify that the DFPlayer and eye LEDs do not draw more current than the chosen power supply can provide.
- Confirm the servo ground and the ESP32 ground are tied together. If the servo twitches or jitters, the common ground is usually the root cause.
- Angle the two ToF sensors slightly outward for clean left-vs-right tracking. If they face straight ahead, their overlapping zone can be ambiguous.
