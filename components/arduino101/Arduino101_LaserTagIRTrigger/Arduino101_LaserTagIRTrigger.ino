#include <Arduino.h>
#include <EEPROM.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

// Laser tag IR trigger. The same source is used by Arduino101_, UNO_, and Nano_LaserTagIRTrigger; keep the copies identical.
// - Main trigger: any valid IR code (laser tag gun, TV remote, LED-strip remote) on D3 pulls D4 LOW and drives D5 HIGH
//   for the duration selected with jumpers on D8/D9 (read live; a change applies to the next trigger).
// - Wired trigger: D2 fires the same main outputs. Off by default (wired_input setting) so an unwired pin cannot fire.
// - Aux trigger: a HIGH -> LOW edge on D10 (button or contact to GND) pulls D11 LOW for aux_ms.
// - Each output ignores new triggers until its pulse ends plus rearm_ms, so edge-triggered props see one event per pulse.
// - Settings are changed over USB serial (115200) with tools/web-config/index.html or typed commands (HELP), saved in EEPROM.
// - Optional DFPlayer Mini (or compatible Mini MP3 module), auto-detected: IR plays the next file in SD folder 01 (back to
//   001 after mp3_reset_s with no IR sound), D2 plays 02/001, D10 plays 02/002.
// Phones (Face ID / camera IR) and sunlight do not form a valid frame, so they never trigger.
// Decoded IR formats (receiver output idles HIGH, LOW while the 38 kHz carrier is present):
// - Pulse width (laser tag guns, Sony): leader mark, then each bit's mark length is the data (short 0, long 1).
// - Pulse distance (NEC, Samsung, Panasonic, most LED-strip remotes): leader, short marks, the space length is the data.
// - NEC repeat frames (button held) are logged but do not trigger.

#if defined(ARDUINO_ARCH_ARC32)
#define BOARD_NAME "Arduino 101"
constexpr bool RELEASE_NEGATIVE_WHEN_IDLE = true;  // 3.3 V I/O: released lets a 5 V relay module's IN pull fully HIGH
#elif defined(ARDUINO_AVR_NANO)
#define BOARD_NAME "Arduino Nano"
constexpr bool RELEASE_NEGATIVE_WHEN_IDLE = false;
#elif defined(ARDUINO_AVR_UNO)
#define BOARD_NAME "Arduino UNO"
constexpr bool RELEASE_NEGATIVE_WHEN_IDLE = false;
#else
#error "Unsupported board: add it to the board block at the top of the sketch"
#endif

#if defined(ARDUINO_ARCH_ARC32)
#define MP3_PORT Serial1  // USB serial is separate on the Arduino 101, so D0/D1 are free
#define MP3_PINS "D0 RX <- module TX, D1 TX -> module RX"
#else
#include <SoftwareSerial.h>
SoftwareSerial mp3Serial(A0, A1);  // D0/D1 are the USB serial port on the UNO/Nano
#define MP3_PORT mp3Serial
#define MP3_PINS "A0 RX <- module TX, A1 TX -> module RX"
#endif

constexpr uint8_t WIRED_INPUT_PIN = 2;       // main trigger input; mode set by wired_input
constexpr uint8_t IR_RECEIVER_PIN = 3;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 4;  // main, active LOW
constexpr uint8_t POSITIVE_TRIGGER_PIN = 5;  // main, active HIGH (board logic level)
constexpr uint8_t RELAY_LED_PIN = 6;         // on while either output is active
constexpr uint8_t IR_LED_PIN = 7;            // on briefly for any IR activity, including ignored IR
constexpr uint8_t DURATION_PIN_A = 8;        // INPUT_PULLUP; jumper to GND adds 1 to the duration index
constexpr uint8_t DURATION_PIN_B = 9;        // INPUT_PULLUP; jumper to GND adds 2
constexpr uint8_t AUX_INPUT_PIN = 10;        // INPUT_PULLUP, negative trigger input
constexpr uint8_t AUX_OUTPUT_PIN = 11;       // aux, active LOW

#define FIRMWARE_VERSION "2026-10-26"

// Index 1: D8 GND | 2: D9 GND | 3: D8 + D9 GND. Index 0 (no jumper) uses the saved main_ms setting.
constexpr uint16_t JUMPER_DURATIONS_MS[4] = {0, 5000, 10000, 15000};
constexpr uint32_t AUX_DEBOUNCE_MS = 30;
constexpr uint32_t WIRED_DEBOUNCE_MS = 30;
constexpr uint32_t DURATION_POLL_MS = 50;
constexpr uint32_t HEARTBEAT_MS = 5000;
constexpr uint32_t IR_LED_HOLD_MS = 100;

constexpr uint32_t FRAME_END_GAP_US = 8000;  // HIGH this long ends a capture; the longest NEC space is 4.5 ms
constexpr uint8_t MAX_DURATIONS = 200;
constexpr uint8_t PW_MIN_BITS = 6;
constexpr uint8_t PW_MAX_BITS = 24;
constexpr uint8_t PD_MIN_BITS = 12;
constexpr uint8_t PD_MAX_BITS = 64;
constexpr uint8_t MIN_REPORTED_DURATIONS = 6;  // shorter unknown captures are noise and are not printed

constexpr uint8_t IR_FORMAT_PULSE_WIDTH = 1;     // laser tag guns, Sony
constexpr uint8_t IR_FORMAT_PULSE_DISTANCE = 2;  // NEC, Samsung, Panasonic, LED-strip remotes
constexpr uint8_t WIRED_OFF = 0;
constexpr uint8_t WIRED_NEGATIVE = 1;  // INPUT_PULLUP, fires on HIGH -> LOW
constexpr uint8_t WIRED_POSITIVE = 2;  // INPUT, fires on LOW -> HIGH; needs an external 10 kOhm pull-down
constexpr uint16_t CONFIG_MAGIC = 0x4C54;
constexpr uint8_t CONFIG_VERSION = 3;
constexpr uint8_t NAME_MAX = 16;
constexpr uint8_t CMD_MAX = 64;

