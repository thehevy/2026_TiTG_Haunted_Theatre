#include <WiFi.h>
#include <PubSubClient.h>
#include <WebServer.h>
#include <Preferences.h>
#include <IRremote.hpp>

constexpr uint8_t RELAY1_PIN = 4;
constexpr uint8_t RELAY2_PIN = 5;
constexpr uint8_t RELAY3_PIN = 18;
constexpr uint8_t POSITIVE_TRIGGER_PIN = 16;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = 17;
constexpr uint8_t IR_RECEIVER_PIN = 19;
constexpr bool RELAY_ACTIVE_LOW = true;

constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long INPUT_DEBOUNCE_MS = 50;
constexpr unsigned long IR_DEBOUNCE_MS = 250;
constexpr unsigned long WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr unsigned long WIFI_RETRY_MS = 30000;
constexpr unsigned long MQTT_RETRY_MS = 5000;
constexpr uint16_t CONFIG_PORTAL_PORT = 80;
constexpr uint8_t IR_CODE_0 = 0x00;

const char *DEFAULT_WIFI_SSID = "TITG2026HT";
const char *DEFAULT_WIFI_PASSWORD = "";
const char *DEFAULT_MQTT_HOST = "192.168.1.10";
constexpr uint16_t DEFAULT_MQTT_PORT = 1883;
const char *DEFAULT_DEVICE_ID = "2026-ht-node-01";

char wifiSsid[33];
char wifiPassword[65];
char mqttHost[64];
uint16_t mqttPort = DEFAULT_MQTT_PORT;
char deviceId[33];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
WebServer configServer(CONFIG_PORTAL_PORT);
Preferences prefs;

unsigned long relayOffAt[3] = {0, 0, 0};
bool relayState[3] = {false, false, false};
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;
unsigned long lastHeartbeatAt = 0;
unsigned long lastIrTriggerAt = 0;
uint8_t lastIrCommand = 0;
uint32_t lastIrRawData = 0;
unsigned long lastMqttAttemptAt = 0;
unsigned long lastWiFiRetryAt = 0;
bool configPortalActive = false;
bool restartPending = false;
unsigned long restartAt = 0;

void copyCString(char *destination, size_t destinationSize, const char *source) {
  if (destinationSize == 0) {
    return;
  }

  strncpy(destination, source, destinationSize - 1);
  destination[destinationSize - 1] = '\0';
}

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

  String ssid = prefs.getString("ssid", wifiSsid);
  String password = prefs.getString("pass", wifiPassword);
  String host = prefs.getString("mqtt_host", mqttHost);
  String storedDeviceId = prefs.getString("device_id", deviceId);
  uint16_t storedPort = prefs.getUShort("mqtt_port", mqttPort);
  prefs.end();

  ssid.toCharArray(wifiSsid, sizeof(wifiSsid));
  password.toCharArray(wifiPassword, sizeof(wifiPassword));
  host.toCharArray(mqttHost, sizeof(mqttHost));
  storedDeviceId.toCharArray(deviceId, sizeof(deviceId));
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

void setRelay(uint8_t pin, bool on) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void pulseRelay(uint8_t index, uint8_t pin, unsigned long durationMs) {
  relayState[index] = true;
  setRelay(pin, true);
  relayOffAt[index] = millis() + durationMs;
}

void holdRelayActive(uint8_t index, uint8_t pin) {
  relayState[index] = true;
  setRelay(pin, true);
}

void releaseRelay(uint8_t index, uint8_t pin) {
  relayState[index] = false;
  setRelay(pin, false);
}

void toggleRelay(uint8_t index, uint8_t pin) {
  relayState[index] = !relayState[index];
  setRelay(pin, relayState[index]);
}

bool timeReached(unsigned long now, unsigned long target) {
  return target != 0 && static_cast<long>(now - target) >= 0;
}

