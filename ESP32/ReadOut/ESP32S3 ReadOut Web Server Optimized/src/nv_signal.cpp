/**
 * nv_signal.cpp — ADS1115 acquisition, IIR filtering, and dip detection
 *
 * Design overview
 * ───────────────
 * The ADS1115 is driven in continuous-conversion mode at 860 SPS. Each call
 * to nv_step() blocks for exactly `samplesToAvg` conversion periods (≈1.16 ms
 * each) to read and average that many fresh samples.
 *
 * Two parallel IIR filters run on the averaged raw value:
 *
 *   • Slow display filter (α = IIR_ALPHA, default 0.05):
 *       Produces the smooth waveform shown in the browser. Intentionally
 *       sluggish so the trace is easy to read.
 *
 *   • Fast detection filter (α = DETECT_ALPHA = 0.50):
 *       Used exclusively for threshold comparisons. Its higher α means a
 *       genuine sharp dip passes through with minimal attenuation, so the
 *       threshold works predictably regardless of the display filter setting.
 *
 * Dip detection uses a two-state hysteresis machine:
 *   Entry  – fast delta < DIP_THRESHOLD  (and inter-dip guard elapsed)
 *   Exit   – fast delta > DIP_THRESHOLD × 0.5
 *
 * RPM is computed as a rolling average of the last RPM_HIST inter-dip
 * intervals, expressed in dips per minute (useful as a proxy for rotational
 * or repetitive event rate).
 */

#include "nv_signal.h"
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

static Adafruit_ADS1115 ads;

// ── Runtime-configurable parameters (adjustable via web UI) ───────────────────
static float    IIR_ALPHA     = 0.05f;   // Display IIR coefficient (0.01–0.50)
static uint16_t samplesToAvg  = 10;      // ADC readings averaged per raw sample
static float    DIP_THRESHOLD = -15.0f;  // Detection threshold in ADC counts

// ── Filter state ──────────────────────────────────────────────────────────────
static float baseline         = 0.0f;   // Captured zero reference (ADC counts)
static float filteredRaw      = 0.0f;   // Output of slow display IIR filter

// ── Fast detection filter ─────────────────────────────────────────────────────
// A separate IIR with a much higher α (0.50) is used only for threshold
// comparisons. This prevents the slow display filter from masking or delaying
// detection of real dips.
static float detectedFiltered = 0.0f;
static const float DETECT_ALPHA = 0.50f;

// ── Dip state machine ─────────────────────────────────────────────────────────
static bool dipState = false;  // true while signal is below the threshold band

// ── Inter-dip guard timer ─────────────────────────────────────────────────────
// After a dip fires the detector ignores new triggers for this many ms.
// Prevents burst false-positives when the signal rings / oscillates near the
// hysteresis exit threshold.
// 50 ms corresponds to a maximum detectable rate of ~1200 events/min.
static const unsigned long MIN_DIP_INTERVAL_MS = 50UL;

// ── RPM rolling average ───────────────────────────────────────────────────────
static const uint8_t RPM_HIST = 8;              // Number of intervals to average
static unsigned long dipIntervals[RPM_HIST];    // Circular buffer of inter-dip intervals (ms)
static uint8_t  dipHistHead  = 0;               // Write pointer into dipIntervals[]
static uint8_t  dipHistCount = 0;               // Number of valid entries (0–RPM_HIST)
static unsigned long lastDipTimeMs = 0;         // millis() timestamp of the previous dip
static float rpm = 0.0f;                        // Most recent computed RPM value

/**
 * computeRpm() — Record a new dip timestamp and update the RPM average.
 *
 * Inserts the elapsed time since the previous dip into the ring buffer,
 * then recomputes RPM as 60 000 / mean_interval_ms.
 *
 * @ param now  Current millis() value (passed in to avoid a second call).
 */
static void computeRpm(unsigned long now) {
  if (lastDipTimeMs != 0) {
    unsigned long dt = now - lastDipTimeMs;
    if (dt > 0) {
      dipIntervals[dipHistHead] = dt;
      dipHistHead = (dipHistHead + 1) % RPM_HIST;    // Advance circular write pointer
      if (dipHistCount < RPM_HIST) dipHistCount++;   // Saturate at RPM_HIST

      // Average all valid intervals in the ring buffer
      unsigned long sumDt = 0;
      for (uint8_t i = 0; i < dipHistCount; i++) sumDt += dipIntervals[i];
      rpm = 60000.0f / (sumDt / (float)dipHistCount);
    }
  }
  lastDipTimeMs = now;  // Update timestamp for the next dip's interval calculation
}

// ── Conversion timing ─────────────────────────────────────────────────────────
// At 860 SPS the ADS1115 outputs a new result every ~1163 µs. Waiting this
// long between getLastConversionResults() calls ensures we always read a fresh
// (not repeated) conversion.
static const unsigned long CONV_PERIOD_US = 1163UL;

