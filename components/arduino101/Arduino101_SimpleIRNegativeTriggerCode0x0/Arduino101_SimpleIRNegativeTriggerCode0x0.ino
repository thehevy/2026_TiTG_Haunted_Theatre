#include <Arduino.h>

// 433 MHz RF code trigger. D3 is an ARC GPIO on the Arduino 101 and has no both-edge (CHANGE) interrupt, so
// rc-switch's interrupt receiver cannot run on it. This sketch polls D3 and decodes with rc-switch's protocol
// table and rules, so printed codes match what rc-switch reports. Unlike rc-switch, it can also report code 0.
constexpr uint8_t RF_RECEIVER_PIN = 3;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 4;
constexpr uint8_t RF_LED_PIN = 5;
constexpr uint8_t RELAY_LED_PIN = 6;
constexpr uint32_t TRIGGER_PULSE_MS = 500;
constexpr uint32_t TRIGGER_CODE = 0x0;

constexpr uint8_t MIN_CODE_BITS = 12;      // shorter decodes are treated as receiver noise
constexpr uint8_t CONFIRM_COUNT = 2;       // identical decodes needed before a code is printed and acted on
constexpr uint32_t CODE_RELEASE_MS = 300;  // a code must stop this long before it counts as a new button press
constexpr uint32_t HEARTBEAT_MS = 5000;
constexpr uint32_t RF_LED_HOLD_MS = 150;

struct PulseRatio {
  uint8_t high;
  uint8_t low;
};

struct RfProtocol {
  uint16_t pulseLengthUs;
  PulseRatio sync;
  PulseRatio zero;
  PulseRatio one;
  bool inverted;
};

// Protocols 1-12 from rc-switch 2.6.4.
const RfProtocol RF_PROTOCOLS[] = {
  {350, {1, 31}, {1, 3}, {3, 1}, false},
  {650, {1, 10}, {1, 2}, {2, 1}, false},
  {100, {30, 71}, {4, 11}, {9, 6}, false},
  {380, {1, 6}, {1, 3}, {3, 1}, false},
  {500, {6, 14}, {1, 2}, {2, 1}, false},
  {450, {23, 1}, {1, 2}, {2, 1}, true},
  {150, {2, 62}, {1, 6}, {6, 1}, false},
  {200, {3, 130}, {7, 16}, {3, 16}, false},
  {200, {130, 7}, {16, 7}, {16, 3}, true},
  {365, {18, 1}, {3, 1}, {1, 3}, true},
  {270, {36, 1}, {1, 2}, {2, 1}, true},
  {320, {36, 1}, {1, 2}, {2, 1}, true},
};
constexpr uint8_t RF_PROTOCOL_COUNT = sizeof(RF_PROTOCOLS) / sizeof(RF_PROTOCOLS[0]);
constexpr uint32_t RF_RECEIVE_TOLERANCE_PERCENT = 60;
constexpr uint32_t RF_SEPARATION_LIMIT_US = 4300;
constexpr uint8_t RF_MAX_CHANGES = 67;

struct RfCode {
  uint32_t value;
  uint8_t bits;
  uint8_t protocol;
  uint32_t pulseUs;
};

uint32_t rfTimings[RF_MAX_CHANGES];
uint8_t rfChangeCount = 0;
uint8_t rfRepeatCount = 0;
unsigned long rfLastEdgeAt = 0;
int rfLastLevel = LOW;
bool rfCodeReady = false;
RfCode rfDecoded = {0, 0, 0, 0};

bool hasLastCode = false;
RfCode lastCode = {0, 0, 0, 0};
unsigned long lastCodeAt = 0;
uint8_t matchCount = 0;
unsigned long codesReceived = 0;
unsigned long rfLedOnAt = 0;
bool rfLedLit = false;

bool triggerActive = false;
unsigned long triggerStartedAt = 0;
unsigned long lastHeartbeatAt = 0;

uint32_t timingDiff(uint32_t a, uint32_t b) {
  return a > b ? a - b : b - a;
}

bool decodeProtocol(uint8_t index, uint8_t changeCount, RfCode &out) {
  const RfProtocol &p = RF_PROTOCOLS[index];
  uint32_t syncPulses = p.sync.low > p.sync.high ? p.sync.low : p.sync.high;
  uint32_t pulseUs = rfTimings[0] / syncPulses;
  uint32_t tolerance = pulseUs * RF_RECEIVE_TOLERANCE_PERCENT / 100;
  uint32_t value = 0;

  for (uint8_t i = p.inverted ? 2 : 1; i + 1 < changeCount; i += 2) {
    value <<= 1;
    if (timingDiff(rfTimings[i], pulseUs * p.zero.high) < tolerance &&
        timingDiff(rfTimings[i + 1], pulseUs * p.zero.low) < tolerance) {
      continue;
    }
    if (timingDiff(rfTimings[i], pulseUs * p.one.high) < tolerance &&
        timingDiff(rfTimings[i + 1], pulseUs * p.one.low) < tolerance) {
      value |= 1;
      continue;
    }
    return false;
  }

  if (changeCount <= 7) {
    return false;
  }
  out.value = value;
  out.bits = (changeCount - 1) / 2;
  out.protocol = index + 1;
  out.pulseUs = pulseUs;
  return true;
}

