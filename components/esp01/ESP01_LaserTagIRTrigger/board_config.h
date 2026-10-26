#pragma once
// ESP-01 / ESP-01S pin map for the shared *_LaserTagIRTrigger sketch. Keep in sync with PINOUT.md.
// Only GPIO0 and GPIO2 are usable: one input + one negative output. Both are boot pins and must idle HIGH at power-up.
// GPIO2 is either the IR receiver (decoded codes) or, with wired_input on, a digital IR sensor (break-beam/obstacle).
#define BOARD_NAME "ESP-01"
#define HAS_WIFI 1
#define IR_USE_INTERRUPT 1
#define RELEASE_NEGATIVE_WHEN_IDLE 1
#define HAS_POS 0
#define HAS_WIRED 1
#define WIRED_SHARES_IR_PIN 1
#define HAS_AUX 0
#define HAS_DURATION_PINS 0
#define HAS_RELAY_LED 0
#define RELAY_LED_ACTIVE_LOW 0
#define HAS_IR_LED 0
#define HAS_MP3 0
#define HAS_AP_BUTTON 0

#define PIN_IR 2
#define PIN_WIRED 2
#define PIN_NEG 0

#define LBL_IR "GPIO2"
#define LBL_WIRED "GPIO2"
#define LBL_NEG "GPIO0"
#define LBL_AP_TRIGGER "reset 3 times quickly"
#define WIRED_POSITIVE_TEXT "Positive: sensor drives HIGH to fire (no pull-down: GPIO2 must be HIGH at boot)"
