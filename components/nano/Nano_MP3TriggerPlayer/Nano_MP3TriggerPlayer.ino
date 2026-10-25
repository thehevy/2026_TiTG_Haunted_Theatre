#include <Arduino.h>
#include <SoftwareSerial.h>

// Arduino Nano + DIYables Serial MP3 Player (YX5300 chip, micro SD, 3.5 mm jack).
// Each trigger input plays its own MP3 file exactly once. While a file is playing, every other trigger is ignored
// (not queued). Playback end is detected from the module's "track finished" message (0x3D), with a status poll and
// a maximum play time as backups.
// - Negative trigger inputs: INPUT_PULLUP, fire on the HIGH -> LOW edge (button or dry contact to GND).
// - PIR input (HC-SR501): fires on the LOW -> HIGH edge. It must drop LOW again before it can fire again, so one
//   motion event plays the file once even if the PIR output stays HIGH.
// SD card layout: folder "01" with files named 001*.mp3, 002*.mp3, ... (for example 01/001_scream.mp3).

constexpr uint8_t MP3_RX_PIN = 7;  // Nano RX <- module TX
constexpr uint8_t MP3_TX_PIN = 6;  // Nano TX -> module RX
constexpr uint8_t STATUS_LED_PIN = LED_BUILTIN;  // onboard D13, on while a file is playing

constexpr uint8_t MP3_VOLUME = 25;  // 0..30

struct TriggerInput {
  uint8_t pin;
  bool activeLow;  // true = negative trigger (INPUT_PULLUP); false = active HIGH sensor such as a PIR
  uint8_t folder;  // SD folder 01..99
  uint8_t file;    // file number 001..255 inside the folder
  const char *name;
};

TriggerInput triggers[] = {
  {2, true, 1, 1, "Trigger D2"},
  {3, true, 1, 2, "Trigger D3"},
  {4, true, 1, 3, "Trigger D4"},
  {5, true, 1, 4, "Trigger D5"},
  {8, false, 1, 5, "PIR D8"},
};
constexpr uint8_t TRIGGER_COUNT = sizeof(triggers) / sizeof(triggers[0]);

constexpr uint32_t DEBOUNCE_MS = 30;
constexpr uint32_t PIR_WARMUP_MS = 30000;          // HC-SR501 output is unreliable for ~30-60 s after power-up
constexpr uint32_t POST_PLAY_LOCKOUT_MS = 1000;    // quiet time after a file ends before the next trigger
constexpr uint32_t STATUS_POLL_GRACE_MS = 1500;    // module reports "stopped" briefly after a play command
constexpr uint32_t STATUS_POLL_MS = 2000;          // 0 = rely only on the 0x3D finished message and MAX_PLAY_MS
constexpr uint32_t MAX_PLAY_MS = 300000;           // safety unlock if no end-of-track message arrives
constexpr uint32_t HEARTBEAT_MS = 5000;

// YX5300 serial commands
constexpr uint8_t CMD_SET_VOLUME = 0x06;
constexpr uint8_t CMD_SEL_DEV = 0x09;
constexpr uint8_t CMD_STOP = 0x16;
constexpr uint8_t CMD_PLAY_FOLDER_FILE = 0x0F;
constexpr uint8_t CMD_QUERY_STATUS = 0x42;
constexpr uint8_t DEV_TF = 0x02;
// YX5300 replies
constexpr uint8_t RPL_CARD_INSERTED = 0x3A;
constexpr uint8_t RPL_CARD_REMOVED = 0x3B;
constexpr uint8_t RPL_TRACK_FINISHED = 0x3D;
constexpr uint8_t RPL_INIT_DONE = 0x3F;
constexpr uint8_t RPL_ERROR = 0x40;

SoftwareSerial mp3(MP3_RX_PIN, MP3_TX_PIN);

bool stableActive[TRIGGER_COUNT];
bool lastRawActive[TRIGGER_COUNT];
unsigned long lastRawChangeAt[TRIGGER_COUNT];

bool playing = false;
int8_t playingIndex = -1;
unsigned long playStartedAt = 0;
unsigned long lastStatusPollAt = 0;
unsigned long lockoutUntil = 0;
unsigned long bootAt = 0;
unsigned long lastHeartbeatAt = 0;
unsigned long playsStarted = 0;
unsigned long triggersIgnored = 0;

