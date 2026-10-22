#include <Arduino.h>

// Laser tag IR dual trigger: any shot from the Sharper Image laser tag gun pulls the negative trigger output (D4) LOW
// and drives the positive trigger output (D5) HIGH, both for TRIGGER_PULSE_MS.
// IRremote has no Curie support, so this sketch decodes frames itself by timing the IR receiver output
// (idle HIGH, LOW while the IR carrier is received). Only the gun's frame shape triggers, so phones (Face ID /
// proximity IR), TV remotes, and sunlight cannot fire the relay.
// - Laser tag gun: 6 ms leader mark + 0.5 ms space, then 9 bits where a ~1.5 ms mark is 1 and a ~0.5 ms mark is 0
//   (about 2 ms per bit), first bit = most significant. Each shot repeats the frame about 5 times. The low bits change
//   with the gun's team/mode settings; every setting triggers unless GUN_CODE_MASK says otherwise.
// - NEC remotes: printed for reference (address/command match IRremote on the UNO and Nano nodes); never trigger.
// Anything else is reported as an unrecognized signal and ignored.
constexpr uint8_t IR_RECEIVER_PIN = 3;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 4;
constexpr uint8_t POSITIVE_TRIGGER_PIN = 5;  // active HIGH (3.3 V) while triggered, LOW when idle
constexpr uint8_t IR_LED_PIN = 7;
constexpr uint8_t RELAY_LED_PIN = 6;
constexpr uint32_t TRIGGER_PULSE_MS = 500;

constexpr uint8_t GUN_FRAME_BITS = 9;       // 0 = accept any pulse-width frame length
constexpr uint16_t GUN_CODE_MASK = 0x000;   // 0 = any code; e.g. mask 0x1F0 + value 0x140 to require the 10100 header
constexpr uint16_t GUN_CODE_VALUE = 0x000;

constexpr uint32_t HEARTBEAT_MS = 5000;
constexpr uint32_t IR_LED_HOLD_MS = 100;
constexpr uint32_t FRAME_END_GAP_US = 8000;    // HIGH this long ends a capture; the longest NEC space is 4.5 ms
constexpr uint8_t MIN_REPORTED_DURATIONS = 6;  // shorter unknown captures are noise and are not printed
constexpr bool PRINT_UNKNOWN_TIMINGS = false;  // true = dump mark/space lengths of signals that do not decode
constexpr uint8_t MAX_DURATIONS = 200;
constexpr uint8_t PW_MIN_BITS = 6;
constexpr uint8_t PW_MAX_BITS = 16;
constexpr uint8_t PW_CONFIRM_FRAMES = 2;       // identical frames needed in one shot (the gun sends about 5)
constexpr uint32_t CODE_RELEASE_MS = 300;      // repeated frames closer than this count as the same shot

// Alternating mark (LOW) and space (HIGH) lengths in microseconds, starting with the first mark.
uint16_t durations[MAX_DURATIONS];
uint8_t durationCount = 0;
bool frameOverflow = false;
bool frameInProgress = false;
int lastLevel = HIGH;
unsigned long lastEdgeAt = 0;
bool lastFrameWasRepeat = false;
unsigned long codesReceived = 0;
unsigned long shotsReceived = 0;
bool hasLastPwCode = false;
uint16_t lastPwCode = 0;
uint8_t lastPwBits = 0;
unsigned long lastPwAt = 0;

bool triggerActive = false;
unsigned long triggerStartedAt = 0;
unsigned long lastHeartbeatAt = 0;
bool irActivitySeen = false;
unsigned long lastIrActivityAt = 0;

// Negative output idles high-Z so a 5 V relay module pulls IN fully HIGH; a 3.3 V HIGH can leave it partly on.
// Positive output is driven: HIGH (3.3 V) when active, LOW when idle.
void setTrigger(bool active) {
  if (active) {
    pinMode(NEGATIVE_TRIGGER_PIN, OUTPUT);
    digitalWrite(NEGATIVE_TRIGGER_PIN, LOW);
  } else {
    pinMode(NEGATIVE_TRIGGER_PIN, INPUT);
  }
  digitalWrite(POSITIVE_TRIGGER_PIN, active ? HIGH : LOW);
  digitalWrite(RELAY_LED_PIN, active ? HIGH : LOW);
  triggerActive = active;
  if (active) {
    triggerStartedAt = millis();
  }
}

void printHex(uint32_t value, uint8_t digits) {
  Serial.print(F("0x"));
  for (int8_t shift = (digits - 1) * 4; shift >= 0; shift -= 4) {
    Serial.print((value >> shift) & 0xF, HEX);
  }
}