struct Config {
  uint16_t magic;
  uint8_t version;
  char name[NAME_MAX + 1];
  uint16_t mainMs;       // main pulse when no duration jumper is installed
  uint16_t auxMs;
  uint16_t rearmMs;      // idle time after a pulse before that output can fire again
  uint8_t pwMinFrames;   // raise to 2 if phones or sunlight ever fire the trigger
  uint8_t irFormats;     // IR_FORMAT_* bits
  uint8_t printTimings;  // 1 = dump mark/space lengths of signals that do not decode
  uint8_t wiredInput;    // WIRED_*
  uint8_t mp3Volume;     // 0-30
  uint16_t mp3ResetS;    // IR sound sequence restarts at 001 after this long with no IR sound; 0 = never
  uint8_t checksum;
};

Config config;
bool configDirty = false;
char cmdBuffer[CMD_MAX + 1];
uint8_t cmdLength = 0;
bool cmdOverflow = false;

struct PulseOutput {
  bool active;
  unsigned long startedAt;
  unsigned long endedAt;
  uint32_t lengthMs;
};

PulseOutput mainPulse = {false, 0, 0, 0};
PulseOutput auxPulse = {false, 0, 0, 0};

// Alternating mark (LOW) and space (HIGH) lengths in microseconds, starting with the first mark.
uint16_t durations[MAX_DURATIONS];
uint8_t durationCount = 0;
bool frameOverflow = false;
bool frameInProgress = false;
int lastLevel = HIGH;
unsigned long lastEdgeAt = 0;
bool lastFrameWasRepeat = false;

uint8_t durationIndex = 0;
uint8_t durationCandidate = 0;
unsigned long lastDurationPollAt = 0;

bool auxStableLow = false;
bool auxLastRawLow = false;
unsigned long auxLastChangeAt = 0;

bool wiredStableActive = false;
bool wiredLastRawActive = false;
unsigned long wiredLastChangeAt = 0;

unsigned long irFired = 0;
unsigned long irIgnored = 0;
unsigned long wiredFired = 0;
unsigned long wiredIgnored = 0;
unsigned long auxFired = 0;
unsigned long auxIgnored = 0;
unsigned long lastHeartbeatAt = 0;
bool irActivitySeen = false;
unsigned long lastIrActivityAt = 0;

void writeNegative(uint8_t pin, bool active) {
  if (active) {
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  } else if (RELEASE_NEGATIVE_WHEN_IDLE) {
    pinMode(pin, INPUT);
  } else {
    digitalWrite(pin, HIGH);  // set HIGH before OUTPUT so the relay never blips at boot
    pinMode(pin, OUTPUT);
  }
}

void applyOutputs() {
  writeNegative(NEGATIVE_TRIGGER_PIN, mainPulse.active);
  digitalWrite(POSITIVE_TRIGGER_PIN, mainPulse.active ? HIGH : LOW);
  writeNegative(AUX_OUTPUT_PIN, auxPulse.active);
  digitalWrite(RELAY_LED_PIN, mainPulse.active || auxPulse.active ? HIGH : LOW);
}

bool isReady(const PulseOutput &pulse, unsigned long now) {
  return !pulse.active && now - pulse.endedAt >= config.rearmMs;
}

void startPulse(PulseOutput &pulse, uint32_t lengthMs, unsigned long now) {
  pulse.active = true;
  pulse.startedAt = now;
  pulse.lengthMs = lengthMs;
  applyOutputs();
}

bool endPulseIfDue(PulseOutput &pulse, unsigned long now) {
  if (!pulse.active || now - pulse.startedAt < pulse.lengthMs) {
    return false;
  }
  pulse.active = false;
  pulse.endedAt = now;
  applyOutputs();
  return true;
}

void printHex(uint32_t value, uint8_t digits) {
  Serial.print(F("0x"));
  for (int8_t shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
    Serial.print((value >> shift) & 0xF, HEX);
  }
}

// DFPlayer Mini frames: 7E FF 06 cmd feedback data_hi data_lo checksum_hi checksum_lo EF. No library, so nothing blocks.
constexpr uint8_t MP3_CMD_SET_VOLUME = 0x06;
constexpr uint8_t MP3_CMD_SEL_DEV = 0x09;
constexpr uint8_t MP3_CMD_PLAY_FOLDER_FILE = 0x0F;
constexpr uint8_t MP3_CMD_QUERY_STATUS = 0x42;
constexpr uint8_t MP3_CMD_QUERY_FOLDER_COUNT = 0x4E;
constexpr uint8_t MP3_RPL_CARD_INSERTED = 0x3A;
constexpr uint8_t MP3_RPL_CARD_REMOVED = 0x3B;
constexpr uint8_t MP3_RPL_ERROR = 0x40;
constexpr uint8_t MP3_DEV_TF = 0x02;
constexpr uint8_t MP3_IR_FOLDER = 1;
constexpr uint8_t MP3_OTHER_FOLDER = 2;
constexpr uint8_t MP3_WIRED_FILE = 1;
constexpr uint8_t MP3_AUX_FILE = 2;
constexpr uint8_t MP3_QUEUE_SIZE = 6;
constexpr uint32_t MP3_CMD_GAP_MS = 100;  // the module drops commands sent back to back
constexpr uint32_t MP3_FIRST_PROBE_MS = 1500;
constexpr uint32_t MP3_PROBE_INTERVAL_MS = 2500;
constexpr uint8_t MP3_PROBE_COUNT = 3;

