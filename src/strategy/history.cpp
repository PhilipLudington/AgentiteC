#include "agentite/agentite.h"
#include "agentite/history.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

struct Agentite_History {
    // Snapshots as circular buffer
    Agentite_MetricSnapshot snapshots[AGENTITE_HISTORY_MAX_SNAPSHOTS];
    int snapshot_head;      // Next write position
    int snapshot_count;     // Total stored (max = AGENTITE_HISTORY_MAX_SNAPSHOTS)

    // Events as circular buffer
    Agentite_HistoryEvent events[AGENTITE_HISTORY_MAX_EVENTS];
    int event_head;
    int event_count;

    // Metric names for debugging
    char metric_names[AGENTITE_HISTORY_MAX_METRICS][32];
};

Agentite_History *agentite_history_create(void) {
    Agentite_History *h = AGENTITE_ALLOC(Agentite_History);
    if (!h) return NULL;

    // Initialize metric names
    for (int i = 0; i < AGENTITE_HISTORY_MAX_METRICS; i++) {
        snprintf(h->metric_names[i], sizeof(h->metric_names[i]), "Metric %d", i);
    }

    return h;
}

void agentite_history_destroy(Agentite_History *h) {
    free(h);
}

void agentite_history_set_metric_name(Agentite_History *h, int index, const char *name) {
    if (!h || index < 0 || index >= AGENTITE_HISTORY_MAX_METRICS || !name) return;

    strncpy(h->metric_names[index], name, sizeof(h->metric_names[index]) - 1);
    h->metric_names[index][sizeof(h->metric_names[index]) - 1] = '\0';
}

const char *agentite_history_get_metric_name(const Agentite_History *h, int index) {
    if (!h || index < 0 || index >= AGENTITE_HISTORY_MAX_METRICS) return "Unknown";
    return h->metric_names[index];
}

void agentite_history_add_snapshot(Agentite_History *h, const Agentite_MetricSnapshot *snap) {
    if (!h || !snap) return;

    h->snapshots[h->snapshot_head] = *snap;
    h->snapshot_head = (h->snapshot_head + 1) % AGENTITE_HISTORY_MAX_SNAPSHOTS;

    if (h->snapshot_count < AGENTITE_HISTORY_MAX_SNAPSHOTS) {
        h->snapshot_count++;
    }
}

void agentite_history_add_event(Agentite_History *h, const Agentite_HistoryEvent *event) {
    if (!h || !event) return;

    h->events[h->event_head] = *event;
    h->event_head = (h->event_head + 1) % AGENTITE_HISTORY_MAX_EVENTS;

    if (h->event_count < AGENTITE_HISTORY_MAX_EVENTS) {
        h->event_count++;
    }
}

void agentite_history_add_event_ex(Agentite_History *h, int turn, int type,
                                  const char *title, const char *description,
                                  float value_before, float value_after) {
    if (!h) return;

    Agentite_HistoryEvent event = {0};
    event.turn = turn;
    event.type = type;
    event.value_before = value_before;
    event.value_after = value_after;

    if (title) {
        strncpy(event.title, title, sizeof(event.title) - 1);
    }
    if (description) {
        strncpy(event.description, description, sizeof(event.description) - 1);
    }

    agentite_history_add_event(h, &event);
}

int agentite_history_snapshot_count(const Agentite_History *h) {
    if (!h) return 0;
    return h->snapshot_count;
}

const Agentite_MetricSnapshot *agentite_history_get_snapshot(const Agentite_History *h, int index) {
    if (!h || index < 0 || index >= h->snapshot_count) return NULL;

    // Convert logical index to physical index in circular buffer
    // Index 0 = oldest, index (count-1) = newest
    int oldest = (h->snapshot_head - h->snapshot_count + AGENTITE_HISTORY_MAX_SNAPSHOTS)
                 % AGENTITE_HISTORY_MAX_SNAPSHOTS;
    int physical = (oldest + index) % AGENTITE_HISTORY_MAX_SNAPSHOTS;

    return &h->snapshots[physical];
}

const Agentite_MetricSnapshot *agentite_history_get_latest_snapshot(const Agentite_History *h) {
    if (!h || h->snapshot_count == 0) return NULL;
    return agentite_history_get_snapshot(h, h->snapshot_count - 1);
}

int agentite_history_event_count(const Agentite_History *h) {
    if (!h) return 0;
    return h->event_count;
}

const Agentite_HistoryEvent *agentite_history_get_event(const Agentite_History *h, int index) {
    if (!h || index < 0 || index >= h->event_count) return NULL;

    // Convert logical index to physical index in circular buffer
    int oldest = (h->event_head - h->event_count + AGENTITE_HISTORY_MAX_EVENTS)
                 % AGENTITE_HISTORY_MAX_EVENTS;
    int physical = (oldest + index) % AGENTITE_HISTORY_MAX_EVENTS;

    return &h->events[physical];
}

Agentite_GraphData agentite_history_get_graph(const Agentite_History *h, int metric_index) {
    Agentite_GraphData data = {0};

    if (!h || metric_index < 0 || metric_index >= AGENTITE_HISTORY_MAX_METRICS) {
        return data;
    }

    if (h->snapshot_count == 0) {
        return data;
    }

    // Allocate values array
    data.values = (float*)malloc(sizeof(float) * h->snapshot_count);
    if (!data.values) return data;

    data.count = h->snapshot_count;
    data.min_value = FLT_MAX;
    data.max_value = -FLT_MAX;

    // Extract metric values in chronological order
    for (int i = 0; i < h->snapshot_count; i++) {
        const Agentite_MetricSnapshot *snap = agentite_history_get_snapshot(h, i);
        float val = snap->values[metric_index];
        data.values[i] = val;

        if (val < data.min_value) data.min_value = val;
        if (val > data.max_value) data.max_value = val;
    }

    return data;
}

void agentite_graph_data_free(Agentite_GraphData *data) {
    if (!data) return;
    free(data->values);
    data->values = NULL;
    data->count = 0;
}

void agentite_history_clear(Agentite_History *h) {
    if (!h) return;

    h->snapshot_head = 0;
    h->snapshot_count = 0;
    h->event_head = 0;
    h->event_count = 0;
}
