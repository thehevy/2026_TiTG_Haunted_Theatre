// WeMos D1 R32 Node
// ESP32-based board in Arduino UNO R3 form factor.
// Serial and IR relay controller - same behavior as UNO_Node.
// Board: ESP32 Dev Module in Arduino IDE (IDE 2.x recommended).
// NOTE: 3.3V logic. Use relay modules with optocoupler inputs that accept 3.3V.
// NOTE: KY-022 IR receiver must be powered from 3.3V, not 5V.

#include <Arduino.h>
#include <IRremote.hpp>

// GPIO assignments for WeMos D1 R32 (UNO form factor ESP32).
// These match safe non-strapping GPIOs on the ESP32-WROOM module.
// Silk label reference: D4=GPIO16, D3=GPIO17, D5=GPIO18, D6=GPIO19, D7=GPIO23, D9=GPIO27
constexpr uint8_t RELAY1_PIN          = 16;  // D4 label on board
constexpr uint8_t RELAY2_PIN          = 17;  // D3 label on board
constexpr uint8_t RELAY3_PIN          = 18;  // D5 label on board
constexpr uint8_t POSITIVE_TRIGGER_PIN = 23; // D7 label on board - active HIGH
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 27; // D9 label on board - active LOW
constexpr uint8_t IR_RECEIVER_PIN     = 19;  // D6 label on board
constexpr bool    RELAY_ACTIVE_LOW    = true;

constexpr unsigned long PULSE_MS               = 250;
constexpr unsigned long LOCKOUT_MS             = 15000;
constexpr unsigned long INPUT_DEBOUNCE_MS      = 50;
constexpr unsigned long IR_DEBOUNCE_MS         = 250;
constexpr unsigned long IR_LOCKOUT_MODE_NONE_MS = 0;
constexpr unsigned long IR_LOCKOUT_MODE_5S_MS  = 5000;
constexpr unsigned long IR_LOCKOUT_MODE_15S_MS = 15000;

// IR remote button codes (NEC protocol, same remote as UNO_Node).
constexpr uint8_t IR_CODE_A1    = 0x0C;
constexpr uint8_t IR_CODE_A2    = 0x18;
constexpr uint8_t IR_CODE_A3    = 0x5E;
constexpr uint8_t IR_CODE_A4    = 0x08;
constexpr uint8_t IR_CODE_A5    = 0x1C;
constexpr uint8_t IR_CODE_A6    = 0x5A;
constexpr uint8_t IR_CODE_A7    = 0x42;
constexpr uint8_t IR_CODE_M1    = 0x07;
constexpr uint8_t IR_CODE_M2    = 0x15;
constexpr uint8_t IR_CODE_M3    = 0x09;
constexpr uint8_t IR_CODE_POWER = 0x45;

