/*
 * Carbon UI - Tween/Animation System
 *
 * Provides property-based animations with easing for UI elements.
 *
 * Usage:
 *   CUI_TweenManager *tm = cui_tween_manager_create();
 *
 *   // Animate a node's opacity
 *   uint32_t id = cui_tween_property(tm, node, CUI_TWEEN_OPACITY, 0.0f, 1.0f,
 *                                    0.3f, CUI_EASE_OUT_QUAD);
 *
 *   // Convenience functions
 *   cui_tween_fade_in(tm, node, 0.3f);
 *   cui_tween_slide_in(tm, node, CUI_DIR_LEFT, 0.5f);
 *
 *   // Each frame
 *   cui_tween_manager_update(tm, delta_time);
 *
 *   cui_tween_manager_destroy(tm);
 */

#ifndef CARBON_UI_TWEEN_H
#define CARBON_UI_TWEEN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct CUI_Node CUI_Node;
typedef struct CUI_TweenManager CUI_TweenManager;

/* ============================================================================
 * Easing Types
 * ============================================================================ */

typedef enum CUI_EaseType {
    /* Linear */
    CUI_EASE_LINEAR,

    /* Sine */
    CUI_EASE_IN_SINE,
    CUI_EASE_OUT_SINE,
    CUI_EASE_IN_OUT_SINE,

    /* Quadratic */
    CUI_EASE_IN_QUAD,
    CUI_EASE_OUT_QUAD,
    CUI_EASE_IN_OUT_QUAD,

    /* Cubic */
    CUI_EASE_IN_CUBIC,
    CUI_EASE_OUT_CUBIC,
    CUI_EASE_IN_OUT_CUBIC,

    /* Quartic */
    CUI_EASE_IN_QUART,
    CUI_EASE_OUT_QUART,
    CUI_EASE_IN_OUT_QUART,

    /* Quintic */
    CUI_EASE_IN_QUINT,
    CUI_EASE_OUT_QUINT,
    CUI_EASE_IN_OUT_QUINT,

    /* Exponential */
    CUI_EASE_IN_EXPO,
    CUI_EASE_OUT_EXPO,
    CUI_EASE_IN_OUT_EXPO,

    /* Circular */
    CUI_EASE_IN_CIRC,
    CUI_EASE_OUT_CIRC,
    CUI_EASE_IN_OUT_CIRC,

    /* Back (overshoot) */
    CUI_EASE_IN_BACK,
    CUI_EASE_OUT_BACK,
    CUI_EASE_IN_OUT_BACK,

    /* Elastic */
    CUI_EASE_IN_ELASTIC,
    CUI_EASE_OUT_ELASTIC,
    CUI_EASE_IN_OUT_ELASTIC,

    /* Bounce */
    CUI_EASE_IN_BOUNCE,
    CUI_EASE_OUT_BOUNCE,
    CUI_EASE_IN_OUT_BOUNCE,

    CUI_EASE_COUNT
} CUI_EaseType;

/* ============================================================================
 * Tween Property Types
 * ============================================================================ */

typedef enum CUI_TweenProperty {
    /* Position */
    CUI_TWEEN_POSITION_X,
    CUI_TWEEN_POSITION_Y,

    /* Size */
    CUI_TWEEN_SIZE_X,
    CUI_TWEEN_SIZE_Y,

    /* Anchor offsets (for retained-mode nodes) */
    CUI_TWEEN_OFFSET_LEFT,
    CUI_TWEEN_OFFSET_TOP,
    CUI_TWEEN_OFFSET_RIGHT,
    CUI_TWEEN_OFFSET_BOTTOM,

    /* Visual */
    CUI_TWEEN_OPACITY,
    CUI_TWEEN_ROTATION,
    CUI_TWEEN_SCALE_X,
    CUI_TWEEN_SCALE_Y,

    /* Color channels (0-255 values internally, 0-1 for API) */
    CUI_TWEEN_COLOR_R,
    CUI_TWEEN_COLOR_G,
    CUI_TWEEN_COLOR_B,
    CUI_TWEEN_COLOR_A,

    /* Scroll position (for scroll containers) */
    CUI_TWEEN_SCROLL_X,
    CUI_TWEEN_SCROLL_Y,

    /* Custom property (uses callback) */
    CUI_TWEEN_CUSTOM,

    CUI_TWEEN_PROPERTY_COUNT
} CUI_TweenProperty;

