constexpr uint8_t IR_SENSOR_PIN = 2;    // GPIO2 on ESP-01, active HIGH
constexpr uint8_t RELAY_PIN = 0;        // GPIO0 on ESP-01, active LOW relay driver
constexpr bool RELAY_ACTIVE_LOW = true;
constexpr uint32_t TRIGGER_PULSE_MS = 1000;
constexpr uint32_t COOLDOWN_MS = 15000;
constexpr uint32_t DEBOUNCE_MS = 25;

bool relayOn = false;
bool lastSensorState = false;
unsigned long relayOffAt = 0;
unsigned long lastTriggerAt = 0;
unsigned long lastRawEdgeAt = 0;

void setRelay(bool on) {
  relayOn = on;
  digitalWrite(RELAY_PIN, RELAY_ACTIVE_LOW ? (on ? LOW : HIGH) : (on ? HIGH : LOW));
}

void pulseRelay() {
  setRelay(true);
  relayOffAt = millis() + TRIGGER_PULSE_MS;
  Serial.println(F("IR trigger detected: relay pulse for 1 second"));
}

void setup() {
  Serial.begin(115200);
  delay(100);

  pinMode(IR_SENSOR_PIN, INPUT);
  pinMode(RELAY_PIN, OUTPUT);
  setRelay(false);

  lastSensorState = (digitalRead(IR_SENSOR_PIN) == HIGH);
  Serial.println(F("ESP-01 IR trigger node ready"));
  Serial.println(F("Sensor input: GPIO2 active HIGH"));
  Serial.println(F("Relay output: GPIO0 pulse for 1 second"));
  Serial.print(F("Cooldown: "));
  Serial.print(COOLDOWN_MS / 1000);
  Serial.println(F(" seconds"));
}

void loop() {
  unsigned long now = millis();
  bool sensorState = (digitalRead(IR_SENSOR_PIN) == HIGH);

  if (sensorState && !lastSensorState) {
    lastRawEdgeAt = now;
  }

  if (sensorState && !lastSensorState && (now - lastRawEdgeAt >= DEBOUNCE_MS)) {
    if (now - lastTriggerAt >= COOLDOWN_MS) {
      pulseRelay();
      lastTriggerAt = now;
    } else {
      Serial.println(F("Trigger ignored: cooldown active"));
    }
  }

  if (relayOffAt != 0 && now >= relayOffAt) {
    setRelay(false);
    relayOffAt = 0;
  }

  lastSensorState = sensorState;
  delay(10);
}
