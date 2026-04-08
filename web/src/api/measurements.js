/**
 * measurements.js – API client for the TDC7201 GPSDO Module REST API
 *
 * Provides wrapper functions for all endpoints with error handling,
 * exponential-backoff reconnection, and optional polling.
 */

import axios from 'axios'

/** Base URL resolved relative to the serving ESP32-C3.
 *  Set VITE_API_BASE_URL in .env to override during development. */
const BASE_URL = import.meta.env.VITE_API_BASE_URL || ''

/** Axios instance with shared base URL and default timeout. */
const api = axios.create({
  baseURL: BASE_URL,
  timeout: 5000,
  headers: { 'Content-Type': 'application/json' },
})

// ---------------------------------------------------------------------------
// API call wrappers
// ---------------------------------------------------------------------------

/**
 * GET /api/measurements
 * Returns current phase difference, frequency offset, and uncertainty breakdown.
 *
 * @returns {Promise<Object>} Measurements object.
 */
export async function fetchMeasurements() {
  const response = await api.get('/api/measurements')
  return response.data
}

/**
 * GET /api/stats
 * Returns statistical summary (mean, std dev, min, max, moving average).
 *
 * @returns {Promise<Object>} Statistics object.
 */
export async function fetchStats() {
  const response = await api.get('/api/stats')
  return response.data
}

/**
 * GET /api/config
 * Returns current device configuration.
 *
 * @returns {Promise<Object>} Configuration object.
 */
export async function fetchConfig() {
  const response = await api.get('/api/config')
  return response.data
}

/**
 * POST /api/config
 * Applies new configuration settings.
 *
 * @param {Object} config - Configuration to apply.
 * @param {number} [config.delay_line_tap] - Delay line tap (0–14).
 * @param {string} [config.measurement_mode] - Measurement mode string.
 * @returns {Promise<Object>} API response.
 */
export async function postConfig(config) {
  const response = await api.post('/api/config', config)
  return response.data
}

/**
 * POST /api/calibrate
 * Triggers a calibration reset.
 *
 * @returns {Promise<Object>} API response.
 */
export async function postCalibrate() {
  const response = await api.post('/api/calibrate', {})
  return response.data
}

// ---------------------------------------------------------------------------
// Polling helper
// ---------------------------------------------------------------------------

/**
 * Start polling /api/measurements at a fixed interval.
 *
 * @param {Function} onData    - Callback invoked with measurement data.
 * @param {Function} onError   - Callback invoked with Error on failure.
 * @param {number}   intervalMs - Poll interval in milliseconds (default 100).
 * @returns {{ stop: Function }} Object with a `stop()` method to halt polling.
 */
export function startPolling(onData, onError, intervalMs = 100) {
  let retryDelay = intervalMs
  let timerId = null
  let stopped = false

  async function poll() {
    if (stopped) return
    try {
      const data = await fetchMeasurements()
      retryDelay = intervalMs  // reset backoff on success
      onData(data)
    } catch (err) {
      if (onError) onError(err)
      // Exponential backoff capped at 5 s
      retryDelay = Math.min(retryDelay * 2, 5000)
    }
    if (!stopped) {
      timerId = setTimeout(poll, retryDelay)
    }
  }

  timerId = setTimeout(poll, 0)

  return {
    stop() {
      stopped = true
      if (timerId !== null) {
        clearTimeout(timerId)
        timerId = null
      }
    },
  }
}
