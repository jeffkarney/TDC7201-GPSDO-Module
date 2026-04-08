/**
 * @file wifi_config.c
 * @brief WiFi connection management and REST API server implementation
 */

#include "wifi_config.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "cJSON.h"

static const char *TAG = "wifi_config";

/* -------------------------------------------------------------------------
 * WiFi internal state
 * ------------------------------------------------------------------------- */

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT   BIT0
#define WIFI_FAIL_BIT        BIT1

static volatile wifi_status_t s_wifi_status = WIFI_STATUS_DISCONNECTED;
static char                   s_ip_str[16]  = "0.0.0.0";
static int                    s_retry_num   = 0;

/* -------------------------------------------------------------------------
 * Shared measurement context (set by http_server_start)
 * ------------------------------------------------------------------------- */
static measurement_ctx_t *s_meas_ctx = NULL;

/* Shared HTTP server handle */
static httpd_handle_t s_server = NULL;

/* -------------------------------------------------------------------------
 * WiFi event handler
 * ------------------------------------------------------------------------- */

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        s_wifi_status = WIFI_STATUS_CONNECTING;
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAX_RETRIES) {
            esp_wifi_connect();
            s_retry_num++;
            s_wifi_status = WIFI_STATUS_CONNECTING;
            ESP_LOGW(TAG, "Retrying WiFi connection (%d/%d)...",
                     s_retry_num, WIFI_MAX_RETRIES);
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            s_wifi_status = WIFI_STATUS_FAILED;
            ESP_LOGE(TAG, "WiFi connection failed after %d retries", WIFI_MAX_RETRIES);
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num   = 0;
        s_wifi_status = WIFI_STATUS_CONNECTED;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "WiFi connected. IP: %s", s_ip_str);
    }
}

/* -------------------------------------------------------------------------
 * WiFi init
 * ------------------------------------------------------------------------- */

esp_err_t wifi_init_sta(const char *ssid, const char *password)
{
    s_wifi_event_group = xEventGroupCreate();
    if (!s_wifi_event_group) { return ESP_ERR_NO_MEM; }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any, inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &inst_any));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &inst_got_ip));

    wifi_config_t wifi_cfg = { 0 };
    strncpy((char *)wifi_cfg.sta.ssid,     ssid,     sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, password, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Connecting to '%s'...", ssid);

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE,
                                           portMAX_DELAY);

    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, inst_any);
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, inst_got_ip);
    vEventGroupDelete(s_wifi_event_group);

    if (bits & WIFI_CONNECTED_BIT) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

/* -------------------------------------------------------------------------
 * JSON response helpers
 * ------------------------------------------------------------------------- */

static void send_json_response(httpd_req_t *req, cJSON *root)
{
    char *body = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_send(req, body, (ssize_t)strlen(body));
    free(body);
}

static void send_error(httpd_req_t *req, int status, const char *msg)
{
    httpd_resp_set_status(req, status == 400 ? "400 Bad Request" : "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");
    char buf[128];
    snprintf(buf, sizeof(buf), "{\"error\":\"%s\"}", msg);
    httpd_resp_send(req, buf, (ssize_t)strlen(buf));
}

/* -------------------------------------------------------------------------
 * GET /api/measurements
 * ------------------------------------------------------------------------- */

