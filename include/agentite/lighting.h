/*
 * Agentite 2D Lighting System
 *
 * A flexible 2D lighting system that provides point lights, directional lights,
 * ambient lighting, and shadow casting for sprites and tilemaps.
 *
 * The lighting system uses a render-to-texture approach:
 * 1. Lights are rendered to a lightmap texture using additive blending
 * 2. The lightmap is then blended with the scene during final render
 *
 * Basic Usage:
 *   // Create lighting system
 *   Agentite_LightingConfig config = AGENTITE_LIGHTING_CONFIG_DEFAULT;
 *   Agentite_LightingSystem *ls = agentite_lighting_create(ss, window, &config);
 *
 *   // Set ambient light
 *   agentite_lighting_set_ambient(ls, 0.1f, 0.1f, 0.15f, 1.0f);
 *
 *   // Add lights
 *   uint32_t torch = agentite_lighting_add_point_light(ls,
 *       &(Agentite_PointLightDesc){ .x = 100.0f, .y = 100.0f,
 *           .radius = 150.0f, .color = {1.0f, 0.8f, 0.5f, 1.0f} });
 *
 *   // In render loop:
 *   agentite_lighting_begin(ls);
 *   agentite_lighting_render_lights(ls, cmd, camera);  // Render lightmap
 *
 *   // Render scene to texture, then composite with lighting:
 *   agentite_begin_render_pass_to_texture(engine, scene_texture, ...);
 *   agentite_sprite_render(sr, cmd, pass);
 *   agentite_end_render_pass_no_submit(engine);
 *
 *   agentite_begin_render_pass(engine, 0, 0, 0, 1);
 *   agentite_lighting_apply(ls, cmd, pass, scene_texture);  // Composite
 *   agentite_end_render_pass(engine);
 *
 *   // Cleanup
 *   agentite_lighting_destroy(ls);
 *
 * Shadow Casting:
 *   The system supports raycast-based shadow casting from tilemap geometry
 *   and explicit shadow-casting shapes.
 *
 * Thread Safety:
 *   - All functions are NOT thread-safe (main thread only)
 *   - All GPU operations must occur on the rendering thread
 */

#ifndef AGENTITE_LIGHTING_H
#define AGENTITE_LIGHTING_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_LightingSystem Agentite_LightingSystem;
typedef struct Agentite_ShaderSystem Agentite_ShaderSystem;
typedef struct Agentite_Camera Agentite_Camera;
typedef struct Agentite_Tilemap Agentite_Tilemap;

/* ============================================================================
 * Light Types and Enumerations
 * ============================================================================ */

/**
 * Types of lights supported by the system.
 */
typedef enum Agentite_LightType {
    AGENTITE_LIGHT_POINT,       /* Point light with circular falloff */
    AGENTITE_LIGHT_SPOT,        /* Spot light with cone and direction */
    AGENTITE_LIGHT_DIRECTIONAL  /* Directional light (sun/moon) */
} Agentite_LightType;

/**
 * Light falloff curve types.
 */
typedef enum Agentite_LightFalloff {
    AGENTITE_FALLOFF_LINEAR,    /* Linear falloff (1 - d/r) */
    AGENTITE_FALLOFF_QUADRATIC, /* Inverse square (1 / (1 + d^2)) */
    AGENTITE_FALLOFF_SMOOTH,    /* Smooth hermite curve (default) */
    AGENTITE_FALLOFF_NONE       /* No falloff (constant within radius) */
} Agentite_LightFalloff;

/**
 * Blend mode for compositing lighting with the scene.
 */
typedef enum Agentite_LightBlendMode {
    AGENTITE_LIGHT_BLEND_MULTIPLY,  /* Multiply scene by lightmap (default) */
    AGENTITE_LIGHT_BLEND_ADDITIVE,  /* Add lightmap to scene */
    AGENTITE_LIGHT_BLEND_OVERLAY    /* Overlay blend (preserves colors) */
} Agentite_LightBlendMode;

/* ============================================================================
 * Light Descriptors
 * ============================================================================ */

/**
 * RGBA color for lights.
 */
typedef struct Agentite_LightColor {
    float r, g, b, a;
} Agentite_LightColor;

/**
 * Point light descriptor.
 */
typedef struct Agentite_PointLightDesc {
    float x, y;                     /* World position */
    float radius;                   /* Light radius in world units */
    float intensity;                /* Light intensity multiplier (default: 1.0) */
    Agentite_LightColor color;      /* Light color (RGBA) */
    Agentite_LightFalloff falloff;  /* Falloff curve type */
    bool casts_shadows;             /* Whether this light casts shadows */
    float z_height;                 /* Height for shadow calculations (optional) */
} Agentite_PointLightDesc;

