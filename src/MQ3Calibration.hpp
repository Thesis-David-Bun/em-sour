#pragma once
#include <Arduino.h>

class MQ3Calibration {
 private:
  // Raw ADC limits
  const float RAW_MIN = 1220.0f;  // clean air baseline → 0 ppm
  const float RAW_MAX = 4095.0f;  // max reading → 500 ppm

  // PPM limits
  const float PPM_MIN = 0.0f;
  const float PPM_MAX = 500.0f;

  float scale;   // slope
  float offset;  // intercept

 public:
  MQ3Calibration() {
    // Compute linear mapping once.
    scale = (PPM_MAX - PPM_MIN) / (RAW_MAX - RAW_MIN);
    offset = -RAW_MIN * scale;  // so that RAW_MIN → 0
  }

  // Apply linear mapping and clamp 0–500
  float apply(float raw) const {
    if (raw <= RAW_MIN) return 0.0f;
    if (raw >= RAW_MAX) return PPM_MAX;

    return raw * scale + offset;
  }

  // Optional: allow user to update clean-air baseline (RAW_MIN)
  // If you want a dynamic baseline measurement.
  void setBaseline(float newBaselineRaw) {
    // Limit to valid range
    float baseline = newBaselineRaw;
    if (baseline < 0) baseline = 0;
    if (baseline > 4095) baseline = 4095;

    // Update linear mapping
    float span = RAW_MAX - baseline;
    if (span < 1) span = 1;  // avoid divide by zero

    scale = (PPM_MAX - PPM_MIN) / span;
    offset = -baseline * scale;
  }
};
