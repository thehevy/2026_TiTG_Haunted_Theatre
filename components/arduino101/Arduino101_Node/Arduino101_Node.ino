#include <EEPROM.h>
#include <RCSwitch.h>

RCSwitch rfReceiver = RCSwitch();

constexpr uint8_t RF_RECEIVER_PIN = 2;
constexpr uint8_t POSITIVE_TRIGGER_PIN = 7;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 8;
constexpr uint8_t RELAY1_PIN = 4;
constexpr uint8_t RELAY2_PIN = 5;
constexpr uint8_t RELAY3_PIN = 6;

constexpr bool RELAY_ACTIVE_LOW = true;
constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long INPUT_DEBOUNCE_MS = 50;
constexpr uint32_t CONFIG_MAGIC = 0x41433130UL;
constexpr int EEPROM_SLOT_ADDR = 0;

struct Config {
  uint32_t magic;
  uint32_t pulseCode;
  uint32_t lockoutCode;
  uint32_t toggleCode;
};

Config config;

unsigned long relay1OffAt = 0;
unsigned long relay2OffAt = 0;
unsigned long relay3OffAt = 0;
unsigned long lastHeartbeatAt = 0;
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;

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

void holdRelayActive(uint8_t pin, unsigned long &offAt) {
  setRelay(pin, true);
  offAt = 0;
}

void releaseRelay(uint8_t pin, unsigned long &offAt) {
  setRelay(pin, false);
  offAt = 0;
}

void loadConfig() {
  EEPROM.get(EEPROM_SLOT_ADDR, config);
  if (config.magic != CONFIG_MAGIC) {
    config.magic = CONFIG_MAGIC;
    config.pulseCode = 0;
    config.lockoutCode = 0;
    config.toggleCode = 0;
    EEPROM.put(EEPROM_SLOT_ADDR, config);
  }
}

bool codeMatches(uint32_t configuredCode, uint32_t receivedCode) {
  return configuredCode == 0 || configuredCode == receivedCode;
}

void printCode(const __FlashStringHelper *label, uint32_t code) {
  Serial.print(label);
  if (code == 0) {
    Serial.println(F("any"));
  } else {
    Serial.print(F("0x"));
    Serial.println(code, HEX);
  }
}

void printHeader() {
  Serial.println(F("=== Arduino 101 Haunted Node ==="));
  Serial.println(F("Purpose: RF programming input plus level-based local trigger outputs."));
  Serial.println(F("Input pins:"));
  Serial.print(F("  RF receiver data: D"));
  Serial.println(RF_RECEIVER_PIN);
  Serial.print(F("  Positive trigger input (active HIGH, holds Relay 2): D"));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("  Negative trigger input (active LOW, holds Relay 3): D"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.println(F("Output pins:"));
  Serial.print(F("  Relay 1 (RF 0x0 pulse): D"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("  Relay 2 (D7 hold while active): D"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("  Relay 3 (D8 hold while active): D"));
  Serial.println(RELAY3_PIN);
  Serial.println(F("Relay mode: active LOW"));
  Serial.print(F("Pulse duration (ms): "));
  Serial.println(PULSE_MS);
  Serial.println(F("Default RF timeout: none"));
  Serial.println(F("Configured RF codes:"));
  printCode(F("  pulseCode = "), config.pulseCode);
  printCode(F("  lockoutCode = "), config.lockoutCode);
  printCode(F("  toggleCode = "), config.toggleCode);
  Serial.println(F("Ready."));
}

void firePositiveInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: POSITIVE -> Relay 2 active while D7 stays HIGH"));
  holdRelayActive(RELAY2_PIN, relay2OffAt);
  lastPositiveTriggerAt = now;
}

void fireNegativeInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: NEGATIVE -> Relay 3 active while D8 stays LOW"));
  Serial.println(F("Reprinting header due to negative trigger."));
  printHeader();
  holdRelayActive(RELAY3_PIN, relay3OffAt);
  lastNegativeTriggerAt = now;
}

void handleInputTriggers(unsigned long now) {
  bool positiveActive = digitalRead(POSITIVE_TRIGGER_PIN) == HIGH;
  bool negativeActive = digitalRead(NEGATIVE_TRIGGER_PIN) == LOW;

  if (positiveActive && !lastPositiveActive && (now - lastPositiveTriggerAt >= INPUT_DEBOUNCE_MS)) {
    firePositiveInputTrigger(now);
  }

  if (!positiveActive && lastPositiveActive) {
    releaseRelay(RELAY2_PIN, relay2OffAt);
  }

  if (negativeActive && !lastNegativeActive && (now - lastNegativeTriggerAt >= INPUT_DEBOUNCE_MS)) {
    fireNegativeInputTrigger(now);
  }

  if (!negativeActive && lastNegativeActive) {
    releaseRelay(RELAY3_PIN, relay3OffAt);
  }

  lastPositiveActive = positiveActive;
  lastNegativeActive = negativeActive;
}

void setup() {
  Serial.begin(115200);

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

  loadConfig();
  rfReceiver.enableReceive(digitalPinToInterrupt(RF_RECEIVER_PIN));

  printHeader();
}

void loop() {
  unsigned long now = millis();

  handleInputTriggers(now);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    Serial.println(F("Heartbeat: node running"));
  }

  if (timeReached(now, relay1OffAt)) {
    setRelay(RELAY1_PIN, false);
    relay1OffAt = 0;
  }

  if (timeReached(now, relay2OffAt)) {
    setRelay(RELAY2_PIN, false);
    relay2OffAt = 0;
  }

  if (timeReached(now, relay3OffAt)) {
    setRelay(RELAY3_PIN, false);
    relay3OffAt = 0;
  }

  if (rfReceiver.available()) {
    uint32_t received = rfReceiver.getReceivedValue();
    Serial.print(F("RF received: "));
    Serial.println(received);

    if (received == 0x0) {
      pulseRelay(RELAY1_PIN, relay1OffAt, PULSE_MS);
    }

    if (codeMatches(config.pulseCode, received) && received != 0x0) {
      pulseRelay(RELAY1_PIN, relay1OffAt, PULSE_MS);
    }

    if (codeMatches(config.lockoutCode, received)) {
      // Timeout is intentionally disabled by default; keep the stored programming value available without automatic expiry.
    }

    if (codeMatches(config.toggleCode, received)) {
      // Toggle behavior is disabled in the new trigger model; RF programming only configures the relay 1 code.
    }

    rfReceiver.resetAvailable();
  }
}