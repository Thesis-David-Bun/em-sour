#pragma once
#include <Arduino.h>
#include <Preferences.h>

/*
  FuzzyMamdaniPrefs.hpp
  - Mamdani fuzzy inference system for sourdough readiness
  - Stores minimal state in Preferences so it survives deep sleep/reboots
  - Inputs:
      - deltaH_mm   : delta height (mm) measured (you said you already compute
  delta)
      - ethanolRaw  : raw ADC or mapped ppm (we check for saturation 4095)
      - tempC       : temperature in Celsius
  - Call begin(namespace) once. Then call update(deltaH_mm, ethanolRaw, tempC)
    on each sampling cycle. update() returns the State enum.
*/

class FLC {
 public:
  enum State {
    OVERFLOW,
    READY,
    READY_URGENT,
    READY_OPTIONAL,
    NOT_READY,
    FEED_AGAIN,
    RECOVERY
  };

  FLC(const int x) : OUT_STEPS(x) {}

  // ---------- Preferences keys (short) ----------
  const char* PREF_NS = "starter";
  const char* K_LAST_DH = "lastdh";  // float
  const char* K_PEAK_H = "peakh";    // float
  const char* K_HASPEAK = "hp";      // uint8_t
  const char* K_ETHCNT = "ethc";     // uint8_t
  const char* K_NORISE = "noris";    // uint16_t

  // ---------- Tunable thresholds (modifiable) ----------
  const float overflowHeightThreshold =
      90.0f;  // mm absolute or jar-specific (you said ΔH available)
  const int ethanolSaturateValue = 4095;  // raw ADC saturation
  const int ethanolSaturateNeeded = 3;    // count threshold

  // For rise/fall detection (compare last delta to current)
  const float riseEpsilon = 0.5f;  // mm difference to consider change
  const float fallEpsilon = 0.5f;

  // Small meaningful rise
  const float minMeaningfulDelta = 0.1f;  // mm

  // Output domain resolution
  const int OUT_STEPS;

  // --- Fuzzy thresholds (inputs) ---
  // Temperature (°C)
  const float T_low_max = 29.0f;
  const float T_med_min = 29.0f;
  const float T_med_max = 33.0f;
  const float T_high_min = 33.0f;

  // Delta height (mm)
  const float H_low_max = 160.0f;
  const float H_med_min = 160.0f;
  const float H_med_max = 140.0f;
  const float H_high_min = 140.0f;

  // Ethanol (assume scaled 0..4095 OR ppm); thresholds chosen as raw ADC-ish
  const float E_low_max = 2000.0f;
  const float E_med_min = 2000.0f;
  const float E_med_max = 3000.0f;
  const float E_high_min = 3000.0f;

  // ---------- Public API ----------
  void begin(const char* ns = nullptr) {
    if (ns) PREF_NS = ns;
    prefs.begin(PREF_NS, false);
    // Ensure keys exist
    if (!prefs.isKey(K_LAST_DH)) prefs.putFloat(K_LAST_DH, 0.0f);
    if (!prefs.isKey(K_PEAK_H)) prefs.putFloat(K_PEAK_H, 0.0f);
    if (!prefs.isKey(K_HASPEAK)) prefs.putUChar(K_HASPEAK, 0);
    if (!prefs.isKey(K_ETHCNT)) prefs.putUChar(K_ETHCNT, 0);
    if (!prefs.isKey(K_NORISE)) prefs.putUShort(K_NORISE, 0);
  }

