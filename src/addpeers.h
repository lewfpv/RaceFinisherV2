#ifndef ADDPEERS_H
#define ADDPEERS_H

#include <esp_now.h>
#include <Arduino.h>  // Serial miatt

// MAC címek deklarálása és inicializálása itt:
// Vevők MAC címei


uint8_t receiver1[] = { 0xF4, 0x12, 0xFA, 0xE7, 0xBB, 0xCC };  // ESP32 5" kijelző

void addPeers() {
  // Első peer
  esp_now_peer_info_t peer1 = {};
  memcpy(peer1.peer_addr, receiver1, 6);
  peer1.channel = 0;
  peer1.encrypt = false;
  if (!esp_now_add_peer(&peer1)) {
    Serial.println("Peer1 added (Touch_EEZ)");
  }


}

#endif
