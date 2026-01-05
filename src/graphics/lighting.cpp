/*
 * Agentite 2D Lighting System Implementation
 *
 * Provides point lights, spot lights, directional lights, ambient lighting,
 * and shadow casting for 2D games.
 */

#include "agentite/lighting.h"
#include "agentite/shader.h"
#include "agentite/error.h"
#include "agentite/camera.h"
#include "agentite/tilemap.h"
#include "lighting_shaders.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <cfloat>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define LIGHT_ID_INVALID 0
#define LIGHT_ID_OFFSET_POINT 1
#define LIGHT_ID_OFFSET_SPOT 10000
#define MAX_SHADOW_CASTING_LIGHTS 8

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Internal point light representation */
typedef struct {
    Agentite_PointLightDesc desc;
    bool active;
    bool enabled;
    uint32_t id;
} InternalPointLight;

/* Internal spot light representation */
typedef struct {
    Agentite_SpotLightDesc desc;
    bool active;
    bool enabled;
    uint32_t id;
} InternalSpotLight;

/* Internal occluder representation */
typedef struct {
    Agentite_Occluder occluder;
    bool active;
    uint32_t id;
} InternalOccluder;

/* Lighting system state */
struct Agentite_LightingSystem {
    Agentite_ShaderSystem *shader_system;
    SDL_GPUDevice *gpu;
    SDL_Window *window;

    /* Configuration */
    Agentite_LightingConfig config;

    /* Ambient light */
    Agentite_LightColor ambient;

    /* Directional light */
    Agentite_DirectionalLightDesc directional;
    bool directional_enabled;

    /* Point lights */
    InternalPointLight *point_lights;
    uint32_t point_light_count;
    uint32_t next_point_light_id;

    /* Spot lights */
    InternalSpotLight *spot_lights;
    uint32_t spot_light_count;
    uint32_t next_spot_light_id;

    /* Occluders */
    InternalOccluder *occluders;
    uint32_t occluder_count;
    uint32_t next_occluder_id;

    /* Lightmap render target */
    SDL_GPUTexture *lightmap;
    int lightmap_width;
    int lightmap_height;

    /* Shaders */
    Agentite_Shader *point_light_shader;
    Agentite_Shader *spot_light_shader;
    Agentite_Shader *composite_shader;
    Agentite_Shader *ambient_shader;
    Agentite_Shader *point_light_shadow_shader;  /* Point light with shadow sampling */
    bool shaders_initialized;

    /* GPU resources */
    SDL_GPUBuffer *quad_vertex_buffer;
    SDL_GPUSampler *sampler;

    /* Shadow mapping */
    SDL_GPUTexture *shadow_map;           /* 2D texture atlas for shadow maps (resolution x MAX_SHADOW_CASTING_LIGHTS) */
    int shadow_map_resolution;            /* Width of shadow map (angles per light) */
    SDL_GPUBuffer *occluder_buffer;       /* GPU buffer for occluder data */
    uint32_t occluder_buffer_capacity;    /* Max occluders in buffer */

    /* Multi-light shadow tracking */
    int shadow_light_indices[MAX_SHADOW_CASTING_LIGHTS];  /* Maps shadow slot -> point light index */
    int active_shadow_light_count;                         /* Number of shadow-casting point lights this frame */

    /* Spot light shadow tracking */
    Agentite_Shader *spot_light_shadow_shader;  /* Spot light with shadow sampling */
    int spot_shadow_light_indices[MAX_SHADOW_CASTING_LIGHTS];  /* Maps shadow slot -> spot light index */
    int active_spot_shadow_light_count;                         /* Number of shadow-casting spot lights this frame */

    /* State */
    bool frame_started;
};

/* Quad vertex for fullscreen rendering */
typedef struct {
    float pos[2];
    float uv[2];
} LightQuadVertex;

/* Point light uniform data - must match PointLightParams in lighting_shaders.h
 * Metal alignment: float2 requires 8-byte alignment, float4 requires 16-byte alignment */
typedef struct {
    float light_center[2];  /* offset 0: Light center in UV space (0-1) */
    float radius;           /* offset 8: Light radius in UV space */
    float intensity;        /* offset 12: Light intensity multiplier */
    float color[4];         /* offset 16: RGBA color */
    float falloff_type;     /* offset 32: 0=linear, 1=quadratic, 2=smooth, 3=none */
    float _pad_align;       /* offset 36: padding for float2 alignment */
    float aspect[2];        /* offset 40: Aspect ratio correction */
    float _pad;             /* offset 48: padding */
} PointLightUniforms;

/* Spot light uniform data - must match SpotLightParams in lighting_shaders.h
 * Metal alignment: float2 requires 8-byte alignment, float4 requires 16-byte alignment */
typedef struct {
    float light_center[2];  /* offset 0: Light center in UV space (0-1) */
    float direction[2];     /* offset 8: Normalized direction vector */
    float radius;           /* offset 16: Max distance in UV space */
    float inner_angle;      /* offset 20: Cosine of inner cone angle */
    float outer_angle;      /* offset 24: Cosine of outer cone angle */
    float intensity;        /* offset 28: Light intensity multiplier */
    float color[4];         /* offset 32: RGBA color */
    float falloff_type;     /* offset 48: Falloff curve type */
    float _pad_align;       /* offset 52: padding for float2 alignment */
    float aspect[2];        /* offset 56: Aspect ratio correction */
} SpotLightUniforms;

/* Ambient uniform data - must match AmbientParams in lighting_shaders.h */
typedef struct {
    float color[4];         /* Ambient RGBA */
} AmbientUniforms;

/* Composite uniform data - must match CompositeParams in lighting_shaders.h */
typedef struct {
    float ambient[4];       /* Ambient RGBA */
    float blend_mode;       /* 0=multiply, 1=additive, 2=overlay */
    float _pad[3];
} CompositeUniforms;

/* Point light shadow uniform data - matches PointLightShadowParams in shader
 * Metal alignment: float2 requires 8-byte alignment */
typedef struct {
    float light_center[2];  /* offset 0: Light center in UV space (0-1) */
    float radius;           /* offset 8: Light radius in UV space */
    float intensity;        /* offset 12: Light intensity multiplier */
    float color[4];         /* offset 16: RGBA color */
    float falloff_type;     /* offset 32: 0=linear, 1=quadratic, 2=smooth, 3=none */
    float shadow_softness;  /* offset 36: Shadow edge softness (world units) */
    float aspect[2];        /* offset 40: Aspect ratio correction */
    float lightmap_size[2]; /* offset 48: Lightmap dimensions for UV to world conversion */
    float radius_world;     /* offset 56: Light radius in world units */
    float shadow_row;       /* offset 60: Which row in shadow atlas (0-7) */
    float atlas_height;     /* offset 64: Total atlas height (8.0) */
    float _pad[3];          /* offset 68: padding to 80 bytes */
} PointLightShadowUniforms;

/* Spot light shadow uniform data - matches SpotLightShadowParams in shader
 * Metal alignment: float2 requires 8-byte alignment */
typedef struct {
    float light_center[2];  /* offset 0: Light center in UV space (0-1) */
    float direction[2];     /* offset 8: Normalized direction vector */
    float radius;           /* offset 16: Max distance in UV space */
    float inner_angle;      /* offset 20: Cosine of inner cone angle */
    float outer_angle;      /* offset 24: Cosine of outer cone angle */
    float intensity;        /* offset 28: Light intensity multiplier */
    float color[4];         /* offset 32: RGBA color */
    float falloff_type;     /* offset 48: Falloff curve type */
    float shadow_softness;  /* offset 52: Shadow edge softness (world units) */
    float aspect[2];        /* offset 56: Aspect ratio correction */
    float lightmap_size[2]; /* offset 64: Lightmap dimensions for UV to world */
    float radius_world;     /* offset 72: Light radius in world units */
    float shadow_row;       /* offset 76: Which row in shadow atlas (0-7) */
    float atlas_height;     /* offset 80: Total atlas height (8.0) */
    float cone_angle_rad;   /* offset 84: Outer cone angle in radians (for shadow map) */
    float _pad[2];          /* offset 88: padding to 96 bytes */
} SpotLightShadowUniforms;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static bool create_gpu_resources(Agentite_LightingSystem *ls);
static void destroy_gpu_resources(Agentite_LightingSystem *ls);
static bool create_lightmap(Agentite_LightingSystem *ls, int width, int height);
static void render_point_light(Agentite_LightingSystem *ls,
                               SDL_GPUCommandBuffer *cmd,
                               SDL_GPURenderPass *pass,
                               const InternalPointLight *light,
                               const Agentite_Camera *camera);
