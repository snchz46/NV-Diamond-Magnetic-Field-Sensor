/**
 * nv_signal.h — Public API for ADS1115 signal acquisition and dip detection
 *
 * This module owns all ADC interaction, digital filtering, and dip-event
 * logic. Everything else in the firmware treats it as a black box:
 *   • Call nv_init() once at startup.
 *   • Call nv_step() repeatedly in the main loop to obtain processed samples.
 *   • Use nv_get_config() / nv_set_config() to tune parameters at runtime.
 *   • Call nv_recalibrate() to re-capture the baseline without rebooting.
 */

#pragma once
#include <Arduino.h>

/**
 * NvSample — One processed output sample.
 *
 * Produced by nv_step() and consumed by the web UI and CSV logger.
 *
 * Fields:
 *   t_ms     – Timestamp in milliseconds since boot (millis()).
 *   raw      – Average of samplesToAvg consecutive ADC readings (ADC counts).
 *   filtered – Slow IIR-filtered value of raw, used for the display line.
 *   delta    – filtered minus the captured baseline; positive = above baseline,
 *              negative = below. The dip threshold is applied against the
 *              fast detection delta, not this value.
 *   dip      – true on the single sample where a new dip event starts.
 *              Cleared automatically; only ever true for one nv_step() call
 *              per physical dip crossing.
 *   rpm      – Rolling 8-sample average of detected dip rate (dips per minute).
 *              Zero until at least two dips have been detected.
 */
struct NvSample {
  uint32_t t_ms;
  float raw;
  float filtered;
  float delta;
  bool dip;
  float rpm;
};

/**
 * NvConfig — Runtime-tunable detection parameters.
 *
 * Passed to / from nv_set_config() and nv_get_config().
 *
 * Fields:
 *   alpha – IIR smoothing coefficient for the *display* filter (0.01–0.50).
 *           Smaller = smoother but slower to track real changes.
 *           The separate fast detection filter always uses a fixed α = 0.50.
 *   avg   – Number of back-to-back ADC conversions averaged per raw sample
 *           (1–32). More averaging reduces noise but slows the loop.
 *   thr   – Dip detection threshold in ADC counts relative to baseline.
 *           Must be negative (e.g. -75). A dip fires when the fast delta
 *           drops below this value.
 */
struct NvConfig {
  float alpha;
  uint16_t avg;
  float thr;
};

/**
 * nv_init() — Configure the ADS1115 and capture the initial baseline.
 *
 * Starts continuous-conversion mode at 860 SPS with gain GAIN_SIXTEEN
 * (±0.256 V full-scale, ~7.8 µV/count).
 *
 * @return true on success, false if the ADS1115 is not found on the I²C bus.
 */
bool nv_init();

/**
 * nv_recalibrate() — Re-capture the zero baseline without rebooting.
 *
 * Blocks for ~300 ms while averaging 60 fresh conversions. Should be called
 * when the DC operating point of the signal has drifted significantly.
 */
void nv_recalibrate();

/**
 * nv_step() — Perform one acquisition + filter + detection cycle.
 *
 * Reads samplesToAvg fresh conversions, updates both IIR filters, and checks
 * dip entry/exit conditions. Call this as fast as possible from loop().
 *
 * @return A fully populated NvSample struct.
 */
NvSample nv_step();

/** nv_get_config() — Return the current runtime configuration. */
NvConfig nv_get_config();

/**
 * nv_set_config() — Update runtime parameters.
 *
 * Values are clamped to safe ranges internally; the caller does not need to
 * validate them first.
 */
void nv_set_config(const NvConfig& cfg);