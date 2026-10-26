#pragma once
// Arduino Nano pin map for the shared *_LaserTagIRTrigger sketch. Keep in sync with PINOUT.md.
#include <SoftwareSerial.h>

#define BOARD_NAME "Arduino Nano"
#define HAS_WIFI 0
#define IR_USE_INTERRUPT 0  // polled, the same as the Arduino 101, for identical timing
#define RELEASE_NEGATIVE_WHEN_IDLE 0
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
#define LBL_MP3 "A0 RX <- module TX, A1 TX -> module RX"

static SoftwareSerial mp3Serial(A0, A1);  // D0/D1 are the USB serial port
#define MP3_PORT mp3Serial
#define MP3_BEGIN() mp3Serial.begin(9600)
