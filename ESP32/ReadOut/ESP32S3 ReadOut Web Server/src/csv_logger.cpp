#include "csv_logger.h"
#include <WebServer.h>

static const int CSV_BUF_N = 2000;
static NvSample buf[CSV_BUF_N];
static int head = 0;
static bool full = false;

void csv_push(const NvSample& s) {
  buf[head++] = s;
  if (head >= CSV_BUF_N) { head = 0; full = true; }
}

void csv_write_to_client(WebServer& server) {
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "text/csv", "");
  server.sendContent("t_ms,raw,filtered,delta,dip,rpm\n");

  int count = full ? CSV_BUF_N : head;
  int start = full ? head : 0;

  for (int i = 0; i < count; i++) {
    int idx = (start + i) % CSV_BUF_N;
    const NvSample &s = buf[idx];
    char line[160];
    snprintf(line, sizeof(line),
             "%lu,%.3f,%.3f,%.3f,%u,%.3f\n",
             (unsigned long)s.t_ms, s.raw, s.filtered, s.delta, s.dip ? 1 : 0, s.rpm);
    server.sendContent(line);
  }
  server.sendContent("");
}
