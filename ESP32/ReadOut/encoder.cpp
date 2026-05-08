#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

//    🧲  ←── magnet rotating around sensor
//     │
//     ▼
//  ╔══════╗
//  ║  NV  ║  ← change in magnetic field causes dip in ADC signal
//  ╚══════╝
//     │
//     ▼
//  Signal:  ▔▔▔▔╲__/▔▔▔▔╲__/▔▔▔▔
//                 ↑    ↑
//               dip1  dip2  → time between them → RPM

Adafruit_ADS1115 ads;

// -------- CONFIG --------
const float ALPHA = 0.05f;                // IIR (0.05–0.2, smooth filtering)
float filtered = 0.0f;                    // filtered raw ADC value
float baseline = 0.0f;                    // average raw baseline

// DIP threshold in raw counts (adjust by looking at graph)
// Example: -40 means the dip is detected when filtered is 40 counts below baseline
const float DIP_THRESHOLD = -45.0f;

bool dipState = false;

unsigned long lastDipTime = 0;
unsigned long dipCount = 0;

float rpm = 0;
float omega = 0;
float lastOmega = 0;
float alpha = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(13, 15);

  ads.begin();
  ads.setGain(GAIN_SIXTEEN);                 // gain affects full-scale voltage, raw counts stay same
  ads.setDataRate(RATE_ADS1115_860SPS);      // fast enough for encoder dips

  // --- Initialize baseline using RAW COUNTS ---
  for (int i = 0; i < 50; i++) {
    baseline += ads.readADC_SingleEnded(0);  // read raw ADC value (0..32767)
    delay(5);
  }
  baseline /= 50.0f;

  Serial.println("Encoder NV Sensor Ready (RAW MODE)");
}

void loop() {

  int16_t raw = ads.readADC_SingleEnded(0);   // RAW ADC counts

  // --- IIR filtering on RAW counts ---
  filtered += ALPHA * (raw - filtered);

  // Deviation from baseline in raw counts
  float delta = filtered - baseline;

  // --- Dip detection ---
  if (!dipState && delta < DIP_THRESHOLD) {

    dipState = true;

    unsigned long now = millis();
    unsigned long dt = now - lastDipTime;

    if (lastDipTime > 0 && dt > 0) {
      float revPerMs = 1.0 / dt;
      float revPerSec = revPerMs * 1000.0;

      rpm = revPerSec * 60.0;
      omega = revPerSec * 2.0 * PI;
      alpha = (omega - lastOmega) / (dt / 1000.0);

      lastOmega = omega;
    }

    lastDipTime = now;
    dipCount++;

    Serial.print("Dip #"); Serial.print(dipCount);
    Serial.print(" | RPM: "); Serial.print(rpm);
    Serial.print(" | ω: "); Serial.print(omega);
    Serial.print(" rad/s | α: "); Serial.println(alpha);
  }

  // --- hysteresis to exit dip ---
  if (dipState && delta > DIP_THRESHOLD * 0.5f) {
    dipState = false;
  }
}