struct Mp3Command {
  uint8_t command;
  uint16_t data;
};

Mp3Command mp3Queue[MP3_QUEUE_SIZE];
uint8_t mp3QueueHead = 0;
uint8_t mp3QueueCount = 0;
unsigned long mp3LastSendAt = 0;
bool mp3Present = false;
bool mp3AbsentReported = false;
uint8_t mp3ProbesSent = 0;
uint8_t mp3RxFrame[10];
uint8_t mp3RxIndex = 0;
int16_t mp3IrFileCount = -1;  // -1 = not known yet
uint8_t mp3NextIrFile = 1;
uint8_t mp3LastPlayFolder = 0;
uint8_t mp3LastPlayFile = 0;
bool mp3IrPlayed = false;
unsigned long mp3LastIrPlayAt = 0;

void mp3Send(uint8_t command, uint16_t data) {
  uint8_t frame[10] = {0x7E, 0xFF, 0x06, command, 0x00, (uint8_t)(data >> 8), (uint8_t)data, 0, 0, 0xEF};
  uint16_t checksum = 0;
  for (uint8_t i = 1; i < 7; ++i) {
    checksum -= frame[i];
  }
  frame[7] = checksum >> 8;
  frame[8] = checksum & 0xFF;
  MP3_PORT.write(frame, sizeof(frame));
  mp3LastSendAt = millis();
}

void mp3Enqueue(uint8_t command, uint16_t data) {
  if (mp3QueueCount == MP3_QUEUE_SIZE) {
    Serial.println(F("MP3 command queue full; command dropped"));
    return;
  }
  Mp3Command &slot = mp3Queue[(mp3QueueHead + mp3QueueCount) % MP3_QUEUE_SIZE];
  slot.command = command;
  slot.data = data;
  ++mp3QueueCount;
}

void printMp3File(uint8_t folder, uint8_t file) {
  if (folder < 10) {
    Serial.print('0');
  }
  Serial.print(folder);
  Serial.print('/');
  if (file < 100) {
    Serial.print('0');
  }
  if (file < 10) {
    Serial.print('0');
  }
  Serial.print(file);
}

void mp3Play(uint8_t folder, uint8_t file) {
  if (!mp3Present) {
    return;
  }
  mp3LastPlayFolder = folder;
  mp3LastPlayFile = file;
  mp3Enqueue(MP3_CMD_PLAY_FOLDER_FILE, ((uint16_t)folder << 8) | file);
  Serial.print(F("  MP3 play "));
  printMp3File(folder, file);
  Serial.println();
}

void playIrSound(unsigned long now) {
  if (!mp3Present) {
    return;
  }
  if (mp3IrFileCount == 0) {
    Serial.println(F("  MP3: folder 01 is empty"));
    return;
  }
  if (mp3IrPlayed && config.mp3ResetS > 0 && now - mp3LastIrPlayAt >= config.mp3ResetS * 1000UL) {
    mp3NextIrFile = 1;
  }
  if (mp3NextIrFile == 0 || (mp3IrFileCount > 0 && mp3NextIrFile > mp3IrFileCount)) {
    mp3NextIrFile = 1;
  }
  mp3Play(MP3_IR_FOLDER, mp3NextIrFile++);
  mp3IrPlayed = true;
  mp3LastIrPlayAt = now;
}

void mp3OnDetected() {
  mp3Present = true;
  Serial.println(F("DFPlayer detected"));
  mp3Enqueue(MP3_CMD_SEL_DEV, MP3_DEV_TF);
  mp3Enqueue(MP3_CMD_SET_VOLUME, config.mp3Volume);
  mp3Enqueue(MP3_CMD_QUERY_FOLDER_COUNT, MP3_IR_FOLDER);
}

void handleMp3Reply(uint8_t command, uint8_t dataHigh, uint8_t dataLow) {
  if (!mp3Present) {
    mp3OnDetected();
  }
  switch (command) {
    case MP3_RPL_CARD_INSERTED:
      Serial.println(F("MP3: SD card inserted"));
      mp3IrFileCount = -1;
      mp3Enqueue(MP3_CMD_SEL_DEV, MP3_DEV_TF);
      mp3Enqueue(MP3_CMD_QUERY_FOLDER_COUNT, MP3_IR_FOLDER);
      break;
    case MP3_RPL_CARD_REMOVED:
      Serial.println(F("MP3: SD card removed"));
      mp3IrFileCount = -1;
      break;
    case MP3_CMD_QUERY_FOLDER_COUNT:
      mp3IrFileCount = ((uint16_t)dataHigh << 8) | dataLow;
      Serial.print(F("MP3: folder 01 has "));
      Serial.print(mp3IrFileCount);
      Serial.println(F(" files"));
      break;
    case MP3_RPL_ERROR:
      Serial.print(F("MP3 error 0x"));
      Serial.print(dataLow, HEX);
      // A missing file past the end of folder 01 marks the end of the IR sequence.
      if (mp3LastPlayFolder == MP3_IR_FOLDER && mp3LastPlayFile > 1) {
        mp3IrFileCount = mp3LastPlayFile - 1;
        Serial.print(F(": 01/ has "));
        Serial.print(mp3IrFileCount);
        Serial.println(F(" files; restarting at 001"));
        mp3NextIrFile = 1;
        mp3Play(MP3_IR_FOLDER, mp3NextIrFile++);
      } else {
        Serial.println(F(" (check the SD card folder and file names)"));
      }
      mp3LastPlayFolder = 0;
      break;
    default:
      break;
  }
}

