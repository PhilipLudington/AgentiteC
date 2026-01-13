/*
 * Agentite Screen Transition System Implementation
 *
 * Provides smooth visual transitions between game screens/scenes.
 */

#include "agentite/transition.h"
#include "agentite/shader.h"
#include "agentite/error.h"
#include "transition_shaders.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdio>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_PARAM_SIZE 64

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Agentite_Transition {
    /* Dependencies */
    Agentite_ShaderSystem *shader_system;
    SDL_GPUDevice *gpu;

    /* Configuration */
    Agentite_TransitionConfig config;

    /* Render targets */
    SDL_GPUTexture *source_texture;     /* Captured outgoing scene */
    SDL_GPUTexture *render_target;      /* For scene capture */
    int width;
    int height;

    /* Shaders for effects */
    Agentite_Shader *fade_shader;
    Agentite_Shader *crossfade_shader;
    Agentite_Shader *wipe_shader;
    Agentite_Shader *dissolve_shader;
    Agentite_Shader *pixelate_shader;
    Agentite_Shader *slide_shader;
    Agentite_Shader *circle_shader;

    /* State */
    Agentite_TransitionState state;
    float elapsed;
    float progress;
    float eased_progress;
    bool midpoint_triggered;
    bool has_source;
};

/* ============================================================================
 * Shader Parameter Structures (16-byte aligned)
 * ============================================================================ */

typedef struct {
    float color[4];     /* Fade color RGBA */
} TransitionParams_Fade;

typedef struct {
    float progress;     /* 0-1 progress */
    float softness;     /* Edge softness */
    float _pad[2];
} TransitionParams_Crossfade;

typedef struct {
    float progress;     /* 0-1 progress */
    float direction;    /* 0=left, 1=right, 2=up, 3=down, 4=diagonal */
    float softness;     /* Edge softness */
    float _pad;
} TransitionParams_Wipe;

typedef struct {
    float progress;     /* 0-1 progress */
    float edge_width;   /* Dissolve edge width */
    float _pad[2];
} TransitionParams_Dissolve;

typedef struct {
    float progress;     /* 0-1 progress */
    float pixel_size;   /* Current pixel size */
    float _pad[2];
} TransitionParams_Pixelate;

typedef struct {
    float progress;     /* 0-1 progress */
    float direction;    /* 0=left, 1=right, 2=up, 3=down */
    float is_push;      /* 0=slide, 1=push */
    float _pad;
} TransitionParams_Slide;

typedef struct {
    float progress;     /* 0-1 progress */
    float center_x;     /* Circle center X (0-1) */
    float center_y;     /* Circle center Y (0-1) */
    float is_open;      /* 0=close, 1=open */
} TransitionParams_Circle;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static void destroy_render_targets(Agentite_Transition *trans);
static bool create_shaders(Agentite_Transition *trans);
static void destroy_shaders(Agentite_Transition *trans);
static Agentite_Shader *get_shader_for_effect(Agentite_Transition *trans,
                                               Agentite_TransitionEffect effect);
static void build_params_for_effect(Agentite_Transition *trans,
                                    Agentite_TransitionEffect effect,
                                    float progress,
                                    void *out_params,
                                    size_t *out_size);

/* ============================================================================
 * Easing Functions
 * ============================================================================ */

static float ease_linear(float t)
{
    return t;
}

static float ease_in_quad(float t)
{
    return t * t;
}

static float ease_out_quad(float t)
{
    return t * (2.0f - t);
}

static float ease_in_out_quad(float t)
{
    return t < 0.5f ? 2.0f * t * t : -1.0f + (4.0f - 2.0f * t) * t;
}

static float ease_in_cubic(float t)
{
    return t * t * t;
}

static float ease_out_cubic(float t)
{
    float f = t - 1.0f;
    return f * f * f + 1.0f;
}

