/*
 * Carbon UI - Tween/Animation System Implementation
 */

#include "carbon/ui_tween.h"
#include "carbon/ui_node.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define CUI_MAX_TWEENS 256
#define CUI_MAX_SEQUENCES 32

/* ============================================================================
 * Transition Presets
 * ============================================================================ */

const CUI_Transition CUI_TRANSITION_FADE_FAST = {
    .property = CUI_TWEEN_OPACITY,
    .duration = 0.15f,
    .ease = CUI_EASE_OUT_QUAD
};

const CUI_Transition CUI_TRANSITION_FADE_NORMAL = {
    .property = CUI_TWEEN_OPACITY,
    .duration = 0.3f,
    .ease = CUI_EASE_OUT_QUAD
};

const CUI_Transition CUI_TRANSITION_SLIDE_FAST = {
    .property = CUI_TWEEN_POSITION_X,
    .duration = 0.2f,
    .ease = CUI_EASE_OUT_CUBIC
};

const CUI_Transition CUI_TRANSITION_SLIDE_NORMAL = {
    .property = CUI_TWEEN_POSITION_X,
    .duration = 0.4f,
    .ease = CUI_EASE_OUT_CUBIC
};

const CUI_Transition CUI_TRANSITION_SCALE_POP = {
    .property = CUI_TWEEN_SCALE_X,
    .duration = 0.3f,
    .ease = CUI_EASE_OUT_BACK
};

/* ============================================================================
 * Tween Manager Structure
 * ============================================================================ */

struct CUI_TweenManager {
    CUI_PropertyTween tweens[CUI_MAX_TWEENS];
    int tween_count;
    uint32_t next_id;

    CUI_TweenSequence *sequences[CUI_MAX_SEQUENCES];
    int sequence_count;

    /* Pool of value pointers for value-only tweens */
    float value_pool[64];
    int value_pool_used;
};

/* ============================================================================
 * Easing Functions Implementation
 * ============================================================================ */

static float ease_linear(float t) { return t; }

/* Sine easing */
static float ease_in_sine(float t) {
    return 1.0f - cosf((t * (float)M_PI) / 2.0f);
}

static float ease_out_sine(float t) {
    return sinf((t * (float)M_PI) / 2.0f);
}

static float ease_in_out_sine(float t) {
    return -(cosf((float)M_PI * t) - 1.0f) / 2.0f;
}

/* Quadratic easing */
static float ease_in_quad(float t) {
    return t * t;
}

static float ease_out_quad(float t) {
    return 1.0f - (1.0f - t) * (1.0f - t);
}

static float ease_in_out_quad(float t) {
    return t < 0.5f ? 2.0f * t * t : 1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f;
}

/* Cubic easing */
static float ease_in_cubic(float t) {
    return t * t * t;
}

static float ease_out_cubic(float t) {
    return 1.0f - powf(1.0f - t, 3.0f);
}

static float ease_in_out_cubic(float t) {
    return t < 0.5f ? 4.0f * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 3.0f) / 2.0f;
}

/* Quartic easing */
static float ease_in_quart(float t) {
    return t * t * t * t;
}

static float ease_out_quart(float t) {
    return 1.0f - powf(1.0f - t, 4.0f);
}

static float ease_in_out_quart(float t) {
    return t < 0.5f ? 8.0f * t * t * t * t : 1.0f - powf(-2.0f * t + 2.0f, 4.0f) / 2.0f;
}

/* Quintic easing */
static float ease_in_quint(float t) {
    return t * t * t * t * t;
}

static float ease_out_quint(float t) {
    return 1.0f - powf(1.0f - t, 5.0f);
}

static float ease_in_out_quint(float t) {
    return t < 0.5f ? 16.0f * t * t * t * t * t
                    : 1.0f - powf(-2.0f * t + 2.0f, 5.0f) / 2.0f;
}

