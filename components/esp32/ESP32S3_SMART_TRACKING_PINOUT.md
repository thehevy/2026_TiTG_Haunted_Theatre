# ESP32-S3 Smart Tracking Skull Pinout

**Created:** 2026-10-13  
**Last Updated:** 2026-10-13

Target sketch:

- `components/esp32/ESP32S3_SmartTracking_Node/ESP32S3_SmartTracking_Node.ino`

## Overview

This variant tracks a target with two VL53L1X time-of-flight sensors, rotates a skull servo toward the detected target, and triggers a scare event with blinking eye LEDs and a DFPlayer Mini sound effect.

## Recommended Bill of Materials

- Microcontroller: ESP32-S3-WROOM-1 module
- Sensors: 2x VL53L1X ToF sensor modules
- Actuator: 1/10-scale RC servo
- Audio: DFPlayer Mini, speaker, and MicroSD card
- Visuals: 2x LEDs with 220 ohm resistors
- Power source: 2S LiPo or 7.2V NiMH with UBEC
- Hardware: breadboard, jumper wires, brackets, and common-ground wiring

## ESP32-S3 Board Setup

- Board: `ESP32S3 Dev Module` or equivalent `ESP32-S3-WROOM-1` board package
- USB/COM note: if the module exposes two USB-C connectors, use the one labeled `USB` for the upload/programming port; the `COM` port is not the bootloader port and is not used for the sketch firmware upload
- Serial monitor baud: `115200`
- Required libraries:
  - `VL53L1X` by Pololu
  - `ESP32Servo` by Kevin Harrington
  - `DFRobotDFPlayerMini`

## Safe Pin Selection

This sketch intentionally avoids the boot-strapping pins and uses the common S3 GPIOs below:

| Function                     | ESP32-S3 Pin | Notes                                              |
| --------------------------- | ------------ | -------------------------------------------------- |
| Left ToF XSHUT              | GPIO4        | Shared low-power reset line                        |
| Right ToF XSHUT             | GPIO5        | Shared low-power reset line                        |
| Left eye LED                | GPIO6        | 220 ohm series resistor required                   |
| Right eye LED               | GPIO7        | 220 ohm series resistor required                   |
| Servo signal                | GPIO8        | Servo PWM control                                   |
| Negative trigger debug input | GPIO18     | Active LOW; prints config to serial when asserted  |
| Remote init lockout pin     | GPIO35      | LOW = skip DFPlayer + ToF init at boot            |
| Remote retry init pin       | GPIO36      | LOW = retry DFPlayer + ToF init if not ready       |
| DFPlayer RX                 | GPIO16      | Serial2 RX                                         |
| DFPlayer TX                 | GPIO17      | Serial2 TX                                         |
| ToF SDA                     | GPIO21      | Shared I2C data line                               |
| ToF SCL                     | GPIO20      | Shared I2C clock line; GPIO22 is not used here     |

## System Pinout Map

| Component               | Component Pin | ESP32-S3 Connection | Notes                                      |
| ----------------------- | ------------- | ------------------- | ------------------------------------------ |
| Left ToF sensor         | VIN           | 5V rail             | Sensor logic supply                        |
| Left ToF sensor         | GND           | GND                 | Shared system ground                      |
| Left ToF sensor         | SDA           | GPIO21              | Shared I2C data bus                       |
| Left ToF sensor         | SCL           | GPIO20              | Shared I2C clock                          |
| Left ToF sensor         | XSHUT         | GPIO4               | Hard reset / address selection            |
| Right ToF sensor        | VIN           | 5V rail             | Sensor logic supply                        |
| Right ToF sensor        | GND           | GND                 | Shared system ground                      |
| Right ToF sensor        | SDA           | GPIO21              | Shared I2C data bus                       |
| Right ToF sensor        | SCL           | GPIO20              | Shared I2C clock                          |
| Right ToF sensor        | XSHUT         | GPIO5               | Hard reset / address selection            |
| Servo                   | Signal        | GPIO8               | PWM signal from S3                        |
| Negative trigger input   | Signal        | GPIO18              | Active LOW debug trigger; prints config to serial |
| Remote init lockout pin | Signal        | GPIO35              | LOW = skip DFPlayer + ToF init at boot            |
| Remote retry init pin   | Signal        | GPIO36              | LOW = retry init later if modules are absent      |
| Servo                   | V+            | UBEC 6V output      | Do not feed from ESP32 3.3V rail                  |
| Servo                   | GND           | UBEC ground + ESP32 GND | Common ground required                        |
| Left eye LED            | Positive      | GPIO6               | 220 ohm series resistor                           |
| Left eye LED            | Negative      | GND                 | Common return                             |
| Right eye LED           | Positive      | GPIO7               | 220 ohm series resistor                   |
| Right eye LED           | Negative      | GND                 | Common return                             |
| DFPlayer Mini           | VCC           | 5V                  | Audio module power                        |
| DFPlayer Mini           | GND           | GND                 | Shared ground                             |
| DFPlayer Mini           | RX            | GPIO16              | Serial2 RX                                |
| DFPlayer Mini           | TX            | GPIO17              | Serial2 TX                                |
| UBEC                    | Input +       | Battery +           | 7.4V LiPo or 7.2V NiMH                   |
| UBEC                    | Input -       | Battery -           | Negative supply                           |
| UBEC                    | Output +      | Servo V+            | Regulated 6V                              |
| UBEC                    | Output -      | ESP32-S3 GND        | Common return path                        |

