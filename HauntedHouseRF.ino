#include <EEPROM.h>
#include <RCSwitch.h>

RCSwitch rfReceiver = RCSwitch();

constexpr uint8_t RF_RECEIVER_PIN = 2;
constexpr uint8_t RELAY_PULSE_PIN = 4;
constexpr uint8_t RELAY_LOCKOUT_PULSE_PIN = 5;
constexpr uint8_t RELAY_TOGGLE_PIN = 6;

constexpr unsigned long PULSE_DURATION_MS = 250;
constexpr unsigned long LOCKOUT_MS = 15000;

constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint32_t CONFIG_MAGIC = 0x48484F55UL; // "HHOU"
constexpr int EEPROM_SLOT_ADDR = 0;
constexpr size_t SERIAL_LINE_SIZE = 64;

struct Config {
  uint32_t magic;
  uint32_t pulseCode;
  uint32_t lockoutPulseCode;
  uint32_t toggleCode;
};

Config config;

enum class LearnTarget : uint8_t {
  None,
  Pulse,
  Lockout,
  Toggle,
};

unsigned long pulseOffAt = 0;
unsigned long lockoutPulseOffAt = 0;
unsigned long lockoutReleaseAt = 0;
bool toggleRelayState = false;
LearnTarget learnTarget = LearnTarget::None;

char serialLine[SERIAL_LINE_SIZE];
size_t serialLineLength = 0;

bool timeReached(unsigned long now, unsigned long target) {
  return target != 0 && static_cast<long>(now - target) >= 0;
}

void setRelay(uint8_t pin, bool energized) {
  if (RELAY_ACTIVE_LOW) {
    digitalWrite(pin, energized ? LOW : HIGH);
  } else {
    digitalWrite(pin, energized ? HIGH : LOW);
  }
}

void pulseRelay(uint8_t pin, unsigned long &offAt, unsigned long durationMs) {
  setRelay(pin, true);
  offAt = millis() + durationMs;
}

void toggleRelay(uint8_t pin, bool &state) {
  state = !state;
  setRelay(pin, state);
}

void printCode(uint32_t code) {
  if (code == 0) {
    Serial.print(F("any"));
  } else {
    Serial.print(F("0x"));
    Serial.print(code, HEX);
  }
}

void printConfig() {
  Serial.println(F("Current RF configuration:"));
  Serial.print(F("  Pulse code: "));
  printCode(config.pulseCode);
  Serial.println();
  Serial.print(F("  Lockout pulse code: "));
  printCode(config.lockoutPulseCode);
  Serial.println();
  Serial.print(F("  Toggle code: "));
  printCode(config.toggleCode);
  Serial.println();
}

void saveConfig() {
  EEPROM.put(EEPROM_SLOT_ADDR, config);
}

void loadConfig() {
  EEPROM.get(EEPROM_SLOT_ADDR, config);
  if (config.magic != CONFIG_MAGIC) {
    config.magic = CONFIG_MAGIC;
    config.pulseCode = 0;
    config.lockoutPulseCode = 0;
    config.toggleCode = 0;
    saveConfig();
  }
}

bool codeMatches(uint32_t configuredCode, uint32_t receivedCode) {
  return configuredCode == 0 || configuredCode == receivedCode;
}

void printHelp() {
  Serial.println(F("Commands:"));
  Serial.println(F("  help"));
  Serial.println(F("  show"));
  Serial.println(F("  pulse <code|any>"));
  Serial.println(F("  lockout <code|any>"));
  Serial.println(F("  toggle <code|any>"));
  Serial.println(F("  learn pulse|lockout|toggle"));
  Serial.println(F("  cancel"));
  Serial.println(F("  clear pulse|lockout|toggle|all"));
  Serial.println(F("  reset"));
  Serial.println(F("Legacy aliases: list, set, clear."));
  Serial.println(F("Codes may be decimal or hex with 0x prefix."));
  Serial.println(F("Use any or 0 to make an action accept any received RF code."));
  Serial.println(F("Learn mode stores the next received RF code for the chosen action."));
}

uint32_t parseCode(const char *text) {
  if (strcmp(text, "any") == 0 || strcmp(text, "ANY") == 0) {
    return 0;
  }

  return static_cast<uint32_t>(strtoul(text, nullptr, 0));
}

