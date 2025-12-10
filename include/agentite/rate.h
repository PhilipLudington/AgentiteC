/**
 * @file rate.h
 * @brief Rate Tracking / Metrics History System
 *
 * Rolling window metrics for production and consumption rates. Useful for
 * economy statistics, performance monitoring, and analytics displays.
 *
 * Features:
 * - Multiple tracked metrics (resources, power, etc.)
 * - Periodic sampling into circular buffer
 * - Time window queries (last N seconds)
 * - Min/max/mean/sum calculations
 * - Production and consumption tracking
 */

#ifndef AGENTITE_RATE_H
#define AGENTITE_RATE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of metrics to track */
#ifndef AGENTITE_RATE_MAX_METRICS
#define AGENTITE_RATE_MAX_METRICS 32
#endif

/* Maximum number of samples in history */
#ifndef AGENTITE_RATE_MAX_SAMPLES
#define AGENTITE_RATE_MAX_SAMPLES 256
#endif

/**
 * @brief Single sample for one metric
 */
typedef struct Agentite_RateSample {
    float timestamp;        /**< Time when sample was taken */
    int32_t produced;       /**< Amount produced during interval */
    int32_t consumed;       /**< Amount consumed during interval */
} Agentite_RateSample;

/**
 * @brief Accumulated stats for a time window
 */
typedef struct Agentite_RateStats {
    float time_window;      /**< Actual time covered by stats */
    int sample_count;       /**< Number of samples in window */

    int32_t total_produced; /**< Total production in window */
    int32_t total_consumed; /**< Total consumption in window */
    int32_t total_net;      /**< Net (produced - consumed) */

    float production_rate;  /**< Production per second */
    float consumption_rate; /**< Consumption per second */
    float net_rate;         /**< Net change per second */

    int32_t min_production; /**< Minimum production in any sample */
    int32_t max_production; /**< Maximum production in any sample */
    int32_t min_consumption;/**< Minimum consumption in any sample */
    int32_t max_consumption;/**< Maximum consumption in any sample */
} Agentite_RateStats;

/**
 * @brief Opaque rate tracker handle
 */
typedef struct Agentite_RateTracker Agentite_RateTracker;

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

/**
 * @brief Create a rate tracker
 *
 * @param metric_count Number of metrics to track (max AGENTITE_RATE_MAX_METRICS)
 * @param sample_interval Seconds between samples (e.g., 0.5f = twice per second)
 * @param history_size Number of samples to keep (max AGENTITE_RATE_MAX_SAMPLES)
 * @return New rate tracker or NULL on failure
 *
 * @note Total time coverage = sample_interval * history_size
 *       e.g., 0.5f interval * 120 samples = 60 seconds of history
 */
Agentite_RateTracker *agentite_rate_create(int metric_count, float sample_interval, int history_size);

/**
 * @brief Destroy a rate tracker
 *
 * @param tracker Rate tracker to destroy
 */
void agentite_rate_destroy(Agentite_RateTracker *tracker);

/**
 * @brief Reset all metrics and history
 *
 * @param tracker Rate tracker
 */
void agentite_rate_reset(Agentite_RateTracker *tracker);

/* ============================================================================
 * Metric Configuration
 * ========================================================================= */

/**
 * @brief Set name for a metric (for debugging/display)
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index (0 to metric_count-1)
 * @param name Metric name (copied, max 31 chars)
 */
void agentite_rate_set_name(Agentite_RateTracker *tracker, int metric_id, const char *name);

/**
 * @brief Get name of a metric
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @return Metric name or empty string if not set
 */
const char *agentite_rate_get_name(Agentite_RateTracker *tracker, int metric_id);

/**
 * @brief Get number of metrics being tracked
 *
 * @param tracker Rate tracker
 * @return Metric count
 */
int agentite_rate_get_metric_count(Agentite_RateTracker *tracker);

/* ============================================================================
 * Recording
 * ========================================================================= */

/**
 * @brief Update tracker (call each frame)
 *
 * Accumulates delta time and takes a sample when interval is reached.
 *
 * @param tracker Rate tracker
 * @param delta_time Time since last update in seconds
 */