static float apply_falloff(float dist, float radius, Agentite_LightFalloff falloff);

/* Shadow casting */
static float ray_segment_intersect(float ox, float oy, float dx, float dy,
                                   float x1, float y1, float x2, float y2);
static float ray_box_intersect(float ox, float oy, float dx, float dy,
                               float bx, float by, float bw, float bh);
static float ray_circle_intersect(float ox, float oy, float dx, float dy,
                                  float cx, float cy, float r);
static void generate_shadow_map(Agentite_LightingSystem *ls,
                                float light_x, float light_y,
                                float light_radius,
                                float *shadow_distances);
static void generate_spot_shadow_map(Agentite_LightingSystem *ls,
                                     float light_x, float light_y,
                                     float light_radius,
                                     float dir_x, float dir_y,
                                     float outer_angle,
                                     float *shadow_distances);
static bool upload_shadow_map(Agentite_LightingSystem *ls,
                              SDL_GPUCommandBuffer *cmd,
                              const float *shadow_distances);
static bool upload_shadow_map_row(Agentite_LightingSystem *ls,
                                  SDL_GPUCommandBuffer *cmd,
                                  const float *shadow_distances,
                                  int row_index);
static bool create_shadow_map_texture(Agentite_LightingSystem *ls);

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_LightingSystem *agentite_lighting_create(SDL_GPUDevice *gpu,
                                                   Agentite_ShaderSystem *shader_system,
                                                   SDL_Window *window,
                                                   const Agentite_LightingConfig *config)
{
    if (!gpu) {
        agentite_set_error("Lighting: GPU device is NULL");
        return NULL;
    }

    if (!shader_system) {
        agentite_set_error("Lighting: Shader system is NULL");
        return NULL;
    }

    Agentite_LightingConfig default_config = AGENTITE_LIGHTING_CONFIG_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    /* Determine lightmap size */
    int width = config->lightmap_width;
    int height = config->lightmap_height;

    if (width == 0 || height == 0) {
        if (window) {
            SDL_GetWindowSize(window, &width, &height);
        } else {
            agentite_set_error("Lighting: Window required when size not specified");
            return NULL;
        }
    }

    /* Apply scale factor */
    width = (int)(width * config->lightmap_scale);
    height = (int)(height * config->lightmap_scale);

    /* Allocate system */
    Agentite_LightingSystem *ls = (Agentite_LightingSystem *)calloc(1, sizeof(*ls));
    if (!ls) {
        agentite_set_error("Lighting: Failed to allocate lighting system");
        return NULL;
    }

    ls->gpu = gpu;
    ls->shader_system = shader_system;
    ls->window = window;
    ls->config = *config;

    /* Allocate light arrays */
    ls->point_lights = (InternalPointLight *)calloc(config->max_point_lights,
                                                     sizeof(InternalPointLight));
    if (!ls->point_lights) {
        agentite_set_error("Lighting: Failed to allocate point lights");
        free(ls);
        return NULL;
    }

    ls->spot_lights = (InternalSpotLight *)calloc(config->max_spot_lights,
                                                   sizeof(InternalSpotLight));
    if (!ls->spot_lights) {
        agentite_set_error("Lighting: Failed to allocate spot lights");
        free(ls->point_lights);
        free(ls);
        return NULL;
    }

    ls->occluders = (InternalOccluder *)calloc(config->max_occluders,
                                                sizeof(InternalOccluder));
    if (!ls->occluders) {
        agentite_set_error("Lighting: Failed to allocate occluders");
        free(ls->spot_lights);
        free(ls->point_lights);
        free(ls);
        return NULL;
    }

    /* Initialize IDs */
    ls->next_point_light_id = LIGHT_ID_OFFSET_POINT;
    ls->next_spot_light_id = LIGHT_ID_OFFSET_SPOT;
    ls->next_occluder_id = 1;

    /* Set default ambient */
    ls->ambient.r = 0.1f;
    ls->ambient.g = 0.1f;
    ls->ambient.b = 0.1f;
    ls->ambient.a = 1.0f;

    ls->lightmap_width = width;
    ls->lightmap_height = height;

    return ls;
}

void agentite_lighting_destroy(Agentite_LightingSystem *ls)
{
    if (!ls) return;

    destroy_gpu_resources(ls);

    free(ls->occluders);
    free(ls->spot_lights);
    free(ls->point_lights);
    free(ls);
}

bool agentite_lighting_resize(Agentite_LightingSystem *ls, int width, int height)
{
    if (!ls || width <= 0 || height <= 0) return false;

    /* Apply scale */
    width = (int)(width * ls->config.lightmap_scale);
    height = (int)(height * ls->config.lightmap_scale);

    if (ls->lightmap_width == width && ls->lightmap_height == height) {
        return true;
    }

    /* Recreate lightmap */
    if (ls->lightmap && ls->gpu) {
        SDL_ReleaseGPUTexture(ls->gpu, ls->lightmap);
        ls->lightmap = NULL;
    }

    return create_lightmap(ls, width, height);
}

/* ============================================================================
 * Ambient Light
 * ============================================================================ */

void agentite_lighting_set_ambient(Agentite_LightingSystem *ls,
                                   float r, float g, float b, float a)
{
    if (!ls) return;

    ls->ambient.r = r;
    ls->ambient.g = g;
    ls->ambient.b = b;
    ls->ambient.a = a;
}

void agentite_lighting_get_ambient(const Agentite_LightingSystem *ls,
                                   Agentite_LightColor *color)
{
    if (!ls || !color) return;
    *color = ls->ambient;
}

/* ============================================================================
 * Point Lights
 * ============================================================================ */

uint32_t agentite_lighting_add_point_light(Agentite_LightingSystem *ls,
                                           const Agentite_PointLightDesc *desc)
{
    if (!ls || !desc) return LIGHT_ID_INVALID;

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (!ls->point_lights[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("Lighting: Maximum point lights reached (%d)",
                          ls->config.max_point_lights);
        return LIGHT_ID_INVALID;
    }

    InternalPointLight *light = &ls->point_lights[slot];
    light->desc = *desc;
    light->active = true;
    light->enabled = true;
    light->id = ls->next_point_light_id++;

    ls->point_light_count++;

    return light->id;
}

bool agentite_lighting_get_point_light(const Agentite_LightingSystem *ls,
                                       uint32_t light_id,
                                       Agentite_PointLightDesc *desc)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return false;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            if (desc) {
                *desc = ls->point_lights[i].desc;
            }
            return true;
        }
    }

    return false;
}

bool agentite_lighting_set_point_light(Agentite_LightingSystem *ls,
                                       uint32_t light_id,
                                       const Agentite_PointLightDesc *desc)
{
    if (!ls || !desc || light_id == LIGHT_ID_INVALID) return false;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            ls->point_lights[i].desc = *desc;
            return true;
        }
    }

    return false;
}

void agentite_lighting_set_point_light_position(Agentite_LightingSystem *ls,
                                                uint32_t light_id,
                                                float x, float y)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            ls->point_lights[i].desc.x = x;
            ls->point_lights[i].desc.y = y;
            return;
        }
    }
}

void agentite_lighting_set_point_light_intensity(Agentite_LightingSystem *ls,
                                                 uint32_t light_id,
                                                 float intensity)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            ls->point_lights[i].desc.intensity = intensity;
            return;
        }
    }
}

void agentite_lighting_remove_point_light(Agentite_LightingSystem *ls,
                                          uint32_t light_id)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            ls->point_lights[i].active = false;
            ls->point_light_count--;
            return;
        }
    }
}

/* ============================================================================
 * Spot Lights
 * ============================================================================ */

uint32_t agentite_lighting_add_spot_light(Agentite_LightingSystem *ls,
                                          const Agentite_SpotLightDesc *desc)
{
    if (!ls || !desc) return LIGHT_ID_INVALID;

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (!ls->spot_lights[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("Lighting: Maximum spot lights reached (%d)",
                          ls->config.max_spot_lights);
        return LIGHT_ID_INVALID;
    }

    InternalSpotLight *light = &ls->spot_lights[slot];
    light->desc = *desc;
    light->active = true;
    light->enabled = true;
    light->id = ls->next_spot_light_id++;

    ls->spot_light_count++;

    return light->id;
}

bool agentite_lighting_get_spot_light(const Agentite_LightingSystem *ls,
                                      uint32_t light_id,
                                      Agentite_SpotLightDesc *desc)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return false;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (ls->spot_lights[i].active && ls->spot_lights[i].id == light_id) {
            if (desc) {
                *desc = ls->spot_lights[i].desc;
            }
            return true;
        }
    }

    return false;
}

