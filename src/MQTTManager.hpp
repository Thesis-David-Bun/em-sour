#pragma once
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

class MM {
 private:
  const char* broker_url;
  const int port;
  const char* user_name;
  const char* password;

  unsigned long MAX_DURATION = 10000;  // 10 seconds

  WiFiClientSecure wifiClient;
  PubSubClient mqttClient;

 public:
  MM(const char* bu, const int p, const char* un, const char* pass)
      : broker_url(bu),
        port(p),
        user_name(un),
        password(pass),
        mqttClient(wifiClient) {}

  void connect() {
    wifiClient.setInsecure();
    mqttClient.setServer(broker_url, port);

    String clientID = "ESP32-C" + String(random(0xffff), HEX);
    unsigned long start = millis();
    while (!mqttClient.connected() && millis() - start < MAX_DURATION) {
      if (mqttClient.connect(clientID.c_str(), user_name, password)) {
        break;
      } else {
        delay(100);
      }
    }
  }

  void ensureConnection() {
    if (!mqttClient.connected()) {
      connect();
    }
  }

  void publish(const char* topics, const char* payloads) {
    mqttClient.publish(topics, payloads);
  }

  bool status() { return mqttClient.connected(); }

  void loop() { mqttClient.loop(); }
};