void readMp3Replies() {
  while (MP3_PORT.available() > 0) {
    uint8_t b = MP3_PORT.read();
    if (b == 0x7E) {
      mp3RxIndex = 0;
    }
    if (mp3RxIndex >= sizeof(mp3RxFrame)) {
      mp3RxIndex = 0;
      continue;
    }
    mp3RxFrame[mp3RxIndex++] = b;
    if (b == 0xEF && mp3RxFrame[0] == 0x7E && (mp3RxIndex == 8 || mp3RxIndex == 10)) {
      handleMp3Reply(mp3RxFrame[3], mp3RxFrame[5], mp3RxFrame[6]);
      mp3RxIndex = 0;
    }
  }
}

// Probes a few times after boot; a module that powers up later is still detected by its own init or status message.
void serviceMp3(unsigned long now) {
  readMp3Replies();
  if (!mp3Present && mp3ProbesSent < MP3_PROBE_COUNT &&
      now >= MP3_FIRST_PROBE_MS + mp3ProbesSent * MP3_PROBE_INTERVAL_MS) {
    ++mp3ProbesSent;
    mp3Send(MP3_CMD_QUERY_STATUS, 0);
  }
  if (!mp3Present && !mp3AbsentReported &&
      now >= MP3_FIRST_PROBE_MS + MP3_PROBE_COUNT * MP3_PROBE_INTERVAL_MS) {
    mp3AbsentReported = true;
    Serial.println(F("DFPlayer: none detected (sounds off)"));
  }
  if (mp3QueueCount > 0 && now - mp3LastSendAt >= MP3_CMD_GAP_MS) {
    Mp3Command next = mp3Queue[mp3QueueHead];
    mp3QueueHead = (mp3QueueHead + 1) % MP3_QUEUE_SIZE;
    --mp3QueueCount;
    mp3Send(next.command, next.data);
  }
}

uint8_t readDurationPins() {
  return (digitalRead(DURATION_PIN_B) == LOW ? 2 : 0) | (digitalRead(DURATION_PIN_A) == LOW ? 1 : 0);
}

uint32_t mainDurationMs() {
  return durationIndex == 0 ? config.mainMs : JUMPER_DURATIONS_MS[durationIndex];
}

void printDurationSelect() {
  Serial.print(mainDurationMs());
  if (durationIndex == 0) {
    Serial.print(F(" ms (no jumper: saved main_ms)"));
    return;
  }
  Serial.print(F(" ms (jumper "));
  Serial.print(durationIndex == 1 ? F("D8") : durationIndex == 2 ? F("D9") : F("D8+D9"));
  Serial.print(')');
}

// Two matching reads in a row are required, so a jumper being moved does not flicker through other settings.
void pollDurationSelect(unsigned long now) {
  if (now - lastDurationPollAt < DURATION_POLL_MS) {
    return;
  }
  lastDurationPollAt = now;
  uint8_t reading = readDurationPins();
  if (reading != durationCandidate) {
    durationCandidate = reading;
    return;
  }
  if (reading != durationIndex) {
    durationIndex = reading;
    Serial.print(F("Duration select changed: "));
    printDurationSelect();
    Serial.println(F("; applies to the next trigger"));
  }
}

void pollAuxInput(unsigned long now) {
  bool rawLow = digitalRead(AUX_INPUT_PIN) == LOW;
  if (rawLow != auxLastRawLow) {
    auxLastRawLow = rawLow;
    auxLastChangeAt = now;
    return;
  }
  if (rawLow == auxStableLow || now - auxLastChangeAt < AUX_DEBOUNCE_MS) {
    return;
  }
  auxStableLow = rawLow;
  if (!rawLow) {
    return;  // released: re-arms the input
  }
  Serial.print(F("Aux D10 LOW"));
  if (isReady(auxPulse, now)) {
    ++auxFired;
    startPulse(auxPulse, config.auxMs, now);
    Serial.print(F(" -> TRIGGER: D11 LOW for "));
    Serial.print(config.auxMs);
    Serial.println(F(" ms"));
    mp3Play(MP3_OTHER_FOLDER, MP3_AUX_FILE);
  } else {
    ++auxIgnored;
    Serial.println(auxPulse.active ? F(" -> ignored (D11 active)") : F(" -> ignored (re-arm)"));
  }
}

// Finishes the caller's log line. Returns true if the main outputs fired.
bool fireMain(unsigned long now) {
  if (!isReady(mainPulse, now)) {
    Serial.println(mainPulse.active ? F(" -> ignored (trigger active)") : F(" -> ignored (re-arm)"));
    return false;
  }
  uint32_t lengthMs = mainDurationMs();
  startPulse(mainPulse, lengthMs, now);
  Serial.print(F(" -> TRIGGER: D4 LOW + D5 HIGH for "));
  Serial.print(lengthMs);
  Serial.println(F(" ms"));
  return true;
}

bool readWiredActive() {
  int level = digitalRead(WIRED_INPUT_PIN);
  return config.wiredInput == WIRED_NEGATIVE ? level == LOW : level == HIGH;
}

// An input already active when it is enabled (or at boot) must release before it can fire.
void applyWiredInputMode() {
  pinMode(WIRED_INPUT_PIN, config.wiredInput == WIRED_POSITIVE ? INPUT : INPUT_PULLUP);
  delay(2);
  wiredStableActive = wiredLastRawActive = config.wiredInput != WIRED_OFF && readWiredActive();
  wiredLastChangeAt = millis();
}

const __FlashStringHelper *wiredModeName() {
  if (config.wiredInput == WIRED_NEGATIVE) {
    return F("negative (LOW to fire)");
  }
  if (config.wiredInput == WIRED_POSITIVE) {
    return F("positive (HIGH to fire)");
  }
  return F("off");
}

