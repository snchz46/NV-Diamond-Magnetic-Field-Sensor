# NV-Center Optical Readout System — Fraunhofer IAO

Hardware and firmware project for a Nitrogen-Vacancy (NV) center-based optical sensing platform. The system acquires photon-count signals from NV-center diamond samples, applies quenching pulses, and streams data via a browser-accessible web interface hosted on an ESP32-S3.

---

## Project Structure

```
Fraunhofer/
├── Design/                        # Mechanical CAD (Autodesk Inventor)
│   ├── Parts/                     # Component libraries (PCB 3D models, connectors, sensors)
│   ├── Photos/                    # Renders and assembly photos
│   ├── Print/                     # 3D-printable STL exports
│   ├── OldVersions/               # Superseded design revisions
│   ├── Assembly_NV_Module.iam     # NV sensor module assembly
│   ├── Assembly_Optical_Module.iam
│   ├── Assembly_Quenching.iam
│   ├── Assembly_Box.iam           # Enclosure assembly
│   └── *.ipt / *.stl              # Individual parts and 3D-print files
│
├── Design Photos/                 # Step-by-step build photos and design plots
│
├── ESP32/                         # Firmware (PlatformIO / Arduino)
│   ├── ReadOut/
│   │   ├── ESP32S3 ReadOut/                   # Base data acquisition firmware
│   │   ├── ESP32S3 ReadOut Web Server/        # Web interface (HTML/JS served from ESP32)
│   │   ├── ESP32S3 ReadOut Web Server Optimized/  # Performance-tuned variant
│   │   └── ESP32S3 Noise Optimization/        # ADC noise reduction experiments
│   ├── servo/                     # Servo control sketch
│   └── ESP32 PinOut.png           # Pin assignment reference
│
└── Ordered PCB/                   # PCB manufacturing files (PCB revision 5)
    ├── Gerber_PCB5_2026-02-13.zip         # Gerber files for fab
    ├── BOM_Quenching_kit_PCB5_2026-02-13.xlsx
    ├── PickAndPlace_PCB5_2026-02-13.xlsx
    ├── SCH_Schematic4_2026-02-13.pdf      # Full schematic
    ├── 3D_PCB5.step                       # 3D PCB model
    └── OLD Schematics/                    # Previous PCB revisions
```

> **Note:** The `Onboarding/` folder contains personal HR and administrative documents and is excluded from version control via `.gitignore`.

---

## Hardware Overview

| Module | Description |
|--------|-------------|
| **NV Module** | Diamond sample holder with optical coupling |
| **Optical Module** | Photodetector (BPW34) with signal conditioning |
| **Quenching PCB (rev 5)** | Laser/MW pulse driver for spin initialization |
| **Readout Box** | Enclosure integrating all modules |
| **ESP32-S3** | Main MCU — ADC readout, Wi-Fi web server |
| **ADS1015/ADS1115** | External 12/16-bit ADC for precision readout |

---

## Firmware

Built with **PlatformIO** (Arduino framework). Open the relevant subfolder in VS Code with the PlatformIO extension.

```bash
# Example: open the optimized web-server variant
cd "ESP32/ReadOut/ESP32S3 ReadOut Web Server Optimized"
pio run --target upload
```

### Web Interface

After flashing, the ESP32 hosts a web page (default IP shown on serial monitor) with:
- Live ADC signal plot
- Start/Stop acquisition controls
- Data export

---

## PCB Manufacturing

The `/Ordered PCB/` folder contains all files required to order PCB revision 5:

- **Gerber** → upload `Gerber_PCB5_2026-02-13.zip` to your PCB fab (JLCPCB, PCBWay, etc.)
- **BOM** → `BOM_Quenching_kit_PCB5_2026-02-13.xlsx`
- **Pick & Place** → `PickAndPlace_PCB5_2026-02-13.xlsx`

---

## 3D Printing

STL files for the enclosure and module covers are in `Design/Print/` and `Design/*.stl`. Designed for FDM printing; no supports required for standard orientation.

---

## Dependencies

| Tool | Version / Notes |
|------|-----------------|
| Autodesk Inventor | 2024+ (for `.iam`/`.ipt` files) |
| PlatformIO | Latest (VS Code extension) |
| EasyEDA | v6.5.51 (installer in Onboarding/) |
| Arduino framework | ESP32 board package |

---

## Authors

Samuel Sanchez Moreno — Fraunhofer IAO (HiWi, 2025–2026)
