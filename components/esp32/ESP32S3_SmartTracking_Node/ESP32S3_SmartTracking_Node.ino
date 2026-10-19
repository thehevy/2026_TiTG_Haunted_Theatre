#include <Wire.h>
#include <VL53L1X.h>
#include <ESP32Servo.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

/*
  ESP32-S3 Smart Tracking Skull

  This board variant uses the ESP32-S3-WROOM-1 module and keeps the same
  tracking behavior as the standard ESP32 skull design, while moving the GPIOs
  to a safer set for the S3 board.

  Bill of materials:
  - ESP32-S3-WROOM-1 module
  - 2x VL53L1X ToF sensors
  - 1x RC steering servo (powered from a UBEC)
  - 1x DFPlayer Mini + MicroSD card
  - 2x LEDs with 220 ohm resistors
  - 2S LiPo or 7.2V NiMH pack + UBEC

  Wiring notes:
  - All grounds must be common: ESP32-S3, ToFs, servo, DFPlayer, and LED returns.
  - The servo should get its power from a separate UBEC output, not directly from the ESP32 3.3V rail.
  - Keep the DFPlayer RX line short and use a 1k resistor if noise is present.
  - Sensor mounting should be roughly 15-20 degrees outward to improve left/right discrimination.
*/

VL53L1X leftSensor;
VL53L1X rightSensor;
Servo skullServo;
HardwareSerial dfPlayerSerial(2);
DFRobotDFPlayerMini myDFPlayer;

// Tracking state variables
float currentAngle = 90.0;
float targetAngle = 90.0;
unsigned long lastTargetTime = 0;
bool isScaring = false;
bool leftSensorReady = false;
bool rightSensorReady = false;
bool dfPlayerReady = false;

// Configuration constants
const int CENTER_ANGLE = 90;
const int MIN_ANGLE = 30;
const int MAX_ANGLE = 150;
const int COOLDOWN_MS = 3000;
const float EASING_SPEED = 0.18;
const int MIN_DIST = 300;
const int MAX_DIST = 915;

// ESP32-S3-safe pin assignments
const int LEFT_EYE_PIN = 6;
const int RIGHT_EYE_PIN = 7;
const int XSHUT_LEFT = 4;
const int XSHUT_RIGHT = 5;
const int SERVO_PIN = 8;
const int I2C_SDA_PIN = 21;
const int I2C_SCL_PIN = 20;
const int NEGATIVE_TRIGGER_PIN = 18;
const int REMOTE_INIT_LOCKOUT_PIN = 35; // LOW = skip DFPlayer + ToF initialization; GPIO34 is not exposed on this module
const int REMOTE_INIT_RETRY_PIN = 36;   // LOW = retry init later if not already initialized
const int DFPLAYER_RX_PIN = 16;
const int DFPLAYER_TX_PIN = 17;

const unsigned long STATUS_INTERVAL_MS = 5000;
const unsigned long RETRY_INIT_INTERVAL_MS = 10000;
unsigned long lastStatusAt = 0;
unsigned long lastRetryInitAt = 0;
bool lastNegativeTriggerActive = false;

bool isRemoteInitLocked() {
  return digitalRead(REMOTE_INIT_LOCKOUT_PIN) == LOW;
}

bool shouldRetryRemoteInit() {
  return digitalRead(REMOTE_INIT_RETRY_PIN) == LOW;
}

void setEyeState(bool on) {
  digitalWrite(LEFT_EYE_PIN, on ? HIGH : LOW);
  digitalWrite(RIGHT_EYE_PIN, on ? HIGH : LOW);
}

