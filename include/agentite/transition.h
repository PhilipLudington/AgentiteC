/*
 * Agentite Screen Transition System
 *
 * Provides smooth visual transitions between game screens/scenes with support
 * for various effects including fade, wipe, dissolve, and crossfade.
 *
 * Basic Usage:
 *   // Create transition system
 *   Agentite_TransitionConfig config = AGENTITE_TRANSITION_CONFIG_DEFAULT;
 *   config.duration = 0.5f;
 *   config.effect = AGENTITE_TRANSITION_FADE;
 *   Agentite_Transition *trans = agentite_transition_create(ss, window, &config);
 *
 *   // Capture outgoing scene to texture before scene change
 *   agentite_transition_capture_source(trans, cmd, source_texture);
 *
 *   // ... change scene ...
 *
 *   // Start transition (will render new scene during transition)
 *   agentite_transition_start(trans);
 *
 *   // In render loop:
 *   if (agentite_transition_is_active(trans)) {
 *       agentite_transition_update(trans, delta_time);
 *       agentite_transition_render(trans, cmd, pass, current_scene_texture);
 *   }
 *
 *   // Cleanup
 *   agentite_transition_destroy(trans);
 *
 * Thread Safety:
 *   - All functions are NOT thread-safe (main thread only)
 *   - All GPU operations must occur on the rendering thread
 */

#ifndef AGENTITE_TRANSITION_H
#define AGENTITE_TRANSITION_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_Transition Agentite_Transition;
typedef struct Agentite_ShaderSystem Agentite_ShaderSystem;

/* ============================================================================
 * Transition Types
 * ============================================================================ */

/**
 * Built-in transition effects.
 */
typedef enum Agentite_TransitionEffect {
    AGENTITE_TRANSITION_NONE = 0,

    /* Fade effects */
    AGENTITE_TRANSITION_FADE,           /* Fade through solid color */
    AGENTITE_TRANSITION_CROSSFADE,      /* Cross-dissolve between scenes */

    /* Wipe effects */
    AGENTITE_TRANSITION_WIPE_LEFT,      /* Wipe from right to left */
    AGENTITE_TRANSITION_WIPE_RIGHT,     /* Wipe from left to right */
    AGENTITE_TRANSITION_WIPE_UP,        /* Wipe from bottom to top */
    AGENTITE_TRANSITION_WIPE_DOWN,      /* Wipe from top to bottom */
    AGENTITE_TRANSITION_WIPE_DIAGONAL,  /* Diagonal wipe */

    /* Dissolve effects */
    AGENTITE_TRANSITION_DISSOLVE,       /* Noise-based dissolve */
    AGENTITE_TRANSITION_PIXELATE,       /* Pixelate out/in */

    /* Slide effects */
    AGENTITE_TRANSITION_SLIDE_LEFT,     /* Slide new scene from right */
    AGENTITE_TRANSITION_SLIDE_RIGHT,    /* Slide new scene from left */
    AGENTITE_TRANSITION_SLIDE_UP,       /* Slide new scene from bottom */
    AGENTITE_TRANSITION_SLIDE_DOWN,     /* Slide new scene from top */

    /* Push effects (old scene pushes out) */
    AGENTITE_TRANSITION_PUSH_LEFT,      /* Push old scene left */
    AGENTITE_TRANSITION_PUSH_RIGHT,     /* Push old scene right */
    AGENTITE_TRANSITION_PUSH_UP,        /* Push old scene up */
    AGENTITE_TRANSITION_PUSH_DOWN,      /* Push old scene down */

    /* Special effects */
    AGENTITE_TRANSITION_CIRCLE_OPEN,    /* Iris/circle open */
    AGENTITE_TRANSITION_CIRCLE_CLOSE,   /* Iris/circle close */

    AGENTITE_TRANSITION_EFFECT_COUNT
} Agentite_TransitionEffect;

/**
 * Easing functions for transition timing.
 */
