/*
 * Agentite Particle System
 *
 * High-performance particle emitter system for visual effects like explosions,
 * smoke, fire, rain, and other particle-based effects.
 *
 * Usage:
 *   // Create particle system with default config
 *   Agentite_ParticleSystemConfig config = AGENTITE_PARTICLE_SYSTEM_DEFAULT;
 *   Agentite_ParticleSystem *ps = agentite_particle_system_create(&config);
 *
 *   // Create an emitter
 *   Agentite_ParticleEmitterConfig emitter_cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
 *   emitter_cfg.shape = AGENTITE_EMITTER_CIRCLE;
 *   emitter_cfg.radius = 20.0f;
 *   emitter_cfg.emission_rate = 100.0f;  // particles per second
 *   emitter_cfg.particle.lifetime_min = 1.0f;
 *   emitter_cfg.particle.lifetime_max = 2.0f;
 *   emitter_cfg.particle.start_color = (Agentite_Color){1.0f, 0.5f, 0.0f, 1.0f};
 *   emitter_cfg.particle.end_color = (Agentite_Color){1.0f, 0.0f, 0.0f, 0.0f};
 *   Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &emitter_cfg);
 *
 *   // Set position and start emitting
 *   agentite_particle_emitter_set_position(emitter, 400.0f, 300.0f);
 *   agentite_particle_emitter_start(emitter);
 *
 *   // Each frame:
 *   agentite_particle_system_update(ps, delta_time);
 *   agentite_sprite_begin(sr, cmd);
 *   agentite_particle_system_draw(ps, sr);
 *   agentite_sprite_upload(sr, cmd);
 *   // ... begin render pass ...
 *   agentite_sprite_render(sr, cmd, pass);
 *
 *   // Cleanup
 *   agentite_particle_emitter_destroy(emitter);
 *   agentite_particle_system_destroy(ps);
 */

#ifndef AGENTITE_PARTICLE_H
#define AGENTITE_PARTICLE_H

#include "sprite.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_ParticleSystem Agentite_ParticleSystem;
typedef struct Agentite_ParticleEmitter Agentite_ParticleEmitter;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/* Emitter shape for particle spawn distribution */
typedef enum Agentite_EmitterShape {
    AGENTITE_EMITTER_POINT,      /* Emit from a single point */
    AGENTITE_EMITTER_LINE,       /* Emit along a line segment */
    AGENTITE_EMITTER_CIRCLE,     /* Emit within a circle */
    AGENTITE_EMITTER_CIRCLE_EDGE,/* Emit from circle perimeter only */
    AGENTITE_EMITTER_RECTANGLE,  /* Emit within a rectangle */
    AGENTITE_EMITTER_RECTANGLE_EDGE /* Emit from rectangle perimeter only */
} Agentite_EmitterShape;

/* Emission pattern */
typedef enum Agentite_EmissionMode {
    AGENTITE_EMISSION_CONTINUOUS, /* Emit particles continuously at emission_rate */
    AGENTITE_EMISSION_BURST,      /* Emit burst_count particles at once */
    AGENTITE_EMISSION_TIMED       /* Emit for duration then stop */
} Agentite_EmissionMode;

/* Coordinate space for particles */
typedef enum Agentite_ParticleSpace {
    AGENTITE_PARTICLE_WORLD,     /* Particles move in world space (default) */
    AGENTITE_PARTICLE_LOCAL      /* Particles move relative to emitter */
} Agentite_ParticleSpace;

/* Blend mode for particle rendering */
typedef enum Agentite_ParticleBlend {
    AGENTITE_BLEND_ALPHA,        /* Standard alpha blending (default) */
    AGENTITE_BLEND_ADDITIVE,     /* Additive blending (for fire, glow) */
    AGENTITE_BLEND_MULTIPLY      /* Multiply blending (for shadows) */
} Agentite_ParticleBlend;

/* Easing function for interpolation */
typedef enum Agentite_EaseFunc {
    AGENTITE_EASE_LINEAR,        /* Linear interpolation (default) */
    AGENTITE_EASE_IN_QUAD,       /* Quadratic ease in */
    AGENTITE_EASE_OUT_QUAD,      /* Quadratic ease out */
    AGENTITE_EASE_IN_OUT_QUAD,   /* Quadratic ease in/out */
    AGENTITE_EASE_IN_CUBIC,      /* Cubic ease in */
    AGENTITE_EASE_OUT_CUBIC,     /* Cubic ease out */
    AGENTITE_EASE_IN_OUT_CUBIC,  /* Cubic ease in/out */
    AGENTITE_EASE_IN_EXPO,       /* Exponential ease in */
    AGENTITE_EASE_OUT_EXPO,      /* Exponential ease out */
    AGENTITE_EASE_IN_OUT_EXPO    /* Exponential ease in/out */
} Agentite_EaseFunc;