bool agentite_lighting_set_spot_light(Agentite_LightingSystem *ls,
                                      uint32_t light_id,
                                      const Agentite_SpotLightDesc *desc)
{
    if (!ls || !desc || light_id == LIGHT_ID_INVALID) return false;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (ls->spot_lights[i].active && ls->spot_lights[i].id == light_id) {
            ls->spot_lights[i].desc = *desc;
            return true;
        }
    }

    return false;
}

void agentite_lighting_set_spot_light_transform(Agentite_LightingSystem *ls,
                                                uint32_t light_id,
                                                float x, float y,
                                                float dir_x, float dir_y)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (ls->spot_lights[i].active && ls->spot_lights[i].id == light_id) {
            ls->spot_lights[i].desc.x = x;
            ls->spot_lights[i].desc.y = y;
            ls->spot_lights[i].desc.direction_x = dir_x;
            ls->spot_lights[i].desc.direction_y = dir_y;
            return;
        }
    }
}

void agentite_lighting_remove_spot_light(Agentite_LightingSystem *ls,
                                         uint32_t light_id)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (ls->spot_lights[i].active && ls->spot_lights[i].id == light_id) {
            ls->spot_lights[i].active = false;
            ls->spot_light_count--;
            return;
        }
    }
}

/* ============================================================================
 * Directional Light
 * ============================================================================ */

void agentite_lighting_set_directional(Agentite_LightingSystem *ls,
                                       const Agentite_DirectionalLightDesc *desc)
{
    if (!ls) return;

    if (desc) {
        ls->directional = *desc;
        ls->directional_enabled = true;
    } else {
        ls->directional_enabled = false;
    }
}

bool agentite_lighting_get_directional(const Agentite_LightingSystem *ls,
                                       Agentite_DirectionalLightDesc *desc)
{
    if (!ls) return false;

    if (ls->directional_enabled && desc) {
        *desc = ls->directional;
    }

    return ls->directional_enabled;
}

/* ============================================================================
 * Shadow Occluders
 * ============================================================================ */

uint32_t agentite_lighting_add_occluder(Agentite_LightingSystem *ls,
                                        const Agentite_Occluder *occluder)
{
    if (!ls || !occluder) return 0;

    /* Find free slot */
    int slot = -1;
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_occluders; i++) {
        if (!ls->occluders[i].active) {
            slot = (int)i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("Lighting: Maximum occluders reached (%d)",
                          ls->config.max_occluders);
        return 0;
    }

    InternalOccluder *occ = &ls->occluders[slot];
    occ->occluder = *occluder;
    occ->active = true;
    occ->id = ls->next_occluder_id++;

    ls->occluder_count++;

    return occ->id;
}

void agentite_lighting_remove_occluder(Agentite_LightingSystem *ls,
                                       uint32_t occluder_id)
{
    if (!ls || occluder_id == 0) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_occluders; i++) {
        if (ls->occluders[i].active && ls->occluders[i].id == occluder_id) {
            ls->occluders[i].active = false;
            ls->occluder_count--;
            return;
        }
    }
}

void agentite_lighting_clear_occluders(Agentite_LightingSystem *ls)
{
    if (!ls) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_occluders; i++) {
        ls->occluders[i].active = false;
    }
    ls->occluder_count = 0;
}

