#ifndef AGENTITE_THRESHOLD_H
#define AGENTITE_THRESHOLD_H

#include <stdbool.h>

#define AGENTITE_THRESHOLD_MAX 16

// Callback when threshold is crossed
typedef void (*Agentite_ThresholdCallback)(int threshold_id,
                                          float old_value,
                                          float new_value,
                                          bool crossed_above,  // true = went above, false = went below
                                          void *userdata);

// Single threshold
typedef struct Agentite_Threshold {
    float boundary;
    Agentite_ThresholdCallback callback;
    void *userdata;
    bool was_above;         // Previous state
    bool active;            // Slot in use
} Agentite_Threshold;

// Tracker for multiple thresholds on one value
typedef struct Agentite_ThresholdTracker {
    Agentite_Threshold thresholds[AGENTITE_THRESHOLD_MAX];
    int count;
    float current_value;
} Agentite_ThresholdTracker;

// Initialize
void agentite_threshold_init(Agentite_ThresholdTracker *tracker, float initial_value);

// Add threshold, returns ID (slot index) or -1 if full
int agentite_threshold_add(Agentite_ThresholdTracker *tracker,
                          float boundary,
                          Agentite_ThresholdCallback callback,
                          void *userdata);

// Remove threshold by ID
void agentite_threshold_remove(Agentite_ThresholdTracker *tracker, int threshold_id);

// Update value and fire callbacks if thresholds crossed
void agentite_threshold_update(Agentite_ThresholdTracker *tracker, float new_value);

// Query
float agentite_threshold_get_value(const Agentite_ThresholdTracker *tracker);
int agentite_threshold_count(const Agentite_ThresholdTracker *tracker);

#endif // AGENTITE_THRESHOLD_H