/* ============================================================================
 * Data Types
 * ============================================================================ */

/* RGBA color (0.0-1.0 range) */
typedef struct Agentite_Color {
    float r, g, b, a;
} Agentite_Color;

/* 2D vector */
typedef struct Agentite_Vec2 {
    float x, y;
} Agentite_Vec2;

/* Range for randomized values */
typedef struct Agentite_Range {
    float min;
    float max;
} Agentite_Range;

/* Color range for randomized start colors */
typedef struct Agentite_ColorRange {
    Agentite_Color min;
    Agentite_Color max;
} Agentite_ColorRange;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/* Individual particle properties configuration */
typedef struct Agentite_ParticleConfig {
    /* Lifetime */
    float lifetime_min;          /* Minimum lifetime in seconds */
    float lifetime_max;          /* Maximum lifetime in seconds */

    /* Initial velocity */
    float speed_min;             /* Minimum initial speed */
    float speed_max;             /* Maximum initial speed */
    float direction_min;         /* Minimum angle in degrees (0 = right, 90 = up) */
    float direction_max;         /* Maximum angle in degrees */
    float spread;                /* Cone spread in degrees (0 = no spread) */

    /* Acceleration and forces */
    Agentite_Vec2 acceleration;  /* Constant acceleration (e.g., wind) */
    float gravity;               /* Gravity strength (positive = down) */
    float drag;                  /* Air resistance (0 = none, 1 = full stop) */

    /* Size */
    float start_size_min;        /* Minimum starting size */
    float start_size_max;        /* Maximum starting size */
    float end_size_min;          /* Minimum ending size */
    float end_size_max;          /* Maximum ending size */
    Agentite_EaseFunc size_ease; /* Easing for size interpolation */

    /* Color */
    Agentite_Color start_color;  /* Starting color */
    Agentite_Color end_color;    /* Ending color (alpha 0 = fade out) */
    Agentite_EaseFunc color_ease;/* Easing for color interpolation */
    bool randomize_start_color;  /* If true, randomize between start_color and start_color_alt */
    Agentite_Color start_color_alt; /* Alternative start color for randomization */

    /* Rotation */
    float start_rotation_min;    /* Minimum starting rotation in degrees */
    float start_rotation_max;    /* Maximum starting rotation in degrees */
    float angular_velocity_min;  /* Minimum rotation speed (deg/sec) */
    float angular_velocity_max;  /* Maximum rotation speed (deg/sec) */

    /* Texture animation (for animated particles) */
    uint32_t frame_count;        /* Number of animation frames (1 = static) */
    float frame_rate;            /* Frames per second for animation */
    bool loop_animation;         /* Loop animation or play once */
    bool random_start_frame;     /* Start at random frame */
} Agentite_ParticleConfig;

/* Default particle configuration */
#define AGENTITE_PARTICLE_CONFIG_DEFAULT { \
    .lifetime_min = 1.0f, \
    .lifetime_max = 1.0f, \
    .speed_min = 50.0f, \
    .speed_max = 100.0f, \
    .direction_min = 0.0f, \
    .direction_max = 360.0f, \
    .spread = 0.0f, \
    .acceleration = {0.0f, 0.0f}, \
    .gravity = 0.0f, \
    .drag = 0.0f, \
    .start_size_min = 8.0f, \
    .start_size_max = 8.0f, \
    .end_size_min = 8.0f, \
    .end_size_max = 8.0f, \
    .size_ease = AGENTITE_EASE_LINEAR, \
    .start_color = {1.0f, 1.0f, 1.0f, 1.0f}, \
    .end_color = {1.0f, 1.0f, 1.0f, 0.0f}, \
    .color_ease = AGENTITE_EASE_LINEAR, \
    .randomize_start_color = false, \
    .start_color_alt = {1.0f, 1.0f, 1.0f, 1.0f}, \
    .start_rotation_min = 0.0f, \
    .start_rotation_max = 0.0f, \
    .angular_velocity_min = 0.0f, \
    .angular_velocity_max = 0.0f, \
    .frame_count = 1, \
    .frame_rate = 10.0f, \
    .loop_animation = true, \
    .random_start_frame = false \
}

