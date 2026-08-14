// Acebott ESP32-Max v1.0 Node
// ESP32-WROOM-DA based board.
// WiFi + MQTT relay controller with IR command map, AP config portal,
// and local trigger inputs - matching UNO_Node IR behavior and ESP32_Node
// network behavior.
//
// IDE: Arduino IDE 2.x, Board: ESP32 Dev Module
// Required libraries: PubSubClient, IRremote (via Library Manager)

#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <IRremote.hpp>

// GPIO pin assignments - safe non-strapping GPIOs on ESP32-WROOM-DA.
constexpr uint8_t RELAY1_PIN           = 4;
constexpr uint8_t RELAY2_PIN           = 5;
constexpr uint8_t RELAY3_PIN           = 18;
constexpr uint8_t POSITIVE_TRIGGER_PIN = 16;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 17;
constexpr uint8_t IR_RECEIVER_PIN      = 19;
constexpr bool    RELAY_ACTIVE_LOW     = true;

constexpr unsigned long PULSE_MS               = 250;
constexpr unsigned long LOCKOUT_MS             = 15000;
constexpr unsigned long INPUT_DEBOUNCE_MS      = 50;
constexpr unsigned long IR_DEBOUNCE_MS         = 250;
constexpr unsigned long IR_LOCKOUT_MODE_NONE_MS = 0;
constexpr unsigned long IR_LOCKOUT_MODE_5S_MS  = 5000;
constexpr unsigned long IR_LOCKOUT_MODE_15S_MS = 15000;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr unsigned long WIFI_RETRY_MS          = 30000;
constexpr unsigned long MQTT_RETRY_MS          = 5000;
constexpr uint16_t CONFIG_PORTAL_PORT          = 80;

// IR remote button codes (NEC protocol, same as UNO_Node).
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

// Stored/runtime network config.
const char *DEFAULT_WIFI_SSID     = "TITG2026HT";
const char *DEFAULT_WIFI_PASSWORD = "";
const char *DEFAULT_MQTT_HOST     = "192.168.1.10";
constexpr uint16_t DEFAULT_MQTT_PORT = 1883;
const char *DEFAULT_DEVICE_ID     = "acebott-esp32max-01";

char wifiSsid[33];
char wifiPassword[65];
char mqttHost[64];
uint16_t mqttPort = DEFAULT_MQTT_PORT;
char deviceId[33];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WebServer configServer(CONFIG_PORTAL_PORT);
Preferences prefs;

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
unsigned long lastMqttAttemptAt = 0;
unsigned long lastWiFiRetryAt = 0;
bool configPortalActive = false;
bool restartPending = false;
unsigned long restartAt = 0;

// ---- Utility ---------------------------------------------------------------

void copyCString(char *dst, size_t dstSize, const char *src) {
  if (dstSize == 0) return;
  strncpy(dst, src, dstSize - 1);
  dst[dstSize - 1] = '\0';
}

// ---- Config persistence ----------------------------------------------------

void loadConfig() {
  copyCString(wifiSsid, sizeof(wifiSsid), DEFAULT_WIFI_SSID);
  copyCString(wifiPassword, sizeof(wifiPassword), DEFAULT_WIFI_PASSWORD);
  copyCString(mqttHost, sizeof(mqttHost), DEFAULT_MQTT_HOST);
  copyCString(deviceId, sizeof(deviceId), DEFAULT_DEVICE_ID);
  mqttPort = DEFAULT_MQTT_PORT;

  if (!prefs.begin("hauntcfg", true)) {
    Serial.println(F("Config storage unavailable, using defaults."));
    return;
  }

  prefs.getString("ssid", wifiSsid, sizeof(wifiSsid));
  prefs.getString("pass", wifiPassword, sizeof(wifiPassword));
  prefs.getString("mqtt_host", mqttHost, sizeof(mqttHost));
  prefs.getString("device_id", deviceId, sizeof(deviceId));
  uint16_t storedPort = prefs.getUShort("mqtt_port", DEFAULT_MQTT_PORT);
  prefs.end();

  mqttPort = storedPort == 0 ? DEFAULT_MQTT_PORT : storedPort;
}

