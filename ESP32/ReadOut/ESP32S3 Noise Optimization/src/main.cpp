#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_ADS1X15.h>

Adafruit_ADS1115 ads;

// ---------- CONFIGURACIÓN ----------
//const int espAdcPin = 36;         // GPIO 36 (ESP WROOM 32)
const int espAdcPin = 5;            // GPIO 5 (STAMP S3)
const float VadcEsp = 2.65;         // Rango real ADC interno
const int ADCmaxEsp = 4095;

const float VadcADS = 4.096;        // Rango PGA ADS1115 (GAIN_ONE) 4.096 v
const int N = 4;                    // Promedio puntual
const int windowSize = 500;         // Ventana para cálculo de ruido
const uint32_t sampleInterval = 2000; // µs → 500 Hz aprox
// -----------------------------------

uint32_t lastSample = 0;

// Buffers circulares para mantener últimas lecturas
float bufferADS[windowSize];
float bufferESP[windowSize];
int idx = 0;
bool filled = false;

void setup() {
  Serial.begin(115200);
  delay(800);

  // --- ADC interno ---
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // --- ADS1115 ---
  Wire.begin(13, 15);         //(21, 22); // SDA, SCL para WROOM32
  if (!ads.begin(0x48)) {
    Serial.println("❌ No se detecta el ADS1115");
    while (true) delay(100);
  }
  //ads.setGain(GAIN_ONE);
  //ads.setDataRate(RATE_ADS1115_860SPS);

  ads.setGain(GAIN_FOUR);                // ±2.048 V  → 62.5 µV/LSB
  ads.setDataRate(RATE_ADS1115_128SPS); // menos ruido, banda suficiente


  Serial.println("=== Comparación de ruido continua: ADS1115 vs ADC interno ===");
  Serial.println("sample,adsV,espV,noiseADS_mV,noiseESP_mV");
}

void loop() {
  if (micros() - lastSample >= sampleInterval) {
    lastSample += sampleInterval;

    // --- ADS1115 ---
    long tmpADS = 0;
    for (int i = 0; i < N; i++) {
      tmpADS += ads.readADC_SingleEnded(0);
      delayMicroseconds(100);
    }
    float rawADS = tmpADS / (float)N;
    float vADS = (rawADS / 32768.0f) * VadcADS;

    // --- ADC interno ---
    long tmpESP = 0;
    for (int i = 0; i < N; i++) {
      tmpESP += analogRead(espAdcPin);
      delayMicroseconds(50);
    }
    float rawESP = tmpESP / (float)N;
    float vESP = (rawESP / (float)ADCmaxEsp) * VadcEsp;

    // --- Guarda las nuevas lecturas en el buffer circular ---
    bufferADS[idx] = vADS;
    bufferESP[idx] = vESP;
    idx = (idx + 1) % windowSize;
    if (idx == 0) filled = true;

    // --- Calcula ruido actual usando la ventana disponible ---
    int n = filled ? windowSize : idx;
    double sumADS = 0, sumESP = 0, sumSqADS = 0, sumSqESP = 0;
    for (int i = 0; i < n; i++) {
      sumADS += bufferADS[i];
      sumESP += bufferESP[i];
      sumSqADS += bufferADS[i] * bufferADS[i];
      sumSqESP += bufferESP[i] * bufferESP[i];
    }
    double meanADS = sumADS / n;
    double meanESP = sumESP / n;
    double varADS = (sumSqADS / n) - (meanADS * meanADS);
    double varESP = (sumSqESP / n) - (meanESP * meanESP);
    double noiseADS = sqrt(varADS) * 1000.0; // mV RMS
    double noiseESP = sqrt(varESP) * 1000.0; // mV RMS

    // --- Muestra resultados en CSV ---
    Serial.printf("vADS: %.6f\n", vADS);
  }
}
