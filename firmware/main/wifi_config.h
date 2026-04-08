/**
 * @file wifi_config.h
 * @brief WiFi connection management and REST API server for TDC7201 GPSDO Module
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdint.h>
#include "esp_err.h"
#include "measurement.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Configuration constants (override via menuconfig / sdkconfig)
 * ------------------------------------------------------------------------- */

#ifndef WIFI_SSID
/** Default WiFi SSID – override in sdkconfig or pass at build time. */
#define WIFI_SSID        "your-ssid"
#endif

#ifndef WIFI_PASSWORD
/** Default WiFi password – override in sdkconfig or pass at build time. */
#define WIFI_PASSWORD    "your-password"
#endif

/** Maximum WiFi reconnection attempts before giving up. */
#define WIFI_MAX_RETRIES  10

/** HTTP server port. */
#define HTTP_SERVER_PORT  80

/* -------------------------------------------------------------------------
 * WiFi status
 * ------------------------------------------------------------------------- */
typedef enum {
    WIFI_STATUS_DISCONNECTED = 0,
    WIFI_STATUS_CONNECTING,
    WIFI_STATUS_CONNECTED,
    WIFI_STATUS_FAILED,
} wifi_status_t;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialise WiFi in station mode and connect.
 *
 * Blocks until connected or max retries are exceeded.
 *
 * @param ssid     Network SSID.
 * @param password Network password.
 * @return ESP_OK if connected, ESP_FAIL otherwise.
 */
esp_err_t wifi_init_sta(const char *ssid, const char *password);

/**
 * @brief Start the HTTP REST API server.
 *
 * Registers all API endpoints:
 *   GET  /api/measurements
 *   POST /api/config
 *   GET  /api/config
 *   POST /api/calibrate
 *   GET  /api/stats
 *
 * @param meas_ctx Pointer to the shared measurement context.
 * @return ESP_OK on success.
 */
esp_err_t http_server_start(measurement_ctx_t *meas_ctx);

/**
 * @brief Stop and free the HTTP server.
 *
 * @return ESP_OK on success.
 */
esp_err_t http_server_stop(void);

/**
 * @brief Get the current WiFi connection status.
 */
wifi_status_t wifi_get_status(void);

/**
 * @brief Get the device IP address as a string (e.g., "192.168.1.42").
 *
 * @param[out] buf  Buffer to write into.
 * @param buf_len   Buffer length (at least 16 bytes).
 */
void wifi_get_ip_str(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif

#endif /* WIFI_CONFIG_H */
