// ARCHIVED 2026-10-26 (never uploaded). Non-WiFi boards use Arduino101_/UNO_/Nano_LaserTagIRTrigger with the Web Serial
// config page instead. If an ESP32 build is needed later, port the unified sketch rather than reviving this one.
//
// WeMos D1 R32 Node
// ESP32-based board in Arduino UNO R3 form factor.
// Serial and IR relay controller with held-trigger behavior.
// Board: ESP32 Dev Module in Arduino IDE (IDE 2.x recommended).
// NOTE: 3.3V logic. Use relay modules with optocoupler inputs that accept 3.3V.
// NOTE: KY-022 IR receiver must be powered from 3.3V, not 5V.

#include <Arduino.h>
#include <IRremote.hpp>

// GPIO assignments for WeMos D1 R32 (UNO form factor ESP32).
// Silk label reference: D4=GPIO16, D3=GPIO17, D5=GPIO18, D6=GPIO19, D7=GPIO23, D9=GPIO27
constexpr uint8_t IR_RECEIVER_PIN = 19;  // D6 label on board
constexpr uint8_t POSITIVE_TRIGGER_PIN = 23; // D7 label on board - active HIGH
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 27; // D9 label on board - active LOW
constexpr uint8_t RELAY1_PIN = 16;  // D4 label on board
constexpr uint8_t RELAY2_PIN = 17;  // D3 label on board
constexpr uint8_t RELAY3_PIN = 18;  // D5 label on board
constexpr bool RELAY_ACTIVE_LOW = true;

constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long INPUT_DEBOUNCE_MS = 50;
constexpr unsigned long IR_DEBOUNCE_MS = 250;
constexpr uint8_t IR_CODE_0 = 0x00;

unsigned long relay1OffAt = 0;
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;
unsigned long lastHeartbeatAt = 0;
unsigned long lastIrTriggerAt = 0;
uint8_t lastIrCommand = 0;
uint32_t lastIrRawData = 0;

bool timeReached(unsigned long now, unsigned long target) {
  return target != 0 && static_cast<long>(now - target) >= 0;
}

void setRelay(uint8_t pin, bool on) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void pulseRelay(uint8_t pin, unsigned long &offAt, unsigned long durationMs) {
  setRelay(pin, true);
  offAt = millis() + durationMs;
}

void holdRelayActive(uint8_t pin) {
  setRelay(pin, true);
}

void releaseRelay(uint8_t pin) {
  setRelay(pin, false);
}

void printHeader() {
  Serial.println(F("=== WeMos D1 R32 Node ==="));
  Serial.println(F("Purpose: IR programming input plus level-based local trigger outputs."));
  Serial.println(F("Input pins:"));
  Serial.print(F("  IR receiver data: GPIO"));
  Serial.println(IR_RECEIVER_PIN);
  Serial.print(F("  Positive trigger input (active HIGH, holds Relay 2): GPIO"));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("  Negative trigger input (active LOW, holds Relay 3): GPIO"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.println(F("Output pins:"));
  Serial.print(F("  Relay 1 (IR 0x00 pulse): GPIO"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("  Relay 2 (D7 hold while active): GPIO"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("  Relay 3 (D9 hold while active): GPIO"));
  Serial.println(RELAY3_PIN);
  Serial.println(F("Relay mode: active LOW"));
  Serial.print(F("Pulse duration (ms): "));
  Serial.println(PULSE_MS);
  Serial.println(F("Default timeout: none"));
  Serial.println(F("IR programming: 0x00 triggers Relay 1 once."));
  Serial.println(F("Ready."));
}

void printLastIrDebug() {
  Serial.print(F("Last IR command: 0x"));
  if (lastIrCommand < 0x10) {
    Serial.print('0');
  }
  Serial.println(lastIrCommand, HEX);

  Serial.print(F("Last IR raw: 0x"));
  Serial.println(lastIrRawData, HEX);
}

void handleIrInput(unsigned long now) {
  if (!IrReceiver.decode()) {
    return;
  }

  uint8_t command = IrReceiver.decodedIRData.command;
  bool isRepeat = (IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT) != 0;
  lastIrCommand = command;
  lastIrRawData = IrReceiver.decodedIRData.decodedRawData;

  Serial.print(F("IR command: 0x"));
  Serial.println(command, HEX);

  if (!isRepeat && (now - lastIrTriggerAt >= IR_DEBOUNCE_MS)) {
    if (command == IR_CODE_0) {
      pulseRelay(RELAY1_PIN, relay1OffAt, PULSE_MS);
      Serial.println(F("IR 0x00 received: Relay 1 pulse"));
    }

    lastIrTriggerAt = now;
  }

  IrReceiver.resume();
}

void firePositiveInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: POSITIVE -> Relay 2 active while D7 stays HIGH"));
  holdRelayActive(RELAY2_PIN);
  lastPositiveTriggerAt = now;
}

void fireNegativeInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: NEGATIVE -> Relay 3 active while D9 stays LOW"));
  holdRelayActive(RELAY3_PIN);
  lastNegativeTriggerAt = now;
}

void handleInputTriggers(unsigned long now) {
  bool positiveActive = digitalRead(POSITIVE_TRIGGER_PIN) == HIGH;
  bool negativeActive = digitalRead(NEGATIVE_TRIGGER_PIN) == LOW;

  if (positiveActive && !lastPositiveActive && (now - lastPositiveTriggerAt >= INPUT_DEBOUNCE_MS)) {
    firePositiveInputTrigger(now);
  }

  if (!positiveActive && lastPositiveActive) {
    releaseRelay(RELAY2_PIN);
  }

  if (negativeActive && !lastNegativeActive && (now - lastNegativeTriggerAt >= INPUT_DEBOUNCE_MS)) {
    fireNegativeInputTrigger(now);
  }

  if (!negativeActive && lastNegativeActive) {
    releaseRelay(RELAY3_PIN);
  }

  lastPositiveActive = positiveActive;
  lastNegativeActive = negativeActive;
}

void setup() {
  Serial.begin(115200);

  pinMode(IR_RECEIVER_PIN, INPUT);
  pinMode(POSITIVE_TRIGGER_PIN, INPUT);
  pinMode(NEGATIVE_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  setRelay(RELAY1_PIN, false);
  setRelay(RELAY2_PIN, false);
  setRelay(RELAY3_PIN, false);

  lastPositiveActive = digitalRead(POSITIVE_TRIGGER_PIN) == HIGH;
  lastNegativeActive = digitalRead(NEGATIVE_TRIGGER_PIN) == LOW;

  IrReceiver.begin(IR_RECEIVER_PIN, DISABLE_LED_FEEDBACK);

  printHeader();
}

void loop() {
  unsigned long now = millis();

  handleInputTriggers(now);
  handleIrInput(now);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    Serial.println(F("Heartbeat: node running"));
  }

  if (timeReached(now, relay1OffAt)) {
    setRelay(RELAY1_PIN, false);
    relay1OffAt = 0;
  }
}
