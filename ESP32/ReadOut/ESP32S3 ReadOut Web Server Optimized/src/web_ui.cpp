/**
 * web_ui.cpp — WiFi AP, HTTP server, WebSocket server, and mDNS
 *
 * Network topology
 * ────────────────
 *   ESP32 acts as a WiFi Access Point.
 *   Clients connect to SSID "NV-Plotter-3" and navigate to:
 *     http://192.168.10.1        (static IP)
 *     http://nv-plotter.local    (mDNS — works on most modern OS/browsers)
 *
 * HTTP routes
 * ───────────
 *   GET /            → Serves the single-page plotter UI (from PROGMEM)
 *   GET /export.csv  → Streams the CSV ring buffer as a downloadable file
 *   GET /recal       → Triggers a blocking baseline recapture (~300 ms)
 *
 * WebSocket (port 81)
 * ───────────────────
 *   Server → Client  (JSON text frames):
 *     {"t":…,"raw":…,"filtered":…,"delta":…,"dip":0|1,"rpm":…}
 *     {"type":"recal_done"}
 *     {"type":"cfg_ack","alpha":…,"avg":…,"thr":…}
 *
 *   Client → Server  (JSON text frames):
 *     {"type":"cfg","alpha":…,"avg":…,"thr":…}   → update config
 *     {"recal":true}                              → trigger recalibration
 */

#include "web_ui.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>

#include "web_page.h"
#include "nv_signal.h"
#include "csv_logger.h"

// ── WiFi AP credentials ───────────────────────────────────────────────────────
static const char* AP_SSID = "NV-Plotter-8";
static const char* AP_PASS = "12345678";

// Static IP configuration for the soft AP
static IPAddress local_ip(192, 168, 10, 1);
static IPAddress gateway(192, 168, 10, 1);
static IPAddress subnet(255, 255, 255, 0);

// HTTP server on the standard port; handles UI and CSV export
static WebServer         server(80);

// WebSocket server on port 81; used for real-time bidirectional data exchange
static WebSocketsServer  ws(81);

// ── Minimal JSON number extractor ─────────────────────────────────────────────
/**
 * extractNumber() — Parse a single numeric value from a flat JSON string.
 *
 * Searches for `key` (e.g. `"alpha"`), locates the colon that follows, then
 * reads contiguous digit / sign / decimal characters as a float. This avoids
 * pulling in a full JSON library for such a simple use case.
 *
 * @param s    The raw JSON text received from the WebSocket client.
 * @param key  The JSON key string to look for (including quotes, e.g. `"avg"`).
 * @param out  Set to the parsed value if found.
 * @return     true if the key was found and a number was successfully parsed.
 */
static bool extractNumber(const String& s, const char* key, float &out) {
  int k = s.indexOf(key);
  if (k < 0) return false;
  k = s.indexOf(':', k);
  if (k < 0) return false;

  // Skip whitespace and any stray quote characters after the colon
  int start = k + 1;
  while (start < (int)s.length() && (s[start] == ' ' || s[start] == '"')) start++;

  // Walk forward collecting numeric characters
  int end = start;
  while (end < (int)s.length()) {
    char c = s[end];
    if ((c >= '0' && c <= '9') || c=='-' || c=='+' || c=='.') end++;
    else break;
  }
  if (end <= start) return false;
  out = s.substring(start, end).toFloat();
  return true;
}

// ── HTTP handlers ─────────────────────────────────────────────────────────────

/**
 * handleRoot() — Serve the embedded single-page plotter UI.
 *
 * Cache-control headers are set aggressively to "no-store" so that after a
 * firmware update the browser always receives the new page rather than a
 * stale cached copy.
 */
static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma",  "no-cache");
  server.sendHeader("Expires", "0");
  server.send_P(200, "text/html", INDEX_HTML);  // INDEX_HTML lives in PROGMEM
}

/**
 * handleExportCSV() — Stream the CSV ring buffer to the client as a download.
 *
 * Delegates entirely to csv_write_to_client() which uses chunked transfer so
 * the full buffer does not need to fit in RAM as a single string.
 */
static void handleExportCSV() {
  csv_write_to_client(server);
}

