/**
 * main.cpp — Application entry point for the NV Quenching Plotter
 *
 * This firmware runs on an ESP32 and performs the following tasks in a
 * cooperative loop:
 *   1. Reads and filters ADC samples from an ADS1115 (via nv_signal).
 *   2. Detects "dip" events (drops in signal below a configurable threshold).
 *   3. Pushes decimated samples to a circular CSV log buffer.
 *   4. Broadcasts live data to connected browser clients over WebSocket.
 *
 * Hardware connections (I²C):
 *   SDA → GPIO 13
 *   SCL → GPIO 15
 */

#include <Arduino.h>
#include <Wire.h>
#include "nv_signal.h"
#include "web_ui.h"
#include "csv_logger.h"

// ── Decimation ────────────────────────────────────────────────────────────────
// nv_step() is called every loop iteration (potentially several hundred Hz),
// but the web UI and CSV buffer only receive every Nth sample.
// Reducing this reduces bandwidth and CPU; raising it increases data density.
static const uint8_t SEND_EVERY_N = 4;

// Running sample counter used for modulo-based decimation.
static uint32_t counter = 0;

// ── Dip accumulation across decimation window ─────────────────────────────────
// If a dip fires on any of the N raw samples between published outputs, this
// flag is set so the dip is NOT silently dropped by the decimation step.
static bool dipPending = false;

void setup() {
  Serial.begin(115200);
  delay(200);  // Allow USB serial to stabilise before first prints

  // Initialise I²C bus on non-default pins required by the PCB layout.
  Wire.begin(13, 15);

  // Initialise the ADS1115 ADC, set gain/data-rate, and capture the signal
  // baseline. Halts with an error message if the chip is not found.
  if (!nv_init()) {
    Serial.println("ADS1115 not found!");
    while (true) delay(200);  // Blink-of-death; sensor is required
  }

  // Start the WiFi AP, HTTP server, and WebSocket server.
  web_init();
}

void loop() {
  // Service any pending HTTP requests and WebSocket frames from connected
  // clients. Must be called frequently to avoid client timeouts.
  web_loop();

  // Read samplesToAvg fresh ADC conversions, apply IIR filtering, and run
  // dip-detection logic. Returns a fully populated NvSample struct.
  NvSample s = nv_step();

  // Latch any dip that fires inside the current decimation window so it is
  // still reported even if this particular raw sample did not carry it.
  if (s.dip) dipPending = true;

  if (++counter % SEND_EVERY_N == 0) {
    // Stamp the accumulated dip flag onto the outgoing (decimated) sample,
    // then clear it ready for the next decimation window.
    s.dip    = dipPending;
    dipPending = false;

    // Append to the circular CSV ring buffer (oldest entry is overwritten
    // once the buffer is full — see csv_logger.cpp).
    csv_push(s);

    // Broadcast a compact JSON message to all connected WebSocket clients.
    web_publish_sample(s.raw, s.filtered, s.delta, s.dip, s.rpm);
  }
}