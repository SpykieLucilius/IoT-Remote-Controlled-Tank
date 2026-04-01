#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>        

const int IN1 = 18; const int IN2 = 19; const int ENA = 23;
const int IN3 = 16; const int IN4 = 17; const int ENB = 12;
const int LEFT_PWM_CHANNEL = 0; const int RIGHT_PWM_CHANNEL = 1;
const int PWM_FREQ = 500; const int PWM_RESOLUTION = 8;

bool leftDirectionChanged = false; bool isLeftDirectionBackward = false;
bool rightDirectionChanged = false; bool isRightDirectionBackward = false;

Servo turretServo; Servo cannonServo;
const int TURRET_PIN = 14; const int CANNON_PIN = 13; 

int turretAngle = 90;
bool turretTurningLeft = false; bool turretTurningRight = false;
unsigned long lastTurretMove = 0;
const int TURRET_SPEED_MS = 30; const int TURRET_STEP = 3;      

bool cannonSpinning = false;
const int CANNON_STOP_PULSE = 1500; 
const int CANNON_SPEED_PULSE = 2000; 

typedef struct struct_message {
  int leftTrigger;
  int rightTrigger;
  bool buttonY;
  bool buttonB;
  bool dpadLeft;
  bool dpadRight;
  bool buttonA;
  bool buttonX;
} struct_message;

struct_message s;

void changeLeftMotorDirection() {
  if(isLeftDirectionBackward) { digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW); isLeftDirectionBackward = false; }
  else { digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH); isLeftDirectionBackward = true; }
}

void changeRightMotorDirection() {
  if(isRightDirectionBackward) { digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW); isRightDirectionBackward = false; } 
  else { digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH); isRightDirectionBackward = true; }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&s, incomingData, sizeof(s));

  ledcWriteChannel(LEFT_PWM_CHANNEL, s.rightTrigger);
  ledcWriteChannel(RIGHT_PWM_CHANNEL, s.leftTrigger);

  if(s.buttonY) { if(!leftDirectionChanged) { leftDirectionChanged = true; changeLeftMotorDirection(); } } 
  else { leftDirectionChanged = false; }

  if(s.buttonB) { if(!rightDirectionChanged) { rightDirectionChanged = true; changeRightMotorDirection(); } } 
  else { rightDirectionChanged = false; }

  turretTurningLeft = s.dpadLeft;
  turretTurningRight = s.dpadRight;

  if (s.buttonA && !cannonSpinning) {
    cannonSpinning = true;
    if (!cannonServo.attached()) { cannonServo.attach(CANNON_PIN, 1000, 2000); }
    cannonServo.writeMicroseconds(CANNON_SPEED_PULSE);         
  }

  if (s.buttonX) {
    if (cannonServo.attached()) {
      cannonServo.writeMicroseconds(CANNON_STOP_PULSE); 
      cannonServo.detach(); 
    }
    cannonSpinning = false;
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT); pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT); pinMode(ENB, OUTPUT);
  digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
  ledcAttachChannel(ENA, PWM_FREQ, PWM_RESOLUTION, LEFT_PWM_CHANNEL);
  ledcAttachChannel(ENB, PWM_FREQ, PWM_RESOLUTION, RIGHT_PWM_CHANNEL);
  
  ESP32PWM::allocateTimer(2); ESP32PWM::allocateTimer(3);
  turretServo.setPeriodHertz(50); cannonServo.setPeriodHertz(50);
  turretServo.attach(TURRET_PIN, 500, 2400); 
  cannonServo.attach(CANNON_PIN, 1000, 2000); 
  cannonServo.writeMicroseconds(CANNON_STOP_PULSE);
  delay(50);
  cannonServo.detach();
  turretServo.write(turretAngle); 

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

void loop() {
  if (millis() - lastTurretMove >= TURRET_SPEED_MS) {
    if (turretTurningLeft && turretAngle > 0) {
      turretAngle = max(0, turretAngle - TURRET_STEP);
      turretServo.write(turretAngle);
    } else if (turretTurningRight && turretAngle < 180) {
      turretAngle = min(180, turretAngle + TURRET_STEP);
      turretServo.write(turretAngle);
    }
    lastTurretMove = millis();
  }
}