/* Exponential easing */
static float ease_in_expo(float t) {
    return t == 0.0f ? 0.0f : powf(2.0f, 10.0f * t - 10.0f);
}

static float ease_out_expo(float t) {
    return t == 1.0f ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
}

static float ease_in_out_expo(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    if (t < 0.5f) return powf(2.0f, 20.0f * t - 10.0f) / 2.0f;
    return (2.0f - powf(2.0f, -20.0f * t + 10.0f)) / 2.0f;
}

/* Circular easing */
static float ease_in_circ(float t) {
    return 1.0f - sqrtf(1.0f - powf(t, 2.0f));
}

static float ease_out_circ(float t) {
    return sqrtf(1.0f - powf(t - 1.0f, 2.0f));
}

static float ease_in_out_circ(float t) {
    return t < 0.5f
        ? (1.0f - sqrtf(1.0f - powf(2.0f * t, 2.0f))) / 2.0f
        : (sqrtf(1.0f - powf(-2.0f * t + 2.0f, 2.0f)) + 1.0f) / 2.0f;
}

/* Back easing (overshoot) */
static const float BACK_C1 = 1.70158f;
static const float BACK_C2 = BACK_C1 * 1.525f;
static const float BACK_C3 = BACK_C1 + 1.0f;

static float ease_in_back(float t) {
    return BACK_C3 * t * t * t - BACK_C1 * t * t;
}

static float ease_out_back(float t) {
    return 1.0f + BACK_C3 * powf(t - 1.0f, 3.0f) + BACK_C1 * powf(t - 1.0f, 2.0f);
}

static float ease_in_out_back(float t) {
    return t < 0.5f
        ? (powf(2.0f * t, 2.0f) * ((BACK_C2 + 1.0f) * 2.0f * t - BACK_C2)) / 2.0f
        : (powf(2.0f * t - 2.0f, 2.0f) * ((BACK_C2 + 1.0f) * (t * 2.0f - 2.0f) + BACK_C2) + 2.0f) / 2.0f;
}

/* Elastic easing */
static const float ELASTIC_C4 = (2.0f * (float)M_PI) / 3.0f;
static const float ELASTIC_C5 = (2.0f * (float)M_PI) / 4.5f;

static float ease_in_elastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return -powf(2.0f, 10.0f * t - 10.0f) * sinf((t * 10.0f - 10.75f) * ELASTIC_C4);
}

static float ease_out_elastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    return powf(2.0f, -10.0f * t) * sinf((t * 10.0f - 0.75f) * ELASTIC_C4) + 1.0f;
}

static float ease_in_out_elastic(float t) {
    if (t == 0.0f) return 0.0f;
    if (t == 1.0f) return 1.0f;
    if (t < 0.5f) {
        return -(powf(2.0f, 20.0f * t - 10.0f) * sinf((20.0f * t - 11.125f) * ELASTIC_C5)) / 2.0f;
    }
    return (powf(2.0f, -20.0f * t + 10.0f) * sinf((20.0f * t - 11.125f) * ELASTIC_C5)) / 2.0f + 1.0f;
}

/* Bounce easing */
static float ease_out_bounce(float t) {
    const float n1 = 7.5625f;
    const float d1 = 2.75f;

    if (t < 1.0f / d1) {
        return n1 * t * t;
    } else if (t < 2.0f / d1) {
        t -= 1.5f / d1;
        return n1 * t * t + 0.75f;
    } else if (t < 2.5f / d1) {
        t -= 2.25f / d1;
        return n1 * t * t + 0.9375f;
    } else {
        t -= 2.625f / d1;
        return n1 * t * t + 0.984375f;
    }
}

static float ease_in_bounce(float t) {
    return 1.0f - ease_out_bounce(1.0f - t);
}

static float ease_in_out_bounce(float t) {
    return t < 0.5f
        ? (1.0f - ease_out_bounce(1.0f - 2.0f * t)) / 2.0f
        : (1.0f + ease_out_bounce(2.0f * t - 1.0f)) / 2.0f;
}

