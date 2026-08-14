#include <Arduino.h>
#include <IRremote.hpp>

constexpr uint8_t RELAY1_PIN = 4;
constexpr uint8_t RELAY2_PIN = 5;
constexpr uint8_t RELAY3_PIN = 6;
constexpr uint8_t POSITIVE_TRIGGER_PIN = 7;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 8;
constexpr uint8_t IR_RECEIVER_PIN = 2;
constexpr bool RELAY_ACTIVE_LOW = true;

constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long LOCKOUT_MS = 15000;
constexpr unsigned long INPUT_DEBOUNCE_MS = 50;
constexpr unsigned long IR_DEBOUNCE_MS = 250;
constexpr uint8_t IR_COMMAND_RELAY1 = 0x45;
constexpr uint8_t IR_COMMAND_RELAY2 = 0x46;
constexpr uint8_t IR_COMMAND_RELAY3 = 0x47;

bool relayState[3] = {false, false, false};
unsigned long relayOffAt[3] = {0, 0, 0};
unsigned long relay2LockoutUntil = 0;
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;
unsigned long lastHeartbeatAt = 0;
unsigned long lastIrTriggerAt = 0;
uint8_t lastIrCommand = 0;
uint32_t lastIrRawData = 0;

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

bool timeReached(unsigned long now, unsigned long target) {
  return target != 0 && static_cast<long>(now - target) >= 0;
}

void printHeader() {
  Serial.println(F("=== UNO Node ==="));
  Serial.println(F("Purpose: Serial, IR, and local trigger relay controller"));
  Serial.print(F("Relay 1 pulse pin: D"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("Relay 2 pulse+lockout pin: D"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("Relay 3 toggle pin: D"));
  Serial.println(RELAY3_PIN);
  Serial.print(F("Positive trigger input (active HIGH): D"));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("Negative trigger input (active LOW): D"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.print(F("IR receiver input: D"));
  Serial.println(IR_RECEIVER_PIN);
  Serial.println(F("IR commands: 1->relay1 pulse, 2->relay2 pulse, 3->relay3 toggle"));
  Serial.println(F("Serial debug command: i -> print last IR code"));
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
    if (command == IR_COMMAND_RELAY1) {
      pulseRelay(0, RELAY1_PIN, PULSE_MS);
    } else if (command == IR_COMMAND_RELAY2) {
      if (relay2LockoutUntil == 0) {
        pulseRelay(1, RELAY2_PIN, PULSE_MS);
        relay2LockoutUntil = millis() + LOCKOUT_MS;
      } else {
        Serial.println(F("IR relay2 command ignored during lockout"));
      }
    } else if (command == IR_COMMAND_RELAY3) {
      toggleRelay(2, RELAY3_PIN);
    }

    lastIrTriggerAt = now;
  }

  IrReceiver.resume();
}

void firePositiveInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: POSITIVE"));
  pulseRelay(0, RELAY1_PIN, PULSE_MS);
  lastPositiveTriggerAt = now;
}

void fireNegativeInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: NEGATIVE"));
  Serial.println(F("Reprinting header due to negative trigger."));
  printHeader();
  if (relay2LockoutUntil == 0) {
    pulseRelay(1, RELAY2_PIN, PULSE_MS);
    relay2LockoutUntil = millis() + LOCKOUT_MS;
  } else {
    Serial.println(F("Negative input ignored during lockout"));
  }
  lastNegativeTriggerAt = now;
}

void handleInputTriggers(unsigned long now) {
  bool positiveActive = digitalRead(POSITIVE_TRIGGER_PIN) == HIGH;
  bool negativeActive = digitalRead(NEGATIVE_TRIGGER_PIN) == LOW;

  if (positiveActive && !lastPositiveActive && (now - lastPositiveTriggerAt >= INPUT_DEBOUNCE_MS)) {
    firePositiveInputTrigger(now);
  }

  if (negativeActive && !lastNegativeActive && (now - lastNegativeTriggerAt >= INPUT_DEBOUNCE_MS)) {
    fireNegativeInputTrigger(now);
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

  IrReceiver.begin(IR_RECEIVER_PIN, DISABLE_LED_FEEDBACK);

  printHeader();
  Serial.println(F("Use serial commands: 1, 2, 3, a, b, c"));
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
      case 'i': printLastIrDebug(); break;
      default: break;
    }
  }
}

void loop() {
  unsigned long now = millis();
  handleInputTriggers(now);
  handleIrInput(now);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    Serial.println(F("Heartbeat: node running"));
  }

  if (timeReached(now, relay2LockoutUntil)) {
    relay2LockoutUntil = 0;
  }

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