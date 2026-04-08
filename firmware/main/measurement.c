/**
 * @file measurement.c
 * @brief Measurement processing implementation for TDC7201 GPSDO Module
 */

#include "measurement.h"

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "measurement";

/* -------------------------------------------------------------------------
 * Helpers
 * ------------------------------------------------------------------------- */

/** Compute RSS of two values. */
static inline double rss2(double a, double b)
{
    return sqrt(a * a + b * b);
}

/* -------------------------------------------------------------------------
 * Public API
 * ------------------------------------------------------------------------- */

void measurement_init(measurement_ctx_t *ctx, double ref_freq_hz, double ema_alpha)
{
    memset(ctx, 0, sizeof(*ctx));
    ctx->ref_freq_hz = ref_freq_hz;
    ctx->ema_alpha   = ema_alpha;
    ctx->delay_tap   = 0;

    ctx->stats.min_ps = 1e15;   /* Large sentinel */
    ctx->stats.max_ps = -1e15;

    measurement_calc_uncertainty(&ctx->uncertainty);

    ESP_LOGI(TAG, "Measurement context initialised (ref=%.0f Hz, α=%.2f)",
             ref_freq_hz, ema_alpha);
}

void measurement_process(measurement_ctx_t *ctx,
                         const tdc7201_result_t *raw,
                         measurement_result_t *out)
{
    memset(out, 0, sizeof(*out));
    out->timestamp_us = (uint64_t)esp_timer_get_time();

    if (!raw || raw->tof_ps == 0.0) {
        ESP_LOGW(TAG, "Rejected zero or null raw measurement");
        out->valid = false;
        return;
    }

    /* Apply delay-line correction: subtract the tap offset so that the
     * START delay is cancelled and the result represents the true phase
     * difference between the PPS and 10 MHz reference. */
    double tap_correction_ps = (double)ctx->delay_tap * MEASUREMENT_TAP_DELAY_PS;
    double corrected_ps = raw->tof_ps - MEASUREMENT_START_DELAY_PS + tap_correction_ps;

    /* Sanity check: reject obviously bad values (e.g., > 1 s or < -1 s) */
    if (corrected_ps > 1e12 || corrected_ps < -1e12) {
        ESP_LOGW(TAG, "Rejected out-of-range measurement: %.3f ps", corrected_ps);
        out->valid = false;
        return;
    }

    out->phase_diff_ps = corrected_ps;
    out->valid = true;

    /* Frequency offset in ppb:
     *   Δf/f = Δt / T_ref = Δt * f_ref
     *   ppb  = (Δf/f) * 1e9
     *
     * Δt is phase_diff in seconds = phase_diff_ps * 1e-12
     */
    if (ctx->ref_freq_hz > 0) {
        double delta_t_s = corrected_ps * 1e-12;
        out->freq_offset_ppb = delta_t_s * ctx->ref_freq_hz * 1e9;
    }

    ESP_LOGD(TAG, "Processed: tof_raw=%.3f ps, corrected=%.3f ps, freq=%.3f ppb",
             raw->tof_ps, corrected_ps, out->freq_offset_ppb);
}

void measurement_add(measurement_ctx_t *ctx, const measurement_result_t *result)
{
    if (!result->valid) { return; }

    ctx->history[ctx->head] = *result;
    ctx->head = (ctx->head + 1) % MEASUREMENT_HISTORY_SIZE;
    if (ctx->fill < MEASUREMENT_HISTORY_SIZE) {
        ctx->fill++;
    }

    /* Update min/max */
    if (result->phase_diff_ps < ctx->stats.min_ps) {
        ctx->stats.min_ps = result->phase_diff_ps;
    }
    if (result->phase_diff_ps > ctx->stats.max_ps) {
        ctx->stats.max_ps = result->phase_diff_ps;
    }

    /* Exponential moving average */
    double alpha = ctx->ema_alpha;
    if (ctx->stats.count == 0) {
        ctx->stats.moving_avg_ps = result->phase_diff_ps;
    } else {
        ctx->stats.moving_avg_ps =
            alpha * result->phase_diff_ps +
            (1.0 - alpha) * ctx->stats.moving_avg_ps;
    }

    ctx->stats.count++;
}

void measurement_update_stats(measurement_ctx_t *ctx)
{
    if (ctx->fill == 0) { return; }

    /* Compute mean */
    double sum = 0.0;
    for (uint32_t i = 0; i < ctx->fill; i++) {
        uint32_t idx = (ctx->head + MEASUREMENT_HISTORY_SIZE - ctx->fill + i)
                       % MEASUREMENT_HISTORY_SIZE;
        sum += ctx->history[idx].phase_diff_ps;
    }
    double mean = sum / (double)ctx->fill;
    ctx->stats.mean_ps = mean;

    /* Compute standard deviation (population) */
    double sq_sum = 0.0;
    for (uint32_t i = 0; i < ctx->fill; i++) {
        uint32_t idx = (ctx->head + MEASUREMENT_HISTORY_SIZE - ctx->fill + i)
                       % MEASUREMENT_HISTORY_SIZE;
        double diff = ctx->history[idx].phase_diff_ps - mean;
        sq_sum += diff * diff;
    }
    ctx->stats.std_dev_ps = sqrt(sq_sum / (double)ctx->fill);

    ESP_LOGD(TAG, "Stats: mean=%.3f ps, stddev=%.3f ps, count=%u",
             ctx->stats.mean_ps, ctx->stats.std_dev_ps, ctx->stats.count);
}

void measurement_calc_uncertainty(measurement_uncertainty_t *u)
{
    u->comparator_offset_ps  = UNCERT_COMPARATOR_OFFSET_PS;
    u->comparator_jitter_ps  = UNCERT_COMPARATOR_JITTER_PS;
    u->tdc_resolution_ps     = UNCERT_TDC_RESOLUTION_PS;
    u->delay_line_error_ps   = UNCERT_DELAY_LINE_PS;
    u->temperature_drift_ps  = UNCERT_TEMPERATURE_DRIFT_PS;

    /* Total RSS */
    double rss = 0.0;
    rss = rss2(rss, u->comparator_offset_ps);
    rss = rss2(rss, u->comparator_jitter_ps);
    rss = rss2(rss, u->tdc_resolution_ps);
    rss = rss2(rss, u->delay_line_error_ps);
    rss = rss2(rss, u->temperature_drift_ps);
    u->total_rms_ps = rss;

    ESP_LOGI(TAG, "Uncertainty budget: total_rss=%.3f ps", rss);
}

void measurement_set_delay_tap(measurement_ctx_t *ctx, uint8_t tap)
{
    if (tap > 14) { tap = 14; }
    ctx->delay_tap = tap;
    ESP_LOGI(TAG, "Delay tap updated to %u (~%.0f ns correction)",
             tap, tap * 7.0);
}

void measurement_reset(measurement_ctx_t *ctx)
{
    double ref  = ctx->ref_freq_hz;
    double alpha = ctx->ema_alpha;
    measurement_init(ctx, ref, alpha);
    ESP_LOGI(TAG, "Measurement context reset");
}
