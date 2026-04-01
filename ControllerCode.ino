#include <esp_now.h>
#include <WiFi.h>

// REMPLACER PAR L'ADRESSE MAC DU CHAR OBTENUE PRÉCÉDEMMENT
uint8_t broadcastAddress[] = {0x24, 0x6F, 0x28, 0x00, 0x00, 0x00};

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

struct_message myData;
esp_now_peer_info_t peerInfo;

const int VITESSE_FIXE = 255;

void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) return;

  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  pinMode(32, INPUT_PULLUP); // Chenille Gauche
  pinMode(33, INPUT_PULLUP); // Chenille Droite
  pinMode(25, INPUT_PULLUP); // Inverser Gauche
  pinMode(26, INPUT_PULLUP); // Inverser Droite
  pinMode(27, INPUT_PULLUP); // Tourelle Gauche
  pinMode(14, INPUT_PULLUP); // Tourelle Droite
  pinMode(12, INPUT_PULLUP); // Tirer (A)
  pinMode(13, INPUT_PULLUP); // Arrêt (X)
}

void loop() {
  myData.leftTrigger = !digitalRead(32) ? VITESSE_FIXE : 0;
  myData.rightTrigger = !digitalRead(33) ? VITESSE_FIXE : 0;
  myData.buttonY = !digitalRead(25);
  myData.buttonB = !digitalRead(26);
  myData.dpadLeft = !digitalRead(27);
  myData.dpadRight = !digitalRead(14);
  myData.buttonA = !digitalRead(12);
  myData.buttonX = !digitalRead(13);

  esp_now_send(broadcastAddress, (uint8_t *) &myData, sizeof(myData));
  delay(20); 
}