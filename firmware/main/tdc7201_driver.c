/**
 * @file tdc7201_driver.c
 * @brief TDC7201 Time-to-Digital Converter Driver Implementation
 *
 * Implements the TDC7201 driver API declared in tdc7201_driver.h.
 * Supports both SPI and I2C interfaces.
 *
 * Time-of-flight formula (Mode 1):
 *   tof = (TIME1 / normLSB) - CLOCK_COUNT1 * T_clk
 *
 * where:
 *   normLSB = (CALIBRATION2 - CALIBRATION1) / (cal_periods - 1) / T_clk
 *   T_clk   = 1 / ref_clk_hz
 */

#include "tdc7201_driver.h"

#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "esp_log.h"

static const char *TAG = "tdc7201";

/* -------------------------------------------------------------------------
 * Internal device structure
 * ------------------------------------------------------------------------- */
struct tdc7201_dev_t {
    tdc7201_config_t cfg;
    /* SPI */
    spi_device_handle_t spi_dev;
    /* I2C */
    i2c_port_t i2c_port;
    /* Delay-line tap (external GPIO-driven shift register or I2C DAC) */
    uint8_t delay_tap;
    /* Shadow of CONFIG1 so we can set START_MEAS without losing other bits */
    uint8_t config1_shadow;
};

/* -------------------------------------------------------------------------
 * SPI helpers
 * ------------------------------------------------------------------------- */

/**
 * TDC7201 SPI frame:
 *   Byte 0: [RW | A5 | A4 | A3 | A2 | A1 | A0 | 0]
 *           RW=0 write, RW=1 read
 *   Byte 1: data (write) or don't-care (read)
 *   For 24-bit reads: 3 additional bytes follow.
 */
#define SPI_CMD_WRITE(reg)  ((uint8_t)((reg) & 0x3F))
#define SPI_CMD_READ(reg)   ((uint8_t)(0x80 | ((reg) & 0x3F)))

static esp_err_t spi_write_reg(struct tdc7201_dev_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t tx[2] = { SPI_CMD_WRITE(reg), val };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };
    return spi_device_transmit(dev->spi_dev, &t);
}

