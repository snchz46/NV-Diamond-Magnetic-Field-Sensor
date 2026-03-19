# Firmware Organization

This folder contains the embedded application for the ESP32-S3 based controller.

The goal is to keep the firmware modular so that acquisition, processing, networking, and UI serving can evolve independently.

## Recommended layout

```text
firmware/
├── README.md
├── platformio.ini              # optional if PlatformIO is used
├── include/
│   ├── config.h
│   ├── data_types.h
│   └── defaults.h
├── lib/
│   └── README.md
└── src/
    ├── README.md
    ├── main.cpp
    ├── app/
    ├── acquisition/
    ├── processing/
    ├── network/
    ├── ui/
    └── storage/
```

## Module responsibilities

| Module | Responsibility |
|---|---|
| `app/` | boot sequence, task coordination, global state transitions |
| `acquisition/` | ADS1115 setup, sampling loop, averaging, buffering |
| `processing/` | IIR filters, baseline tracking, dip detection, RPM calculation |
| `network/` | Wi-Fi AP mode, HTTP server, WebSocket streaming |
| `ui/` | embedded web assets or asset registration |
| `storage/` | CSV export and optional future persistent logging |
| `include/` | shared types, defaults, compile-time configuration |

## Design recommendations

### Separate raw, display, and detection values

Use explicit data structures for:
- raw ADC samples
- averaged values
- display-filter output
- detection-filter output
- baseline-corrected signal
- event and rate estimates

This makes debugging easier and avoids confusing processing stages.

### Keep configuration centralized

Parameters such as the following should be declared in one place:
- ADC data rate
- averaging count
- display filter coefficient
- detection filter coefficient
- baseline adaptation rate
- dip threshold values
- hysteresis band
- WebSocket update interval

### Prefer stateless transport objects

Networking code should transport measurements rather than owning core processing logic. This keeps processing testable and easier to modify.

## Suggested development order

1. bring up ADS1115 reading and serial debug output
2. add averaging and simple filters
3. add baseline subtraction and dip detection
4. expose measurements through HTTP and WebSocket
5. serve the browser UI from the device
6. add CSV export and saved configuration if needed

## Notes for future expansion

The present architecture is compatible with later additions such as:
- ODMR-related control modules
- additional ADC channels
- local storage
- calibration routines
- remote experiment metadata capture