void pollWiredInput(unsigned long now) {
  if (config.wiredInput == WIRED_OFF) {
    return;
  }
  bool raw = readWiredActive();
  if (raw != wiredLastRawActive) {
    wiredLastRawActive = raw;
    wiredLastChangeAt = now;
    return;
  }
  if (raw == wiredStableActive || now - wiredLastChangeAt < WIRED_DEBOUNCE_MS) {
    return;
  }
  wiredStableActive = raw;
  if (!raw) {
    return;  // released: re-arms the input
  }
  Serial.print(config.wiredInput == WIRED_NEGATIVE ? F("Wired D2 LOW") : F("Wired D2 HIGH"));
  if (fireMain(now)) {
    ++wiredFired;
    mp3Play(MP3_OTHER_FOLDER, MP3_WIRED_FILE);
  } else {
    ++wiredIgnored;
  }
}

void handleIrCode(const __FlashStringHelper *format, bool enabled, uint32_t code, uint8_t bits, uint8_t frames) {
  unsigned long now = millis();
  Serial.print(F("IR "));
  Serial.print(format);
  Serial.print(F(" code "));
  printHex(code, bits >= 32 ? 8 : (bits + 3) / 4);
  Serial.print(F(" ("));
  Serial.print(bits);
  Serial.print(F(" bits"));
  if (bits > 32) {
    Serial.print(F(", first 32 shown"));
  }
  if (frames > 1) {
    Serial.print(F(", "));
    Serial.print(frames);
    Serial.print(F(" frames"));
  }
  Serial.print(')');

  if (!enabled) {
    ++irIgnored;
    Serial.println(F(" -> ignored (format disabled in ir_formats)"));
  } else if (fireMain(now)) {
    ++irFired;
    playIrSound(now);
  } else {
    ++irIgnored;
  }
}

bool within(uint16_t value, uint16_t low, uint16_t high) {
  return value >= low && value <= high;
}

// NEC, Samsung, Panasonic, and most LED-strip remotes. Bits are stored LSB first (NEC order); bits past 32 are dropped.
bool decodePulseDistance(uint32_t &code, uint8_t &bits) {
  if (!within(durations[0], 2500, 11000) || !within(durations[1], 1300, 5500)) {
    return false;
  }
  code = 0;
  bits = 0;
  for (uint8_t i = 2; i < durationCount; i += 2) {
    if (!within(durations[i], 250, 900)) {
      return false;
    }
    if (i + 1 >= durationCount || durations[i + 1] >= 3000) {
      return bits >= PD_MIN_BITS;  // that mark was the stop bit
    }
    uint16_t space = durations[i + 1];
    bool one;
    if (within(space, 250, 900)) {
      one = false;
    } else if (within(space, 1100, 2100)) {
      one = true;
    } else {
      return false;
    }
    if (one && bits < 32) {
      code |= 1UL << bits;
    }
    if (++bits > PD_MAX_BITS) {
      return false;
    }
  }
  return false;
}

const __FlashStringHelper *pulseDistanceName(uint8_t bits) {
  if (bits == 32 && durations[0] >= 7000) {
    return F("NEC");
  }
  if (bits == 32) {
    return F("Samsung");
  }
  if (bits == 48) {
    return F("Panasonic");
  }
  return F("pulse-distance");
}

// Parses one pulse-width frame whose leader mark is at durations[start]. Returns the index after the frame, or 0 if
// there is no valid frame there. Bits are stored MSB first (first bit received = highest bit).
uint8_t parsePulseWidthFrame(uint8_t start, uint32_t &code, uint8_t &bits) {
  if (start + 1 >= durationCount || !within(durations[start], 1800, 8000) || !within(durations[start + 1], 250, 1000)) {
    return 0;
  }
  code = 0;
  bits = 0;
  for (uint8_t i = start + 2; i < durationCount; i += 2) {
    uint16_t mark = durations[i];
    if (within(mark, 1000, 2000)) {
      code = (code << 1) | 1;
    } else if (within(mark, 250, 900)) {
      code <<= 1;
    } else {
      return 0;
    }
    if (++bits > PW_MAX_BITS) {
      return 0;
    }
    if (i + 1 >= durationCount || durations[i + 1] >= 3000) {
      return bits >= PW_MIN_BITS ? i + 2 : 0;
    }
    if (!within(durations[i + 1], 250, 1900)) {
      return 0;
    }
  }
  return 0;
}

// A gun shot repeats its frame several times in one capture. Any frame start is accepted, so a capture whose first
// frame was clipped (long range, glancing hit) still decodes from a later frame.
bool decodePulseWidth(uint32_t &code, uint8_t &bits, uint8_t &frames, uint16_t &leaderMark) {
  frames = 0;
  uint32_t frameCode = 0;
  uint8_t frameBits = 0;
  for (uint8_t i = 0; i < durationCount; i += 2) {
    uint8_t next = parsePulseWidthFrame(i, frameCode, frameBits);
    if (next == 0) {
      continue;
    }
    if (frames == 0) {
      code = frameCode;
      bits = frameBits;
      leaderMark = durations[i];
    }
    if (frameCode == code && frameBits == bits) {
      ++frames;
    }
    i = next - 2;
  }
  return frames >= config.pwMinFrames;
}

void printUnknownFrame() {
  Serial.print(F("IR ignored: not a valid code (phone, sunlight, or unsupported remote), "));
  Serial.print(durationCount);
  Serial.print(F(" marks/spaces, first mark "));
  Serial.print(durations[0]);
  Serial.print(F(" us, first space "));
  Serial.print(durations[1]);
  Serial.print(F(" us"));
  if (frameOverflow) {
    Serial.print(F(" (truncated)"));
  }
  Serial.println();
  if (config.printTimings) {
    Serial.print(F("  timings us (mark,space,...):"));
    for (uint8_t i = 0; i < durationCount; ++i) {
      Serial.print(i == 0 ? ' ' : ',');
      Serial.print(durations[i]);
    }
    Serial.println();
  }
}