static esp_err_t spi_read_reg(struct tdc7201_dev_t *dev, uint8_t reg, uint8_t *val)
{
    uint8_t tx[2] = { SPI_CMD_READ(reg), 0x00 };
    uint8_t rx[2] = { 0 };
    spi_transaction_t t = {
        .length    = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t ret = spi_device_transmit(dev->spi_dev, &t);
    if (ret == ESP_OK) {
        *val = rx[1];
    }
    return ret;
}

static esp_err_t spi_read_reg24(struct tdc7201_dev_t *dev, uint8_t reg, uint32_t *val)
{
    uint8_t tx[4] = { SPI_CMD_READ(reg), 0x00, 0x00, 0x00 };
    uint8_t rx[4] = { 0 };
    spi_transaction_t t = {
        .length    = 32,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t ret = spi_device_transmit(dev->spi_dev, &t);
    if (ret == ESP_OK) {
        *val = ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * I2C helpers
 * ------------------------------------------------------------------------- */

#define I2C_TIMEOUT_MS  50

static esp_err_t i2c_write_reg(struct tdc7201_dev_t *dev, uint8_t reg, uint8_t val)
{
    uint8_t addr = dev->cfg.i2c.address;
    i2c_port_t port = dev->i2c_port;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write_byte(cmd, val, true);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read_reg(struct tdc7201_dev_t *dev, uint8_t reg, uint8_t *val)
{
    uint8_t addr = dev->cfg.i2c.address;
    i2c_port_t port = dev->i2c_port;

    /* Write register address */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    /* Repeated start + read */
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, val, I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static esp_err_t i2c_read_reg24(struct tdc7201_dev_t *dev, uint8_t reg, uint32_t *val)
{
    uint8_t addr = dev->cfg.i2c.address;
    i2c_port_t port = dev->i2c_port;
    uint8_t buf[3] = { 0 };

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read(cmd, buf, 2, I2C_MASTER_ACK);
    i2c_master_read_byte(cmd, &buf[2], I2C_MASTER_NACK);
    i2c_master_stop(cmd);

    esp_err_t ret = i2c_master_cmd_begin(port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    if (ret == ESP_OK) {
        *val = ((uint32_t)buf[0] << 16) | ((uint32_t)buf[1] << 8) | buf[2];
    }
    return ret;
}

/* -------------------------------------------------------------------------
 * Public API Implementation
 * ------------------------------------------------------------------------- */

esp_err_t tdc7201_init(const tdc7201_config_t *cfg, tdc7201_handle_t *hdl)
{
    if (!cfg || !hdl) {
        return ESP_ERR_INVALID_ARG;
    }

    struct tdc7201_dev_t *dev = calloc(1, sizeof(*dev));
    if (!dev) {
        return ESP_ERR_NO_MEM;
    }
    dev->cfg = *cfg;
    *hdl = dev;

    esp_err_t ret = ESP_OK;

    if (cfg->interface == TDC7201_IFACE_SPI) {
        /* Configure ENABLE and TRIG GPIOs */
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        if (cfg->spi.enable_io >= 0) {
            io_conf.pin_bit_mask = 1ULL << cfg->spi.enable_io;
            gpio_config(&io_conf);
            gpio_set_level(cfg->spi.enable_io, 1);  /* Assert ENABLE */
        }
        if (cfg->spi.trig_io >= 0) {
            io_conf.pin_bit_mask = 1ULL << cfg->spi.trig_io;
            gpio_config(&io_conf);
            gpio_set_level(cfg->spi.trig_io, 0);
        }

        spi_bus_config_t bus_cfg = {
            .mosi_io_num   = cfg->spi.mosi_io,
            .miso_io_num   = cfg->spi.miso_io,
            .sclk_io_num   = cfg->spi.sck_io,
            .quadwp_io_num = -1,
            .quadhd_io_num = -1,
            .max_transfer_sz = 32,
        };
        ret = spi_bus_initialize(SPI2_HOST, &bus_cfg, SPI_DMA_CH_AUTO);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_initialize failed: %s", esp_err_to_name(ret));
            goto err_free;
        }

        spi_device_interface_config_t dev_cfg = {
            .clock_speed_hz = 10 * 1000 * 1000,  /* 10 MHz */
            .mode           = 0,                  /* CPOL=0, CPHA=0 */
            .spics_io_num   = cfg->spi.cs_io,
            .queue_size     = 4,
        };
        ret = spi_bus_add_device(SPI2_HOST, &dev_cfg, &dev->spi_dev);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "spi_bus_add_device failed: %s", esp_err_to_name(ret));
            goto err_free;
        }
    } else {
        /* I2C interface */
        i2c_config_t i2c_cfg = {
            .mode             = I2C_MODE_MASTER,
            .sda_io_num       = cfg->i2c.sda_io,
            .scl_io_num       = cfg->i2c.scl_io,
            .sda_pullup_en    = GPIO_PULLUP_ENABLE,
            .scl_pullup_en    = GPIO_PULLUP_ENABLE,
            .master.clk_speed = cfg->i2c.clk_hz,
        };
        dev->i2c_port = I2C_NUM_0;
        ret = i2c_param_config(dev->i2c_port, &i2c_cfg);
        if (ret != ESP_OK) { goto err_free; }
        ret = i2c_driver_install(dev->i2c_port, I2C_MODE_MASTER, 0, 0, 0);
        if (ret != ESP_OK) { goto err_free; }
    }

    /* Soft reset via ENABLE toggle */
    ret = tdc7201_reset(dev);
    if (ret != ESP_OK) { goto err_free; }

    /* Build CONFIG1 shadow value */
    uint8_t config1 = cfg->cal_periods;  /* caller uses TDC7201_CONFIG1_CAL_PERIODS_* */
    if (cfg->measurement_mode == 2) {
        config1 |= TDC7201_CONFIG1_MEAS_MODE2;
    }
    dev->config1_shadow = config1;

    ret = tdc7201_write_reg(dev, TDC7201_REG_CONFIG1, config1);
    if (ret != ESP_OK) { goto err_free; }

    /* CONFIG2: set number of stop pulses (bits [2:0] – 0 means 1 stop) */
    uint8_t config2 = 0;
    if (cfg->num_stops > 1 && cfg->num_stops <= 5) {
        config2 = (uint8_t)(cfg->num_stops - 1) & 0x07;
    }
    ret = tdc7201_write_reg(dev, TDC7201_REG_CONFIG2, config2);
    if (ret != ESP_OK) { goto err_free; }

    /* Clear any pending interrupts */
    ret = tdc7201_write_reg(dev, TDC7201_REG_INT_STATUS, 0x1F);
    if (ret != ESP_OK) { goto err_free; }

    /* Unmask new-measurement interrupt */
    ret = tdc7201_write_reg(dev, TDC7201_REG_INT_MASK, TDC7201_INT_STATUS_NEW_MEAS);
    if (ret != ESP_OK) { goto err_free; }

    ESP_LOGI(TAG, "TDC7201 initialized (iface=%s, mode=%d)",
             cfg->interface == TDC7201_IFACE_SPI ? "SPI" : "I2C",
             cfg->measurement_mode);
    return ESP_OK;

err_free:
    free(dev);
    *hdl = NULL;
    return ret;
}

esp_err_t tdc7201_deinit(tdc7201_handle_t hdl)
{
    if (!hdl) { return ESP_ERR_INVALID_ARG; }
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;

    if (dev->cfg.interface == TDC7201_IFACE_SPI) {
        spi_bus_remove_device(dev->spi_dev);
        spi_bus_free(SPI2_HOST);
    } else {
        i2c_driver_delete(dev->i2c_port);
    }
    free(dev);
    return ESP_OK;
}

esp_err_t tdc7201_write_reg(tdc7201_handle_t hdl, uint8_t reg, uint8_t val)
{
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;
    if (dev->cfg.interface == TDC7201_IFACE_SPI) {
        return spi_write_reg(dev, reg, val);
    }
    return i2c_write_reg(dev, reg, val);
}

esp_err_t tdc7201_read_reg(tdc7201_handle_t hdl, uint8_t reg, uint8_t *val)
{
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;
    if (dev->cfg.interface == TDC7201_IFACE_SPI) {
        return spi_read_reg(dev, reg, val);
    }
    return i2c_read_reg(dev, reg, val);
}

esp_err_t tdc7201_read_reg24(tdc7201_handle_t hdl, uint8_t reg, uint32_t *val)
{
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;
    if (dev->cfg.interface == TDC7201_IFACE_SPI) {
        return spi_read_reg24(dev, reg, val);
    }
    return i2c_read_reg24(dev, reg, val);
}

esp_err_t tdc7201_trigger_measurement(tdc7201_handle_t hdl)
{
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;
    uint8_t config1 = dev->config1_shadow | TDC7201_CONFIG1_START_MEAS;
    return tdc7201_write_reg(hdl, TDC7201_REG_CONFIG1, config1);
}

esp_err_t tdc7201_wait_for_result(tdc7201_handle_t hdl, uint32_t timeout_ms)
{
    uint32_t elapsed = 0;
    uint8_t status = 0;

    while (elapsed < timeout_ms) {
        esp_err_t ret = tdc7201_read_reg(hdl, TDC7201_REG_INT_STATUS, &status);
        if (ret != ESP_OK) { return ret; }

        if (status & TDC7201_INT_STATUS_COARSE_OVF ||
            status & TDC7201_INT_STATUS_CLOCK_OVF) {
            ESP_LOGW(TAG, "TDC overflow: INT_STATUS=0x%02x", status);
            return ESP_ERR_INVALID_RESPONSE;
        }
        if (status & TDC7201_INT_STATUS_NEW_MEAS) {
            /* Clear interrupt flag */
            tdc7201_write_reg(hdl, TDC7201_REG_INT_STATUS, TDC7201_INT_STATUS_NEW_MEAS);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        elapsed++;
    }
    return ESP_ERR_TIMEOUT;
}

esp_err_t tdc7201_read_result(tdc7201_handle_t hdl, tdc7201_result_t *r)
{
    if (!hdl || !r) { return ESP_ERR_INVALID_ARG; }
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;

    memset(r, 0, sizeof(*r));

    esp_err_t ret;
    ret = tdc7201_read_reg24(hdl, TDC7201_REG_TIME1,        &r->time1_raw);
    if (ret != ESP_OK) { return ret; }
    ret = tdc7201_read_reg24(hdl, TDC7201_REG_TIME2,        &r->time2_raw);
    if (ret != ESP_OK) { return ret; }
    ret = tdc7201_read_reg24(hdl, TDC7201_REG_CLOCK_COUNT1, &r->clock_count1);
    if (ret != ESP_OK) { return ret; }
    ret = tdc7201_read_reg24(hdl, TDC7201_REG_CALIBRATION1, &r->calibration1);
    if (ret != ESP_OK) { return ret; }
    ret = tdc7201_read_reg24(hdl, TDC7201_REG_CALIBRATION2, &r->calibration2);
    if (ret != ESP_OK) { return ret; }

    /* -------------------------------------------------------------------
     * TDC7201 ToF formula (Mode 1):
     *
     *   normLSB = (CAL2 - CAL1) / (cal_periods - 1) * T_clk
     *   tof     = TIME1 * normLSB - CLOCK_COUNT1 * T_clk
     *
     * Where T_clk = 1 / ref_clk_hz (in seconds).
     * ------------------------------------------------------------------- */
    double T_clk_s = 1.0 / dev->cfg.ref_clk_hz;

    /* Number of calibration periods stored in CONFIG1 bits [7:6] */
    uint8_t cal_per_code = (dev->config1_shadow >> 6) & 0x03;
    static const uint8_t cal_per_table[] = { 2, 10, 20, 40 };
    uint8_t cal_per = cal_per_table[cal_per_code];

    double cal_diff = (double)r->calibration2 - (double)r->calibration1;
    if (cal_diff <= 0) {
        ESP_LOGW(TAG, "Invalid calibration values: CAL1=%u CAL2=%u",
                 r->calibration1, r->calibration2);
        return ESP_ERR_INVALID_RESPONSE;
    }

    double normLSB_s = (cal_diff / (double)(cal_per - 1)) * T_clk_s;

    /* tof in seconds */
    double tof_s = (double)r->time1_raw * normLSB_s
                   - (double)r->clock_count1 * T_clk_s;

    /* Convert to picoseconds */
    r->tof_ps = tof_s * 1e12;

    ESP_LOGD(TAG, "TIME1=%u CLK_CNT1=%u CAL1=%u CAL2=%u ToF=%.3f ps",
             r->time1_raw, r->clock_count1,
             r->calibration1, r->calibration2, r->tof_ps);

    return ESP_OK;
}

esp_err_t tdc7201_measure(tdc7201_handle_t hdl, uint32_t timeout_ms,
                          tdc7201_result_t *r)
{
    esp_err_t ret = tdc7201_trigger_measurement(hdl);
    if (ret != ESP_OK) { return ret; }

    ret = tdc7201_wait_for_result(hdl, timeout_ms);
    if (ret != ESP_OK) { return ret; }

    return tdc7201_read_result(hdl, r);
}

esp_err_t tdc7201_set_delay_tap(tdc7201_handle_t hdl, uint8_t tap)
{
    if (!hdl) { return ESP_ERR_INVALID_ARG; }
    if (tap > 14) { return ESP_ERR_INVALID_ARG; }

    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;
    dev->delay_tap = tap;

    /*
     * The delay tap is controlled externally via GPIO-driven shift register
     * to the TL07401 delay line.  The actual GPIO write would be done here;
     * the implementation is board-specific and should be adapted to match
     * the hardware design.
     */
    ESP_LOGI(TAG, "Delay line tap set to %u (~%.0f ns)", tap, tap * 7.0);
    return ESP_OK;
}

esp_err_t tdc7201_reset(tdc7201_handle_t hdl)
{
    if (!hdl) { return ESP_ERR_INVALID_ARG; }
    struct tdc7201_dev_t *dev = (struct tdc7201_dev_t *)hdl;

    if (dev->cfg.interface == TDC7201_IFACE_SPI &&
        dev->cfg.spi.enable_io >= 0) {
        gpio_set_level(dev->cfg.spi.enable_io, 0);
        vTaskDelay(pdMS_TO_TICKS(1));
        gpio_set_level(dev->cfg.spi.enable_io, 1);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return ESP_OK;
}
