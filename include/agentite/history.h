#ifndef AGENTITE_HISTORY_H
#define AGENTITE_HISTORY_H

#include <stdbool.h>
#include <stdint.h>

#define AGENTITE_HISTORY_MAX_SNAPSHOTS 100
#define AGENTITE_HISTORY_MAX_EVENTS 50
#define AGENTITE_HISTORY_MAX_METRICS 16

// Metric snapshot (one per turn)
typedef struct Agentite_MetricSnapshot {
    int turn;
    float values[AGENTITE_HISTORY_MAX_METRICS];
} Agentite_MetricSnapshot;

// Significant event (game-defined types)
typedef struct Agentite_HistoryEvent {
    int turn;
    int type;               // Game-defined enum
    char title[64];
    char description[256];
    float value_before;
    float value_after;
} Agentite_HistoryEvent;

// Graph data for rendering
typedef struct Agentite_GraphData {
    float *values;
    int count;
    float min_value;
    float max_value;
} Agentite_GraphData;

// History tracker
typedef struct Agentite_History Agentite_History;

Agentite_History *agentite_history_create(void);
void agentite_history_destroy(Agentite_History *h);

// Register metric names (for debugging)
void agentite_history_set_metric_name(Agentite_History *h, int index, const char *name);
const char *agentite_history_get_metric_name(const Agentite_History *h, int index);

// Record a snapshot (circular buffer, keeps last N)
void agentite_history_add_snapshot(Agentite_History *h, const Agentite_MetricSnapshot *snap);

// Record a significant event
void agentite_history_add_event(Agentite_History *h, const Agentite_HistoryEvent *event);

// Convenience: add event with params
void agentite_history_add_event_ex(Agentite_History *h, int turn, int type,
                                  const char *title, const char *description,
                                  float value_before, float value_after);

// Query snapshots
int agentite_history_snapshot_count(const Agentite_History *h);
const Agentite_MetricSnapshot *agentite_history_get_snapshot(const Agentite_History *h, int index);
const Agentite_MetricSnapshot *agentite_history_get_latest_snapshot(const Agentite_History *h);

// Query events
int agentite_history_event_count(const Agentite_History *h);
const Agentite_HistoryEvent *agentite_history_get_event(const Agentite_History *h, int index);

// Get graph data for a metric (for UI graphing)
// Caller must free returned values array with agentite_graph_data_free
Agentite_GraphData agentite_history_get_graph(const Agentite_History *h, int metric_index);
void agentite_graph_data_free(Agentite_GraphData *data);

// Clear all history
void agentite_history_clear(Agentite_History *h);

#endif // AGENTITE_HISTORY_H
