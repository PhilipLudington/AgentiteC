#ifndef CARBON_HISTORY_H
#define CARBON_HISTORY_H

#include <stdbool.h>
#include <stdint.h>

#define CARBON_HISTORY_MAX_SNAPSHOTS 100
#define CARBON_HISTORY_MAX_EVENTS 50
#define CARBON_HISTORY_MAX_METRICS 16

// Metric snapshot (one per turn)
typedef struct Carbon_MetricSnapshot {
    int turn;
    float values[CARBON_HISTORY_MAX_METRICS];
} Carbon_MetricSnapshot;

// Significant event (game-defined types)
typedef struct Carbon_HistoryEvent {
    int turn;
    int type;               // Game-defined enum
    char title[64];
    char description[256];
    float value_before;
    float value_after;
} Carbon_HistoryEvent;

// Graph data for rendering
typedef struct Carbon_GraphData {
    float *values;
    int count;
    float min_value;
    float max_value;
} Carbon_GraphData;

// History tracker
typedef struct Carbon_History Carbon_History;

Carbon_History *carbon_history_create(void);
void carbon_history_destroy(Carbon_History *h);

// Register metric names (for debugging)
void carbon_history_set_metric_name(Carbon_History *h, int index, const char *name);
const char *carbon_history_get_metric_name(const Carbon_History *h, int index);

// Record a snapshot (circular buffer, keeps last N)
void carbon_history_add_snapshot(Carbon_History *h, const Carbon_MetricSnapshot *snap);

// Record a significant event
void carbon_history_add_event(Carbon_History *h, const Carbon_HistoryEvent *event);

// Convenience: add event with params
void carbon_history_add_event_ex(Carbon_History *h, int turn, int type,
                                  const char *title, const char *description,
                                  float value_before, float value_after);

// Query snapshots
int carbon_history_snapshot_count(const Carbon_History *h);
const Carbon_MetricSnapshot *carbon_history_get_snapshot(const Carbon_History *h, int index);
const Carbon_MetricSnapshot *carbon_history_get_latest_snapshot(const Carbon_History *h);

// Query events
int carbon_history_event_count(const Carbon_History *h);
const Carbon_HistoryEvent *carbon_history_get_event(const Carbon_History *h, int index);

// Get graph data for a metric (for UI graphing)
// Caller must free returned values array with carbon_graph_data_free
Carbon_GraphData carbon_history_get_graph(const Carbon_History *h, int metric_index);
void carbon_graph_data_free(Carbon_GraphData *data);

// Clear all history
void carbon_history_clear(Carbon_History *h);

#endif // CARBON_HISTORY_H
