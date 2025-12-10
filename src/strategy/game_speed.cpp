/**
 * @file game_speed.c
 * @brief Variable Game Speed System implementation
 */

#include "agentite/agentite.h"
#include "agentite/game_speed.h"
#include "agentite/validate.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structure
 * ========================================================================= */

struct Agentite_GameSpeed {
    /* Current state */
    float base_speed;           /**< Base speed multiplier (unaffected by pause) */
    float current_speed;        /**< Current effective speed (0 if paused) */
    float target_speed;         /**< Target speed for smooth transitions */
    bool paused;                /**< Pause state */

    /* Speed limits */
    float min_speed;            /**< Minimum speed */
    float max_speed;            /**< Maximum speed */

    /* Presets */
    float presets[AGENTITE_GAME_SPEED_MAX_PRESETS];
    int preset_count;
    int current_preset;         /**< Current preset index, -1 if not on preset */

    /* Smooth transitions */
    bool smooth_transitions;    /**< Enable smooth transitions */
    float transition_rate;      /**< How fast to interpolate */

    /* Callbacks */
    Agentite_GameSpeedCallback speed_callback;
    void *speed_callback_data;
    Agentite_GameSpeedPauseCallback pause_callback;
    void *pause_callback_data;

    /* Statistics */
    float total_scaled_time;    /**< Total game time */
    float total_real_time;      /**< Total real time */
    float total_paused_time;    /**< Total time spent paused */
};

/* ============================================================================
 * Helper Functions
 * ========================================================================= */

static float clamp_speed(float speed, float min_val, float max_val) {
    if (speed < min_val) return min_val;
    if (speed > max_val) return max_val;
    return speed;
}

static float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

static bool float_equal(float a, float b, float epsilon) {
    return fabsf(a - b) < epsilon;
}

static void notify_speed_change(Agentite_GameSpeed *speed, float old_speed, float new_speed) {
    if (speed->speed_callback && !float_equal(old_speed, new_speed, 0.001f)) {
        speed->speed_callback(speed, old_speed, new_speed, speed->speed_callback_data);
    }
}

static void notify_pause_change(Agentite_GameSpeed *speed, bool paused) {
    if (speed->pause_callback) {
        speed->pause_callback(speed, paused, speed->pause_callback_data);
    }
}

static int find_preset_index(Agentite_GameSpeed *speed, float value) {
    for (int i = 0; i < speed->preset_count; i++) {
        if (float_equal(speed->presets[i], value, 0.001f)) {
            return i;
        }
    }
    return -1;
}

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

Agentite_GameSpeed *agentite_game_speed_create(void) {
    return agentite_game_speed_create_ex(AGENTITE_GAME_SPEED_DEFAULT);
}

Agentite_GameSpeed *agentite_game_speed_create_ex(float initial_speed) {
    Agentite_GameSpeed *speed = AGENTITE_ALLOC(Agentite_GameSpeed);
    if (!speed) {
        agentite_set_error("Failed to allocate game speed system");
        return NULL;
    }

    /* Initialize limits first */
    speed->min_speed = AGENTITE_GAME_SPEED_MIN;
    speed->max_speed = AGENTITE_GAME_SPEED_MAX;

    /* Clamp initial speed */
    initial_speed = clamp_speed(initial_speed, speed->min_speed, speed->max_speed);

    /* Initialize state */
    speed->base_speed = initial_speed;
    speed->current_speed = initial_speed;
    speed->target_speed = initial_speed;
    speed->paused = false;

    /* Default presets (1x, 2x, 4x) */
    speed->presets[0] = 1.0f;
    speed->presets[1] = 2.0f;
    speed->presets[2] = 4.0f;
    speed->preset_count = 3;
    speed->current_preset = find_preset_index(speed, initial_speed);

    /* Smooth transitions disabled by default */
    speed->smooth_transitions = false;
    speed->transition_rate = 5.0f;

    /* Initialize statistics */
    speed->total_scaled_time = 0.0f;
    speed->total_real_time = 0.0f;
    speed->total_paused_time = 0.0f;

    return speed;
}

void agentite_game_speed_destroy(Agentite_GameSpeed *speed) {
    if (speed) {
        free(speed);
    }
}

