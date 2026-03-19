# Low-Cost NV-Diamond Magnetic Field Sensor

A low-cost research prototype for magnetic field sensing using fluorescence quenching from nitrogen-vacancy (NV) centers in diamond.

This repository documents a complete sensing stack built around an optical readout chain, an ESP32-S3 microcontroller, and a browser-based interface for real-time visualization and data export.

## Overview

The system measures changes in NV-diamond fluorescence intensity caused by variations in the local magnetic environment. The current prototype focuses on **optical quenching readout without microwave excitation**, prioritizing simplicity, low cost, and accessibility for teaching laboratories and early-stage experiments.

### Key characteristics

- **Sensing principle:** fluorescence intensity change from NV centers in diamond
- **Analog front-end:** BPW34 photodiode → MCP607 transimpedance amplifier → RC low-pass filter → ADS1115 ADC
- **Controller:** ESP32-S3 based M5 Stamp module
- **Acquisition rate:** ADS1115 at up to 860 samples/s
- **Firmware pipeline:** hardware averaging, dual IIR filtering, baseline subtraction, dip detection, and RPM/event-rate estimation
- **Connectivity:** Wi-Fi access point, HTTP server, WebSocket streaming, CSV export
- **Target bill of materials:** **below 20 €** for the core electronics, excluding optics, mechanics, and the diamond sample

## Why this repository exists

This repository is intended to be useful in three settings:

- **Student projects:** clear architecture, straightforward hardware, and readable documentation
- **Research prototyping:** modular firmware and explicit signal-processing stages that can be modified or extended
- **Instrumentation development:** a documented starting point for future lock-in, ODMR, or closed-loop sensing approaches

## Repository structure

```text
.
├── docs/
│   ├── architecture.md
│   └── theory.md
├── firmware/
│   ├── README.md
│   ├── include/
│   ├── lib/
│   └── src/
│       └── README.md
├── hardware/
│   ├── README.md
│   ├── assembly.md
│   ├── bom.md
│   ├── schematics/
│   ├── pcb/
│   ├── enclosure/
│   └── test-data/
├── LICENSE
└── README.md
```

## System architecture

```text
              Optical path                                  Embedded processing
┌───────────────┐   fluorescence   ┌────────┐   current   ┌────────┐   voltage   ┌────────┐
│ NV diamond +  │ ───────────────► │ BPW34  │ ───────────► │ MCP607 │ ───────────► │ RC LPF │
│ excitation    │                  │ diode  │             │  TIA   │             │ filter │
└───────────────┘                  └────────┘             └────────┘             └────────┘
                                                                                       │
                                                                                       ▼
                                                                                  ┌────────┐
                                                                                  │ADS1115 │
                                                                                  │ 16-bit │
                                                                                  └────────┘
                                                                                       │ I2C
                                                                                       ▼
                                                                                  ┌──────────┐
                                                                                  │ ESP32-S3 │
                                                                                  └──────────┘
                                                                                       │
                                          ┌────────────────────────────────────────────┼──────────────────────────────────────────┐
                                          ▼                                            ▼                                          ▼
                                   Display filter                                Detection filter                          Web services
                                   Baseline removal                              Dip / hysteresis                           HTTP + WS + CSV
```

## Signal-processing pipeline

The firmware separates visualization from detection so that the browser interface can remain smooth without reducing event sensitivity.

```text
ADC samples
   │
   ├─► hardware averaging
   │
   ├─► IIR filter (display path) ───────► web plot / live display
   │
   └─► IIR filter (detection path)
             │
             ├─► baseline estimation
             │
             ├─► baseline subtraction
             │
             ├─► dip detection with hysteresis
             │
             └─► event interval / RPM estimation
```

## Features

### Embedded acquisition
- ADS1115 sampling at up to 860 samples/s
- deterministic acquisition loop with tunable averaging
- configurable baseline and threshold parameters
- modular processing blocks to support later algorithm upgrades

### Real-time user interface
- browser-based canvas plot for live intensity traces
- tunable processing and detector parameters exposed through the web UI
- WebSocket transport for low-latency updates
- CSV export for offline analysis in Python, MATLAB, or spreadsheets

### Low-cost hardware strategy
- readily available photodiode, op-amp, ADC, and ESP32-class controller
- simple analog front-end suitable for breadboard or compact PCB builds
- documentation organized for iterative hardware refinement

## Design goals

1. **Keep the electronics affordable.** The core sensing electronics should remain below roughly **20 €** in typical hobbyist or educational quantities.
2. **Make the signal chain explicit.** Each hardware and firmware stage should be understandable, measurable, and replaceable.
3. **Support experimentation.** The system should be easy to adapt for different filters, thresholds, optics, and mechanical arrangements.
4. **Provide immediate feedback.** Real-time visualization helps users align optics, tune thresholds, and inspect noise sources.

## Intended workflow

1. Assemble the analog front-end and connect the ADS1115 to the ESP32-S3.
2. Illuminate the NV-diamond sample and collect fluorescence on the BPW34 photodiode.
3. Stream acquired data to the browser UI over the ESP32 Wi-Fi access point.
4. Tune averaging, filters, and dip-detection thresholds from the web interface.
5. Export CSV data for later analysis and documentation.

## Documentation map

- [`docs/architecture.md`](docs/architecture.md) — hardware, firmware, networking, and data-flow architecture
- [`docs/theory.md`](docs/theory.md) — sensing principle, measurement constraints, and practical interpretation
- [`firmware/README.md`](firmware/README.md) — suggested firmware module layout and responsibilities
- [`hardware/README.md`](hardware/README.md) — hardware organization, schematic expectations, and test strategy
- [`hardware/bom.md`](hardware/bom.md) — example low-cost bill of materials
- [`hardware/assembly.md`](hardware/assembly.md) — recommended assembly and bring-up sequence

## Project maturity

This repository is structured as a **serious prototype** rather than a finished instrument. The present design is intended for:

- proof-of-concept sensing
- educational demonstrations
- signal-chain validation
- low-cost experimentation before moving to more advanced NV protocols

It is **not** yet a calibrated magnetometer and does not currently implement microwave-driven ODMR techniques.

## Roadmap ideas

- microwave excitation and frequency sweep support
- improved calibration and temperature compensation
- logged experiments with timestamped metadata
- optional SD-card storage
- multi-channel optical readout
- desktop analysis notebooks and reference datasets

## Contributing

Contributions should preserve three qualities:

- technical clarity
- modular implementation
- reproducible documentation

When adding new hardware or algorithms, document both the design rationale and the expected effect on signal quality.
