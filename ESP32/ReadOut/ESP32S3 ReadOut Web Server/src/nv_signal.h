#pragma once
#include <Arduino.h>

struct NvSample {
  uint32_t t_ms;
  float raw;
  float filtered;
  float delta;
  bool dip;
  float rpm;
};

struct NvConfig {
  float alpha;
  uint16_t avg;
  float thr;
};

bool nv_init();                 // init ADS, baseline
NvSample nv_step();             // take one step (read + filter + detect)
NvConfig nv_get_config();
void nv_set_config(const NvConfig& cfg);