void agentite_rate_update(Agentite_RateTracker *tracker, float delta_time);

/**
 * @brief Record production for a metric
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param amount Amount produced (positive)
 */
void agentite_rate_record_production(Agentite_RateTracker *tracker, int metric_id, int32_t amount);

/**
 * @brief Record consumption for a metric
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param amount Amount consumed (positive)
 */
void agentite_rate_record_consumption(Agentite_RateTracker *tracker, int metric_id, int32_t amount);

/**
 * @brief Record both production and consumption
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param produced Amount produced
 * @param consumed Amount consumed
 */
void agentite_rate_record(Agentite_RateTracker *tracker, int metric_id,
                        int32_t produced, int32_t consumed);

/**
 * @brief Force a sample to be taken now
 *
 * Useful for turn-based games that want to sample at specific points.
 *
 * @param tracker Rate tracker
 */
void agentite_rate_force_sample(Agentite_RateTracker *tracker);

/* ============================================================================
 * Rate Queries
 * ========================================================================= */

/**
 * @brief Get production rate over a time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Production per second
 */
float agentite_rate_get_production_rate(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get consumption rate over a time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Consumption per second
 */
float agentite_rate_get_consumption_rate(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get net rate (production - consumption) over a time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Net change per second
 */
float agentite_rate_get_net_rate(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get comprehensive stats for a time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Stats structure with all calculated values
 */
Agentite_RateStats agentite_rate_get_stats(Agentite_RateTracker *tracker, int metric_id, float time_window);

/* ============================================================================
 * Aggregate Queries
 * ========================================================================= */

/**
 * @brief Get total production in a time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Total production
 */
int32_t agentite_rate_get_total_production(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get total consumption in a time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Total consumption
 */
int32_t agentite_rate_get_total_consumption(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get minimum production in any sample within time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Minimum production
 */
int32_t agentite_rate_get_min_production(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get maximum production in any sample within time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Maximum production
 */
int32_t agentite_rate_get_max_production(Agentite_RateTracker *tracker, int metric_id, float time_window);

/**
 * @brief Get average production per sample in time window
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @return Average production per sample
 */
float agentite_rate_get_avg_production(Agentite_RateTracker *tracker, int metric_id, float time_window);

/* ============================================================================
 * History Access
 * ========================================================================= */

/**
 * @brief Get sample history for a metric
 *
 * Returns samples in chronological order (oldest first).
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param time_window Time window in seconds (0 = all history)
 * @param out_samples Array to fill with samples
 * @param max_samples Size of output array
 * @return Number of samples written
 */
int agentite_rate_get_history(Agentite_RateTracker *tracker, int metric_id, float time_window,
                            Agentite_RateSample *out_samples, int max_samples);

/**
 * @brief Get the most recent sample for a metric
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @param out_sample Output sample
 * @return true if sample exists
 */
bool agentite_rate_get_latest_sample(Agentite_RateTracker *tracker, int metric_id,
                                   Agentite_RateSample *out_sample);

/**
 * @brief Get number of samples currently in history
 *
 * @param tracker Rate tracker
 * @param metric_id Metric index
 * @return Sample count
 */
int agentite_rate_get_sample_count(Agentite_RateTracker *tracker, int metric_id);

/* ============================================================================
 * Configuration Queries
 * ========================================================================= */

/**
 * @brief Get sample interval
 *
 * @param tracker Rate tracker
 * @return Sample interval in seconds
 */
float agentite_rate_get_interval(Agentite_RateTracker *tracker);

/**
 * @brief Get maximum history size
 *
 * @param tracker Rate tracker
 * @return Maximum number of samples stored
 */
int agentite_rate_get_history_size(Agentite_RateTracker *tracker);

/**
 * @brief Get total time coverage of history
 *
 * @param tracker Rate tracker
 * @return Maximum time in seconds that can be queried
 */
float agentite_rate_get_max_time_window(Agentite_RateTracker *tracker);

/**
 * @brief Get current time accumulator
 *
 * @param tracker Rate tracker
 * @return Current total time in seconds
 */
float agentite_rate_get_current_time(Agentite_RateTracker *tracker);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_RATE_H */
