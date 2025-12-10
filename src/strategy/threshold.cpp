#include "agentite/threshold.h"
#include <string.h>

void agentite_threshold_init(Agentite_ThresholdTracker *tracker, float initial_value) {
    if (!tracker) return;

    memset(tracker, 0, sizeof(Agentite_ThresholdTracker));
    tracker->current_value = initial_value;
}

int agentite_threshold_add(Agentite_ThresholdTracker *tracker,
                          float boundary,
                          Agentite_ThresholdCallback callback,
                          void *userdata) {
    if (!tracker || !callback) return -1;

    // Find first inactive slot
    for (int i = 0; i < AGENTITE_THRESHOLD_MAX; i++) {
        if (!tracker->thresholds[i].active) {
            Agentite_Threshold *t = &tracker->thresholds[i];
            t->boundary = boundary;
            t->callback = callback;
            t->userdata = userdata;
            t->was_above = tracker->current_value > boundary;
            t->active = true;
            tracker->count++;
            return i;
        }
    }

    return -1;  // No free slots
}

void agentite_threshold_remove(Agentite_ThresholdTracker *tracker, int threshold_id) {
    if (!tracker || threshold_id < 0 || threshold_id >= AGENTITE_THRESHOLD_MAX) return;

    if (tracker->thresholds[threshold_id].active) {
        tracker->thresholds[threshold_id].active = false;
        tracker->count--;
    }
}

void agentite_threshold_update(Agentite_ThresholdTracker *tracker, float new_value) {
    if (!tracker) return;

    float old_value = tracker->current_value;
    tracker->current_value = new_value;

    for (int i = 0; i < AGENTITE_THRESHOLD_MAX; i++) {
        Agentite_Threshold *t = &tracker->thresholds[i];
        if (!t->active) continue;

        bool is_above = new_value > t->boundary;

        // Check if threshold was crossed
        if (is_above != t->was_above) {
            // Fire callback
            t->callback(i, old_value, new_value, is_above, t->userdata);
            t->was_above = is_above;
        }
    }
}

float agentite_threshold_get_value(const Agentite_ThresholdTracker *tracker) {
    if (!tracker) return 0.0f;
    return tracker->current_value;
}

int agentite_threshold_count(const Agentite_ThresholdTracker *tracker) {
    if (!tracker) return 0;
    return tracker->count;
}
