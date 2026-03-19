# Example Bill of Materials

This bill of materials is intended for the **core electronics only**. It excludes the NV-diamond sample, excitation source, optics, mechanical mounts, and enclosure.

## Cost target

The target is to keep the core electronics near or below **20 €** in low-volume purchase quantities.

## Example BOM

| Item | Example part | Qty | Typical role | Approx. unit cost (€) | Approx. subtotal (€) |
|---|---|---:|---|---:|---:|
| Microcontroller | M5 Stamp ESP32-S3 or similar ESP32-S3 module | 1 | Control, Wi-Fi, web server | 6.00 | 6.00 |
| ADC | ADS1115 breakout or IC | 1 | 16-bit digitization | 2.50 | 2.50 |
| Photodiode | BPW34 | 1 | Fluorescence sensing | 1.20 | 1.20 |
| Op-amp | MCP607 | 1 | Transimpedance amplifier | 1.00 | 1.00 |
| Precision resistor(s) | Through-hole or SMD | 2-4 | TIA gain / bias network | 0.50 | 0.50 |
| Capacitors | Ceramic / film assortment | 4-8 | Filtering and decoupling | 0.60 | 0.60 |
| Proto PCB / small board | Generic | 1 | Assembly support | 1.50 | 1.50 |
| Connectors / headers | Generic | 1 set | Wiring and module connection | 1.20 | 1.20 |
| Power regulation / cable allowance | Generic | 1 | Stable power integration | 1.50 | 1.50 |
| Miscellaneous passives | Generic | 1 set | Pull-ups, RC tuning, spare parts | 1.50 | 1.50 |

**Estimated total:** approximately **17.50 €**

## Notes

- Costs depend strongly on distributor, package type, and whether modules or bare ICs are used.
- The ADS1115 and ESP32-S3 costs can vary the most depending on sourcing.
- If a prebuilt development board is used instead of a compact module, the total may exceed the 20 € target.
- The BOM target applies to electronics only, not the full optical experiment.

## Parts selection rationale

### BPW34
A common and inexpensive photodiode suitable for first optical sensing experiments.

### MCP607
A practical low-cost op-amp for single-supply transimpedance designs in this performance range.

### ADS1115
An easy-to-integrate 16-bit ADC that simplifies digital acquisition over I2C.

### ESP32-S3
Combines embedded control with enough compute and connectivity for hosting a compact browser UI.
