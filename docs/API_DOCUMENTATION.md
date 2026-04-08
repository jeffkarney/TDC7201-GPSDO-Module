# API Documentation

## Endpoint: GET /api/measurements
- **Description:** Retrieves the latest measurement data.
- **Response Schema:**
  ```json
  {
      "phase_difference_ps": number,
      "measurement_count": number,
      "uncertainty_ps": number,
      "uncertainty_breakdown": {
          "comparator_offset_ps": number,
          "comparator_jitter_ps": number,
          "tdc_resolution_ps": number,
          "delay_line_error_ps": number,
          "temperature_drift_ps": number
      }
  }
  ```
- **Example Curl Request:**
  ```bash
  curl -X GET http://<your-domain>/api/measurements
  ```

## Endpoint: POST /api/config
- **Description:** Applies configuration settings to the device.
- **Request Body:**
  ```json
  {
      "delay_line_tap": 0-14,
      "measurement_mode": "string"
  }
  ```
- **Example Curl Request:**
  ```bash
  curl -X POST http://<your-domain>/api/config -H "Content-Type: application/json" -d '{"delay_line_tap": 0, "measurement_mode": "normal"}'
  ```

## Endpoint: GET /api/status
- **Description:** Retrieves device health and WiFi connection info.
- **Response Schema:**
  The response will vary based on device state. Ensure to parse accordingly.
- **Example Curl Request:**
  ```bash
  curl -X GET http://<your-domain>/api/status
  ```

## Endpoint: GET /api/calibration
- **Description:** Retrieves current calibration parameters.
- **Response Schema:**
  The response will vary based on current calibration state.
- **Example Curl Request:**
  ```bash
  curl -X GET http://<your-domain>/api/calibration
  ```

## Endpoint: POST /api/calibration
- **Description:** Adjusts calibration parameters for delay line trimming.
- **Request Body:**
  ```json
  {
      "offset_adjustment": number
  }
  ```
- **Example Curl Request:**
  ```bash
  curl -X POST http://<your-domain>/api/calibration -H "Content-Type: application/json" -d '{"offset_adjustment": 10}'
  ```

## Error Codes
- **400 Bad Request:** Invalid input.
- **401 Unauthorized:** Authentication required.
- **404 Not Found:** Endpoint not found.
- **500 Internal Server Error:** Unexpected error.

## Rate Limiting
- **Requests:** Up to 1000 requests per hour per IP Address.

---

**Document created by:** jeffkarney - 2026-04-08 16:47:30 UTC