/* ============================================================================
 * Speed Control
 * ========================================================================= */

void agentite_game_speed_set(Agentite_GameSpeed *speed, float multiplier) {
    AGENTITE_VALIDATE_PTR(speed);

    float old_speed = speed->base_speed;
    multiplier = clamp_speed(multiplier, speed->min_speed, speed->max_speed);

    speed->base_speed = multiplier;
    speed->target_speed = multiplier;
    speed->current_preset = find_preset_index(speed, multiplier);

    if (!speed->smooth_transitions) {
        speed->current_speed = multiplier;
    }

    if (!speed->paused) {
        notify_speed_change(speed, old_speed, multiplier);
    }
}

float agentite_game_speed_get(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0.0f);

    if (speed->paused) {
        return AGENTITE_GAME_SPEED_PAUSED;
    }
    return speed->current_speed;
}

float agentite_game_speed_get_base(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, AGENTITE_GAME_SPEED_DEFAULT);
    return speed->base_speed;
}

void agentite_game_speed_multiply(Agentite_GameSpeed *speed, float factor) {
    AGENTITE_VALIDATE_PTR(speed);
    if (factor <= 0.0f) return;

    agentite_game_speed_set(speed, speed->base_speed * factor);
}

void agentite_game_speed_divide(Agentite_GameSpeed *speed, float divisor) {
    AGENTITE_VALIDATE_PTR(speed);
    if (divisor <= 0.0f) return;

    agentite_game_speed_set(speed, speed->base_speed / divisor);
}

void agentite_game_speed_reset(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);
    agentite_game_speed_set(speed, AGENTITE_GAME_SPEED_DEFAULT);
}

/* ============================================================================
 * Pause Control
 * ========================================================================= */

void agentite_game_speed_pause(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);

    if (!speed->paused) {
        speed->paused = true;
        notify_pause_change(speed, true);
    }
}

void agentite_game_speed_resume(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);

    if (speed->paused) {
        speed->paused = false;
        notify_pause_change(speed, false);
    }
}

void agentite_game_speed_toggle_pause(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);

    if (speed->paused) {
        agentite_game_speed_resume(speed);
    } else {
        agentite_game_speed_pause(speed);
    }
}

bool agentite_game_speed_is_paused(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, true);
    return speed->paused;
}

/* ============================================================================
 * Delta Time Scaling
 * ========================================================================= */

float agentite_game_speed_scale_delta(Agentite_GameSpeed *speed, float raw_delta) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0.0f);

    /* Update statistics */
    speed->total_real_time += raw_delta;

    if (speed->paused) {
        speed->total_paused_time += raw_delta;
        return 0.0f;
    }

    float scaled = raw_delta * speed->current_speed;
    speed->total_scaled_time += scaled;
    return scaled;
}

void agentite_game_speed_update(Agentite_GameSpeed *speed, float delta_time) {
    AGENTITE_VALIDATE_PTR(speed);

    /* Update smooth transitions */
    if (speed->smooth_transitions && !float_equal(speed->current_speed, speed->target_speed, 0.001f)) {
        float old_speed = speed->current_speed;
        float t = 1.0f - expf(-speed->transition_rate * delta_time);
        speed->current_speed = lerp(speed->current_speed, speed->target_speed, t);

        /* Snap to target if close enough */
        if (float_equal(speed->current_speed, speed->target_speed, 0.001f)) {
            speed->current_speed = speed->target_speed;
        }

        if (!speed->paused) {
            notify_speed_change(speed, old_speed, speed->current_speed);
        }
    }
}

/* ============================================================================
 * Speed Presets
 * ========================================================================= */

void agentite_game_speed_set_presets(Agentite_GameSpeed *speed, const float *presets, int count) {
    AGENTITE_VALIDATE_PTR(speed);
    AGENTITE_VALIDATE_PTR(presets);

    if (count < 1) count = 1;
    if (count > AGENTITE_GAME_SPEED_MAX_PRESETS) count = AGENTITE_GAME_SPEED_MAX_PRESETS;

    for (int i = 0; i < count; i++) {
        speed->presets[i] = clamp_speed(presets[i], speed->min_speed, speed->max_speed);
    }
    speed->preset_count = count;
    speed->current_preset = find_preset_index(speed, speed->base_speed);
}