uint8_t rxFrame[10];
uint8_t rxIndex = 0;

void mp3Command(uint8_t command, uint16_t data) {
  const uint8_t frame[8] = {0x7E, 0xFF, 0x06, command, 0x00, (uint8_t)(data >> 8), (uint8_t)data, 0xEF};
  mp3.write(frame, sizeof(frame));
}

bool readActive(const TriggerInput &t) {
  const int level = digitalRead(t.pin);
  return t.activeLow ? level == LOW : level == HIGH;
}

void printFile(uint8_t folder, uint8_t file) {
  if (folder < 10) Serial.print('0');
  Serial.print(folder);
  Serial.print('/');
  if (file < 100) Serial.print('0');
  if (file < 10) Serial.print('0');
  Serial.print(file);
}

void startPlayback(uint8_t index) {
  const TriggerInput &t = triggers[index];
  mp3Command(CMD_PLAY_FOLDER_FILE, ((uint16_t)t.folder << 8) | t.file);
  playing = true;
  playingIndex = index;
  playStartedAt = millis();
  lastStatusPollAt = playStartedAt;
  playsStarted++;
  digitalWrite(STATUS_LED_PIN, HIGH);
  Serial.print(t.name);
  Serial.print(F(" -> PLAY "));
  printFile(t.folder, t.file);
  Serial.println(F(" (other triggers locked out until it finishes)"));
}

void endPlayback(const __FlashStringHelper *reason) {
  if (!playing) {
    return;
  }
  playing = false;
  lockoutUntil = millis() + POST_PLAY_LOCKOUT_MS;
  digitalWrite(STATUS_LED_PIN, LOW);
  Serial.print(F("Playback ended ("));
  Serial.print(reason);
  Serial.print(F(") after "));
  Serial.print((millis() - playStartedAt) / 1000.0, 1);
  Serial.println(F(" s; triggers re-armed"));
  playingIndex = -1;
}

void handleReply(uint8_t command, uint8_t dataHigh, uint8_t dataLow) {
  switch (command) {
    case RPL_TRACK_FINISHED:
      endPlayback(F("track finished"));
      break;
    case CMD_QUERY_STATUS:
      // dataLow: 0 = stopped, 1 = playing, 2 = paused
      if (playing && dataLow == 0 && millis() - playStartedAt >= STATUS_POLL_GRACE_MS) {
        endPlayback(F("status stopped"));
      }
      break;
    case RPL_ERROR:
      Serial.print(F("MP3 module error 0x"));
      Serial.print(dataLow, HEX);
      Serial.println(F(" (check SD card and folder/file names)"));
      endPlayback(F("module error"));
      break;
    case RPL_CARD_INSERTED:
      Serial.println(F("MP3 module: SD card inserted"));
      mp3Command(CMD_SEL_DEV, DEV_TF);
      break;
    case RPL_CARD_REMOVED:
      Serial.println(F("MP3 module: SD card removed"));
      endPlayback(F("card removed"));
      break;
    case RPL_INIT_DONE:
      Serial.println(F("MP3 module: initialized"));
      break;
    default:
      break;
  }
  (void)dataHigh;
}

// Replies are 7E FF 06 cmd fb dh dl EF (YX5300). 10-byte frames with a checksum before EF are also accepted.
void readMp3Replies() {
  while (mp3.available()) {
    const uint8_t b = mp3.read();
    if (b == 0x7E) {
      rxIndex = 0;
    }
    if (rxIndex >= sizeof(rxFrame)) {
      rxIndex = 0;
      continue;
    }
    rxFrame[rxIndex++] = b;
    if (b == 0xEF && rxFrame[0] == 0x7E && (rxIndex == 8 || rxIndex == 10)) {
      handleReply(rxFrame[3], rxFrame[5], rxFrame[6]);
      rxIndex = 0;
    }
  }
}

void checkPlaybackEnd() {
  if (!playing) {
    return;
  }
  const unsigned long now = millis();
  if (now - playStartedAt >= MAX_PLAY_MS) {
    mp3Command(CMD_STOP, 0);
    endPlayback(F("MAX_PLAY_MS timeout"));
    return;
  }
  if (STATUS_POLL_MS > 0 && now - playStartedAt >= STATUS_POLL_GRACE_MS && now - lastStatusPollAt >= STATUS_POLL_MS) {
    lastStatusPollAt = now;
    mp3Command(CMD_QUERY_STATUS, 0);
  }
}

