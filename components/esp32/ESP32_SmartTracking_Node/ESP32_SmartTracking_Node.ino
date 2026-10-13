#include <Wire.h>
#include <VL53L1X.h>
#include <ESP32Servo.h>
#include <HardwareSerial.h>
#include <DFRobotDFPlayerMini.h>

/*
  Smart Tracking Skull for ESP32

  Bill of materials:
  - Microcontroller: ESP32 Dev Module
  - Sensors: 2x VL53L1X ToF sensors
  - Actuator: 1/10-scale RC steering servo
  - Audio: DFPlayer Mini MP3 module + MicroSD card (FAT32, folder 01/001.mp3)
  - Visuals: 2x 5V LEDs with 220 ohm resistors
  - Power: 2S LiPo (7.4V) or 7.2V NiMH pack with UBEC regulated to 6V for the servo
  - Hardware: breadboard, jumper wires, angle brackets, and sensor mounts angled 15-20 degrees outward

  Important wiring notes:
  - The two ToF sensors share the same I2C bus on SDA=GPIO21 and SCL=GPIO22.
  - Use XSHUT pins to hard-reset the sensors and then re-address them to 0x30 and 0x31.
  - All grounds must be tied together: ESP32 ground, DFPlayer ground, servo ground, and both ToF grounds.
  - The Servo power should come from the UBEC regulated output, not directly from the ESP32 5V rail.
  - The DFPlayer RX line should be conditioned with a 1k resistor in noisy layouts.
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

// Configuration settings
const int CENTER_ANGLE = 90;
const int COOLDOWN_MS = 3000;
const float EASING_SPEED = 0.15;

// Distance thresholds in millimeters (300mm = ~1 foot, 915mm = ~3 feet)
const int MIN_DIST = 300;
const int MAX_DIST = 915;

// Pin assignments - ESP32-safe pins for a dev board
const int LEFT_EYE_PIN = 25;
const int RIGHT_EYE_PIN = 26;
const int XSHUT_LEFT = 4;
const int XSHUT_RIGHT = 5;
const int SERVO_PIN = 13;
const int DFPLAYER_RX_PIN = 16;
const int DFPLAYER_TX_PIN = 17;

void setEyeState(bool on) {
  digitalWrite(LEFT_EYE_PIN, on ? HIGH : LOW);
  digitalWrite(RIGHT_EYE_PIN, on ? HIGH : LOW);
}

void initSensors() {
  pinMode(XSHUT_LEFT, OUTPUT);
  pinMode(XSHUT_RIGHT, OUTPUT);
  digitalWrite(XSHUT_LEFT, LOW);
  digitalWrite(XSHUT_RIGHT, LOW);
  delay(10);

  pinMode(XSHUT_LEFT, INPUT);
  delay(10);
  leftSensor.setTimeout(500);
  if (!leftSensor.init()) {
    Serial.println(F("Left VL53L1X initialization failed."));
    while (1) {
      delay(10);
    }
  }
  leftSensor.setAddress(0x30);

  pinMode(XSHUT_RIGHT, INPUT);
  delay(10);
  rightSensor.setTimeout(500);
  if (!rightSensor.init()) {
    Serial.println(F("Right VL53L1X initialization failed."));
    while (1) {
      delay(10);
    }
  }
  rightSensor.setAddress(0x31);

  leftSensor.startContinuous(50);
  rightSensor.startContinuous(50);
}

void initDFPlayer() {
  dfPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  if (myDFPlayer.begin(dfPlayerSerial)) {
    myDFPlayer.volume(25);
    Serial.println(F("DFPlayer Mini initialized."));
  } else {
    Serial.println(F("DFPlayer Mini failed to initialize."));
  }
}

void triggerScare() {
  if (isScaring) {
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
  myDFPlayer.stop();
  Serial.println(F("Target lost: idle state restored."));
}

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(LEFT_EYE_PIN, OUTPUT);
  pinMode(RIGHT_EYE_PIN, OUTPUT);
  setEyeState(false);

  skullServo.attach(SERVO_PIN);
  skullServo.write(CENTER_ANGLE);

  initDFPlayer();
  initSensors();

  lastTargetTime = millis();
  Serial.println(F("Smart tracking skull initialized."));
}

void loop() {
  int leftDist = leftSensor.read();
  int rightDist = rightSensor.read();

  bool leftValid = (leftDist >= MIN_DIST && leftDist <= MAX_DIST);
  bool rightValid = (rightDist >= MIN_DIST && rightDist <= MAX_DIST);

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

  targetAngle = constrain(targetAngle, 30, 150);
  currentAngle += (targetAngle - currentAngle) * EASING_SPEED;
  skullServo.write((int)currentAngle);

  delay(30);
}
