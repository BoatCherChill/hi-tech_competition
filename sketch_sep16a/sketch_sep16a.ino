#include <Wire.h>  // не нужно, но пусть будет если понадобится
#include <SoftwareSerial.h>

// ==================== ПИНЫ ====================
// Мотор 1 — напрямую
#define M1_IN1 7
#define M1_IN2 6
#define M1_IN3 5
#define M1_IN4 4

// Джойстик
const int pinX  = A0;
const int pinY  = A1;
const int swPin = 8;
const int deadZone = 90;

// ==================== ОТЛАДКА ====================
// SoftwareSerial на D10/D11 для отладки (не обязательно)
// Если не нужна — закомментируй
SoftwareSerial debugSerial(10, 11);

// ==================== ШАГИ ====================
int m1_dl = 2;
unsigned long m1_lastStep = 0;
int m1_stepPhase = 0;
int m1_dir = 0;
int m2_dir = 0;   // для отправки на второй Arduino
int m2_lastSent = 99;  // предыдущее отправленное значение

// ==================== SETUP ====================
void setup() {
  // Аппаратный UART (пины 0/1) — связь со вторым Arduino
  Serial.begin(9600);

  // Отладка (если подключена)
  // debugSerial.begin(9600);

  pinMode(M1_IN1, OUTPUT);
  pinMode(M1_IN2, OUTPUT);
  pinMode(M1_IN3, OUTPUT);
  pinMode(M1_IN4, OUTPUT);

  pinMode(swPin, INPUT_PULLUP);

  stopMotor1();

  // debugSerial.println("Master ready");
}

// ==================== LOOP ====================
void loop() {
  int xOffset = analogRead(pinX) - 512;
  int yOffset = analogRead(pinY) - 512;

  // Vx -> мотор 1 (локально)
  if (xOffset >  deadZone)      m1_dir =  1;
  else if (xOffset < -deadZone) m1_dir = -1;
  else                          m1_dir =  0;

  // Vy -> мотор 2 (отправляем на второй Arduino)
  if (yOffset >  deadZone)      m2_dir =  1;
  else if (yOffset < -deadZone) m2_dir = -1;
  else                          m2_dir =  0;

  // Управляем мотором 1
  if (m1_dir != 0) stepMotor1(m1_dir);
  else             stopMotor1();

  // Отправляем команду для мотора 2 только если направление изменилось
  if (m2_dir != m2_lastSent) {
    m2_lastSent = m2_dir;
    Serial.print("Y:");
    Serial.println(m2_dir);   // "Y:1", "Y:-1", "Y:0"
  }

  // Можно добавить приём статуса от второго Arduino, если нужно
  // while (Serial.available()) { char c = Serial.read(); ... }
}

// ==================== МОТОР 1 ====================
void stepMotor1(int dir) {
  unsigned long now = millis();
  if (now - m1_lastStep < (unsigned long)m1_dl) return;
  m1_lastStep = now;

  m1_stepPhase += dir;
  if (m1_stepPhase > 3) m1_stepPhase = 0;
  if (m1_stepPhase < 0) m1_stepPhase = 3;

  switch (m1_stepPhase) {
    case 0:
      digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, LOW);
      digitalWrite(M1_IN3, LOW);  digitalWrite(M1_IN4, HIGH);
      break;
    case 1:
      digitalWrite(M1_IN1, HIGH); digitalWrite(M1_IN2, HIGH);
      digitalWrite(M1_IN3, LOW);  digitalWrite(M1_IN4, LOW);
      break;
    case 2:
      digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, HIGH);
      digitalWrite(M1_IN3, HIGH); digitalWrite(M1_IN4, LOW);
      break;
    case 3:
      digitalWrite(M1_IN1, LOW);  digitalWrite(M1_IN2, LOW);
      digitalWrite(M1_IN3, HIGH); digitalWrite(M1_IN4, HIGH);
      break;
  }
}

void stopMotor1() {
  digitalWrite(M1_IN1, LOW); digitalWrite(M1_IN2, LOW);
  digitalWrite(M1_IN3, LOW); digitalWrite(M1_IN4, LOW);
}
