#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

// -------- CONFIG --------
const float IIR_ALPHA = 0.15f;        // IIR smoothing factor
const uint16_t samplesToAvg = 8;      // Manual oversampling / averaging
// -------------------------

float filteredRaw = 0.0f;             // filtered raw ADC counts

void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(13, 15);

  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not found!");
    while (1) delay(200);
  }

  ads.setGain(GAIN_SIXTEEN);              // gain affects physical range, not raw counts
  ads.setDataRate(RATE_ADS1115_250SPS);   // stable for oversampling
}

void loop() {

  long sum = 0;

  // --- Manual Oversampling ---
  for (int i = 0; i < samplesToAvg; i++) {
    sum += ads.readADC_SingleEnded(0);    // raw counts 0..32767
  }

  float rawAvg = sum / (float)samplesToAvg;   // averaged raw count

  // --- IIR Filter on raw counts ---
  filteredRaw = filteredRaw + IIR_ALPHA * (rawAvg - filteredRaw);

  // Print filtered raw ADC value
  Serial.println(filteredRaw, 2);
}
