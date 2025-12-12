#include <Arduino.h>

#include "MQTTManager.hpp"
#include "SensorsManager.hpp"
#include "WiFiManager.hpp"

constexpr const char* ssid = "Lenhost2";
constexpr const char* pass = "77969988";

constexpr const char* hiveMQ_url =
    "c8655e388ba0426a84f2197ce7a2e4ff.s1.eu.hivemq.cloud";
constexpr const int port = 8883;
constexpr const char* id_mqtt = "Canary";
constexpr const char* pass_mqtt = "iwn72(*Hejoo82!";
constexpr const char* topic = "/test";

constexpr const int pin_mq3 = 34;
constexpr const int pin_ds = 4;

WFM wfm(ssid, pass);
MM mm(hiveMQ_url, port, id_mqtt, pass_mqtt);
SM sm(pin_mq3, pin_ds, 23);

constexpr uint64_t SLEEP_TIME = 10ULL * 60ULL * 1000000ULL;  // 10 MINUTES

void setup() {
  uint64_t start = millis();
  // Serial.begin(115200);
  delay(100);

  wfm.connect();
  mm.connect();
  sm.setup();

  // 10 readings
  for (int i = 1; i <= SIZE_ARR; i++) {
    if (wfm.status() && mm.status()) {
      SensorsReading t = sm.readAll(i - 1);
      // String payload = String("{") + "\"fil_eth\":" + t.filEth + "," +
      //                  "\"raw_eth\":" + t.rawEth + "," +
      //                  "\"fil_temperature\":" + t.filTemp + "," +
      //                  "\"raw_temperature\":" + t.rawTemp + "," +
      //                  "\"fil_height\":" + t.filDist + "," +
      //                  "\"raw_height\":" + t.rawDist + "," + "\"n\":" + i
      // +
      //                  "}";
      // mm.publish(topic, payload.c_str());
      if (i == 10) {
        SensorsReading p = sm.getMean();
        String payload =
            String("{") + "\"raw_mean_E\":" + p.rawE +
            ",\"fil_mean_E\":" + p.filE + ",\"raw_mean_T\":" + p.rawT +
            ",\"fil_mean_T\":" + p.filT + ",\"raw_mean_H\":" + p.rawH +
            ",\"fil_mean_H\":" + p.filH + "}";
        mm.publish(topic, payload.c_str());
      }
    } else {
      wfm.ensureConnection();
      mm.ensureConnection();
      i--;
    }
    delay(100);
  }

  digitalWrite(23, LOW);
  WiFi.disconnect(true, true);
  WiFi.mode(WIFI_OFF);

  uint64_t finish = (millis() - start) * 1000;
  esp_sleep_enable_timer_wakeup(SLEEP_TIME - finish);
  esp_deep_sleep_start();
}

void loop() {}