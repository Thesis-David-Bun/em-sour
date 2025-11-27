#pragma once
#include <Arduino.h>

struct CalPoint {
  int raw;
  int truth;
};

class ToFCalibration {
 private:
  int a, b;

 public:
  ToFCalibration() : a(1), b(0) {}

  int apply(int raw) const { return a * raw + b; }

  void calibrateOffset(int raw, int truth) {
    a = 1;
    b = truth - raw;
  }

  float computeFromPoints(const CalPoint* pts, int n) {
    if (n < 2) return -1.0f;

    double sumR = 0.0, sumT = 0.0;
    for (int i = 0; i < n; i++) {
      sumR += pts[i].raw;
      sumT += pts[i].truth;
    }

    double rBar = sumR / n;
    double tBar = sumT / n;
    double num = 0.0, den = 0.0;

    for (int i = 0; i < n; i++) {
      num += (pts[i].raw - rBar) * (pts[i].truth - tBar);
      den += (pts[i].raw - rBar) * (pts[i].raw - rBar);
    }
    if (den == 0) return -1.0f;

    a = (float)(num / den);
    b = (float)(tBar - a * rBar);

    // compute RMSE
    double se = 0.0;
    for (int i = 0; i < n; i++) {
      double pred = a * pts[i].raw + b;
      double err = pred - pts[i].truth;
      se += err * err;
    }
    return (float)sqrt(se / n);
  }

  float medianOf(int n, float (*readFunc)()) {
    if (n <= 0) return 0;
    const int MAX_SAMPLES = 50;
    if (n > MAX_SAMPLES) n = MAX_SAMPLES;

    float v[MAX_SAMPLES];
    for (int i = 0; i < n; i++) {
      v[i] = readFunc();
      delay(10);
    }

    // Simple bubble sort (n is small, so performance fine)
    for (int i = 0; i < n - 1; i++) {
      for (int j = 0; j < n - i - 1; j++) {
        if (v[j] > v[j + 1]) {
          float tmp = v[j];
          v[j] = v[j + 1];
          v[j + 1] = tmp;
        }
      }
    }

    if (n % 2 == 1)
      return v[n / 2];
    else
      return 0.5f * (v[n / 2 - 1] + v[n / 2]);
  }
};
