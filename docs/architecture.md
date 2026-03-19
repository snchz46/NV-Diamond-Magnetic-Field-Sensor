# System Architecture

This document describes the hardware, firmware, and software organization of the low-cost NV-diamond magnetic field sensor.

## 1. Top-level architecture

```text
Optical excitation / sample
        │
        ▼
NV-diamond fluorescence
        │
        ▼
Photodiode detection (BPW34)
        │
        ▼
Transimpedance amplification (MCP607)
        │
        ▼
Analog RC filtering
        │
        ▼
ADS1115 digitization
        │ I2C
        ▼
ESP32-S3 firmware
        ├── acquisition
        ├── filtering
        ├── baseline removal
        ├── dip detection
        ├── event-rate estimation
        └── network services
               ├── HTTP configuration UI
               ├── WebSocket live stream
               └── CSV export
```

## 2. Hardware blocks

### 2.1 Optical and sensor stage

The optical stage is intentionally simple:

- an illuminated NV-diamond sample produces fluorescence
- the BPW34 photodiode converts received optical power into current
- no microwave excitation is required in the first prototype

This keeps the first version accessible for teaching and early hardware verification.

### 2.2 Analog front-end

The analog chain is optimized for low-cost intensity readout.

| Stage | Function | Design intent |
|---|---|---|
| BPW34 photodiode | Converts light to current | Low cost, wide availability |
| MCP607 transimpedance amplifier | Converts current to voltage | Low input bias current, single-supply operation |
| RC filter | Suppresses high-frequency noise | Simple anti-noise stage before ADC |
| ADS1115 | Digitizes conditioned signal | 16-bit ADC with easy I2C integration |

### 2.3 Embedded controller

The ESP32-S3 is responsible for:

- configuring and reading the ADS1115
- processing incoming samples in real time
- serving the browser UI over Wi-Fi AP mode
- streaming derived data with low latency via WebSocket
- exporting recorded windows as CSV

## 3. Firmware architecture

The firmware should be organized by responsibility rather than by device driver alone.

```text
firmware/
├── src/
│   ├── main.cpp
│   ├── app/
│   │   ├── app_controller.cpp
│   │   └── app_controller.h
│   ├── acquisition/
│   │   ├── ads1115_sampler.cpp
│   │   └── sample_buffer.cpp
│   ├── processing/
│   │   ├── averaging.cpp
│   │   ├── iir_filter.cpp
│   │   ├── baseline_tracker.cpp
│   │   ├── dip_detector.cpp
│   │   └── rpm_estimator.cpp
│   ├── network/
│   │   ├── wifi_ap.cpp
│   │   ├── http_server.cpp
│   │   └── websocket_stream.cpp
│   ├── ui/
│   │   └── embedded_assets.cpp
│   └── storage/
│       └── csv_export.cpp
└── include/
    ├── config.h
    ├── data_types.h
    └── defaults.h
```

### 3.1 Core modules

#### Acquisition
- schedules ADS1115 reads
- applies hardware averaging
- timestamps samples or sample windows
- forwards samples to processing stages

#### Processing
- maintains separate filter paths for display and detection
- tracks slow baseline drift
- identifies dips with hysteresis to avoid chatter
- estimates interval, event rate, or RPM-like quantities

#### Network services
- starts Wi-Fi access point mode
- hosts static UI assets over HTTP
- publishes real-time packets through WebSocket
- exposes configuration and export endpoints

#### Configuration
- stores tunable parameters such as gain, thresholds, filter constants, and averaging depth
- centralizes default values so experiments are reproducible

## 4. Data flow and timing

At 860 samples/s, the firmware must remain deterministic enough to avoid UI updates interfering with detection logic.

### Recommended processing separation

```text
ADS1115 sample
   │
   ├─► acquisition queue / buffer
   │
   ├─► display filter ───────► downsampled websocket payload ───────► browser plot
   │
   └─► detection filter ─────► baseline subtraction ─────► hysteretic dip detector ─────► rate estimate
```

This separation avoids tying detection sensitivity to plotting requirements.

## 5. Browser UI architecture

The browser interface should remain lightweight and directly useful during experiments.

### Suggested UI panels

| Panel | Purpose |
|---|---|
| Live plot | Show raw or filtered signal over time |
| Detection status | Show threshold crossings and current event count |
| Rate view | Display event frequency or RPM-style estimate |
| Configuration | Adjust averaging, filter coefficients, and detection thresholds |
| Export controls | Download recent data as CSV |

### UI design principles

- load quickly from the microcontroller
- keep dependencies minimal
- present numerical values alongside plots
- make parameter changes visible and reversible

## 6. Networking model

### Wi-Fi access point mode

Access point mode is useful for standalone demos and lab benches because it removes the dependency on institutional or home Wi-Fi.

Advantages:
- deterministic connection path
- simple onboarding from phone or laptop
- portable for demonstrations

Tradeoff:
- internet access is typically unavailable while connected to the device AP

### HTTP + WebSocket split

- **HTTP** serves the static UI and configuration endpoints
- **WebSocket** carries continuous live data without repeated polling overhead
- **CSV export** provides an analysis-friendly format for later work

## 7. Reliability considerations

Even in a prototype, several practices improve measurement quality:

- isolate analog ground return paths where possible
- keep photodiode and TIA wiring short
- reduce ambient light leakage in the optical enclosure
- separate UI refresh rate from processing rate
- store parameter values with exported data when possible

## 8. Extension paths

This architecture intentionally supports future upgrades:

- ODMR with microwave drive hardware
- alternative photodetectors or TIAs
- improved digital filtering strategies
- closed-loop control of illumination or sample position
- calibration routines and metadata logging

The modular structure allows those upgrades without rewriting the full repository.