void printHeartbeat() {
  Serial.print(F("Heartbeat: IR input D"));
  Serial.print(IR_RECEIVER_PIN);
  Serial.print(F(", negative trigger output D"));
  Serial.print(NEGATIVE_TRIGGER_PIN);
  Serial.print(F(", positive trigger output D"));
  Serial.print(POSITIVE_TRIGGER_PIN);
  Serial.print(F(", trigger: any laser tag shot, shots "));
  Serial.print(shotsReceived);
  Serial.print(F(", other codes "));
  Serial.println(codesReceived);
}

bool within(uint16_t value, uint16_t low, uint16_t high) {
  return value >= low && value <= high;
}

void handleNecFrame(uint32_t raw) {
  uint8_t addressLow = raw & 0xFF;
  uint8_t addressHigh = (raw >> 8) & 0xFF;
  bool shortAddress = addressHigh == static_cast<uint8_t>(~addressLow);
  uint16_t address = shortAddress ? addressLow : (raw & 0xFFFF);
  uint8_t command = (raw >> 16) & 0xFF;

  ++codesReceived;
  Serial.print(F("IR NEC address "));
  printHex(address, shortAddress ? 2 : 4);
  Serial.print(F(", command "));
  printHex(command, 2);
  Serial.print(F(", raw "));
  printHex(raw, 8);
  Serial.println(F(" -> ignored (not a laser tag shot)"));
}

void printBinary(uint16_t value, uint8_t bits) {
  for (int8_t bit = bits - 1; bit >= 0; --bit) {
    Serial.print((value >> bit) & 1);
  }
}

// Parses one pulse-width frame whose leader mark is at durations[start]. Returns the index after the frame, or 0 if
// there is no valid frame there.
uint16_t parsePulseWidthFrame(uint16_t start, uint16_t &code, uint8_t &bits) {
  if (start + 1 >= durationCount || !within(durations[start], 4500, 7500) || !within(durations[start + 1], 250, 900)) {
    return 0;
  }
  code = 0;
  bits = 0;
  for (uint16_t i = start + 2; i < durationCount; i += 2) {
    uint16_t mark = durations[i];
    if (within(mark, 1100, 2000)) {
      code = (code << 1) | 1;
    } else if (within(mark, 250, 900)) {
      code <<= 1;
    } else {
      return 0;
    }
    if (++bits > PW_MAX_BITS) {
      return 0;
    }
    bool captureEnds = i + 1 >= durationCount;
    if (captureEnds || durations[i + 1] >= 3000) {
      return bits >= PW_MIN_BITS ? i + 2 : 0;
    }
    if (!within(durations[i + 1], 250, 1900)) {
      return 0;
    }
  }
  return 0;
}

// A press sends the same frame several times back to back; all of them land in one capture.
bool decodePulseWidth(uint16_t &code, uint8_t &bits, uint8_t &frames) {
  frames = 0;
  uint16_t frameCode = 0;
  uint8_t frameBits = 0;
  for (uint16_t i = 0; i < durationCount; i += 2) {
    uint16_t next = parsePulseWidthFrame(i, frameCode, frameBits);
    if (next == 0) {
      continue;
    }
    if (frames == 0) {
      code = frameCode;
      bits = frameBits;
    }
    if (frameCode == code && frameBits == bits) {
      ++frames;
    }
    i = next - 2;
  }
  return frames >= PW_CONFIRM_FRAMES;
}

void handlePulseWidthCode(uint16_t code, uint8_t bits, uint8_t frames) {
  unsigned long now = millis();
  bool samePress = hasLastPwCode && code == lastPwCode && bits == lastPwBits && now - lastPwAt < CODE_RELEASE_MS;
  hasLastPwCode = true;
  lastPwCode = code;
  lastPwBits = bits;
  lastPwAt = now;

  bool gunShot = (GUN_FRAME_BITS == 0 || bits == GUN_FRAME_BITS) && (code & GUN_CODE_MASK) == GUN_CODE_VALUE;
  if (samePress) {
    if (gunShot) {
      setTrigger(true);  // continuous fire keeps the relay on
    }
    return;
  }

  Serial.print(gunShot ? F("Laser tag shot: code ") : F("IR pulse-width code "));
  printHex(code, (bits + 3) / 4);
  Serial.print(F(" ("));
  Serial.print(bits);
  Serial.print(F(" bits "));
  printBinary(code, bits);
  Serial.print(F(", "));
  Serial.print(frames);
  Serial.print(F(" frames)"));

  if (gunShot) {
    ++shotsReceived;
    Serial.print(F(" -> TRIGGER: negative D4 LOW + positive D5 HIGH for "));
    Serial.print(TRIGGER_PULSE_MS);
    Serial.println(F(" ms"));
    setTrigger(true);
  } else {
    ++codesReceived;
    Serial.println(F(" -> ignored (does not match GUN_FRAME_BITS / GUN_CODE_MASK)"));
  }
}

