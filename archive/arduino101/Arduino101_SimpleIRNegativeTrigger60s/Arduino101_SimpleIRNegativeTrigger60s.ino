// ARCHIVED 2026-10-26: replaced by components/arduino101/Arduino101_LaserTagIRTrigger. The 60 s duration is retired;
// use a programmable timer relay for long on-times.
// Issues seen: fires on any IR activity, including iPhone camera / Face ID IR, so guests filming the haunt set off
// props. Only useful as a fallback on a controller without free pins for the full sketch.
#include <Arduino.h>

// IRremote has no Curie support; the receiver output idles HIGH and pulses LOW on any IR carrier.
constexpr uint8_t IR_RECEIVER_PIN = 3;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 4;
constexpr uint8_t IR_LED_PIN = 5;
constexpr uint8_t RELAY_LED_PIN = 6;
constexpr uint32_t TRIGGER_PULSE_MS = 60000;

constexpr uint32_t HEARTBEAT_MS = 5000;
constexpr uint32_t IR_LED_HOLD_MS = 100;
constexpr uint32_t MIN_IR_MARK_US = 200;  // shorter LOW blips are treated as noise

bool triggerActive = false;
unsigned long triggerStartedAt = 0;
unsigned long lastHeartbeatAt = 0;
bool irMarkInProgress = false;
unsigned long irMarkStartedAt = 0;
bool irActivitySeen = false;
unsigned long lastIrActivityAt = 0;

// Idle is high-Z so the 5 V relay module pulls IN fully HIGH; a 3.3 V HIGH can leave it partly on.
void setTrigger(bool active) {
  if (active) {
    pinMode(NEGATIVE_TRIGGER_PIN, OUTPUT);
    digitalWrite(NEGATIVE_TRIGGER_PIN, LOW);
  } else {
    pinMode(NEGATIVE_TRIGGER_PIN, INPUT);
  }
  digitalWrite(RELAY_LED_PIN, active ? HIGH : LOW);
  triggerActive = active;
  if (active) {
    triggerStartedAt = millis();
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: IR input D"));
  Serial.print(IR_RECEIVER_PIN);
  Serial.print(F(", negative trigger output D"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
}

// Lights on any raw receiver activity, including bursts too short to trigger the relay.
void updateIrLed(unsigned long now) {
  if (digitalRead(IR_RECEIVER_PIN) == LOW) {
    irActivitySeen = true;
    lastIrActivityAt = now;
  }
  digitalWrite(IR_LED_PIN, irActivitySeen && now - lastIrActivityAt < IR_LED_HOLD_MS ? HIGH : LOW);
}

void handleIrInput() {
  if (digitalRead(IR_RECEIVER_PIN) == HIGH) {
    irMarkInProgress = false;
    return;
  }

  if (!irMarkInProgress) {
    irMarkInProgress = true;
    irMarkStartedAt = micros();
    return;
  }

  if (micros() - irMarkStartedAt >= MIN_IR_MARK_US) {
    if (!triggerActive) {
      Serial.print(F("IR signal received; negative trigger LOW for "));
      Serial.print(TRIGGER_PULSE_MS);
      Serial.println(F(" ms"));
    }
    setTrigger(true);
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(NEGATIVE_TRIGGER_PIN, INPUT);
  pinMode(IR_RECEIVER_PIN, INPUT_PULLUP);
  digitalWrite(IR_LED_PIN, LOW);
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(RELAY_LED_PIN, LOW);
  pinMode(RELAY_LED_PIN, OUTPUT);

  Serial.println(F("Arduino 101 simple IR negative trigger (60 s) ready"));
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
  handleIrInput();

  if (triggerActive && now - triggerStartedAt >= TRIGGER_PULSE_MS) {
    setTrigger(false);
  }

  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
