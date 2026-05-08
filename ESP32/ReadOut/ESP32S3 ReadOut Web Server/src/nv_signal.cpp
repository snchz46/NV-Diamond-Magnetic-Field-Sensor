#include "nv_signal.h"
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

static Adafruit_ADS1115 ads;

static float IIR_ALPHA = 0.10f;
static uint16_t samplesToAvg = 4;
static float DIP_THRESHOLD = -25.0f;

static float baseline = 0.0f;
static float filteredRaw = 0.0f;
static bool dipState = false;

static unsigned long lastDipTimeMs = 0;
static float rpm = 0.0f;

static const uint16_t ADS_RATE = RATE_ADS1115_860SPS;

static void initBaseline() {
  baseline = 0.0f;
  const int M = 60;
  for (int i = 0; i < M; i++) {
    baseline += ads.readADC_SingleEnded(0);
    delay(5);
  }
  baseline /= (float)M;
  filteredRaw = baseline;
}

bool nv_init() {
  if (!ads.begin(0x48)) return false;
  ads.setGain(GAIN_SIXTEEN);
  ads.setDataRate(ADS_RATE);
  initBaseline();
  return true;
}

NvConfig nv_get_config() {
  return NvConfig{ IIR_ALPHA, samplesToAvg, DIP_THRESHOLD };
}

void nv_set_config(const NvConfig& cfg) {
  IIR_ALPHA = constrain(cfg.alpha, 0.01f, 0.5f);
  uint16_t a = cfg.avg;
  if (a < 1) a = 1;
  if (a > 32) a = 32;
  samplesToAvg = a;
  DIP_THRESHOLD = constrain(cfg.thr, -20000.0f, 20000.0f);
}

NvSample nv_step() {
  long sum = 0;
  for (uint16_t i = 0; i < samplesToAvg; i++) {
    sum += ads.readADC_SingleEnded(0);
  }
  float raw = sum / (float)samplesToAvg;

  filteredRaw += IIR_ALPHA * (raw - filteredRaw);
  float delta = filteredRaw - baseline;

  bool dipEvent = false;
  if (!dipState && delta < DIP_THRESHOLD) {
    dipState = true;
    dipEvent = true;

    unsigned long now = millis();
    if (lastDipTimeMs != 0) {
      unsigned long dt = now - lastDipTimeMs;
      if (dt > 0) rpm = 60000.0f / (float)dt;
    }
    lastDipTimeMs = now;
  }
  if (dipState && delta > DIP_THRESHOLD * 0.5f) {
    dipState = false;
  }

  return NvSample{ millis(), raw, filteredRaw, delta, dipEvent, rpm };
}