void setActionCode(const char *which, uint32_t code) {
  if (strcmp(which, "pulse") == 0) {
    config.pulseCode = code;
  } else if (strcmp(which, "lockout") == 0) {
    config.lockoutPulseCode = code;
  } else if (strcmp(which, "toggle") == 0) {
    config.toggleCode = code;
  }
}

bool isActionName(const char *which) {
  return strcmp(which, "pulse") == 0 || strcmp(which, "lockout") == 0 || strcmp(which, "toggle") == 0;
}

LearnTarget actionNameToLearnTarget(const char *which) {
  if (strcmp(which, "pulse") == 0) {
    return LearnTarget::Pulse;
  }

  if (strcmp(which, "lockout") == 0) {
    return LearnTarget::Lockout;
  }

  if (strcmp(which, "toggle") == 0) {
    return LearnTarget::Toggle;
  }

  return LearnTarget::None;
}

const __FlashStringHelper *learnTargetName(LearnTarget target) {
  switch (target) {
    case LearnTarget::Pulse:
      return F("pulse");
    case LearnTarget::Lockout:
      return F("lockout");
    case LearnTarget::Toggle:
      return F("toggle");
    default:
      return F("none");
  }
}

bool clearActionCode(const char *which) {
  if (strcmp(which, "pulse") == 0) {
    config.pulseCode = 0;
  } else if (strcmp(which, "lockout") == 0) {
    config.lockoutPulseCode = 0;
  } else if (strcmp(which, "toggle") == 0) {
    config.toggleCode = 0;
  } else if (strcmp(which, "all") == 0) {
    config.pulseCode = 0;
    config.lockoutPulseCode = 0;
    config.toggleCode = 0;
  } else {
    return false;
  }

  return true;
}

char *trimLeading(char *text) {
  while (*text == ' ' || *text == '\t') {
    ++text;
  }
  return text;
}

void handleSerialCommand(char *line) {
  char *command = trimLeading(line);
  if (*command == '\0') {
    return;
  }

  char *first = strtok(command, " \t");
  if (first == nullptr) {
    return;
  }

  if (strcmp(first, "help") == 0) {
    printHelp();
    return;
  }

  if (strcmp(first, "list") == 0 || strcmp(first, "show") == 0) {
    printConfig();
    return;
  }

  if (strcmp(first, "reset") == 0) {
    config.magic = CONFIG_MAGIC;
    config.pulseCode = 0;
    config.lockoutPulseCode = 0;
    config.toggleCode = 0;
    learnTarget = LearnTarget::None;
    saveConfig();
    Serial.println(F("Configuration reset to wildcard mode."));
    return;
  }

  if (strcmp(first, "cancel") == 0) {
    learnTarget = LearnTarget::None;
    Serial.println(F("Learn mode canceled."));
    return;
  }

  if (strcmp(first, "learn") == 0) {
    char *which = strtok(nullptr, " \t");
    if (which == nullptr) {
      Serial.println(F("Usage: learn pulse|lockout|toggle"));
      return;
    }

    learnTarget = actionNameToLearnTarget(which);
    if (learnTarget == LearnTarget::None) {
      Serial.println(F("Unknown action. Use pulse, lockout, or toggle."));
      return;
    }

    Serial.print(F("Learn mode armed for "));
    Serial.print(which);
    Serial.println(F(". Send the RF button now."));
    return;
  }

  if (strcmp(first, "clear") == 0) {
    char *which = strtok(nullptr, " \t");
    if (which == nullptr) {
      Serial.println(F("Usage: clear pulse|lockout|toggle|all"));
      return;
    }

    if (!clearActionCode(which)) {
      Serial.println(F("Unknown action. Use pulse, lockout, toggle, or all."));
      return;
    }

    saveConfig();
    Serial.println(F("Cleared. That action now accepts any received RF code."));
    return;
  }

  if (strcmp(first, "set") == 0) {
    char *which = strtok(nullptr, " \t");
    char *codeText = strtok(nullptr, " \t");
    if (which == nullptr || codeText == nullptr) {
      Serial.println(F("Usage: set pulse|lockout|toggle <code|any>"));
      return;
    }

    if (!isActionName(which)) {
      Serial.println(F("Unknown action. Use pulse, lockout, or toggle."));
      return;
    }

    uint32_t code = parseCode(codeText);
    setActionCode(which, code);

    saveConfig();
    Serial.print(F("Saved "));
    Serial.print(which);
    Serial.print(F(" = "));
    printCode(code);
    Serial.println();
    return;
  }

  if (strcmp(first, "pulse") == 0 || strcmp(first, "lockout") == 0 || strcmp(first, "toggle") == 0) {
    char *codeText = strtok(nullptr, " \t");
    if (codeText == nullptr) {
      Serial.println(F("Usage: pulse|lockout|toggle <code|any>"));
      return;
    }

    uint32_t code = parseCode(codeText);
    setActionCode(first, code);
    saveConfig();

    Serial.print(F("Saved "));
    Serial.print(first);
    Serial.print(F(" = "));
    printCode(code);
    Serial.println();
    return;
  }

  Serial.println(F("Unknown command. Type help."));
}