void agentite_game_speed_set_default_presets(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);

    float defaults[] = { 1.0f, 2.0f, 4.0f };
    agentite_game_speed_set_presets(speed, defaults, 3);
}

void agentite_game_speed_cycle(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);
    if (speed->preset_count == 0) return;

    int next = (speed->current_preset + 1) % speed->preset_count;
    if (speed->current_preset < 0) {
        /* Find nearest preset or start at 0 */
        next = 0;
        for (int i = 0; i < speed->preset_count; i++) {
            if (speed->presets[i] > speed->base_speed) {
                next = i;
                break;
            }
        }
    }

    agentite_game_speed_set(speed, speed->presets[next]);
    speed->current_preset = next;
}

void agentite_game_speed_cycle_reverse(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);
    if (speed->preset_count == 0) return;

    int prev = speed->current_preset - 1;
    if (prev < 0) {
        prev = speed->preset_count - 1;
    }
    if (speed->current_preset < 0) {
        /* Find nearest preset going backward or start at last */
        prev = speed->preset_count - 1;
        for (int i = speed->preset_count - 1; i >= 0; i--) {
            if (speed->presets[i] < speed->base_speed) {
                prev = i;
                break;
            }
        }
    }

    agentite_game_speed_set(speed, speed->presets[prev]);
    speed->current_preset = prev;
}

bool agentite_game_speed_set_preset(Agentite_GameSpeed *speed, int index) {
    AGENTITE_VALIDATE_PTR_RET(speed, false);

    if (index < 0 || index >= speed->preset_count) {
        return false;
    }

    agentite_game_speed_set(speed, speed->presets[index]);
    speed->current_preset = index;
    return true;
}

int agentite_game_speed_get_preset_index(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, -1);
    return speed->current_preset;
}

int agentite_game_speed_get_preset_count(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0);
    return speed->preset_count;
}

float agentite_game_speed_get_preset(Agentite_GameSpeed *speed, int index) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0.0f);

    if (index < 0 || index >= speed->preset_count) {
        return 0.0f;
    }

    return speed->presets[index];
}

/* ============================================================================
 * Smooth Transitions
 * ========================================================================= */

void agentite_game_speed_set_smooth_transitions(Agentite_GameSpeed *speed, bool enabled) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->smooth_transitions = enabled;

    if (!enabled) {
        /* Complete any pending transition immediately */
        speed->current_speed = speed->target_speed;
    }
}

bool agentite_game_speed_get_smooth_transitions(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, false);
    return speed->smooth_transitions;
}

void agentite_game_speed_set_transition_rate(Agentite_GameSpeed *speed, float rate) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->transition_rate = rate > 0.0f ? rate : 5.0f;
}

bool agentite_game_speed_is_transitioning(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, false);
    return speed->smooth_transitions && !float_equal(speed->current_speed, speed->target_speed, 0.001f);
}

void agentite_game_speed_complete_transition(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);

    if (!float_equal(speed->current_speed, speed->target_speed, 0.001f)) {
        float old_speed = speed->current_speed;
        speed->current_speed = speed->target_speed;

        if (!speed->paused) {
            notify_speed_change(speed, old_speed, speed->current_speed);
        }
    }
}

/* ============================================================================
 * Speed Limits
 * ========================================================================= */

void agentite_game_speed_set_min(Agentite_GameSpeed *speed, float min_speed) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->min_speed = min_speed > 0.0f ? min_speed : AGENTITE_GAME_SPEED_MIN;

    if (speed->min_speed > speed->max_speed) {
        speed->min_speed = speed->max_speed;
    }

    /* Re-clamp current speeds */
    speed->base_speed = clamp_speed(speed->base_speed, speed->min_speed, speed->max_speed);
    speed->current_speed = clamp_speed(speed->current_speed, speed->min_speed, speed->max_speed);
    speed->target_speed = clamp_speed(speed->target_speed, speed->min_speed, speed->max_speed);
}