void saveConfig() {
  if (!prefs.begin("hauntcfg", false)) {
    Serial.println(F("Failed to open config storage for writing."));
    return;
  }
  prefs.putString("ssid", wifiSsid);
  prefs.putString("pass", wifiPassword);
  prefs.putString("mqtt_host", mqttHost);
  prefs.putString("device_id", deviceId);
  prefs.putUShort("mqtt_port", mqttPort);
  prefs.end();
}

// ---- Relay control ---------------------------------------------------------

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

void pulseAllRelays() {
  pulseRelay(0, RELAY1_PIN, PULSE_MS);
  pulseRelay(1, RELAY2_PIN, PULSE_MS);
  pulseRelay(2, RELAY3_PIN, PULSE_MS);
}

bool timeReached(unsigned long now, unsigned long target) {
  return target != 0 && static_cast<long>(now - target) >= 0;
}

// ---- Serial header ---------------------------------------------------------

void printHeader() {
  Serial.println(F("=== Acebott ESP32-Max Node ==="));
  Serial.println(F("Purpose: WiFi/MQTT, IR, and local trigger relay controller"));
  Serial.print(F("Device ID: "));
  Serial.println(deviceId);
  Serial.print(F("WiFi SSID: "));
  Serial.println(wifiSsid);
  Serial.print(F("MQTT host: "));
  Serial.print(mqttHost);
  Serial.print(F(":"));
  Serial.println(mqttPort);
  Serial.print(F("Relay 1 pulse: GPIO"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("Relay 2 pulse+lockout: GPIO"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("Relay 3 toggle: GPIO"));
  Serial.println(RELAY3_PIN);
  Serial.print(F("Positive trigger (active HIGH): GPIO"));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("Negative trigger (active LOW): GPIO"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.print(F("IR receiver: GPIO"));
  Serial.println(IR_RECEIVER_PIN);
  Serial.println(F("IR map: A1/A2/A3 pulse, A4/A5/A6 toggle, A7 pulse all"));
  Serial.println(F("IR map: M1/M2/M3 lockout none/5s/15s, POWER reprint header"));
  Serial.println(F("Serial debug: i -> print last IR code"));
  Serial.println(F("Serial commands: 1,2,3 pulse | a,b,c toggle"));
  Serial.println(F("Ready."));
}

// ---- Lockout helpers -------------------------------------------------------

bool isIrLockoutActive(unsigned long now) {
  return irActionLockoutUntil != 0 && static_cast<long>(now - irActionLockoutUntil) < 0;
}

void startIrLockoutIfEnabled(unsigned long now) {
  irActionLockoutUntil = (irActionLockoutMs == 0) ? 0 : now + irActionLockoutMs;
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

void printLockoutCountdown(const __FlashStringHelper *label, unsigned long now,
                           unsigned long lockoutUntil, int &lastPrintedSeconds) {
  if (lockoutUntil == 0 || static_cast<long>(now - lockoutUntil) >= 0) {
    if (lastPrintedSeconds != -1) {
      Serial.print(label);
      Serial.println(F(" lockout cleared"));
      lastPrintedSeconds = -1;
    }
    return;
  }
  int remaining = static_cast<int>((lockoutUntil - now + 999) / 1000);
  if (remaining != lastPrintedSeconds) {
    Serial.print(label);
    Serial.print(F(" lockout remaining: "));
    Serial.print(remaining);
    Serial.println(F("s"));
    lastPrintedSeconds = remaining;
  }
}

// ---- IR decode -------------------------------------------------------------

void printLastIrDebug() {
  Serial.print(F("Last IR command: 0x"));
  if (lastIrCommand < 0x10) Serial.print('0');
  Serial.println(lastIrCommand, HEX);
  Serial.print(F("Last IR raw: 0x"));
  Serial.println(lastIrRawData, HEX);
}

void handleIrInput(unsigned long now) {
  if (!IrReceiver.decode()) return;

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

// ---- Local trigger inputs --------------------------------------------------

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

// ---- Serial commands -------------------------------------------------------

void readSerialDebugCommands() {
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

// ---- MQTT ------------------------------------------------------------------

void publishStatus(const char *payload) {
  char topic[64];
  snprintf(topic, sizeof(topic), "haunt/%s/status", deviceId);
  mqttClient.publish(topic, payload, true);
}

void handleMqttMessage(char *topic, byte *payload, unsigned int length) {
  (void)topic;
  char message[64];
  unsigned int copyLen = length < sizeof(message) - 1 ? length : sizeof(message) - 1;
  memcpy(message, payload, copyLen);
  message[copyLen] = '\0';

  if (strcmp(message, "relay1:pulse") == 0) {
    pulseRelay(0, RELAY1_PIN, PULSE_MS);
  } else if (strcmp(message, "relay2:pulse") == 0) {
    if (relay2LockoutUntil == 0) {
      pulseRelay(1, RELAY2_PIN, PULSE_MS);
      relay2LockoutUntil = millis() + LOCKOUT_MS;
    }
  } else if (strcmp(message, "relay3:toggle") == 0) {
    toggleRelay(2, RELAY3_PIN);
  }
}

void ensureMqttConnected(unsigned long now) {
  if (mqttClient.connected() || WiFi.status() != WL_CONNECTED || configPortalActive) return;
  if ((now - lastMqttAttemptAt) < MQTT_RETRY_MS) return;

  lastMqttAttemptAt = now;
  String clientId = String(deviceId) + "-" + String(static_cast<uint32_t>(ESP.getEfuseMac()), HEX);

  if (mqttClient.connect(clientId.c_str())) {
    char topic[96];
    snprintf(topic, sizeof(topic), "haunt/%s/trigger", deviceId);
    mqttClient.subscribe(topic);
    publishStatus("online");
    Serial.println(F("MQTT connected."));
  } else {
    Serial.print(F("MQTT connect failed. State: "));
    Serial.println(mqttClient.state());
  }
}

// ---- WiFi and AP config portal ---------------------------------------------

bool connectWiFiWithTimeout(unsigned long timeoutMs) {
  if (strlen(wifiSsid) == 0) {
    Serial.println(F("WiFi SSID is empty. Starting config portal."));
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifiSsid, wifiPassword);

  Serial.print(F("Connecting to WiFi"));
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print(F("WiFi connected. IP: "));
    Serial.println(WiFi.localIP());
    return true;
  }

  Serial.print(F("WiFi connection failed. Status: "));
  Serial.println(static_cast<int>(WiFi.status()));
  WiFi.disconnect(true);
  return false;
}

void sendConfigPortalPage() {
  String html;
  html.reserve(1400);
  html += F("<!doctype html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width,initial-scale=1'>");
  html += F("<title>Acebott ESP32-Max Config</title>");
  html += F("<style>body{font-family:Arial,sans-serif;margin:24px;}label{display:block;margin-top:12px;}");
  html += F("input{width:100%;max-width:420px;padding:8px;}button{margin-top:14px;padding:10px 14px;}</style>");
  html += F("</head><body><h1>Acebott ESP32-Max Setup</h1>");
  html += F("<p>Enter WiFi and MQTT settings, then save to reboot.</p>");
  html += F("<form method='POST' action='/save'>");
  html += F("<label>WiFi SSID</label><input name='ssid' required value='"); html += wifiSsid; html += F("'>");
  html += F("<label>WiFi Password</label><input name='pass' type='password' value='"); html += wifiPassword; html += F("'>");
  html += F("<label>MQTT Host</label><input name='mqtt_host' required value='"); html += mqttHost; html += F("'>");
  html += F("<label>MQTT Port</label><input name='mqtt_port' required value='"); html += mqttPort; html += F("'>");
  html += F("<label>Device ID</label><input name='device_id' required value='"); html += deviceId; html += F("'>");
  html += F("<button type='submit'>Save and Restart</button></form>");
  html += F("<p><small>AP password: hauntsetup</small></p></body></html>");
  configServer.send(200, "text/html", html);
}

void handleSaveConfig() {
  if (!configServer.hasArg("ssid") || !configServer.hasArg("mqtt_host") ||
      !configServer.hasArg("mqtt_port") || !configServer.hasArg("device_id")) {
    configServer.send(400, "text/plain", "Missing required fields.");
    return;
  }

  String ssid = configServer.arg("ssid");   ssid.trim();
  String pass = configServer.arg("pass");
  String host = configServer.arg("mqtt_host"); host.trim();
  String portText = configServer.arg("mqtt_port"); portText.trim();
  String id = configServer.arg("device_id");   id.trim();

  if (ssid.length() == 0 || host.length() == 0 || id.length() == 0) {
    configServer.send(400, "text/plain", "SSID, MQTT host, and device ID are required.");
    return;
  }

  long parsedPort = portText.toInt();
  if (parsedPort <= 0 || parsedPort > 65535) {
    configServer.send(400, "text/plain", "MQTT port must be 1-65535.");
    return;
  }

  ssid.toCharArray(wifiSsid, sizeof(wifiSsid));
  pass.toCharArray(wifiPassword, sizeof(wifiPassword));
  host.toCharArray(mqttHost, sizeof(mqttHost));
  id.toCharArray(deviceId, sizeof(deviceId));
  mqttPort = static_cast<uint16_t>(parsedPort);

  saveConfig();
  configServer.send(200, "text/plain", "Saved. Rebooting in 2 seconds...");
  restartPending = true;
  restartAt = millis() + 2000;
}

void startConfigPortal() {
  configPortalActive = true;
  WiFi.disconnect(true, true);
  delay(100);
  WiFi.mode(WIFI_AP);

  uint32_t chipSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFF);
  char apName[32];
  snprintf(apName, sizeof(apName), "HauntSetup-%06lX", chipSuffix);

  if (!WiFi.softAP(apName, "hauntsetup")) {
    Serial.println(F("Failed to start configuration AP."));
    return;
  }

  Serial.println(F("Configuration AP started."));
  Serial.print(F("AP SSID: ")); Serial.println(apName);
  Serial.println(F("AP password: hauntsetup"));
  Serial.print(F("Open http://")); Serial.print(WiFi.softAPIP()); Serial.println(F(" in a browser."));

  configServer.on("/", HTTP_GET, sendConfigPortalPage);
  configServer.on("/save", HTTP_POST, handleSaveConfig);
  configServer.begin();
}

// ---- Arduino entry points --------------------------------------------------

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

  loadConfig();
  printHeader();

  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setCallback(handleMqttMessage);

  if (!connectWiFiWithTimeout(WIFI_CONNECT_TIMEOUT_MS)) {
    startConfigPortal();
  }
}

void loop() {
  unsigned long now = millis();

  if (restartPending && static_cast<long>(now - restartAt) >= 0) {
    ESP.restart();
  }

  readSerialDebugCommands();

  if (configPortalActive) {
    configServer.handleClient();
  } else if (WiFi.status() != WL_CONNECTED) {
    if ((now - lastWiFiRetryAt) >= WIFI_RETRY_MS) {
      lastWiFiRetryAt = now;
      Serial.println(F("WiFi disconnected; retrying."));
      if (!connectWiFiWithTimeout(WIFI_CONNECT_TIMEOUT_MS)) {
        startConfigPortal();
      }
    }
  } else {
    ensureMqttConnected(now);
    mqttClient.loop();
  }

  handleInputTriggers(now);
  handleIrInput(now);

  printLockoutCountdown(F("IR pulse action"), now, irActionLockoutUntil, lastIrLockoutCountdownSeconds);
  printLockoutCountdown(F("Relay2 input"), now, relay2LockoutUntil, lastRelay2LockoutCountdownSeconds);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    if (configPortalActive) {
      Serial.println(F("Heartbeat: config portal active"));
    } else {
      Serial.println(F("Heartbeat: node running"));
    }
  }

  if (timeReached(now, relay2LockoutUntil)) relay2LockoutUntil = 0;
  if (timeReached(now, irActionLockoutUntil)) irActionLockoutUntil = 0;

  for (uint8_t i = 0; i < 3; ++i) {
    if (relayOffAt[i] != 0 && static_cast<long>(now - relayOffAt[i]) >= 0) {
      relayOffAt[i] = 0;
      if (i == 0) setRelay(RELAY1_PIN, false);
      if (i == 1) setRelay(RELAY2_PIN, false);
      if (i == 2) setRelay(RELAY3_PIN, relayState[2]);
    }
  }
}
