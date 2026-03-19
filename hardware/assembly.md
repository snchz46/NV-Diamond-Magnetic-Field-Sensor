# Assembly and Bring-Up

This document outlines a practical build order for the hardware prototype.

## 1. Recommended assembly sequence

### Step 1 — Build the analog front-end

Assemble:
- BPW34 photodiode stage
- MCP607 transimpedance amplifier
- RC filter stage
- local decoupling capacitors

At this point, do **not** assume the optical signal is correct until the analog chain has been checked electrically.

### Step 2 — Verify power and reference conditions

Check:
- supply voltage at the op-amp
- ADC supply voltage
- common ground continuity
- expected DC operating point with no light and with illumination

### Step 3 — Connect the ADC

Wire the ADS1115 to the analog output and to the ESP32-S3 I2C bus.

Verify:
- I2C address visibility
- stable ADC readings at rest
- no clipping under expected light levels

### Step 4 — Bring up firmware

Start with:
- raw ADC readout over serial
- fixed-rate sampling
- simple averaging

Only after raw acquisition is stable should filtering and event detection be enabled.

### Step 5 — Enable web interface

Once the numeric signal looks reasonable:
- enable Wi-Fi AP mode
- open the browser UI
- compare plotted values against serial diagnostics
- tune filter and threshold parameters

## 2. Practical measurement checks

### Dark check
Shield the photodiode from light and observe:
- ADC offset
- drift
- noise floor

### Illumination check
Apply controlled illumination and verify:
- polarity of the signal
- absence of amplifier saturation
- expected response magnitude

### Dynamic check
Introduce a repeatable optical or magnetic perturbation and verify:
- visible dips or changes in the live trace
- stable threshold crossings
- sensible event-rate estimates

## 3. Common prototype issues

| Symptom | Likely cause | Suggested action |
|---|---|---|
| Flat saturated output | TIA gain too high or too much light | reduce gain or attenuate light |
| Very noisy signal | poor grounding or ambient light leakage | improve shielding and wiring |
| No ADC change | wiring fault or wrong signal polarity | verify analog node with multimeter or oscilloscope |
| Missed events | too much smoothing or wrong thresholds | retune detection path |
| Unstable UI updates | network payload too heavy | reduce update rate or packet size |

## 4. Suggested lab documentation

For reproducibility, record:
- op-amp gain values
- RC filter values
- ADC gain/range setting
- sample rate and averaging count
- illumination conditions
- ambient light control method
- browser-side parameter settings