/* ============================================================================
 * Easing Function Dispatch
 * ============================================================================ */

float cui_ease(CUI_EaseType type, float t) {
    /* Clamp t to 0-1 */
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    switch (type) {
        case CUI_EASE_LINEAR:        return ease_linear(t);

        case CUI_EASE_IN_SINE:       return ease_in_sine(t);
        case CUI_EASE_OUT_SINE:      return ease_out_sine(t);
        case CUI_EASE_IN_OUT_SINE:   return ease_in_out_sine(t);

        case CUI_EASE_IN_QUAD:       return ease_in_quad(t);
        case CUI_EASE_OUT_QUAD:      return ease_out_quad(t);
        case CUI_EASE_IN_OUT_QUAD:   return ease_in_out_quad(t);

        case CUI_EASE_IN_CUBIC:      return ease_in_cubic(t);
        case CUI_EASE_OUT_CUBIC:     return ease_out_cubic(t);
        case CUI_EASE_IN_OUT_CUBIC:  return ease_in_out_cubic(t);

        case CUI_EASE_IN_QUART:      return ease_in_quart(t);
        case CUI_EASE_OUT_QUART:     return ease_out_quart(t);
        case CUI_EASE_IN_OUT_QUART:  return ease_in_out_quart(t);

        case CUI_EASE_IN_QUINT:      return ease_in_quint(t);
        case CUI_EASE_OUT_QUINT:     return ease_out_quint(t);
        case CUI_EASE_IN_OUT_QUINT:  return ease_in_out_quint(t);

        case CUI_EASE_IN_EXPO:       return ease_in_expo(t);
        case CUI_EASE_OUT_EXPO:      return ease_out_expo(t);
        case CUI_EASE_IN_OUT_EXPO:   return ease_in_out_expo(t);

        case CUI_EASE_IN_CIRC:       return ease_in_circ(t);
        case CUI_EASE_OUT_CIRC:      return ease_out_circ(t);
        case CUI_EASE_IN_OUT_CIRC:   return ease_in_out_circ(t);

        case CUI_EASE_IN_BACK:       return ease_in_back(t);
        case CUI_EASE_OUT_BACK:      return ease_out_back(t);
        case CUI_EASE_IN_OUT_BACK:   return ease_in_out_back(t);

        case CUI_EASE_IN_ELASTIC:    return ease_in_elastic(t);
        case CUI_EASE_OUT_ELASTIC:   return ease_out_elastic(t);
        case CUI_EASE_IN_OUT_ELASTIC: return ease_in_out_elastic(t);

        case CUI_EASE_IN_BOUNCE:     return ease_in_bounce(t);
        case CUI_EASE_OUT_BOUNCE:    return ease_out_bounce(t);
        case CUI_EASE_IN_OUT_BOUNCE: return ease_in_out_bounce(t);

        default: return t;
    }
}

const char *cui_ease_name(CUI_EaseType type) {
    static const char *names[] = {
        "Linear",
        "InSine", "OutSine", "InOutSine",
        "InQuad", "OutQuad", "InOutQuad",
        "InCubic", "OutCubic", "InOutCubic",
        "InQuart", "OutQuart", "InOutQuart",
        "InQuint", "OutQuint", "InOutQuint",
        "InExpo", "OutExpo", "InOutExpo",
        "InCirc", "OutCirc", "InOutCirc",
        "InBack", "OutBack", "InOutBack",
        "InElastic", "OutElastic", "InOutElastic",
        "InBounce", "OutBounce", "InOutBounce"
    };

    if (type >= 0 && type < CUI_EASE_COUNT) {
        return names[type];
    }
    return "Unknown";
}

/* ============================================================================
 * Tween Manager Lifecycle
 * ============================================================================ */

