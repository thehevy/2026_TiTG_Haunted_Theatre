#pragma once
// Arduino 101 pin map for the shared *_LaserTagIRTrigger sketch. Keep in sync with PINOUT.md.
#define BOARD_NAME "Arduino 101"
#define HAS_WIFI 0
#define IR_USE_INTERRUPT 0             // D3 has no both-edge interrupt on the Curie, so the receiver is polled
#define RELEASE_NEGATIVE_WHEN_IDLE 1   // 3.3 V I/O: released lets a 5 V relay module's IN pull fully HIGH
#define HAS_POS 1
#define HAS_WIRED 1
#define HAS_AUX 1
#define HAS_DURATION_PINS 1
#define HAS_RELAY_LED 1
#define RELAY_LED_ACTIVE_LOW 0
#define HAS_IR_LED 1
#define HAS_MP3 1

#define PIN_WIRED 2
#define PIN_IR 3
#define PIN_NEG 4
#define PIN_POS 5
#define PIN_RELAY_LED 6
#define PIN_IR_LED 7
#define PIN_DUR_A 8
#define PIN_DUR_B 9
#define PIN_AUX_IN 10
#define PIN_AUX_OUT 11

#define LBL_WIRED "D2"
#define LBL_IR "D3"
#define LBL_NEG "D4"
#define LBL_POS "D5"
#define LBL_DUR_A "D8"
#define LBL_DUR_B "D9"
#define LBL_AUX_IN "D10"
#define LBL_AUX_OUT "D11"
#define LBL_LEDS "relay D6, IR D7"
#define LBL_MP3 "D0 RX <- module TX, D1 TX -> module RX"

#define MP3_PORT Serial1  // USB serial is separate on the Arduino 101, so D0/D1 are free
#define MP3_BEGIN() Serial1.begin(9600)
