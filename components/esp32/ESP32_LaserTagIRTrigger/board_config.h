#pragma once
// ESP32 DevKit (ESP32-WROOM-32) and Acebott ESP32-Max V1.0 pin map for the shared *_LaserTagIRTrigger sketch.
// The Acebott header is labeled with the same GPIO numbers. Keep in sync with PINOUT.md.
#define BOARD_NAME "ESP32"
#define HAS_WIFI 1
#define IR_USE_INTERRUPT 1
#define RELEASE_NEGATIVE_WHEN_IDLE 1  // 3.3 V I/O: released lets a 5 V relay module's IN pull fully HIGH
#define HAS_POS 1
#define HAS_WIRED 1
#define HAS_AUX 1
#define HAS_DURATION_PINS 1
#define HAS_RELAY_LED 1
#define RELAY_LED_ACTIVE_LOW 0
#define HAS_IR_LED 1
#define HAS_MP3 1
#define HAS_AP_BUTTON 1

#define PIN_WIRED 27
#define PIN_IR 19
#define PIN_NEG 4
#define PIN_POS 18
#define PIN_RELAY_LED 21
#define PIN_IR_LED 22
#define PIN_DUR_A 32
#define PIN_DUR_B 33
#define PIN_AUX_IN 26
#define PIN_AUX_OUT 25
#define PIN_AP_BUTTON 0  // BOOT button

#define LBL_WIRED "GPIO27"
#define LBL_IR "GPIO19"
#define LBL_NEG "GPIO4"
#define LBL_POS "GPIO18"
#define LBL_DUR_A "GPIO32"
#define LBL_DUR_B "GPIO33"
#define LBL_AUX_IN "GPIO26"
#define LBL_AUX_OUT "GPIO25"
#define LBL_LEDS "relay GPIO21, IR GPIO22"
#define LBL_MP3 "GPIO16 RX <- module TX, GPIO17 TX -> module RX"
#define LBL_AP_TRIGGER "BOOT 3 s, or reset 3 times quickly"

#define MP3_PORT Serial2
#define MP3_BEGIN() Serial2.begin(9600, SERIAL_8N1, 16, 17)