int agentite_lighting_add_tilemap_occluders(Agentite_LightingSystem *ls,
                                            const Agentite_Tilemap *tilemap,
                                            int layer)
{
    /* TODO: Implement tilemap occluder generation */
    /* This requires iterating through solid tiles and creating box occluders */
    (void)ls;
    (void)tilemap;
    (void)layer;
    return 0;
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

void agentite_lighting_begin(Agentite_LightingSystem *ls)
{
    if (!ls) return;
    ls->frame_started = true;
}

void agentite_lighting_render_lights(Agentite_LightingSystem *ls,
                                     SDL_GPUCommandBuffer *cmd,
                                     const Agentite_Camera *camera)
{
    if (!ls || !cmd || !ls->frame_started) return;

    /* Initialize shaders if needed */
    if (!ls->shaders_initialized) {
        create_gpu_resources(ls);
    }

    /* Create lightmap if needed */
    if (!ls->lightmap) {
        create_lightmap(ls, ls->lightmap_width, ls->lightmap_height);
    }

    if (!ls->lightmap || !ls->shaders_initialized) {
        ls->frame_started = false;
        return;
    }

    /* Calculate aspect ratio for correct circular falloff */
    float aspect_x = (float)ls->lightmap_width / (float)ls->lightmap_height;
    float aspect_y = 1.0f;

    /* ========================================================================
     * Shadow Map Generation (before render pass)
     *
     * For shadow-casting lights, we generate a shadow map containing the
     * distance to the nearest occluder at each angle from the light center.
     * This must happen before the render pass since it uses a copy pass.
     *
     * Supports up to MAX_SHADOW_CASTING_LIGHTS (8) lights with shadows.
     * Each light's shadow map is stored in a row of the shadow atlas texture.
     * ======================================================================== */

    float *shadow_distances = NULL;
    ls->active_shadow_light_count = 0;

    if (ls->config.enable_shadows && ls->point_light_shadow_shader && ls->occluder_count > 0) {
        /* Initialize shadow map resolution if needed */
        if (ls->shadow_map_resolution <= 0) {
            ls->shadow_map_resolution = ls->config.shadow_ray_count > 0
                ? ls->config.shadow_ray_count : 720;
        }

        /* Allocate reusable shadow distances array */
        shadow_distances = (float *)malloc((size_t)ls->shadow_map_resolution * sizeof(float));
        if (!shadow_distances) {
            SDL_Log("Lighting: Failed to allocate shadow distances array");
        } else {
            /* Generate shadow maps for ALL shadow-casting lights (up to MAX) */
            for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
                if (ls->active_shadow_light_count >= MAX_SHADOW_CASTING_LIGHTS) break;

                InternalPointLight *light = &ls->point_lights[i];
                if (!light->active || !light->enabled) continue;
                if (!light->desc.casts_shadows) continue;

                /* Generate shadow map for this light */
                generate_shadow_map(ls,
                                    light->desc.x, light->desc.y,
                                    light->desc.radius,
                                    shadow_distances);

                /* Upload to the appropriate row in the atlas */
                if (upload_shadow_map_row(ls, cmd, shadow_distances, ls->active_shadow_light_count)) {
                    /* Track which light is in which shadow slot */
                    ls->shadow_light_indices[ls->active_shadow_light_count] = (int)i;
                    ls->active_shadow_light_count++;
                } else {
                    SDL_Log("Lighting: Failed to upload shadow map for light %u", i);
                }
            }

            free(shadow_distances);
            shadow_distances = NULL;

            /* Log shadow light count on first frame with shadows */
            static bool logged_shadow_count = false;
            if (!logged_shadow_count && ls->active_shadow_light_count > 0) {
                SDL_Log("Lighting: %d shadow-casting point lights active, %u occluders",
                        ls->active_shadow_light_count, ls->occluder_count);
                logged_shadow_count = true;
            }
        }
    }

    /* ========================================================================
     * Shadow Map Generation for Spot Lights
     * ======================================================================== */

    ls->active_spot_shadow_light_count = 0;

    if (ls->config.enable_shadows && ls->spot_light_shadow_shader && ls->occluder_count > 0) {
        /* Initialize shadow map resolution if needed */
        if (ls->shadow_map_resolution <= 0) {
            ls->shadow_map_resolution = ls->config.shadow_ray_count > 0
                ? ls->config.shadow_ray_count : 720;
        }

        /* Allocate reusable shadow distances array */
        shadow_distances = (float *)malloc((size_t)ls->shadow_map_resolution * sizeof(float));
        if (!shadow_distances) {
            SDL_Log("Lighting: Failed to allocate shadow distances array for spot lights");
        } else {
            /* Generate shadow maps for shadow-casting spot lights */
            /* Spot lights use rows after point lights in the atlas */
            int base_row = ls->active_shadow_light_count;

            for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
                int row_index = base_row + ls->active_spot_shadow_light_count;
                if (row_index >= MAX_SHADOW_CASTING_LIGHTS) break;

                InternalSpotLight *light = &ls->spot_lights[i];
                if (!light->active || !light->enabled) continue;
                if (!light->desc.casts_shadows) continue;

                /* Generate shadow map for this spot light */
                generate_spot_shadow_map(ls,
                                         light->desc.x, light->desc.y,
                                         light->desc.radius,
                                         light->desc.direction_x, light->desc.direction_y,
                                         light->desc.outer_angle,
                                         shadow_distances);

                /* Upload to the appropriate row in the atlas */
                if (upload_shadow_map_row(ls, cmd, shadow_distances, row_index)) {
                    /* Track which spot light is in which shadow slot */
                    ls->spot_shadow_light_indices[ls->active_spot_shadow_light_count] = (int)i;
                    ls->active_spot_shadow_light_count++;
                } else {
                    SDL_Log("Lighting: Failed to upload shadow map for spot light %u", i);
                }
            }

            free(shadow_distances);
            shadow_distances = NULL;

            /* Log spot shadow light count */
            static bool logged_spot_shadow_count = false;
            if (!logged_spot_shadow_count && ls->active_spot_shadow_light_count > 0) {
                SDL_Log("Lighting: %d shadow-casting spot lights active",
                        ls->active_spot_shadow_light_count);
                logged_spot_shadow_count = true;
            }
        }
    }

    /* ========================================================================
     * Begin render pass to lightmap
     * ======================================================================== */

    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = ls->lightmap;
    color_target.clear_color.r = 0.0f;
    color_target.clear_color.g = 0.0f;
    color_target.clear_color.b = 0.0f;
    color_target.clear_color.a = 0.0f;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    if (!pass) {
        free(shadow_distances);
        ls->frame_started = false;
        return;
    }

    /* Set viewport to match lightmap dimensions (critical for HiDPI displays) */
    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = (float)ls->lightmap_width;
    viewport.h = (float)ls->lightmap_height;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(pass, &viewport);

    /* ========================================================================
     * Render Point Lights
     * ======================================================================== */

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        InternalPointLight *light = &ls->point_lights[i];
        if (!light->active || !light->enabled) continue;

        /* Convert world position to screen coordinates */
        float screen_x = light->desc.x;
        float screen_y = light->desc.y;

        /* Apply camera transform if provided */
        if (camera) {
            agentite_camera_world_to_screen((Agentite_Camera *)camera,
                light->desc.x, light->desc.y, &screen_x, &screen_y);
        }

        /* Convert screen coordinates to UV space (0-1) */
        float uv_x = screen_x / (float)ls->lightmap_width;
        float uv_y = screen_y / (float)ls->lightmap_height;

        /* Convert radius from world units to UV space */
        float radius_uv = light->desc.radius / (float)ls->lightmap_height;

        /* Look up if this light has a shadow slot */
        int shadow_slot = -1;
        for (int s = 0; s < ls->active_shadow_light_count; s++) {
            if (ls->shadow_light_indices[s] == (int)i) {
                shadow_slot = s;
                break;
            }
        }

        /* Check if this light uses the shadow shader */
        if (shadow_slot >= 0 && ls->shadow_map) {
            /* Use shadow shader with shadow map atlas texture */
            PointLightShadowUniforms shadow_params;
            shadow_params.light_center[0] = uv_x;
            shadow_params.light_center[1] = uv_y;
            shadow_params.radius = radius_uv;
            shadow_params.intensity = light->desc.intensity;
            shadow_params.color[0] = light->desc.color.r;
            shadow_params.color[1] = light->desc.color.g;
            shadow_params.color[2] = light->desc.color.b;
            shadow_params.color[3] = light->desc.color.a;
            shadow_params.falloff_type = (float)light->desc.falloff;
            shadow_params.shadow_softness = 4.0f;  /* Soft shadow edge in world units */
            shadow_params.aspect[0] = aspect_x;
            shadow_params.aspect[1] = aspect_y;
            shadow_params.lightmap_size[0] = (float)ls->lightmap_width;
            shadow_params.lightmap_size[1] = (float)ls->lightmap_height;
            shadow_params.radius_world = light->desc.radius;
            shadow_params.shadow_row = (float)shadow_slot;
            shadow_params.atlas_height = (float)MAX_SHADOW_CASTING_LIGHTS;
            shadow_params._pad[0] = 0.0f;
            shadow_params._pad[1] = 0.0f;
            shadow_params._pad[2] = 0.0f;

            /* Draw with shadow shader, passing shadow map atlas texture */
            agentite_shader_draw_fullscreen(ls->shader_system, cmd, pass,
                ls->point_light_shadow_shader, ls->shadow_map,
                &shadow_params, sizeof(shadow_params));
        } else {
            /* Use regular shader (no shadows) */
            PointLightUniforms params;
            params.light_center[0] = uv_x;
            params.light_center[1] = uv_y;
            params.radius = radius_uv;
            params.intensity = light->desc.intensity;
            params.color[0] = light->desc.color.r;
            params.color[1] = light->desc.color.g;
            params.color[2] = light->desc.color.b;
            params.color[3] = light->desc.color.a;
            params.falloff_type = (float)light->desc.falloff;
            params._pad_align = 0.0f;
            params.aspect[0] = aspect_x;
            params.aspect[1] = aspect_y;
            params._pad = 0.0f;

            agentite_shader_draw_fullscreen(ls->shader_system, cmd, pass,
                ls->point_light_shader, NULL, &params, sizeof(params));
        }
    }

    /* Step 2: Render each enabled spot light */
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        InternalSpotLight *light = &ls->spot_lights[i];
        if (!light->active || !light->enabled) continue;

        /* Convert world position to screen coordinates */
        float screen_x = light->desc.x;
        float screen_y = light->desc.y;

        /* Apply camera transform if provided */
        if (camera) {
            agentite_camera_world_to_screen((Agentite_Camera *)camera,
                light->desc.x, light->desc.y, &screen_x, &screen_y);
        }

        /* Convert screen coordinates to UV space (0-1) */
        float uv_x = screen_x / (float)ls->lightmap_width;
        float uv_y = screen_y / (float)ls->lightmap_height;

        /* Convert radius from world units to UV space */
        float radius_uv = light->desc.radius / (float)ls->lightmap_height;

        /* Look up if this spot light has a shadow slot */
        int shadow_slot = -1;
        for (int s = 0; s < ls->active_spot_shadow_light_count; s++) {
            if (ls->spot_shadow_light_indices[s] == (int)i) {
                /* Shadow slot is after point lights in the atlas */
                shadow_slot = ls->active_shadow_light_count + s;
                break;
            }
        }

        /* Check if this spot light uses the shadow shader */
        if (shadow_slot >= 0 && ls->shadow_map) {
            /* Use shadow shader with shadow map atlas texture */
            SpotLightShadowUniforms shadow_params;
            shadow_params.light_center[0] = uv_x;
            shadow_params.light_center[1] = uv_y;
            shadow_params.direction[0] = light->desc.direction_x;
            shadow_params.direction[1] = light->desc.direction_y;
            shadow_params.radius = radius_uv;
            shadow_params.inner_angle = cosf(light->desc.inner_angle);
            shadow_params.outer_angle = cosf(light->desc.outer_angle);
            shadow_params.intensity = light->desc.intensity;
            shadow_params.color[0] = light->desc.color.r;
            shadow_params.color[1] = light->desc.color.g;
            shadow_params.color[2] = light->desc.color.b;
            shadow_params.color[3] = light->desc.color.a;
            shadow_params.falloff_type = (float)light->desc.falloff;
            shadow_params.shadow_softness = 4.0f;  /* Soft shadow edge in world units */
            shadow_params.aspect[0] = aspect_x;
            shadow_params.aspect[1] = aspect_y;
            shadow_params.lightmap_size[0] = (float)ls->lightmap_width;
            shadow_params.lightmap_size[1] = (float)ls->lightmap_height;
            shadow_params.radius_world = light->desc.radius;
            shadow_params.shadow_row = (float)shadow_slot;
            shadow_params.atlas_height = (float)MAX_SHADOW_CASTING_LIGHTS;
            shadow_params.cone_angle_rad = light->desc.outer_angle;
            shadow_params._pad[0] = 0.0f;
            shadow_params._pad[1] = 0.0f;

            /* Draw with shadow shader, passing shadow map atlas texture */
            agentite_shader_draw_fullscreen(ls->shader_system, cmd, pass,
                ls->spot_light_shadow_shader, ls->shadow_map,
                &shadow_params, sizeof(shadow_params));
        } else {
            /* Use regular shader (no shadows) */
            SpotLightUniforms params;
            params.light_center[0] = uv_x;
            params.light_center[1] = uv_y;
            params.direction[0] = light->desc.direction_x;
            params.direction[1] = light->desc.direction_y;
            params.radius = radius_uv;
            params.inner_angle = cosf(light->desc.inner_angle);
            params.outer_angle = cosf(light->desc.outer_angle);
            params.intensity = light->desc.intensity;
            params.color[0] = light->desc.color.r;
            params.color[1] = light->desc.color.g;
            params.color[2] = light->desc.color.b;
            params.color[3] = light->desc.color.a;
            params.falloff_type = (float)light->desc.falloff;
            params._pad_align = 0.0f;
            params.aspect[0] = aspect_x;
            params.aspect[1] = aspect_y;

            /* Draw spot light using fullscreen shader with additive blending */
            agentite_shader_draw_fullscreen(ls->shader_system, cmd, pass,
                ls->spot_light_shader, NULL, &params, sizeof(params));
        }
    }

    SDL_EndGPURenderPass(pass);
    ls->frame_started = false;
}