CUI_TweenManager *cui_tween_manager_create(void) {
    CUI_TweenManager *tm = (CUI_TweenManager *)calloc(1, sizeof(CUI_TweenManager));
    if (!tm) return NULL;

    tm->next_id = 1;
    return tm;
}

void cui_tween_manager_destroy(CUI_TweenManager *tm) {
    if (!tm) return;

    /* Destroy sequences */
    for (int i = 0; i < tm->sequence_count; i++) {
        if (tm->sequences[i]) {
            free(tm->sequences[i]->tween_ids);
            free(tm->sequences[i]);
        }
    }

    free(tm);
}

/* ============================================================================
 * Property Value Access
 * ============================================================================ */

/* Get current value of a property on a node */
static float cui_tween_get_property_value(CUI_Node *node, CUI_TweenProperty prop) {
    if (!node) return 0.0f;

    /* NOTE: This requires CUI_Node to be defined. For now, return 0.
       When ui_node.h is integrated, this will access actual node properties. */
    (void)prop;

    /* Placeholder - will be implemented when CUI_Node is available */
    return 0.0f;
}

/* Set a property value on a node */
static void cui_tween_set_property_value(CUI_Node *node, CUI_TweenProperty prop,
                                          float value) {
    if (!node) return;

    /* NOTE: This requires CUI_Node to be defined.
       When ui_node.h is integrated, this will set actual node properties. */
    (void)prop;
    (void)value;

    /* Placeholder - will be implemented when CUI_Node is available */
}

/* ============================================================================
 * Tween Update
 * ============================================================================ */

static void cui_tween_update_one(CUI_TweenManager *tm, CUI_PropertyTween *tween,
                                  float dt) {
    if (tween->state != CUI_TWEEN_RUNNING) return;

    /* Handle delay */
    if (tween->elapsed < tween->config.delay) {
        tween->elapsed += dt;
        if (tween->elapsed < tween->config.delay) {
            return;
        }
        /* Adjust for leftover time after delay */
        dt = tween->elapsed - tween->config.delay;
        tween->elapsed = tween->config.delay;
    }

    /* Update elapsed time */
    float active_elapsed = tween->elapsed - tween->config.delay + dt;
    tween->elapsed += dt;

    /* Calculate progress */
    float progress = active_elapsed / tween->config.duration;
    if (progress > 1.0f) progress = 1.0f;

    /* Handle reverse */
    if (tween->reversing) {
        progress = 1.0f - progress;
    }

    /* Apply easing */
    float eased = cui_ease(tween->config.ease, progress);

    /* Calculate value */
    float start = tween->config.start_value;
    float end = tween->config.end_value;
    tween->current_value = start + (end - start) * eased;

    /* Apply value */
    if (tween->config.property == CUI_TWEEN_CUSTOM) {
        if (tween->config.custom_setter) {
            tween->config.custom_setter(tween->config.target,
                                         tween->current_value,
                                         tween->config.custom_userdata);
        }
    } else {
        cui_tween_set_property_value(tween->config.target,
                                      tween->config.property,
                                      tween->current_value);
    }

    /* Check for completion */
    if (active_elapsed >= tween->config.duration) {
        if (tween->config.auto_reverse && !tween->reversing) {
            /* Start reverse */
            tween->reversing = true;
            tween->elapsed = tween->config.delay;
        } else if (tween->config.repeat_count != 0) {
            /* Repeat */
            if (tween->config.repeat_count > 0) {
                tween->current_repeat++;
                if (tween->current_repeat >= tween->config.repeat_count) {
                    tween->state = CUI_TWEEN_FINISHED;
                    if (tween->config.on_complete) {
                        tween->config.on_complete(tween->id,
                                                   tween->config.callback_userdata);
                    }
                    return;
                }
            }
            /* Reset for next iteration */
            tween->elapsed = tween->config.delay;
            tween->reversing = false;
        } else {
            /* Done */
            tween->state = CUI_TWEEN_FINISHED;
            if (tween->config.on_complete) {
                tween->config.on_complete(tween->id,
                                           tween->config.callback_userdata);
            }
        }
    }
}