/* ============================================================================
 * Direction (for slide animations)
 * ============================================================================ */

typedef enum CUI_Direction {
    CUI_DIR_LEFT,
    CUI_DIR_RIGHT,
    CUI_DIR_UP,
    CUI_DIR_DOWN
} CUI_Direction;

/* ============================================================================
 * Tween State
 * ============================================================================ */

typedef enum CUI_TweenState {
    CUI_TWEEN_IDLE,
    CUI_TWEEN_RUNNING,
    CUI_TWEEN_PAUSED,
    CUI_TWEEN_FINISHED
} CUI_TweenState;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/* Called when a tween completes */
typedef void (*CUI_TweenCallback)(uint32_t tween_id, void *userdata);

/* Called to set a custom property value */
typedef void (*CUI_TweenSetter)(CUI_Node *node, float value, void *userdata);

/* Called to get a custom property value */
typedef float (*CUI_TweenGetter)(CUI_Node *node, void *userdata);

/* ============================================================================
 * Tween Configuration
 * ============================================================================ */

typedef struct CUI_TweenConfig {
    CUI_Node *target;           /* Target node (can be NULL for value-only tweens) */
    CUI_TweenProperty property;
    float start_value;
    float end_value;
    float duration;             /* In seconds */
    float delay;                /* Delay before starting (seconds) */
    CUI_EaseType ease;

    /* Callbacks */
    CUI_TweenCallback on_complete;
    void *callback_userdata;

    /* For custom properties */
    CUI_TweenSetter custom_setter;
    CUI_TweenGetter custom_getter;
    void *custom_userdata;

    /* Options */
    bool auto_reverse;          /* Ping-pong animation */
    int repeat_count;           /* 0 = once, -1 = infinite */
    bool relative;              /* Add to current value instead of set */
} CUI_TweenConfig;

/* ============================================================================
 * Property Tween (internal structure, exposed for inspection)
 * ============================================================================ */

typedef struct CUI_PropertyTween {
    uint32_t id;
    CUI_TweenConfig config;
    CUI_TweenState state;
    float elapsed;
    float current_value;
    int current_repeat;
    bool reversing;             /* Currently playing in reverse */
} CUI_PropertyTween;

/* ============================================================================
 * Tween Sequence
 * ============================================================================ */

typedef struct CUI_TweenSequence {
    uint32_t id;
    uint32_t *tween_ids;
    int tween_count;
    int tween_capacity;
    int current_index;
    bool parallel;              /* Run all tweens simultaneously */
    bool loop;
    bool active;
} CUI_TweenSequence;

/* ============================================================================
 * Easing Function
 * ============================================================================ */

/* Apply easing to a 0-1 progress value */
float cui_ease(CUI_EaseType type, float t);

/* Get easing function name (for debugging) */
const char *cui_ease_name(CUI_EaseType type);

/* ============================================================================
 * Tween Manager Lifecycle
 * ============================================================================ */

/* Create a tween manager */
CUI_TweenManager *cui_tween_manager_create(void);

/* Destroy a tween manager (stops all tweens) */
void cui_tween_manager_destroy(CUI_TweenManager *tm);

/* Update all active tweens (call each frame) */
void cui_tween_manager_update(CUI_TweenManager *tm, float delta_time);

/* Stop all tweens */
void cui_tween_manager_stop_all(CUI_TweenManager *tm);

/* ============================================================================
 * Property Tweens
 * ============================================================================ */

/* Create a property tween with full config */
uint32_t cui_tween_create(CUI_TweenManager *tm, const CUI_TweenConfig *config);

/* Simplified property tween (from current value to target) */
uint32_t cui_tween_property(CUI_TweenManager *tm, CUI_Node *node,
                             CUI_TweenProperty prop, float to,
                             float duration, CUI_EaseType ease);

/* Property tween with explicit start/end values */
uint32_t cui_tween_property_from_to(CUI_TweenManager *tm, CUI_Node *node,
                                     CUI_TweenProperty prop,
                                     float from, float to,
                                     float duration, CUI_EaseType ease);

