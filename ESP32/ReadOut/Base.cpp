#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

//    🧲  ←── magnet
//     │
//     ▼
//  ╔══════╗
//  ║  NV  ║  ← change in magnetic field causes dip in ADC signal
//  ╚══════╝
//     │
//     ▼
//  Signal:  ▔▔▔▔╲__/▔▔▔▔╲__/▔▔▔▔
//                   ↑           ↑
//                  dip1         dip2  → time between them → RPM

Adafruit_ADS1115 ads;

// -------- FILTER CONFIG --------
const float     IIR_ALPHA     = 0.05f;   // IIR smoothing factor (lower = smoother)
const uint16_t  SAMPLES_AVG   = 8;       // manual oversampling count
// --------------------------------

// -------- ENCODER CONFIG --------
const float DIP_THRESHOLD = -45.0f;     // raw counts below baseline to trigger dip
// ---------------------------------

float filteredRaw = 0.0f;
float baseline    = 0.0f;
bool  dipState    = false;

unsigned long lastDipTime = 0;
unsigned long dipCount    = 0;

float rpm       = 0.0f;
float omega     = 0.0f;
float lastOmega = 0.0f;
float angularAccel = 0.0f;

// ------------------------------------------------------------------
// Reads SAMPLES_AVG raw ADC counts and returns their average
// ------------------------------------------------------------------
float oversampledRead() {
  long sum = 0;
  for (int i = 0; i < SAMPLES_AVG; i++) {
    sum += ads.readADC_SingleEnded(0);
  }
  return sum / (float)SAMPLES_AVG;
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Wire.begin(13, 15);

  if (!ads.begin(0x48)) {
    Serial.println("ADS1115 not found!");
    while (1) delay(200);
  }

  ads.setGain(GAIN_SIXTEEN);
  ads.setDataRate(RATE_ADS1115_860SPS);

  // --- Baseline calibration using oversampled reads ---
  Serial.println("Calibrating baseline...");
  for (int i = 0; i < 50; i++) {
    baseline += oversampledRead();
    delay(5);
  }
  baseline /= 50.0f;
  filteredRaw = baseline;   // warm-start the IIR filter at baseline

  Serial.print("Baseline (raw counts): ");
  Serial.println(baseline, 2);
  Serial.println("NV Encoder Ready.");
}

void loop() {

  // --- Step 1: Manual oversampling ---
  float rawAvg = oversampledRead();

  // --- Step 2: IIR filter on the oversampled average ---
  filteredRaw += IIR_ALPHA * (rawAvg - filteredRaw);

  // --- Step 3: Deviation from baseline ---
  float delta = filteredRaw - baseline;

  // --- Step 4: Dip detection (leading edge) ---
  if (!dipState && delta < DIP_THRESHOLD) {

    dipState = true;

    unsigned long now = millis();
    unsigned long dt  = now - lastDipTime;

    if (lastDipTime > 0 && dt > 0) {
      float revPerSec = 1000.0f / dt;

      rpm          = revPerSec * 60.0f;
      omega        = revPerSec * 2.0f * PI;
      angularAccel = (omega - lastOmega) / (dt / 1000.0f);

      lastOmega = omega;
    }

    lastDipTime = now;
    dipCount++;

    // Output
    Serial.print("Dip #");    Serial.print(dipCount);
    Serial.print(" | RPM: "); Serial.print(rpm, 1);
    Serial.print(" | w: ");   Serial.print(omega, 2);
    Serial.print(" rad/s | a: "); Serial.print(angularAccel, 2);
    Serial.println(" rad/s2");
  }

  // --- Step 5: Hysteresis — exit dip when signal recovers 50% ---
  if (dipState && delta > DIP_THRESHOLD * 0.5f) {
    dipState = false;
  }

  // --- Step 6: Plot output (Serial Plotter compatible) ---
  Serial.print("Filtered:");   Serial.print(filteredRaw, 2);
  Serial.print(",Delta:");     Serial.print(delta, 2);
  Serial.print(",Threshold:"); Serial.println(DIP_THRESHOLD);
}