typedef enum Agentite_TransitionEasing {
    AGENTITE_EASING_LINEAR = 0,
    AGENTITE_EASING_EASE_IN,            /* Slow start */
    AGENTITE_EASING_EASE_OUT,           /* Slow end */
    AGENTITE_EASING_EASE_IN_OUT,        /* Slow start and end */
    AGENTITE_EASING_QUAD_IN,            /* Quadratic ease in */
    AGENTITE_EASING_QUAD_OUT,           /* Quadratic ease out */
    AGENTITE_EASING_QUAD_IN_OUT,        /* Quadratic ease in-out */
    AGENTITE_EASING_CUBIC_IN,           /* Cubic ease in */
    AGENTITE_EASING_CUBIC_OUT,          /* Cubic ease out */
    AGENTITE_EASING_CUBIC_IN_OUT,       /* Cubic ease in-out */
    AGENTITE_EASING_BACK_IN,            /* Overshoot at start */
    AGENTITE_EASING_BACK_OUT,           /* Overshoot at end */
    AGENTITE_EASING_BOUNCE_OUT,         /* Bounce effect at end */

    AGENTITE_EASING_COUNT
} Agentite_TransitionEasing;

/**
 * Transition state.
 */
typedef enum Agentite_TransitionState {
    AGENTITE_TRANSITION_IDLE = 0,       /* Not transitioning */
    AGENTITE_TRANSITION_RUNNING,        /* Transition in progress */
    AGENTITE_TRANSITION_COMPLETE        /* Transition just finished */
} Agentite_TransitionState;

/**
 * Transition callback function type.
 *
 * @param trans     Transition that triggered the callback
 * @param user_data User-provided data
 */
typedef void (*Agentite_TransitionCallback)(Agentite_Transition *trans, void *user_data);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Transition configuration.
 */
typedef struct Agentite_TransitionConfig {
    /* Effect settings */
    Agentite_TransitionEffect effect;   /* Transition effect type */
    Agentite_TransitionEasing easing;   /* Timing easing function */
    float duration;                      /* Duration in seconds */

    /* Fade color (for FADE effect) */
    float fade_color[4];                 /* RGBA fade color (default: black) */

    /* Effect parameters */
    float edge_softness;                 /* Softness of wipe/dissolve edges (0-1) */
    float pixel_size;                    /* Max pixel size for pixelate (default: 16) */

    /* Circle transition center (0-1 normalized) */
    float circle_center_x;               /* Circle center X (default: 0.5) */
    float circle_center_y;               /* Circle center Y (default: 0.5) */

    /* Render target settings */
    int width;                           /* Render target width (0 = window) */
    int height;                          /* Render target height (0 = window) */
    SDL_GPUTextureFormat format;         /* Texture format */

    /* Callbacks */
    Agentite_TransitionCallback on_start;     /* Called when transition starts */
    Agentite_TransitionCallback on_midpoint;  /* Called at 50% (scene change point) */
    Agentite_TransitionCallback on_complete;  /* Called when transition completes */
    void *callback_user_data;                  /* User data for callbacks */
} Agentite_TransitionConfig;

/**
 * Default configuration.
 */
#define AGENTITE_TRANSITION_CONFIG_DEFAULT { \
    .effect = AGENTITE_TRANSITION_FADE,      \
    .easing = AGENTITE_EASING_EASE_IN_OUT,   \
    .duration = 0.5f,                         \
    .fade_color = { 0.0f, 0.0f, 0.0f, 1.0f }, \
    .edge_softness = 0.1f,                    \
    .pixel_size = 16.0f,                      \
    .circle_center_x = 0.5f,                  \
    .circle_center_y = 0.5f,                  \
    .width = 0,                               \
    .height = 0,                              \
    .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, \
    .on_start = NULL,                         \
    .on_midpoint = NULL,                      \
    .on_complete = NULL,                      \
    .callback_user_data = NULL                \
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create a transition system.
 *
 * Caller OWNS the returned pointer and MUST call agentite_transition_destroy().
 * Returns NULL on failure (check agentite_get_last_error()).
 *
 * @param shader_system Shader system for transition effects
 * @param window        Window for sizing (can be NULL if config specifies size)
 * @param config        Configuration (NULL for defaults)
 * @return New transition system, or NULL on failure
 */
