#include <Arduino.h>
#include <IRremote.hpp>

constexpr uint8_t IR_RECEIVER_PIN = 19;
constexpr uint8_t POSITIVE_TRIGGER_PIN = 27;
constexpr uint8_t IR_LED_PIN = 25;
constexpr uint8_t RELAY_LED_PIN = 26;
constexpr uint32_t TRIGGER_PULSE_MS = 500;
constexpr uint32_t HEARTBEAT_MS = 5000;
constexpr uint32_t IR_LED_HOLD_MS = 100;

bool positiveTriggerActive = false;
unsigned long positiveTriggerStartedAt = 0;
unsigned long lastHeartbeatAt = 0;
bool irActivitySeen = false;
unsigned long lastIrActivityAt = 0;

void setPositiveTrigger(bool active) {
  digitalWrite(POSITIVE_TRIGGER_PIN, active ? HIGH : LOW);
  digitalWrite(RELAY_LED_PIN, active ? HIGH : LOW);
  positiveTriggerActive = active;
  if (active) {
    positiveTriggerStartedAt = millis();
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: IR input GPIO"));
  Serial.print(IR_RECEIVER_PIN);
  Serial.print(F(", positive trigger output GPIO"));
  Serial.println(POSITIVE_TRIGGER_PIN);
}

// Raw receiver activity, so the LED lights even when IRremote can't decode the signal.
void updateIrLed(unsigned long now) {
  if (digitalRead(IR_RECEIVER_PIN) == LOW) {
    irActivitySeen = true;
    lastIrActivityAt = now;
  }
  digitalWrite(IR_LED_PIN, irActivitySeen && now - lastIrActivityAt < IR_LED_HOLD_MS ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);

  digitalWrite(POSITIVE_TRIGGER_PIN, LOW);
  pinMode(POSITIVE_TRIGGER_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW);
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(RELAY_LED_PIN, LOW);
  pinMode(RELAY_LED_PIN, OUTPUT);
  IrReceiver.begin(IR_RECEIVER_PIN, DISABLE_LED_FEEDBACK);

  Serial.println(F("WeMos D1 R32 simple IR positive trigger ready"));
  Serial.print(F("Status LEDs: IR GPIO"));
  Serial.print(IR_LED_PIN);
  Serial.print(F(", relay GPIO"));
  Serial.println(RELAY_LED_PIN);
  printHeartbeat();
  lastHeartbeatAt = millis();
}

void loop() {
  unsigned long now = millis();

  updateIrLed(now);

  if (IrReceiver.decode()) {
    setPositiveTrigger(true);
    Serial.println(F("IR signal received; positive trigger HIGH for 500 ms"));
    IrReceiver.resume();
  }

  if (positiveTriggerActive && now - positiveTriggerStartedAt >= TRIGGER_PULSE_MS) {
    setPositiveTrigger(false);
  }

  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