bool initSensors() {
  if (isRemoteInitLocked()) {
    Serial.println(F("Remote init lockout active: skipping VL53L1X initialization."));
    return false;
  }

  leftSensorReady = false;
  rightSensorReady = false;

  pinMode(XSHUT_LEFT, OUTPUT);
  pinMode(XSHUT_RIGHT, OUTPUT);
  digitalWrite(XSHUT_LEFT, LOW);
  digitalWrite(XSHUT_RIGHT, LOW);
  delay(10);

  pinMode(XSHUT_LEFT, INPUT);
  delay(10);
  leftSensor.setTimeout(500);
  if (!leftSensor.init()) {
    Serial.println(F("Left VL53L1X initialization failed; remote sensor may be unplugged."));
    return false;
  }
  leftSensorReady = true;
  leftSensor.setAddress(0x30);

  pinMode(XSHUT_RIGHT, INPUT);
  delay(10);
  rightSensor.setTimeout(500);
  if (!rightSensor.init()) {
    Serial.println(F("Right VL53L1X initialization failed; remote sensor may be unplugged."));
    return false;
  }
  rightSensorReady = true;
  rightSensor.setAddress(0x31);

  leftSensor.startContinuous(50);
  rightSensor.startContinuous(50);
  Serial.println(F("VL53L1X sensors initialized."));
  return true;
}

bool initDFPlayer() {
  if (isRemoteInitLocked()) {
    Serial.println(F("Remote init lockout active: skipping DFPlayer initialization."));
    return false;
  }

  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(dfPlayerSerial)) {
    myDFPlayer.volume(25);
    dfPlayerReady = true;
    Serial.println(F("DFPlayer Mini initialized."));
    return true;
  }

  dfPlayerReady = false;
  Serial.println(F("DFPlayer Mini failed to initialize; remote module may be unplugged."));
  return false;
}

void triggerScare() {
  if (isScaring || !dfPlayerReady) {
    return;
  }

  isScaring = true;
  setEyeState(true);
  myDFPlayer.playFolder(1, 1);
  Serial.println(F("Tracking target: eyes on and scare audio triggered."));
}

void endScare() {
  if (!isScaring) {
    return;
  }

  isScaring = false;
  setEyeState(false);
  if (dfPlayerReady) {
    myDFPlayer.stop();
  }
  Serial.println(F("Target lost: idle state restored."));
}

void printConfig() {
  Serial.println(F("=== Smart Tracking Skull Config ==="));
  Serial.print(F("Center angle: "));
  Serial.println(CENTER_ANGLE);
  Serial.print(F("Min angle: "));
  Serial.println(MIN_ANGLE);
  Serial.print(F("Max angle: "));
  Serial.println(MAX_ANGLE);
  Serial.print(F("Cooldown ms: "));
  Serial.println(COOLDOWN_MS);
  Serial.print(F("Min distance mm: "));
  Serial.println(MIN_DIST);
  Serial.print(F("Max distance mm: "));
  Serial.println(MAX_DIST);
  Serial.print(F("Left eye pin: "));
  Serial.println(LEFT_EYE_PIN);
  Serial.print(F("Right eye pin: "));
  Serial.println(RIGHT_EYE_PIN);
  Serial.print(F("Servo pin: "));
  Serial.println(SERVO_PIN);
  Serial.print(F("I2C SDA pin: "));
  Serial.println(I2C_SDA_PIN);
  Serial.print(F("I2C SCL pin: "));
  Serial.println(I2C_SCL_PIN);
  Serial.print(F("DFPlayer RX pin: "));
  Serial.println(DFPLAYER_RX_PIN);
  Serial.print(F("DFPlayer TX pin: "));
  Serial.println(DFPLAYER_TX_PIN);
  Serial.print(F("Negative trigger pin: "));
  Serial.println(NEGATIVE_TRIGGER_PIN);
  Serial.print(F("Remote init lockout pin: "));
  Serial.println(REMOTE_INIT_LOCKOUT_PIN);
  Serial.print(F("Remote retry init pin: "));
  Serial.println(REMOTE_INIT_RETRY_PIN);
  Serial.print(F("ToF left XSHUT: "));
  Serial.println(XSHUT_LEFT);
  Serial.print(F("ToF right XSHUT: "));
  Serial.println(XSHUT_RIGHT);
  Serial.println(F("================================="));
}