void pollTriggers() {
  const unsigned long now = millis();
  for (uint8_t i = 0; i < TRIGGER_COUNT; i++) {
    const bool raw = readActive(triggers[i]);
    if (raw != lastRawActive[i]) {
      lastRawActive[i] = raw;
      lastRawChangeAt[i] = now;
    }
    if (raw == stableActive[i] || now - lastRawChangeAt[i] < DEBOUNCE_MS) {
      continue;
    }
    stableActive[i] = raw;
    if (!raw) {
      continue;  // released / PIR back to LOW: re-arms this input
    }

    const TriggerInput &t = triggers[i];
    if (!t.activeLow && now - bootAt < PIR_WARMUP_MS) {
      Serial.print(t.name);
      Serial.println(F(" ignored (PIR warm-up)"));
    } else if (playing) {
      triggersIgnored++;
      Serial.print(t.name);
      Serial.print(F(" ignored (busy playing "));
      printFile(triggers[playingIndex].folder, triggers[playingIndex].file);
      Serial.println(')');
    } else if ((long)(now - lockoutUntil) < 0) {
      triggersIgnored++;
      Serial.print(t.name);
      Serial.println(F(" ignored (post-play lockout)"));
    } else {
      startPlayback(i);
    }
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: "));
  if (playing) {
    Serial.print(F("playing "));
    printFile(triggers[playingIndex].folder, triggers[playingIndex].file);
  } else {
    Serial.print(F("idle"));
  }
  Serial.print(F(", inputs "));
  for (uint8_t i = 0; i < TRIGGER_COUNT; i++) {
    Serial.print('D');
    Serial.print(triggers[i].pin);
    Serial.print('=');
    Serial.print(stableActive[i] ? F("ACTIVE") : F("idle"));
    Serial.print(i + 1 < TRIGGER_COUNT ? F(" ") : F(""));
  }
  Serial.print(F(", plays "));
  Serial.print(playsStarted);
  Serial.print(F(", ignored "));
  Serial.println(triggersIgnored);
}

void setup() {
  Serial.begin(115200);
  pinMode(STATUS_LED_PIN, OUTPUT);
  digitalWrite(STATUS_LED_PIN, LOW);

  for (uint8_t i = 0; i < TRIGGER_COUNT; i++) {
    pinMode(triggers[i].pin, triggers[i].activeLow ? INPUT_PULLUP : INPUT);
  }

  mp3.begin(9600);
  delay(500);  // module boot time
  mp3Command(CMD_SEL_DEV, DEV_TF);
  delay(200);
  mp3Command(CMD_SET_VOLUME, MP3_VOLUME);
  delay(100);

  // Inputs already active at boot (held button, PIR HIGH) must release before they can fire.
  const unsigned long now = millis();
  for (uint8_t i = 0; i < TRIGGER_COUNT; i++) {
    stableActive[i] = lastRawActive[i] = readActive(triggers[i]);
    lastRawChangeAt[i] = now;
  }
  bootAt = now;

  Serial.println(F("Arduino Nano MP3 trigger player ready"));
  Serial.print(F("MP3 module: Nano D"));
  Serial.print(MP3_RX_PIN);
  Serial.print(F(" <- TX, D"));
  Serial.print(MP3_TX_PIN);
  Serial.print(F(" -> RX, volume "));
  Serial.println(MP3_VOLUME);
  for (uint8_t i = 0; i < TRIGGER_COUNT; i++) {
    Serial.print(triggers[i].name);
    Serial.print(triggers[i].activeLow ? F(": active LOW (to GND) -> ") : F(": active HIGH (PIR) -> "));
    printFile(triggers[i].folder, triggers[i].file);
    Serial.println();
  }
  Serial.print(F("PIR warm-up: "));
  Serial.print(PIR_WARMUP_MS / 1000);
  Serial.println(F(" s"));
}

void loop() {
  readMp3Replies();
  checkPlaybackEnd();
  pollTriggers();

  const unsigned long now = millis();
  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
