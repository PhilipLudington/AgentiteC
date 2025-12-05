#ifndef CARBON_THRESHOLD_H
#define CARBON_THRESHOLD_H

#include <stdbool.h>

#define CARBON_THRESHOLD_MAX 16

// Callback when threshold is crossed
typedef void (*Carbon_ThresholdCallback)(int threshold_id,
                                          float old_value,
                                          float new_value,
                                          bool crossed_above,  // true = went above, false = went below
                                          void *userdata);

// Single threshold
typedef struct Carbon_Threshold {
    float boundary;
    Carbon_ThresholdCallback callback;
    void *userdata;
    bool was_above;         // Previous state
    bool active;            // Slot in use
} Carbon_Threshold;

// Tracker for multiple thresholds on one value
typedef struct Carbon_ThresholdTracker {
    Carbon_Threshold thresholds[CARBON_THRESHOLD_MAX];
    int count;
    float current_value;
} Carbon_ThresholdTracker;

// Initialize
void carbon_threshold_init(Carbon_ThresholdTracker *tracker, float initial_value);

// Add threshold, returns ID (slot index) or -1 if full
int carbon_threshold_add(Carbon_ThresholdTracker *tracker,
                          float boundary,
                          Carbon_ThresholdCallback callback,
                          void *userdata);

// Remove threshold by ID
void carbon_threshold_remove(Carbon_ThresholdTracker *tracker, int threshold_id);

// Update value and fire callbacks if thresholds crossed
void carbon_threshold_update(Carbon_ThresholdTracker *tracker, float new_value);

// Query
float carbon_threshold_get_value(const Carbon_ThresholdTracker *tracker);
int carbon_threshold_count(const Carbon_ThresholdTracker *tracker);

#endif // CARBON_THRESHOLD_H
