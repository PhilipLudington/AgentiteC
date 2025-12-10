/**
 * @file rate.c
 * @brief Rate Tracking / Metrics History System implementation
 */

#include "agentite/agentite.h"
#include "agentite/rate.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Per-metric tracking data
 */
typedef struct Agentite_MetricTracker {
    char name[32];                              /**< Metric name */

    /* Current accumulator (before next sample) */
    int32_t pending_produced;
    int32_t pending_consumed;

    /* Circular buffer of samples */
    Agentite_RateSample *samples;
    int sample_head;                            /**< Next write position */
    int sample_count;                           /**< Current number of samples */
} Agentite_MetricTracker;

/**
 * @brief Rate tracker structure
 */
struct Agentite_RateTracker {
    Agentite_MetricTracker *metrics;
    int metric_count;

    float sample_interval;                      /**< Seconds between samples */
    int history_size;                           /**< Max samples per metric */

    float time_accumulator;                     /**< Accumulated time since last sample */
    float total_time;                           /**< Total time since creation */
};

/* ============================================================================
 * Helper Functions
 * ========================================================================= */

/**
 * @brief Take a sample for all metrics
 */
static void take_sample(Agentite_RateTracker *tracker) {
    float timestamp = tracker->total_time;

    for (int i = 0; i < tracker->metric_count; i++) {
        Agentite_MetricTracker *m = &tracker->metrics[i];

        /* Create sample from accumulated values */
        Agentite_RateSample sample;
        sample.timestamp = timestamp;
        sample.produced = m->pending_produced;
        sample.consumed = m->pending_consumed;

        /* Write to circular buffer */
        m->samples[m->sample_head] = sample;
        m->sample_head = (m->sample_head + 1) % tracker->history_size;
        if (m->sample_count < tracker->history_size) {
            m->sample_count++;
        }

        /* Reset accumulators */
        m->pending_produced = 0;
        m->pending_consumed = 0;
    }
}

/**
 * @brief Get sample at index (0 = oldest)
 */
static Agentite_RateSample *get_sample(Agentite_MetricTracker *m, int index, int history_size) {
    if (index < 0 || index >= m->sample_count) return NULL;

    /* Calculate actual position in circular buffer */
    int start = (m->sample_head - m->sample_count + history_size) % history_size;
    int pos = (start + index) % history_size;
    return &m->samples[pos];
}

/**
 * @brief Find first sample within time window
 *
 * @return Index of first sample in window (oldest), or -1 if none
 */
static int find_window_start(Agentite_MetricTracker *m, int history_size, float current_time, float time_window) {
    if (m->sample_count == 0) return -1;
    if (time_window <= 0.0f) return 0;  /* All history */

    float cutoff = current_time - time_window;

    /* Binary search for first sample >= cutoff */
    int lo = 0;
    int hi = m->sample_count - 1;
    int result = m->sample_count;  /* Default: no samples in window */

    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        Agentite_RateSample *s = get_sample(m, mid, history_size);
        if (s && s->timestamp >= cutoff) {
            result = mid;
            hi = mid - 1;
        } else {
            lo = mid + 1;
        }
    }

    return result < m->sample_count ? result : -1;
}

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

Agentite_RateTracker *agentite_rate_create(int metric_count, float sample_interval, int history_size) {
    if (metric_count <= 0) metric_count = 1;
    if (metric_count > AGENTITE_RATE_MAX_METRICS) metric_count = AGENTITE_RATE_MAX_METRICS;
    if (history_size <= 0) history_size = 64;
    if (history_size > AGENTITE_RATE_MAX_SAMPLES) history_size = AGENTITE_RATE_MAX_SAMPLES;
    if (sample_interval < 0.01f) sample_interval = 0.01f;

    Agentite_RateTracker *tracker = AGENTITE_ALLOC(Agentite_RateTracker);
    if (!tracker) {
        agentite_set_error("Rate: Failed to allocate tracker");
        return NULL;
    }

    tracker->metrics = AGENTITE_ALLOC_ARRAY(Agentite_MetricTracker, metric_count);
    if (!tracker->metrics) {
        agentite_set_error("Rate: Failed to allocate metrics");
        free(tracker);
        return NULL;
    }

    /* Allocate sample buffers for each metric */
    for (int i = 0; i < metric_count; i++) {
        tracker->metrics[i].samples = AGENTITE_ALLOC_ARRAY(Agentite_RateSample, history_size);
        if (!tracker->metrics[i].samples) {
            agentite_set_error("Rate: Failed to allocate sample buffer");
            for (int j = 0; j < i; j++) {
                free(tracker->metrics[j].samples);
            }
            free(tracker->metrics);
            free(tracker);
            return NULL;
        }
        tracker->metrics[i].sample_head = 0;
        tracker->metrics[i].sample_count = 0;
    }

    tracker->metric_count = metric_count;
    tracker->sample_interval = sample_interval;
    tracker->history_size = history_size;
    tracker->time_accumulator = 0.0f;
    tracker->total_time = 0.0f;

    return tracker;
}

