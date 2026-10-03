# ESP-01 Pinout Guide

**Created:** 2026-09-25
**Last Updated:** 2026-09-25

Target sketch:

- `components/esp01/ESP01_Node.ino`

## Function Map

| Function        | ESP-01 Pin | Notes                                                                                       |
| --------------- | ---------- | ------------------------------------------------------------------------------------------- |
| IR sensor input | GPIO2      | Active HIGH input; sensor output should be pulled to VCC when active.                       |
| Relay output    | GPIO0      | Active HIGH relay driver. GPIO0 goes HIGH for 1 second on trigger in the current sketch. |
| Sensor power    | 3V3        | Connect IR sensor VCC here.                                                                 |
| Common ground   | GND        | Shared ground between ESP-01, sensor, and relay board.                                      |

## Serial Output

- Open the Serial Monitor at `115200` baud.
- The sketch prints trigger, cooldown, and status messages on the serial console.

## Trigger Behavior

- An IR sensor transition to HIGH triggers the relay pulse.
- Relay output pulse length: 1 second.
- Cooldown period: 15 seconds before the input can trigger again.
- This logic prevents the relay from retriggering while the sensor remains active.

## Power and Ground

| Device             | ESP-01 Connection           |
| ------------------ | --------------------------- |
| IR sensor VCC      | 3V3                         |
| IR sensor GND      | GND                         |
| Relay module VCC   | External 5V recommended     |
| Relay module GND   | Common GND with ESP-01      |

## Board Power Input (Onboard Connector)

| Input Path               | Connector Available                        | Minimum Input | Maximum Input | Notes                                                                |
| ------------------------ | ------------------------------------------ | ------------- | ------------- | -------------------------------------------------------------------- |
| 3.3V regulator input     | Usually via 3.3V pin or USB serial adapter | 3.0V          | 3.6V          | ESP-01 module itself runs at 3.3V.                                   |
| External 5V relay supply | Relay board VCC input                      | 4.75V         | 5.25V         | Keep relay coil power separate from the ESP-01 logic rail when possible. |

## Wiring Notes

1. Connect the IR sensor signal line to GPIO2.
2. Connect the sensor ground to GND.
3. Connect the sensor VCC to 3.3V.
4. Connect the relay control input to GPIO0.
5. Use a common ground between the ESP-01, relay module, and sensor.

## Relay Logic

- The sketch is configured for active HIGH relay drive on GPIO0.
- If your relay board is active LOW instead, set `RELAY_ACTIVE_LOW` to `true` in the sketch.

## Important ESP-01 Considerations

- GPIO0 is a boot strapping pin. Keep the relay control path from forcing GPIO0 low at reset unless you intentionally want that behavior.
- GPIO1 and GPIO3 are used for serial upload and should not be repurposed for the main trigger input during development.
- When using a bare ESP-01 module, a 3.3V serial adapter is required for programming.

## Example Trigger Timing

- Sensor becomes active (HIGH) at t = 0s.
- Relay output goes active for 1s.
- Relay stays inactive until t = 15s.
- The trigger can rearm at t = 15s if the input is still active or has returned to a new edge.
