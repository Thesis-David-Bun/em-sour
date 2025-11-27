#pragma once
#include <WiFi.h>

class WFM {
 private:
  const char* ssid;
  const char* password;

  unsigned long MAX_DURATION = 10000;  // 10 seconds

 public:
  WFM(const char* s, const char* p) : ssid(s), password(p) {}

  void connect() {
    WiFi.begin(ssid, password);

    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < MAX_DURATION) {
      delay(100);
    }
  }

  void ensureConnection() {
    if (WiFi.status() != WL_CONNECTED) {
      connect();
    }
  }

  bool status() { return WiFi.status() == WL_CONNECTED; }
};