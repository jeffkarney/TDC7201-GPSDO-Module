# TDC7201 GPSDO Module – REST API Documentation

Base URL: `http://<device-ip>/api`

All responses are JSON (`Content-Type: application/json`).
CORS header `Access-Control-Allow-Origin: *` is included on all responses.

---

## GET /api/measurements

Returns the most recent phase-difference measurement and full uncertainty breakdown.

**Response:**

```json
{
  "phase_difference_ps": -123.45,
  "freq_offset_ppb": -1.234,
  "measurement_count": 42,
  "uncertainty_ps": 718.2,
  "uncertainty_breakdown": {
    "comparator_offset_ps": 500.0,
    "comparator_jitter_ps": 50.0,
    "tdc_resolution_ps": 55.0,
    "delay_line_error_ps": 500.0,
    "temperature_drift_ps": 100.0
  }
}
```

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `phase_difference_ps` | float | ps | Measured phase difference (positive = PPS leads reference) |
| `freq_offset_ppb` | float | ppb | Derived frequency offset |
| `measurement_count` | int | — | Total measurements since boot / last calibration reset |
| `uncertainty_ps` | float | ps | Total RMS uncertainty (RSS of components) |
| `uncertainty_breakdown` | object | ps | Individual uncertainty contributions |

**Example:**

```bash
curl http://192.168.1.42/api/measurements
```

---

## GET /api/stats

Returns statistical summary over the history buffer (up to 256 samples).

**Response:**

```json
{
  "mean_ps": -120.5,
  "std_dev_ps": 45.2,
  "min_ps": -250.1,
  "max_ps": 10.3,
  "moving_avg_ps": -122.0,
  "count": 256
}
```

| Field | Type | Unit | Description |
|-------|------|------|-------------|
| `mean_ps` | float | ps | Arithmetic mean of history buffer |
| `std_dev_ps` | float | ps | Population standard deviation |
| `min_ps` | float | ps | Minimum observed value |
| `max_ps` | float | ps | Maximum observed value |
| `moving_avg_ps` | float | ps | Exponential moving average (α = 0.1) |
| `count` | int | — | Total valid measurements |

**Example:**

```bash
curl http://192.168.1.42/api/stats
```

---

## GET /api/config

Returns current device configuration.

**Response:**

```json
{
  "delay_line_tap": 2,
  "ema_alpha": 0.1,
  "ref_freq_hz": 16000000.0
}
```

**Example:**

```bash
curl http://192.168.1.42/api/config
```

---

## POST /api/config

Applies new configuration settings.  All fields are optional.

**Request body:**

```json
{
  "delay_line_tap": 3,
  "measurement_mode": "phase"
}
```

| Field | Type | Range | Description |
|-------|------|-------|-------------|
| `delay_line_tap` | int | 0–14 | Delay line tap (each tap ≈ 7 ns) |
| `measurement_mode` | string | `"phase"`, `"frequency"`, `"time-interval"` | Display mode (informational) |

**Response:**

```json
{ "status": "ok" }
```

**Example:**

```bash
curl -X POST http://192.168.1.42/api/config \
     -H "Content-Type: application/json" \
     -d '{"delay_line_tap": 2}'
```

---

## POST /api/calibrate

Resets the measurement history buffer and running statistics.  Use after
adjusting the delay line tap to get a clean baseline.

**Request body:** empty `{}` or no body.

**Response:**

```json
{ "status": "calibration_reset" }
```

**Example:**

```bash
curl -X POST http://192.168.1.42/api/calibrate
```

---

## Error Responses

| HTTP Status | Meaning |
|-------------|---------|
| 400 | Bad Request – invalid JSON body |
| 404 | Endpoint not found |
| 500 | Internal error (no measurement context) |

Error response format:

```json
{ "error": "description of error" }
```

---

## Rate Limiting

The HTTP server is single-threaded.  Recommended polling interval: **100 ms** (10 Hz).
Faster polling may cause request queuing on the ESP32-C3.

---

*Last updated: 2026-04-08*

