/**
 * @file main.c
 * @brief TDC7201 GPSDO Module - Main Application Entry Point (ESP-IDF)
 *
 * Initialises the TDC7201 precision timing IC, starts a background
 * measurement task, connects to WiFi, and serves a REST API.
 *
 * Build with ESP-IDF v5.x:
 *   idf.py build flash monitor
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_system.h"

#include "tdc7201_driver.h"
#include "measurement.h"
#include "wifi_config.h"

static const char *TAG = "main";

/* -------------------------------------------------------------------------
 * Hardware pin assignment (ESP32-C3-WROOM-02U-N4)
 * Adjust to match PCB routing.
 * ------------------------------------------------------------------------- */

/* SPI interface to TDC7201 */
#define PIN_TDC_MOSI    GPIO_NUM_6
#define PIN_TDC_MISO    GPIO_NUM_5
#define PIN_TDC_SCK     GPIO_NUM_4
#define PIN_TDC_CS      GPIO_NUM_7
#define PIN_TDC_ENABLE  GPIO_NUM_8
#define PIN_TDC_TRIG    GPIO_NUM_9   /**< Asserts START pulse */
#define PIN_TDC_INT     GPIO_NUM_10  /**< Active-low interrupt from TDC7201 */

/* -------------------------------------------------------------------------
 * Application configuration
 * ------------------------------------------------------------------------- */

/** Reference clock frequency connected to TDC7201 CLK pin (Hz). */
#define REF_CLK_HZ          16000000.0  /* 16 MHz TCXO */

/** Measurement update period. */
#define MEAS_PERIOD_MS      100         /* 10 Hz */

/** EMA smoothing factor (0.1 = slow, 1.0 = no smoothing). */
#define EMA_ALPHA           0.1

/** WiFi credentials – set via menuconfig or override here. */
#define WIFI_STA_SSID       CONFIG_WIFI_SSID
#define WIFI_STA_PASSWORD   CONFIG_WIFI_PASSWORD

/* -------------------------------------------------------------------------
 * Global state
 * ------------------------------------------------------------------------- */

static tdc7201_handle_t  s_tdc  = NULL;
static measurement_ctx_t s_meas = { 0 };
static SemaphoreHandle_t s_meas_mutex = NULL;

/* -------------------------------------------------------------------------
 * Measurement task
 * ------------------------------------------------------------------------- */

static void measurement_task(void *pvParameters)
{
    TickType_t last_wake = xTaskGetTickCount();

    ESP_LOGI(TAG, "Measurement task started");

    while (true) {
        tdc7201_result_t     raw    = { 0 };
        measurement_result_t result = { 0 };

        /* Perform a complete trigger → wait → read cycle */
        esp_err_t ret = tdc7201_measure(s_tdc, 50 /* ms */, &raw);
        if (ret == ESP_OK) {
            /* Process raw data */
            xSemaphoreTake(s_meas_mutex, portMAX_DELAY);
            measurement_process(&s_meas, &raw, &result);
            measurement_add(&s_meas, &result);
            xSemaphoreGive(s_meas_mutex);

            ESP_LOGD(TAG, "Phase: %.3f ps  Freq offset: %.3f ppb",
                     result.phase_diff_ps, result.freq_offset_ppb);
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "TDC measurement timeout");
        } else {
            ESP_LOGE(TAG, "TDC measurement error: %s", esp_err_to_name(ret));
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(MEAS_PERIOD_MS));
    }
}

/* -------------------------------------------------------------------------
 * TDC7201 initialisation
 * ------------------------------------------------------------------------- */

static esp_err_t init_tdc7201(void)
{
    tdc7201_config_t cfg = {
        .interface        = TDC7201_IFACE_SPI,
        .spi = {
            .mosi_io   = PIN_TDC_MOSI,
            .miso_io   = PIN_TDC_MISO,
            .sck_io    = PIN_TDC_SCK,
            .cs_io     = PIN_TDC_CS,
            .enable_io = PIN_TDC_ENABLE,
            .trig_io   = PIN_TDC_TRIG,
            .int_io    = PIN_TDC_INT,
        },
        .measurement_mode = 2,                          /* Mode 2: multi-stop */
        .cal_periods      = TDC7201_CONFIG1_CAL_PERIODS_10,
        .num_stops        = 1,
        .ref_clk_hz       = REF_CLK_HZ,
    };

    esp_err_t ret = tdc7201_init(&cfg, &s_tdc);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "TDC7201 init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* Set default delay-line tap to compensate the 12 ns START delay.
     * At 7 ns/tap, tap 2 gives 14 ns ≈ 12 ns.  Fine-tune via /api/config. */
    tdc7201_set_delay_tap(s_tdc, 2);
    measurement_set_delay_tap(&s_meas, 2);

    ESP_LOGI(TAG, "TDC7201 ready");
    return ESP_OK;
}

/* -------------------------------------------------------------------------
 * app_main
 * ------------------------------------------------------------------------- */

void app_main(void)
{
    ESP_LOGI(TAG, "TDC7201 GPSDO Module starting...");
    ESP_LOGI(TAG, "IDF version: %s", esp_get_idf_version());

    /* ------------------------------------------------------------------ */
    /* 1. Initialise NVS flash (required by WiFi stack)                   */
    /* ------------------------------------------------------------------ */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* ------------------------------------------------------------------ */
    /* 2. Initialise measurement context                                   */
    /* ------------------------------------------------------------------ */
    measurement_init(&s_meas, REF_CLK_HZ, EMA_ALPHA);
    measurement_calc_uncertainty(&s_meas.uncertainty);

    s_meas_mutex = xSemaphoreCreateMutex();
    if (!s_meas_mutex) {
        ESP_LOGE(TAG, "Failed to create measurement mutex");
        esp_restart();
    }

    /* ------------------------------------------------------------------ */
    /* 3. Initialise TDC7201                                               */
    /* ------------------------------------------------------------------ */
    ESP_ERROR_CHECK(init_tdc7201());

    /* ------------------------------------------------------------------ */
    /* 4. Start measurement task (core 0, high priority)                   */
    /* ------------------------------------------------------------------ */
    xTaskCreatePinnedToCore(
        measurement_task,
        "meas_task",
        4096,    /* stack bytes */
        NULL,
        5,       /* priority */
        NULL,
        0        /* core */
    );

    /* ------------------------------------------------------------------ */
    /* 5. Connect to WiFi                                                  */
    /* ------------------------------------------------------------------ */
    ret = wifi_init_sta(WIFI_STA_SSID, WIFI_STA_PASSWORD);
    if (ret == ESP_OK) {
        char ip[16];
        wifi_get_ip_str(ip, sizeof(ip));
        ESP_LOGI(TAG, "Connected! IP: %s", ip);

        /* ---------------------------------------------------------------- */
        /* 6. Start HTTP REST API server                                    */
        /* ---------------------------------------------------------------- */
        ESP_ERROR_CHECK(http_server_start(&s_meas));
        ESP_LOGI(TAG, "REST API available at http://%s/api/measurements", ip);
    } else {
        ESP_LOGW(TAG, "WiFi not available – running in standalone mode");
    }

    ESP_LOGI(TAG, "Initialisation complete");
    /* app_main returns; FreeRTOS idle task handles cleanup */
}