void cui_tween_manager_update(CUI_TweenManager *tm, float delta_time) {
    if (!tm) return;

    /* Update all tweens */
    for (int i = 0; i < tm->tween_count; i++) {
        cui_tween_update_one(tm, &tm->tweens[i], delta_time);
    }

    /* Update sequences */
    for (int i = 0; i < tm->sequence_count; i++) {
        CUI_TweenSequence *seq = tm->sequences[i];
        if (!seq || !seq->active) continue;

        if (seq->parallel) {
            /* All tweens run together - check if all finished */
            bool all_finished = true;
            for (int j = 0; j < seq->tween_count; j++) {
                CUI_PropertyTween *t = cui_tween_get(tm, seq->tween_ids[j]);
                if (t && t->state != CUI_TWEEN_FINISHED) {
                    all_finished = false;
                }
            }

            if (all_finished) {
                if (seq->loop) {
                    /* Restart all */
                    for (int j = 0; j < seq->tween_count; j++) {
                        cui_tween_restart(tm, seq->tween_ids[j]);
                    }
                } else {
                    seq->active = false;
                }
            }
        } else {
            /* Sequential: advance when current finishes */
            if (seq->current_index < seq->tween_count) {
                CUI_PropertyTween *t = cui_tween_get(tm, seq->tween_ids[seq->current_index]);
                if (t && t->state == CUI_TWEEN_FINISHED) {
                    seq->current_index++;
                    if (seq->current_index < seq->tween_count) {
                        /* Start next tween */
                        CUI_PropertyTween *next = cui_tween_get(tm, seq->tween_ids[seq->current_index]);
                        if (next && next->state == CUI_TWEEN_IDLE) {
                            next->state = CUI_TWEEN_RUNNING;
                        }
                    } else if (seq->loop) {
                        /* Restart sequence */
                        seq->current_index = 0;
                        for (int j = 0; j < seq->tween_count; j++) {
                            cui_tween_restart(tm, seq->tween_ids[j]);
                        }
                        /* Start first */
                        CUI_PropertyTween *first = cui_tween_get(tm, seq->tween_ids[0]);
                        if (first) first->state = CUI_TWEEN_RUNNING;
                    } else {
                        seq->active = false;
                    }
                }
            }
        }
    }

    /* Compact finished tweens (optional, for performance) */
    /* For now, leave them in place to preserve IDs */
}

void cui_tween_manager_stop_all(CUI_TweenManager *tm) {
    if (!tm) return;

    for (int i = 0; i < tm->tween_count; i++) {
        tm->tweens[i].state = CUI_TWEEN_FINISHED;
    }

    for (int i = 0; i < tm->sequence_count; i++) {
        if (tm->sequences[i]) {
            tm->sequences[i]->active = false;
        }
    }
}

/* ============================================================================
 * Property Tweens
 * ============================================================================ */

uint32_t cui_tween_create(CUI_TweenManager *tm, const CUI_TweenConfig *config) {
    if (!tm || !config) return 0;
    if (tm->tween_count >= CUI_MAX_TWEENS) return 0;

    CUI_PropertyTween *tween = &tm->tweens[tm->tween_count++];
    memset(tween, 0, sizeof(*tween));

    tween->id = tm->next_id++;
    tween->config = *config;
    tween->state = CUI_TWEEN_RUNNING;
    tween->elapsed = 0.0f;
    tween->current_value = config->start_value;

    return tween->id;
}

uint32_t cui_tween_property(CUI_TweenManager *tm, CUI_Node *node,
                             CUI_TweenProperty prop, float to,
                             float duration, CUI_EaseType ease) {
    float from = cui_tween_get_property_value(node, prop);
    return cui_tween_property_from_to(tm, node, prop, from, to, duration, ease);
}

