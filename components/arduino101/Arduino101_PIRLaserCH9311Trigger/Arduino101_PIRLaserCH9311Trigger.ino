#include <Arduino.h>

constexpr uint8_t SENSOR_COUNT = 4;
const char *const SENSOR_NAMES[SENSOR_COUNT] = {"PIR 1", "PIR 2", "PIR 3", "Laser"};
constexpr uint8_t SENSOR_PINS[SENSOR_COUNT] = {2, 3, 4, 5};
// AM312 PIRs drive HIGH on motion; the laser module drives HIGH when the beam is broken.
constexpr uint8_t SENSOR_ACTIVE_LEVEL[SENSOR_COUNT] = {HIGH, HIGH, HIGH, HIGH};
// A disconnected input floats and false-triggers: set unused sensors to false (or tie the pin to GND).
constexpr bool SENSOR_ENABLED[SENSOR_COUNT] = {true, true, true, true};
constexpr uint8_t TRIGGER_OUTPUT_PINS[SENSOR_COUNT] = {6, 7, 8, 9};

// CH9311 is low-level control: holding Kn at GND plays sound n.
constexpr uint8_t CH9311_KEY_PINS[SENSOR_COUNT] = {10, 11, 12, A0};
const char *const CH9311_KEY_PIN_LABELS[SENSOR_COUNT] = {"D10", "D11", "D12", "A0"};
// false: pin sinks Kn directly (only if Kn idles at 5 V or less). true: pin HIGH drives an NPN that pulls Kn to GND.
constexpr bool KEY_DRIVE_VIA_TRANSISTOR = false;
constexpr uint32_t KEY_PRESS_MS = 200;

// The CH9311 has no BUSY header pin; without a wire tapped from its Busy LED, each sound lasts SENSOR_SOUND_MS.
constexpr bool USE_BUSY_PIN = false;
constexpr uint8_t CH9311_BUSY_PIN = A1;
constexpr uint8_t BUSY_ACTIVE_LEVEL = LOW;
constexpr uint32_t SENSOR_SOUND_MS[SENSOR_COUNT] = {10000, 10000, 10000, 10000};  // set to each sound's length

constexpr uint8_t PLAYING_LED_PIN = 13;

constexpr uint32_t COOLDOWN_MS = 15000;
constexpr uint32_t SENSOR_WARMUP_MS = 10000;
constexpr uint32_t DEBOUNCE_MS = 50;
constexpr uint32_t BUSY_START_TIMEOUT_MS = 1500;
constexpr uint32_t BUSY_END_CONFIRM_MS = 100;
constexpr uint32_t MAX_SOUND_MS = 180000;
constexpr uint32_t HEARTBEAT_MS = 5000;

enum PlayState : uint8_t { IDLE, STARTING, PLAYING };

PlayState playState = IDLE;
int8_t activeSensor = -1;
unsigned long soundStartedAt = 0;
bool busyHighPending = false;
unsigned long busyHighSince = 0;
int8_t keySensor = -1;

bool stableActive[SENSOR_COUNT];
bool rawActive[SENSOR_COUNT];
unsigned long rawChangedAt[SENSOR_COUNT];
bool everTriggered[SENSOR_COUNT];
unsigned long lastTriggerAt[SENSOR_COUNT];

unsigned long setupDoneAt = 0;
unsigned long lastHeartbeatAt = 0;

void setKey(uint8_t sensor, bool pressed) {
  uint8_t pin = CH9311_KEY_PINS[sensor];
  if (KEY_DRIVE_VIA_TRANSISTOR) {
    digitalWrite(pin, pressed ? HIGH : LOW);
    return;
  }

  // Released is high-Z so the CH9311's own pull-up holds Kn HIGH.
  if (pressed) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  } else {
    pinMode(pin, INPUT);
  }
}

// Idle is high-Z so a 5 V relay module pulls IN fully HIGH; a 3.3 V HIGH can leave it partly on.
void setTriggerOutput(uint8_t sensor, bool active) {
  if (active) {
    pinMode(TRIGGER_OUTPUT_PINS[sensor], OUTPUT);
    digitalWrite(TRIGGER_OUTPUT_PINS[sensor], LOW);
  } else {
    pinMode(TRIGGER_OUTPUT_PINS[sensor], INPUT);
  }
}

bool readSensorActive(uint8_t sensor) {
  return digitalRead(SENSOR_PINS[sensor]) == SENSOR_ACTIVE_LEVEL[sensor];
}

// Returns true once when the debounced input changes from inactive to active.
bool sensorBecameActive(uint8_t sensor, unsigned long now) {
  bool raw = readSensorActive(sensor);
  if (raw != rawActive[sensor]) {
    rawActive[sensor] = raw;
    rawChangedAt[sensor] = now;
  }

  if (raw == stableActive[sensor] || now - rawChangedAt[sensor] < DEBOUNCE_MS) {
    return false;
  }

  stableActive[sensor] = raw;
  return raw;
}

void startSound(uint8_t sensor, unsigned long now) {
  activeSensor = sensor;
  everTriggered[sensor] = true;
  lastTriggerAt[sensor] = now;
  soundStartedAt = now;
  busyHighPending = false;
  playState = STARTING;

  setTriggerOutput(sensor, true);
  digitalWrite(PLAYING_LED_PIN, HIGH);
  setKey(sensor, true);
  keySensor = sensor;

  Serial.print(SENSOR_NAMES[sensor]);
  Serial.print(F(" triggered: output D"));
  Serial.print(TRIGGER_OUTPUT_PINS[sensor]);
  Serial.print(F(" LOW, pressing CH9311 K"));
  Serial.println(sensor + 1);
}

