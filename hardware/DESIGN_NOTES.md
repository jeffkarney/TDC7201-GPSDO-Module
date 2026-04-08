# TDC7201 GPSDO Module – Design Notes

## Overview

This document describes the design decisions and technical rationale for the
TDC7201 Precision Timing Measurement Module.  The module measures the phase
difference between a 1 PPS GPS signal and a 10 MHz GPSDO reference oscillator
using a Texas Instruments TDC7201 time-to-digital converter (TDC).

---

## 1. System Architecture

```
1 PPS Input (SMA J1)  ──► TLV3501 U4 (comparator) ──► START  ┐
                                                               ├──► TDC7201 U3 ──► SPI ──► ESP32-C3 U7
10 MHz Ref (SMA J2)   ──► TLV3501 U5 (comparator) ──► STOP   ┘
                                                                         │
Programmable delay (TL07401 U6) on START path to cancel 12 ns offset     │
                                                               REST API over WiFi
```

The ESP32-C3 reads TDC results via SPI at up to 10 MHz, computes the
calibrated time-of-flight, and serves the results via a JSON REST API.

---

## 2. TLV3501 Comparator Selection

### Why TLV3501?

| Parameter              | TLV3501     | LT1711      | MAX9601     |
|------------------------|-------------|-------------|-------------|
| Propagation delay      | 2.5 ns typ  | 4.5 ns      | 2.5 ns      |
| Input offset voltage   | ±2 mV typ   | ±3 mV       | ±4 mV       |
| Supply range           | 2.7–5.5 V   | 5 V only    | 3–5.5 V     |
| Package (JLCPCB stock) | TSSOP-8 ✓   | SOT-23 ✓    | SC70        |
| PSRR                   | 80 dB       | 70 dB       | 75 dB       |

The TLV3501 was selected for its combination of ultra-low propagation delay
(2.5 ns) and low input offset voltage (±2 mV), both critical for sub-nanosecond
timing repeatability.  It is available in TSSOP-8 from JLCPCB basic parts.

### Comparator Timing Contribution

- Input offset voltage: ±2 mV → ±0.5 ns at typical input slew rate
- Propagation delay variation over temperature: ±0.3 ns
- Channel-to-channel matching: ±0.2 ns

---

## 3. 12 ns START Delay Correction

The TDC7201 has an inherent ~12 ns delay from START pin assertion to the
internal ring oscillator, due to internal latching logic.

**Solution:** A TL07401 programmable delay line is inserted in the START signal
path.  Each tap adds ~7 ns of delay.

```
1 PPS ─► TLV3501 ─► TL07401 (tap N) ─► TDC7201 START
          delay compensation: N × 7 ns
```

- At tap 0: raw propagation, ~2 ns delay
- At tap 2: ~14 ns delay → effectively cancels the 12 ns offset
- Tap range: 0–14 (controlled by ESP32-C3 GPIO shift register)

The tap is user-configurable via the `/api/config` REST endpoint and stored
in NVS for power-cycle persistence.

---

## 4. Input Signal Conditioning

### 4.1 Single-Ended Input (default)

```
SMA ─► 50Ω (R_term) ─► 100 nF (AC coupling) ─► TLV3501 IN+
                        Hysteresis set by R_ref divider
```

- 50 Ω termination at SMA connector to match cable impedance
- AC coupling removes DC offset from the GPS receiver output
- Voltage divider sets threshold at ~1.65 V (VCC/2) for 3.3 V logic signals

### 4.2 Differential Input (optional, via solder jumper)

An optional passive transformer/balun allows differential input for
improved common-mode noise rejection, useful in electrically noisy
environments.  Selectable via a PCB solder jumper.

---

## 5. Power Supply Architecture

```
USB-C J4 (5 V) ─► USBLC6-2SC6 D1 (ESD) ─► 5 V rail
                                              │
                                         AMS1117-3.3 U2
                                              │
                                           3.3 V rail (digital + analog)
                                              │
                         Ferrite bead (L1,L2) separates analog/digital
```

- **5 V rail:** Feeds TLV3501 comparators (5 V single-supply, lower jitter)
- **3.3 V rail:** Feeds TDC7201, ESP32-C3, and digital logic
- **Decoupling:** 100 nF ceramic + 10 µF bulk at each power pin
- **ESD:** USBLC6-2SC6 on USB-C power and data lines

---

## 6. 4-Layer PCB Stack-up

```
Layer 1 (Signal, Top):   Component side, short high-speed traces
Layer 2 (GND):           Solid copper ground plane – unbroken under analog circuits
Layer 3 (Power):         3.3 V flood (digital) / 5 V island (analog comparators)
Layer 4 (Signal, Bottom): Non-critical routing, test point connections
```