uint32_t cui_tween_property_from_to(CUI_TweenManager *tm, CUI_Node *node,
                                     CUI_TweenProperty prop,
                                     float from, float to,
                                     float duration, CUI_EaseType ease) {
    CUI_TweenConfig config = {0};
    config.target = node;
    config.property = prop;
    config.start_value = from;
    config.end_value = to;
    config.duration = duration;
    config.ease = ease;

    return cui_tween_create(tm, &config);
}

uint32_t cui_tween_value(CUI_TweenManager *tm, float *value,
                          float from, float to,
                          float duration, CUI_EaseType ease) {
    if (!tm || !value) return 0;

    /* Use custom setter to update the value pointer */
    CUI_TweenConfig config = {0};
    config.target = NULL;
    config.property = CUI_TWEEN_CUSTOM;
    config.start_value = from;
    config.end_value = to;
    config.duration = duration;
    config.ease = ease;
    config.custom_userdata = value;
    config.custom_setter = [](CUI_Node*, float v, void *ud) {
        *(float *)ud = v;
    };

    return cui_tween_create(tm, &config);
}

/* ============================================================================
 * Convenience Functions
 * ============================================================================ */

uint32_t cui_tween_fade_in(CUI_TweenManager *tm, CUI_Node *node, float duration) {
    return cui_tween_property_from_to(tm, node, CUI_TWEEN_OPACITY,
                                       0.0f, 1.0f, duration, CUI_EASE_OUT_QUAD);
}

uint32_t cui_tween_fade_out(CUI_TweenManager *tm, CUI_Node *node, float duration) {
    return cui_tween_property_from_to(tm, node, CUI_TWEEN_OPACITY,
                                       1.0f, 0.0f, duration, CUI_EASE_OUT_QUAD);
}

uint32_t cui_tween_fade_to(CUI_TweenManager *tm, CUI_Node *node,
                            float opacity, float duration) {
    return cui_tween_property(tm, node, CUI_TWEEN_OPACITY,
                               opacity, duration, CUI_EASE_OUT_QUAD);
}

uint32_t cui_tween_slide_in(CUI_TweenManager *tm, CUI_Node *node,
                             CUI_Direction from, float duration) {
    CUI_TweenProperty prop;
    float start_offset = 100.0f;  /* Slide distance */

    switch (from) {
        case CUI_DIR_LEFT:
            prop = CUI_TWEEN_OFFSET_LEFT;
            start_offset = -100.0f;
            break;
        case CUI_DIR_RIGHT:
            prop = CUI_TWEEN_OFFSET_LEFT;
            start_offset = 100.0f;
            break;
        case CUI_DIR_UP:
            prop = CUI_TWEEN_OFFSET_TOP;
            start_offset = -100.0f;
            break;
        case CUI_DIR_DOWN:
            prop = CUI_TWEEN_OFFSET_TOP;
            start_offset = 100.0f;
            break;
        default:
            return 0;
    }

    float current = cui_tween_get_property_value(node, prop);
    return cui_tween_property_from_to(tm, node, prop,
                                       current + start_offset, current,
                                       duration, CUI_EASE_OUT_CUBIC);
}

uint32_t cui_tween_slide_out(CUI_TweenManager *tm, CUI_Node *node,
                              CUI_Direction to, float duration) {
    CUI_TweenProperty prop;
    float end_offset = 100.0f;

    switch (to) {
        case CUI_DIR_LEFT:
            prop = CUI_TWEEN_OFFSET_LEFT;
            end_offset = -100.0f;
            break;
        case CUI_DIR_RIGHT:
            prop = CUI_TWEEN_OFFSET_LEFT;
            end_offset = 100.0f;
            break;
        case CUI_DIR_UP:
            prop = CUI_TWEEN_OFFSET_TOP;
            end_offset = -100.0f;
            break;
        case CUI_DIR_DOWN:
            prop = CUI_TWEEN_OFFSET_TOP;
            end_offset = 100.0f;
            break;
        default:
            return 0;
    }

    float current = cui_tween_get_property_value(node, prop);
    return cui_tween_property_from_to(tm, node, prop,
                                       current, current + end_offset,
                                       duration, CUI_EASE_IN_CUBIC);
}