## Power and Wiring Notes

- Keep all grounds common: ESP32-S3, DFPlayer, sensors, servo, and LEDs.
- Servo power should come from the UBEC, not the ESP32 board rail.
- Use the 5V rail for the ToF sensors and DFPlayer, but keep the servo on the regulated UBEC output.
- Add a 220 ohm resistor for each LED.
- If the DFPlayer RX line is noisy, add a 1k resistor on the RX lead.
- The ToF sensors are reset with XSHUT and then re-addressed to `0x30` and `0x31`.
- The sensor pair should be angled slightly outward to improve left/right discrimination.

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
UBEC 6V Output - --------------------> ESP32-S3 GND

ESP32-S3 GND ------------------------> ToF GND
ESP32-S3 GND ------------------------> DFPlayer GND
ESP32-S3 GND ------------------------> Servo GND

ESP32-S3 GPIO21 ----------------------> ToF SDA
ESP32-S3 GPIO20 ----------------------> ToF SCL
ESP32-S3 GPIO4 -----------------------> Left ToF XSHUT
ESP32-S3 GPIO5 -----------------------> Right ToF XSHUT
ESP32-S3 GPIO8 -----------------------> Servo signal
ESP32-S3 GPIO18 ----------------------> Negative trigger debug input
ESP32-S3 GPIO35 ----------------------> Remote init lockout pin (LOW = skip init)
ESP32-S3 GPIO36 ----------------------> Remote retry init pin (LOW = retry later)
ESP32-S3 GPIO6 -----------------------> Left eye LED (+)
ESP32-S3 GPIO7 -----------------------> Right eye LED (+)
ESP32-S3 GPIO16 ----------------------> DFPlayer RX
ESP32-S3 GPIO17 ----------------------> DFPlayer TX
```

## Behavior Summary

- If both sensors detect a target, the skull turns to the midpoint between them.
- If only the left sensor detects a target, the skull swings left.
- If only the right sensor detects a target, the skull swings right.
- If neither sensor detects a target for more than 3 seconds, the skull returns to center.
- During a target event, the eye LEDs turn on and the DFPlayer plays folder `01`, track `001.mp3`.
- During idle periods, the eye LEDs stay off and the DFPlayer is stopped.
- An active-low negative trigger on GPIO18 prints the current configuration and operating values to the serial monitor.
- If GPIO35 is held LOW, the DFPlayer and VL53L1X modules are skipped during startup so unplugged remote hardware does not block boot.
- If GPIO36 is held LOW and a module is not initialized, the sketch will retry that module later without halting the rest of the system.
- The serial monitor also emits a periodic status line with distance, angle, and scare state for runtime health checks.

## Sound Card Layout

Place the scare clip on a FAT32-formatted MicroSD card in this layout:

- `/01/001.mp3`

## Safety and Setup Notes

- Keep the servo travel bounded to the skull range, roughly `30` to `150` degrees.
- Do not power the servo from the ESP32-S3 3.3V rail.
- If the servo jitters or twitches, verify the common ground and UBEC output wiring.
- Keep the sensor pair aimed slightly outward so the target is easier to resolve left vs. right.