### Signal Integrity Guidelines

1. START and STOP traces: ≤ 50 mm total, 50 Ω controlled impedance (≈ 100 µm on JLC04161H-7628)
2. Via stitching: ground vias every 5 mm around TDC7201 and comparator circuits
3. Return current continuity: no splits in GND plane beneath TDC7201
4. Clock trace (16 MHz TCXO → TDC7201 CLK): matched length, guard ring

---

## 7. Measurement Uncertainty Budget

| Source                     | Uncertainty  | Notes                                  |
|----------------------------|--------------|----------------------------------------|
| TLV3501 input offset       | ±500 ps      | ±2 mV @ slew 1 V/ns = 2 ps, RSS budget |
| TLV3501 propagation jitter | ±50 ps RMS   | Datasheet jitter specification         |
| TDC7201 resolution (1 LSB) | ±55 ps       | 55 ps nominal ring oscillator period   |
| Delay-line trim error      | ±500 ps      | ±0.5 tap × 7 ns (calibrated to ±0.5) |
| Temperature drift          | ±100 ps      | 100 ps/°C × 1 °C ambient change        |
| **Total (RSS)**            | **±723 ps**  | Root-sum-of-squares                    |

*Note: The dominant contributors are TLV3501 offset and delay-line trim.
       Both can be reduced by running the automated tap-optimization algorithm.*

### Uncertainty Formula

```
u_total = sqrt(u_offset² + u_jitter² + u_tdc² + u_delay² + u_temp²)
        = sqrt(500² + 50² + 55² + 500² + 100²)
        ≈ 723 ps RMS
```

---

## 8. Component Specifications Summary

### TDC7201 (U3)
- Resolution: 55 ps (ring oscillator period, 16 MHz reference)
- Modes: Mode 1 (single STOP), Mode 2 (up to 5 STOPs)
- Interface: SPI up to 10 MHz, I2C up to 400 kHz
- Supply: 3.0–3.6 V

### TLV3501 (U4, U5)
- Propagation delay: 2.5 ns typ
- Input offset voltage: ±2 mV typ, ±7 mV max
- Supply: 2.7–5.5 V (using 5 V for best noise margin)
- Package: TSSOP-8

### ESP32-C3-WROOM-02U-N4 (U7)
- CPU: RISC-V 160 MHz single-core
- Flash: 4 MB
- RAM: 400 KB
- WiFi: 802.11 b/g/n 2.4 GHz
- Interfaces: SPI (10 MHz to TDC7201), GPIO for ENABLE/TRIG/INT

### AMS1117-3.3 (U2)
- Input: 4.75–15 V
- Output: 3.3 V @ up to 1 A
- Dropout: 1.2 V typ

---

## 9. Testing Procedures

1. **Power supply verification:**
   - Apply 5 V USB-C; verify 3.3 V at TP3, 5 V at TP1
   - Check idle current < 200 mA

2. **SPI communication test:**
   - Read TDC7201 CONFIG1 register (addr 0x00) via JTAG/logic analyser
   - Expected: 0x00 (default after reset)

3. **Comparator response test:**
   - Apply 1 kHz square wave to J1 (PPS input)
   - Observe TDC7201 START signal at TP5 with oscilloscope

4. **WiFi connection test:**
   - Flash firmware, open serial monitor at 115200 baud
   - Verify "Connected! IP: 192.168.x.x" message

5. **API functionality test:**
   - `curl http://<IP>/api/measurements` → JSON response
   - `curl http://<IP>/api/stats` → statistics object

6. **Timing accuracy test:**
   - Connect precision signal generator to J1 and J2
   - Set J1 = 1 PPS, J2 = 10 MHz; verify phase result < ±10 ns

---

## 10. Calibration Steps

1. Set delay tap to 0 via `POST /api/config {"delay_line_tap": 0}`
2. Apply coincident 1 PPS and 10 MHz reference (phase = 0)
3. Collect 100 measurements; note mean phase offset
4. Calculate required tap: `tap = round(offset_ns / 7)`
5. Apply tap: `POST /api/config {"delay_line_tap": <N>}`
6. Repeat until |mean offset| < 1 ns
7. Trigger calibration reset: `POST /api/calibrate`

---

## 11. Future Improvements

- Add on-board temperature sensor (e.g., TMP117) for drift compensation
- Replace TL07401 delay line with DG419 analog switch array for finer trim
- Add OCXO/TCXO reference clock option (10 MHz SMA input)
- Implement NTP server on ESP32-C3 using measured phase difference
- Add second TDC7201 channel for redundant measurement and voting

---

*Last updated: 2026-04-08*
*Author: jeffkarney*