/**
 * handleRecal() — HTTP endpoint: GET /recal
 *
 * Triggers a blocking baseline recapture (captureBaseline() inside nv_signal,
 * ~300 ms). After completion, broadcasts a "recal_done" WebSocket message to
 * all connected clients so their delta displays can reset, then returns 200.
 *
 * The browser UI calls this when the user presses the "Recalibrate" button.
 */
static void handleRecal() {
  nv_recalibrate();
  // Notify all connected WebSocket clients so they can reset their delta view
  ws.broadcastTXT("{\"type\":\"recal_done\"}");
  server.send(200, "text/plain", "OK");
}

// ── WebSocket event handler ───────────────────────────────────────────────────
/**
 * onWsEvent() — Handle incoming WebSocket text frames from browser clients.
 *
 * Supported messages:
 *   • {"recal":…}           → Re-run baseline calibration
 *   • {"alpha":…}           → Update IIR display-filter coefficient
 *   • {"avg":…}             → Update ADC hardware averaging count
 *   • {"thr":…}             → Update dip-detection threshold
 *
 * Any combination of alpha/avg/thr may appear in a single message.
 * After applying a configuration change, an acknowledgement frame is sent
 * back to the requesting client showing the clamped/accepted values.
 */
static void onWsEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length) {
  // Only process text frames with actual content
  if (type != WStype_TEXT || !payload || length == 0) return;

  // Reassemble the byte payload into an Arduino String for easy parsing
  String msg;
  for (size_t i = 0; i < length; i++) msg += (char)payload[i];

  // ── Recalibration command ────────────────────────────────────────────────
  // Accepts the WebSocket path as an alternative to the HTTP /recal route.
  if (msg.indexOf("\"recal\"") >= 0) {
    nv_recalibrate();
    ws.broadcastTXT("{\"type\":\"recal_done\"}");
    return;
  }

  // ── Configuration update ─────────────────────────────────────────────────
  NvConfig cfg     = nv_get_config();  // Start from current values
  bool     changed = false;

  float v;
  if (extractNumber(msg, "\"alpha\"", v)) { cfg.alpha = v;              changed = true; }
  if (extractNumber(msg, "\"avg\"",   v)) { cfg.avg   = (uint16_t)v;    changed = true; }
  if (extractNumber(msg, "\"thr\"",   v)) { cfg.thr   = v;              changed = true; }

  if (changed) {
    nv_set_config(cfg);  // Applies clamping internally

    // Echo back the clamped values so the UI can correct out-of-range inputs
    NvConfig now = nv_get_config();
    char ack[128];
    snprintf(ack, sizeof(ack),
      "{\"type\":\"cfg_ack\",\"alpha\":%.3f,\"avg\":%u,\"thr\":%.2f}",
      now.alpha, now.avg, now.thr);
    ws.sendTXT(clientNum, ack);
  }
}

// ── Initialisation ────────────────────────────────────────────────────────────

bool web_init() {
  // Configure and start the WiFi soft AP
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  // Start mDNS so clients on modern OSes can reach http://nv-plotter.local
  if (MDNS.begin("nv-plotter")) {
    Serial.println("mDNS responder started: http://nv-plotter.local");
  }

  // Register HTTP route handlers
  server.on("/",           handleRoot);
  server.on("/export.csv", handleExportCSV);
  server.on("/recal",      handleRecal);
  server.begin();

  // Start WebSocket server and register the event callback
  ws.begin();
  ws.onEvent(onWsEvent);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Open: http://nv-plotter.local");

  return true;
}

// ── Main loop service ─────────────────────────────────────────────────────────

void web_loop() {
  server.handleClient();  // Process any pending HTTP request
  ws.loop();              // Process any pending WebSocket frames / heartbeats
}

// ── Sample publishing ─────────────────────────────────────────────────────────

void web_publish_sample(float raw, float filtered, float delta, bool dip, float rpm) {
  // Serialise the sample as a compact JSON object and broadcast to all clients.
  // millis() is included as the timestamp so the browser can compute relative
  // time without needing a wall-clock.
  char msg[200];
  snprintf(msg, sizeof(msg),
    "{\"t\":%lu,\"raw\":%.2f,\"filtered\":%.2f,\"delta\":%.2f,\"dip\":%d,\"rpm\":%.2f}",
    (unsigned long)millis(), raw, filtered, delta, dip ? 1 : 0, rpm);
  ws.broadcastTXT(msg);
}