uint32_t cui_tween_scale_pop(CUI_TweenManager *tm, CUI_Node *node, float duration) {
    /* Scale from 0.8 to 1.0 with overshoot */
    uint32_t tx = cui_tween_property_from_to(tm, node, CUI_TWEEN_SCALE_X,
                                              0.8f, 1.0f, duration, CUI_EASE_OUT_BACK);
    cui_tween_property_from_to(tm, node, CUI_TWEEN_SCALE_Y,
                                0.8f, 1.0f, duration, CUI_EASE_OUT_BACK);
    return tx;
}

uint32_t cui_tween_scale_to(CUI_TweenManager *tm, CUI_Node *node,
                             float scale_x, float scale_y, float duration) {
    uint32_t tx = cui_tween_property(tm, node, CUI_TWEEN_SCALE_X,
                                      scale_x, duration, CUI_EASE_OUT_QUAD);
    cui_tween_property(tm, node, CUI_TWEEN_SCALE_Y,
                        scale_y, duration, CUI_EASE_OUT_QUAD);
    return tx;
}

uint32_t cui_tween_shake(CUI_TweenManager *tm, CUI_Node *node,
                          float intensity, float duration) {
    /* Create a sequence of quick back-and-forth movements */
    CUI_TweenSequence *seq = cui_tween_sequence_create(tm);
    if (!seq) return 0;

    int shakes = (int)(duration / 0.05f);
    if (shakes < 2) shakes = 2;
    if (shakes > 10) shakes = 10;

    float shake_duration = duration / shakes;
    float current = cui_tween_get_property_value(node, CUI_TWEEN_OFFSET_LEFT);

    for (int i = 0; i < shakes; i++) {
        float offset = (i % 2 == 0) ? intensity : -intensity;
        offset *= (1.0f - (float)i / shakes);  /* Decay */

        uint32_t id = cui_tween_property_from_to(tm, node, CUI_TWEEN_OFFSET_LEFT,
                                                  current + offset, current,
                                                  shake_duration, CUI_EASE_OUT_QUAD);
        cui_tween_sequence_add(seq, id);
    }

    cui_tween_sequence_play(seq);
    return seq->id;
}

/* ============================================================================
 * Tween Control
 * ============================================================================ */

CUI_PropertyTween *cui_tween_get(CUI_TweenManager *tm, uint32_t id) {
    if (!tm || id == 0) return NULL;

    for (int i = 0; i < tm->tween_count; i++) {
        if (tm->tweens[i].id == id) {
            return &tm->tweens[i];
        }
    }
    return NULL;
}

void cui_tween_pause(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    if (t && t->state == CUI_TWEEN_RUNNING) {
        t->state = CUI_TWEEN_PAUSED;
    }
}

void cui_tween_resume(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    if (t && t->state == CUI_TWEEN_PAUSED) {
        t->state = CUI_TWEEN_RUNNING;
    }
}

void cui_tween_stop(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    if (t) {
        t->state = CUI_TWEEN_FINISHED;
    }
}

void cui_tween_restart(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    if (t) {
        t->state = CUI_TWEEN_IDLE;
        t->elapsed = 0.0f;
        t->current_repeat = 0;
        t->reversing = false;
        t->current_value = t->config.start_value;
    }
}

void cui_tween_stop_node(CUI_TweenManager *tm, CUI_Node *node) {
    if (!tm) return;

    for (int i = 0; i < tm->tween_count; i++) {
        if (tm->tweens[i].config.target == node) {
            tm->tweens[i].state = CUI_TWEEN_FINISHED;
        }
    }
}

bool cui_tween_is_running(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    return t && t->state == CUI_TWEEN_RUNNING;
}