void printStatus(int leftDist, int rightDist, bool leftValid, bool rightValid) {
  Serial.print(F("Status: left="));
  Serial.print(leftDist);
  Serial.print(F("mm right="));
  Serial.print(rightDist);
  Serial.print(F("mm leftValid="));
  Serial.print(leftValid ? F("true") : F("false"));
  Serial.print(F(" rightValid="));
  Serial.print(rightValid ? F("true") : F("false"));
  Serial.print(F(" angle="));
  Serial.print((int)currentAngle);
  Serial.print(F(" target="));
  Serial.print((int)targetAngle);
  Serial.print(F(" scare="));
  Serial.println(isScaring ? F("true") : F("false"));
}

void handleNegativeTrigger() {
  bool negativeTriggerActive = digitalRead(NEGATIVE_TRIGGER_PIN) == LOW;

  if (negativeTriggerActive && !lastNegativeTriggerActive) {
    Serial.println(F("Negative trigger: printing configuration to serial."));
    printConfig();
  }

  lastNegativeTriggerActive = negativeTriggerActive;
}

void setup() {
  Serial.begin(115200);
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  pinMode(LEFT_EYE_PIN, OUTPUT);
  pinMode(RIGHT_EYE_PIN, OUTPUT);
  pinMode(NEGATIVE_TRIGGER_PIN, INPUT_PULLUP);
  pinMode(REMOTE_INIT_LOCKOUT_PIN, INPUT_PULLUP);
  pinMode(REMOTE_INIT_RETRY_PIN, INPUT_PULLUP);
  setEyeState(false);

  skullServo.setPeriodHertz(50);
  skullServo.attach(SERVO_PIN, 500, 2400);
  skullServo.write(CENTER_ANGLE);

  if (!isRemoteInitLocked()) {
    initDFPlayer();
    initSensors();
  } else {
    Serial.println(F("Remote init lockout pin is LOW; skipping DFPlayer and ToF initialization."));
  }

  lastTargetTime = millis();
  printConfig();
  Serial.println(F("ESP32-S3 smart tracking skull initialized."));
}

void loop() {
  if (shouldRetryRemoteInit() && (!dfPlayerReady || !leftSensorReady || !rightSensorReady)) {
    if (millis() - lastRetryInitAt >= RETRY_INIT_INTERVAL_MS) {
      Serial.println(F("Retrying remote DFPlayer/VL53L1X initialization."));
      if (!dfPlayerReady) {
        initDFPlayer();
      }
      if (!leftSensorReady || !rightSensorReady) {
        initSensors();
      }
      lastRetryInitAt = millis();
    }
  }

  int leftDist = leftSensorReady ? leftSensor.read() : -1;
  int rightDist = rightSensorReady ? rightSensor.read() : -1;

  bool leftValid = leftSensorReady && (leftDist >= MIN_DIST && leftDist <= MAX_DIST);
  bool rightValid = rightSensorReady && (rightDist >= MIN_DIST && rightDist <= MAX_DIST);

  handleNegativeTrigger();

  if (leftValid || rightValid) {
    triggerScare();
    lastTargetTime = millis();

    if (leftValid && rightValid) {
      int diff = leftDist - rightDist;
      targetAngle = map(diff, -300, 300, 55, 125);
    } else if (leftValid) {
      targetAngle = 45;
    } else {
      targetAngle = 135;
    }
  } else {
    if (millis() - lastTargetTime > COOLDOWN_MS) {
      targetAngle = CENTER_ANGLE;
      if (isScaring) {
        endScare();
      }
    }
  }

  targetAngle = constrain(targetAngle, MIN_ANGLE, MAX_ANGLE);
  currentAngle += (targetAngle - currentAngle) * EASING_SPEED;
  skullServo.write((int)currentAngle);

  if (millis() - lastStatusAt >= STATUS_INTERVAL_MS) {
    printStatus(leftDist, rightDist, leftValid, rightValid);
    lastStatusAt = millis();
  }

  delay(30);
}
