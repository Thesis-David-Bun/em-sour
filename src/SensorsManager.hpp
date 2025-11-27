#pragma once
#include <DallasTemperature.h>
#include <OneWire.h>
#include <Preferences.h>
#include <VL53L0X.h>
#include <Wire.h>

#include "Kalman1D.hpp"

constexpr const int tofTimeout = 3500;
constexpr const int tofTimingMeasure = 2000000;           // 2000ms
constexpr const uint32_t delayTime = 1000 * 60 * 5;       // 5 minutes
constexpr const short int MAX_SETUP_MQ3 = (60 / 5) * 48;  // 48 hours

constexpr const float sigmaToF = 1.33f * 1.33f;
constexpr const float sigmaDS = 0.32f * 0.32f;
constexpr const float sigmaMQ3 = 8.29f * 8.29f;

constexpr const int SIZE_ARR = 10;

struct SensorsReading {
  float rawE;  // Ethanol -> MQ-3
  float rawT;  // Temperature -> DS18B20
  float rawH;  // Height -> VL53L0X

  float filE;
  float filT;
  float filH;
};

class SM {
 private:
  float arrTemp[SIZE_ARR] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 0.0f, 0.0f};

  float totalEth = 0.0;
  float totalTemp = 0.0;
  float totalDist = 0.0;
  int n = 0;
  int pinMQ3;
  int pinDS18B20;
  int pinIRLML2505;  // To trigger on and off MQ-3

  OneWire oneWire;
  DallasTemperature tempSensor;
  VL53L0X tofSensor;
  Preferences preferences;
  Kalman1D kalmanToF;
  Kalman1D kalmanMQ3;
  Kalman1D kalmanDS;

 public:
  SM(const int a, const int b, const int c)
      : pinMQ3(a),
        pinDS18B20(b),
        pinIRLML2505(c),
        oneWire(b),
        tempSensor(&oneWire),
        kalmanToF(0.7f, sigmaToF),
        kalmanMQ3(1.0f, sigmaMQ3),
        kalmanDS(0.5f, sigmaDS) {}

  void setup() {
    tempSensor.begin();
    Wire.begin();
    tofSensor.setTimeout(tofTimeout);

    if (tofSensor.init()) {
      tofSensor.setMeasurementTimingBudget(tofTimingMeasure);
    }
    delay(100);
    readTemp();
    pinMode(pinIRLML2505, OUTPUT);
    digitalWrite(pinIRLML2505, HIGH);
  }

  void initialSetup() {
    preferences.begin("my", false);
    short int counter = preferences.getShort("counter", 0);
    while (counter != MAX_SETUP_MQ3) {
      delay(delayTime);
      counter++;
      preferences.putShort("counter", counter);
    }
    preferences.end();
  }

  void setEstimate() {
    kalmanDS.setEstimate(arrTemp[0]);
    kalmanToF.setEstimate(tofSensor.readRangeSingleMillimeters());
    kalmanMQ3.setEstimate(analogRead(pinMQ3));
    delay(100);
  }

  void readTemp() {
    for (int i = 0; i < SIZE_ARR; i++) {
      tempSensor.requestTemperatures();
      arrTemp[i] = tempSensor.getTempCByIndex(0);
      delay(100);
    }
  }

  SensorsReading readAll(const int i) {
    SensorsReading temp;

    temp.rawT = arrTemp[i];
    totalTemp += temp.rawT;
    temp.rawE = analogRead(pinMQ3);
    totalEth += temp.rawE;
    temp.rawH = tofSensor.readRangeSingleMillimeters();
    totalDist += temp.rawH;
    n++;

    temp.filE = kalmanMQ3.update(temp.rawE);
    temp.filT = kalmanDS.update(temp.rawT);
    temp.filH = kalmanToF.update(temp.rawH);

    return temp;
  }

  SensorsReading getMean(const int i) const {
    float raw_meanMQ3 = totalEth / n;
    float raw_meanDS = totalTemp / n;
    float raw_meanToF = totalDist / n;

    float fil_meanMQ3 = kalmanMQ3.getMean();
    float fil_meanDS = kalmanDS.getMean();
    float fil_meanToF = kalmanToF.getMean();

    // String payload = String("{") + "\"mean_eth\":" + kalmanMQ3.getMean() +
    // "," +
    //                  "\"mean_raw_eth\":" + raw_meanMQ3 + "," +
    //                  "\"mean_temp\":" + kalmanDS.getMean() + "," +
    //                  "\"mean_raw_temp\":" + raw_meanDS + "," +
    //                  "\"mean_hei\":" + kalmanToF.getMean() + "," +
    //                  "\"mean_raw_hei\":" + raw_meanToF + "," +
    //                  "\"n\":" + (i + 1) + "}";
    return {raw_meanMQ3, raw_meanDS, raw_meanToF,
            fil_meanMQ3, fil_meanDS, fil_meanToF};
  }
};