void agentite_lighting_apply(Agentite_LightingSystem *ls,
                             SDL_GPUCommandBuffer *cmd,
                             SDL_GPURenderPass *pass,
                             SDL_GPUTexture *scene_texture)
{
    if (!ls || !cmd || !pass || !scene_texture) return;

    /* Ensure we have the necessary resources */
    if (!ls->composite_shader || !ls->lightmap || !ls->sampler) return;

    /* Get the pipeline from the composite shader */
    SDL_GPUGraphicsPipeline *pipeline = agentite_shader_get_pipeline(ls->composite_shader);
    if (!pipeline) return;

    /* Bind composite shader pipeline */
    SDL_BindGPUGraphicsPipeline(pass, pipeline);

    /* Bind both textures: scene at slot 0, lightmap at slot 1 */
    SDL_GPUTextureSamplerBinding bindings[2] = {};
    bindings[0].texture = scene_texture;
    bindings[0].sampler = ls->sampler;
    bindings[1].texture = ls->lightmap;
    bindings[1].sampler = ls->sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, bindings, 2);

    /* Build composite uniform data */
    CompositeUniforms params;
    params.ambient[0] = ls->ambient.r;
    params.ambient[1] = ls->ambient.g;
    params.ambient[2] = ls->ambient.b;
    params.ambient[3] = ls->ambient.a;
    params.blend_mode = (float)ls->config.blend;
    params._pad[0] = 0.0f;
    params._pad[1] = 0.0f;
    params._pad[2] = 0.0f;

    /* Push uniform data */
    SDL_PushGPUFragmentUniformData(cmd, 0, &params, sizeof(params));

    /* Bind fullscreen quad vertex buffer and draw */
    SDL_GPUBuffer *quad_buffer = agentite_shader_get_quad_buffer(ls->shader_system);
    if (quad_buffer) {
        SDL_GPUBufferBinding vb_binding = {};
        vb_binding.buffer = quad_buffer;
        vb_binding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);  /* 6 vertices for fullscreen quad */
    }
}

SDL_GPUTexture *agentite_lighting_get_lightmap(const Agentite_LightingSystem *ls)
{
    return ls ? ls->lightmap : NULL;
}

/* ============================================================================
 * Light Management
 * ============================================================================ */

void agentite_lighting_clear_lights(Agentite_LightingSystem *ls)
{
    if (!ls) return;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        ls->point_lights[i].active = false;
    }
    ls->point_light_count = 0;

    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        ls->spot_lights[i].active = false;
    }
    ls->spot_light_count = 0;

    ls->directional_enabled = false;
}

void agentite_lighting_set_light_enabled(Agentite_LightingSystem *ls,
                                         uint32_t light_id,
                                         bool enabled)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return;

    /* Check point lights */
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            ls->point_lights[i].enabled = enabled;
            return;
        }
    }

    /* Check spot lights */
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (ls->spot_lights[i].active && ls->spot_lights[i].id == light_id) {
            ls->spot_lights[i].enabled = enabled;
            return;
        }
    }
}

bool agentite_lighting_is_light_enabled(const Agentite_LightingSystem *ls,
                                        uint32_t light_id)
{
    if (!ls || light_id == LIGHT_ID_INVALID) return false;

    /* Check point lights */
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_point_lights; i++) {
        if (ls->point_lights[i].active && ls->point_lights[i].id == light_id) {
            return ls->point_lights[i].enabled;
        }
    }

    /* Check spot lights */
    for (uint32_t i = 0; i < (uint32_t)ls->config.max_spot_lights; i++) {
        if (ls->spot_lights[i].active && ls->spot_lights[i].id == light_id) {
            return ls->spot_lights[i].enabled;
        }
    }

    return false;
}

/* ============================================================================
 * Day/Night Cycle
 * ============================================================================ */

static float lerp_color_channel(float a, float b, float t)
{
    return a + (b - a) * t;
}

static void lerp_color(const Agentite_LightColor *a, const Agentite_LightColor *b,
                       float t, Agentite_LightColor *out)
{
    out->r = lerp_color_channel(a->r, b->r, t);
    out->g = lerp_color_channel(a->g, b->g, t);
    out->b = lerp_color_channel(a->b, b->b, t);
    out->a = lerp_color_channel(a->a, b->a, t);
}

void agentite_lighting_update_time_of_day(Agentite_LightingSystem *ls,
                                          const Agentite_TimeOfDay *tod)
{
    if (!ls || !tod) return;

    /* Normalize time to 0-24 range */
    float time = fmodf(tod->time, 24.0f);
    if (time < 0) time += 24.0f;

    float sunrise_start = tod->sunrise_hour;
    float sunrise_end = tod->sunrise_hour + tod->transition_hours;
    float sunset_start = tod->sunset_hour;
    float sunset_end = tod->sunset_hour + tod->transition_hours;

    Agentite_LightColor ambient;
    Agentite_LightColor sun_color;
    float sun_intensity = 0.0f;
    bool is_day = false;

    if (time >= sunrise_end && time < sunset_start) {
        /* Full day */
        ambient = tod->ambient_day;
        sun_color = tod->sun_color;
        sun_intensity = 1.0f;
        is_day = true;
    } else if (time >= sunset_end || time < sunrise_start) {
        /* Full night */
        ambient = tod->ambient_night;
        sun_color = tod->moon_color;
        sun_intensity = 0.3f;
        is_day = false;
    } else if (time >= sunrise_start && time < sunrise_end) {
        /* Sunrise transition */
        float t = (time - sunrise_start) / tod->transition_hours;
        lerp_color(&tod->ambient_night, &tod->ambient_day, t, &ambient);
        lerp_color(&tod->moon_color, &tod->sunset_color, t, &sun_color);
        sun_intensity = 0.3f + 0.7f * t;
        is_day = t > 0.5f;
    } else {
        /* Sunset transition */
        float t = (time - sunset_start) / tod->transition_hours;
        lerp_color(&tod->ambient_day, &tod->ambient_night, t, &ambient);
        lerp_color(&tod->sun_color, &tod->sunset_color, 1.0f - t, &sun_color);
        sun_intensity = 1.0f - 0.7f * t;
        is_day = t < 0.5f;
    }

    /* Apply ambient */
    agentite_lighting_set_ambient(ls, ambient.r, ambient.g, ambient.b, ambient.a);

    /* is_day can be used for future features like shadow direction */
    (void)is_day;

    /* Apply directional light */
    Agentite_DirectionalLightDesc dir = AGENTITE_DIRECTIONAL_LIGHT_DEFAULT;
    dir.color = sun_color;
    dir.intensity = sun_intensity;

    /* Calculate sun/moon direction based on time */
    float angle = (float)M_PI * ((time - 6.0f) / 12.0f); /* 0 at sunrise, PI at sunset */
    dir.direction_x = cosf(angle);
    dir.direction_y = sinf(angle);

    agentite_lighting_set_directional(ls, &dir);
}

void agentite_lighting_advance_time(Agentite_LightingSystem *ls,
                                    Agentite_TimeOfDay *tod,
                                    float delta_hours)
{
    if (!ls || !tod) return;

    tod->time = fmodf(tod->time + delta_hours, 24.0f);
    if (tod->time < 0) tod->time += 24.0f;

    agentite_lighting_update_time_of_day(ls, tod);
}

/* ============================================================================
 * Statistics and Debug
 * ============================================================================ */

void agentite_lighting_get_stats(const Agentite_LightingSystem *ls,
                                 Agentite_LightingStats *stats)
{
    if (!ls || !stats) return;

    stats->point_light_count = ls->point_light_count;
    stats->spot_light_count = ls->spot_light_count;
    stats->occluder_count = ls->occluder_count;
    stats->max_point_lights = (uint32_t)ls->config.max_point_lights;
    stats->max_spot_lights = (uint32_t)ls->config.max_spot_lights;
    stats->max_occluders = (uint32_t)ls->config.max_occluders;
    stats->lightmap_width = ls->lightmap_width;
    stats->lightmap_height = ls->lightmap_height;
    stats->shadows_enabled = ls->config.enable_shadows;
}

