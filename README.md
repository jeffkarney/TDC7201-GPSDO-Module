# TDC7201 GPSDO Precision Timing Measurement Module

A standalone **precision timing measurement module** built around the Texas Instruments
**TDC7201** time-to-digital converter.  It measures the phase difference between a
1 PPS (Pulse Per Second) GPS signal and a 10 MHz reference oscillator with sub-nanosecond
resolution, and exposes the results via a REST API served by an **ESP32-C3** WiFi
microcontroller.

> **This is a precision timing measurement module**, not a full GPSDO.  It can be
> integrated into a GPSDO, frequency counter, or any system requiring high-accuracy
> phase comparison.

---

## Features

- **TDC7201** time-to-digital converter – 55 ps resolution per LSB
- **Dual TLV3501** high-speed comparators (2.5 ns propagation delay) for both input channels
- **TL07401** programmable delay line for 12 ns START offset correction
- **ESP32-C3-WROOM-02U-N4** WiFi microcontroller with REST API
- **USB-C** power input (5 V) with USBLC6-2SC6 ESD protection
- **3× SMA connectors** for PPS input, 10 MHz input, and WiFi antenna
- **4-layer PCB** optimised for JLCPCB manufacture (JLC04161H-7628 stack-up)
- **Vue 3 web dashboard** with real-time measurements and uncertainty visualisation
- **Complete REST API** for measurement queries, configuration, and calibration

---

## Specifications

| Parameter                  | Value                           |
|----------------------------|---------------------------------|
| TDC resolution             | 55 ps / LSB                     |
| Combined measurement uncertainty | ±718 ps RMS (95%: ±1.44 ns) |
| Reference input frequency  | 10 MHz (SMA)                    |
| Pulse input                | 1 PPS or any TTL/3.3V edge      |
| Supply voltage             | 5 V USB-C                       |
| Current consumption        | < 200 mA typical                |
| WiFi                       | 802.11 b/g/n 2.4 GHz            |
| Operating temperature      | 0 °C – 50 °C                    |
| PCB dimensions             | 80 mm × 50 mm                   |
| PCB layers                 | 4 (signal / GND / power / signal) |

---

## Repository Structure

```
TDC7201-GPSDO-Module/
├── README.md
├── hardware/
│   ├── BOM.csv                          # Complete BOM with JLCPCB part numbers
│   ├── DESIGN_NOTES.md                  # Design decisions and rationale
│   └── kicad/
│       ├── TDC7201-Module.kicad_sch     # KiCad schematic
│       ├── TDC7201-Module.kicad_pcb     # KiCad PCB layout (4-layer)
│       └── sym-lib-table                # Symbol library table
├── firmware/
│   ├── main/
│   │   ├── main.c                       # Application entry point (ESP-IDF)
│   │   ├── tdc7201_driver.c/h           # TDC7201 SPI/I2C driver
│   │   ├── measurement.c/h              # Phase difference and statistics
│   │   ├── wifi_config.c/h              # WiFi station + HTTP REST server
│   ├── CMakeLists.txt                   # ESP-IDF build configuration
│   └── idf_component.yml               # Component manifest
├── web/
│   ├── src/
│   │   ├── App.vue                      # Main dashboard (Vue 3)
│   │   ├── main.js                      # Application entry point
│   │   ├── components/
│   │   │   ├── MeasurementDisplay.vue   # Real-time measurement display
│   │   │   ├── UncertaintyAnalysis.vue  # Uncertainty breakdown chart
│   │   │   └── ConfigPanel.vue          # Configuration controls
│   │   └── api/
│   │       └── measurements.js          # REST API client
│   ├── public/index.html
│   ├── package.json
│   └── vite.config.js
└── docs/
    ├── API_DOCUMENTATION.md
    ├── CALIBRATION_GUIDE.md
    └── MEASUREMENT_UNCERTAINTY.md
```

---

## Quick Start

### Hardware Setup

1. Connect a **1 PPS** signal (3.3 V TTL, e.g. from a GPS module) to **J1** (SMA)
2. Connect a **10 MHz reference** signal (3.3 V, ≥ 0 dBm) to **J2** (SMA)
3. Connect a **WiFi antenna** to **J3** (SMA) or **J5** (IPX)
4. Apply **5 V USB-C** power via **J4**

### Firmware Build & Flash

Requires [ESP-IDF v5.x](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/).

```bash
# Clone and enter firmware directory
cd firmware

# Configure WiFi credentials
idf.py menuconfig  # Set CONFIG_WIFI_SSID and CONFIG_WIFI_PASSWORD

# Build and flash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Web Dashboard

```bash
cd web
npm install
npm run dev       # Development server at http://localhost:3000
npm run build     # Production build in web/dist/
```

Set `VITE_DEV_TARGET=http://<ESP32-IP>` to proxy API calls during development.

---

## REST API Quick Reference

| Method | Endpoint          | Description                     |
|--------|-------------------|---------------------------------|
| GET    | /api/measurements | Current phase difference & uncertainty |
| GET    | /api/stats        | Statistical summary (mean, stddev) |
| GET    | /api/config       | Current configuration           |
| POST   | /api/config       | Apply new configuration         |
| POST   | /api/calibrate    | Reset measurement history       |

See [docs/API_DOCUMENTATION.md](docs/API_DOCUMENTATION.md) for full details.

---

## Integration

This module can be integrated into a GPSDO or other system via:

- **REST API** – query phase difference over WiFi
- **SPI/I2C direct** – bypass the ESP32-C3 and read TDC7201 directly
- **USB-C serial** – debug console at 115200 baud
- **Phase output** – use Δt to drive a PLL/frequency-locking algorithm

---

## Documentation

| Document | Description |
|----------|-------------|
| [hardware/DESIGN_NOTES.md](hardware/DESIGN_NOTES.md) | Design decisions, layout guidelines, calibration |
| [docs/API_DOCUMENTATION.md](docs/API_DOCUMENTATION.md) | Complete REST API reference |
| [docs/CALIBRATION_GUIDE.md](docs/CALIBRATION_GUIDE.md) | Step-by-step calibration procedures |
| [docs/MEASUREMENT_UNCERTAINTY.md](docs/MEASUREMENT_UNCERTAINTY.md) | GUM-compliant uncertainty analysis |

---

## License

MIT License – see [LICENSE](LICENSE) for details.

*Author: jeffkarney*