/* Emitter configuration */
typedef struct Agentite_ParticleEmitterConfig {
    /* Shape and size */
    Agentite_EmitterShape shape; /* Shape of emission area */
    float radius;                /* Radius for circle shapes */
    float width;                 /* Width for rectangle/line shapes */
    float height;                /* Height for rectangle shapes */
    Agentite_Vec2 line_end;      /* End point for line (start is position) */

    /* Emission settings */
    Agentite_EmissionMode mode;  /* Continuous, burst, or timed */
    float emission_rate;         /* Particles per second (for continuous/timed) */
    uint32_t burst_count;        /* Particles per burst (for burst mode) */
    float burst_interval;        /* Time between bursts (0 = manual trigger) */
    float duration;              /* Duration in seconds (for timed mode) */

    /* Particle behavior */
    Agentite_ParticleSpace space;/* World or local space */
    Agentite_ParticleBlend blend;/* Blend mode for rendering */
    Agentite_ParticleConfig particle; /* Particle properties */

    /* Texture */
    Agentite_Texture *texture;   /* Particle texture (NULL = default white) */
    Agentite_Sprite sprite;      /* Sprite region (if using sprite sheet) */
    bool use_sprite;             /* True to use sprite instead of full texture */

    /* Limits */
    uint32_t max_particles;      /* Max particles for this emitter (0 = system default) */
    bool prewarm;                /* Simulate particles at start */
} Agentite_ParticleEmitterConfig;

/* Default emitter configuration */
#define AGENTITE_PARTICLE_EMITTER_DEFAULT { \
    .shape = AGENTITE_EMITTER_POINT, \
    .radius = 0.0f, \
    .width = 0.0f, \
    .height = 0.0f, \
    .line_end = {0.0f, 0.0f}, \
    .mode = AGENTITE_EMISSION_CONTINUOUS, \
    .emission_rate = 10.0f, \
    .burst_count = 10, \
    .burst_interval = 0.0f, \
    .duration = 1.0f, \
    .space = AGENTITE_PARTICLE_WORLD, \
    .blend = AGENTITE_BLEND_ALPHA, \
    .particle = AGENTITE_PARTICLE_CONFIG_DEFAULT, \
    .texture = NULL, \
    .sprite = {0}, \
    .use_sprite = false, \
    .max_particles = 0, \
    .prewarm = false \
}

/* Particle system configuration */
typedef struct Agentite_ParticleSystemConfig {
    uint32_t max_particles;      /* Global particle pool size (default: 10000) */
    uint32_t max_emitters;       /* Maximum number of emitters (default: 64) */
} Agentite_ParticleSystemConfig;

/* Default system configuration */
#define AGENTITE_PARTICLE_SYSTEM_DEFAULT { \
    .max_particles = 10000, \
    .max_emitters = 64 \
}

/* ============================================================================
 * Particle System Lifecycle
 * ============================================================================ */

/**
 * Create a particle system.
 * Caller OWNS the returned pointer and MUST call agentite_particle_system_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return Particle system, or NULL on failure
 */
Agentite_ParticleSystem *agentite_particle_system_create(
    const Agentite_ParticleSystemConfig *config);

/**
 * Destroy particle system and all emitters.
 * Safe to call with NULL.
 */
void agentite_particle_system_destroy(Agentite_ParticleSystem *ps);

/**
 * Update all particles and emitters.
 * Call once per frame.
 *
 * @param ps Particle system
 * @param dt Delta time in seconds
 */
void agentite_particle_system_update(Agentite_ParticleSystem *ps, float dt);

/**
 * Draw all particles to sprite renderer.
 * Call between agentite_sprite_begin() and agentite_sprite_upload().
 *
 * @param ps Particle system
 * @param sr Sprite renderer
 */
void agentite_particle_system_draw(Agentite_ParticleSystem *ps,
                                   Agentite_SpriteRenderer *sr);

/**
 * Draw particles with camera transformation.
 *
 * @param ps Particle system
 * @param sr Sprite renderer
 * @param camera Camera for world-to-screen transform (NULL for screen space)
 */
void agentite_particle_system_draw_camera(Agentite_ParticleSystem *ps,
                                          Agentite_SpriteRenderer *sr,
                                          Agentite_Camera *camera);

/**
 * Clear all particles from the system.
 * Does not destroy emitters, just removes active particles.
 */
void agentite_particle_system_clear(Agentite_ParticleSystem *ps);

/**
 * Get number of active particles across all emitters.
 */
uint32_t agentite_particle_system_get_count(const Agentite_ParticleSystem *ps);

/**
 * Get maximum particle capacity.
 */
uint32_t agentite_particle_system_get_capacity(const Agentite_ParticleSystem *ps);

