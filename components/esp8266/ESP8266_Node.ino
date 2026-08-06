#include <ESP8266WiFi.h>
#include <PubSubClient.h>

constexpr uint8_t RELAY1_PIN = D1;
constexpr uint8_t RELAY2_PIN = D2;
constexpr uint8_t RELAY3_PIN = D5;
constexpr uint8_t POSITIVE_TRIGGER_PIN = D6;
constexpr uint8_t NEGATIVE_TRIGGER_PIN = D7;
constexpr bool RELAY_ACTIVE_LOW = true;
constexpr unsigned long PULSE_MS = 250;
constexpr unsigned long LOCKOUT_MS = 15000;
constexpr unsigned long INPUT_DEBOUNCE_MS = 50;

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *MQTT_HOST = "192.168.1.10";
constexpr uint16_t MQTT_PORT = 1883;
const char *DEVICE_ID = "esp8266-node-01";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool relayState[3] = {false, false, false};
unsigned long relayOffAt[3] = {0, 0, 0};
unsigned long relay2LockoutUntil = 0;
unsigned long lastPositiveTriggerAt = 0;
unsigned long lastNegativeTriggerAt = 0;
bool lastPositiveActive = false;
bool lastNegativeActive = false;
unsigned long lastHeartbeatAt = 0;

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

bool timeReached(unsigned long now, unsigned long target) {
  return target != 0 && static_cast<long>(now - target) >= 0;
}

void printHeader() {
  Serial.println(F("=== ESP8266 Node ==="));
  Serial.println(F("Purpose: MQTT and local trigger relay controller"));
  Serial.print(F("Relay 1 pulse pin: "));
  Serial.println(RELAY1_PIN);
  Serial.print(F("Relay 2 pulse+lockout pin: "));
  Serial.println(RELAY2_PIN);
  Serial.print(F("Relay 3 toggle pin: "));
  Serial.println(RELAY3_PIN);
  Serial.print(F("Positive trigger input (active HIGH): "));
  Serial.println(POSITIVE_TRIGGER_PIN);
  Serial.print(F("Negative trigger input (active LOW): "));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.println(F("Ready."));
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
    if (relay2LockoutUntil == 0) {
      pulseRelay(1, RELAY2_PIN, PULSE_MS);
      relay2LockoutUntil = millis() + LOCKOUT_MS;
    }
  } else if (strcmp(message, "relay3:toggle") == 0) {
    toggleRelay(2, RELAY3_PIN);
  }
}

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

void connectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
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
    } else {
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

  connectWiFi();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(handleMessage);
  printHeader();
}

void loop() {
  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  handleInputTriggers(now);

  if (now - lastHeartbeatAt >= 5000) {
    lastHeartbeatAt = now;
    Serial.println(F("Heartbeat: node running"));
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