# Fraunhofer NV-Center Project — Claude Context

## What this project is

ESP32-S3 firmware + hardware design for an NV-center (nitrogen-vacancy diamond) optical readout system at Fraunhofer IAO. The system reads photon-count signals, applies quenching pulses via a custom PCB, and serves a live data dashboard over Wi-Fi.

## Repository layout

- `Design/` — Autodesk Inventor CAD (`.iam` assemblies, `.ipt` parts, `.stl` prints)
- `Design Photos/` — build photos and design plots
- `ESP32/ReadOut/` — PlatformIO firmware; four variants (base, web server, optimized, noise reduction)
- `Ordered PCB/` — manufacturing files for PCB rev 5 (Gerber, BOM, pick-and-place, schematic)
- `Onboarding/` — personal HR documents; **excluded from git**

## Key hardware

- MCU: ESP32-S3
- External ADC: ADS1015 / ADS1115 (I2C)
- Photodetector: BPW34
- Quenching PCB rev 5 (2026-02-13)

## Firmware

Built with PlatformIO (Arduino framework). Main work happens in:
- `ESP32/ReadOut/ESP32S3 ReadOut Web Server Optimized/` — production variant
- `ESP32/ReadOut/ESP32S3 Noise Optimization/` — ADC noise experiments

## What NOT to touch

- `Onboarding/` — HR/personal data, never commit
- `*.o`, `*.d`, `*.a`, `*.elf` — PlatformIO build artifacts, always ignored
