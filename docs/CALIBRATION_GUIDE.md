# Calibration Guide

## 1. Hardware Setup
### 1.1. 1PPS & 10MHz Reference Signal Generators
 - Connect the 1PPS output from the GPSDO to your timing device.
 - Connect the 10MHz output to your measurement device.
 - Ensure signal integrity through proper cabling and termination.

## 2. Firmware Programming Guide for ESP32-C3
1. Install the necessary toolchain for ESP32-C3.
2. Clone the firmware repository.
3. Build the firmware using the command:
   ```bash
   make build
   ```
4. Upload the firmware to the ESP32-C3:
   ```bash
   make flash
   ```

## 3. Delay Line Tap Optimization Algorithm
1. Start at tap 7.
2. Measure 100 samples at this tap.
3. Calculate mean offset.
4. Adjust tap by ±1 and repeat the measurement until the offset is minimized.

## 4. Uncertainty Validation
 - Compare measurements against a precision oscilloscope to validate uncertainty in measurements.
 - Document the findings in this section.

## 5. Temperature Stability Testing
 - Test the module over a temperature range of 0-50°C.
 - Document the performance results at each temperature increment.

## 6. Long-term Drift Characterization
 - Monitor the output over an extended period to characterize long-term drift.
 - Document findings with appropriate graphs and statistics.

## 7. Documented Baseline Configuration Template
 - Include a template for recording the baseline configuration of the setup for consistency in calibrations.

---

## Conclusion
This guide aims to assist in performing a comprehensive calibration of the TDC7201 GPSDO Module. Follow each procedural section carefully and document all findings for future reference.