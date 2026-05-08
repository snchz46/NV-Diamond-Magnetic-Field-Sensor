// ADS1115 raw count reading with IIR low-pass filter
// No voltage conversion (more efficient and more accurate for dip detection)

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

const float ALPHA = 0.1f;   // IIR smoothing
float filtered = 0.0f;      // filtered raw counts

void setup() {
  Serial.begin(115200);
  Wire.begin(13, 15);

  ads.begin(0x48);
  ads.setGain(GAIN_SIXTEEN);                    // gain affects range but raw counts remain 16-bit
  ads.setDataRate(RATE_ADS1115_250SPS);         // stable rate for filtering
}

void loop() {
  int16_t raw = ads.readADC_SingleEnded(0);     // raw ADC reading (0..32767)

  // IIR filtering directly on raw counts
  filtered = filtered + ALPHA * (raw - filtered);

  // Print filtered raw counts
  Serial.println(filtered, 2);   
}
