# Hardware Documentation

This folder contains the hardware description for the low-cost NV-diamond magnetic field sensor.

## Scope

The hardware design focuses on the **core readout electronics** required to detect fluorescence changes from an NV-diamond sample:

- photodiode sensing
- transimpedance amplification
- analog filtering
- ADC conversion
- ESP32-S3 control and networking

Optics, mounts, and sample preparation are treated as experiment-specific extensions.

## Folder layout

```text
hardware/
├── README.md
├── assembly.md
├── bom.md
├── schematics/
│   └── README.md
├── pcb/
│   └── README.md
├── enclosure/
│   └── README.md
└── test-data/
    └── README.md
```

## Hardware block diagram

```text
NV fluorescence
      │
      ▼
  BPW34 photodiode
      │ current
      ▼
 MCP607 transimpedance amplifier
      │ voltage
      ▼
   RC low-pass filter
      │
      ▼
      ADS1115
      │ I2C
      ▼
   ESP32-S3 (M5 Stamp)
      │
      ├── USB / power
      └── Wi-Fi AP + browser UI
```

## Hardware goals

- keep the core electronics affordable
- minimize analog complexity for first builds
- support breadboard testing before PCB integration
- leave room for future optical and microwave upgrades

## Recommended documentation to add over time

- finalized schematic PDFs or source files
- PCB layout snapshots and fabrication outputs
- measured analog response plots
- noise characterization data
- enclosure or optical mount drawings
