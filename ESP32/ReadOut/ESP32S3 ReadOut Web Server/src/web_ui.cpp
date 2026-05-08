#include "web_ui.h"
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ESPmDNS.h>

#include "web_page.h"
#include "nv_signal.h"
#include "csv_logger.h"

static const char* AP_SSID = "NV-Plotter-2";
static const char* AP_PASS = "12345678";

// Custom IP configuration
static IPAddress local_ip(192, 168, 10, 1);
static IPAddress gateway(192, 168, 10, 1);
static IPAddress subnet(255, 255, 255, 0);

static WebServer server(80);
static WebSocketsServer ws(81);

// --------------------------------------------------

static bool extractNumber(const String& s, const char* key, float &out) {
  int k = s.indexOf(key);
  if (k < 0) return false;
  k = s.indexOf(':', k);
  if (k < 0) return false;

  int start = k + 1;
  while (start < (int)s.length() && (s[start] == ' ' || s[start] == '"')) start++;

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

// --------------------------------------------------

static void handleRoot() {
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
  server.sendHeader("Pragma", "no-cache");
  server.sendHeader("Expires", "0");
  server.send_P(200, "text/html", INDEX_HTML);
}

static void handleExportCSV() {
  csv_write_to_client(server);
}

// --------------------------------------------------

static void onWsEvent(uint8_t clientNum, WStype_t type, uint8_t *payload, size_t length) {
  if (type != WStype_TEXT || !payload || length == 0) return;

  String msg;
  for (size_t i = 0; i < length; i++) msg += (char)payload[i];

  NvConfig cfg = nv_get_config();
  bool changed = false;

  float v;
  if (extractNumber(msg, "\"alpha\"", v)) { cfg.alpha = v; changed = true; }
  if (extractNumber(msg, "\"avg\"", v))   { cfg.avg   = (uint16_t)v; changed = true; }
  if (extractNumber(msg, "\"thr\"", v))   { cfg.thr   = v; changed = true; }

  if (changed) {
    nv_set_config(cfg);
    NvConfig now = nv_get_config();

    char ack[128];
    snprintf(ack, sizeof(ack),
      "{\"type\":\"cfg_ack\",\"alpha\":%.3f,\"avg\":%u,\"thr\":%.2f}",
      now.alpha, now.avg, now.thr);
    ws.sendTXT(clientNum, ack);
  }
}

// --------------------------------------------------

bool web_init() {
  WiFi.mode(WIFI_AP);

  // 👉 IP fija
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASS);

  // 👉 mDNS
  if (MDNS.begin("nv-plotter")) {
    Serial.println("mDNS responder started: http://nv-plotter.local");
  }

  server.on("/", handleRoot);
  server.on("/export.csv", handleExportCSV);
  server.begin();

  ws.begin();
  ws.onEvent(onWsEvent);

  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("Open: http://nv-plotter.local");

  return true;
}

void web_loop() {
  server.handleClient();
  ws.loop();
}

void web_publish_sample(float raw, float filtered, float delta, bool dip, float rpm) {
  char msg[200];
snprintf(msg, sizeof(msg),
  "{\"t\":%lu,\"raw\":%.2f,\"filtered\":%.2f,\"delta\":%.2f,\"dip\":%d,\"rpm\":%.2f}",
  (unsigned long)millis(), raw, filtered, delta, dip ? 1 : 0, rpm);

  ws.broadcastTXT(msg);
}