/* ============================================================================
 * Emitter Lifecycle
 * ============================================================================ */

/**
 * Create a particle emitter within a system.
 * Caller OWNS the returned pointer and MUST call agentite_particle_emitter_destroy().
 *
 * @param ps Parent particle system
 * @param config Emitter configuration (NULL for defaults)
 * @return Emitter, or NULL on failure
 */
Agentite_ParticleEmitter *agentite_particle_emitter_create(
    Agentite_ParticleSystem *ps,
    const Agentite_ParticleEmitterConfig *config);

/**
 * Destroy an emitter.
 * Active particles from this emitter continue to live until expiration.
 * Safe to call with NULL.
 */
void agentite_particle_emitter_destroy(Agentite_ParticleEmitter *emitter);

/* ============================================================================
 * Emitter Control
 * ============================================================================ */

/**
 * Start emitting particles.
 */
void agentite_particle_emitter_start(Agentite_ParticleEmitter *emitter);

/**
 * Stop emitting particles.
 * Existing particles continue until they expire.
 */
void agentite_particle_emitter_stop(Agentite_ParticleEmitter *emitter);

/**
 * Pause emission (can be resumed).
 */
void agentite_particle_emitter_pause(Agentite_ParticleEmitter *emitter);

/**
 * Resume paused emission.
 */
void agentite_particle_emitter_resume(Agentite_ParticleEmitter *emitter);

/**
 * Reset emitter to initial state.
 * Clears any accumulated emission time.
 */
void agentite_particle_emitter_reset(Agentite_ParticleEmitter *emitter);

/**
 * Emit a burst of particles immediately.
 * Works regardless of emission mode.
 *
 * @param emitter Emitter
 * @param count Number of particles to emit (0 = use burst_count from config)
 */
void agentite_particle_emitter_burst(Agentite_ParticleEmitter *emitter,
                                     uint32_t count);

/**
 * Check if emitter is currently active.
 */
bool agentite_particle_emitter_is_active(const Agentite_ParticleEmitter *emitter);

/**
 * Check if emitter has finished (timed mode completed).
 */
bool agentite_particle_emitter_is_finished(const Agentite_ParticleEmitter *emitter);

/**
 * Get number of active particles from this emitter.
 */
uint32_t agentite_particle_emitter_get_count(const Agentite_ParticleEmitter *emitter);

/* ============================================================================
 * Emitter Transform
 * ============================================================================ */

/**
 * Set emitter position.
 */
void agentite_particle_emitter_set_position(Agentite_ParticleEmitter *emitter,
                                            float x, float y);

/**
 * Get emitter position.
 */
void agentite_particle_emitter_get_position(const Agentite_ParticleEmitter *emitter,
                                            float *x, float *y);

/**
 * Set emitter rotation (affects emission direction).
 *
 * @param emitter Emitter
 * @param degrees Rotation in degrees
 */
void agentite_particle_emitter_set_rotation(Agentite_ParticleEmitter *emitter,
                                            float degrees);

/**
 * Set emitter scale (affects spawn area size).
 */
void agentite_particle_emitter_set_scale(Agentite_ParticleEmitter *emitter,
                                         float scale_x, float scale_y);

/* ============================================================================
 * Emitter Properties (Runtime Modification)
 * ============================================================================ */

/**
 * Set emission rate (particles per second).
 */
void agentite_particle_emitter_set_rate(Agentite_ParticleEmitter *emitter, float rate);

/**
 * Set emission mode.
 */
void agentite_particle_emitter_set_mode(Agentite_ParticleEmitter *emitter,
                                        Agentite_EmissionMode mode);

/**
 * Set particle texture.
 */
void agentite_particle_emitter_set_texture(Agentite_ParticleEmitter *emitter,
                                           Agentite_Texture *texture);

/**
 * Set particle sprite (for sprite sheets).
 */
void agentite_particle_emitter_set_sprite(Agentite_ParticleEmitter *emitter,
                                          const Agentite_Sprite *sprite);

/**
 * Set blend mode.
 */
void agentite_particle_emitter_set_blend(Agentite_ParticleEmitter *emitter,
                                         Agentite_ParticleBlend blend);

/**
 * Set start/end colors.
 */
void agentite_particle_emitter_set_colors(Agentite_ParticleEmitter *emitter,
                                          Agentite_Color start,
                                          Agentite_Color end);

/**
 * Set start/end sizes.
 */
void agentite_particle_emitter_set_sizes(Agentite_ParticleEmitter *emitter,
                                         float start_min, float start_max,
                                         float end_min, float end_max);

/**
 * Set gravity.
 */