Agentite_Transition *agentite_transition_create(Agentite_ShaderSystem *shader_system,
                                                 SDL_Window *window,
                                                 const Agentite_TransitionConfig *config);

/**
 * Destroy a transition system.
 * Accepts NULL safely.
 *
 * @param trans Transition to destroy
 */
void agentite_transition_destroy(Agentite_Transition *trans);

/**
 * Resize transition render targets.
 * Call when window resizes.
 *
 * @param trans  Transition system
 * @param width  New width
 * @param height New height
 * @return true on success
 */
bool agentite_transition_resize(Agentite_Transition *trans, int width, int height);

/* ============================================================================
 * Configuration Modification
 * ============================================================================ */

/**
 * Set transition effect.
 *
 * @param trans  Transition system
 * @param effect New effect type
 */
void agentite_transition_set_effect(Agentite_Transition *trans,
                                    Agentite_TransitionEffect effect);

/**
 * Set transition easing.
 *
 * @param trans  Transition system
 * @param easing New easing function
 */
void agentite_transition_set_easing(Agentite_Transition *trans,
                                    Agentite_TransitionEasing easing);

/**
 * Set transition duration.
 *
 * @param trans    Transition system
 * @param duration Duration in seconds
 */
void agentite_transition_set_duration(Agentite_Transition *trans, float duration);

/**
 * Set fade color (for FADE effect).
 *
 * @param trans Transition system
 * @param r     Red (0-1)
 * @param g     Green (0-1)
 * @param b     Blue (0-1)
 * @param a     Alpha (0-1)
 */
void agentite_transition_set_fade_color(Agentite_Transition *trans,
                                        float r, float g, float b, float a);

/**
 * Set callbacks.
 *
 * @param trans       Transition system
 * @param on_start    Start callback (or NULL)
 * @param on_midpoint Midpoint callback (or NULL)
 * @param on_complete Complete callback (or NULL)
 * @param user_data   User data for callbacks
 */
void agentite_transition_set_callbacks(Agentite_Transition *trans,
                                       Agentite_TransitionCallback on_start,
                                       Agentite_TransitionCallback on_midpoint,
                                       Agentite_TransitionCallback on_complete,
                                       void *user_data);

/* ============================================================================
 * Transition Control
 * ============================================================================ */

/**
 * Capture the current scene as the source (outgoing) scene.
 * Call this before changing scenes to preserve the old scene.
 *
 * @param trans   Transition system
 * @param cmd     Command buffer for copy operations
 * @param texture Source texture to capture (e.g., from post-process target)
 * @return true on success
 */
bool agentite_transition_capture_source(Agentite_Transition *trans,
                                        SDL_GPUCommandBuffer *cmd,
                                        SDL_GPUTexture *texture);

/**
 * Start the transition.
 * After calling this, use agentite_transition_render() each frame.
 *
 * @param trans Transition system
 * @return true if started, false if already transitioning
 */
bool agentite_transition_start(Agentite_Transition *trans);

/**
 * Start a transition with specific effect (convenience function).
 *
 * @param trans   Transition system
 * @param effect  Effect to use for this transition
 * @return true if started
 */
bool agentite_transition_start_with_effect(Agentite_Transition *trans,
                                           Agentite_TransitionEffect effect);

/**
 * Cancel an in-progress transition.
 * Immediately jumps to idle state.
 *
 * @param trans Transition system
 */
void agentite_transition_cancel(Agentite_Transition *trans);

/**
 * Update transition state.
 * Call once per frame while transitioning.
 *
 * @param trans      Transition system
 * @param delta_time Frame delta time in seconds
 */
void agentite_transition_update(Agentite_Transition *trans, float delta_time);

/* ============================================================================
 * Rendering
 * ============================================================================ */

