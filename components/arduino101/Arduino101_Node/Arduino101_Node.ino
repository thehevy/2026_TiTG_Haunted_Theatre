#include <EEPROM.h>
#include <RCSwitch.h>

RCSwitch rfReceiver = RCSwitch();

constexpr uint8_t RF_RECEIVER_PIN = 2;
constexpr uint8_t RELAY1_PIN = 4;
constexpr uint8_t RELAY2_PIN = 5;
constexpr uint8_t RELAY3_PIN = 6;

constexpr bool RELAY_ACTIVE_LOW = true;
constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long LOCKOUT_MS = 15000;
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
unsigned long relay2LockoutUntil = 0;
bool relay3State = false;
unsigned long lastHeartbeatAt = 0;

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

void toggleRelay(uint8_t pin, bool &state) {
  state = !state;
  setRelay(pin, state);
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

void setup() {
  Serial.begin(115200);

  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  setRelay(RELAY1_PIN, false);
  setRelay(RELAY2_PIN, false);
  setRelay(RELAY3_PIN, false);

  loadConfig();
  rfReceiver.enableReceive(digitalPinToInterrupt(RF_RECEIVER_PIN));

  Serial.println(F("=== Arduino 101 Haunted Node ==="));
  Serial.println(F("Purpose: RF receiver drives three relay behaviors."));
  Serial.println(F("Input pin:"));
  Serial.print(F("  RF receiver data: D"));
  Serial.println(RF_RECEIVER_PIN);
  Serial.println(F("Output pins:"));
  Serial.print(F("  Relay 1 (momentary pulse): D"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("  Relay 2 (momentary pulse + 15s lockout): D"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("  Relay 3 (toggle): D"));
  Serial.println(RELAY3_PIN);
  Serial.println(F("Relay mode: active LOW"));
  Serial.print(F("Pulse duration (ms): "));
  Serial.println(PULSE_MS);
  Serial.print(F("Lockout duration (ms): "));
  Serial.println(LOCKOUT_MS);
  Serial.println(F("Configured RF codes:"));
  printCode(F("  pulseCode = "), config.pulseCode);
  printCode(F("  lockoutCode = "), config.lockoutCode);
  printCode(F("  toggleCode = "), config.toggleCode);
  Serial.println(F("Ready."));
}

void loop() {
  unsigned long now = millis();

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

  if (timeReached(now, relay2LockoutUntil)) {
    relay2LockoutUntil = 0;
  }

  if (rfReceiver.available()) {
    uint32_t received = rfReceiver.getReceivedValue();
    Serial.print(F("RF received: "));
    Serial.println(received);

    if (codeMatches(config.pulseCode, received)) {
      pulseRelay(RELAY1_PIN, relay1OffAt, PULSE_MS);
    }

    if (codeMatches(config.lockoutCode, received) && relay2LockoutUntil == 0) {
      pulseRelay(RELAY2_PIN, relay2OffAt, PULSE_MS);
      relay2LockoutUntil = millis() + LOCKOUT_MS;
    }

    if (codeMatches(config.toggleCode, received)) {
      toggleRelay(RELAY3_PIN, relay3State);
    }

    rfReceiver.resetAvailable();
  }
}