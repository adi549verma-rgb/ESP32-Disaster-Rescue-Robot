#include <Wire.h>
#include "BluetoothSerial.h"

BluetoothSerial SerialBT;

// ===== MOTOR PINS =====
#define IN1 26
#define IN2 27
#define IN3 14
#define IN4 12
#define MOTOR_SPEED 100

// ===== ULTRASONIC PINS =====
#define TRIG_F 5
#define ECHO_F 18
#define TRIG_R 4
#define ECHO_R 2

// ===== LED & BUZZER =====
#define LED 33
#define BUZZER 25

// ===== SCD40 =====
#define SCD40_ADDR 0x62

uint16_t co2;
float temperature, humidity;
char cmd;
unsigned long lastSensorTime = 0;

void sendCommand(uint16_t cmdData) {
  Wire.beginTransmission(SCD40_ADDR);
  Wire.write(cmdData >> 8);
  Wire.write(cmdData & 0xFF);
  Wire.endTransmission();
}

bool readData(uint16_t &co2, float &temp, float &hum) {
  Wire.requestFrom(SCD40_ADDR, 9);
  if (Wire.available() != 9) return false;

  uint8_t data[9];
  for (int i = 0; i < 9; i++) data[i] = Wire.read();

  co2 = (data[0] << 8) | data[1];
  uint16_t rawTemp = (data[3] << 8) | data[4];
  uint16_t rawHum  = (data[6] << 8) | data[7];

  temp = -45 + 175 * ((float)rawTemp / 65535.0);
  hum  = 100 * ((float)rawHum / 65535.0);
  return true;
}

int getDistance(int trigPin, int echoPin) {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000);
  int distance = duration * 0.034 / 2;
  if (distance <= 0 || distance > 400) distance = 400;
  return distance;
}

void forward() {
  analogWrite(IN1, MOTOR_SPEED); analogWrite(IN2, 0);
  analogWrite(IN3, MOTOR_SPEED); analogWrite(IN4, 0);
}

void backward() {
  analogWrite(IN1, 0); analogWrite(IN2, MOTOR_SPEED);
  analogWrite(IN3, 0); analogWrite(IN4, MOTOR_SPEED);
}

void left() {
  analogWrite(IN1, 0); analogWrite(IN2, MOTOR_SPEED);
  analogWrite(IN3, MOTOR_SPEED); analogWrite(IN4, 0);
}

void right() {
  analogWrite(IN1, MOTOR_SPEED); analogWrite(IN2, 0);
  analogWrite(IN3, 0); analogWrite(IN4, MOTOR_SPEED);
}

void stopMotor() {
  analogWrite(IN1, 0); analogWrite(IN2, 0);
  analogWrite(IN3, 0); analogWrite(IN4, 0);
}

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32_ROBOT");

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(TRIG_F, OUTPUT); pinMode(ECHO_F, INPUT);
  pinMode(TRIG_R, OUTPUT); pinMode(ECHO_R, INPUT);
  pinMode(LED, OUTPUT); pinMode(BUZZER, OUTPUT);

  Wire.begin(21, 22);
  delay(1000);

  sendCommand(0x21B1); // Start SCD40 periodic measurement
  delay(5000);

  stopMotor();
}

void loop() {
  while (SerialBT.available()) {
    cmd = SerialBT.read();
    switch (cmd) {
      case 'F': forward(); break;
      case 'B': backward(); break;
      case 'L': left(); break;
      case 'R': right(); break;
      case 'S': stopMotor(); break;
    }
  }

  if (millis() - lastSensorTime >= 4000) {
    lastSensorTime = millis();

    int frontDist = getDistance(TRIG_F, ECHO_F);
    delay(20);
    int rightDist = getDistance(TRIG_R, ECHO_R);

    sendCommand(0xEC05);
    delay(10);
    bool scdOK = readData(co2, temperature, humidity);

    if (co2 > 1700) {
      digitalWrite(LED, HIGH); digitalWrite(BUZZER, HIGH);
      SerialBT.println("HUMAN DETECTED");
    } else if (frontDist < 15) {
      digitalWrite(LED, LOW); digitalWrite(BUZZER, LOW);
      SerialBT.println("OBSTACLE DETECTED");
    } else {
      digitalWrite(LED, LOW); digitalWrite(BUZZER, LOW);
    }

    SerialBT.print("Front: "); SerialBT.print(frontDist); SerialBT.println(" cm");
    SerialBT.print("Right: "); SerialBT.print(rightDist); SerialBT.println(" cm");

    if (scdOK) {
      SerialBT.print("CO2: ");  SerialBT.print(co2);         SerialBT.println(" ppm");
      SerialBT.print("Temp: "); SerialBT.print(temperature); SerialBT.println(" C");
      SerialBT.print("Hum: ");  SerialBT.print(humidity);    SerialBT.println(" %");
    }
  }
}
