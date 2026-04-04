/**
 * ControllerCode.ino
 *
 * ESP32-based physical button controller for the RC tank.
 * Reads a set of digital input buttons (with internal pull-up resistors),
 * packs their state into a struct, and transmits it to the tank over ESP-NOW.
 *
 * This sketch runs on the handheld controller board, not on the tank.
 * The tank's MAC address must be set in broadcastAddress below.
 * Use MACAdressFetch.ino to obtain it if needed.
 *
 * Hardware:
 *   - ESP32 microcontroller (controller side)
 *   - 8 momentary push buttons connected to GPIO pins (active LOW via INPUT_PULLUP)
 *
 * Control mapping:
 *   Pin 32 → Left track forward
 *   Pin 33 → Right track forward
 *   Pin 26 → Toggle left track direction
 *   Pin 25 → Toggle right track direction
 *   Pin 14 → Turret rotate left
 *   Pin 27 → Turret rotate right
 *   Pin 12 → Fire cannon (A)
 *   Pin 13 → Emergency stop cannon (X)
 */

#include <esp_now.h>
#include <WiFi.h>

// ─── Target Address ───────────────────────────────────────────────────────────────

// MAC address of the tank's ESP32 (Wi-Fi STA interface).
// Update this value using the address printed by MACAdressFetch.ino.
uint8_t broadcastAddress[] = {0x78, 0xE3, 0x6D, 0x11, 0x70, 0xC4};

// ─── Shared Data Structure ───────────────────────────────────────────────────────

// Must match the struct definition in TankESPController.ino exactly,
// as ESP-NOW transmits raw bytes with no schema negotiation.
typedef struct struct_message {
  int  leftTrigger;   // Left track speed  (0 or FIXED_SPEED)
  int  rightTrigger;  // Right track speed (0 or FIXED_SPEED)
  bool buttonY;       // Toggle left track direction
  bool buttonB;       // Toggle right track direction
  bool dpadLeft;      // Turret rotate left
  bool dpadRight;     // Turret rotate right
  bool buttonA;       // Fire cannon
  bool buttonX;       // Emergency stop cannon
} struct_message;

struct_message myData;
esp_now_peer_info_t peerInfo;

// ─── Speed Configuration ─────────────────────────────────────────────────────────

// Physical buttons have no analog range, so full speed (255) is sent when pressed.
const int FIXED_SPEED = 255;

// ─── Setup ───────────────────────────────────────────────────────────────────────

void setup() {
  Serial.begin(115200);

  // ESP-NOW requires Wi-Fi to be initialized in station mode
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  // Register the tank as an ESP-NOW peer
  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;      // 0 = use current Wi-Fi channel
  peerInfo.encrypt = false;  // No encryption for low-latency control
  esp_now_add_peer(&peerInfo);

  // Configure all button pins as inputs with internal pull-up resistors.
  // Buttons are active LOW: pressed = LOW (reads 0), released = HIGH (reads 1).
  pinMode(32, INPUT_PULLUP);  // Left track forward
  pinMode(33, INPUT_PULLUP);  // Right track forward
  pinMode(26, INPUT_PULLUP);  // Toggle left track direction
  pinMode(25, INPUT_PULLUP);  // Toggle right track direction
  pinMode(27, INPUT_PULLUP);  // Turret rotate right
  pinMode(14, INPUT_PULLUP);  // Turret rotate left
  pinMode(12, INPUT_PULLUP);  // Fire cannon (A)
  pinMode(13, INPUT_PULLUP);  // Emergency stop cannon (X)
}

// ─── Main Loop ───────────────────────────────────────────────────────────────────

void loop() {
  // Read each button and map to the message struct.
  // The ! operator inverts the active-LOW logic: pressed = true, released = false.
  myData.leftTrigger  = !digitalRead(32) ? FIXED_SPEED : 0;
  myData.rightTrigger = !digitalRead(33) ? FIXED_SPEED : 0;

  myData.buttonY = !digitalRead(26);
  myData.buttonB = !digitalRead(25);

  myData.dpadLeft  = !digitalRead(14);
  myData.dpadRight = !digitalRead(27);

  myData.buttonA = !digitalRead(12);
  myData.buttonX = !digitalRead(13);

  // Broadcast the current button state to the tank
  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));

  delay(20);  // ~50 Hz transmission rate
}

// ─── References ──────────────────────────────────────────────────────────────────
//
// ESP-NOW API documentation:
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
//
// Arduino ESP-NOW getting started guide:
//   https://randomnerdtutorials.com/esp-now-esp32-arduino-ide/
//
// INPUT_PULLUP explained (active-LOW button wiring):
//   https://docs.arduino.cc/learn/microcontrollers/digital-pins/