void decodeFrame() {
  if (durationCount < 3) {
    return;
  }

  // NEC repeat: 9 ms mark + 2.25 ms space + stop mark.
  if (durationCount <= 4 && within(durations[0], 7000, 11000) && within(durations[1], 1700, 2800)) {
    if (!lastFrameWasRepeat) {
      Serial.println(F("IR NEC repeat (button held) -> ignored"));
    }
    lastFrameWasRepeat = true;
    return;
  }
  lastFrameWasRepeat = false;

  uint32_t code = 0;
  uint8_t bits = 0;
  if (decodePulseDistance(code, bits)) {
    handleIrCode(pulseDistanceName(bits), config.irFormats & IR_FORMAT_PULSE_DISTANCE, code, bits, 1);
    return;
  }

  uint8_t frames = 0;
  uint16_t leaderMark = 0;
  if (decodePulseWidth(code, bits, frames, leaderMark)) {
    handleIrCode(leaderMark < 3500 ? F("Sony/pulse-width") : F("laser tag"), config.irFormats & IR_FORMAT_PULSE_WIDTH, code,
                 bits, frames);
    return;
  }

  if (durationCount >= MIN_REPORTED_DURATIONS) {
    printUnknownFrame();
  }
}

// Polled because D3 on the Arduino 101 has no both-edge interrupt; the AVR boards use the same code for identical timing.
void pollIrReceiver() {
  int level = digitalRead(IR_RECEIVER_PIN);
  unsigned long nowUs = micros();

  if (level != lastLevel) {
    if (level == LOW && !frameInProgress) {
      frameInProgress = true;
      durationCount = 0;
      frameOverflow = false;
    } else if (frameInProgress) {
      uint32_t elapsed = nowUs - lastEdgeAt;
      if (durationCount < MAX_DURATIONS) {
        durations[durationCount++] = elapsed > 0xFFFF ? 0xFFFF : elapsed;
      } else {
        frameOverflow = true;
      }
    }
    lastLevel = level;
    lastEdgeAt = nowUs;
    return;
  }

  if (frameInProgress && level == HIGH && nowUs - lastEdgeAt >= FRAME_END_GAP_US) {
    frameInProgress = false;
    decodeFrame();
  }
}

void updateIrLed(unsigned long now) {
  if (lastLevel == LOW) {
    irActivitySeen = true;
    lastIrActivityAt = now;
  }
  digitalWrite(IR_LED_PIN, irActivitySeen && now - lastIrActivityAt < IR_LED_HOLD_MS ? HIGH : LOW);
}

uint8_t configChecksum(const Config &c) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&c);
  uint8_t sum = 0xA5;
  for (size_t i = 0; i < offsetof(Config, checksum); ++i) {
    sum += bytes[i];
  }
  return sum;
}

void setDefaults() {
  memset(&config, 0, sizeof(config));  // zeroed padding keeps the checksum stable
  config.magic = CONFIG_MAGIC;
  config.version = CONFIG_VERSION;
  strncpy(config.name, "unnamed", NAME_MAX);
  config.mainMs = 500;
  config.auxMs = 500;
  config.rearmMs = 250;
  config.pwMinFrames = 1;
  config.irFormats = IR_FORMAT_PULSE_WIDTH | IR_FORMAT_PULSE_DISTANCE;
  config.printTimings = 0;
  config.wiredInput = WIRED_OFF;
  config.mp3Volume = 25;
  config.mp3ResetS = 120;
}

bool configValid(const Config &c) {
  return c.magic == CONFIG_MAGIC && c.version == CONFIG_VERSION && c.checksum == configChecksum(c) &&
         c.name[NAME_MAX] == '\0' && c.mainMs >= 100 && c.mainMs <= 15000 && c.auxMs >= 100 && c.auxMs <= 15000 &&
         c.rearmMs <= 5000 && c.pwMinFrames >= 1 && c.pwMinFrames <= 3 && c.irFormats >= 1 && c.irFormats <= 3 &&
         c.printTimings <= 1 && c.wiredInput <= WIRED_POSITIVE && c.mp3Volume <= 30 && c.mp3ResetS <= 3600;
}

bool loadConfig() {
  EEPROM.get(0, config);
  if (configValid(config)) {
    return true;
  }
  setDefaults();
  return false;
}

void saveConfig() {
  config.checksum = configChecksum(config);
  EEPROM.put(0, config);
  configDirty = false;
}

void printCfg(const __FlashStringHelper *key, uint32_t value) {
  Serial.print(F("CFG "));
  Serial.print(key);
  Serial.print('=');
  Serial.println(value);
}

// Machine-readable settings for tools/web-config; the block always ends with "CFG end".
void printConfig() {
  Serial.println(F("CFG fw=" FIRMWARE_VERSION));
  Serial.println(F("CFG board=" BOARD_NAME));
  Serial.print(F("CFG name="));
  Serial.println(config.name);
  printCfg(F("main_ms"), config.mainMs);
  printCfg(F("aux_ms"), config.auxMs);
  printCfg(F("rearm_ms"), config.rearmMs);
  printCfg(F("pw_min_frames"), config.pwMinFrames);
  printCfg(F("ir_formats"), config.irFormats);
  printCfg(F("timings"), config.printTimings);
  printCfg(F("wired_input"), config.wiredInput);
  printCfg(F("mp3_volume"), config.mp3Volume);
  printCfg(F("mp3_reset_s"), config.mp3ResetS);
  printCfg(F("mp3"), mp3Present);
  Serial.print(F("CFG mp3_files="));
  if (mp3IrFileCount < 0) {
    Serial.println('?');
  } else {
    Serial.println(mp3IrFileCount);
  }
  printCfg(F("mp3_next"), mp3NextIrFile);
  printCfg(F("jumper"), durationIndex);
  printCfg(F("active_ms"), mainDurationMs());
  printCfg(F("unsaved"), configDirty);
  Serial.println(F("CFG end"));
}