bool cui_tween_is_finished(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    return t && t->state == CUI_TWEEN_FINISHED;
}

float cui_tween_get_progress(CUI_TweenManager *tm, uint32_t id) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    if (!t) return 0.0f;

    float active_elapsed = t->elapsed - t->config.delay;
    if (active_elapsed < 0) return 0.0f;

    float progress = active_elapsed / t->config.duration;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}

void cui_tween_on_complete(CUI_TweenManager *tm, uint32_t id,
                            CUI_TweenCallback callback, void *userdata) {
    CUI_PropertyTween *t = cui_tween_get(tm, id);
    if (t) {
        t->config.on_complete = callback;
        t->config.callback_userdata = userdata;
    }
}

/* ============================================================================
 * Tween Sequences
 * ============================================================================ */

CUI_TweenSequence *cui_tween_sequence_create(CUI_TweenManager *tm) {
    if (!tm || tm->sequence_count >= CUI_MAX_SEQUENCES) return NULL;

    CUI_TweenSequence *seq = (CUI_TweenSequence *)calloc(1, sizeof(CUI_TweenSequence));
    if (!seq) return NULL;

    seq->id = tm->next_id++;
    seq->tween_capacity = 16;
    seq->tween_ids = (uint32_t *)calloc(seq->tween_capacity, sizeof(uint32_t));
    if (!seq->tween_ids) {
        free(seq);
        return NULL;
    }

    tm->sequences[tm->sequence_count++] = seq;
    return seq;
}

void cui_tween_sequence_add(CUI_TweenSequence *seq, uint32_t tween_id) {
    if (!seq || tween_id == 0) return;

    /* Grow if needed */
    if (seq->tween_count >= seq->tween_capacity) {
        int new_cap = seq->tween_capacity * 2;
        uint32_t *new_ids = (uint32_t *)realloc(seq->tween_ids,
                                                 new_cap * sizeof(uint32_t));
        if (!new_ids) return;
        seq->tween_ids = new_ids;
        seq->tween_capacity = new_cap;
    }

    seq->tween_ids[seq->tween_count++] = tween_id;
}

void cui_tween_sequence_add_delay(CUI_TweenManager *tm, CUI_TweenSequence *seq,
                                   float delay) {
    if (!tm || !seq) return;

    /* Create a dummy tween that just waits */
    CUI_TweenConfig config = {0};
    config.duration = delay;
    config.ease = CUI_EASE_LINEAR;

    uint32_t id = cui_tween_create(tm, &config);
    cui_tween_sequence_add(seq, id);
}

void cui_tween_sequence_set_parallel(CUI_TweenSequence *seq, bool parallel) {
    if (seq) seq->parallel = parallel;
}

void cui_tween_sequence_set_loop(CUI_TweenSequence *seq, bool loop) {
    if (seq) seq->loop = loop;
}

void cui_tween_sequence_play(CUI_TweenSequence *seq) {
    if (!seq) return;

    seq->active = true;
    seq->current_index = 0;

    /* For sequential, tweens should start as idle, first one starts now */
    /* For parallel, all tweens start immediately */

    /* Note: Since tweens are created as RUNNING, we need to set them to IDLE
       for sequential playback, then start the first one. */
}

void cui_tween_sequence_stop(CUI_TweenSequence *seq) {
    if (seq) seq->active = false;
}

void cui_tween_sequence_destroy(CUI_TweenManager *tm, CUI_TweenSequence *seq) {
    if (!tm || !seq) return;

    /* Find and remove from manager */
    for (int i = 0; i < tm->sequence_count; i++) {
        if (tm->sequences[i] == seq) {
            free(seq->tween_ids);
            free(seq);
            /* Shift remaining */
            for (int j = i; j < tm->sequence_count - 1; j++) {
                tm->sequences[j] = tm->sequences[j + 1];
            }
            tm->sequence_count--;
            return;
        }
    }
}