static esp_err_t handler_get_measurements(httpd_req_t *req)
{
    if (!s_meas_ctx) { send_error(req, 500, "no measurement context"); return ESP_OK; }

    measurement_update_stats(s_meas_ctx);

    cJSON *root = cJSON_CreateObject();

    /* Latest measurement */
    uint32_t latest_idx = s_meas_ctx->fill == 0 ? 0
        : (s_meas_ctx->head + MEASUREMENT_HISTORY_SIZE - 1) % MEASUREMENT_HISTORY_SIZE;
    const measurement_result_t *latest = &s_meas_ctx->history[latest_idx];

    cJSON_AddNumberToObject(root, "phase_difference_ps", latest->phase_diff_ps);
    cJSON_AddNumberToObject(root, "freq_offset_ppb",     latest->freq_offset_ppb);
    cJSON_AddNumberToObject(root, "measurement_count",   s_meas_ctx->stats.count);

    /* Uncertainty */
    const measurement_uncertainty_t *u = &s_meas_ctx->uncertainty;
    cJSON_AddNumberToObject(root, "uncertainty_ps", u->total_rms_ps);

    cJSON *ub = cJSON_CreateObject();
    cJSON_AddNumberToObject(ub, "comparator_offset_ps",  u->comparator_offset_ps);
    cJSON_AddNumberToObject(ub, "comparator_jitter_ps",  u->comparator_jitter_ps);
    cJSON_AddNumberToObject(ub, "tdc_resolution_ps",     u->tdc_resolution_ps);
    cJSON_AddNumberToObject(ub, "delay_line_error_ps",   u->delay_line_error_ps);
    cJSON_AddNumberToObject(ub, "temperature_drift_ps",  u->temperature_drift_ps);
    cJSON_AddItemToObject(root, "uncertainty_breakdown", ub);

    send_json_response(req, root);
    cJSON_Delete(root);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * GET /api/stats
 * ------------------------------------------------------------------------- */

static esp_err_t handler_get_stats(httpd_req_t *req)
{
    if (!s_meas_ctx) { send_error(req, 500, "no measurement context"); return ESP_OK; }

    measurement_update_stats(s_meas_ctx);
    const measurement_stats_t *s = &s_meas_ctx->stats;

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "mean_ps",       s->mean_ps);
    cJSON_AddNumberToObject(root, "std_dev_ps",    s->std_dev_ps);
    cJSON_AddNumberToObject(root, "min_ps",        s->min_ps);
    cJSON_AddNumberToObject(root, "max_ps",        s->max_ps);
    cJSON_AddNumberToObject(root, "moving_avg_ps", s->moving_avg_ps);
    cJSON_AddNumberToObject(root, "count",         s->count);

    send_json_response(req, root);
    cJSON_Delete(root);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * GET /api/config
 * ------------------------------------------------------------------------- */

static esp_err_t handler_get_config(httpd_req_t *req)
{
    if (!s_meas_ctx) { send_error(req, 500, "no measurement context"); return ESP_OK; }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "delay_line_tap", s_meas_ctx->delay_tap);
    cJSON_AddNumberToObject(root, "ema_alpha",      s_meas_ctx->ema_alpha);
    cJSON_AddNumberToObject(root, "ref_freq_hz",    s_meas_ctx->ref_freq_hz);

    send_json_response(req, root);
    cJSON_Delete(root);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * POST /api/config
 * Reads body: {"delay_line_tap": N, "measurement_mode": "..."}
 * ------------------------------------------------------------------------- */

#define MAX_BODY_LEN  256

static esp_err_t handler_post_config(httpd_req_t *req)
{
    if (!s_meas_ctx) { send_error(req, 500, "no measurement context"); return ESP_OK; }

    char body[MAX_BODY_LEN] = { 0 };
    int  rcvd = httpd_req_recv(req, body, sizeof(body) - 1);
    if (rcvd <= 0) { send_error(req, 400, "empty body"); return ESP_OK; }

    cJSON *json = cJSON_ParseWithLength(body, (size_t)rcvd);
    if (!json) { send_error(req, 400, "invalid JSON"); return ESP_OK; }

    cJSON *tap_item = cJSON_GetObjectItemCaseSensitive(json, "delay_line_tap");
    if (cJSON_IsNumber(tap_item)) {
        uint8_t tap = (uint8_t)tap_item->valueint;
        measurement_set_delay_tap(s_meas_ctx, tap);
        ESP_LOGI(TAG, "Config: delay_line_tap -> %u", tap);
    }

    cJSON_Delete(json);

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    send_json_response(req, resp);
    cJSON_Delete(resp);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * POST /api/calibrate
 * Resets statistics and measurement history.
 * ------------------------------------------------------------------------- */

static esp_err_t handler_post_calibrate(httpd_req_t *req)
{
    if (!s_meas_ctx) { send_error(req, 500, "no measurement context"); return ESP_OK; }

    measurement_reset(s_meas_ctx);
    ESP_LOGI(TAG, "Calibration reset triggered via API");

    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "calibration_reset");
    send_json_response(req, resp);
    cJSON_Delete(resp);
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * Server start / stop
 * ------------------------------------------------------------------------- */

esp_err_t http_server_start(measurement_ctx_t *meas_ctx)
{
    s_meas_ctx = meas_ctx;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port    = HTTP_SERVER_PORT;
    config.max_uri_handlers = 8;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t uris[] = {
        { .uri = "/api/measurements", .method = HTTP_GET,  .handler = handler_get_measurements, .user_ctx = NULL },
        { .uri = "/api/stats",        .method = HTTP_GET,  .handler = handler_get_stats,        .user_ctx = NULL },
        { .uri = "/api/config",       .method = HTTP_GET,  .handler = handler_get_config,       .user_ctx = NULL },
        { .uri = "/api/config",       .method = HTTP_POST, .handler = handler_post_config,      .user_ctx = NULL },
        { .uri = "/api/calibrate",    .method = HTTP_POST, .handler = handler_post_calibrate,   .user_ctx = NULL },
    };

    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(s_server, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", HTTP_SERVER_PORT);
    return ESP_OK;
}

esp_err_t http_server_stop(void)
{
    if (s_server) {
        httpd_stop(s_server);
        s_server = NULL;
    }
    return ESP_OK;
}

wifi_status_t wifi_get_status(void)
{
    return s_wifi_status;
}

void wifi_get_ip_str(char *buf, size_t buf_len)
{
    strncpy(buf, s_ip_str, buf_len - 1);
    buf[buf_len - 1] = '\0';
}