void readSerialCommands() {
  while (Serial.available() > 0) {
    char incoming = static_cast<char>(Serial.read());
    if (incoming == '\r') {
      continue;
    }

    if (incoming == '\n') {
      serialLine[serialLineLength] = '\0';
      handleSerialCommand(serialLine);
      serialLineLength = 0;
      continue;
    }

    if (serialLineLength < SERIAL_LINE_SIZE - 1) {
      serialLine[serialLineLength++] = incoming;
    }
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PULSE_PIN, OUTPUT);
  pinMode(RELAY_LOCKOUT_PULSE_PIN, OUTPUT);
  pinMode(RELAY_TOGGLE_PIN, OUTPUT);

  setRelay(RELAY_PULSE_PIN, false);
  setRelay(RELAY_LOCKOUT_PULSE_PIN, false);
  setRelay(RELAY_TOGGLE_PIN, false);

  loadConfig();
  rfReceiver.enableReceive(digitalPinToInterrupt(RF_RECEIVER_PIN));

  Serial.println(F("Haunted House RF controller ready"));
  Serial.println(F("If a code is 0, that action accepts any received RF code."));
  printHelp();
  printConfig();
}

void loop() {
  unsigned long now = millis();

  readSerialCommands();

  if (timeReached(now, pulseOffAt)) {
    setRelay(RELAY_PULSE_PIN, false);
    pulseOffAt = 0;
  }

  if (timeReached(now, lockoutPulseOffAt)) {
    setRelay(RELAY_LOCKOUT_PULSE_PIN, false);
    lockoutPulseOffAt = 0;
  }

  if (lockoutReleaseAt != 0 && timeReached(now, lockoutReleaseAt)) {
    lockoutReleaseAt = 0;
  }

  if (rfReceiver.available()) {
    unsigned long receivedCode = rfReceiver.getReceivedValue();

    Serial.print(F("Received RF code: "));
    Serial.println(receivedCode);

    if (learnTarget != LearnTarget::None) {
      switch (learnTarget) {
        case LearnTarget::Pulse:
          config.pulseCode = receivedCode;
          break;
        case LearnTarget::Lockout:
          config.lockoutPulseCode = receivedCode;
          break;
        case LearnTarget::Toggle:
          config.toggleCode = receivedCode;
          break;
        default:
          break;
      }

      saveConfig();
      Serial.print(F("Learned and saved "));
      Serial.print(learnTargetName(learnTarget));
      Serial.print(F(" = "));
      printCode(receivedCode);
      Serial.println();
      learnTarget = LearnTarget::None;
      rfReceiver.resetAvailable();
      return;
    }

    if (codeMatches(config.pulseCode, receivedCode)) {
      pulseRelay(RELAY_PULSE_PIN, pulseOffAt, PULSE_DURATION_MS);
    }

    if (codeMatches(config.lockoutPulseCode, receivedCode)) {
      if (lockoutReleaseAt == 0) {
        pulseRelay(RELAY_LOCKOUT_PULSE_PIN, lockoutPulseOffAt, PULSE_DURATION_MS);
        lockoutReleaseAt = millis() + LOCKOUT_MS;
      } else {
        Serial.println(F("Lockout pulse ignored during 15 second cooldown"));
      }
    }

    if (codeMatches(config.toggleCode, receivedCode)) {
      toggleRelay(RELAY_TOGGLE_PIN, toggleRelayState);
    }

    rfReceiver.resetAvailable();
  }
}
