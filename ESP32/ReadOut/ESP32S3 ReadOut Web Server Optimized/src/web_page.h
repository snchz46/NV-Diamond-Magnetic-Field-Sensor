/**
 * web_page.h — Declaration of the embedded single-page plotter UI
 *
 * The full HTML/CSS/JavaScript for the browser UI is stored in program
 * flash (PROGMEM) as a raw string literal in web_page.cpp, keeping it out
 * of the ESP32's limited data RAM.
 *
 * The UI provides:
 *   • A live scrolling canvas plot of the ADC signal (raw, filtered, or delta)
 *   • A metric strip showing current raw, filtered, delta, RPM, and dip state
 *   • Sliders for IIR α, averaging count, and dip-detection threshold
 *   • Freeze / Clear / Recalibrate / CSV-export controls
 *   • Touch (pinch/pan) and mouse (wheel/drag) gesture support for zoom/pan
 *   • Collapsible settings panel and an in-page usage guide
 *
 * Usage: include this header and pass INDEX_HTML to server.send_P() in
 * web_ui.cpp to serve the page over HTTP.
 */

#pragma once
#include <Arduino.h>

/** Null-terminated HTML document stored in program flash. */
extern const char INDEX_HTML[] PROGMEM;