#pragma once
#include <Arduino.h>

/*

To know rise or not we need to store n-1 data and compare it with data at time
n.

Also we need to store peak height, assume the highest height is the peak.

Store a value of how many times the device read ethanol = 4095.

Rules
0. IF (Height >= Threshold) OR (Ethanol = 4095 for 3 times measurement) THEN
(RECOVERY, need to re feed again)
1. IF (RISE) AND (Height High) AND (Ethanol High) AND (Temp Med) THEN
(READY TO USE)
2. IF (RISE) AND (Height High) AND (Ethanol High) AND (Temp Low) THEN (READY TO
USE, BUT NOT RECOMMENDED)
3. IF (RISE) AND (Height High) AND (Ethanol High) AND (Temp High) THEN (READY TO
USE, URGENT)
4. IF (RISE) AND (Height Med) AND (Ethanol High/Med) THEN (NOT READY TO USE,
feed once again and it is ready to use)
5. IF (NOT RISE or NOT FALL for X times) THEN (STARTER NEED FEED)
6. IF (FALL >= Threshold) THEN (RE-FEED)

*/

class FLC {
 private:
  // ======== THRESHOLDS (Tunable) ========
  float H_overflow = 85.0f;

  // Temperature (°C)
  float T_low_max = 29.9f;
  float T_med_min = 30.0f;
  float T_med_max = 32.9f;
  float T_high_min = 33.0f;

  // Height (mm)
  float H_low_max = 15.0f;
  float H_med_min = 16.0f;
  float H_med_max = 25.0f;
  float H_high_min = 26.0f;

  // Ethanol (ppm)
  float E_low_max = 2178.9f;
  float E_med_min = 2179.0f;
  float E_med_max = 3136.9f;
  float E_high_min = 3137.0f;

  // Output variable domain (0.0 → 1.0)
  static constexpr int OUT_STEPS = 50;  // Resolution of centroid integration

 public:
  // ===== Linear MF helpers =====
  float riseLinear(float x, float x0, float x1) {
    if (x <= x0) return 0.0f;
    if (x >= x1) return 1.0f;
    return (x - x0) / (x1 - x0);
  }

  float fallLinear(float x, float x0, float x1) {
    if (x <= x0) return 1.0f;
    if (x >= x1) return 0.0f;
    return (x1 - x) / (x1 - x0);
  }

  // ===== INPUT MEMBERSHIP FUNCTIONS =====
  float tempCold(float T) { return fallLinear(T, T_low_max, T_med_min); }
  float tempWarm(float T) {
    float L = riseLinear(T, T_med_min, (T_med_min + T_med_max) * 0.5f);
    float R = fallLinear(T, (T_med_min + T_med_max) * 0.5f, T_med_max);
    return min(L, R);
  }
  float tempHot(float T) {
    return riseLinear(T, T_high_min, T_high_min + 4.0f);
  }

  float riseLow(float h) { return fallLinear(h, H_low_max, H_med_min); }
  float riseMed(float h) {
    float L = riseLinear(h, H_med_min, (H_med_min + H_med_max) * 0.5f);
    float R = fallLinear(h, (H_med_min + H_med_max) * 0.5f, H_med_max);
    return min(L, R);
  }
  float riseHigh(float h) {
    return riseLinear(h, H_high_min, H_high_min + 10.0f);
  }

  float ethLow(float e) { return fallLinear(e, E_low_max, E_med_min); }
  float ethMed(float e) {
    float L = riseLinear(e, E_med_min, (E_med_min + E_med_max) * 0.5f);
    float R = fallLinear(e, (E_med_min + E_med_max) * 0.5f, E_med_max);
    return min(L, R);
  }
  float ethHigh(float e) {
    return riseLinear(e, E_high_min, E_high_min + 200.0f);
  }

  // ===== OUTPUT MEMBERSHIP FUNCTIONS =====
  float mfReady(float x) { return riseLinear(x, 0.75f, 1.0f); }
  float mfReadyUrgent(float x) { return riseLinear(x, 0.85f, 1.0f); }
  float mfReadyOptional(float x) { return riseLinear(x, 0.55f, 0.75f); }
  float mfNotReady(float x) { return riseLinear(x, 0.25f, 0.45f); }
  float mfFeedAgain(float x) { return riseLinear(x, 0.1f, 0.3f); }
  float mfRecovery(float x) { return riseLinear(x, 0.0f, 0.2f); }

  // ===== MAIN INFERENCE =====
  String infer(float T, float H, float E) {
    // 0. Hard overflow rule
    if (H > H_overflow) return "\"OVERFLOW\"";

    // Fuzzify inputs
    float C = tempCold(T);
    float W = tempWarm(T);
    float Ht = tempHot(T);

    float RL = riseLow(H);
    float RM = riseMed(H);
    float RH = riseHigh(H);

    float EL = ethLow(E);
    float EM = ethMed(E);
    float EH = ethHigh(E);

    // Rule strengths (activation)
    float r_ready = min(W, min(RH, EM));
    float r_readyUrgent = min(Ht, min(RH, EM));
    float r_readyOpt = min(C, min(RH, EM));
    float r_feedAgain = min(RM, max(EM, EH));
    float r_notReady = RL;
    float r_recovery = min(RH, EH);

    // Aggregate output fuzzy shape
    float num = 0.0f;
    float den = 0.0f;

    for (int i = 0; i <= OUT_STEPS; i++) {
      float x = float(i) / OUT_STEPS;

      float miu_ready = min(r_ready, mfReady(x));
      float miu_urgent = min(r_readyUrgent, mfReadyUrgent(x));
      float miu_opt = min(r_readyOpt, mfReadyOptional(x));
      float miu_notReady = min(r_notReady, mfNotReady(x));
      float miu_feed = min(r_feedAgain, mfFeedAgain(x));
      float miu_recover = min(r_recovery, mfRecovery(x));

      float miu =
          max(max(max(max(max(miu_ready, miu_urgent), miu_opt), miu_notReady),
                  miu_feed),
              miu_recover);

      num += x * miu;
      den += miu;
    }

    float crisp = (den == 0 ? 0 : num / den);

    // Categorize crisp output into discrete states
    if (crisp > 0.85f) return "\"READY_URGENT\"";
    if (crisp > 0.70f) return "\"READY\"";
    if (crisp > 0.55f) return "\"READY_OPTIONAL\"";
    if (crisp > 0.30f) return "\"NOT_READY\"";
    if (crisp > 0.20f) return "\"FEED_AGAIN\"";
    return "\"RECOVERY\"";
  }
};