void printHelp() {
  Serial.println(F("Commands (115200 baud, one per line):"));
  Serial.println(F("  GET                     show settings"));
  Serial.println(F("  SET name=<1-16 chars>   prop name shown in logs"));
  Serial.println(F("  SET main_ms=<100-15000> main pulse when no D8/D9 jumper is installed"));
  Serial.println(F("  SET aux_ms=<100-15000>  D11 pulse"));
  Serial.println(F("  SET rearm_ms=<0-5000>   idle gap after each pulse"));
  Serial.println(F("  SET pw_min_frames=<1-3> matching laser tag frames needed per shot"));
  Serial.println(F("  SET ir_formats=<1-3>    1 laser tag/Sony, 2 NEC/Samsung/LED remotes, 3 both"));
  Serial.println(F("  SET timings=<0|1>       print raw timings of unrecognized IR"));
  Serial.println(F("  SET wired_input=<0-2>   D2: 0 off, 1 negative (to GND), 2 positive (needs 10k pull-down)"));
  Serial.println(F("  SET mp3_volume=<0-30>   MP3 player volume"));
  Serial.println(F("  SET mp3_reset_s=<0-3600> idle seconds before the IR sound sequence restarts at 01/001"));
  Serial.println(F("  SAVE                    store settings in EEPROM"));
  Serial.println(F("  DEFAULTS                load defaults (SAVE to keep)"));
  Serial.println(F("  TEST MAIN | TEST AUX    fire an output once"));
}

void handleSet(char *arg) {
  char *eq = strchr(arg, '=');
  if (eq == nullptr) {
    Serial.println(F("ERR usage: SET key=value"));
    return;
  }
  *eq = '\0';
  char *value = eq + 1;
  for (char *p = arg; *p; ++p) {
    *p = tolower(*p);
  }

  if (strcmp(arg, "name") == 0) {
    size_t length = strlen(value);
    if (length == 0 || length > NAME_MAX) {
      Serial.println(F("ERR name must be 1-16 characters"));
      return;
    }
    for (size_t i = 0; i < length; ++i) {
      if (value[i] < 0x20 || value[i] > 0x7E) {
        Serial.println(F("ERR name must be printable ASCII"));
        return;
      }
    }
    memset(config.name, 0, sizeof(config.name));
    memcpy(config.name, value, length);
  } else {
    char *end = nullptr;
    unsigned long n = strtoul(value, &end, 10);
    if (end == value || *end != '\0') {
      Serial.println(F("ERR value must be a whole number"));
      return;
    }
    if (strcmp(arg, "main_ms") == 0 && n >= 100 && n <= 15000) {
      config.mainMs = n;
    } else if (strcmp(arg, "aux_ms") == 0 && n >= 100 && n <= 15000) {
      config.auxMs = n;
    } else if (strcmp(arg, "rearm_ms") == 0 && n <= 5000) {
      config.rearmMs = n;
    } else if (strcmp(arg, "pw_min_frames") == 0 && n >= 1 && n <= 3) {
      config.pwMinFrames = n;
    } else if (strcmp(arg, "ir_formats") == 0 && n >= 1 && n <= 3) {
      config.irFormats = n;
    } else if (strcmp(arg, "timings") == 0 && n <= 1) {
      config.printTimings = n;
    } else if (strcmp(arg, "wired_input") == 0 && n <= WIRED_POSITIVE) {
      config.wiredInput = n;
      applyWiredInputMode();
    } else if (strcmp(arg, "mp3_volume") == 0 && n <= 30) {
      config.mp3Volume = n;
      if (mp3Present) {
        mp3Enqueue(MP3_CMD_SET_VOLUME, config.mp3Volume);
      }
    } else if (strcmp(arg, "mp3_reset_s") == 0 && n <= 3600) {
      config.mp3ResetS = n;
    } else {
      Serial.println(F("ERR unknown key or value out of range (type HELP)"));
      return;
    }
  }
  configDirty = true;
  Serial.print(F("OK "));
  Serial.print(arg);
  Serial.print('=');
  Serial.println(value);
}

void testOutput(bool aux) {
  unsigned long now = millis();
  PulseOutput &pulse = aux ? auxPulse : mainPulse;
  if (!isReady(pulse, now)) {
    Serial.println(F("ERR output busy"));
    return;
  }
  uint32_t lengthMs = aux ? config.auxMs : mainDurationMs();
  startPulse(pulse, lengthMs, now);
  Serial.print(aux ? F("OK test: D11 LOW for ") : F("OK test: D4 LOW + D5 HIGH for "));
  Serial.print(lengthMs);
  Serial.println(F(" ms"));
  if (aux) {
    mp3Play(MP3_OTHER_FOLDER, MP3_AUX_FILE);
  } else {
    playIrSound(now);
  }
}

// Every command except HELP ends with exactly one "OK ...", "ERR ...", or "CFG end" line.
void handleCommand(char *line) {
  while (*line == ' ') {
    ++line;
  }
  char *arg = strchr(line, ' ');
  if (arg != nullptr) {
    *arg++ = '\0';
    while (*arg == ' ') {
      ++arg;
    }
  } else {
    arg = line + strlen(line);
  }
  for (char *p = line; *p; ++p) {
    *p = toupper(*p);
  }

  if (strcmp(line, "GET") == 0) {
    printConfig();
  } else if (strcmp(line, "SET") == 0) {
    handleSet(arg);
  } else if (strcmp(line, "SAVE") == 0) {
    saveConfig();
    Serial.println(F("OK saved"));
  } else if (strcmp(line, "DEFAULTS") == 0) {
    setDefaults();
    applyWiredInputMode();
    if (mp3Present) {
      mp3Enqueue(MP3_CMD_SET_VOLUME, config.mp3Volume);
    }
    configDirty = true;
    Serial.println(F("OK defaults loaded (SAVE to keep)"));
  } else if (strcmp(line, "TEST") == 0 && (arg[0] == 'm' || arg[0] == 'M')) {
    testOutput(false);
  } else if (strcmp(line, "TEST") == 0 && (arg[0] == 'a' || arg[0] == 'A')) {
    testOutput(true);
  } else if (strcmp(line, "HELP") == 0) {
    printHelp();
  } else if (line[0] != '\0') {
    Serial.println(F("ERR unknown command (type HELP)"));
  }
}