#define AGENTITE_POINT_LIGHT_DEFAULT { \
    .x = 0.0f, .y = 0.0f,              \
    .radius = 100.0f,                   \
    .intensity = 1.0f,                  \
    .color = { 1.0f, 1.0f, 1.0f, 1.0f }, \
    .falloff = AGENTITE_FALLOFF_SMOOTH, \
    .casts_shadows = false,             \
    .z_height = 0.0f                    \
}

/**
 * Spot light descriptor.
 */
typedef struct Agentite_SpotLightDesc {
    float x, y;                     /* World position */
    float direction_x, direction_y; /* Normalized direction vector */
    float radius;                   /* Maximum light distance */
    float inner_angle;              /* Inner cone angle in radians (full intensity) */
    float outer_angle;              /* Outer cone angle in radians (falloff edge) */
    float intensity;                /* Light intensity multiplier */
    Agentite_LightColor color;      /* Light color (RGBA) */
    Agentite_LightFalloff falloff;  /* Distance falloff curve */
    bool casts_shadows;             /* Whether this light casts shadows */
} Agentite_SpotLightDesc;

#define AGENTITE_SPOT_LIGHT_DEFAULT {   \
    .x = 0.0f, .y = 0.0f,               \
    .direction_x = 0.0f,                \
    .direction_y = -1.0f,               \
    .radius = 200.0f,                   \
    .inner_angle = 0.2617994f,          \
    .outer_angle = 0.5235988f,          \
    .intensity = 1.0f,                  \
    .color = { 1.0f, 1.0f, 1.0f, 1.0f }, \
    .falloff = AGENTITE_FALLOFF_SMOOTH, \
    .casts_shadows = false              \
}

/**
 * Directional light descriptor (e.g., sun, moon).
 */
typedef struct Agentite_DirectionalLightDesc {
    float direction_x, direction_y; /* Normalized direction vector */
    float intensity;                /* Light intensity multiplier */
    Agentite_LightColor color;      /* Light color (RGBA) */
    bool casts_shadows;             /* Whether this light casts shadows */
} Agentite_DirectionalLightDesc;

#define AGENTITE_DIRECTIONAL_LIGHT_DEFAULT { \
    .direction_x = 0.0f,                     \
    .direction_y = -1.0f,                    \
    .intensity = 1.0f,                       \
    .color = { 1.0f, 1.0f, 0.9f, 1.0f },     \
    .casts_shadows = false                   \
}

/* ============================================================================
 * Shadow Casting Shapes
 * ============================================================================ */

/**
 * Shadow occluder shape type.
 */
typedef enum Agentite_OccluderType {
    AGENTITE_OCCLUDER_SEGMENT,  /* Line segment */
    AGENTITE_OCCLUDER_BOX,      /* Axis-aligned box */
    AGENTITE_OCCLUDER_CIRCLE    /* Circle */
} Agentite_OccluderType;

/**
 * Shadow-casting occluder.
 */
typedef struct Agentite_Occluder {
    Agentite_OccluderType type;
    union {
        struct { float x1, y1, x2, y2; } segment;
        struct { float x, y, w, h; } box;
        struct { float x, y, radius; } circle;
    };
} Agentite_Occluder;

/* ============================================================================
 * System Configuration
 * ============================================================================ */

/**
 * Lighting system configuration.
 */
typedef struct Agentite_LightingConfig {
    int max_point_lights;           /* Max simultaneous point lights (default: 64) */
    int max_spot_lights;            /* Max simultaneous spot lights (default: 16) */
    int max_occluders;              /* Max shadow occluders (default: 256) */

    int lightmap_width;             /* Lightmap width (0 = window size) */
    int lightmap_height;            /* Lightmap height (0 = window size) */
    float lightmap_scale;           /* Lightmap resolution scale (default: 1.0) */

    SDL_GPUTextureFormat format;    /* Lightmap texture format */
    Agentite_LightBlendMode blend;  /* Default blend mode */

    bool enable_shadows;            /* Enable shadow casting (default: false) */
    int shadow_ray_count;           /* Rays per light for shadows (default: 64) */
} Agentite_LightingConfig;

