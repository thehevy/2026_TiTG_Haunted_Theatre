#include <Arduino.h>

// ARCHIVED 2026-10-26 (never deployed): replaced by components/arduino101/Arduino101_LaserTagIRTrigger, whose D2 wired
// input (wired_input = 1 negative / 2 positive, off by default) drives the same D4 relay with jumper durations.
// Issues:
// - Positive input D7 is INPUT with no pull-down; unwired, it floats and can fire the relay (same as Arduino101_Node).
// - Pins conflicted with the unified layout (D5 trigger LED, D7/D8 inputs vs D5 positive out, D7 IR LED, D8/D9 jumpers).
// - 30 s / 60 s options retired; use a programmable timer relay for long on-times.

// Either trigger input starts one relay pulse; the pulse length comes from the duration selector.
constexpr uint8_t POSITIVE_TRIGGER_PIN = 7;  // active HIGH; needs an external 10 kOhm pull-down to GND
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 8;  // active LOW; internal pull-up
constexpr uint8_t RELAY_PIN = 4;             // active LOW relay module; released (high-Z) when idle
constexpr uint8_t TRIGGER_LED_PIN = 5;
constexpr uint8_t RELAY_LED_PIN = 6;

// Set to false for an input that is not wired, so a floating pin cannot fire the relay.
constexpr bool POSITIVE_TRIGGER_ENABLED = true;
constexpr bool NEGATIVE_TRIGGER_ENABLED = true;

struct DurationOption {
  uint8_t pin;
  uint32_t ms;
};

// Ground exactly one selector pin (rotary switch, DIP switch, or jumper). If several are grounded, the shortest wins.
constexpr DurationOption DURATION_OPTIONS[] = {
  {A0, 1000UL},
  {A1, 5000UL},
  {A2, 15000UL},
  {A3, 30000UL},
  {A4, 60000UL},
};
constexpr uint8_t DURATION_COUNT = sizeof(DURATION_OPTIONS) / sizeof(DURATION_OPTIONS[0]);
constexpr uint32_t DEFAULT_DURATION_MS = 1000UL;  // used when no selector pin is grounded

constexpr uint32_t TRIGGER_DEBOUNCE_MS = 30;
constexpr uint32_t SELECTOR_DEBOUNCE_MS = 100;
constexpr uint32_t HEARTBEAT_MS = 5000;

struct TriggerInput {
  const char *name;
  uint8_t pin;
  uint8_t activeLevel;
  bool enabled;
  bool stableActive;
  bool lastRawActive;
  unsigned long changedAt;
};

TriggerInput triggers[] = {
  {"Positive", POSITIVE_TRIGGER_PIN, HIGH, POSITIVE_TRIGGER_ENABLED, false, false, 0},
  {"Negative", NEGATIVE_TRIGGER_PIN, LOW, NEGATIVE_TRIGGER_ENABLED, false, false, 0},
};
constexpr uint8_t TRIGGER_COUNT = sizeof(triggers) / sizeof(triggers[0]);

bool relayOn = false;
unsigned long pulseStartedAt = 0;
uint32_t pulseDurationMs = 0;
unsigned long lastHeartbeatAt = 0;

uint8_t selectorMask = 0;
uint8_t selectorRawMask = 0;
unsigned long selectorChangedAt = 0;

// Idle is high-Z so the 5 V relay module pulls IN fully HIGH; a 3.3 V HIGH can leave it partly on.
void setRelay(bool on) {
  if (on) {
    pinMode(RELAY_PIN, OUTPUT);
    digitalWrite(RELAY_PIN, LOW);
  } else {
    pinMode(RELAY_PIN, INPUT);
  }
  digitalWrite(RELAY_LED_PIN, on ? HIGH : LOW);
  relayOn = on;
}

bool readTriggerActive(const TriggerInput &t) {
  return digitalRead(t.pin) == t.activeLevel;
}

uint8_t readSelectorMask() {
  uint8_t mask = 0;
  for (uint8_t i = 0; i < DURATION_COUNT; ++i) {
    if (digitalRead(DURATION_OPTIONS[i].pin) == LOW) {
      mask |= (1 << i);
    }
  }
  return mask;
}

int8_t selectedIndex(uint8_t mask) {
  for (uint8_t i = 0; i < DURATION_COUNT; ++i) {
    if (mask & (1 << i)) {
      return i;
    }
  }
  return -1;
}

uint32_t selectedDurationMs() {
  int8_t index = selectedIndex(selectorMask);
  return index < 0 ? DEFAULT_DURATION_MS : DURATION_OPTIONS[index].ms;
}

void printSeconds(uint32_t ms) {
  Serial.print(ms / 1000UL);
  Serial.print(F(" s"));
}

void printPinName(uint8_t pin) {
  if (pin >= A0) {
    Serial.print('A');
    Serial.print(pin - A0);
  } else {
    Serial.print('D');
    Serial.print(pin);
  }
}

void printSelector() {
  Serial.print(F("Duration selector: "));
  int8_t index = selectedIndex(selectorMask);
  if (index < 0) {
    Serial.print(F("no pin grounded, using default "));
    printSeconds(DEFAULT_DURATION_MS);
  } else {
    printSeconds(DURATION_OPTIONS[index].ms);
    Serial.print(F(" ("));
    printPinName(DURATION_OPTIONS[index].pin);
    Serial.print(F(")"));
    if (selectorMask & ~(1 << index)) {
      Serial.print(F(" - WARNING: several pins grounded, using the shortest"));
    }
  }
  Serial.println();
}