/* Tween a value directly (no node) */
uint32_t cui_tween_value(CUI_TweenManager *tm, float *value,
                          float from, float to,
                          float duration, CUI_EaseType ease);

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/* Fade animations */
uint32_t cui_tween_fade_in(CUI_TweenManager *tm, CUI_Node *node, float duration);
uint32_t cui_tween_fade_out(CUI_TweenManager *tm, CUI_Node *node, float duration);
uint32_t cui_tween_fade_to(CUI_TweenManager *tm, CUI_Node *node,
                            float opacity, float duration);

/* Slide animations */
uint32_t cui_tween_slide_in(CUI_TweenManager *tm, CUI_Node *node,
                             CUI_Direction from, float duration);
uint32_t cui_tween_slide_out(CUI_TweenManager *tm, CUI_Node *node,
                              CUI_Direction to, float duration);

/* Scale animations */
uint32_t cui_tween_scale_pop(CUI_TweenManager *tm, CUI_Node *node, float duration);
uint32_t cui_tween_scale_to(CUI_TweenManager *tm, CUI_Node *node,
                             float scale_x, float scale_y, float duration);

/* Shake animation (returns sequence ID) */
uint32_t cui_tween_shake(CUI_TweenManager *tm, CUI_Node *node,
                          float intensity, float duration);

/* ============================================================================
 * Tween Control
 * ============================================================================ */

/* Get tween by ID */
CUI_PropertyTween *cui_tween_get(CUI_TweenManager *tm, uint32_t id);

/* Control individual tweens */
void cui_tween_pause(CUI_TweenManager *tm, uint32_t id);
void cui_tween_resume(CUI_TweenManager *tm, uint32_t id);
void cui_tween_stop(CUI_TweenManager *tm, uint32_t id);
void cui_tween_restart(CUI_TweenManager *tm, uint32_t id);

/* Stop all tweens on a specific node */
void cui_tween_stop_node(CUI_TweenManager *tm, CUI_Node *node);

/* Check tween state */
bool cui_tween_is_running(CUI_TweenManager *tm, uint32_t id);
bool cui_tween_is_finished(CUI_TweenManager *tm, uint32_t id);
float cui_tween_get_progress(CUI_TweenManager *tm, uint32_t id);

/* Set completion callback after creation */
void cui_tween_on_complete(CUI_TweenManager *tm, uint32_t id,
                            CUI_TweenCallback callback, void *userdata);

/* ============================================================================
 * Tween Sequences
 * ============================================================================ */

/* Create a sequence of tweens */
CUI_TweenSequence *cui_tween_sequence_create(CUI_TweenManager *tm);

/* Add a tween to a sequence */
void cui_tween_sequence_add(CUI_TweenSequence *seq, uint32_t tween_id);

/* Add a delay to a sequence (creates internal delay tween) */
void cui_tween_sequence_add_delay(CUI_TweenManager *tm, CUI_TweenSequence *seq,
                                   float delay);

/* Configure sequence */
void cui_tween_sequence_set_parallel(CUI_TweenSequence *seq, bool parallel);
void cui_tween_sequence_set_loop(CUI_TweenSequence *seq, bool loop);

/* Control sequence */
void cui_tween_sequence_play(CUI_TweenSequence *seq);
void cui_tween_sequence_stop(CUI_TweenSequence *seq);
void cui_tween_sequence_destroy(CUI_TweenManager *tm, CUI_TweenSequence *seq);

/* ============================================================================
 * Transition Presets
 * ============================================================================ */

/* Predefined transition configurations */
typedef struct CUI_Transition {
    CUI_TweenProperty property;
    float duration;
    CUI_EaseType ease;
} CUI_Transition;

/* Common transition presets */
extern const CUI_Transition CUI_TRANSITION_FADE_FAST;
extern const CUI_Transition CUI_TRANSITION_FADE_NORMAL;
extern const CUI_Transition CUI_TRANSITION_SLIDE_FAST;
extern const CUI_Transition CUI_TRANSITION_SLIDE_NORMAL;
extern const CUI_Transition CUI_TRANSITION_SCALE_POP;

#ifdef __cplusplus
}
#endif

#endif /* CARBON_UI_TWEEN_H */
