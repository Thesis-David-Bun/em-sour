#pragma once
#include <Preferences.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

Preferences lo_st;

void putData(const short int x = 0, const char* key = "res") {
  lo_st.begin("reset", false);
  // short int counter = lo_st.getShort(key, 0);
  lo_st.putShort(key, x);
  lo_st.end();
}

short int getData(const char* key) {
  lo_st.begin("reset", false);
  short int counter = lo_st.getShort(key, 0);
  // lo_st.putShort(key, x);
  lo_st.end();
  return counter;
}

void callback(char* topic, byte* message, unsigned int length) {
  String messageTemp;

  for (int i = 0; i < length; i++) {
    messageTemp += (char)message[i];
  }

  if (String(topic) == "/is_reset") {
    if (messageTemp = "1") putData(1, "res");
  }
}

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
    mqttClient.setCallback(callback);

    String clientID = "ESP32-C" + String(random(0xffff), HEX);
    unsigned long start = millis();
    while (!mqttClient.connected() && millis() - start < MAX_DURATION) {
      if (mqttClient.connect(clientID.c_str(), user_name, password)) {
        break;
      } else {
        delay(100);
      }
    }
    is_reset();
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

  void is_reset() {
    publish("/is_reset", "p");
    unsigned long start = millis();
    while (millis() - start < MAX_DURATION) {
      delay(100);
    }
  }
};