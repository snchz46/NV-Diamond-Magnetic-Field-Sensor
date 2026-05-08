/**
 * web_ui.h — Public interface for the HTTP / WebSocket web server
 *
 * This module manages:
 *   • A WiFi soft access point (AP) at 192.168.10.1
 *   • An mDNS hostname (http://nv-plotter.local)
 *   • An HTTP server (port 80) serving the single-page plotter UI and CSV export
 *   • A WebSocket server (port 81) for real-time bidirectional data exchange
 *
 * Clients connect to the AP, navigate to the IP or mDNS hostname, and receive
 * live signal data as JSON frames over the WebSocket connection. They can also
 * send configuration updates (IIR α, averaging, threshold) back to the ESP32
 * via the same WebSocket channel.
 */

#pragma once
#include <Arduino.h>

/**
 * web_init() — Start the WiFi AP, HTTP server, WebSocket server, and mDNS.
 *
 * Must be called once from setup() after nv_init() has succeeded.
 *
 * @return true on success (currently always true; kept for future error paths).
 */
bool web_init();

/**
 * web_loop() — Service pending HTTP and WebSocket events.
 *
 * Must be called on every iteration of loop() to prevent client timeouts and
 * ensure incoming WebSocket messages (config updates, recal requests) are
 * processed promptly.
 */
void web_loop();

/**
 * web_publish_sample() — Broadcast one processed sample to all WebSocket clients.
 *
 * Serialises the provided values into a compact JSON frame and sends it to
 * every currently connected client. Safe to call even when no clients are
 * connected (the broadcast is a no-op in that case).
 *
 * @param raw      Averaged raw ADC value (ADC counts).
 * @param filtered Slow IIR-filtered value (ADC counts).
 * @param delta    Filtered value minus the captured baseline (ADC counts).
 * @param dip      true if a new dip event was detected on this sample.
 * @param rpm      Current rolling-average RPM estimate (0 if not yet computed).
 */
void web_publish_sample(float raw, float filtered, float delta, bool dip, float rpm);