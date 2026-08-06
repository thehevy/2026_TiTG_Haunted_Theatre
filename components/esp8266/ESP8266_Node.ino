#include <ESP8266WiFi.h>
#include <PubSubClient.h>

constexpr uint8_t RELAY1_PIN = D1;
constexpr uint8_t RELAY2_PIN = D2;
constexpr uint8_t RELAY3_PIN = D5;
constexpr bool RELAY_ACTIVE_LOW = true;

const char *WIFI_SSID = "YOUR_WIFI_SSID";
const char *WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char *MQTT_HOST = "192.168.1.10";
constexpr uint16_t MQTT_PORT = 1883;
const char *DEVICE_ID = "esp8266-node-01";

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool relayState[3] = {false, false, false};
unsigned long relayOffAt[3] = {0, 0, 0};

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

void handleMessage(char *topic, byte *payload, unsigned int length) {
  (void)topic;

  char message[64];
  unsigned int copyLength = length < sizeof(message) - 1 ? length : sizeof(message) - 1;
  memcpy(message, payload, copyLength);
  message[copyLength] = '\0';

  if (strcmp(message, "relay1:pulse") == 0) {
    pulseRelay(0, RELAY1_PIN, 250);
  } else if (strcmp(message, "relay2:pulse") == 0) {
    pulseRelay(1, RELAY2_PIN, 250);
  } else if (strcmp(message, "relay3:toggle") == 0) {
    toggleRelay(2, RELAY3_PIN);
  }
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
  pinMode(RELAY1_PIN, OUTPUT);
  pinMode(RELAY2_PIN, OUTPUT);
  pinMode(RELAY3_PIN, OUTPUT);

  setRelay(RELAY1_PIN, false);
  setRelay(RELAY2_PIN, false);
  setRelay(RELAY3_PIN, false);

  connectWiFi();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCallback(handleMessage);
}

void loop() {
  if (!mqttClient.connected()) {
    connectMqtt();
  }
  mqttClient.loop();

  unsigned long now = millis();
  for (uint8_t i = 0; i < 3; ++i) {
    if (relayOffAt[i] != 0 && static_cast<long>(now - relayOffAt[i]) >= 0) {
      relayOffAt[i] = 0;
    }
  }

  if (relayOffAt[0] == 0) setRelay(RELAY1_PIN, false);
  if (relayOffAt[1] == 0) setRelay(RELAY2_PIN, false);
  if (relayOffAt[2] == 0) setRelay(RELAY3_PIN, relayState[2]);
}