// Same framing as rc-switch: a long gap that repeats marks the frame boundary; decode on every second gap.
void handleRfEdge(unsigned long nowUs) {
  uint32_t duration = nowUs - rfLastEdgeAt;

  if (duration > RF_SEPARATION_LIMIT_US) {
    if (rfRepeatCount == 0 || timingDiff(duration, rfTimings[0]) < 200) {
      if (++rfRepeatCount == 2) {
        RfCode code;
        for (uint8_t i = 0; i < RF_PROTOCOL_COUNT; ++i) {
          if (decodeProtocol(i, rfChangeCount, code)) {
            if (code.bits >= MIN_CODE_BITS) {
              rfDecoded = code;
              rfCodeReady = true;
            }
            break;
          }
        }
        rfRepeatCount = 0;
      }
    }
    rfChangeCount = 0;
  }

  if (rfChangeCount >= RF_MAX_CHANGES) {
    rfChangeCount = 0;
    rfRepeatCount = 0;
  }
  rfTimings[rfChangeCount++] = duration;
  rfLastEdgeAt = nowUs;
}

void pollRfReceiver() {
  int level = digitalRead(RF_RECEIVER_PIN);
  if (level != rfLastLevel) {
    rfLastLevel = level;
    handleRfEdge(micros());
  }
}

// Idle is high-Z so the 5 V relay module pulls IN fully HIGH; a 3.3 V HIGH can leave it partly on.
void setTrigger(bool active) {
  if (active) {
    pinMode(NEGATIVE_TRIGGER_PIN, OUTPUT);
    digitalWrite(NEGATIVE_TRIGGER_PIN, LOW);
  } else {
    pinMode(NEGATIVE_TRIGGER_PIN, INPUT);
  }
  digitalWrite(RELAY_LED_PIN, active ? HIGH : LOW);
  triggerActive = active;
  if (active) {
    triggerStartedAt = millis();
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: RF input D"));
  Serial.print(RF_RECEIVER_PIN);
  Serial.print(F(", negative trigger output D"));
  Serial.print(NEGATIVE_TRIGGER_PIN);
  Serial.print(F(", trigger code 0x"));
  Serial.print(TRIGGER_CODE, HEX);
  Serial.print(F(", codes received "));
  Serial.println(codesReceived);
}

void printCode(const RfCode &code) {
  Serial.print(F("RF code 0x"));
  Serial.print(code.value, HEX);
  Serial.print(F(" ("));
  Serial.print(code.value);
  Serial.print(F(") | "));
  Serial.print(code.bits);
  Serial.print(F(" bits | protocol "));
  Serial.print(code.protocol);
  Serial.print(F(" | pulse "));
  Serial.print(code.pulseUs);
  Serial.print(F(" us"));
}

bool sameCode(const RfCode &a, const RfCode &b) {
  return a.value == b.value && a.bits == b.bits && a.protocol == b.protocol;
}

// Acts once per button press: after CONFIRM_COUNT identical decodes, then not again until the code stops.
void handleDecodedCode(unsigned long now) {
  if (!rfCodeReady) {
    return;
  }
  RfCode code = rfDecoded;
  rfCodeReady = false;

  bool samePress = hasLastCode && sameCode(code, lastCode) && now - lastCodeAt < CODE_RELEASE_MS;
  matchCount = samePress ? (matchCount < 255 ? matchCount + 1 : matchCount) : 1;
  hasLastCode = true;
  lastCode = code;
  lastCodeAt = now;
  rfLedOnAt = now;
  rfLedLit = true;

  if (matchCount != CONFIRM_COUNT) {
    return;
  }

  ++codesReceived;
  printCode(code);
  if (code.value == TRIGGER_CODE) {
    Serial.print(F(" -> TRIGGER: negative trigger LOW for "));
    Serial.print(TRIGGER_PULSE_MS);
    Serial.println(F(" ms"));
    setTrigger(true);
  } else {
    Serial.print(F(" -> ignored (trigger code is 0x"));
    Serial.print(TRIGGER_CODE, HEX);
    Serial.println(F(")"));
  }
}

void updateRfLed(unsigned long now) {
  if (rfLedLit && now - rfLedOnAt >= RF_LED_HOLD_MS) {
    rfLedLit = false;
  }
  digitalWrite(RF_LED_PIN, rfLedLit ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(NEGATIVE_TRIGGER_PIN, INPUT);
  pinMode(RF_RECEIVER_PIN, INPUT);
  digitalWrite(RF_LED_PIN, LOW);
  pinMode(RF_LED_PIN, OUTPUT);
  digitalWrite(RELAY_LED_PIN, LOW);
  pinMode(RELAY_LED_PIN, OUTPUT);

  rfLastLevel = digitalRead(RF_RECEIVER_PIN);
  rfLastEdgeAt = micros();

  Serial.println(F("Arduino 101 RF code negative trigger ready"));
  Serial.print(F("Trigger code: 0x"));
  Serial.print(TRIGGER_CODE, HEX);
  Serial.print(F(" -> negative trigger LOW for "));
  Serial.print(TRIGGER_PULSE_MS);
  Serial.println(F(" ms"));
  Serial.print(F("RF decoder: polling D"));
  Serial.print(RF_RECEIVER_PIN);
  Serial.print(F(", rc-switch protocols 1-12, min "));
  Serial.print(MIN_CODE_BITS);
  Serial.print(F(" bits, "));
  Serial.print(CONFIRM_COUNT);
  Serial.println(F(" matching frames per press"));
  Serial.print(F("Status LEDs: RF code D"));
  Serial.print(RF_LED_PIN);
  Serial.print(F(", relay D"));
  Serial.println(RELAY_LED_PIN);
  printHeartbeat();
  lastHeartbeatAt = millis();
}

void loop() {
  pollRfReceiver();

  unsigned long now = millis();
  handleDecodedCode(now);
  updateRfLed(now);

  if (triggerActive && now - triggerStartedAt >= TRIGGER_PULSE_MS) {
    setTrigger(false);
  }

  if (now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
