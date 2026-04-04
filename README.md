# IoT Remote-Controlled Tank

A 3D-printed remote-controlled tank featuring a traversing turret and a shooting cannon, built around the ESP32 microcontroller. The tank supports two independent control modes: an Xbox gamepad over Bluetooth, and a custom hand-built button controller communicating over ESP-NOW.

---

## Features

- **Tracked drive** — two independent DC motors (left/right tracks) allow differential steering and neutral turns
- **Traversing turret** — 180° servo rotates the turret left and right
- **Shooting cannon** — 360° continuous rotation servo launches ball projectiles
- **Range-finding laser** — mounted on the turret for targeting assistance
- **Dual control modes** — Xbox BLE gamepad or a custom ESP-NOW button controller
- **Screw-based assembly** — all 3D-printed parts are fastened with screws rather than glue, making the tank easy to disassemble for repairs or upgrades

---

## Hardware

| Component | Role |
|---|---|
| ESP32 (×2) | Main controller (tank) + controller board (ESP-NOW mode) |
| Dual H-bridge motor driver | Powers the two DC track motors |
| 2× DC motors | Left and right track drive |
| 180° servo motor | Turret rotation |
| 360° continuous rotation servo | Cannon firing mechanism |
| 8× momentary push buttons | Physical ESP-NOW controller inputs |
| Laser module | Range-finding / targeting |
| 3D-printed chassis | Rover body and turret (sourced from Thingiverse + custom mounting plate) |

---

## Repository Structure

| File | Description |
|---|---|
| `TankXboxController.ino` | Tank firmware — Xbox BLE gamepad input |
| `TankESPController.ino` | Tank firmware — ESP-NOW input (physical controller) |
| `ControllerCode.ino` | Controller firmware — reads buttons and sends state over ESP-NOW |
| `MACAdressFetch.ino` | Utility sketch — prints the ESP32's MAC address for ESP-NOW pairing |

---

## Control Schemes

### Xbox Controller (Bluetooth)

| Input | Action |
|---|---|
| Right trigger | Left track speed |
| Left trigger | Right track speed |
| Y button | Toggle left track direction |
| B button | Toggle right track direction |
| D-pad Left / Right | Rotate turret |
| A button | Fire cannon (auto-stops after 1.5 s) |
| X button | Emergency stop cannon |

### Physical Button Controller (ESP-NOW)

The hand-built controller replicates the same control mapping using eight labeled push buttons wired to an ESP32. It communicates with the tank via ESP-NOW, which proved faster and more power-efficient than the Bluetooth alternative.

---

## Getting Started

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) with ESP32 board support
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) library
- [BLEGamepadClient](https://github.com/dme86/BLEGamepadClient) library (Xbox mode only)

### Setup

1. **Find the tank's MAC address** — flash `MACAdressFetch.ino` onto the tank's ESP32, open the Serial Monitor at 115200 baud, and note the printed address.
2. **Update the controller** — paste the MAC address into the `broadcastAddress` field in `ControllerCode.ino`.
3. **Flash the tank** — upload either `TankXboxController.ino` (Xbox) or `TankESPController.ino` (ESP-NOW) to the tank's ESP32.
4. **Flash the controller** *(ESP-NOW mode only)* — upload `ControllerCode.ino` to the controller ESP32.

---

## 3D Printing

The chassis is based on pre-made designs from Thingiverse. A custom mounting plate was designed from scratch to join the rover body and turret assembly. Parts were printed on a Bambu Lab A1 Mini.

| Model | Source |
|---|---|
| Tracked rover body | [Thingiverse #3112734](https://www.thingiverse.com/thing:3112734) |
| Cannon / turret assembly | [Thingiverse #2856707](https://www.thingiverse.com/thing:2856707) |
| Mounting plate | Custom design |

---

## Authors

- Elias Jungman
- Lucas Martins Celeiro
- Andrei Muzhev
- Alexander Soprych

Project developed as part of the *3D + Robotics* course at Haaga-Helia University of Applied Sciences.