void updateKeyPress(unsigned long now) {
  if (keySensor >= 0 && now - soundStartedAt >= KEY_PRESS_MS) {
    setKey(keySensor, false);
    keySensor = -1;
  }
}

void finishSound(const __FlashStringHelper *reason) {
  setTriggerOutput(activeSensor, false);
  digitalWrite(PLAYING_LED_PIN, LOW);

  Serial.print(SENSOR_NAMES[activeSensor]);
  Serial.print(F(" released: "));
  Serial.println(reason);

  activeSensor = -1;
  playState = IDLE;
}

void handleTrigger(uint8_t sensor, unsigned long now) {
  if (now - setupDoneAt < SENSOR_WARMUP_MS) {
    return;
  }

  if (playState != IDLE) {
    Serial.print(SENSOR_NAMES[sensor]);
    Serial.print(F(" ignored: "));
    Serial.print(SENSOR_NAMES[activeSensor]);
    Serial.println(F(" sound still playing"));
    return;
  }

  if (everTriggered[sensor] && now - lastTriggerAt[sensor] < COOLDOWN_MS) {
    Serial.print(SENSOR_NAMES[sensor]);
    Serial.print(F(" ignored: cooldown, "));
    Serial.print((COOLDOWN_MS - (now - lastTriggerAt[sensor]) + 999) / 1000);
    Serial.println(F(" s left"));
    return;
  }

  startSound(sensor, now);
}

void updatePlayback(unsigned long now) {
  if (playState == IDLE) {
    return;
  }

  if (!USE_BUSY_PIN) {
    if (now - soundStartedAt >= SENSOR_SOUND_MS[activeSensor]) {
      finishSound(F("sound time elapsed"));
    }
    return;
  }

  bool busy = digitalRead(CH9311_BUSY_PIN) == BUSY_ACTIVE_LEVEL;

  if (playState == STARTING) {
    if (busy) {
      playState = PLAYING;
    } else if (now - soundStartedAt >= BUSY_START_TIMEOUT_MS) {
      finishSound(F("no playback detected; check 12V power, K wiring, DIP switch mode, and BUSY tap"));
    }
    return;
  }

  // Keys only start sounds, so a sound may keep playing after this release.
  if (now - soundStartedAt >= MAX_SOUND_MS) {
    finishSound(F("maximum sound time reached"));
    return;
  }

  if (busy) {
    busyHighPending = false;
    return;
  }

  if (!busyHighPending) {
    busyHighPending = true;
    busyHighSince = now;
  } else if (now - busyHighSince >= BUSY_END_CONFIRM_MS) {
    finishSound(F("sound finished"));
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: CH9311 "));
  if (playState == IDLE) {
    Serial.println(F("idle"));
  } else {
    Serial.print(F("playing "));
    Serial.println(SENSOR_NAMES[activeSensor]);
  }
}

void printConfig() {
  Serial.println(F("Arduino 101 PIR/laser CH9311 sound trigger ready"));
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(SENSOR_NAMES[i]);
    Serial.print(F(": input D"));
    Serial.print(SENSOR_PINS[i]);
    Serial.print(F(" -> output D"));
    Serial.print(TRIGGER_OUTPUT_PINS[i]);
    Serial.print(F(", CH9311 K"));
    Serial.print(i + 1);
    Serial.print(F(" on "));
    Serial.print(CH9311_KEY_PIN_LABELS[i]);
    if (!USE_BUSY_PIN) {
      Serial.print(F(", sound "));
      Serial.print(SENSOR_SOUND_MS[i]);
      Serial.print(F(" ms"));
    }
    Serial.println(SENSOR_ENABLED[i] ? F("") : F(" (disabled)"));
  }
  Serial.print(F("CH9311 key drive: "));
  Serial.println(KEY_DRIVE_VIA_TRANSISTOR ? F("NPN transistor (pin HIGH = press)") : F("direct (pin LOW = press, high-Z = release)"));
  Serial.print(F("End of sound: "));
  Serial.println(USE_BUSY_PIN ? F("BUSY tap on A1") : F("fixed SENSOR_SOUND_MS per sensor"));
  Serial.print(F("Cooldown per sensor (ms): "));
  Serial.println(COOLDOWN_MS);
  Serial.print(F("Sensor warm-up (ms): "));
  Serial.println(SENSOR_WARMUP_MS);
}

void setup() {
  Serial.begin(115200);

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    setTriggerOutput(i, false);
    pinMode(SENSOR_PINS[i], INPUT);
    if (KEY_DRIVE_VIA_TRANSISTOR) {
      digitalWrite(CH9311_KEY_PINS[i], LOW);
      pinMode(CH9311_KEY_PINS[i], OUTPUT);
    }
    setKey(i, false);
  }
  if (USE_BUSY_PIN) {
    pinMode(CH9311_BUSY_PIN, INPUT);
  }
  digitalWrite(PLAYING_LED_PIN, LOW);
  pinMode(PLAYING_LED_PIN, OUTPUT);

  unsigned long now = millis();
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    bool active = SENSOR_ENABLED[i] && readSensorActive(i);
    stableActive[i] = active;
    rawActive[i] = active;
    rawChangedAt[i] = now;
    everTriggered[i] = false;
    lastTriggerAt[i] = 0;
  }

  printConfig();
  setupDoneAt = now;
  lastHeartbeatAt = now;
}

void loop() {
  unsigned long now = millis();

  updateKeyPress(now);

  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    if (SENSOR_ENABLED[i] && sensorBecameActive(i, now)) {
      handleTrigger(i, now);
    }
  }

  updatePlayback(now);

  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