void printHeader() {
  Serial.println(F("=== ESP32 Node ==="));
  Serial.println(F("Purpose: MQTT, IR, and local trigger relay controller."));
  Serial.print(F("Device ID: "));
  Serial.println(deviceId);
  Serial.print(F("WiFi SSID: "));
  Serial.println(wifiSsid);
  Serial.print(F("MQTT host: "));
  Serial.print(mqttHost);
  Serial.print(F(":"));
  Serial.println(mqttPort);
  Serial.println(F("Input pins:"));
  Serial.print(F("  IR receiver data: GPIO"));
  Serial.println(IR_RECEIVER_PIN);
  Serial.print(F("  Positive trigger input (active HIGH, holds Relay 2): GPIO"));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("  Negative trigger input (active LOW, holds Relay 3): GPIO"));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.println(F("Output pins:"));
  Serial.print(F("  Relay 1 (IR 0x00 pulse): GPIO"));
  Serial.println(RELAY1_PIN);
  Serial.print(F("  Relay 2 (GPIO16 hold while active): GPIO"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("  Relay 3 (GPIO17 hold while active): GPIO"));
  Serial.println(RELAY3_PIN);
  Serial.println(F("Relay mode: active LOW"));
  Serial.print(F("Pulse duration (ms): "));
  Serial.println(PULSE_MS);
  Serial.println(F("Default timeout: none"));
  Serial.println(F("IR programming: 0x00 triggers Relay 1 once."));
  Serial.println(F("Ready."));
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
    if (command == IR_CODE_0) {
      pulseRelay(0, RELAY1_PIN, PULSE_MS);
      Serial.println(F("IR 0x00 received: Relay 1 pulse"));
    }

    lastIrTriggerAt = now;
  }

  IrReceiver.resume();
}

void readSerialDebugCommands() {
  while (Serial.available() > 0) {
    char command = static_cast<char>(Serial.read());
    switch (command) {
      case '1': pulseRelay(0, RELAY1_PIN, PULSE_MS); break;
      case '2': holdRelayActive(1, RELAY2_PIN); break;
      case '3': holdRelayActive(2, RELAY3_PIN); break;
      case 'i': printLastIrDebug(); break;
      default: break;
    }
  }
}

void publishStatus(const char *payload) {
  char topic[64];
  snprintf(topic, sizeof(topic), "haunt/%s/status", deviceId);
  mqttClient.publish(topic, payload, true);
}

void handleMessage(char *topic, byte *payload, unsigned int length) {
  (void)topic;

  char message[64];
  unsigned int copyLength = length < sizeof(message) - 1 ? length : sizeof(message) - 1;
  memcpy(message, payload, copyLength);
  message[copyLength] = '\0';

  if (strcmp(message, "relay1:pulse") == 0) {
    pulseRelay(0, RELAY1_PIN, PULSE_MS);
  } else if (strcmp(message, "relay2:pulse") == 0) {
    holdRelayActive(1, RELAY2_PIN);
  } else if (strcmp(message, "relay3:toggle") == 0) {
    toggleRelay(2, RELAY3_PIN);
  }
}

void firePositiveInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: POSITIVE -> Relay 2 active while GPIO16 stays HIGH"));
  holdRelayActive(1, RELAY2_PIN);
  lastPositiveTriggerAt = now;
}

void fireNegativeInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: NEGATIVE -> Relay 3 active while GPIO17 stays LOW"));
  holdRelayActive(2, RELAY3_PIN);
  lastNegativeTriggerAt = now;
}