void agentite_lighting_set_blend_mode(Agentite_LightingSystem *ls,
                                      Agentite_LightBlendMode mode)
{
    if (!ls) return;
    ls->config.blend = mode;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static bool create_gpu_resources(Agentite_LightingSystem *ls)
{
    if (!ls || !ls->shader_system || ls->shaders_initialized) {
        return ls && ls->shaders_initialized;
    }

    /* Check for shader format support */
    SDL_GPUShaderFormat formats = agentite_shader_get_formats(ls->shader_system);
    bool has_spirv = (formats & SDL_GPU_SHADERFORMAT_SPIRV) != 0;
    bool has_msl = (formats & SDL_GPU_SHADERFORMAT_MSL) != 0;

    /* Try SPIR-V first (Vulkan, D3D12) */
    if (has_spirv) {
        SDL_Log("Lighting: Loading SPIR-V shaders for Vulkan/D3D12");

        Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
        desc.num_fragment_uniforms = 1;
        desc.num_fragment_samplers = 0;
        desc.blend_mode = AGENTITE_BLEND_ADDITIVE;

        /* Point light shader */
        ls->point_light_shader = agentite_shader_load_spirv(ls->shader_system,
            "assets/shaders/lighting/lighting.vert.spv",
            "assets/shaders/lighting/point_light.frag.spv",
            &desc);
        if (!ls->point_light_shader) {
            SDL_Log("Lighting: Failed to create point light shader: %s",
                    agentite_get_last_error());
        }

        /* Spot light shader */
        ls->spot_light_shader = agentite_shader_load_spirv(ls->shader_system,
            "assets/shaders/lighting/lighting.vert.spv",
            "assets/shaders/lighting/spot_light.frag.spv",
            &desc);
        if (!ls->spot_light_shader) {
            SDL_Log("Lighting: Failed to create spot light shader: %s",
                    agentite_get_last_error());
        }

        /* Composite shader (uses scene texture + light texture) */
        desc.num_fragment_samplers = 2;
        desc.blend_mode = AGENTITE_BLEND_NONE;
        ls->composite_shader = agentite_shader_load_spirv(ls->shader_system,
            "assets/shaders/lighting/lighting.vert.spv",
            "assets/shaders/lighting/composite.frag.spv",
            &desc);
        if (!ls->composite_shader) {
            SDL_Log("Lighting: Failed to create composite shader: %s",
                    agentite_get_last_error());
        }

        /* Ambient shader */
        desc.num_fragment_samplers = 0;
        ls->ambient_shader = agentite_shader_load_spirv(ls->shader_system,
            "assets/shaders/lighting/lighting.vert.spv",
            "assets/shaders/lighting/ambient.frag.spv",
            &desc);
        if (!ls->ambient_shader) {
            SDL_Log("Lighting: Failed to create ambient shader: %s",
                    agentite_get_last_error());
        }

        /* Point light shadow shader (samples from shadow map texture) */
        desc.num_fragment_samplers = 1;
        desc.blend_mode = AGENTITE_BLEND_ADDITIVE;
        ls->point_light_shadow_shader = agentite_shader_load_spirv(ls->shader_system,
            "assets/shaders/lighting/lighting.vert.spv",
            "assets/shaders/lighting/point_light_shadow.frag.spv",
            &desc);
        if (!ls->point_light_shadow_shader) {
            SDL_Log("Lighting: Failed to create point light shadow shader: %s",
                    agentite_get_last_error());
        }

        /* Spot light shadow shader (samples from shadow map texture) */
        ls->spot_light_shadow_shader = agentite_shader_load_spirv(ls->shader_system,
            "assets/shaders/lighting/lighting.vert.spv",
            "assets/shaders/lighting/spot_light_shadow.frag.spv",
            &desc);
        if (!ls->spot_light_shadow_shader) {
            SDL_Log("Lighting: Failed to create spot light shadow shader: %s",
                    agentite_get_last_error());
        }
    }
    /* Fall back to MSL (Metal on macOS/iOS) */
    else if (has_msl) {
        Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
        desc.num_fragment_uniforms = 1;
        desc.num_fragment_samplers = 0;
        desc.blend_mode = AGENTITE_BLEND_ADDITIVE;
        desc.vertex_entry = "lighting_vertex";
        desc.fragment_entry = "point_light_fragment";

        /* Create point light shader */
        ls->point_light_shader = agentite_shader_load_msl(ls->shader_system,
                                                           point_light_msl,
                                                           &desc);
        if (!ls->point_light_shader) {
            SDL_Log("Lighting: Failed to create point light shader: %s",
                    agentite_get_last_error());
        }

        /* Create spot light shader */
        desc.fragment_entry = "spot_light_fragment";
        ls->spot_light_shader = agentite_shader_load_msl(ls->shader_system,
                                                          spot_light_msl,
                                                          &desc);
        if (!ls->spot_light_shader) {
            SDL_Log("Lighting: Failed to create spot light shader: %s",
                    agentite_get_last_error());
        }

        /* Create composite shader (uses scene texture + light texture) */
        desc.num_fragment_samplers = 2;
        desc.blend_mode = AGENTITE_BLEND_NONE;
        desc.fragment_entry = "composite_fragment";
        ls->composite_shader = agentite_shader_load_msl(ls->shader_system,
                                                         composite_msl,
                                                         &desc);
        if (!ls->composite_shader) {
            SDL_Log("Lighting: Failed to create composite shader: %s",
                    agentite_get_last_error());
        }

        /* Create ambient shader */
        desc.num_fragment_samplers = 0;
        desc.fragment_entry = "ambient_fragment";
        ls->ambient_shader = agentite_shader_load_msl(ls->shader_system,
                                                       ambient_msl,
                                                       &desc);
        if (!ls->ambient_shader) {
            SDL_Log("Lighting: Failed to create ambient shader: %s",
                    agentite_get_last_error());
        }

        /* Create point light shadow shader (samples from shadow map texture) */
        desc.num_fragment_samplers = 1;
        desc.blend_mode = AGENTITE_BLEND_ADDITIVE;
        desc.fragment_entry = "point_light_shadow_fragment";
        ls->point_light_shadow_shader = agentite_shader_load_msl(ls->shader_system,
                                                                  point_light_shadow_msl,
                                                                  &desc);
        if (!ls->point_light_shadow_shader) {
            SDL_Log("Lighting: Failed to create point light shadow shader: %s",
                    agentite_get_last_error());
        }

        /* Create spot light shadow shader (samples from shadow map texture) */
        desc.fragment_entry = "spot_light_shadow_fragment";
        ls->spot_light_shadow_shader = agentite_shader_load_msl(ls->shader_system,
                                                                 spot_light_shadow_msl,
                                                                 &desc);
        if (!ls->spot_light_shadow_shader) {
            SDL_Log("Lighting: Failed to create spot light shadow shader: %s",
                    agentite_get_last_error());
        }
    }
    else {
        SDL_Log("Lighting: No supported shader format (need SPIR-V or MSL)");
        return true;
    }

    /* Create sampler */
    SDL_GPUSamplerCreateInfo sampler_info = {};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;

    ls->sampler = SDL_CreateGPUSampler(ls->gpu, &sampler_info);
    if (!ls->sampler) {
        SDL_Log("Lighting: Failed to create sampler: %s", SDL_GetError());
    }

    ls->shaders_initialized = true;
    return true;
}

static void destroy_gpu_resources(Agentite_LightingSystem *ls)
{
    if (!ls || !ls->gpu) return;

    if (ls->lightmap) {
        SDL_ReleaseGPUTexture(ls->gpu, ls->lightmap);
        ls->lightmap = NULL;
    }

    if (ls->shadow_map) {
        SDL_ReleaseGPUTexture(ls->gpu, ls->shadow_map);
        ls->shadow_map = NULL;
    }

    if (ls->quad_vertex_buffer) {
        SDL_ReleaseGPUBuffer(ls->gpu, ls->quad_vertex_buffer);
        ls->quad_vertex_buffer = NULL;
    }

    if (ls->sampler) {
        SDL_ReleaseGPUSampler(ls->gpu, ls->sampler);
        ls->sampler = NULL;
    }

    /* Destroy shaders (shader system owns them) */
    if (ls->shader_system) {
        if (ls->point_light_shader) {
            agentite_shader_destroy(ls->shader_system, ls->point_light_shader);
            ls->point_light_shader = NULL;
        }
        if (ls->spot_light_shader) {
            agentite_shader_destroy(ls->shader_system, ls->spot_light_shader);
            ls->spot_light_shader = NULL;
        }
        if (ls->composite_shader) {
            agentite_shader_destroy(ls->shader_system, ls->composite_shader);
            ls->composite_shader = NULL;
        }
        if (ls->ambient_shader) {
            agentite_shader_destroy(ls->shader_system, ls->ambient_shader);
            ls->ambient_shader = NULL;
        }
        if (ls->point_light_shadow_shader) {
            agentite_shader_destroy(ls->shader_system, ls->point_light_shadow_shader);
            ls->point_light_shadow_shader = NULL;
        }
        if (ls->spot_light_shadow_shader) {
            agentite_shader_destroy(ls->shader_system, ls->spot_light_shadow_shader);
            ls->spot_light_shadow_shader = NULL;
        }
    }

    ls->shaders_initialized = false;
}

static bool create_lightmap(Agentite_LightingSystem *ls, int width, int height)
{
    if (!ls || !ls->gpu) return false;

    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = ls->config.format;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    ls->lightmap = SDL_CreateGPUTexture(ls->gpu, &tex_info);
    if (!ls->lightmap) {
        agentite_set_error_from_sdl("Lighting: Failed to create lightmap texture");
        return false;
    }

    ls->lightmap_width = width;
    ls->lightmap_height = height;

    return true;
}

/* Silence unused function warning until GPU rendering is fully implemented */
#ifdef __GNUC__
__attribute__((unused))
#endif
static void render_point_light(Agentite_LightingSystem *ls,
                               SDL_GPUCommandBuffer *cmd,
                               SDL_GPURenderPass *pass,
                               const InternalPointLight *light,
                               const Agentite_Camera *camera)
{
    /* TODO: Render single point light as a quad with radial falloff shader */
    (void)ls;
    (void)cmd;
    (void)pass;
    (void)light;
    (void)camera;
}

/* Silence unused function warning until GPU rendering is implemented */
#ifdef __GNUC__
__attribute__((unused))
#endif
static float apply_falloff(float dist, float radius, Agentite_LightFalloff falloff)
{
    if (dist >= radius) return 0.0f;

    float normalized = dist / radius;

    switch (falloff) {
        case AGENTITE_FALLOFF_LINEAR:
            return 1.0f - normalized;

        case AGENTITE_FALLOFF_QUADRATIC:
            return 1.0f / (1.0f + normalized * normalized * 4.0f);

        case AGENTITE_FALLOFF_SMOOTH:
            /* Hermite smoothstep */
            return 1.0f - normalized * normalized * (3.0f - 2.0f * normalized);

        case AGENTITE_FALLOFF_NONE:
            return 1.0f;

        default:
            return 1.0f - normalized;
    }
}

/* ============================================================================
 * Shadow Casting - Ray-Occluder Intersection Functions
 * ============================================================================ */

/*
 * Ray-segment intersection using parametric line intersection.
 * Ray: P = O + t*D where t >= 0
 * Segment: Q = A + s*(B-A) where 0 <= s <= 1
 *
 * Returns distance to intersection point, or FLT_MAX if no intersection.
 */
static float ray_segment_intersect(float ox, float oy, float dx, float dy,
                                   float x1, float y1, float x2, float y2)
{
    /* Segment direction */
    float sx = x2 - x1;
    float sy = y2 - y1;

    /* Cross product of directions: D x S */
    float cross = dx * sy - dy * sx;

    /* If cross is near zero, lines are parallel */
    if (fabsf(cross) < 1e-8f) {
        return FLT_MAX;
    }

    /* Vector from ray origin to segment start */
    float qx = x1 - ox;
    float qy = y1 - oy;

    /* Parameter t for ray: intersection at O + t*D */
    float t = (qx * sy - qy * sx) / cross;

    /* Parameter s for segment: intersection at A + s*(B-A) */
    float s = (qx * dy - qy * dx) / cross;

    /* Check if intersection is valid (ray forward, within segment) */
    if (t >= 0.0f && s >= 0.0f && s <= 1.0f) {
        return t;  /* Distance along ray */
    }

    return FLT_MAX;
}

/*
 * Ray-box intersection by testing all 4 edges.
 * Box is defined by top-left corner (bx, by) and dimensions (bw, bh).
 *
 * Returns distance to nearest intersection, or FLT_MAX if no intersection.
 */
static float ray_box_intersect(float ox, float oy, float dx, float dy,
                               float bx, float by, float bw, float bh)
{
    float min_dist = FLT_MAX;

    /* Box corners */
    float x1 = bx;
    float y1 = by;
    float x2 = bx + bw;
    float y2 = by + bh;

    /* Test all 4 edges */
    float d;

    /* Top edge: (x1,y1) to (x2,y1) */
    d = ray_segment_intersect(ox, oy, dx, dy, x1, y1, x2, y1);
    if (d < min_dist) min_dist = d;

    /* Bottom edge: (x1,y2) to (x2,y2) */
    d = ray_segment_intersect(ox, oy, dx, dy, x1, y2, x2, y2);
    if (d < min_dist) min_dist = d;

    /* Left edge: (x1,y1) to (x1,y2) */
    d = ray_segment_intersect(ox, oy, dx, dy, x1, y1, x1, y2);
    if (d < min_dist) min_dist = d;

    /* Right edge: (x2,y1) to (x2,y2) */
    d = ray_segment_intersect(ox, oy, dx, dy, x2, y1, x2, y2);
    if (d < min_dist) min_dist = d;

    return min_dist;
}

/*
 * Ray-circle intersection using quadratic formula.
 * Circle at (cx, cy) with radius r.
 * Ray: P = O + t*D
 *
 * Solve: |O + t*D - C|^2 = r^2
 * Expanding: t^2*(D.D) + 2t*(D.(O-C)) + (O-C).(O-C) - r^2 = 0
 *
 * Returns distance to nearest intersection (in front of ray), or FLT_MAX if none.
 */
static float ray_circle_intersect(float ox, float oy, float dx, float dy,
                                  float cx, float cy, float r)
{
    /* Vector from circle center to ray origin */
    float ocx = ox - cx;
    float ocy = oy - cy;

    /* Quadratic coefficients: at^2 + bt + c = 0 */
    float a = dx * dx + dy * dy;
    float b = 2.0f * (dx * ocx + dy * ocy);
    float c = ocx * ocx + ocy * ocy - r * r;

    /* Discriminant */
    float discriminant = b * b - 4.0f * a * c;

    if (discriminant < 0.0f) {
        return FLT_MAX;  /* No intersection */
    }

    float sqrt_disc = sqrtf(discriminant);
    float inv_2a = 1.0f / (2.0f * a);

    /* Two possible solutions */
    float t1 = (-b - sqrt_disc) * inv_2a;
    float t2 = (-b + sqrt_disc) * inv_2a;

    /* Return nearest positive t */
    if (t1 >= 0.0f) {
        return t1;
    }
    if (t2 >= 0.0f) {
        return t2;
    }

    return FLT_MAX;  /* Both behind ray origin */
}

/*
 * Generate shadow map for a single light.
 * Casts rays from light position in all directions (720 rays for 0.5 degree resolution).
 * Stores distance to nearest occluder for each ray angle.
 *
 * shadow_distances: output array of size ls->shadow_map_resolution
 */
static void generate_shadow_map(Agentite_LightingSystem *ls,
                                float light_x, float light_y,
                                float light_radius,
                                float *shadow_distances)
{
    if (!ls || !shadow_distances) return;

    int ray_count = ls->shadow_map_resolution;
    if (ray_count <= 0) ray_count = 720;  /* Default to 720 rays */

    float angle_step = (float)(2.0 * M_PI) / (float)ray_count;

    /* For each ray angle */
    for (int i = 0; i < ray_count; i++) {
        float angle = (float)i * angle_step;
        float dx = cosf(angle);
        float dy = sinf(angle);

        float min_dist = light_radius;  /* Start with max range */

        /* Test against all active occluders */
        for (uint32_t j = 0; j < (uint32_t)ls->config.max_occluders; j++) {
            if (!ls->occluders[j].active) continue;

            const Agentite_Occluder *occ = &ls->occluders[j].occluder;
            float dist = FLT_MAX;

            switch (occ->type) {
                case AGENTITE_OCCLUDER_SEGMENT:
                    dist = ray_segment_intersect(light_x, light_y, dx, dy,
                                                 occ->segment.x1, occ->segment.y1,
                                                 occ->segment.x2, occ->segment.y2);
                    break;

                case AGENTITE_OCCLUDER_BOX:
                    dist = ray_box_intersect(light_x, light_y, dx, dy,
                                             occ->box.x, occ->box.y,
                                             occ->box.w, occ->box.h);
                    break;

                case AGENTITE_OCCLUDER_CIRCLE:
                    dist = ray_circle_intersect(light_x, light_y, dx, dy,
                                                occ->circle.x, occ->circle.y,
                                                occ->circle.radius);
                    break;
            }

            if (dist < min_dist) {
                min_dist = dist;
            }
        }

        shadow_distances[i] = min_dist;
    }
}

/*
 * Generate shadow map for a spot light.
 * Casts rays within the cone angle from light position.
 * The rays cover the full outer cone angle, mapped to the full shadow map resolution.
 *
 * shadow_distances: output array of size ls->shadow_map_resolution
 * dir_x, dir_y: normalized direction of the spot light
 * outer_angle: outer cone half-angle in radians
 */
static void generate_spot_shadow_map(Agentite_LightingSystem *ls,
                                     float light_x, float light_y,
                                     float light_radius,
                                     float dir_x, float dir_y,
                                     float outer_angle,
                                     float *shadow_distances)
{
    if (!ls || !shadow_distances) return;

    int ray_count = ls->shadow_map_resolution;
    if (ray_count <= 0) ray_count = 720;  /* Default to 720 rays */

    /* Calculate base direction angle */
    float dir_angle = atan2f(dir_y, dir_x);

    /* Rays span from -outer_angle to +outer_angle relative to direction */
    float total_angle = 2.0f * outer_angle;
    float angle_step = total_angle / (float)(ray_count - 1);

    /* For each ray within the cone */
    for (int i = 0; i < ray_count; i++) {
        /* Calculate angle relative to direction: -outer_angle to +outer_angle */
        float rel_angle = -outer_angle + (float)i * angle_step;
        float angle = dir_angle + rel_angle;

        float dx = cosf(angle);
        float dy = sinf(angle);

        float min_dist = light_radius;  /* Start with max range */

        /* Test against all active occluders */
        for (uint32_t j = 0; j < (uint32_t)ls->config.max_occluders; j++) {
            if (!ls->occluders[j].active) continue;

            const Agentite_Occluder *occ = &ls->occluders[j].occluder;
            float dist = FLT_MAX;

            switch (occ->type) {
                case AGENTITE_OCCLUDER_SEGMENT:
                    dist = ray_segment_intersect(light_x, light_y, dx, dy,
                                                 occ->segment.x1, occ->segment.y1,
                                                 occ->segment.x2, occ->segment.y2);
                    break;

                case AGENTITE_OCCLUDER_BOX:
                    dist = ray_box_intersect(light_x, light_y, dx, dy,
                                             occ->box.x, occ->box.y,
                                             occ->box.w, occ->box.h);
                    break;

                case AGENTITE_OCCLUDER_CIRCLE:
                    dist = ray_circle_intersect(light_x, light_y, dx, dy,
                                                occ->circle.x, occ->circle.y,
                                                occ->circle.radius);
                    break;
            }

            if (dist < min_dist) {
                min_dist = dist;
            }
        }

        shadow_distances[i] = min_dist;
    }
}

/*
 * Create the shadow map atlas texture.
 * Format: R32_FLOAT, dimensions: shadow_map_resolution x MAX_SHADOW_CASTING_LIGHTS
 * Each row stores the shadow distances for one light.
 */
static bool create_shadow_map_texture(Agentite_LightingSystem *ls)
{
    if (!ls || !ls->gpu) return false;

    /* Release existing shadow map if any */
    if (ls->shadow_map) {
        SDL_ReleaseGPUTexture(ls->gpu, ls->shadow_map);
        ls->shadow_map = NULL;
    }

    /* Use config or default */
    int resolution = ls->config.shadow_ray_count;
    if (resolution <= 0) resolution = 720;
    ls->shadow_map_resolution = resolution;

    /* Create 2D atlas texture (resolution x MAX_SHADOW_CASTING_LIGHTS)
     * Each row stores shadow distances for one light */
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R32_FLOAT;
    tex_info.width = (Uint32)resolution;
    tex_info.height = MAX_SHADOW_CASTING_LIGHTS;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;  /* Copy pass upload works with this */

    ls->shadow_map = SDL_CreateGPUTexture(ls->gpu, &tex_info);
    if (!ls->shadow_map) {
        SDL_Log("Lighting: Failed to create shadow map atlas texture: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Lighting: Created shadow map atlas %dx%d", resolution, MAX_SHADOW_CASTING_LIGHTS);
    return true;
}

/*
 * Upload shadow distances to the shadow map texture.
 * Uses a transfer buffer to copy CPU data to GPU texture.
 */
static bool upload_shadow_map(Agentite_LightingSystem *ls,
                              SDL_GPUCommandBuffer *cmd,
                              const float *shadow_distances)
{
    if (!ls || !cmd || !shadow_distances) return false;

    /* Create shadow map texture if needed */
    if (!ls->shadow_map) {
        if (!create_shadow_map_texture(ls)) {
            return false;
        }
    }

    int resolution = ls->shadow_map_resolution;
    size_t data_size = (size_t)resolution * sizeof(float);

    /* Create transfer buffer for upload */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)data_size;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ls->gpu, &transfer_info);
    if (!transfer) {
        SDL_Log("Lighting: Failed to create shadow map transfer buffer");
        return false;
    }

    /* Map and copy data */
    void *mapped = SDL_MapGPUTransferBuffer(ls->gpu, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(ls->gpu, transfer);
        return false;
    }
    memcpy(mapped, shadow_distances, data_size);
    SDL_UnmapGPUTransferBuffer(ls->gpu, transfer);

    /* Begin copy pass */
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_ReleaseGPUTransferBuffer(ls->gpu, transfer);
        return false;
    }

    /* Upload to texture */
    SDL_GPUTextureTransferInfo src = {};
    src.transfer_buffer = transfer;
    src.offset = 0;
    src.pixels_per_row = (Uint32)resolution;
    src.rows_per_layer = 1;

    SDL_GPUTextureRegion dst = {};
    dst.texture = ls->shadow_map;
    dst.w = (Uint32)resolution;
    dst.h = 1;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    /* Release transfer buffer */
    SDL_ReleaseGPUTransferBuffer(ls->gpu, transfer);

    return true;
}

