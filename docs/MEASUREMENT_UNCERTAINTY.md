# TDC7201 GPSDO Module – Measurement Uncertainty Analysis

## Overview

This document provides a detailed mathematical analysis of all uncertainty
sources in the TDC7201 GPSDO Precision Timing Measurement Module.  The
analysis follows the GUM (Guide to the Expression of Uncertainty in
Measurement) framework.

---

## 1. Measurement Model

The measured quantity is the phase difference Δt between the 1 PPS input
signal and a derived edge from the 10 MHz reference:

```
Δt_measured = Δt_true + ε_comp_offset + ε_comp_jitter
              + ε_tdc_resolution + ε_delay_trim + ε_temperature
```

Where each ε term represents an independent uncertainty contribution.

The TDC7201 computes Δt using its Mode 1 formula:

```
Δt = (TIME1 / normLSB) - CLOCK_COUNT1 × T_clk

normLSB = (CAL2 - CAL1) / (cal_periods - 1) × T_clk

T_clk = 1 / f_ref  (e.g. 62.5 ns for a 16 MHz reference)
```

---

## 2. Uncertainty Sources

### 2.1 TLV3501 Input Offset Voltage (u₁)

The TLV3501 comparator has a specified input offset voltage V_OS = ±2 mV (typ),
±7 mV (max).  The timing uncertainty this creates depends on the signal slew
rate at the comparator input.

**Formula:**

```
u_comp_offset = V_OS / (dV/dt)
```

For a 1 PPS signal with a rise time of 10 ns over 3.3 V:

```
dV/dt = 3.3 V / 10 ns = 330 mV/ns
u_comp_offset = 2 mV / (330 mV/ns) ≈ 6 ps (typical)
```

For a degraded, slower signal at the worst case (1 V/µs slew rate):

```
u_comp_offset = 7 mV / (1 V/µs) = 7 ns (worst case, slow input)
```

**Adopted budget value:** 500 ps (moderate slew rate, ±2 mV)

**Distribution:** Rectangular (systematic offset), treated as Gaussian for RSS.

---

### 2.2 TLV3501 Propagation Delay Jitter (u₂)

The TLV3501 datasheet specifies propagation delay jitter (RMS) of approximately
50 ps under normal operating conditions.  This jitter is caused by internal
noise on the comparator output and shot noise in the input stage.

**Adopted budget value:** 50 ps RMS

**Distribution:** Gaussian

---

### 2.3 TDC7201 Resolution (u₃)

The TDC7201 has a nominal ring oscillator period of approximately 55 ps at
3.3 V supply and 25 °C.  Each LSB of the TIME register corresponds to one
ring oscillator period.

The resolution uncertainty is ±0.5 LSB:

```
u_tdc = 0.5 × T_ring = 0.5 × 55 ps = 27.5 ps
```

The device uses an internal calibration to correct for ring oscillator
frequency variation.  The residual error after calibration is typically
less than 1 LSB.

**Adopted budget value:** 55 ps (1 LSB, conservative)

**Distribution:** Uniform ±0.5 LSB → standard uncertainty = LSB / √12 ≈ 16 ps

---

### 2.4 Delay Line Trim Error (u₄)

The TL07401 delay line provides ~7 ns per tap.  After optimisation, the
residual trim error is ±0.5 tap = ±3.5 ns.  With careful calibration,
the absolute offset can be reduced to < 1 tap.

Software can fine-tune the tap to ±1 tap accuracy.  With ±1 tap = ±7 ns:

```
u_delay = 7 ns / 2 = 3.5 ns  (before calibration)
u_delay = 3.5 ns / 7 ≈ 0.5 ns  (1 standard deviation after calibration)
```

**Adopted budget value:** 500 ps (after tap optimisation)

**Distribution:** Rectangular

---

### 2.5 Temperature Coefficient (u₅)

The TDC7201 ring oscillator frequency varies with temperature.  The internal
calibration compensates for slow drift, but fast thermal transients introduce
error.

From the TDC7201 datasheet:
- Ring oscillator TC: approximately -1000 ppm/°C
- At 55 ps/LSB and 1 °C change: 55 ps × 1000 ppm = 55 ps error
- Over the 0–50 °C operating range without temperature compensation: ±2750 ps