void agentite_game_speed_set_max(Agentite_GameSpeed *speed, float max_speed) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->max_speed = max_speed > 0.0f ? max_speed : AGENTITE_GAME_SPEED_MAX;

    if (speed->max_speed < speed->min_speed) {
        speed->max_speed = speed->min_speed;
    }

    /* Re-clamp current speeds */
    speed->base_speed = clamp_speed(speed->base_speed, speed->min_speed, speed->max_speed);
    speed->current_speed = clamp_speed(speed->current_speed, speed->min_speed, speed->max_speed);
    speed->target_speed = clamp_speed(speed->target_speed, speed->min_speed, speed->max_speed);
}

float agentite_game_speed_get_min(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, AGENTITE_GAME_SPEED_MIN);
    return speed->min_speed;
}

float agentite_game_speed_get_max(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, AGENTITE_GAME_SPEED_MAX);
    return speed->max_speed;
}

/* ============================================================================
 * Callbacks
 * ========================================================================= */

void agentite_game_speed_set_callback(Agentite_GameSpeed *speed,
                                     Agentite_GameSpeedCallback callback,
                                     void *userdata) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->speed_callback = callback;
    speed->speed_callback_data = userdata;
}

void agentite_game_speed_set_pause_callback(Agentite_GameSpeed *speed,
                                           Agentite_GameSpeedPauseCallback callback,
                                           void *userdata) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->pause_callback = callback;
    speed->pause_callback_data = userdata;
}

/* ============================================================================
 * Statistics
 * ========================================================================= */

float agentite_game_speed_get_total_scaled_time(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0.0f);
    return speed->total_scaled_time;
}

float agentite_game_speed_get_total_real_time(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0.0f);
    return speed->total_real_time;
}

float agentite_game_speed_get_total_paused_time(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0.0f);
    return speed->total_paused_time;
}

void agentite_game_speed_reset_stats(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR(speed);
    speed->total_scaled_time = 0.0f;
    speed->total_real_time = 0.0f;
    speed->total_paused_time = 0.0f;
}

/* ============================================================================
 * Utility Functions
 * ========================================================================= */

int agentite_game_speed_to_string(Agentite_GameSpeed *speed, char *buffer, int buffer_size) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0);
    AGENTITE_VALIDATE_PTR_RET(buffer, 0);

    if (buffer_size < 1) return 0;

    if (speed->paused) {
        return snprintf(buffer, buffer_size, "Paused");
    }

    float s = speed->current_speed;

    /* Check for clean multipliers */
    if (float_equal(s, 0.25f, 0.001f)) {
        return snprintf(buffer, buffer_size, "0.25x");
    } else if (float_equal(s, 0.5f, 0.001f)) {
        return snprintf(buffer, buffer_size, "0.5x");
    } else if (float_equal(s, 1.0f, 0.001f)) {
        return snprintf(buffer, buffer_size, "1x");
    } else if (float_equal(s, 2.0f, 0.001f)) {
        return snprintf(buffer, buffer_size, "2x");
    } else if (float_equal(s, 4.0f, 0.001f)) {
        return snprintf(buffer, buffer_size, "4x");
    } else if (float_equal(s, 8.0f, 0.001f)) {
        return snprintf(buffer, buffer_size, "8x");
    } else if (float_equal(s, 16.0f, 0.001f)) {
        return snprintf(buffer, buffer_size, "16x");
    }

    /* Generic format */
    if (s < 1.0f) {
        return snprintf(buffer, buffer_size, "%.2fx", s);
    } else {
        return snprintf(buffer, buffer_size, "%.1fx", s);
    }
}

int agentite_game_speed_get_percent(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, 0);

    if (speed->paused) {
        return 0;
    }

    return (int)(speed->current_speed * 100.0f + 0.5f);
}

bool agentite_game_speed_is_at_min(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, false);
    return float_equal(speed->base_speed, speed->min_speed, 0.001f);
}

bool agentite_game_speed_is_at_max(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, false);
    return float_equal(speed->base_speed, speed->max_speed, 0.001f);
}

bool agentite_game_speed_is_normal(Agentite_GameSpeed *speed) {
    AGENTITE_VALIDATE_PTR_RET(speed, false);
    return float_equal(speed->base_speed, AGENTITE_GAME_SPEED_DEFAULT, 0.001f);
}
