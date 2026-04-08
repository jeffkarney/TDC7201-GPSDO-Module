/**
 * @file measurement.h
 * @brief Measurement processing for the TDC7201 GPSDO Module
 *
 * Provides phase-difference calculation, frequency-offset computation,
 * statistical analysis, uncertainty propagation, and moving-average filtering.
 */

#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>
#include <stdbool.h>
#include "tdc7201_driver.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------
 * Constants
 * ------------------------------------------------------------------------- */

/** Maximum history depth for statistics and moving average */
#define MEASUREMENT_HISTORY_SIZE  256

/** Default nominal delay-line offset (ps) introduced by 12 ns START delay
 *  at tap 0 (fully compensated when tap is set correctly). */
#define MEASUREMENT_START_DELAY_PS   12000.0

/** Delay per tap of the TL07401 delay line (~7 ns per stage). */
#define MEASUREMENT_TAP_DELAY_PS      7000.0

/* -------------------------------------------------------------------------
 * Uncertainty budget constants (picoseconds)
 * ------------------------------------------------------------------------- */

/** TLV3501 input offset voltage uncertainty contribution. */
#define UNCERT_COMPARATOR_OFFSET_PS    500.0

/** TLV3501 propagation-delay jitter (50 ps RMS). */
#define UNCERT_COMPARATOR_JITTER_PS     50.0

/** TDC7201 nominal resolution per LSB. */
#define UNCERT_TDC_RESOLUTION_PS        55.0

/** Delay-line tap trimming uncertainty. */
#define UNCERT_DELAY_LINE_PS           500.0

/** Temperature drift contribution (100 ps/°C × 1°C estimate). */
#define UNCERT_TEMPERATURE_DRIFT_PS    100.0

/* -------------------------------------------------------------------------
 * Data types
 * ------------------------------------------------------------------------- */

/** Uncertainty breakdown structure (all values in picoseconds). */
typedef struct {
    double comparator_offset_ps;
    double comparator_jitter_ps;
    double tdc_resolution_ps;
    double delay_line_error_ps;
    double temperature_drift_ps;
    double total_rms_ps;           /**< Root-sum-of-squares total */
} measurement_uncertainty_t;

/** Single processed measurement result. */
typedef struct {
    double   phase_diff_ps;        /**< Phase difference in picoseconds */
    double   freq_offset_ppb;      /**< Frequency offset in parts-per-billion */
    uint64_t timestamp_us;         /**< System time at measurement (µs) */
    bool     valid;                /**< False if raw data was rejected */
} measurement_result_t;

/** Running statistics over the history buffer. */
typedef struct {
    double   mean_ps;              /**< Mean phase difference (ps) */
    double   std_dev_ps;           /**< Standard deviation (ps) */
    double   min_ps;               /**< Minimum observed value (ps) */
    double   max_ps;               /**< Maximum observed value (ps) */
    double   moving_avg_ps;        /**< Exponential moving average (ps) */
    uint32_t count;                /**< Total valid measurements */
} measurement_stats_t;

/** Context structure holding history, statistics, and configuration. */
typedef struct {
    /* History ring buffer */
    measurement_result_t history[MEASUREMENT_HISTORY_SIZE];
    uint32_t             head;          /**< Next write index */
    uint32_t             fill;          /**< Number of valid entries */

    /* Running statistics */
    measurement_stats_t  stats;
    measurement_uncertainty_t uncertainty;

    /* Configuration */
    uint8_t  delay_tap;            /**< Current delay-line tap (0–14) */
    double   ref_freq_hz;          /**< Reference oscillator frequency (Hz) */
    double   ema_alpha;            /**< EMA smoothing factor (0 < α ≤ 1) */
} measurement_ctx_t;

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

/**
 * @brief Initialise a measurement context.
 *
 * @param ctx         Context to initialise.
 * @param ref_freq_hz Reference oscillator frequency (e.g. 10e6).
 * @param ema_alpha   EMA smoothing factor (0.0–1.0).  Use 0.1 for slow, 1.0 for raw.
 */
void measurement_init(measurement_ctx_t *ctx, double ref_freq_hz, double ema_alpha);

/**
 * @brief Process a raw TDC7201 result into a measurement_result_t.
 *
 * Applies delay-line correction, validates the measurement, and converts
 * the raw time-of-flight to phase difference and frequency offset.
 *
 * @param ctx Raw TDC result.
 * @param raw Pointer to TDC result from tdc7201_read_result().
 * @param[out] out Processed result.
 */
void measurement_process(measurement_ctx_t *ctx,
                         const tdc7201_result_t *raw,
                         measurement_result_t *out);

/**
 * @brief Add a processed result to the history buffer and update statistics.
 *
 * @param ctx    Measurement context.
 * @param result Result to add.
 */
void measurement_add(measurement_ctx_t *ctx, const measurement_result_t *result);

/**
 * @brief Recalculate statistics from the history buffer.
 *
 * Call after adding measurements or when statistics are requested.
 *
 * @param ctx Measurement context.
 */
void measurement_update_stats(measurement_ctx_t *ctx);

/**
 * @brief Calculate the total RMS uncertainty from individual components.
 *
 * @param[out] u Uncertainty structure to populate.
 */
void measurement_calc_uncertainty(measurement_uncertainty_t *u);

/**
 * @brief Update the delay-line tap and apply correction offset.
 *
 * @param ctx Measurement context.
 * @param tap New tap value (0–14).
 */
void measurement_set_delay_tap(measurement_ctx_t *ctx, uint8_t tap);

/**
 * @brief Reset all history and statistics.
 *
 * @param ctx Measurement context.
 */
void measurement_reset(measurement_ctx_t *ctx);

#ifdef __cplusplus
}
#endif

#endif /* MEASUREMENT_H */
