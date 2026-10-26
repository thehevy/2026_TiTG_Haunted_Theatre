// ARCHIVED 2026-10-26: replaced by components/uno/UNO_LaserTagIRTrigger.
// Issues seen: fires on any IR activity, including iPhone camera / Face ID IR, so guests filming the haunt set off
// props. Only useful as a fallback on a controller without free pins for the full sketch.
//
// Very simple Arduino Uno R3 IR trigger node
// - IR receiver on Digital Pin 2
// - Negative trigger output on Digital Pin 8
// - Any valid IR signal triggers a 500 ms active-low pulse
// - No sleep, no deep power management
// - Heartbeat every 5 seconds prints the pin numbers

#include <Arduino.h>
#include <IRremote.hpp>

constexpr uint8_t IR_RECEIVER_PIN = 2;       // D2
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 8;  // D8, active LOW
constexpr uint8_t IR_LED_PIN = 5;
constexpr uint8_t RELAY_LED_PIN = 6;
constexpr uint32_t TRIGGER_PULSE_MS = 500;
constexpr uint32_t HEARTBEAT_MS = 5000;
constexpr uint32_t IR_LED_HOLD_MS = 100;

unsigned long negativeTriggerOffAt = 0;
unsigned long lastHeartbeatAt = 0;
bool irActivitySeen = false;
unsigned long lastIrActivityAt = 0;

void setNegativeTrigger(bool active) {
  // Active LOW negative trigger: LOW = active, HIGH = idle
  digitalWrite(NEGATIVE_TRIGGER_PIN, active ? LOW : HIGH);
  digitalWrite(RELAY_LED_PIN, active ? HIGH : LOW);
}

// Raw receiver activity, so the LED lights even when IRremote can't decode the signal.
void updateIrLed(unsigned long now) {
  if (digitalRead(IR_RECEIVER_PIN) == LOW) {
    irActivitySeen = true;
    lastIrActivityAt = now;
  }
  digitalWrite(IR_LED_PIN, irActivitySeen && now - lastIrActivityAt < IR_LED_HOLD_MS ? HIGH : LOW);
}

void triggerNegativePulse() {
  setNegativeTrigger(true);
  negativeTriggerOffAt = millis() + TRIGGER_PULSE_MS;
  Serial.print(F("IR signal received on D"));
  Serial.print(IR_RECEIVER_PIN);
  Serial.print(F(" -> negative trigger D"));
  Serial.print(NEGATIVE_TRIGGER_PIN);
  Serial.println(F(" LOW for 500 ms"));
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: IR input D"));
  Serial.print(IR_RECEIVER_PIN);
  Serial.print(F(", output D"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(RELAY_LED_PIN, OUTPUT);
  pinMode(NEGATIVE_TRIGGER_PIN, OUTPUT);
  setNegativeTrigger(false);  // idle HIGH for active LOW trigger

  IrReceiver.begin(IR_RECEIVER_PIN, DISABLE_LED_FEEDBACK);

  Serial.println(F("Simple IR -> negative trigger node ready"));
  Serial.print(F("IR receiver pin: D"));
  Serial.println(IR_RECEIVER_PIN);
  Serial.print(F("Negative trigger output pin: D"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.print(F("Status LEDs: IR D"));
  Serial.print(IR_LED_PIN);
  Serial.print(F(", relay D"));
  Serial.println(RELAY_LED_PIN);
  printHeartbeat();
  lastHeartbeatAt = millis();
}

void loop() {
  unsigned long now = millis();

  updateIrLed(now);

  if (IrReceiver.decode()) {
    triggerNegativePulse();
    IrReceiver.resume();
  }

  if (negativeTriggerOffAt != 0 && now >= negativeTriggerOffAt) {
    setNegativeTrigger(false);
    negativeTriggerOffAt = 0;
  }

  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
