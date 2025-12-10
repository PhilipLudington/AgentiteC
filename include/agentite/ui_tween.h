/*
 * Agentite UI - Tween/Animation System
 *
 * Provides property-based animations with easing for UI elements.
 *
 * Usage:
 *   AUI_TweenManager *tm = aui_tween_manager_create();
 *
 *   // Animate a node's opacity
 *   uint32_t id = aui_tween_property(tm, node, AUI_TWEEN_OPACITY, 0.0f, 1.0f,
 *                                    0.3f, AUI_EASE_OUT_QUAD);
 *
 *   // Convenience functions
 *   aui_tween_fade_in(tm, node, 0.3f);
 *   aui_tween_slide_in(tm, node, AUI_DIR_LEFT, 0.5f);
 *
 *   // Each frame
 *   aui_tween_manager_update(tm, delta_time);
 *
 *   aui_tween_manager_destroy(tm);
 */

#ifndef AGENTITE_UI_TWEEN_H
#define AGENTITE_UI_TWEEN_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declarations */
typedef struct AUI_Node AUI_Node;
typedef struct AUI_TweenManager AUI_TweenManager;

/* ============================================================================
 * Easing Types
 * ============================================================================ */

typedef enum AUI_EaseType {
    /* Linear */
    AUI_EASE_LINEAR,

    /* Sine */
    AUI_EASE_IN_SINE,
    AUI_EASE_OUT_SINE,
    AUI_EASE_IN_OUT_SINE,

    /* Quadratic */
    AUI_EASE_IN_QUAD,
    AUI_EASE_OUT_QUAD,
    AUI_EASE_IN_OUT_QUAD,

    /* Cubic */
    AUI_EASE_IN_CUBIC,
    AUI_EASE_OUT_CUBIC,
    AUI_EASE_IN_OUT_CUBIC,

    /* Quartic */
    AUI_EASE_IN_QUART,
    AUI_EASE_OUT_QUART,
    AUI_EASE_IN_OUT_QUART,

    /* Quintic */
    AUI_EASE_IN_QUINT,
    AUI_EASE_OUT_QUINT,
    AUI_EASE_IN_OUT_QUINT,

    /* Exponential */
    AUI_EASE_IN_EXPO,
    AUI_EASE_OUT_EXPO,
    AUI_EASE_IN_OUT_EXPO,

    /* Circular */
    AUI_EASE_IN_CIRC,
    AUI_EASE_OUT_CIRC,
    AUI_EASE_IN_OUT_CIRC,

    /* Back (overshoot) */
    AUI_EASE_IN_BACK,
    AUI_EASE_OUT_BACK,
    AUI_EASE_IN_OUT_BACK,

    /* Elastic */
    AUI_EASE_IN_ELASTIC,
    AUI_EASE_OUT_ELASTIC,
    AUI_EASE_IN_OUT_ELASTIC,

    /* Bounce */
    AUI_EASE_IN_BOUNCE,
    AUI_EASE_OUT_BOUNCE,
    AUI_EASE_IN_OUT_BOUNCE,

    AUI_EASE_COUNT
} AUI_EaseType;

/* ============================================================================
 * Tween Property Types
 * ============================================================================ */

typedef enum AUI_TweenProperty {
    /* Position */
    AUI_TWEEN_POSITION_X,
    AUI_TWEEN_POSITION_Y,

    /* Size */
    AUI_TWEEN_SIZE_X,
    AUI_TWEEN_SIZE_Y,

    /* Anchor offsets (for retained-mode nodes) */
    AUI_TWEEN_OFFSET_LEFT,
    AUI_TWEEN_OFFSET_TOP,
    AUI_TWEEN_OFFSET_RIGHT,
    AUI_TWEEN_OFFSET_BOTTOM,

    /* Visual */
    AUI_TWEEN_OPACITY,
    AUI_TWEEN_ROTATION,
    AUI_TWEEN_SCALE_X,
    AUI_TWEEN_SCALE_Y,

    /* Color channels (0-255 values internally, 0-1 for API) */
    AUI_TWEEN_COLOR_R,
    AUI_TWEEN_COLOR_G,
    AUI_TWEEN_COLOR_B,
    AUI_TWEEN_COLOR_A,

    /* Scroll position (for scroll containers) */
    AUI_TWEEN_SCROLL_X,
    AUI_TWEEN_SCROLL_Y,

    /* Custom property (uses callback) */
    AUI_TWEEN_CUSTOM,

    AUI_TWEEN_PROPERTY_COUNT
} AUI_TweenProperty;