/**
 * Render the transition effect.
 * Blends source (old scene) and destination (new scene) textures.
 *
 * @param trans  Transition system
 * @param cmd    Command buffer
 * @param pass   Render pass
 * @param dest   Destination texture (current/new scene)
 */
void agentite_transition_render(Agentite_Transition *trans,
                                SDL_GPUCommandBuffer *cmd,
                                SDL_GPURenderPass *pass,
                                SDL_GPUTexture *dest);

/**
 * Render transition directly from two textures.
 * Lower-level function for manual control.
 *
 * @param trans    Transition system
 * @param cmd      Command buffer
 * @param pass     Render pass
 * @param source   Source (outgoing) texture
 * @param dest     Destination (incoming) texture
 * @param progress Transition progress (0-1)
 */
void agentite_transition_render_blend(Agentite_Transition *trans,
                                      SDL_GPUCommandBuffer *cmd,
                                      SDL_GPURenderPass *pass,
                                      SDL_GPUTexture *source,
                                      SDL_GPUTexture *dest,
                                      float progress);

/* ============================================================================
 * State Queries
 * ============================================================================ */

/**
 * Check if transition is active (running or just completed).
 *
 * @param trans Transition system
 * @return true if transitioning
 */
bool agentite_transition_is_active(const Agentite_Transition *trans);

/**
 * Check if transition is running.
 *
 * @param trans Transition system
 * @return true if currently running
 */
bool agentite_transition_is_running(const Agentite_Transition *trans);

/**
 * Check if transition just completed this frame.
 * Resets to false after querying.
 *
 * @param trans Transition system
 * @return true if just completed
 */
bool agentite_transition_is_complete(Agentite_Transition *trans);

/**
 * Get current transition state.
 *
 * @param trans Transition system
 * @return Current state
 */
Agentite_TransitionState agentite_transition_get_state(const Agentite_Transition *trans);

/**
 * Get current progress (0-1).
 *
 * @param trans Transition system
 * @return Progress from 0 (start) to 1 (complete)
 */
float agentite_transition_get_progress(const Agentite_Transition *trans);

/**
 * Get eased progress (0-1, with easing applied).
 *
 * @param trans Transition system
 * @return Eased progress
 */
float agentite_transition_get_eased_progress(const Agentite_Transition *trans);

/**
 * Get remaining time in seconds.
 *
 * @param trans Transition system
 * @return Remaining time, or 0 if not active
 */
float agentite_transition_get_remaining(const Agentite_Transition *trans);

/**
 * Check if transition has passed the midpoint.
 * Useful for knowing when to switch scenes.
 *
 * @param trans Transition system
 * @return true if past 50% progress
 */
bool agentite_transition_past_midpoint(const Agentite_Transition *trans);

/* ============================================================================
 * Render Target Access
 * ============================================================================ */

/**
 * Get the source (outgoing scene) texture.
 * Valid after agentite_transition_capture_source().
 *
 * @param trans Transition system
 * @return Source texture, or NULL
 */
SDL_GPUTexture *agentite_transition_get_source_texture(const Agentite_Transition *trans);

/**
 * Get a render target for capturing scenes.
 * Use this for rendering scenes to a texture before transition.
 *
 * @param trans Transition system
 * @return Render target texture
 */
SDL_GPUTexture *agentite_transition_get_render_target(const Agentite_Transition *trans);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Apply an easing function to a linear value.
 *
 * @param easing Easing function
 * @param t      Linear value (0-1)
 * @return Eased value
 */
float agentite_transition_apply_easing(Agentite_TransitionEasing easing, float t);

/**
 * Get name of a transition effect.
 *
 * @param effect Effect type
 * @return Effect name string
 */
const char *agentite_transition_effect_name(Agentite_TransitionEffect effect);

/**
 * Get name of an easing function.
 *
 * @param easing Easing type
 * @return Easing name string
 */
const char *agentite_transition_easing_name(Agentite_TransitionEasing easing);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_TRANSITION_H */