/*
 * Upload shadow distances to a specific row of the shadow map atlas.
 * row_index: Which row (0 to MAX_SHADOW_CASTING_LIGHTS-1)
 */
static bool upload_shadow_map_row(Agentite_LightingSystem *ls,
                                  SDL_GPUCommandBuffer *cmd,
                                  const float *shadow_distances,
                                  int row_index)
{
    if (!ls || !cmd || !shadow_distances) return false;
    if (row_index < 0 || row_index >= MAX_SHADOW_CASTING_LIGHTS) return false;

    /* Create shadow map texture if needed */
    if (!ls->shadow_map) {
        if (!create_shadow_map_texture(ls)) {
            return false;
        }
    }

    int resolution = ls->shadow_map_resolution;
    size_t data_size = (size_t)resolution * sizeof(float);

    /* Create transfer buffer for upload */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)data_size;

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ls->gpu, &transfer_info);
    if (!transfer) {
        SDL_Log("Lighting: Failed to create shadow map transfer buffer for row %d", row_index);
        return false;
    }

    /* Map and copy data */
    void *mapped = SDL_MapGPUTransferBuffer(ls->gpu, transfer, false);
    if (!mapped) {
        SDL_ReleaseGPUTransferBuffer(ls->gpu, transfer);
        return false;
    }
    memcpy(mapped, shadow_distances, data_size);
    SDL_UnmapGPUTransferBuffer(ls->gpu, transfer);

    /* Begin copy pass */
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        SDL_ReleaseGPUTransferBuffer(ls->gpu, transfer);
        return false;
    }

    /* Upload to specific row of atlas texture */
    SDL_GPUTextureTransferInfo src = {};
    src.transfer_buffer = transfer;
    src.offset = 0;
    src.pixels_per_row = (Uint32)resolution;
    src.rows_per_layer = 1;

    SDL_GPUTextureRegion dst = {};
    dst.texture = ls->shadow_map;
    dst.x = 0;
    dst.y = (Uint32)row_index;  /* Target specific row */
    dst.w = (Uint32)resolution;
    dst.h = 1;
    dst.d = 1;

    SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    /* Release transfer buffer */
    SDL_ReleaseGPUTransferBuffer(ls->gpu, transfer);

    return true;
}
