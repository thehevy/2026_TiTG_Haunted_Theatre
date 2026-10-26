#pragma once
// ESP-01 / ESP-01S pin map for the shared *_LaserTagIRTrigger sketch. Keep in sync with PINOUT.md.
// Only GPIO0 and GPIO2 are usable: one input + one output. Both are boot pins and must be HIGH at power-up.
// GPIO2 is either a digital IR sensor (default, wired_input) or an IR receiver (wired_input off, decoded codes).
// Defaults match the old ESP01_Node: active-HIGH sensor, 1 s active-HIGH relay pulse on GPIO0, 15 s cooldown.
#define BOARD_NAME "ESP-01"
#define HAS_WIFI 1
#define IR_USE_INTERRUPT 1
#define RELEASE_NEGATIVE_WHEN_IDLE 1
#define MAIN_OUTPUT_ACTIVE_HIGH 1  // GPIO0 -> NPN/relay driver input; 10k pull-up keeps GPIO0 HIGH for boot
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

#define DEFAULT_WIRED_INPUT 2  // positive: the IR sensor drives GPIO2 HIGH when triggered
#define DEFAULT_MAIN_MS 1000
#define DEFAULT_REARM_MS 14000  // 1 s pulse + 14 s re-arm = the old 15 s cooldown

#define PIN_IR 2
#define PIN_WIRED 2
#define PIN_NEG 0

#define LBL_IR "GPIO2"
#define LBL_WIRED "GPIO2"
#define LBL_NEG "GPIO0"
#define LBL_AP_TRIGGER "press RST 3 times quickly; power cycles do not count"
#define WIRED_POSITIVE_TEXT "Positive: sensor drives HIGH to fire (no pull-down: GPIO2 must be HIGH at boot)"
