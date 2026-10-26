#pragma once
// ESP8266 NodeMCU v2/v3 and ESP-12F pin map for the shared *_LaserTagIRTrigger sketch. Keep in sync with PINOUT.md.
// Too few safe GPIOs for everything: no wired input, aux trigger, or DFPlayer on this board.
#define BOARD_NAME "ESP8266"
#define HAS_WIFI 1
#define IR_USE_INTERRUPT 1
#define RELEASE_NEGATIVE_WHEN_IDLE 1  // 3.3 V I/O: released lets a 5 V relay module's IN pull fully HIGH
#define HAS_POS 1
#define HAS_WIRED 0
#define HAS_AUX 0
#define HAS_DURATION_PINS 1
#define HAS_RELAY_LED 1
#define RELAY_LED_ACTIVE_LOW 1  // onboard LED on D4/GPIO2 lights when LOW
#define HAS_IR_LED 1
#define HAS_MP3 0
#define HAS_AP_BUTTON 1

#define PIN_IR 14         // D5
#define PIN_NEG 5         // D1
#define PIN_POS 4         // D2
#define PIN_DUR_A 12      // D6
#define PIN_DUR_B 13      // D7
#define PIN_RELAY_LED 2   // D4, onboard LED; boot pin, must not be pulled LOW at power-up
#define PIN_IR_LED 16     // D0
#define PIN_AP_BUTTON 0   // D3, FLASH button

#define LBL_IR "D5"
#define LBL_NEG "D1"
#define LBL_POS "D2"
#define LBL_DUR_A "D6"
#define LBL_DUR_B "D7"
#define LBL_LEDS "relay D4 (onboard), IR D0"
#define LBL_AP_TRIGGER "hold FLASH 3 s, or press RST 3 times quickly"