void printUnknownFrame() {
  Serial.print(F("IR ignored: unrecognized signal, "));
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
  if (PRINT_UNKNOWN_TIMINGS) {
    Serial.print(F("  timings us (mark,space,...):"));
    for (uint8_t i = 0; i < durationCount; ++i) {
      Serial.print(i == 0 ? ' ' : ',');
      Serial.print(durations[i]);
    }
    Serial.println();
  }
}

// NEC: 9 ms mark + 4.5 ms space, 32 bits LSB first (560 us mark + 560 us space = 0, + 1690 us space = 1), stop mark.
// Repeat (button held): 9 ms mark + 2.25 ms space + stop mark.
void decodeFrame() {
  if (durationCount < 3) {
    return;
  }

  bool necLeader = within(durations[0], 7000, 11000);
  if (necLeader && durationCount <= 4 && within(durations[1], 1700, 2800)) {
    if (!lastFrameWasRepeat) {
      Serial.println(F("IR NEC repeat (button held)"));
    }
    lastFrameWasRepeat = true;
    return;
  }
  lastFrameWasRepeat = false;

  if (necLeader && within(durations[1], 3500, 5500) && durationCount >= 67) {
    uint32_t raw = 0;
    bool valid = true;
    for (uint8_t bit = 0; bit < 32 && valid; ++bit) {
      uint16_t mark = durations[2 + 2 * bit];
      uint16_t space = durations[3 + 2 * bit];
      if (!within(mark, 300, 900)) {
        valid = false;
      } else if (within(space, 1200, 2100)) {
        raw |= 1UL << bit;
      } else if (!within(space, 300, 900)) {
        valid = false;
      }
    }
    if (valid) {
      handleNecFrame(raw);
      return;
    }
  }

  uint16_t pwCode = 0;
  uint8_t pwBits = 0;
  uint8_t pwFrames = 0;
  if (decodePulseWidth(pwCode, pwBits, pwFrames)) {
    handlePulseWidthCode(pwCode, pwBits, pwFrames);
    return;
  }

  if (durationCount >= MIN_REPORTED_DURATIONS) {
    printUnknownFrame();
  }
}

// Polled because D3 is an ARC GPIO with no both-edge interrupt on the Arduino 101.
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

// Lights on any raw receiver activity, including frames that do not decode.
void updateIrLed(unsigned long now) {
  if (lastLevel == LOW) {
    irActivitySeen = true;
    lastIrActivityAt = now;
  }
  digitalWrite(IR_LED_PIN, irActivitySeen && now - lastIrActivityAt < IR_LED_HOLD_MS ? HIGH : LOW);
}

void setup() {
  Serial.begin(115200);

  pinMode(NEGATIVE_TRIGGER_PIN, INPUT);
  digitalWrite(POSITIVE_TRIGGER_PIN, LOW);
  pinMode(POSITIVE_TRIGGER_PIN, OUTPUT);
  pinMode(IR_RECEIVER_PIN, INPUT_PULLUP);
  digitalWrite(IR_LED_PIN, LOW);
  pinMode(IR_LED_PIN, OUTPUT);
  digitalWrite(RELAY_LED_PIN, LOW);
  pinMode(RELAY_LED_PIN, OUTPUT);

  lastLevel = digitalRead(IR_RECEIVER_PIN);
  lastEdgeAt = micros();

  Serial.println(F("Arduino 101 laser tag IR dual trigger ready"));
  Serial.print(F("Decoder: laser tag + NEC on D"));
  Serial.print(IR_RECEIVER_PIN);
  Serial.println(F(" (polled); other IR (phones, remotes) is ignored"));
  Serial.print(F("Trigger: any laser tag shot ("));
  Serial.print(GUN_FRAME_BITS);
  Serial.print(F("-bit frame"));
  if (GUN_CODE_MASK != 0) {
    Serial.print(F(", code & "));
    printHex(GUN_CODE_MASK, 3);
    Serial.print(F(" == "));
    printHex(GUN_CODE_VALUE, 3);
  }
  Serial.print(F(") -> negative D4 LOW + positive D5 HIGH for "));
  Serial.print(TRIGGER_PULSE_MS);
  Serial.println(F(" ms"));
  Serial.print(F("Status LEDs: IR D"));
  Serial.print(IR_LED_PIN);
  Serial.print(F(", relay D"));
  Serial.println(RELAY_LED_PIN);
  printHeartbeat();
  lastHeartbeatAt = millis();
}

void loop() {
  pollIrReceiver();

  unsigned long now = millis();
  updateIrLed(now);

  if (triggerActive && now - triggerStartedAt >= TRIGGER_PULSE_MS) {
    setTrigger(false);
  }

  // Heartbeats wait until a frame finishes so serial output never delays edge capture.
  if (!frameInProgress && now - lastHeartbeatAt >= HEARTBEAT_MS) {
    lastHeartbeatAt = now;
    printHeartbeat();
  }
}
