/**
 * csv_logger.h — Circular buffer CSV logger for NvSample data
 *
 * Maintains a fixed-size ring buffer of NvSample structs in RAM. When the
 * buffer is full, the oldest entry is silently overwritten, so the most
 * recent CSV_BUF_N samples are always available for export.
 *
 * The CSV is streamed directly to the HTTP client using chunked transfer
 * (no heap allocation for the full file), which is important on a device
 * with limited RAM.
 *
 * CSV column format (header: t_s, raw, filtered, delta, dip, rpm):
 *   t_s      – Elapsed seconds since the oldest sample in the export window.
 *              Expressed as a relative time so spreadsheets can be used
 *              immediately without subtracting a large millisecond epoch.
 *   raw      – Averaged raw ADC value (counts).
 *   filtered – Slow IIR-filtered value (counts).
 *   delta    – Filtered minus baseline (counts).
 *   dip      – 1 if a dip event was detected on this sample, 0 otherwise.
 *   rpm      – Rolling-average RPM at the time of this sample.
 */

#pragma once
#include <Arduino.h>
#include "nv_signal.h"

/**
 * csv_push() — Append one sample to the circular log buffer.
 *
 * If the buffer is full the oldest entry is overwritten (ring buffer
 * semantics). Should be called at the decimated output rate from main.cpp.
 *
 * @ param s  The sample to store.
 */
void csv_push(const NvSample& s);

/**
 * csv_write_to_client() — Stream the buffered samples as a CSV download.
 *
 * Sets response headers (Content-Disposition, Cache-Control) and sends the
 * CSV header row followed by all buffered samples in chronological order.
 * Uses CONTENT_LENGTH_UNKNOWN with sendContent() to avoid buffering the
 * entire file in RAM.
 *
 * @ param server  Reference to the active WebServer instance.
 */
void csv_write_to_client(class WebServer& server);