bool relayState[3] = {false, false, false};
unsigned long relayOffAt[3] = {0, 0, 0};
unsigned long relay2LockoutUntil = 0;
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;
unsigned long lastHeartbeatAt = 0;
unsigned long lastIrTriggerAt = 0;
unsigned long irActionLockoutMs = IR_LOCKOUT_MODE_15S_MS;
unsigned long irActionLockoutUntil = 0;
int lastRelay2LockoutCountdownSeconds = -1;
int lastIrLockoutCountdownSeconds = -1;
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
  Serial.println(F("=== WeMos D1 R32 Node ==="));
  Serial.println(F("Purpose: Serial, IR, and local trigger relay controller"));
  Serial.println(F("Logic: 3.3V - use 3.3V-compatible relay modules and KY-022 on 3.3V"));
  Serial.print(F("Relay 1 pulse: GPIO"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("Relay 2 pulse+lockout: GPIO"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("Relay 3 toggle: GPIO"));
  Serial.println(RELAY3_PIN);
  Serial.print(F("Positive trigger input (active HIGH): GPIO"));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("Negative trigger input (active LOW): GPIO"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.print(F("IR receiver: GPIO"));
  Serial.println(IR_RECEIVER_PIN);
  Serial.println(F("IR map: A1/A2/A3 pulse, A4/A5/A6 toggle, A7 pulse all"));
  Serial.println(F("IR map: M1/M2/M3 lockout none/5s/15s, POWER reprint header"));
  Serial.println(F("Serial debug: i -> print last IR code"));
  Serial.println(F("Serial commands: 1,2,3 pulse | a,b,c toggle"));
  Serial.println(F("Ready."));
}

bool isIrLockoutActive(unsigned long now) {
  return irActionLockoutUntil != 0 && static_cast<long>(now - irActionLockoutUntil) < 0;
}

void startIrLockoutIfEnabled(unsigned long now) {
  if (irActionLockoutMs == 0) {
    irActionLockoutUntil = 0;
    return;
  }
  irActionLockoutUntil = now + irActionLockoutMs;
}

void setIrLockoutMode(unsigned long durationMs) {
  irActionLockoutMs = durationMs;
  irActionLockoutUntil = 0;
  Serial.print(F("IR lockout mode set to: "));
  if (durationMs == 0) {
    Serial.println(F("none"));
  } else {
    Serial.print(durationMs / 1000);
    Serial.println(F(" seconds"));
  }
}

void pulseAllRelays() {
  pulseRelay(0, RELAY1_PIN, PULSE_MS);
  pulseRelay(1, RELAY2_PIN, PULSE_MS);
  pulseRelay(2, RELAY3_PIN, PULSE_MS);
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

void printLockoutCountdown(const __FlashStringHelper *label, unsigned long now, unsigned long lockoutUntil,
                           int &lastPrintedSeconds) {
  if (lockoutUntil == 0 || static_cast<long>(now - lockoutUntil) >= 0) {
    if (lastPrintedSeconds != -1) {
      Serial.print(label);
      Serial.println(F(" lockout cleared"));
      lastPrintedSeconds = -1;
    }
    return;
  }
  unsigned long remainingMs = lockoutUntil - now;
  int remainingSeconds = static_cast<int>((remainingMs + 999) / 1000);
  if (remainingSeconds != lastPrintedSeconds) {
    Serial.print(label);
    Serial.print(F(" lockout remaining: "));
    Serial.print(remainingSeconds);
    Serial.println(F("s"));
    lastPrintedSeconds = remainingSeconds;
  }
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
    if (command == IR_CODE_M1) {
      setIrLockoutMode(IR_LOCKOUT_MODE_NONE_MS);
    } else if (command == IR_CODE_M2) {
      setIrLockoutMode(IR_LOCKOUT_MODE_5S_MS);
    } else if (command == IR_CODE_M3) {
      setIrLockoutMode(IR_LOCKOUT_MODE_15S_MS);
    } else if (command == IR_CODE_POWER) {
      Serial.println(F("IR POWER received: reprinting header."));
      printHeader();
    } else if (command == IR_CODE_A4) {
      toggleRelay(0, RELAY1_PIN);
    } else if (command == IR_CODE_A5) {
      toggleRelay(1, RELAY2_PIN);
    } else if (command == IR_CODE_A6) {
      toggleRelay(2, RELAY3_PIN);
    } else if (command == IR_CODE_A1 || command == IR_CODE_A2 ||
               command == IR_CODE_A3 || command == IR_CODE_A7) {
      if (isIrLockoutActive(now)) {
        Serial.println(F("IR pulse action ignored during lockout"));
      } else if (command == IR_CODE_A1) {
        pulseRelay(0, RELAY1_PIN, PULSE_MS);
        startIrLockoutIfEnabled(now);
      } else if (command == IR_CODE_A2) {
        pulseRelay(1, RELAY2_PIN, PULSE_MS);
        startIrLockoutIfEnabled(now);
      } else if (command == IR_CODE_A3) {
        pulseRelay(2, RELAY3_PIN, PULSE_MS);
        startIrLockoutIfEnabled(now);
      } else if (command == IR_CODE_A7) {
        pulseAllRelays();
        startIrLockoutIfEnabled(now);
      }
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
}

void loop() {
  unsigned long now = millis();
  handleInputTriggers(now);
  handleIrInput(now);

  printLockoutCountdown(F("IR pulse action"), now, irActionLockoutUntil, lastIrLockoutCountdownSeconds);
  printLockoutCountdown(F("Relay2 input"), now, relay2LockoutUntil, lastRelay2LockoutCountdownSeconds);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    Serial.println(F("Heartbeat: node running"));
  }

  if (timeReached(now, relay2LockoutUntil)) {
    relay2LockoutUntil = 0;
  }

  if (timeReached(now, irActionLockoutUntil)) {
    irActionLockoutUntil = 0;
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
