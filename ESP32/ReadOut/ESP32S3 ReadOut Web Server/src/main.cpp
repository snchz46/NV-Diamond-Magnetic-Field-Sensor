#include <Arduino.h>
#include <Wire.h>
#include "nv_signal.h"
#include "web_ui.h"
#include "csv_logger.h"

static const uint8_t SEND_EVERY_N = 4;
static uint32_t counter = 0;

void setup() {
  Serial.begin(115200);
  delay(200);

  Wire.begin(13, 15);

  if (!nv_init()) {
    Serial.println("ADS1115 not found!");
    while (true) delay(200);
  }

  web_init();
}

void loop() {
  web_loop();

  NvSample s = nv_step();

  // log for CSV
  if (++counter % SEND_EVERY_N == 0) {
    csv_push(s);
    web_publish_sample(s.raw, s.filtered, s.delta, s.dip, s.rpm);
  }
}
