/**
 * csv_logger.cpp — Circular buffer CSV logger implementation
 *
 * The ring buffer holds the last CSV_BUF_N decimated samples (~8.3 minutes
 * at a 4-sample decimation ratio with a 1 ms step period). Once full, the
 * oldest entries are overwritten silently.
 *
 * Ring buffer layout:
 *   `head` always points to the NEXT write slot.
 *   When `full` is false, valid data occupies indices [0 .. head-1].
 *   When `full` is true,  valid data starts at `head` (oldest) and wraps
 *   around to `head-1` (newest), covering all CSV_BUF_N entries.
 */

#include "csv_logger.h"
#include <WebServer.h>

// ── Ring buffer ────────────────────────────────────────────────────────────────
static const int CSV_BUF_N = 2000;   // Maximum number of samples retained
static NvSample buf[CSV_BUF_N];      // Storage (statically allocated, ~32 KB)
static int  head = 0;                // Index of the next slot to write
static bool full = false;            // True once the buffer has wrapped at least once

// ── Write ──────────────────────────────────────────────────────────────────────

void csv_push(const NvSample& s) {
  buf[head++] = s;
  // When head reaches the end of the array, wrap back to 0 and mark full.
  // From this point forward, the oldest sample is always at position `head`
  // (the slot we are about to overwrite next).
  if (head >= CSV_BUF_N) { head = 0; full = true; }
}

// ── Export ─────────────────────────────────────────────────────────────────────

void csv_write_to_client(WebServer& server) {
  // Use chunked transfer so we never need to hold the full CSV in RAM.
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.sendHeader("Cache-Control", "no-store");

  // Suggest a filename; the browser honours this as the default save-as name.
  server.sendHeader("Content-Disposition", "attachment; filename=\"nv_export.csv\"");
  server.send(200, "text/csv", "");  // Send status + headers, no body yet

  // Write the column header row
  server.sendContent("t_s,raw,filtered,delta,dip,rpm\n");

  // Determine the number of valid samples and the index of the oldest one.
  int count = full ? CSV_BUF_N : head;
  int start = full ? head : 0;  // Oldest entry (wraps when buffer is full)

  // Compute the timestamp of the oldest sample so all rows can be expressed
  // as seconds elapsed since that point. This makes the CSV immediately
  // usable in a spreadsheet without manual epoch subtraction.
  uint32_t t0 = buf[start % CSV_BUF_N].t_ms;

  for (int i = 0; i < count; i++) {
    // Walk through the ring buffer in chronological order
    int idx = (start + i) % CSV_BUF_N;
    const NvSample &s = buf[idx];

    // Convert millisecond timestamp to relative seconds
    float t_s = (float)(s.t_ms - t0) / 1000.0f;

    char line[160];
    snprintf(line, sizeof(line),
             "%.4f,%.3f,%.3f,%.3f,%u,%.3f\n",
             t_s, s.raw, s.filtered, s.delta, s.dip ? 1 : 0, s.rpm);
    server.sendContent(line);
  }

  // Send an empty final chunk to signal end of chunked transfer
  server.sendContent("");
}