/**
 * readOneFreshConversion() — Block until the next ADC result is available,
 * then return it.
 *
 * Uses a static timestamp so back-to-back calls accumulate the correct delay
 * without busy-waiting longer than necessary.
 *
 * @ return Raw 16-bit signed ADC count.
 */
static inline int16_t readOneFreshConversion() {
  static unsigned long lastReadUs = 0;
  unsigned long now = micros();
  if (now - lastReadUs < CONV_PERIOD_US) {
    delayMicroseconds(CONV_PERIOD_US - (now - lastReadUs));
  }
  lastReadUs = micros();
  return ads.getLastConversionResults();
}

/**
 * startContinuous() — Put the ADS1115 into continuous-conversion mode on
 * single-ended channel 0.
 */
static void startContinuous() {
  ads.startADCReading(ADS1X15_REG_CONFIG_MUX_SINGLE_0, /*continuous=*/true);
}

// ── Baseline capture ──────────────────────────────────────────────────────────
/**
 * captureBaseline() — Measure the quiescent signal level and reset all
 * filter / detection state.
 *
 * Discards 10 warm-up conversions then averages 60 samples (~70 ms total).
 * After this call `baseline` represents the expected resting ADC value;
 * `delta` in subsequent NvSamples is relative to it.
 */
static void captureBaseline() {
  // Drain any stale / settling conversions before measuring
  for (int i = 0; i < 10; i++) (void)readOneFreshConversion();

  baseline = 0.0f;
  const int M = 60;  // Number of samples to average for the baseline
  for (int i = 0; i < M; i++) baseline += readOneFreshConversion();
  baseline /= (float)M;

  // Seed filters at the baseline so the display starts flat (not at zero)
  filteredRaw      = baseline;
  detectedFiltered = baseline;

  // Reset dip state and RPM history
  dipState         = false;
  lastDipTimeMs    = 0;
  dipHistHead      = 0;
  dipHistCount     = 0;
  rpm              = 0.0f;
}

// ── Public API ────────────────────────────────────────────────────────────────

bool nv_init() {
  if (!ads.begin(0x48)) return false;     // I²C address 0x48 (ADDR pin → GND)
  ads.setGain(GAIN_SIXTEEN);              // ±0.256 V full-scale → ~7.8 µV/count
  ads.setDataRate(RATE_ADS1115_860SPS);  // Maximum data rate for lowest latency
  startContinuous();
  captureBaseline();
  return true;
}

void nv_recalibrate() {
  captureBaseline();  // Re-measure zero reference; blocks ~300 ms
}

NvConfig nv_get_config() {
  return NvConfig{ IIR_ALPHA, samplesToAvg, DIP_THRESHOLD };
}

void nv_set_config(const NvConfig& cfg) {
  // Clamp each parameter to a safe range before applying
  IIR_ALPHA    = constrain(cfg.alpha, 0.01f, 0.50f);

  uint16_t a = cfg.avg;
  if (a < 1)  a = 1;
  if (a > 32) a = 32;
  samplesToAvg = a;

  DIP_THRESHOLD = constrain(cfg.thr, -5000.0f, 5000.0f);
}

NvSample nv_step() {
  // ── 1. Hardware averaging ──────────────────────────────────────────────────
  // Read samplesToAvg consecutive fresh ADC conversions and average them.
  // This provides basic noise reduction before any software filtering.
  long sum = 0;
  for (uint16_t i = 0; i < samplesToAvg; i++) sum += readOneFreshConversion();
  float raw = sum / (float)samplesToAvg;

  // ── 2. Dual IIR filtering ──────────────────────────────────────────────────
  // Slow filter — drives the waveform shown in the browser UI
  filteredRaw += IIR_ALPHA * (raw - filteredRaw);

  // Fast filter — used only for threshold logic; reacts quickly to real dips
  detectedFiltered += DETECT_ALPHA * (raw - detectedFiltered);

  // Delta for display / CSV (based on slow filter)
  float delta          = filteredRaw      - baseline;

  // Delta for threshold comparisons (based on fast filter)
  float detectionDelta = detectedFiltered - baseline;

  // ── 3. Dip detection ───────────────────────────────────────────────────────
  bool dipEvent = false;
  unsigned long now = millis();

  // Entry condition: fast delta crosses the threshold AND the inter-dip guard
  // time has elapsed since the last confirmed dip.
  if (!dipState
      && detectionDelta < DIP_THRESHOLD
      && (lastDipTimeMs == 0 || (now - lastDipTimeMs) >= MIN_DIP_INTERVAL_MS))
  {
    dipState = true;   // Enter dip state
    dipEvent = true;   // Signal a new dip event to the caller (one-shot)
    computeRpm(now);   // Record interval and update RPM estimate
  }

  // Exit condition: fast delta recovers to above 50% of threshold (hysteresis).
  // Using 50% prevents noisy re-triggering when the signal loiters near the
  // threshold edge.
  if (dipState && detectionDelta > DIP_THRESHOLD * 0.5f) {
    dipState = false;
  }

  return NvSample{ (uint32_t)now, raw, filteredRaw, delta, dipEvent, rpm };
}