#define AGENTITE_LIGHTING_CONFIG_DEFAULT {              \
    .max_point_lights = 64,                             \
    .max_spot_lights = 16,                              \
    .max_occluders = 256,                               \
    .lightmap_width = 0,                                \
    .lightmap_height = 0,                               \
    .lightmap_scale = 1.0f,                             \
    .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,     \
    .blend = AGENTITE_LIGHT_BLEND_MULTIPLY,             \
    .enable_shadows = false,                            \
    .shadow_ray_count = 64                              \
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create a lighting system.
 *
 * Caller OWNS the returned pointer and MUST call agentite_lighting_destroy().
 * Returns NULL on failure (check agentite_get_last_error()).
 *
 * @param gpu           GPU device for rendering
 * @param shader_system Shader system for lighting shaders
 * @param window        Window for sizing (can be NULL if config specifies size)
 * @param config        Configuration (NULL for defaults)
 * @return New lighting system, or NULL on failure
 */
Agentite_LightingSystem *agentite_lighting_create(SDL_GPUDevice *gpu,
                                                   Agentite_ShaderSystem *shader_system,
                                                   SDL_Window *window,
                                                   const Agentite_LightingConfig *config);

/**
 * Destroy a lighting system.
 * Accepts NULL safely.
 *
 * @param ls Lighting system to destroy
 */
void agentite_lighting_destroy(Agentite_LightingSystem *ls);

/**
 * Resize lightmap buffers.
 * Call when window resizes.
 *
 * @param ls     Lighting system
 * @param width  New width
 * @param height New height
 * @return true on success
 */
bool agentite_lighting_resize(Agentite_LightingSystem *ls, int width, int height);

/* ============================================================================
 * Ambient Light
 * ============================================================================ */

/**
 * Set the ambient light color.
 * Ambient light provides a base illumination for all areas.
 *
 * @param ls Lighting system
 * @param r  Red (0-1)
 * @param g  Green (0-1)
 * @param b  Blue (0-1)
 * @param a  Alpha/intensity (0-1)
 */
void agentite_lighting_set_ambient(Agentite_LightingSystem *ls,
                                   float r, float g, float b, float a);

/**
 * Get the current ambient light color.
 *
 * @param ls    Lighting system
 * @param color Output color (or NULL)
 */
void agentite_lighting_get_ambient(const Agentite_LightingSystem *ls,
                                   Agentite_LightColor *color);

/* ============================================================================
 * Point Lights
 * ============================================================================ */

/**
 * Add a point light to the scene.
 * System OWNS the light after adding.
 *
 * @param ls   Lighting system
 * @param desc Point light descriptor
 * @return Light ID (0 = invalid)
 */
uint32_t agentite_lighting_add_point_light(Agentite_LightingSystem *ls,
                                           const Agentite_PointLightDesc *desc);

/**
 * Get a point light's properties.
 *
 * @param ls       Lighting system
 * @param light_id Light ID from add function
 * @param desc     Output descriptor (or NULL)
 * @return true if light exists
 */
bool agentite_lighting_get_point_light(const Agentite_LightingSystem *ls,
                                       uint32_t light_id,
                                       Agentite_PointLightDesc *desc);

/**
 * Update a point light's properties.
 *
 * @param ls       Lighting system
 * @param light_id Light ID from add function
 * @param desc     New properties
 * @return true on success
 */
bool agentite_lighting_set_point_light(Agentite_LightingSystem *ls,
                                       uint32_t light_id,
                                       const Agentite_PointLightDesc *desc);

/**
 * Set a point light's position.
 * Convenience function for moving lights.
 *
 * @param ls       Lighting system
 * @param light_id Light ID
 * @param x        World X position
 * @param y        World Y position
 */
void agentite_lighting_set_point_light_position(Agentite_LightingSystem *ls,
                                                uint32_t light_id,
                                                float x, float y);

/**
 * Set a point light's intensity.
 *
 * @param ls        Lighting system
 * @param light_id  Light ID
 * @param intensity Intensity multiplier
 */
void agentite_lighting_set_point_light_intensity(Agentite_LightingSystem *ls,
                                                 uint32_t light_id,
                                                 float intensity);

/**
 * Remove a point light.
 *
 * @param ls       Lighting system
 * @param light_id Light ID to remove
 */
void agentite_lighting_remove_point_light(Agentite_LightingSystem *ls,
                                          uint32_t light_id);

/* ============================================================================
 * Spot Lights
 * ============================================================================ */

/**
 * Add a spot light to the scene.
 *
 * @param ls   Lighting system
 * @param desc Spot light descriptor
 * @return Light ID (0 = invalid)
 */