void pollSerialCommands() {
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\r' || c == '\n') {
      if (cmdOverflow) {
        Serial.println(F("ERR command too long"));
      } else if (cmdLength > 0) {
        cmdBuffer[cmdLength] = '\0';
        handleCommand(cmdBuffer);
      }
      cmdLength = 0;
      cmdOverflow = false;
    } else if (cmdLength < CMD_MAX) {
      cmdBuffer[cmdLength++] = c;
    } else {
      cmdOverflow = true;
    }
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat ["));
  Serial.print(config.name);
  Serial.print(F("]: main "));
  Serial.print(mainPulse.active ? F("ACTIVE") : F("idle"));
  Serial.print(F(", duration "));
  printDurationSelect();
  Serial.print(F(", aux "));
  Serial.print(auxPulse.active ? F("ACTIVE") : F("idle"));
  Serial.print(F(", IR fired "));
  Serial.print(irFired);
  Serial.print(F(" ignored "));
  Serial.print(irIgnored);
  if (config.wiredInput != WIRED_OFF) {
    Serial.print(F(", wired D2 "));
    Serial.print(wiredStableActive ? F("ACTIVE") : F("idle"));
    Serial.print(F(" fired "));
    Serial.print(wiredFired);
    Serial.print(F(" ignored "));
    Serial.print(wiredIgnored);
  }
  if (mp3Present) {
    Serial.print(F(", mp3 next "));
    printMp3File(MP3_IR_FOLDER, mp3NextIrFile);
  } else {
    Serial.print(F(", mp3 none"));
  }
  Serial.print(F(", aux fired "));
  Serial.print(auxFired);
  Serial.print(F(" ignored "));
  Serial.println(auxIgnored);
}

void setup() {
  Serial.begin(115200);
  MP3_PORT.begin(9600);
  bool configLoaded = loadConfig();

  applyOutputs();
  pinMode(POSITIVE_TRIGGER_PIN, OUTPUT);
  pinMode(RELAY_LED_PIN, OUTPUT);
  digitalWrite(IR_LED_PIN, LOW);
  pinMode(IR_LED_PIN, OUTPUT);
  pinMode(IR_RECEIVER_PIN, INPUT_PULLUP);
  pinMode(DURATION_PIN_A, INPUT_PULLUP);
  pinMode(DURATION_PIN_B, INPUT_PULLUP);
  pinMode(AUX_INPUT_PIN, INPUT_PULLUP);
  delay(5);  // let the pull-ups settle before the first reads

  durationIndex = durationCandidate = readDurationPins();
  // An aux input already LOW at boot must release before it can fire.
  auxStableLow = auxLastRawLow = digitalRead(AUX_INPUT_PIN) == LOW;
  auxLastChangeAt = millis();
  applyWiredInputMode();
  lastLevel = digitalRead(IR_RECEIVER_PIN);
  lastEdgeAt = micros();

  Serial.println(F(BOARD_NAME " laser tag IR trigger ready (fw " FIRMWARE_VERSION ")"));
  Serial.print(F("Name: "));
  Serial.print(config.name);
  Serial.println(configLoaded ? F(" (settings loaded from EEPROM)") : F(" (no saved settings; using defaults)"));
  Serial.println(F("Main: valid IR code on D3 (laser tag, TV, LED remotes) -> D4 LOW + D5 HIGH; phones/sunlight ignored"));
  Serial.print(F("Duration: "));
  printDurationSelect();
  Serial.println(F("; jumpers D8=5 s, D9=10 s, both=15 s, read live"));
  Serial.print(F("Wired: D2 "));
  Serial.print(wiredModeName());
  Serial.println(F(" -> same main outputs and duration"));
  Serial.print(F("Aux: D10 LOW edge -> D11 LOW for "));
  Serial.print(config.auxMs);
  Serial.println(F(" ms"));
  Serial.print(F("Re-arm: "));
  Serial.print(config.rearmMs);
  Serial.println(F(" ms after each pulse; triggers during a pulse are ignored"));
  Serial.println(F("DFPlayer: " MP3_PINS " (1k in series on module RX), auto-detect; IR -> 01/next, D2 -> 02/001, D10 -> 02/002"));
  Serial.println(F("Status LEDs: relay D6, IR D7. Type HELP for settings commands."));
  printHeartbeat();
  lastHeartbeatAt = millis();
}

void loop() {
  pollIrReceiver();

  unsigned long now = millis();
  updateIrLed(now);

  if (endPulseIfDue(mainPulse, now)) {
    Serial.println(F("Main trigger released (D4/D5 idle)"));
  }
  if (endPulseIfDue(auxPulse, now)) {
    Serial.println(F("Aux trigger released (D11 idle)"));
  }

  // Slow inputs and serial output wait until a frame finishes so they never delay IR edge capture.
  if (frameInProgress) {
    return;
  }
  pollAuxInput(now);
  pollWiredInput(now);
  pollDurationSelect(now);
  serviceMp3(now);
  pollSerialCommands();
  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
