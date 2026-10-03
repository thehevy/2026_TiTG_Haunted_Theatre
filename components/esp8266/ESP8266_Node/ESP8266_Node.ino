#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <IRremote.hpp>

constexpr uint8_t RELAY1_PIN = D1;
constexpr uint8_t RELAY2_PIN = D2;
constexpr uint8_t RELAY3_PIN = D5;
constexpr uint8_t POSITIVE_TRIGGER_PIN = D6;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = D7;
constexpr uint8_t IR_RECEIVER_PIN = D4;
constexpr bool RELAY_ACTIVE_LOW = true;

constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long INPUT_DEBOUNCE_MS = 50;
constexpr unsigned long IR_DEBOUNCE_MS = 250;
constexpr uint8_t IR_CODE_0 = 0x00;

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *MQTT_HOST = "192.168.1.10";
constexpr uint16_t MQTT_PORT = 1883;
const char *DEVICE_ID = "esp8266-node-01";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool relayState[3] = {false, false, false};
unsigned long relayOffAt[3] = {0, 0, 0};
unsigned long relay1OffAt = 0;
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;
unsigned long lastHeartbeatAt = 0;
unsigned long lastIrTriggerAt = 0;
uint8_t lastIrCommand = 0;
uint32_t lastIrRawData = 0;

void setRelay(uint8_t pin, bool on) {
  digitalWrite(pin, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void pulseRelay(uint8_t index, uint8_t pin, unsigned long durationMs) {
  setRelay(pin, true);
  relayState[index] = true;
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
  Serial.println(F("=== ESP8266 Node ==="));
  Serial.println(F("Purpose: MQTT, IR, and local trigger relay controller."));
  Serial.print(F("Device ID: "));
  Serial.println(DEVICE_ID);
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
  Serial.print(F("  Relay 2 (GPIO12 hold while active): GPIO"));
  Serial.println(RELAY2_PIN);
  Serial.print(F("  Relay 3 (GPIO13 hold while active): GPIO"));
  Serial.println(RELAY3_PIN);
  Serial.println(F("Relay mode: active LOW"));
  Serial.print(F("Pulse duration (ms): "));
  Serial.println(PULSE_MS);
  Serial.println(F("Default timeout: none"));
  Serial.println(F("IR programming: 0x00 triggers Relay 1 once."));
  Serial.println(F("Ready."));
}

const __FlashStringHelper *wifiStatusString() {
  switch (WiFi.status()) {
    case WL_IDLE_STATUS: return F("IDLE");
    case WL_NO_SSID_AVAIL: return F("NO_SSID");
    case WL_SCAN_COMPLETED: return F("SCAN_DONE");
    case WL_CONNECTED: return F("CONNECTED");
    case WL_CONNECT_FAILED: return F("CONNECT_FAILED");
    case WL_CONNECTION_LOST: return F("CONNECTION_LOST");
    case WL_DISCONNECTED: return F("DISCONNECTED");
    default: return F("UNKNOWN");
  }
}

void printHealthStatus() {
  Serial.print(F("Health: WiFi="));
  Serial.print(wifiStatusString());
  Serial.print(F(" | MQTT="));
  Serial.print(mqttClient.connected() ? F("CONNECTED") : F("DISCONNECTED"));
  Serial.print(F(" | relay1="));
  Serial.print(relayState[0] ? F("ON") : F("OFF"));
  Serial.print(F(" | relay2="));
  Serial.print(relayState[1] ? F("ON") : F("OFF"));
  Serial.print(F(" | relay3="));
  Serial.print(relayState[2] ? F("ON") : F("OFF"));
  Serial.println();
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
  Serial.println(F("Input trigger: POSITIVE -> Relay 2 active while D6 stays HIGH"));
  holdRelayActive(1, RELAY2_PIN);
  lastPositiveTriggerAt = now;
}

void fireNegativeInputTrigger(unsigned long now) {
  Serial.println(F("Input trigger: NEGATIVE -> Relay 3 active while D7 stays LOW"));
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

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print(F("Connecting to WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(500);
  }
  Serial.println();
  Serial.print(F("WiFi connected. IP: "));
  Serial.println(WiFi.localIP());
}

void connectMqtt() {
  while (!mqttClient.connected()) {
    String clientId = String(DEVICE_ID) + "-" + String(ESP.getChipId(), HEX);
    if (mqttClient.connect(clientId.c_str())) {
      char topic[96];
      snprintf(topic, sizeof(topic), "haunt/%s/trigger", DEVICE_ID);
      mqttClient.subscribe(topic);
      char statusTopic[96];
      snprintf(statusTopic, sizeof(statusTopic), "haunt/%s/status", DEVICE_ID);
      mqttClient.publish(statusTopic, "online", true);
      Serial.println(F("MQTT connected."));
    } else {
      Serial.print(F("MQTT connect failed. State: "));
      Serial.println(mqttClient.state());
      delay(2000);
    }
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

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(handleMessage);

  printHeader();
  connectWiFi();
}

void loop() {
  readSerialDebugCommands();

  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  handleInputTriggers(now);
  handleIrInput(now);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    Serial.println(F("Heartbeat: node running"));
    printHealthStatus();
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (relayOffAt[i] != 0 && timeReached(now, relayOffAt[i])) {
      setRelay((i == 0) ? RELAY1_PIN : (i == 1) ? RELAY2_PIN : RELAY3_PIN, false);
      relayState[i] = false;
      relayOffAt[i] = 0;
    }
  }
}

  if (timeReached(now, relay2LockoutUntil)) {
    relay2LockoutUntil = 0;
  }

  for (uint8_t i = 0; i < 3; ++i) {
    if (relayOffAt[i] != 0 && static_cast<long>(now - relayOffAt[i]) >= 0) {
      relayOffAt[i] = 0;
    }
  }

  if (relayOffAt[0] == 0) setRelay(RELAY1_PIN, false);
  if (relayOffAt[1] == 0) setRelay(RELAY2_PIN, false);
  if (relayOffAt[2] == 0) setRelay(RELAY3_PIN, relayState[2]);
}