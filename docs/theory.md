# Theory and Measurement Notes

This document provides a concise theoretical background for the NV-diamond sensing approach implemented in this repository.

## 1. Measurement principle

Nitrogen-vacancy (NV) centers in diamond can emit fluorescence under optical excitation. In practical systems, the measured fluorescence intensity can change with the local magnetic environment and with several non-magnetic influences such as alignment, temperature, excitation stability, and ambient light.

In this project, the instrument monitors **fluorescence intensity variations** rather than performing full microwave-driven optically detected magnetic resonance (ODMR).

That distinction is important:

- this prototype is useful for **signal-chain development** and **low-cost demonstrations**
- it is not yet equivalent to a calibrated vector or scalar magnetometer
- interpretation of intensity changes must be done carefully because multiple effects can contribute

## 2. Why use a photodiode front-end?

The BPW34 photodiode is attractive for low-cost optical experiments because it is:

- inexpensive
- easy to source
- electrically simple to integrate
- compatible with transimpedance readout

The photodiode generates a current proportional to incident optical power. Since the signal current is usually small, a transimpedance amplifier converts current into a measurable voltage.

## 3. Analog signal chain

### 3.1 Transimpedance amplifier

The MCP607 stage converts photodiode current into voltage.

Conceptually:

```text
photocurrent × transimpedance gain → output voltage
```

The transimpedance stage should be chosen to balance:

- sensitivity
- bandwidth
- amplifier stability
- saturation margin under expected illumination

### 3.2 RC filtering

The RC low-pass filter suppresses high-frequency noise before digitization.

Typical noise sources include:
- switching noise from digital electronics
- ambient optical fluctuations
- op-amp broadband noise
- aliasing of unwanted fast components

## 4. Digital acquisition and processing

The ADS1115 provides a simple 16-bit conversion path over I2C. Although not a high-speed ADC, it is well suited to educational and prototype systems where simplicity matters more than maximum bandwidth.

At up to 860 samples/s, the system can:

- monitor slow or moderate fluorescence changes
- support live plotting in a browser
- estimate repeated events or periodic dips
- provide a practical debugging view of the analog chain

## 5. Why dual filtering?

The firmware uses two different IIR filter paths because visualization and detection do not have identical requirements.

### Display path

The display path should:
- look stable to the user
- suppress distracting noise
- preserve enough dynamics for alignment and tuning

### Detection path

The detection path should:
- respond fast enough to real dips
- avoid missing events due to over-smoothing
- support threshold-based event logic

Using separate filters avoids a common problem in small embedded instruments: making the plotted signal attractive at the cost of detection performance.

## 6. Baseline subtraction

The fluorescence signal often contains slow drift caused by:

- LED or laser intensity change
- photodiode alignment changes
- temperature effects
- ambient background light
- analog offset drift

A baseline tracker estimates this slow component and subtracts it from the detection signal. The result is a signal where local deviations are easier to detect.

## 7. Dip detection with hysteresis

Threshold detection alone is often unstable in noisy signals. Hysteresis improves robustness by using different conditions for entering and exiting a detected event state.

```text
if signal falls below lower threshold  → event begins
if signal rises above upper threshold  → event ends
```

This reduces chatter when the signal fluctuates near a threshold.

## 8. Event rate and RPM-style estimation

If a periodic mechanism modulates the fluorescence signal, the time between detected dips can be converted into an event rate.

Example:

```text
rate = events / second
RPM  = 60 × events / second
```

This is useful when the optical signal is synchronized to rotating or repeating motion.

## 9. Practical limitations

This prototype should be understood as a **research and teaching platform**, with several important limitations:

- fluorescence intensity is not exclusively determined by magnetic field
- ambient light shielding strongly affects data quality
- gain and offset settings may require experiment-specific adjustment
- the ADC input range and TIA gain must be matched carefully
- the absence of microwave excitation limits direct comparison with ODMR systems

## 10. What students should learn from this project

A student using this repository should be able to understand:

- how a photodiode and TIA convert light into voltage
- why analog filtering matters before ADC conversion
- how digital filtering can serve different purposes
- why baseline removal helps event detection
- how a browser UI can assist experimental tuning

## 11. What researchers may reuse

Researchers may find this repository useful as a starting point for:

- low-cost optical intensity sensing
- early-stage NV experiment control stacks
- embedded streaming and remote tuning
- rapid validation of analog front-end concepts

## 12. Recommended next steps for more advanced sensing

To evolve beyond the present prototype, likely next additions include:

1. microwave excitation hardware
2. frequency sweep control
3. resonance tracking
4. calibration routines
5. temperature monitoring
6. synchronized experimental logging

Those additions can build on the acquisition, filtering, and networking structure documented here.