uint32_t agentite_lighting_add_spot_light(Agentite_LightingSystem *ls,
                                          const Agentite_SpotLightDesc *desc);

/**
 * Get a spot light's properties.
 *
 * @param ls       Lighting system
 * @param light_id Light ID
 * @param desc     Output descriptor (or NULL)
 * @return true if light exists
 */
bool agentite_lighting_get_spot_light(const Agentite_LightingSystem *ls,
                                      uint32_t light_id,
                                      Agentite_SpotLightDesc *desc);

/**
 * Update a spot light's properties.
 *
 * @param ls       Lighting system
 * @param light_id Light ID
 * @param desc     New properties
 * @return true on success
 */
bool agentite_lighting_set_spot_light(Agentite_LightingSystem *ls,
                                      uint32_t light_id,
                                      const Agentite_SpotLightDesc *desc);

/**
 * Set spot light position and direction.
 *
 * @param ls       Lighting system
 * @param light_id Light ID
 * @param x        World X position
 * @param y        World Y position
 * @param dir_x    Direction X (normalized)
 * @param dir_y    Direction Y (normalized)
 */
void agentite_lighting_set_spot_light_transform(Agentite_LightingSystem *ls,
                                                uint32_t light_id,
                                                float x, float y,
                                                float dir_x, float dir_y);

/**
 * Remove a spot light.
 *
 * @param ls       Lighting system
 * @param light_id Light ID to remove
 */
void agentite_lighting_remove_spot_light(Agentite_LightingSystem *ls,
                                         uint32_t light_id);

/* ============================================================================
 * Directional Light
 * ============================================================================ */

/**
 * Set the directional light (sun/moon).
 * Only one directional light is supported.
 *
 * @param ls      Lighting system
 * @param desc    Directional light descriptor (NULL to disable)
 */
void agentite_lighting_set_directional(Agentite_LightingSystem *ls,
                                       const Agentite_DirectionalLightDesc *desc);

/**
 * Get the directional light properties.
 *
 * @param ls   Lighting system
 * @param desc Output descriptor
 * @return true if directional light is enabled
 */
bool agentite_lighting_get_directional(const Agentite_LightingSystem *ls,
                                       Agentite_DirectionalLightDesc *desc);

/* ============================================================================
 * Shadow Occluders
 * ============================================================================ */

/**
 * Add a shadow-casting occluder.
 *
 * @param ls       Lighting system
 * @param occluder Occluder shape
 * @return Occluder ID (0 = invalid)
 */
uint32_t agentite_lighting_add_occluder(Agentite_LightingSystem *ls,
                                        const Agentite_Occluder *occluder);

/**
 * Remove an occluder.
 *
 * @param ls          Lighting system
 * @param occluder_id Occluder ID to remove
 */
void agentite_lighting_remove_occluder(Agentite_LightingSystem *ls,
                                       uint32_t occluder_id);

/**
 * Clear all occluders.
 *
 * @param ls Lighting system
 */
void agentite_lighting_clear_occluders(Agentite_LightingSystem *ls);

/**
 * Generate occluders from tilemap collision data.
 * Automatically creates occluders from solid tiles.
 *
 * @param ls      Lighting system
 * @param tilemap Tilemap to generate occluders from
 * @param layer   Tilemap layer index
 * @return Number of occluders added
 */
int agentite_lighting_add_tilemap_occluders(Agentite_LightingSystem *ls,
                                            const Agentite_Tilemap *tilemap,
                                            int layer);

/* ============================================================================
 * Rendering
 * ============================================================================ */

/**
 * Begin a new lighting frame.
 * Call at the start of each frame before rendering lights.
 *
 * @param ls Lighting system
 */
void agentite_lighting_begin(Agentite_LightingSystem *ls);

/**
 * Render all lights to the lightmap.
 * Call after begin() and before apply().
 *
 * @param ls     Lighting system
 * @param cmd    Command buffer
 * @param camera Camera for world-to-screen transform (NULL = identity)
 */
void agentite_lighting_render_lights(Agentite_LightingSystem *ls,
                                     SDL_GPUCommandBuffer *cmd,
                                     const Agentite_Camera *camera);

/**
 * Composite the lightmap with the scene.
 * Call during your main render pass after rendering lights.
 *
 * @param ls            Lighting system
 * @param cmd           Command buffer
 * @param pass          Render pass (to swapchain)
 * @param scene_texture Scene texture to composite with lightmap
 */
