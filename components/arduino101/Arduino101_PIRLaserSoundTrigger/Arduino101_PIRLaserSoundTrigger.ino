#include <Arduino.h>
#include <DFRobotDFPlayerMini.h>

constexpr uint8_t SENSOR_COUNT = 4;
const char *const SENSOR_NAMES[SENSOR_COUNT] = {"PIR 1", "PIR 2", "PIR 3", "Laser"};
constexpr uint8_t SENSOR_PINS[SENSOR_COUNT] = {2, 3, 4, 5};
// AM312 PIRs drive HIGH on motion; the laser module drives HIGH when the beam is broken.
constexpr uint8_t SENSOR_ACTIVE_LEVEL[SENSOR_COUNT] = {HIGH, HIGH, HIGH, HIGH};
// A disconnected input floats and false-triggers: set unused sensors to false (or tie the pin to GND).
constexpr bool SENSOR_ENABLED[SENSOR_COUNT] = {true, true, true, true};
constexpr uint8_t TRIGGER_OUTPUT_PINS[SENSOR_COUNT] = {6, 7, 8, 9};
constexpr uint16_t SENSOR_TRACKS[SENSOR_COUNT] = {1, 2, 3, 4};  // SD card /mp3/0001.mp3 ... /mp3/0004.mp3

constexpr uint8_t DFPLAYER_BUSY_PIN = 10;  // LOW while a track is playing
constexpr uint8_t PLAYING_LED_PIN = 13;
constexpr uint8_t DFPLAYER_VOLUME = 25;  // 0-30

constexpr uint32_t COOLDOWN_MS = 15000;
constexpr uint32_t SENSOR_WARMUP_MS = 10000;
constexpr uint32_t DEBOUNCE_MS = 50;
constexpr uint32_t BUSY_START_TIMEOUT_MS = 1500;
constexpr uint32_t BUSY_END_CONFIRM_MS = 100;
constexpr uint32_t MAX_SOUND_MS = 180000;
constexpr uint32_t HEARTBEAT_MS = 5000;

enum PlayState : uint8_t { IDLE, STARTING, PLAYING };

DFRobotDFPlayerMini dfPlayer;
bool dfPlayerReady = false;

PlayState playState = IDLE;
int8_t activeSensor = -1;
unsigned long soundStartedAt = 0;
bool busyHighPending = false;
unsigned long busyHighSince = 0;

bool stableActive[SENSOR_COUNT];
bool rawActive[SENSOR_COUNT];
unsigned long rawChangedAt[SENSOR_COUNT];
bool everTriggered[SENSOR_COUNT];
unsigned long lastTriggerAt[SENSOR_COUNT];

unsigned long setupDoneAt = 0;
unsigned long lastHeartbeatAt = 0;

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
  dfPlayer.playMp3Folder(SENSOR_TRACKS[sensor]);

  Serial.print(SENSOR_NAMES[sensor]);
  Serial.print(F(" triggered: output D"));
  Serial.print(TRIGGER_OUTPUT_PINS[sensor]);
  Serial.print(F(" LOW, playing track "));
  Serial.println(SENSOR_TRACKS[sensor]);
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

  bool busy = digitalRead(DFPLAYER_BUSY_PIN) == LOW;

  if (playState == STARTING) {
    if (busy) {
      playState = PLAYING;
    } else if (now - soundStartedAt >= BUSY_START_TIMEOUT_MS) {
      finishSound(F("no playback detected; check BUSY wire on D10, SD card, and track file"));
    }
    return;
  }

  if (now - soundStartedAt >= MAX_SOUND_MS) {
    dfPlayer.stop();
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
  Serial.print(F("Heartbeat: DFPlayer "));
  Serial.print(dfPlayerReady ? F("ready") : F("NOT ready"));
  Serial.print(F(", "));
  if (playState == IDLE) {
    Serial.println(F("idle"));
  } else {
    Serial.print(F("playing "));
    Serial.println(SENSOR_NAMES[activeSensor]);
  }
}

void printConfig() {
  Serial.println(F("Arduino 101 PIR/laser sound trigger ready"));
  for (uint8_t i = 0; i < SENSOR_COUNT; i++) {
    Serial.print(F("  "));
    Serial.print(SENSOR_NAMES[i]);
    Serial.print(F(": input D"));
    Serial.print(SENSOR_PINS[i]);
    Serial.print(F(" -> output D"));
    Serial.print(TRIGGER_OUTPUT_PINS[i]);
    Serial.print(F(", track "));
    Serial.print(SENSOR_TRACKS[i]);
    Serial.println(SENSOR_ENABLED[i] ? F("") : F(" (disabled)"));
  }
  Serial.print(F("DFPlayer: Serial1 D0/D1, BUSY D"));
  Serial.print(DFPLAYER_BUSY_PIN);
  Serial.print(F(", volume "));
  Serial.println(DFPLAYER_VOLUME);
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
  }
  pinMode(DFPLAYER_BUSY_PIN, INPUT_PULLUP);
  digitalWrite(PLAYING_LED_PIN, LOW);
  pinMode(PLAYING_LED_PIN, OUTPUT);

  Serial1.begin(9600);
  dfPlayerReady = dfPlayer.begin(Serial1);
  if (dfPlayerReady) {
    dfPlayer.volume(DFPLAYER_VOLUME);
  }

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
  if (!dfPlayerReady) {
    Serial.println(F("DFPlayer did not respond: check D0/D1 wiring, 5V power, and the SD card"));
  }
  setupDoneAt = now;
  lastHeartbeatAt = now;
}

void loop() {
  unsigned long now = millis();

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
