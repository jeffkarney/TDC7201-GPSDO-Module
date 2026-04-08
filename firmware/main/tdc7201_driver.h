/**
 * @file tdc7201_driver.h
 * @brief TDC7201 Time-to-Digital Converter Driver Header
 *
 * Driver for the Texas Instruments TDC7201, a high-resolution time-to-digital
 * converter (TDC) used for precision timing measurements.
 *
 * Supports both SPI and I2C interfaces, provides measurement trigger/acquisition,
 * calibration, and interrupt handling.
 */

#ifndef TDC7201_DRIVER_H
#define TDC7201_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Register Map (TDC7201 datasheet Table 1)
 * ------------------------------------------------------------------------- */

/** CONFIG1 register: Measurement mode, start edge, calibration periods */
#define TDC7201_REG_CONFIG1          0x00
/** CONFIG2 register: Calibration2, stop mask, stop number */
#define TDC7201_REG_CONFIG2          0x01
/** INT_STATUS: Interrupt flags (new measurement, coarse overflow, clock overflow) */
#define TDC7201_REG_INT_STATUS       0x02
/** INT_MASK: Interrupt mask */
#define TDC7201_REG_INT_MASK         0x03
/** COARSE_CNTR_OVF_H/L: Coarse counter overflow threshold */
#define TDC7201_REG_COARSE_CNTR_OVF_H  0x04
#define TDC7201_REG_COARSE_CNTR_OVF_L  0x05
/** CLOCK_CNTR_OVF_H/L: Clock counter overflow threshold */
#define TDC7201_REG_CLOCK_CNTR_OVF_H   0x06
#define TDC7201_REG_CLOCK_CNTR_OVF_L   0x07
/** CLOCK_CNTR_STOP: Number of clock counter stop pulses */
#define TDC7201_REG_CLOCK_CNTR_STOP_MASK_H  0x08
#define TDC7201_REG_CLOCK_CNTR_STOP_MASK_L  0x09
/** TIME1–TIME6: Raw ring-oscillator count registers (read-only) */
#define TDC7201_REG_TIME1            0x10
#define TDC7201_REG_TIME2            0x11
#define TDC7201_REG_TIME3            0x12
#define TDC7201_REG_TIME4            0x13
#define TDC7201_REG_TIME5            0x14
#define TDC7201_REG_TIME6            0x15
/** CLOCK_COUNT1–5: Clock counter values (read-only) */
#define TDC7201_REG_CLOCK_COUNT1     0x16
#define TDC7201_REG_CLOCK_COUNT2     0x17
#define TDC7201_REG_CLOCK_COUNT3     0x18
#define TDC7201_REG_CLOCK_COUNT4     0x19
#define TDC7201_REG_CLOCK_COUNT5     0x1A
/** CALIBRATION1/2: Ring oscillator calibration values (read-only) */
#define TDC7201_REG_CALIBRATION1     0x1B
#define TDC7201_REG_CALIBRATION2     0x1C

/* -------------------------------------------------------------------------
 * CONFIG1 Register Bit Definitions
 * ------------------------------------------------------------------------- */
/** Calibration periods: 2, 10, 20, 40 */
#define TDC7201_CONFIG1_CAL_PERIODS_2    (0x00 << 6)
#define TDC7201_CONFIG1_CAL_PERIODS_10   (0x01 << 6)
#define TDC7201_CONFIG1_CAL_PERIODS_20   (0x02 << 6)
#define TDC7201_CONFIG1_CAL_PERIODS_40   (0x03 << 6)
/** Averaging cycles */
#define TDC7201_CONFIG1_AVG_CYCLES_1     (0x00 << 3)
#define TDC7201_CONFIG1_AVG_CYCLES_2     (0x01 << 3)
#define TDC7201_CONFIG1_AVG_CYCLES_4     (0x02 << 3)
#define TDC7201_CONFIG1_AVG_CYCLES_8     (0x03 << 3)
/** Measurement mode */
#define TDC7201_CONFIG1_MEAS_MODE1       (0x00 << 1)  /**< Mode 1: single measurement */
#define TDC7201_CONFIG1_MEAS_MODE2       (0x01 << 1)  /**< Mode 2: multi-stop measurement */
/** Start new measurement */
#define TDC7201_CONFIG1_START_MEAS       (0x01 << 0)

/* -------------------------------------------------------------------------
 * INT_STATUS Register Bit Definitions
 * ------------------------------------------------------------------------- */
#define TDC7201_INT_STATUS_NEW_MEAS      (0x01 << 0)
#define TDC7201_INT_STATUS_COARSE_OVF    (0x01 << 1)
#define TDC7201_INT_STATUS_CLOCK_OVF     (0x01 << 2)

/* -------------------------------------------------------------------------
 * Interface selection
 * ------------------------------------------------------------------------- */
typedef enum {
    TDC7201_IFACE_SPI,  /**< SPI interface (preferred for high speed) */
    TDC7201_IFACE_I2C   /**< I2C interface */
} tdc7201_iface_t;

/* -------------------------------------------------------------------------
 * SPI GPIO configuration
 * ------------------------------------------------------------------------- */
typedef struct {
    int mosi_io;      /**< MOSI GPIO pin */
    int miso_io;      /**< MISO GPIO pin */
    int sck_io;       /**< SCK GPIO pin */
    int cs_io;        /**< Chip select GPIO pin */
    int enable_io;    /**< ENABLE GPIO pin (active high) */
    int trig_io;      /**< TRIG GPIO (START pulse) */
    int int_io;       /**< INT GPIO (interrupt, active low) */
} tdc7201_spi_gpio_t;

