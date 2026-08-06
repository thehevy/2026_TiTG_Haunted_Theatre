#include <Arduino.h>

constexpr uint8_t RELAY1_PIN = 4;
constexpr uint8_t RELAY2_PIN = 5;
constexpr uint8_t RELAY3_PIN = 6;
constexpr bool RELAY_ACTIVE_LOW = true;

constexpr unsigned long PULSE_MS = 250;

bool relayState[3] = {false, false, false};
unsigned long relayOffAt[3] = {0, 0, 0};

void setRelay(uint8_t pin, bool on) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void pulseRelay(uint8_t index, uint8_t pin, unsigned long durationMs) {
  setRelay(pin, true);
  relayOffAt[index] = millis() + durationMs;
}

void toggleRelay(uint8_t index, uint8_t pin) {
  relayState[index] = !relayState[index];
  setRelay(pin, relayState[index]);
}

void setup() {
  Serial.begin(115200);
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  setRelay(RELAY1_PIN, false);
  setRelay(RELAY2_PIN, false);
  setRelay(RELAY3_PIN, false);

  Serial.println(F("UNO relay node ready"));
  Serial.println(F("Use serial commands: 1, 2, 3, t1, t2, t3"));
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char command = static_cast<char>(Serial.read());

    switch (command) {
      case '1': pulseRelay(0, RELAY1_PIN, PULSE_MS); break;
      case '2': pulseRelay(1, RELAY2_PIN, PULSE_MS); break;
      case '3': pulseRelay(2, RELAY3_PIN, PULSE_MS); break;
      case 'a': toggleRelay(0, RELAY1_PIN); break;
      case 'b': toggleRelay(1, RELAY2_PIN); break;
      case 'c': toggleRelay(2, RELAY3_PIN); break;
      default: break;
    }
  }
}

void loop() {
  unsigned long now = millis();
  readSerialCommands();

  for (uint8_t i = 0; i < 3; ++i) {
    if (relayOffAt[i] != 0 && static_cast<long>(now - relayOffAt[i]) >= 0) {
      relayOffAt[i] = 0;
      if (i == 0) setRelay(RELAY1_PIN, false);
      if (i == 1) setRelay(RELAY2_PIN, false);
      if (i == 2) setRelay(RELAY3_PIN, false);
    }
  }
}