void agentite_rate_destroy(Agentite_RateTracker *tracker) {
    if (!tracker) return;

    if (tracker->metrics) {
        for (int i = 0; i < tracker->metric_count; i++) {
            free(tracker->metrics[i].samples);
        }
        free(tracker->metrics);
    }
    free(tracker);
}

void agentite_rate_reset(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR(tracker);

    for (int i = 0; i < tracker->metric_count; i++) {
        Agentite_MetricTracker *m = &tracker->metrics[i];
        m->pending_produced = 0;
        m->pending_consumed = 0;
        m->sample_head = 0;
        m->sample_count = 0;
        memset(m->samples, 0, tracker->history_size * sizeof(Agentite_RateSample));
    }

    tracker->time_accumulator = 0.0f;
    tracker->total_time = 0.0f;
}

/* ============================================================================
 * Metric Configuration
 * ========================================================================= */

void agentite_rate_set_name(Agentite_RateTracker *tracker, int metric_id, const char *name) {
    AGENTITE_VALIDATE_PTR(tracker);
    AGENTITE_VALIDATE_PTR(name);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return;

    strncpy(tracker->metrics[metric_id].name, name, sizeof(tracker->metrics[metric_id].name) - 1);
    tracker->metrics[metric_id].name[sizeof(tracker->metrics[metric_id].name) - 1] = '\0';
}

const char *agentite_rate_get_name(Agentite_RateTracker *tracker, int metric_id) {
    AGENTITE_VALIDATE_PTR_RET(tracker, "");
    if (metric_id < 0 || metric_id >= tracker->metric_count) return "";
    return tracker->metrics[metric_id].name;
}

int agentite_rate_get_metric_count(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0);
    return tracker->metric_count;
}

/* ============================================================================
 * Recording
 * ========================================================================= */

void agentite_rate_update(Agentite_RateTracker *tracker, float delta_time) {
    AGENTITE_VALIDATE_PTR(tracker);
    if (delta_time < 0.0f) delta_time = 0.0f;

    tracker->time_accumulator += delta_time;
    tracker->total_time += delta_time;

    /* Take samples at regular intervals */
    while (tracker->time_accumulator >= tracker->sample_interval) {
        take_sample(tracker);
        tracker->time_accumulator -= tracker->sample_interval;
    }
}

void agentite_rate_record_production(Agentite_RateTracker *tracker, int metric_id, int32_t amount) {
    AGENTITE_VALIDATE_PTR(tracker);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return;
    if (amount < 0) amount = 0;

    tracker->metrics[metric_id].pending_produced += amount;
}

void agentite_rate_record_consumption(Agentite_RateTracker *tracker, int metric_id, int32_t amount) {
    AGENTITE_VALIDATE_PTR(tracker);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return;
    if (amount < 0) amount = 0;

    tracker->metrics[metric_id].pending_consumed += amount;
}

void agentite_rate_record(Agentite_RateTracker *tracker, int metric_id,
                        int32_t produced, int32_t consumed) {
    AGENTITE_VALIDATE_PTR(tracker);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return;

    if (produced > 0) tracker->metrics[metric_id].pending_produced += produced;
    if (consumed > 0) tracker->metrics[metric_id].pending_consumed += consumed;
}

void agentite_rate_force_sample(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR(tracker);
    take_sample(tracker);
    tracker->time_accumulator = 0.0f;
}

/* ============================================================================
 * Rate Queries
 * ========================================================================= */

float agentite_rate_get_production_rate(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.production_rate;
}

float agentite_rate_get_consumption_rate(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.consumption_rate;
}

float agentite_rate_get_net_rate(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.net_rate;
}