void handleInputTriggers(unsigned long now) {
  bool positiveActive = digitalRead(POSITIVE_TRIGGER_PIN) == HIGH;
  bool negativeActive = digitalRead(NEGATIVE_TRIGGER_PIN) == LOW;

  if (positiveActive && !lastPositiveActive && (now - lastPositiveTriggerAt >= INPUT_DEBOUNCE_MS)) {
    firePositiveInputTrigger(now);
  }

  if (!positiveActive && lastPositiveActive) {
    releaseRelay(1, RELAY2_PIN);
  }

  if (negativeActive && !lastNegativeActive && (now - lastNegativeTriggerAt >= INPUT_DEBOUNCE_MS)) {
    fireNegativeInputTrigger(now);
  }

  if (!negativeActive && lastNegativeActive) {
    releaseRelay(2, RELAY3_PIN);
  }

  lastPositiveActive = positiveActive;
  lastNegativeActive = negativeActive;
}

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
  html.reserve(1200);
  html += F("<!doctype html><html><head><meta charset='utf-8'>");
  html += F("<meta name='viewport' content='width=device-width, initial-scale=1'>");
  html += F("<title>ESP32 Config</title>");
  html += F("<style>body{font-family:Arial,sans-serif;margin:24px;}label{display:block;margin-top:12px;}input{width:100%;max-width:420px;padding:8px;}button{margin-top:14px;padding:10px 14px;}small{color:#444;}</style>");
  html += F("</head><body><h1>ESP32 Node Setup</h1>");
  html += F("<p>Enter WiFi and MQTT settings, then save to reboot.</p>");
  html += F("<form method='POST' action='/save'>");
  html += F("<label>WiFi SSID</label><input name='ssid' required value='");
  html += wifiSsid;
  html += F("'>");
  html += F("<label>WiFi Password</label><input name='pass' type='password' value='");
  html += wifiPassword;
  html += F("'>");
  html += F("<label>MQTT Host</label><input name='mqtt_host' required value='");
  html += mqttHost;
  html += F("'>");
  html += F("<label>MQTT Port</label><input name='mqtt_port' required value='");
  html += mqttPort;
  html += F("'>");
  html += F("<label>Device ID</label><input name='device_id' required value='");
  html += deviceId;
  html += F("'>");
  html += F("<button type='submit'>Save and Restart</button></form>");
  html += F("<p><small>AP password: hauntsetup</small></p>");
  html += F("</body></html>");

  configServer.send(200, "text/html", html);
}

void handleSaveConfig() {
  if (!configServer.hasArg("ssid") || !configServer.hasArg("mqtt_host") ||
      !configServer.hasArg("mqtt_port") || !configServer.hasArg("device_id")) {
    configServer.send(400, "text/plain", "Missing required fields.");
    return;
  }

  String ssid = configServer.arg("ssid");
  String password = configServer.arg("pass");
  String host = configServer.arg("mqtt_host");
  String portText = configServer.arg("mqtt_port");
  String id = configServer.arg("device_id");

  ssid.trim();
  host.trim();
  portText.trim();
  id.trim();

  if (ssid.length() == 0 || host.length() == 0 || id.length() == 0) {
    configServer.send(400, "text/plain", "SSID, MQTT host, and device ID are required.");
    return;
  }

  long parsedPort = portText.toInt();
  if (parsedPort <= 0 || parsedPort > 65535) {
    configServer.send(400, "text/plain", "MQTT port must be between 1 and 65535.");
    return;
  }

  ssid.toCharArray(wifiSsid, sizeof(wifiSsid));
  password.toCharArray(wifiPassword, sizeof(wifiPassword));
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

  bool started = WiFi.softAP(apName, "hauntsetup");
  if (!started) {
    Serial.println(F("Failed to start configuration AP."));
    return;
  }

  IPAddress apIp = WiFi.softAPIP();
  Serial.println(F("Configuration AP started."));
  Serial.print(F("AP SSID: "));
  Serial.println(apName);
  Serial.println(F("AP password: hauntsetup"));
  Serial.print(F("Open http://"));
  Serial.print(apIp);
  Serial.println(F(" in a browser."));

  configServer.on("/", HTTP_GET, sendConfigPortalPage);
  configServer.on("/save", HTTP_POST, handleSaveConfig);
  configServer.begin();
}

void ensureMqttConnected(unsigned long now) {
  if (mqttClient.connected() || WiFi.status() != WL_CONNECTED || configPortalActive) {
    return;
  }

  if ((now - lastMqttAttemptAt) < MQTT_RETRY_MS) {
    return;
  }

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
  mqttClient.setCallback(handleMessage);

  if (!connectWiFiWithTimeout(WIFI_CONNECT_TIMEOUT_MS)) {
    startConfigPortal();
  }
}

void loop() {
  unsigned long now = millis();
  readSerialDebugCommands();

  if (restartPending && static_cast<long>(now - restartAt) >= 0) {
    ESP.restart();
  }

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

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    if (configPortalActive) {
      Serial.println(F("Heartbeat: config portal active"));
    } else {
      Serial.println(F("Heartbeat: node running"));
    }
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (relayOffAt[i] != 0 && timeReached(now, relayOffAt[i])) {
      setRelay((i == 0) ? RELAY1_PIN : (i == 1) ? RELAY2_PIN : RELAY3_PIN, false);
      relayState[i] = false;
      relayOffAt[i] = 0;
    }
  }
}