/* ============================================================================
 * Direction (for slide animations)
 * ============================================================================ */

typedef enum AUI_Direction {
    AUI_DIR_LEFT,
    AUI_DIR_RIGHT,
    AUI_DIR_UP,
    AUI_DIR_DOWN
} AUI_Direction;

/* ============================================================================
 * Tween State
 * ============================================================================ */

typedef enum AUI_TweenState {
    AUI_TWEEN_IDLE,
    AUI_TWEEN_RUNNING,
    AUI_TWEEN_PAUSED,
    AUI_TWEEN_FINISHED
} AUI_TweenState;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/* Called when a tween completes */
typedef void (*AUI_TweenCallback)(uint32_t tween_id, void *userdata);

/* Called to set a custom property value */
typedef void (*AUI_TweenSetter)(AUI_Node *node, float value, void *userdata);

/* Called to get a custom property value */
typedef float (*AUI_TweenGetter)(AUI_Node *node, void *userdata);

/* ============================================================================
 * Tween Configuration
 * ============================================================================ */

typedef struct AUI_TweenConfig {
    AUI_Node *target;           /* Target node (can be NULL for value-only tweens) */
    AUI_TweenProperty property;
    float start_value;
    float end_value;
    float duration;             /* In seconds */
    float delay;                /* Delay before starting (seconds) */
    AUI_EaseType ease;

    /* Callbacks */
    AUI_TweenCallback on_complete;
    void *callback_userdata;

    /* For custom properties */
    AUI_TweenSetter custom_setter;
    AUI_TweenGetter custom_getter;
    void *custom_userdata;

    /* Options */
    bool auto_reverse;          /* Ping-pong animation */
    int repeat_count;           /* 0 = once, -1 = infinite */
    bool relative;              /* Add to current value instead of set */
} AUI_TweenConfig;

/* ============================================================================
 * Property Tween (internal structure, exposed for inspection)
 * ============================================================================ */

typedef struct AUI_PropertyTween {
    uint32_t id;
    AUI_TweenConfig config;
    AUI_TweenState state;
    float elapsed;
    float current_value;
    int current_repeat;
    bool reversing;             /* Currently playing in reverse */
} AUI_PropertyTween;

/* ============================================================================
 * Tween Sequence
 * ============================================================================ */

typedef struct AUI_TweenSequence {
    uint32_t id;
    struct AUI_TweenManager *manager;  /* Owning tween manager */
    uint32_t *tween_ids;
    int tween_count;
    int tween_capacity;
    int current_index;
    bool parallel;              /* Run all tweens simultaneously */
    bool loop;
    bool active;
} AUI_TweenSequence;

/* ============================================================================
 * Easing Function
 * ============================================================================ */

/* Apply easing to a 0-1 progress value */
float aui_ease(AUI_EaseType type, float t);

/* Get easing function name (for debugging) */
const char *aui_ease_name(AUI_EaseType type);

/* ============================================================================
 * Tween Manager Lifecycle
 * ============================================================================ */

/* Create a tween manager */
AUI_TweenManager *aui_tween_manager_create(void);

/* Destroy a tween manager (stops all tweens) */
void aui_tween_manager_destroy(AUI_TweenManager *tm);

/* Update all active tweens (call each frame) */
void aui_tween_manager_update(AUI_TweenManager *tm, float delta_time);

/* Stop all tweens */
void aui_tween_manager_stop_all(AUI_TweenManager *tm);

/* ============================================================================
 * Property Tweens
 * ============================================================================ */

/* Create a property tween with full config */
uint32_t aui_tween_create(AUI_TweenManager *tm, const AUI_TweenConfig *config);

/* Simplified property tween (from current value to target) */
uint32_t aui_tween_property(AUI_TweenManager *tm, AUI_Node *node,
                             AUI_TweenProperty prop, float to,
                             float duration, AUI_EaseType ease);

