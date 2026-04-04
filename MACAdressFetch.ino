/**
 * MACAdressFetch.ino
 *
 * One-time utility sketch for retrieving the ESP32's Wi-Fi station MAC address.
 * Flash this onto an ESP32, open the Serial Monitor at 115200 baud, and copy
 * the printed address into ControllerCode.ino's broadcastAddress field so the
 * two boards can communicate over ESP-NOW.
 *
 * This sketch does nothing in loop() — it is intended to be run once,
 * then replaced with the actual application firmware.
 */

#include <Arduino.h>
#include <esp_mac.h>  // ESP-IDF MAC address API

void setup() {
  Serial.begin(115200);
  delay(1000);  // Wait for the Serial Monitor to connect before printing

  uint8_t mac[6];

  // Read the Wi-Fi station MAC address (used by ESP-NOW for peer addressing)
  esp_read_mac(mac, ESP_MAC_WIFI_STA);

  // Print in standard colon-separated hex format (e.g. 78:E3:6D:11:70:C4)
  Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X\n",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void loop() {}

// ─── References ──────────────────────────────────────────────────────────────────
//
// ESP-IDF MAC address API:
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/misc_system_api.html#mac-address
//
// ESP-NOW overview (requires MAC address of each peer):
//   https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html