static float ease_in_out_cubic(float t)
{
    return t < 0.5f
        ? 4.0f * t * t * t
        : (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;
}

static float ease_in(float t)
{
    /* Sine-based ease in */
    return 1.0f - cosf(t * (float)M_PI * 0.5f);
}

static float ease_out(float t)
{
    /* Sine-based ease out */
    return sinf(t * (float)M_PI * 0.5f);
}

static float ease_in_out(float t)
{
    /* Sine-based ease in-out */
    return 0.5f * (1.0f - cosf(t * (float)M_PI));
}

static float ease_back_in(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    return c3 * t * t * t - c1 * t * t;
}

static float ease_back_out(float t)
{
    const float c1 = 1.70158f;
    const float c3 = c1 + 1.0f;
    float f = t - 1.0f;
    return 1.0f + c3 * f * f * f + c1 * f * f;
}

static float ease_bounce_out(float t)
{
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

float agentite_transition_apply_easing(Agentite_TransitionEasing easing, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    switch (easing) {
        case AGENTITE_EASING_LINEAR:       return ease_linear(t);
        case AGENTITE_EASING_EASE_IN:      return ease_in(t);
        case AGENTITE_EASING_EASE_OUT:     return ease_out(t);
        case AGENTITE_EASING_EASE_IN_OUT:  return ease_in_out(t);
        case AGENTITE_EASING_QUAD_IN:      return ease_in_quad(t);
        case AGENTITE_EASING_QUAD_OUT:     return ease_out_quad(t);
        case AGENTITE_EASING_QUAD_IN_OUT:  return ease_in_out_quad(t);
        case AGENTITE_EASING_CUBIC_IN:     return ease_in_cubic(t);
        case AGENTITE_EASING_CUBIC_OUT:    return ease_out_cubic(t);
        case AGENTITE_EASING_CUBIC_IN_OUT: return ease_in_out_cubic(t);
        case AGENTITE_EASING_BACK_IN:      return ease_back_in(t);
        case AGENTITE_EASING_BACK_OUT:     return ease_back_out(t);
        case AGENTITE_EASING_BOUNCE_OUT:   return ease_bounce_out(t);
        default:                           return t;
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_Transition *agentite_transition_create(Agentite_ShaderSystem *shader_system,
                                                 SDL_Window *window,
                                                 const Agentite_TransitionConfig *config)
{
    if (!shader_system) {
        agentite_set_error("Transition: Shader system is NULL");
        return NULL;
    }

    Agentite_TransitionConfig default_config = AGENTITE_TRANSITION_CONFIG_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    int width = config->width;
    int height = config->height;

    if (width == 0 || height == 0) {
        if (window) {
            SDL_GetWindowSize(window, &width, &height);
        } else {
            agentite_set_error("Transition: Window required when size not specified");
            return NULL;
        }
    }

    Agentite_Transition *trans = (Agentite_Transition *)calloc(1, sizeof(*trans));
    if (!trans) {
        agentite_set_error("Transition: Failed to allocate");
        return NULL;
    }

    trans->shader_system = shader_system;
    /* Get GPU device from shader system - we need a way to access it */
    /* For now, store window dimensions */
    trans->width = width;
    trans->height = height;
    trans->config = *config;
    trans->state = AGENTITE_TRANSITION_IDLE;

    /* Get GPU device from shader system internals */
    /* Note: This requires accessing the shader system's GPU device */
    /* We'll create textures lazily when we have access to a command buffer */

    if (!create_shaders(trans)) {
        free(trans);
        return NULL;
    }

    return trans;
}

void agentite_transition_destroy(Agentite_Transition *trans)
{
    if (!trans) return;

    destroy_shaders(trans);
    destroy_render_targets(trans);

    free(trans);
}

bool agentite_transition_resize(Agentite_Transition *trans, int width, int height)
{
    if (!trans || width <= 0 || height <= 0) return false;

    if (trans->width == width && trans->height == height) {
        return true;
    }

    trans->width = width;
    trans->height = height;

    /* Destroy and recreate render targets with new size */
    destroy_render_targets(trans);

    /* Targets will be recreated on next capture */
    return true;
}

/* ============================================================================
 * Configuration Modification
 * ============================================================================ */

void agentite_transition_set_effect(Agentite_Transition *trans,
                                    Agentite_TransitionEffect effect)
{
    if (trans && effect < AGENTITE_TRANSITION_EFFECT_COUNT) {
        trans->config.effect = effect;
    }
}

void agentite_transition_set_easing(Agentite_Transition *trans,
                                    Agentite_TransitionEasing easing)
{
    if (trans && easing < AGENTITE_EASING_COUNT) {
        trans->config.easing = easing;
    }
}

void agentite_transition_set_duration(Agentite_Transition *trans, float duration)
{
    if (trans && duration > 0.0f) {
        trans->config.duration = duration;
    }
}

void agentite_transition_set_fade_color(Agentite_Transition *trans,
                                        float r, float g, float b, float a)
{
    if (trans) {
        trans->config.fade_color[0] = r;
        trans->config.fade_color[1] = g;
        trans->config.fade_color[2] = b;
        trans->config.fade_color[3] = a;
    }
}

void agentite_transition_set_callbacks(Agentite_Transition *trans,
                                       Agentite_TransitionCallback on_start,
                                       Agentite_TransitionCallback on_midpoint,
                                       Agentite_TransitionCallback on_complete,
                                       void *user_data)
{
    if (trans) {
        trans->config.on_start = on_start;
        trans->config.on_midpoint = on_midpoint;
        trans->config.on_complete = on_complete;
        trans->config.callback_user_data = user_data;
    }
}

/* ============================================================================
 * Transition Control
 * ============================================================================ */

bool agentite_transition_capture_source(Agentite_Transition *trans,
                                        SDL_GPUCommandBuffer *cmd,
                                        SDL_GPUTexture *texture)
{
    if (!trans || !cmd || !texture) {
        agentite_set_error("Transition: Invalid capture parameters");
        return false;
    }

    /* For now, just store reference - caller maintains texture lifetime */
    /* In a full implementation, we would copy the texture to our internal buffer */
    trans->source_texture = texture;
    trans->has_source = true;

    return true;
}

bool agentite_transition_start(Agentite_Transition *trans)
{
    if (!trans) return false;

    if (trans->state == AGENTITE_TRANSITION_RUNNING) {
        return false; /* Already running */
    }

    trans->state = AGENTITE_TRANSITION_RUNNING;
    trans->elapsed = 0.0f;
    trans->progress = 0.0f;
    trans->eased_progress = 0.0f;
    trans->midpoint_triggered = false;

    if (trans->config.on_start) {
        trans->config.on_start(trans, trans->config.callback_user_data);
    }

    return true;
}

bool agentite_transition_start_with_effect(Agentite_Transition *trans,
                                           Agentite_TransitionEffect effect)
{
    if (!trans) return false;

    trans->config.effect = effect;
    return agentite_transition_start(trans);
}

void agentite_transition_cancel(Agentite_Transition *trans)
{
    if (trans) {
        trans->state = AGENTITE_TRANSITION_IDLE;
        trans->elapsed = 0.0f;
        trans->progress = 0.0f;
        trans->eased_progress = 0.0f;
    }
}

void agentite_transition_update(Agentite_Transition *trans, float delta_time)
{
    if (!trans || trans->state != AGENTITE_TRANSITION_RUNNING) {
        return;
    }

    trans->elapsed += delta_time;

    /* Calculate progress */
    if (trans->config.duration > 0.0f) {
        trans->progress = trans->elapsed / trans->config.duration;
    } else {
        trans->progress = 1.0f;
    }

    /* Clamp and apply easing */
    if (trans->progress >= 1.0f) {
        trans->progress = 1.0f;
        trans->eased_progress = 1.0f;
        trans->state = AGENTITE_TRANSITION_COMPLETE;

        if (trans->config.on_complete) {
            trans->config.on_complete(trans, trans->config.callback_user_data);
        }
    } else {
        trans->eased_progress = agentite_transition_apply_easing(
            trans->config.easing, trans->progress);
    }

    /* Trigger midpoint callback */
    if (!trans->midpoint_triggered && trans->progress >= 0.5f) {
        trans->midpoint_triggered = true;
        if (trans->config.on_midpoint) {
            trans->config.on_midpoint(trans, trans->config.callback_user_data);
        }
    }
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

void agentite_transition_render(Agentite_Transition *trans,
                                SDL_GPUCommandBuffer *cmd,
                                SDL_GPURenderPass *pass,
                                SDL_GPUTexture *dest)
{
    if (!trans || !cmd || !pass) return;

    /* If not active, just return - caller should render scene normally */
    if (trans->state == AGENTITE_TRANSITION_IDLE) {
        return;
    }

    agentite_transition_render_blend(trans, cmd, pass,
                                     trans->source_texture, dest,
                                     trans->eased_progress);
}

void agentite_transition_render_blend(Agentite_Transition *trans,
                                      SDL_GPUCommandBuffer *cmd,
                                      SDL_GPURenderPass *pass,
                                      SDL_GPUTexture *source,
                                      SDL_GPUTexture *dest,
                                      float progress)
{
    if (!trans || !cmd || !pass) return;

    Agentite_TransitionEffect effect = trans->config.effect;

    /* Handle NONE effect - just render destination */
    if (effect == AGENTITE_TRANSITION_NONE) {
        return;
    }

    /* Get appropriate shader */
    Agentite_Shader *shader = get_shader_for_effect(trans, effect);
    if (!shader) {
        /* Fall back: hard cut at 50% progress */
        return;
    }

    /* Build parameters for the effect */
    uint8_t params[MAX_PARAM_SIZE];
    size_t param_size = 0;
    build_params_for_effect(trans, effect, progress, params, &param_size);

    /* Pixelate uses single-texture builtin shader with different param layout */
    if (effect == AGENTITE_TRANSITION_PIXELATE) {
        /* Builtin pixelate expects: { float pixel_size; float3 _pad; } */
        float t = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
        float pixel_size = 1.0f + t * (trans->config.pixel_size - 1.0f);
        float pixelate_params[4] = { pixel_size, 0, 0, 0 };

        agentite_shader_draw_fullscreen(trans->shader_system, cmd, pass,
                                        shader,
                                        progress < 0.5f ? source : dest,
                                        pixelate_params, sizeof(pixelate_params));
        return;
    }

    /* Fade uses brightness shader to fade through black */
    if (effect == AGENTITE_TRANSITION_FADE) {
        Agentite_Shader *brightness = agentite_shader_get_builtin(trans->shader_system,
                                                                   AGENTITE_SHADER_BRIGHTNESS);
        if (brightness) {
            /* First half: fade source to black, second half: fade dest from black */
            float brightness_amount;
            SDL_GPUTexture *scene;

            if (progress < 0.5f) {
                /* Fading out source: brightness goes from 0 to -1 */
                brightness_amount = -progress * 2.0f;
                scene = source;
            } else {
                /* Fading in dest: brightness goes from -1 to 0 */
                brightness_amount = -1.0f + (progress - 0.5f) * 2.0f;
                scene = dest;
            }

            float brightness_params[4] = { brightness_amount, 0, 0, 0 };
            agentite_shader_draw_fullscreen(trans->shader_system, cmd, pass,
                                            brightness, scene,
                                            brightness_params, sizeof(brightness_params));
            return;
        }
    }

    /* All other transitions use two-texture blend shaders */
    agentite_shader_draw_fullscreen_two_texture(trans->shader_system, cmd, pass,
                                                 shader, source, dest,
                                                 params, param_size);
}

/* ============================================================================
 * State Queries
 * ============================================================================ */

bool agentite_transition_is_active(const Agentite_Transition *trans)
{
    return trans && (trans->state == AGENTITE_TRANSITION_RUNNING ||
                     trans->state == AGENTITE_TRANSITION_COMPLETE);
}

bool agentite_transition_is_running(const Agentite_Transition *trans)
{
    return trans && trans->state == AGENTITE_TRANSITION_RUNNING;
}

bool agentite_transition_is_complete(Agentite_Transition *trans)
{
    if (!trans) return false;

    if (trans->state == AGENTITE_TRANSITION_COMPLETE) {
        /* Reset to idle after querying */
        trans->state = AGENTITE_TRANSITION_IDLE;
        trans->has_source = false;
        return true;
    }
    return false;
}

Agentite_TransitionState agentite_transition_get_state(const Agentite_Transition *trans)
{
    return trans ? trans->state : AGENTITE_TRANSITION_IDLE;
}

float agentite_transition_get_progress(const Agentite_Transition *trans)
{
    return trans ? trans->progress : 0.0f;
}

float agentite_transition_get_eased_progress(const Agentite_Transition *trans)
{
    return trans ? trans->eased_progress : 0.0f;
}

float agentite_transition_get_remaining(const Agentite_Transition *trans)
{
    if (!trans || trans->state != AGENTITE_TRANSITION_RUNNING) {
        return 0.0f;
    }
    return trans->config.duration - trans->elapsed;
}

bool agentite_transition_past_midpoint(const Agentite_Transition *trans)
{
    return trans && trans->progress >= 0.5f;
}

/* ============================================================================
 * Render Target Access
 * ============================================================================ */

SDL_GPUTexture *agentite_transition_get_source_texture(const Agentite_Transition *trans)
{
    return trans ? trans->source_texture : NULL;
}

SDL_GPUTexture *agentite_transition_get_render_target(const Agentite_Transition *trans)
{
    return trans ? trans->render_target : NULL;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

const char *agentite_transition_effect_name(Agentite_TransitionEffect effect)
{
    static const char *names[] = {
        "none",
        "fade",
        "crossfade",
        "wipe_left",
        "wipe_right",
        "wipe_up",
        "wipe_down",
        "wipe_diagonal",
        "dissolve",
        "pixelate",
        "slide_left",
        "slide_right",
        "slide_up",
        "slide_down",
        "push_left",
        "push_right",
        "push_up",
        "push_down",
        "circle_open",
        "circle_close"
    };

    if (effect < 0 || effect >= AGENTITE_TRANSITION_EFFECT_COUNT) {
        return "unknown";
    }
    return names[effect];
}

const char *agentite_transition_easing_name(Agentite_TransitionEasing easing)
{
    static const char *names[] = {
        "linear",
        "ease_in",
        "ease_out",
        "ease_in_out",
        "quad_in",
        "quad_out",
        "quad_in_out",
        "cubic_in",
        "cubic_out",
        "cubic_in_out",
        "back_in",
        "back_out",
        "bounce_out"
    };

    if (easing < 0 || easing >= AGENTITE_EASING_COUNT) {
        return "unknown";
    }
    return names[easing];
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static void destroy_render_targets(Agentite_Transition *trans)
{
    if (!trans || !trans->gpu) return;

    if (trans->source_texture) {
        /* Only release if we own it */
        trans->source_texture = NULL;
    }

    if (trans->render_target) {
        SDL_ReleaseGPUTexture(trans->gpu, trans->render_target);
        trans->render_target = NULL;
    }
}

static bool create_shaders(Agentite_Transition *trans)
{
    /* Get pixelate from built-ins (single texture effect) */
    trans->pixelate_shader = agentite_shader_get_builtin(trans->shader_system,
                                                          AGENTITE_SHADER_PIXELATE);

    /* Shader desc for two-texture transition shaders */
    Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
    desc.num_vertex_uniforms = 1;     /* Projection matrix */
    desc.num_fragment_uniforms = 1;   /* Transition params */
    desc.num_fragment_samplers = 2;   /* Source + dest textures */
    desc.blend_mode = AGENTITE_BLEND_NONE;

    SDL_GPUShaderFormat formats = agentite_shader_get_formats(trans->shader_system);

    if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        /* Load SPIR-V shaders (Vulkan/Linux/Windows) */
        trans->crossfade_shader = agentite_shader_load_spirv(trans->shader_system,
            "assets/shaders/transitions/transition.vert.spv",
            "assets/shaders/transitions/crossfade.frag.spv", &desc);

        trans->wipe_shader = agentite_shader_load_spirv(trans->shader_system,
            "assets/shaders/transitions/transition.vert.spv",
            "assets/shaders/transitions/wipe.frag.spv", &desc);

        trans->circle_shader = agentite_shader_load_spirv(trans->shader_system,
            "assets/shaders/transitions/transition.vert.spv",
            "assets/shaders/transitions/circle.frag.spv", &desc);

        trans->slide_shader = agentite_shader_load_spirv(trans->shader_system,
            "assets/shaders/transitions/transition.vert.spv",
            "assets/shaders/transitions/slide.frag.spv", &desc);

        trans->dissolve_shader = agentite_shader_load_spirv(trans->shader_system,
            "assets/shaders/transitions/transition.vert.spv",
            "assets/shaders/transitions/dissolve.frag.spv", &desc);

        SDL_Log("Transition: Loaded SPIR-V shaders");
    } else if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Load MSL shaders (Metal/macOS/iOS) */
        char combined[8192];

        desc.vertex_entry = "transition_vertex";

        /* Crossfade */
        snprintf(combined, sizeof(combined), "%s\n%s",
                 transition_vertex_msl, transition_crossfade_msl);
        desc.fragment_entry = "crossfade_fragment";
        trans->crossfade_shader = agentite_shader_load_msl(trans->shader_system,
                                                            combined, &desc);

        /* Wipe */
        snprintf(combined, sizeof(combined), "%s\n%s",
                 transition_vertex_msl, transition_wipe_msl);
        desc.fragment_entry = "wipe_fragment";
        trans->wipe_shader = agentite_shader_load_msl(trans->shader_system,
                                                       combined, &desc);

        /* Circle */
        snprintf(combined, sizeof(combined), "%s\n%s",
                 transition_vertex_msl, transition_circle_msl);
        desc.fragment_entry = "circle_fragment";
        trans->circle_shader = agentite_shader_load_msl(trans->shader_system,
                                                         combined, &desc);

        /* Slide */
        snprintf(combined, sizeof(combined), "%s\n%s",
                 transition_vertex_msl, transition_slide_msl);
        desc.fragment_entry = "slide_fragment";
        trans->slide_shader = agentite_shader_load_msl(trans->shader_system,
                                                        combined, &desc);

        /* Dissolve */
        snprintf(combined, sizeof(combined), "%s\n%s",
                 transition_vertex_msl, transition_dissolve_msl);
        desc.fragment_entry = "dissolve_fragment";
        trans->dissolve_shader = agentite_shader_load_msl(trans->shader_system,
                                                           combined, &desc);

        SDL_Log("Transition: Loaded MSL shaders");
    }

    /* Log shader availability */
    SDL_Log("Transition shaders: crossfade=%s wipe=%s circle=%s slide=%s dissolve=%s",
            trans->crossfade_shader ? "OK" : "N/A",
            trans->wipe_shader ? "OK" : "N/A",
            trans->circle_shader ? "OK" : "N/A",
            trans->slide_shader ? "OK" : "N/A",
            trans->dissolve_shader ? "OK" : "N/A");

    return true;
}

static void destroy_shaders(Agentite_Transition *trans)
{
    /* Built-in shaders (pixelate) are not destroyed */

    /* Destroy custom transition shaders */
    if (trans->crossfade_shader) {
        agentite_shader_destroy(trans->shader_system, trans->crossfade_shader);
        trans->crossfade_shader = NULL;
    }
    if (trans->wipe_shader) {
        agentite_shader_destroy(trans->shader_system, trans->wipe_shader);
        trans->wipe_shader = NULL;
    }
    if (trans->circle_shader) {
        agentite_shader_destroy(trans->shader_system, trans->circle_shader);
        trans->circle_shader = NULL;
    }
    if (trans->slide_shader) {
        agentite_shader_destroy(trans->shader_system, trans->slide_shader);
        trans->slide_shader = NULL;
    }
    if (trans->dissolve_shader) {
        agentite_shader_destroy(trans->shader_system, trans->dissolve_shader);
        trans->dissolve_shader = NULL;
    }
}

static Agentite_Shader *get_shader_for_effect(Agentite_Transition *trans,
                                               Agentite_TransitionEffect effect)
{
    switch (effect) {
        case AGENTITE_TRANSITION_FADE:
        case AGENTITE_TRANSITION_CROSSFADE:
            return trans->crossfade_shader;

        case AGENTITE_TRANSITION_WIPE_LEFT:
        case AGENTITE_TRANSITION_WIPE_RIGHT:
        case AGENTITE_TRANSITION_WIPE_UP:
        case AGENTITE_TRANSITION_WIPE_DOWN:
        case AGENTITE_TRANSITION_WIPE_DIAGONAL:
            return trans->wipe_shader;

        case AGENTITE_TRANSITION_DISSOLVE:
            return trans->dissolve_shader;

        case AGENTITE_TRANSITION_PIXELATE:
            return trans->pixelate_shader;

        case AGENTITE_TRANSITION_SLIDE_LEFT:
        case AGENTITE_TRANSITION_SLIDE_RIGHT:
        case AGENTITE_TRANSITION_SLIDE_UP:
        case AGENTITE_TRANSITION_SLIDE_DOWN:
        case AGENTITE_TRANSITION_PUSH_LEFT:
        case AGENTITE_TRANSITION_PUSH_RIGHT:
        case AGENTITE_TRANSITION_PUSH_UP:
        case AGENTITE_TRANSITION_PUSH_DOWN:
            return trans->slide_shader;

        case AGENTITE_TRANSITION_CIRCLE_OPEN:
        case AGENTITE_TRANSITION_CIRCLE_CLOSE:
            return trans->circle_shader;

        default:
            return NULL;
    }
}

static void build_params_for_effect(Agentite_Transition *trans,
                                    Agentite_TransitionEffect effect,
                                    float progress,
                                    void *out_params,
                                    size_t *out_size)
{
    memset(out_params, 0, MAX_PARAM_SIZE);
    *out_size = 16; /* Default to 16-byte aligned minimum */

    switch (effect) {
        case AGENTITE_TRANSITION_FADE: {
            TransitionParams_Fade *p = (TransitionParams_Fade *)out_params;
            /* Fade through color: first half fades to color, second half fades from color */
            float t = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
            p->color[0] = trans->config.fade_color[0];
            p->color[1] = trans->config.fade_color[1];
            p->color[2] = trans->config.fade_color[2];
            p->color[3] = t * trans->config.fade_color[3];
            *out_size = sizeof(TransitionParams_Fade);
            break;
        }

        case AGENTITE_TRANSITION_CROSSFADE: {
            TransitionParams_Crossfade *p = (TransitionParams_Crossfade *)out_params;
            p->progress = progress;
            p->softness = trans->config.edge_softness;
            *out_size = sizeof(TransitionParams_Crossfade);
            break;
        }

        case AGENTITE_TRANSITION_WIPE_LEFT:
        case AGENTITE_TRANSITION_WIPE_RIGHT:
        case AGENTITE_TRANSITION_WIPE_UP:
        case AGENTITE_TRANSITION_WIPE_DOWN:
        case AGENTITE_TRANSITION_WIPE_DIAGONAL: {
            TransitionParams_Wipe *p = (TransitionParams_Wipe *)out_params;
            p->progress = progress;
            p->softness = trans->config.edge_softness;
            switch (effect) {
                case AGENTITE_TRANSITION_WIPE_LEFT:    p->direction = 0.0f; break;
                case AGENTITE_TRANSITION_WIPE_RIGHT:   p->direction = 1.0f; break;
                case AGENTITE_TRANSITION_WIPE_UP:      p->direction = 2.0f; break;
                case AGENTITE_TRANSITION_WIPE_DOWN:    p->direction = 3.0f; break;
                case AGENTITE_TRANSITION_WIPE_DIAGONAL: p->direction = 4.0f; break;
                default: break;
            }
            *out_size = sizeof(TransitionParams_Wipe);
            break;
        }

        case AGENTITE_TRANSITION_DISSOLVE: {
            TransitionParams_Dissolve *p = (TransitionParams_Dissolve *)out_params;
            p->progress = progress;
            p->edge_width = trans->config.edge_softness;
            *out_size = sizeof(TransitionParams_Dissolve);
            break;
        }

        case AGENTITE_TRANSITION_PIXELATE: {
            TransitionParams_Pixelate *p = (TransitionParams_Pixelate *)out_params;
            /* Pixelate up then down */
            float t = progress < 0.5f ? progress * 2.0f : (1.0f - progress) * 2.0f;
            p->progress = progress;
            p->pixel_size = 1.0f + t * (trans->config.pixel_size - 1.0f);
            *out_size = sizeof(TransitionParams_Pixelate);
            break;
        }

        case AGENTITE_TRANSITION_SLIDE_LEFT:
        case AGENTITE_TRANSITION_SLIDE_RIGHT:
        case AGENTITE_TRANSITION_SLIDE_UP:
        case AGENTITE_TRANSITION_SLIDE_DOWN:
        case AGENTITE_TRANSITION_PUSH_LEFT:
        case AGENTITE_TRANSITION_PUSH_RIGHT:
        case AGENTITE_TRANSITION_PUSH_UP:
        case AGENTITE_TRANSITION_PUSH_DOWN: {
            TransitionParams_Slide *p = (TransitionParams_Slide *)out_params;
            p->progress = progress;

            bool is_push = effect >= AGENTITE_TRANSITION_PUSH_LEFT &&
                          effect <= AGENTITE_TRANSITION_PUSH_DOWN;
            p->is_push = is_push ? 1.0f : 0.0f;

            switch (effect) {
                case AGENTITE_TRANSITION_SLIDE_LEFT:
                case AGENTITE_TRANSITION_PUSH_LEFT:  p->direction = 0.0f; break;
                case AGENTITE_TRANSITION_SLIDE_RIGHT:
                case AGENTITE_TRANSITION_PUSH_RIGHT: p->direction = 1.0f; break;
                case AGENTITE_TRANSITION_SLIDE_UP:
                case AGENTITE_TRANSITION_PUSH_UP:    p->direction = 2.0f; break;
                case AGENTITE_TRANSITION_SLIDE_DOWN:
                case AGENTITE_TRANSITION_PUSH_DOWN:  p->direction = 3.0f; break;
                default: break;
            }
            *out_size = sizeof(TransitionParams_Slide);
            break;
        }

        case AGENTITE_TRANSITION_CIRCLE_OPEN:
        case AGENTITE_TRANSITION_CIRCLE_CLOSE: {
            TransitionParams_Circle *p = (TransitionParams_Circle *)out_params;
            p->progress = progress;
            p->center_x = trans->config.circle_center_x;
            p->center_y = trans->config.circle_center_y;
            p->is_open = (effect == AGENTITE_TRANSITION_CIRCLE_OPEN) ? 1.0f : 0.0f;
            *out_size = sizeof(TransitionParams_Circle);
            break;
        }

        default:
            break;
    }
}