With the internal calibration running every measurement cycle, residual drift
over a 1 °C measurement-to-measurement temperature change is:

```
u_temperature ≈ 55 ps × 1000 ppm × 1°C = 55 ps/°C × 1°C = 55 ps
```

**Adopted budget value:** 100 ps (2 °C variation between measurements)

**Distribution:** Gaussian

---

## 3. Combined Standard Uncertainty

All sources are assumed independent (uncorrelated).  The combined standard
uncertainty is computed using root-sum-of-squares (RSS):

```
u_total = sqrt(u₁² + u₂² + u₃² + u₄² + u₅²)

u_total = sqrt(500² + 50² + 55² + 500² + 100²) ps

u_total = sqrt(250000 + 2500 + 3025 + 250000 + 10000) ps

u_total = sqrt(515525) ps

u_total ≈ 718 ps
```

---

## 4. Expanded Uncertainty

For 95% confidence level (coverage factor k = 2, assuming Gaussian distribution):

```
U_95% = k × u_total = 2 × 718 ps ≈ 1436 ps ≈ 1.44 ns
```

---

## 5. Uncertainty Budget Table

| Source                    | Symbol | Value (ps) | u² (ps²)  | Contribution |
|---------------------------|--------|-----------|-----------|--------------|
| TLV3501 offset voltage    | u₁     | 500       | 250,000   | 48.5 %       |
| TLV3501 propagation jitter| u₂     | 50        | 2,500     | 0.5 %        |
| TDC7201 resolution        | u₃     | 55        | 3,025     | 0.6 %        |
| Delay line trim error     | u₄     | 500       | 250,000   | 48.5 %       |
| Temperature drift         | u₅     | 100       | 10,000    | 1.9 %        |
| **Combined (RSS)**        | **u**  | **718**   | **515,525**| **100 %**   |
| **Expanded (k=2, 95%)**   | **U**  | **1436**  | —         | —            |

---

## 6. Sensitivity Coefficients

The dominant uncertainty sources (TLV3501 offset and delay line trim) each
contribute ~48.5% of the total variance.  Reducing either by 50% would reduce
total uncertainty to approximately 507 ps.

```
∂u_total/∂u₁ = u₁ / u_total = 500 / 718 ≈ 0.70
∂u_total/∂u₄ = u₄ / u_total = 500 / 718 ≈ 0.70
```

**Recommendations to reduce uncertainty:**
1. Use a faster-slewing signal driver at J1/J2 (increases dV/dt → reduces u₁)
2. Implement multi-point delay calibration (reduces u₄ to < 100 ps)
3. Add a temperature sensor and correction table (reduces u₅ to < 10 ps)
4. Average N measurements: u_total_avg = u_total / √N
   - N=100 measurements: 718 ps / 10 = 72 ps

---

## 7. Allan Deviation and Long-Term Stability

For a free-running oscillator, Allan deviation characterises frequency stability:

```
σ_y(τ) = u_total / (f_ref × τ)

At τ = 1 s:   σ_y(1 s) = 718e-12 / (10e6 × 1) = 7.18 × 10⁻¹⁷
At τ = 100 s: σ_y(100 s) = 7.18 × 10⁻¹⁹
```

These values represent the measurement noise floor; the actual GPSDO oscillator
drift will dominate at longer averaging times.

---

## 8. Frequency Offset Uncertainty

From the phase difference uncertainty u_Δt and the reference frequency f_ref:

```
u_f/f = u_Δt / τ_ref

For τ_ref = 100 ms (10 Hz measurement rate):
u_f/f = 718e-12 / 0.1 = 7.18 × 10⁻⁹ = 7.18 ppb
```

At 1 Hz measurement rate:
```
u_f/f = 718e-12 / 1.0 = 718 × 10⁻¹² = 0.718 ppb
```

---

*References:*
- *BIPM/ISO/OIML/IUPAC GUM: Evaluation of measurement data (2008)*
- *TDC7201 Datasheet, SNAS602, Texas Instruments*
- *TLV3501 Datasheet, SLOS493, Texas Instruments*
- *Kester, W. (2009). "Which ADC Architecture Is Right for Your Application?" Analog Dialogue 39-06*

*Last updated: 2026-04-08*