  // Update with current sensor readings; returns current State
  String infer(const float deltaH_mm, const float ethanolRaw,
               const float tempC) {
    // Load persisted state
    float last_dh = prefs.getFloat(K_LAST_DH, 0.0f);
    float peak_h = prefs.getFloat(K_PEAK_H, 0.0f);
    bool hasPeaked = prefs.getUChar(K_HASPEAK, 0) != 0;
    uint8_t ethCount = prefs.getUChar(K_ETHCNT, 0);
    uint16_t noRiseCnt = prefs.getUShort(K_NORISE, 0);

    // --- detect rise / fall based on last delta (n-1) compare ---
    bool isRising = false;
    bool isFalling = false;
    float diff = last_dh - deltaH_mm;
    if (diff > riseEpsilon)
      isRising = true;
    else if (diff < -fallEpsilon)
      isFalling = true;
    else if (abs(diff) <= minMeaningfulDelta)
      isRising = false;
    // If very small changes, neither rising nor falling
    // treat as no meaningful rise
    // count no-rise

    // update peak logic: peak = highest observed delta before falling
    if (isRising) {
      if (deltaH_mm > peak_h) {
        peak_h = deltaH_mm;
      }
      // reset no-rise counter
      noRiseCnt = 0;
    } else {
      // if not rising and last state was rising previously, we may have peaked
      // simple heuristic: when first falling after having a peak_h > 0 set
      // hasPeaked
      if (isFalling && peak_h > 0.0f && !hasPeaked) {
        hasPeaked = true;
      }
      // increment no-rise counter if delta small
      if (abs(diff) <= minMeaningfulDelta) {
        noRiseCnt++;
      }
    }

    // ethanol saturate counter
    if (ethanolRaw >= ethanolSaturateValue) {
      ethCount++;
    } else {
      ethCount = 0;
    }

    // persist last delta and counters
    prefs.putFloat(K_LAST_DH, deltaH_mm);
    prefs.putFloat(K_PEAK_H, peak_h);
    prefs.putUChar(K_HASPEAK, hasPeaked ? 1 : 0);
    prefs.putUChar(K_ETHCNT, ethCount);
    prefs.putUShort(K_NORISE, noRiseCnt);

    // --- Safety / hard rules based on stored counters ---
    // Rule 0: Overflow or ethanol saturate 3 times -> OVER-FERMENTED / need
    // refeed
    if (deltaH_mm <= overflowHeightThreshold ||
        ethCount >= ethanolSaturateNeeded) {
      return "\"OVERFLOW\"";  // interpret as over-fermented/overflow condition
    }

    // Rule: If sustained no-rise for long (e.g., 6 samples) ->
    // FEED_AGAIN/RECOVERY (avoid classifying as "dead")
    if (noRiseCnt >= 6) {
      return "\"REFEED\"";  // need refeed/revive
    }

    // Now do fuzzy Mamdani inference using current inputs
    float C = tempCold(tempC);
    float W = tempWarm(tempC);
    float Ht = tempHot(tempC);

    float RL = riseHigh(deltaH_mm);
    float RM = riseMed(deltaH_mm);
    float RH = riseLow(deltaH_mm);

    float EL = ethLow(ethanolRaw);
    float EM = ethMed(ethanolRaw);
    float EH = ethHigh(ethanolRaw);

    // Rule activations (antecedents) — as you defined (revised semantically)
    float r_ready = min(W, min(RH, EH));         // Rule 1
    float r_readyOpt = min(C, min(RH, EH));      // Rule 2
    float r_readyUrgent = min(Ht, min(RH, EH));  // Rule 3
    float r_feedAgain = min(RM, max(EM, EH));    // Rule 4
    float r_notReady = min(RL, max(EM, EH));     // Rule 5

    // Aggregate output fuzzy sets by clipping their output MFs with the rule
    // strengths Output variable domain: x in [0,1], with defined output MFs
    // (triangular-like)
    String temp = String("ready:") + r_ready + ", urgent:" + r_readyUrgent +
                  ", opt:" + r_readyOpt + ", feedAgain:" + r_feedAgain +
                  ", notReady:" + r_notReady;
    Serial.println(temp);

    float num = 0.0f, den = 0.0f;
    for (int i = 0; i <= OUT_STEPS; i++) {
      float x = float(i) / OUT_STEPS;

      float miu_ready = min(r_ready, mfReady(x));
      float miu_urgent = min(r_readyUrgent, mfReadyUrgent(x));
      float miu_opt = min(r_readyOpt, mfReadyOptional(x));
      float miu_notReady = min(r_notReady, mfNotReady(x));
      float miu_feed = min(r_feedAgain, mfFeedAgain(x));

      float miu =
          max(max(max(max(miu_ready, miu_urgent), miu_opt), miu_notReady),
              miu_feed);

      num += x * miu;
      den += miu;
      String temp = String(". ready:") + miu_ready + ", urgent:" + miu_urgent +
                    ", opt:" + miu_opt + ", notReady:" + miu_notReady +
                    ", feed:" + miu_feed + ", num:" + num + ", den:" + den +
                    ", miu:" + miu;
      Serial.print(i);
      Serial.println(temp);
    }

    float crisp = (den == 0.0f) ? 0.0f : (num / den);

    Serial.println(num);
    Serial.println(den);
    Serial.println(crisp);

    // Map crisp to discrete category
    if (crisp > 0.85f) return "\"READY_URGENT\"";
    if (crisp > 0.70f) return "\"READY\"";
    if (crisp > 0.55f) return "\"READY_OPTIONAL\"";
    if (crisp > 0.30f) return "\"NOT_READY\"";
    if (crisp > 0.20f) return "\"FEED_AGAIN\"";
    return "\"RECOVERY\"";
  }

  // For debugging: clear stored preferences
  void clearState() {
    prefs.clear();
    // re-init defaults
    prefs.putFloat(K_LAST_DH, 0.0f);
    prefs.putFloat(K_PEAK_H, 0.0f);
    prefs.putUChar(K_HASPEAK, 0);
    prefs.putUChar(K_ETHCNT, 0);
    prefs.putUShort(K_NORISE, 0);
  }

 private:
  Preferences prefs;

  // ---------- Membership helpers ----------
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

  // Input MF implementations
  float tempCold(float T) { return fallLinear(T, T_low_max, T_med_min); }
  float tempWarm(float T) {
    float L = riseLinear(T, T_med_min, (T_med_min + T_med_max) * 0.5f);
    float R = fallLinear(T, (T_med_min + T_med_max) * 0.5f, T_med_max);
    return min(L, R);
  }
  float tempHot(float T) {
    return riseLinear(T, T_high_min, T_high_min + 1.0f);
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

  // Output MFs over [0..1]
  // float mfReady(float x) { return riseLinear(x, 0.75f, 1.0f); }
  // float mfReadyUrgent(float x) { return riseLinear(x, 0.85f, 1.0f); }
  // float mfReadyOptional(float x) { return riseLinear(x, 0.55f, 0.75f); }
  // float mfNotReady(float x) { return riseLinear(x, 0.25f, 0.45f); }
  // float mfFeedAgain(float x) { return riseLinear(x, 0.1f, 0.3f); }
  // float mfRecovery(float x) { return riseLinear(x, 0.0f, 0.2f); }

  float mfReady(float x) { return riseLinear(x, 0.60f, 0.80f); }
  float mfReadyUrgent(float x) { return riseLinear(x, 0.80f, 1.00f); }
  float mfReadyOptional(float x) { return riseLinear(x, 0.40f, 0.60f); }
  float mfNotReady(float x) { return riseLinear(x, 0.20f, 0.40f); }
  float mfFeedAgain(float x) { return riseLinear(x, 0.00f, 0.20f); }
};