void printHeartbeat(unsigned long now) {
  Serial.print(F("Heartbeat: duration "));
  printSeconds(selectedDurationMs());
  for (uint8_t i = 0; i < TRIGGER_COUNT; ++i) {
    const TriggerInput &t = triggers[i];
    Serial.print(F(" | "));
    Serial.print(t.name);
    Serial.print(F(" D"));
    Serial.print(t.pin);
    Serial.print(' ');
    if (!t.enabled) {
      Serial.print(F("disabled"));
    } else {
      Serial.print(t.stableActive ? F("ACTIVE") : F("idle"));
    }
  }
  Serial.print(F(" | relay "));
  if (relayOn) {
    Serial.print(F("ON, "));
    printSeconds(pulseDurationMs - (now - pulseStartedAt) + 999UL);
    Serial.println(F(" left"));
  } else {
    Serial.println(F("off"));
  }
}

void updateSelector(unsigned long now) {
  uint8_t raw = readSelectorMask();
  if (raw != selectorRawMask) {
    selectorRawMask = raw;
    selectorChangedAt = now;
  }
  if (raw != selectorMask && now - selectorChangedAt >= SELECTOR_DEBOUNCE_MS) {
    selectorMask = raw;
    printSelector();
  }
}

void startPulse(const TriggerInput &t, unsigned long now) {
  pulseDurationMs = selectedDurationMs();
  pulseStartedAt = now;
  setRelay(true);
  Serial.print(t.name);
  Serial.print(F(" trigger on D"));
  Serial.print(t.pin);
  Serial.print(F(" -> relay ON for "));
  printSeconds(pulseDurationMs);
  Serial.println();
}

// Edge-triggered: an input must return to idle before it can start another pulse.
void handleTriggers(unsigned long now) {
  bool anyActive = false;
  for (uint8_t i = 0; i < TRIGGER_COUNT; ++i) {
    TriggerInput &t = triggers[i];
    if (!t.enabled) {
      continue;
    }

    bool raw = readTriggerActive(t);
    if (raw != t.lastRawActive) {
      t.lastRawActive = raw;
      t.changedAt = now;
    }
    if (raw != t.stableActive && now - t.changedAt >= TRIGGER_DEBOUNCE_MS) {
      t.stableActive = raw;
      if (t.stableActive) {
        if (relayOn) {
          Serial.print(t.name);
          Serial.println(F(" trigger ignored: relay pulse already running"));
        } else {
          startPulse(t, now);
        }
      }
    }
    anyActive = anyActive || t.stableActive;
  }
  digitalWrite(TRIGGER_LED_PIN, anyActive ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, INPUT);
  digitalWrite(TRIGGER_LED_PIN, LOW);
  pinMode(TRIGGER_LED_PIN, OUTPUT);
  digitalWrite(RELAY_LED_PIN, LOW);
  pinMode(RELAY_LED_PIN, OUTPUT);

  pinMode(POSITIVE_TRIGGER_PIN, INPUT);
  pinMode(NEGATIVE_TRIGGER_PIN, INPUT_PULLUP);
  for (uint8_t i = 0; i < DURATION_COUNT; ++i) {
    pinMode(DURATION_OPTIONS[i].pin, INPUT_PULLUP);
  }
  delay(5);  // let the pull-ups settle before the first read

  unsigned long now = millis();
  selectorMask = selectorRawMask = readSelectorMask();
  selectorChangedAt = now;

  // Inputs already active at boot do not fire; they must be released first.
  for (uint8_t i = 0; i < TRIGGER_COUNT; ++i) {
    TriggerInput &t = triggers[i];
    t.stableActive = t.lastRawActive = t.enabled && readTriggerActive(t);
    t.changedAt = now;
  }

  Serial.println(F("Arduino 101 timed trigger relay ready"));
  Serial.print(F("Inputs: positive D"));
  Serial.print(POSITIVE_TRIGGER_PIN);
  Serial.print(POSITIVE_TRIGGER_ENABLED ? F(" (active HIGH)") : F(" (disabled)"));
  Serial.print(F(", negative D"));
  Serial.print(NEGATIVE_TRIGGER_PIN);
  Serial.println(NEGATIVE_TRIGGER_ENABLED ? F(" (active LOW)") : F(" (disabled)"));
  Serial.print(F("Relay output D"));
  Serial.print(RELAY_PIN);
  Serial.print(F(" (active LOW); status LEDs: trigger D"));
  Serial.print(TRIGGER_LED_PIN);
  Serial.print(F(", relay D"));
  Serial.println(RELAY_LED_PIN);
  Serial.print(F("Selector pins:"));
  for (uint8_t i = 0; i < DURATION_COUNT; ++i) {
    Serial.print(' ');
    printPinName(DURATION_OPTIONS[i].pin);
    Serial.print('=');
    printSeconds(DURATION_OPTIONS[i].ms);
  }
  Serial.println();
  printSelector();
  for (uint8_t i = 0; i < TRIGGER_COUNT; ++i) {
    if (triggers[i].stableActive) {
      Serial.print(triggers[i].name);
      Serial.println(F(" input is active at boot; release it to arm"));
    }
  }
  printHeartbeat(now);
  lastHeartbeatAt = now;
}

void loop() {
  unsigned long now = millis();

  updateSelector(now);
  handleTriggers(now);

  if (relayOn && now - pulseStartedAt >= pulseDurationMs) {
    setRelay(false);
    Serial.println(F("Relay pulse complete -> relay OFF"));
  }

  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat(now);
  }
}