/* Property tween with explicit start/end values */
uint32_t aui_tween_property_from_to(AUI_TweenManager *tm, AUI_Node *node,
                                     AUI_TweenProperty prop,
                                     float from, float to,
                                     float duration, AUI_EaseType ease);

/* Tween a value directly (no node) */
uint32_t aui_tween_value(AUI_TweenManager *tm, float *value,
                          float from, float to,
                          float duration, AUI_EaseType ease);

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

/* Fade animations */
uint32_t aui_tween_fade_in(AUI_TweenManager *tm, AUI_Node *node, float duration);
uint32_t aui_tween_fade_out(AUI_TweenManager *tm, AUI_Node *node, float duration);
uint32_t aui_tween_fade_to(AUI_TweenManager *tm, AUI_Node *node,
                            float opacity, float duration);

/* Slide animations */
uint32_t aui_tween_slide_in(AUI_TweenManager *tm, AUI_Node *node,
                             AUI_Direction from, float duration);
uint32_t aui_tween_slide_out(AUI_TweenManager *tm, AUI_Node *node,
                              AUI_Direction to, float duration);

/* Scale animations */
uint32_t aui_tween_scale_pop(AUI_TweenManager *tm, AUI_Node *node, float duration);
uint32_t aui_tween_scale_to(AUI_TweenManager *tm, AUI_Node *node,
                             float scale_x, float scale_y, float duration);

/* Shake animation (returns sequence ID) */
uint32_t aui_tween_shake(AUI_TweenManager *tm, AUI_Node *node,
                          float intensity, float duration);

/* ============================================================================
 * Tween Control
 * ============================================================================ */

/* Get tween by ID */
AUI_PropertyTween *aui_tween_get(AUI_TweenManager *tm, uint32_t id);

/* Control individual tweens */
void aui_tween_pause(AUI_TweenManager *tm, uint32_t id);
void aui_tween_resume(AUI_TweenManager *tm, uint32_t id);
void aui_tween_stop(AUI_TweenManager *tm, uint32_t id);
void aui_tween_restart(AUI_TweenManager *tm, uint32_t id);

/* Stop all tweens on a specific node */
void aui_tween_stop_node(AUI_TweenManager *tm, AUI_Node *node);

/* Check tween state */
bool aui_tween_is_running(AUI_TweenManager *tm, uint32_t id);
bool aui_tween_is_finished(AUI_TweenManager *tm, uint32_t id);
float aui_tween_get_progress(AUI_TweenManager *tm, uint32_t id);

/* Set completion callback after creation */
void aui_tween_on_complete(AUI_TweenManager *tm, uint32_t id,
                            AUI_TweenCallback callback, void *userdata);

/* ============================================================================
 * Tween Sequences
 * ============================================================================ */

/* Create a sequence of tweens */
AUI_TweenSequence *aui_tween_sequence_create(AUI_TweenManager *tm);

/* Add a tween to a sequence */
void aui_tween_sequence_add(AUI_TweenSequence *seq, uint32_t tween_id);

/* Add a delay to a sequence (creates internal delay tween) */
void aui_tween_sequence_add_delay(AUI_TweenManager *tm, AUI_TweenSequence *seq,
                                   float delay);

/* Configure sequence */
void aui_tween_sequence_set_parallel(AUI_TweenSequence *seq, bool parallel);
void aui_tween_sequence_set_loop(AUI_TweenSequence *seq, bool loop);

/* Control sequence */
void aui_tween_sequence_play(AUI_TweenSequence *seq);
void aui_tween_sequence_stop(AUI_TweenSequence *seq);
void aui_tween_sequence_destroy(AUI_TweenManager *tm, AUI_TweenSequence *seq);

/* ============================================================================
 * Transition Presets
 * ============================================================================ */

/* Predefined transition configurations */
typedef struct AUI_Transition {
    AUI_TweenProperty property;
    float duration;
    AUI_EaseType ease;
} AUI_Transition;

/* Common transition presets */
extern const AUI_Transition AUI_TRANSITION_FADE_FAST;
extern const AUI_Transition AUI_TRANSITION_FADE_NORMAL;
extern const AUI_Transition AUI_TRANSITION_SLIDE_FAST;
extern const AUI_Transition AUI_TRANSITION_SLIDE_NORMAL;
extern const AUI_Transition AUI_TRANSITION_SCALE_POP;

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_UI_TWEEN_H */
