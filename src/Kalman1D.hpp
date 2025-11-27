#pragma once

class Kalman1D {
 private:
  float q;  // process noise
  float r;  // measurement noise
  float x;  // state estimate
  float p;  // estimate covariance
  float n = 0;
  float total = 0;
  float mean;
  bool initialized = false;

 public:
  Kalman1D(float processNoise = 1.0f, float measurementNoise = 4.0f,
           float initialEstimate = 0.0f, float covariance = 1.0f) {
    q = processNoise;      // process noise covariance
    r = measurementNoise;  // measurement noise covariance
    x = initialEstimate;   // initial estimate
    p = covariance;        // initial estimate covariance
    initialized = true;
  }

  float update(float measurement) {
    p = p + q;

    float k = p / (p + r);
    x = x + k * (measurement - x);
    p = (1.0f - k) * p;

    n++;
    total += x;
    mean = total / n;
    return x;
  }

  void setProcessNoise(float processNoise) { q = processNoise; }
  void setMeasurementNoise(float measurementNoise) { r = measurementNoise; }
  void setEstimate(float estimate) { x = estimate; }
  float getEstimate() const { return x; }
  float getMean() const { return mean; }
};
