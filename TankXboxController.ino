/**
 * TankXboxController.ino
 *
 * ESP32-based RC tank controller driven by an Xbox gamepad over Bluetooth.
 * Controls two DC drive motors (left/right tracks) via a dual H-bridge motor driver,
 * a 180° servo for turret rotation, and a 360° continuous rotation servo for the cannon.
 *
 * Hardware:
 *   - ESP32 microcontroller
 *   - Dual H-bridge motor driver
 *   - Two DC motors (left/right tracks)
 *   - 180° servo motor for turret rotation
 *   - 360° continuous rotation servo motor for cannon
 *   - Xbox Bluetooth controller (via BLEGamepadClient)
 */

#include <Arduino.h>
#include <BLEGamepadClient.h>  // Xbox BLE gamepad library
#include <ESP32Servo.h>        // PWM servo library for ESP32

// ─── Controller ────────────────────────────────────────────────────────────────

XboxController controller;
bool wasConnected = false;  // Tracks previous connection state to detect changes

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
const int MAX_DUTY_CYCLE     = (int)(pow(2, PWM_RESOLUTION) - 1);  // 255

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
const int     TURRET_SPEED_MS    = 30;    // Milliseconds between steps — tuned to absorb BLE latency
const int     TURRET_STEP        = 3;     // Degrees moved per update step

// ─── Cannon Configuration (360° servo) ───────────────────────────────────────────

bool          cannonSpinning         = false;
unsigned long cannonStartTime        = 0;
const unsigned long CANNON_SPIN_DURATION = 1500;  // Auto-stop after 1.5 seconds (ms)

// For a 360° continuous rotation servo, pulse width controls speed and direction:
//   1500 µs = stop, values above/below set speed and spin direction
const int CANNON_STOP_PULSE  = 1500;  // Neutral/stop pulse width (µs)
const int CANNON_SPEED_PULSE = 2000;  // Full-speed pulse width (µs)

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

  controller.begin();
  Serial.println("Searching controller...");
}

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

// ─── Controller Input Callback ───────────────────────────────────────────────────

/**
 * Called by the BLE library whenever gamepad state changes.
 *
 * Control mapping:
 *   Right trigger    → Left track speed
 *   Left trigger     → Right track speed
 *   Y button         → Toggle left track direction (edge-triggered)
 *   B button         → Toggle right track direction (edge-triggered)
 *   D-pad Left/Right → Rotate turret
 *   A button         → Spin cannon (auto-stops after CANNON_SPIN_DURATION ms)
 *   X button         → Emergency stop cannon (kill switch)
 */
void onValueChanged(XboxControlsState &s) {
  // Map trigger values (0.0–1.0) to PWM duty cycle (0–255).
  // Triggers are cross-mapped: right trigger drives left track and vice versa.
  int leftSpeed  = s.leftTrigger  * MAX_DUTY_CYCLE;
  int rightSpeed = s.rightTrigger * MAX_DUTY_CYCLE;

  ledcWriteChannel(LEFT_PWM_CHANNEL,  rightSpeed);
  ledcWriteChannel(RIGHT_PWM_CHANNEL, leftSpeed);

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

  // A button: re-attach the cannon servo pin and start spinning.
  // Auto-stop after CANNON_SPIN_DURATION ms is handled in loop().
  if (s.buttonA && !cannonSpinning) {
    cannonSpinning  = true;
    cannonStartTime = millis();
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

// ─── Main Loop ───────────────────────────────────────────────────────────────────

void loop() {
  // Monitor BLE connection state and register callback on (re)connect
  if (controller.isConnected()) {
    if (!wasConnected) {
      Serial.println("Connected");
      controller.onValueChanged(onValueChanged);
      wasConnected = true;
    }
  } else {
    if (wasConnected) {
      Serial.println("Controller not connected");
      wasConnected = false;
    }
  }

  // Turret movement: non-blocking, time-sliced at TURRET_SPEED_MS intervals.
  // Using millis() avoids blocking the BLE stack with delay().
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

  // Cannon auto-stop: cut signal after CANNON_SPIN_DURATION has elapsed
  if (cannonSpinning && (millis() - cannonStartTime >= CANNON_SPIN_DURATION)) {
    cannonServo.writeMicroseconds(CANNON_STOP_PULSE);
    cannonServo.detach();  // Cut PWM signal to prevent idle jitter on the 360° servo
    cannonSpinning = false;
  }
}

// ─── References ──────────────────────────────────────────────────────────────────
//
// ESP32Servo library:
//   https://github.com/madhephaestus/ESP32Servo
//
// BLEGamepadClient (Xbox BLE support for ESP32):
//   https://github.com/dme86/BLEGamepadClient
//
// ESP32 LEDC (PWM) peripheral documentation:
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/ledc.html
//
// 360° continuous rotation servo signal protocol:
//   1500 µs = stop, < 1500 µs = one direction, > 1500 µs = other direction
//   https://www.adafruit.com/product/154
