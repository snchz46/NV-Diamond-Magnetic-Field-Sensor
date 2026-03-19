# `src/` Code Layout

This directory should hold the runtime implementation of the firmware.

## Example source tree

```text
src/
├── main.cpp
├── app/
│   ├── app_controller.cpp
│   └── app_controller.h
├── acquisition/
│   ├── ads1115_sampler.cpp
│   ├── ads1115_sampler.h
│   ├── sample_buffer.cpp
│   └── sample_buffer.h
├── processing/
│   ├── averaging.cpp
│   ├── iir_filter.cpp
│   ├── baseline_tracker.cpp
│   ├── dip_detector.cpp
│   └── rpm_estimator.cpp
├── network/
│   ├── wifi_ap.cpp
│   ├── http_server.cpp
│   └── websocket_stream.cpp
├── ui/
│   └── embedded_assets.cpp
└── storage/
    └── csv_export.cpp
```

## Runtime sequence

```text
boot
 ├─ initialize board peripherals
 ├─ initialize ADS1115
 ├─ load default configuration
 ├─ start Wi-Fi access point
 ├─ start HTTP and WebSocket services
 └─ enter sampling / processing loop
```

## Implementation guidance

- keep ISR usage minimal unless later timing requirements justify it
- avoid dynamic allocation in high-rate processing paths when practical
- expose processing outputs through small structs instead of many globals
- keep plotting payloads lightweight to avoid blocking acquisition