/* -------------------------------------------------------------------------
 * I2C GPIO configuration
 * ------------------------------------------------------------------------- */
typedef struct {
    int sda_io;       /**< SDA GPIO pin */
    int scl_io;       /**< SCL GPIO pin */
    uint8_t address;  /**< 7-bit I2C device address */
    uint32_t clk_hz;  /**< I2C clock frequency in Hz */
} tdc7201_i2c_gpio_t;

/* -------------------------------------------------------------------------
 * Driver configuration
 * ------------------------------------------------------------------------- */
typedef struct {
    tdc7201_iface_t interface;  /**< Communication interface */
    union {
        tdc7201_spi_gpio_t spi; /**< SPI configuration (if interface == SPI) */
        tdc7201_i2c_gpio_t i2c; /**< I2C configuration (if interface == I2C) */
    };
    uint8_t  measurement_mode;  /**< 1 = Mode 1, 2 = Mode 2 */
    uint8_t  cal_periods;       /**< Calibration periods (use CONFIG1 constants) */
    uint8_t  num_stops;         /**< Number of STOP pulses in Mode 2 (1–5) */
    double   ref_clk_hz;        /**< Reference clock frequency in Hz (e.g. 16e6) */
} tdc7201_config_t;

/* -------------------------------------------------------------------------
 * Measurement result
 * ------------------------------------------------------------------------- */
typedef struct {
    double   tof_ps;          /**< Time-of-flight / phase difference in picoseconds */
    uint32_t time1_raw;       /**< TIME1 ring-oscillator raw value */
    uint32_t time2_raw;       /**< TIME2 ring-oscillator raw value */
    uint32_t clock_count1;    /**< CLOCK_COUNT1 raw value */
    uint32_t calibration1;    /**< CALIBRATION1 raw value */
    uint32_t calibration2;    /**< CALIBRATION2 raw value */
    bool     overflow;        /**< True if coarse or clock counter overflowed */
} tdc7201_result_t;

/* -------------------------------------------------------------------------
 * Device handle (opaque)
 * ------------------------------------------------------------------------- */
typedef struct tdc7201_dev_t *tdc7201_handle_t;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialize TDC7201 driver and underlying peripheral.
 *
 * @param cfg      Pointer to driver configuration structure.
 * @param[out] hdl Handle populated on success.
 * @return ESP_OK on success, or an error code.
 */
esp_err_t tdc7201_init(const tdc7201_config_t *cfg, tdc7201_handle_t *hdl);

/**
 * @brief Free all resources and deinitialize the TDC7201 driver.
 *
 * @param hdl Handle returned by tdc7201_init().
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_deinit(tdc7201_handle_t hdl);

/**
 * @brief Write a single register byte.
 *
 * @param hdl  Device handle.
 * @param reg  Register address.
 * @param val  Value to write.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_write_reg(tdc7201_handle_t hdl, uint8_t reg, uint8_t val);

/**
 * @brief Read a single register byte.
 *
 * @param hdl      Device handle.
 * @param reg      Register address.
 * @param[out] val Value read from register.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_read_reg(tdc7201_handle_t hdl, uint8_t reg, uint8_t *val);

/**
 * @brief Read a 24-bit (3-byte) result register.
 *
 * TDC7201 TIME and CLOCK_COUNT registers are 24-bit wide.
 *
 * @param hdl      Device handle.
 * @param reg      Register address.
 * @param[out] val 24-bit value.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_read_reg24(tdc7201_handle_t hdl, uint8_t reg, uint32_t *val);

/**
 * @brief Trigger a new measurement (set START_MEAS bit in CONFIG1).
 *
 * @param hdl Device handle.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_trigger_measurement(tdc7201_handle_t hdl);

/**
 * @brief Poll the INT_STATUS register and wait for a new measurement.
 *
 * @param hdl          Device handle.
 * @param timeout_ms   Timeout in milliseconds.
 * @return ESP_OK on success, ESP_ERR_TIMEOUT if no result within timeout.
 */
esp_err_t tdc7201_wait_for_result(tdc7201_handle_t hdl, uint32_t timeout_ms);

/**
 * @brief Read and calculate a complete measurement result.
 *
 * Reads TIME1, TIME2, CLOCK_COUNT1, CALIBRATION1/2 and computes the
 * time-of-flight using the TDC7201 formula.
 *
 * @param hdl     Device handle.
 * @param[out] r  Populated result structure.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_read_result(tdc7201_handle_t hdl, tdc7201_result_t *r);

/**
 * @brief Perform a complete trigger → wait → read cycle.
 *
 * Convenience wrapper around trigger + wait_for_result + read_result.
 *
 * @param hdl          Device handle.
 * @param timeout_ms   Timeout passed to tdc7201_wait_for_result().
 * @param[out] r       Populated result structure.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_measure(tdc7201_handle_t hdl, uint32_t timeout_ms,
                          tdc7201_result_t *r);

/**
 * @brief Set the programmable delay-line tap for START offset correction.
 *
 * The TDC7201 has an inherent ~12 ns START delay. A TL07401 delay line
 * provides ~7 ns per tap, allowing software compensation.
 *
 * @param hdl  Device handle.
 * @param tap  Tap value (0–14).
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_set_delay_tap(tdc7201_handle_t hdl, uint8_t tap);

/**
 * @brief Reset TDC7201 by toggling the ENABLE pin.
 *
 * @param hdl Device handle.
 * @return ESP_OK on success.
 */
esp_err_t tdc7201_reset(tdc7201_handle_t hdl);

#ifdef __cplusplus
}
#endif

#endif /* TDC7201_DRIVER_H */