void agentite_lighting_apply(Agentite_LightingSystem *ls,
                             SDL_GPUCommandBuffer *cmd,
                             SDL_GPURenderPass *pass,
                             SDL_GPUTexture *scene_texture);

/**
 * Get the lightmap texture for custom rendering.
 * Returns borrowed reference (do NOT destroy).
 *
 * @param ls Lighting system
 * @return Lightmap texture
 */
SDL_GPUTexture *agentite_lighting_get_lightmap(const Agentite_LightingSystem *ls);

/* ============================================================================
 * Light Management
 * ============================================================================ */

/**
 * Remove all lights (keeps occluders).
 *
 * @param ls Lighting system
 */
void agentite_lighting_clear_lights(Agentite_LightingSystem *ls);

/**
 * Enable or disable a light.
 *
 * @param ls       Lighting system
 * @param light_id Light ID
 * @param enabled  true to enable, false to disable
 */
void agentite_lighting_set_light_enabled(Agentite_LightingSystem *ls,
                                         uint32_t light_id,
                                         bool enabled);

/**
 * Check if a light is enabled.
 *
 * @param ls       Lighting system
 * @param light_id Light ID
 * @return true if enabled
 */
bool agentite_lighting_is_light_enabled(const Agentite_LightingSystem *ls,
                                        uint32_t light_id);

/* ============================================================================
 * Day/Night Cycle
 * ============================================================================ */

/**
 * Day/night cycle time-of-day structure.
 */
typedef struct Agentite_TimeOfDay {
    float time;             /* Time in hours (0-24), wraps automatically */
    float sunrise_hour;     /* Hour when sun begins rising (default: 6.0) */
    float sunset_hour;      /* Hour when sun begins setting (default: 18.0) */
    float transition_hours; /* Hours for sunrise/sunset transition (default: 2.0) */

    /* Color palette */
    Agentite_LightColor ambient_day;    /* Ambient during day */
    Agentite_LightColor ambient_night;  /* Ambient during night */
    Agentite_LightColor sun_color;      /* Sun color at peak */
    Agentite_LightColor sunset_color;   /* Color during sunset/sunrise */
    Agentite_LightColor moon_color;     /* Moon color at peak */
} Agentite_TimeOfDay;

#define AGENTITE_TIME_OF_DAY_DEFAULT {                   \
    .time = 12.0f,                                       \
    .sunrise_hour = 6.0f,                                \
    .sunset_hour = 18.0f,                                \
    .transition_hours = 2.0f,                            \
    .ambient_day = { 0.4f, 0.4f, 0.45f, 1.0f },          \
    .ambient_night = { 0.05f, 0.05f, 0.1f, 1.0f },       \
    .sun_color = { 1.0f, 0.95f, 0.8f, 1.0f },            \
    .sunset_color = { 1.0f, 0.5f, 0.3f, 1.0f },          \
    .moon_color = { 0.3f, 0.3f, 0.5f, 1.0f }             \
}

/**
 * Update lighting based on time of day.
 * Automatically adjusts ambient and directional light.
 *
 * @param ls  Lighting system
 * @param tod Time of day configuration
 */
void agentite_lighting_update_time_of_day(Agentite_LightingSystem *ls,
                                          const Agentite_TimeOfDay *tod);

/**
 * Advance time and update lighting.
 *
 * @param ls          Lighting system
 * @param tod         Time of day configuration (modified with new time)
 * @param delta_hours Hours to advance
 */
void agentite_lighting_advance_time(Agentite_LightingSystem *ls,
                                    Agentite_TimeOfDay *tod,
                                    float delta_hours);

/* ============================================================================
 * Statistics and Debug
 * ============================================================================ */

/**
 * Lighting system statistics.
 */
typedef struct Agentite_LightingStats {
    uint32_t point_light_count;
    uint32_t spot_light_count;
    uint32_t occluder_count;
    uint32_t max_point_lights;
    uint32_t max_spot_lights;
    uint32_t max_occluders;
    int lightmap_width;
    int lightmap_height;
    bool shadows_enabled;
} Agentite_LightingStats;

/**
 * Get lighting system statistics.
 *
 * @param ls    Lighting system
 * @param stats Output statistics
 */
void agentite_lighting_get_stats(const Agentite_LightingSystem *ls,
                                 Agentite_LightingStats *stats);

/**
 * Set blend mode for lighting composite.
 *
 * @param ls   Lighting system
 * @param mode Blend mode
 */
void agentite_lighting_set_blend_mode(Agentite_LightingSystem *ls,
                                      Agentite_LightBlendMode mode);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_LIGHTING_H */