void agentite_particle_emitter_set_gravity(Agentite_ParticleEmitter *emitter,
                                           float gravity);

/**
 * Set particle lifetime range.
 */
void agentite_particle_emitter_set_lifetime(Agentite_ParticleEmitter *emitter,
                                            float min, float max);

/**
 * Set particle speed range.
 */
void agentite_particle_emitter_set_speed(Agentite_ParticleEmitter *emitter,
                                         float min, float max);

/* ============================================================================
 * Preset Emitters
 * ============================================================================ */

/**
 * Create explosion emitter preset.
 * Fast burst of particles that expand outward and fade.
 */
Agentite_ParticleEmitter *agentite_particle_preset_explosion(
    Agentite_ParticleSystem *ps,
    float x, float y,
    Agentite_Color color,
    float scale);

/**
 * Create smoke emitter preset.
 * Rising particles that grow and fade.
 */
Agentite_ParticleEmitter *agentite_particle_preset_smoke(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float rate);

/**
 * Create fire emitter preset.
 * Rising particles with orange-to-red color, additive blending.
 */
Agentite_ParticleEmitter *agentite_particle_preset_fire(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float scale);

/**
 * Create sparks emitter preset.
 * Fast particles with gravity, additive blending.
 */
Agentite_ParticleEmitter *agentite_particle_preset_sparks(
    Agentite_ParticleSystem *ps,
    float x, float y,
    Agentite_Color color);

/**
 * Create rain emitter preset.
 * Falling particles from top of area.
 *
 * @param width Width of rain area
 * @param height Height of rain area (particles spawn at top)
 */
Agentite_ParticleEmitter *agentite_particle_preset_rain(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float width, float height,
    float intensity);

/**
 * Create snow emitter preset.
 * Slowly falling particles with slight drift.
 */
Agentite_ParticleEmitter *agentite_particle_preset_snow(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float width, float height,
    float intensity);

/**
 * Create trail emitter preset.
 * Suitable for projectile trails, follows emitter position.
 */
Agentite_ParticleEmitter *agentite_particle_preset_trail(
    Agentite_ParticleSystem *ps,
    Agentite_Color color,
    float size);

/**
 * Create dust/debris emitter preset.
 * Particles that fall and bounce, with gravity.
 */
Agentite_ParticleEmitter *agentite_particle_preset_dust(
    Agentite_ParticleSystem *ps,
    float x, float y,
    Agentite_Color color);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Evaluate an easing function.
 *
 * @param func Easing function type
 * @param t Progress value (0.0 to 1.0)
 * @return Eased value
 */
float agentite_ease(Agentite_EaseFunc func, float t);

/**
 * Interpolate between two colors.
 *
 * @param a Start color
 * @param b End color
 * @param t Progress (0.0 to 1.0)
 * @return Interpolated color
 */
Agentite_Color agentite_color_lerp(Agentite_Color a, Agentite_Color b, float t);

/**
 * Create color from 8-bit components (0-255).
 */
Agentite_Color agentite_color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);

/**
 * Create color from hex value (0xRRGGBB or 0xRRGGBBAA).
 */
Agentite_Color agentite_color_from_hex(uint32_t hex);

/* Common colors */
#define AGENTITE_COLOR_WHITE   ((Agentite_Color){1.0f, 1.0f, 1.0f, 1.0f})
#define AGENTITE_COLOR_BLACK   ((Agentite_Color){0.0f, 0.0f, 0.0f, 1.0f})
#define AGENTITE_COLOR_RED     ((Agentite_Color){1.0f, 0.0f, 0.0f, 1.0f})
#define AGENTITE_COLOR_GREEN   ((Agentite_Color){0.0f, 1.0f, 0.0f, 1.0f})
#define AGENTITE_COLOR_BLUE    ((Agentite_Color){0.0f, 0.0f, 1.0f, 1.0f})
#define AGENTITE_COLOR_YELLOW  ((Agentite_Color){1.0f, 1.0f, 0.0f, 1.0f})
#define AGENTITE_COLOR_CYAN    ((Agentite_Color){0.0f, 1.0f, 1.0f, 1.0f})
#define AGENTITE_COLOR_MAGENTA ((Agentite_Color){1.0f, 0.0f, 1.0f, 1.0f})
#define AGENTITE_COLOR_ORANGE  ((Agentite_Color){1.0f, 0.5f, 0.0f, 1.0f})
#define AGENTITE_COLOR_CLEAR   ((Agentite_Color){0.0f, 0.0f, 0.0f, 0.0f})

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_PARTICLE_H */
