/**
 * TankESPController.ino
 *
 * ESP32-based RC tank receiver driven by a physical button controller over ESP-NOW.
 * This is the tank-side counterpart to ControllerCode.ino.
 *
 * Receives a control struct via ESP-NOW and applies it to:
 *   - Two DC drive motors (left/right tracks) via a dual H-bridge motor driver
 *   - A 180° servo motor for turret rotation
 *   - A 360° continuous rotation servo motor for the cannon
 *
 * This sketch shares the same motor/turret/cannon logic as TankXboxController.ino,
 * but uses ESP-NOW instead of a Bluetooth gamepad as the input source.
 *
 * Hardware:
 *   - ESP32 microcontroller (tank side)
 *   - Dual H-bridge motor driver
 *   - Two DC motors (left/right tracks)
 *   - 180° servo motor for turret rotation
 *   - 360° continuous rotation servo motor for cannon
 *   - Second ESP32 running ControllerCode.ino (controller side)
 */

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <ESP32Servo.h>  // PWM servo library for ESP32

// ─── Motor Driver Pins (dual H-bridge) ──────────────────────────────────────────

// Left track motor
const int IN1 = 18;  // Direction pin A
const int IN2 = 19;  // Direction pin B
const int ENA = 23;  // PWM enable pin

// Right track motor
const int IN3 = 16;  // Direction pin A
const int IN4 = 17;  // Direction pin B
const int ENB = 12;  // PWM enable pin

// ─── PWM Configuration ──────────────────────────────────────────────────────────

const int LEFT_PWM_CHANNEL  = 0;
const int RIGHT_PWM_CHANNEL = 1;
const int PWM_FREQ           = 500;  // Hz
const int PWM_RESOLUTION     = 8;    // Bits (0–255 duty cycle range)

// ─── Motor Direction State ───────────────────────────────────────────────────────

// Edge-detection flags prevent direction from toggling repeatedly while a button is held
bool leftDirectionChanged     = false;
bool isLeftDirectionBackward  = false;

bool rightDirectionChanged    = false;
bool isRightDirectionBackward = false;

// ─── Servo Instances ─────────────────────────────────────────────────────────────

Servo turretServo;  // 180° servo — controls turret rotation
Servo cannonServo;  // 360° continuous rotation servo — drives the cannon

// ─── Turret Configuration ────────────────────────────────────────────────────────

const int TURRET_PIN = 14;
const int CANNON_PIN = 13;

int           turretAngle        = 90;    // Starting position: center (90°)
bool          turretTurningLeft  = false;
bool          turretTurningRight = false;
unsigned long lastTurretMove     = 0;
const int     TURRET_SPEED_MS    = 30;    // Milliseconds between steps
const int     TURRET_STEP        = 3;     // Degrees moved per update step

// ─── Cannon Configuration (360° servo) ───────────────────────────────────────────

bool      cannonSpinning     = false;
const int CANNON_STOP_PULSE  = 1500;  // Neutral/stop pulse width (µs)
const int CANNON_SPEED_PULSE = 2000;  // Full-speed pulse width (µs)

// ─── Shared Data Structure ───────────────────────────────────────────────────────

// Must match the struct definition in ControllerCode.ino exactly.
typedef struct struct_message {
  int  leftTrigger;   // Left track speed
  int  rightTrigger;  // Right track speed
  bool buttonY;       // Toggle left track direction
  bool buttonB;       // Toggle right track direction
  bool dpadLeft;      // Turret rotate left
  bool dpadRight;     // Turret rotate right
  bool buttonA;       // Fire cannon
  bool buttonX;       // Emergency stop cannon
} struct_message;

struct_message s;  // Holds the latest received controller state

// ─── Motor Direction Helpers ─────────────────────────────────────────────────────

/**
 * Toggles the left track motor direction by flipping the H-bridge control pins.
 */
void changeLeftMotorDirection() {
  if (isLeftDirectionBackward) {
    digitalWrite(IN1, HIGH);
    digitalWrite(IN2, LOW);
    isLeftDirectionBackward = false;
  } else {
    digitalWrite(IN1, LOW);
    digitalWrite(IN2, HIGH);
    isLeftDirectionBackward = true;
  }
}

/**
 * Toggles the right track motor direction by flipping the H-bridge control pins.
 */
void changeRightMotorDirection() {
  if (isRightDirectionBackward) {
    digitalWrite(IN3, HIGH);
    digitalWrite(IN4, LOW);
    isRightDirectionBackward = false;
  } else {
    digitalWrite(IN3, LOW);
    digitalWrite(IN4, HIGH);
    isRightDirectionBackward = true;
  }
}