Agentite_RateStats agentite_rate_get_stats(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = {0};

    AGENTITE_VALIDATE_PTR_RET(tracker, stats);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return stats;

    Agentite_MetricTracker *m = &tracker->metrics[metric_id];
    if (m->sample_count == 0) return stats;

    int start = find_window_start(m, tracker->history_size, tracker->total_time, time_window);
    if (start < 0) return stats;

    /* Initialize min/max */
    stats.min_production = INT_MAX;
    stats.max_production = INT_MIN;
    stats.min_consumption = INT_MAX;
    stats.max_consumption = INT_MIN;

    /* Calculate stats over window */
    float first_time = 0.0f;

    for (int i = start; i < m->sample_count; i++) {
        Agentite_RateSample *s = get_sample(m, i, tracker->history_size);
        if (!s) continue;

        if (stats.sample_count == 0) {
            first_time = s->timestamp;
        }

        stats.total_produced += s->produced;
        stats.total_consumed += s->consumed;

        if (s->produced < stats.min_production) stats.min_production = s->produced;
        if (s->produced > stats.max_production) stats.max_production = s->produced;
        if (s->consumed < stats.min_consumption) stats.min_consumption = s->consumed;
        if (s->consumed > stats.max_consumption) stats.max_consumption = s->consumed;

        stats.sample_count++;
    }

    /* Calculate derived values */
    stats.total_net = stats.total_produced - stats.total_consumed;

    /* Time window is from first sample to current time (including pending interval) */
    stats.time_window = tracker->total_time - first_time;
    if (stats.time_window < 0.001f) stats.time_window = tracker->sample_interval;

    stats.production_rate = (float)stats.total_produced / stats.time_window;
    stats.consumption_rate = (float)stats.total_consumed / stats.time_window;
    stats.net_rate = (float)stats.total_net / stats.time_window;

    /* Handle edge case of no samples */
    if (stats.sample_count == 0) {
        stats.min_production = 0;
        stats.max_production = 0;
        stats.min_consumption = 0;
        stats.max_consumption = 0;
    }

    return stats;
}

/* ============================================================================
 * Aggregate Queries
 * ========================================================================= */

int32_t agentite_rate_get_total_production(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.total_produced;
}

int32_t agentite_rate_get_total_consumption(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.total_consumed;
}

int32_t agentite_rate_get_min_production(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.min_production;
}

int32_t agentite_rate_get_max_production(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    return stats.max_production;
}

float agentite_rate_get_avg_production(Agentite_RateTracker *tracker, int metric_id, float time_window) {
    Agentite_RateStats stats = agentite_rate_get_stats(tracker, metric_id, time_window);
    if (stats.sample_count == 0) return 0.0f;
    return (float)stats.total_produced / (float)stats.sample_count;
}

/* ============================================================================
 * History Access
 * ========================================================================= */

int agentite_rate_get_history(Agentite_RateTracker *tracker, int metric_id, float time_window,
                            Agentite_RateSample *out_samples, int max_samples) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0);
    AGENTITE_VALIDATE_PTR_RET(out_samples, 0);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return 0;
    if (max_samples <= 0) return 0;

    Agentite_MetricTracker *m = &tracker->metrics[metric_id];
    if (m->sample_count == 0) return 0;

    int start = find_window_start(m, tracker->history_size, tracker->total_time, time_window);
    if (start < 0) return 0;

    int count = 0;
    for (int i = start; i < m->sample_count && count < max_samples; i++) {
        Agentite_RateSample *s = get_sample(m, i, tracker->history_size);
        if (s) {
            out_samples[count++] = *s;
        }
    }

    return count;
}

bool agentite_rate_get_latest_sample(Agentite_RateTracker *tracker, int metric_id,
                                   Agentite_RateSample *out_sample) {
    AGENTITE_VALIDATE_PTR_RET(tracker, false);
    AGENTITE_VALIDATE_PTR_RET(out_sample, false);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return false;

    Agentite_MetricTracker *m = &tracker->metrics[metric_id];
    if (m->sample_count == 0) return false;

    Agentite_RateSample *s = get_sample(m, m->sample_count - 1, tracker->history_size);
    if (s) {
        *out_sample = *s;
        return true;
    }
    return false;
}

int agentite_rate_get_sample_count(Agentite_RateTracker *tracker, int metric_id) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0);
    if (metric_id < 0 || metric_id >= tracker->metric_count) return 0;
    return tracker->metrics[metric_id].sample_count;
}

/* ============================================================================
 * Configuration Queries
 * ========================================================================= */

float agentite_rate_get_interval(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0.0f);
    return tracker->sample_interval;
}

int agentite_rate_get_history_size(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0);
    return tracker->history_size;
}

float agentite_rate_get_max_time_window(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0.0f);
    return tracker->sample_interval * (float)tracker->history_size;
}

float agentite_rate_get_current_time(Agentite_RateTracker *tracker) {
    AGENTITE_VALIDATE_PTR_RET(tracker, 0.0f);
    return tracker->total_time;
}