// ─── ESP-NOW Receive Callback ────────────────────────────────────────────────────

/**
 * Called by the ESP-NOW stack each time a packet arrives from the controller.
 * Copies the raw bytes into the control struct and applies all actuator commands.
 *
 * Control mapping:
 *   leftTrigger / rightTrigger → Track speeds (cross-mapped, see below)
 *   buttonY                    → Toggle left track direction (edge-triggered)
 *   buttonB                    → Toggle right track direction (edge-triggered)
 *   dpadLeft / dpadRight       → Turret rotation direction flags
 *   buttonA                    → Spin cannon (360° servo)
 *   buttonX                    → Emergency stop cannon (kill switch)
 */
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&s, incomingData, sizeof(s));

  Serial.println("Signal received!");

  // Apply track speeds — triggers are cross-mapped to compensate for motor wiring
  ledcWriteChannel(LEFT_PWM_CHANNEL,  s.rightTrigger);
  ledcWriteChannel(RIGHT_PWM_CHANNEL, s.leftTrigger);

  // Y button: toggle left track direction (fires once per press, not continuously)
  if (s.buttonY) {
    if (!leftDirectionChanged) {
      leftDirectionChanged = true;
      changeLeftMotorDirection();
    }
  } else {
    leftDirectionChanged = false;
  }

  // B button: toggle right track direction (fires once per press, not continuously)
  if (s.buttonB) {
    if (!rightDirectionChanged) {
      rightDirectionChanged = true;
      changeRightMotorDirection();
    }
  } else {
    rightDirectionChanged = false;
  }

  // D-pad sets turret rotation direction flags; movement is applied in loop()
  turretTurningLeft  = s.dpadLeft;
  turretTurningRight = s.dpadRight;

  // A button: re-attach the cannon servo pin and start spinning
  if (s.buttonA && !cannonSpinning) {
    cannonSpinning = true;
    if (!cannonServo.attached()) {
      cannonServo.attach(CANNON_PIN, 1000, 2000);
    }
    cannonServo.writeMicroseconds(CANNON_SPEED_PULSE);
  }

  // X button: unconditional emergency stop — immediately cuts signal to the cannon servo
  if (s.buttonX) {
    if (cannonServo.attached()) {
      cannonServo.writeMicroseconds(CANNON_STOP_PULSE);
      cannonServo.detach();  // Physically cuts the PWM signal
    }
    cannonSpinning = false;
  }
}

// ─── Setup ───────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // Configure left motor direction pins
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);

  // Configure right motor direction pins
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  // Set initial forward direction for both motors.
  // The inverted logic (LOW/HIGH vs HIGH/LOW) compensates for reversed motor wiring.
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  // Attach PWM channels to motor enable pins
  ledcAttachChannel(ENA, PWM_FREQ, PWM_RESOLUTION, LEFT_PWM_CHANNEL);
  ledcAttachChannel(ENB, PWM_FREQ, PWM_RESOLUTION, RIGHT_PWM_CHANNEL);

  // Allocate hardware timers for servo PWM generation
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  turretServo.setPeriodHertz(50);  // Standard 50 Hz servo signal
  cannonServo.setPeriodHertz(50);

  // Attach the 180° turret servo with full 500–2400 µs range
  turretServo.attach(TURRET_PIN, 500, 2400);

  // Initialize the 360° cannon servo: send a stop pulse then detach to cut the signal.
  // This ensures the servo is properly stopped at startup without unintended movement.
  cannonServo.attach(CANNON_PIN, 1000, 2000);
  cannonServo.writeMicroseconds(CANNON_STOP_PULSE);
  delay(50);
  cannonServo.detach();

  // Center the turret on startup
  turretServo.write(turretAngle);

  // ESP-NOW requires Wi-Fi to be initialized in station mode
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW init failed");
    return;
  }

  // Register the receive callback — all incoming packets are handled here
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}

// ─── Main Loop ───────────────────────────────────────────────────────────────────

void loop() {
  // Turret movement: non-blocking, time-sliced at TURRET_SPEED_MS intervals.
  // Direction flags are set in OnDataRecv(); movement is applied here to avoid
  // blocking the ESP-NOW receive stack.
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

// ─── References ──────────────────────────────────────────────────────────────────
//
// ESP32Servo library:
//   https://github.com/madhephaestus/ESP32Servo
//
// ESP-NOW API documentation:
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
//
// Arduino ESP-NOW getting started guide:
//   https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/
//
// ESP32 LEDC (PWM) peripheral documentation:
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html
//
// 360° continuous rotation servo signal protocol:
//   1500 µs = stop, < 1500 µs = one direction, > 1500 µs = other direction
//   https://www.adafruit.com/product/154
