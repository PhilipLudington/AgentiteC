/*
 * Agentite Shader System
 *
 * A flexible shader abstraction layer built on SDL_GPU that provides:
 * - Loading shaders from SPIR-V files or embedded bytecode
 * - Automatic format selection (Metal MSL, SPIRV, DXIL)
 * - Uniform buffer management
 * - Post-processing pipeline support
 * - Built-in effect shaders (grayscale, blur, glow, outline, etc.)
 *
 * Basic Usage:
 *   Agentite_ShaderSystem *ss = agentite_shader_create(gpu);
 *
 *   // Load a custom shader
 *   Agentite_Shader *shader = agentite_shader_load_spirv(ss,
 *       "shaders/custom.vert.spv", "shaders/custom.frag.spv", &desc);
 *
 *   // Or load embedded bytecode
 *   Agentite_Shader *shader = agentite_shader_load_memory(ss,
 *       vert_spv, vert_spv_len, frag_spv, frag_spv_len, &desc);
 *
 *   // Use built-in post-process effect
 *   Agentite_Shader *grayscale = agentite_shader_get_builtin(ss,
 *       AGENTITE_SHADER_GRAYSCALE);
 *
 *   // In render loop - apply post-process
 *   agentite_postprocess_begin(pp, cmd, source_texture);
 *   agentite_postprocess_apply(pp, cmd, pass, grayscale, NULL);
 *   agentite_postprocess_end(pp, cmd, pass);
 *
 *   agentite_shader_destroy(ss, shader);
 *   agentite_shader_system_destroy(ss);
 *
 * Thread Safety:
 *   - Shader creation/destruction: NOT thread-safe (main thread only)
 *   - Shader parameter updates: NOT thread-safe
 *   - All rendering operations: NOT thread-safe (main thread only)
 */

#ifndef AGENTITE_SHADER_H
#define AGENTITE_SHADER_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Opaque shader system handle */
typedef struct Agentite_ShaderSystem Agentite_ShaderSystem;

/* Opaque shader handle */
typedef struct Agentite_Shader Agentite_Shader;

/* Opaque post-processing pipeline handle */
typedef struct Agentite_PostProcess Agentite_PostProcess;

/* Opaque uniform buffer handle */
typedef struct Agentite_UniformBuffer Agentite_UniformBuffer;

/* Built-in shader effects */
typedef enum Agentite_BuiltinShader {
    AGENTITE_SHADER_NONE = 0,

    /* Color manipulation */
    AGENTITE_SHADER_GRAYSCALE,      /* Convert to grayscale */
    AGENTITE_SHADER_SEPIA,          /* Sepia tone effect */
    AGENTITE_SHADER_INVERT,         /* Invert colors */
    AGENTITE_SHADER_BRIGHTNESS,     /* Adjust brightness (param: amount) */
    AGENTITE_SHADER_CONTRAST,       /* Adjust contrast (param: amount) */
    AGENTITE_SHADER_SATURATION,     /* Adjust saturation (param: amount) */

    /* Blur effects */
    AGENTITE_SHADER_BLUR_BOX,       /* Simple box blur (param: radius) */
    AGENTITE_SHADER_BLUR_GAUSSIAN,  /* Gaussian blur (param: radius, sigma) */

    /* Screen effects */
    AGENTITE_SHADER_VIGNETTE,       /* Darkened edges (param: intensity, softness) */
    AGENTITE_SHADER_CHROMATIC,      /* Chromatic aberration (param: offset) */
    AGENTITE_SHADER_SCANLINES,      /* CRT scanlines (param: intensity, count) */
    AGENTITE_SHADER_PIXELATE,       /* Pixelation effect (param: pixel_size) */

    /* Outline/Edge */
    AGENTITE_SHADER_OUTLINE,        /* Edge outline (param: thickness, color) */
    AGENTITE_SHADER_SOBEL,          /* Sobel edge detection */

    /* Glow effects */
    AGENTITE_SHADER_GLOW,           /* Bloom/glow effect (param: threshold, intensity) */

    /* Game-specific */
    AGENTITE_SHADER_FLASH,          /* Flash white/color (param: color, intensity) */
    AGENTITE_SHADER_DISSOLVE,       /* Dissolve transition (param: progress, noise_tex) */

    AGENTITE_SHADER_BUILTIN_COUNT
} Agentite_BuiltinShader;

/* Shader stage (vertex or fragment) */
typedef enum Agentite_ShaderStage {
    AGENTITE_SHADER_STAGE_VERTEX = 0,
    AGENTITE_SHADER_STAGE_FRAGMENT = 1
} Agentite_ShaderStage;

/* Vertex attribute format */
typedef enum Agentite_VertexFormat {
    AGENTITE_VERTEX_FLOAT = 0,
    AGENTITE_VERTEX_FLOAT2,
    AGENTITE_VERTEX_FLOAT3,
    AGENTITE_VERTEX_FLOAT4,
    AGENTITE_VERTEX_INT,
    AGENTITE_VERTEX_INT2,
    AGENTITE_VERTEX_INT3,
    AGENTITE_VERTEX_INT4,
    AGENTITE_VERTEX_UBYTE4_NORM  /* 4 bytes normalized to 0-1 */
} Agentite_VertexFormat;

/* Blend mode for shaders */
typedef enum Agentite_BlendMode {
    AGENTITE_BLEND_NONE = 0,        /* No blending (opaque) */
    AGENTITE_BLEND_ALPHA,           /* Standard alpha blending */
    AGENTITE_BLEND_ADDITIVE,        /* Additive blending */
    AGENTITE_BLEND_MULTIPLY,        /* Multiply blending */
    AGENTITE_BLEND_PREMULTIPLIED    /* Premultiplied alpha */
} Agentite_BlendMode;

/* Single vertex attribute description */
typedef struct Agentite_VertexAttribute {
    uint32_t location;              /* Shader input location */
    Agentite_VertexFormat format;   /* Data format */
    uint32_t offset;                /* Byte offset in vertex struct */
} Agentite_VertexAttribute;

/* Vertex layout description */
typedef struct Agentite_VertexLayout {
    uint32_t stride;                        /* Bytes per vertex */
    const Agentite_VertexAttribute *attrs;  /* Array of attributes */
    uint32_t attr_count;                    /* Number of attributes */
} Agentite_VertexLayout;

/* Shader creation description */
typedef struct Agentite_ShaderDesc {
    /* Resource counts (must match shader) */
    uint32_t num_vertex_uniforms;       /* Uniform buffers in vertex shader */
    uint32_t num_fragment_uniforms;     /* Uniform buffers in fragment shader */
    uint32_t num_vertex_samplers;       /* Texture samplers in vertex shader */
    uint32_t num_fragment_samplers;     /* Texture samplers in fragment shader */

    /* Uniform buffer sizes (0 = default 16 bytes) */
    uint32_t fragment_uniform_size;     /* Size of fragment uniform data in bytes */

    /* Vertex layout (NULL for fullscreen quad shaders) */
    const Agentite_VertexLayout *vertex_layout;

    /* Blend mode */
    Agentite_BlendMode blend_mode;

    /* Optional: target format (defaults to B8G8R8A8_UNORM) */
    SDL_GPUTextureFormat target_format;

    /* Entry points (defaults to "main" for SPIRV, varies for MSL) */
    const char *vertex_entry;
    const char *fragment_entry;
} Agentite_ShaderDesc;

/* Default shader description */
#define AGENTITE_SHADER_DESC_DEFAULT { \
    .num_vertex_uniforms = 0,          \
    .num_fragment_uniforms = 0,        \
    .num_vertex_samplers = 0,          \
    .num_fragment_samplers = 1,        \
    .fragment_uniform_size = 0,        \
    .vertex_layout = NULL,             \
    .blend_mode = AGENTITE_BLEND_ALPHA,\
    .target_format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM, \
    .vertex_entry = NULL,              \
    .fragment_entry = NULL             \
}

/* Post-processing configuration */
typedef struct Agentite_PostProcessConfig {
    int width;                      /* Render target width (0 = window size) */
    int height;                     /* Render target height (0 = window size) */
    bool use_intermediate;          /* Create intermediate buffer for chaining */
    SDL_GPUTextureFormat format;    /* Render target format */
} Agentite_PostProcessConfig;

#define AGENTITE_POSTPROCESS_CONFIG_DEFAULT { \
    .width = 0,                               \
    .height = 0,                              \
    .use_intermediate = true,                 \
    .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM \
}

/* ============================================================================
 * Shader System Lifecycle
 * ============================================================================ */

/**
 * Create shader system.
 * Caller OWNS the returned pointer and MUST call agentite_shader_system_destroy().
 * Returns NULL on failure (check agentite_get_last_error()).
 */
Agentite_ShaderSystem *agentite_shader_system_create(SDL_GPUDevice *gpu);

/**
 * Destroy shader system and all owned shaders.
 * Accepts NULL safely.
 */
void agentite_shader_system_destroy(Agentite_ShaderSystem *ss);

/* ============================================================================
 * Shader Loading
 * ============================================================================ */

/**
 * Load shader from SPIR-V files.
 * Caller OWNS the returned pointer and MUST call agentite_shader_destroy().
 * Returns NULL on failure.
 *
 * @param vert_path Path to vertex shader SPIR-V file
 * @param frag_path Path to fragment shader SPIR-V file
 * @param desc Shader description (NULL for defaults)
 */
Agentite_Shader *agentite_shader_load_spirv(Agentite_ShaderSystem *ss,
                                            const char *vert_path,
                                            const char *frag_path,
                                            const Agentite_ShaderDesc *desc);

/**
 * Load shader from memory (SPIR-V bytecode).
 * Caller OWNS the returned pointer and MUST call agentite_shader_destroy().
 * Returns NULL on failure.
 *
 * @param vert_data Vertex shader SPIR-V bytecode
 * @param vert_size Size of vertex shader bytecode
 * @param frag_data Fragment shader SPIR-V bytecode
 * @param frag_size Size of fragment shader bytecode
 * @param desc Shader description (NULL for defaults)
 */
Agentite_Shader *agentite_shader_load_memory(Agentite_ShaderSystem *ss,
                                             const void *vert_data, size_t vert_size,
                                             const void *frag_data, size_t frag_size,
                                             const Agentite_ShaderDesc *desc);

/**
 * Load shader from MSL source code (Metal only).
 * Caller OWNS the returned pointer and MUST call agentite_shader_destroy().
 * Returns NULL on failure or if not on Metal.
 *
 * @param msl_source Combined MSL source code
 * @param desc Shader description (vertex_entry/fragment_entry required)
 */
Agentite_Shader *agentite_shader_load_msl(Agentite_ShaderSystem *ss,
                                          const char *msl_source,
                                          const Agentite_ShaderDesc *desc);

/**
 * Get a built-in shader effect.
 * Returns borrowed reference (do NOT destroy).
 * Returns NULL if builtin shaders not available.
 */
Agentite_Shader *agentite_shader_get_builtin(Agentite_ShaderSystem *ss,
                                             Agentite_BuiltinShader builtin);

/**
 * Destroy a shader.
 * Accepts NULL safely.
 * Do NOT call on built-in shaders.
 */
void agentite_shader_destroy(Agentite_ShaderSystem *ss, Agentite_Shader *shader);

/* ============================================================================
 * Shader Properties
 * ============================================================================ */

/**
 * Get the underlying SDL_GPU graphics pipeline.
 * Returns borrowed reference (do NOT release).
 */
SDL_GPUGraphicsPipeline *agentite_shader_get_pipeline(const Agentite_Shader *shader);

/**
 * Check if shader is valid and usable.
 */
bool agentite_shader_is_valid(const Agentite_Shader *shader);

/* ============================================================================
 * Uniform Buffer Management
 * ============================================================================ */

/**
 * Create a uniform buffer.
 * Caller OWNS the returned pointer and MUST call agentite_uniform_destroy().
 *
 * @param size Size of uniform data in bytes (should be 16-byte aligned)
 */
Agentite_UniformBuffer *agentite_uniform_create(Agentite_ShaderSystem *ss, size_t size);

/**
 * Destroy uniform buffer.
 * Accepts NULL safely.
 */
void agentite_uniform_destroy(Agentite_ShaderSystem *ss, Agentite_UniformBuffer *ub);

/**
 * Update uniform buffer contents.
 *
 * @param ub Uniform buffer to update
 * @param data Pointer to data to copy
 * @param size Size of data in bytes
 * @param offset Byte offset into buffer
 */
bool agentite_uniform_update(Agentite_UniformBuffer *ub,
                             const void *data, size_t size, size_t offset);

/**
 * Push uniform data directly to shader (per-draw call).
 * Alternative to using uniform buffers.
 *
 * @param cmd Command buffer
 * @param stage Shader stage (vertex or fragment)
 * @param slot Uniform buffer slot
 * @param data Uniform data
 * @param size Size of data in bytes
 */
void agentite_shader_push_uniform(SDL_GPUCommandBuffer *cmd,
                                  Agentite_ShaderStage stage,
                                  uint32_t slot,
                                  const void *data, size_t size);

/* ============================================================================
 * Post-Processing Pipeline
 * ============================================================================ */

/**
 * Create post-processing pipeline.
 * Caller OWNS the returned pointer and MUST call agentite_postprocess_destroy().
 *
 * @param ss Shader system
 * @param window Window for sizing (can be NULL if config specifies size)
 * @param config Configuration (NULL for defaults)
 */
Agentite_PostProcess *agentite_postprocess_create(Agentite_ShaderSystem *ss,
                                                  SDL_Window *window,
                                                  const Agentite_PostProcessConfig *config);

/**
 * Destroy post-processing pipeline.
 * Accepts NULL safely.
 */
void agentite_postprocess_destroy(Agentite_PostProcess *pp);

/**
 * Resize post-processing buffers.
 * Call when window resizes.
 */
bool agentite_postprocess_resize(Agentite_PostProcess *pp, int width, int height);

/**
 * Get the render target texture for scene rendering.
 * Render your scene to this texture, then call agentite_postprocess_apply().
 */
SDL_GPUTexture *agentite_postprocess_get_target(Agentite_PostProcess *pp);

/**
 * Begin post-processing.
 * This prepares the source texture for processing.
 *
 * @param pp Post-process pipeline
 * @param cmd Command buffer
 * @param source Source texture to process (NULL = use internal target)
 */
void agentite_postprocess_begin(Agentite_PostProcess *pp,
                                SDL_GPUCommandBuffer *cmd,
                                SDL_GPUTexture *source);

/**
 * Apply a shader effect.
 * Can be called multiple times to chain effects.
 *
 * @param pp Post-process pipeline
 * @param cmd Command buffer
 * @param pass Render pass
 * @param shader Shader to apply
 * @param params Optional parameters (shader-specific, can be NULL)
 */
void agentite_postprocess_apply(Agentite_PostProcess *pp,
                                SDL_GPUCommandBuffer *cmd,
                                SDL_GPURenderPass *pass,
                                Agentite_Shader *shader,
                                const void *params);

/**
 * Apply post-processing effect with explicit output dimensions.
 * Use this on HiDPI displays to ensure correct fullscreen quad rendering.
 *
 * @param pp Post-process pipeline
 * @param cmd Command buffer
 * @param pass Render pass
 * @param shader Shader to apply
 * @param params Optional parameters (shader-specific, can be NULL)
 * @param output_width Width of the output render target (e.g., physical/drawable width)
 * @param output_height Height of the output render target (e.g., physical/drawable height)
 */
void agentite_postprocess_apply_scaled(Agentite_PostProcess *pp,
                                        SDL_GPUCommandBuffer *cmd,
                                        SDL_GPURenderPass *pass,
                                        Agentite_Shader *shader,
                                        const void *params,
                                        int output_width,
                                        int output_height);

/**
 * End post-processing and output final result.
 * Call after all apply() calls.
 */
void agentite_postprocess_end(Agentite_PostProcess *pp,
                              SDL_GPUCommandBuffer *cmd,
                              SDL_GPURenderPass *pass);

/**
 * Simple single-pass post-process application.
 * Combines begin + apply + end for single-effect scenarios.
 *
 * @param pp Post-process pipeline
 * @param cmd Command buffer
 * @param pass Render pass
 * @param source Source texture
 * @param shader Shader to apply
 * @param params Optional parameters
 */
void agentite_postprocess_simple(Agentite_PostProcess *pp,
                                 SDL_GPUCommandBuffer *cmd,
                                 SDL_GPURenderPass *pass,
                                 SDL_GPUTexture *source,
                                 Agentite_Shader *shader,
                                 const void *params);

/* ============================================================================
 * Fullscreen Quad Helper
 * ============================================================================ */

/**
 * Render a fullscreen quad with the given shader.
 * Useful for post-processing or fullscreen effects.
 *
 * @param ss Shader system
 * @param cmd Command buffer
 * @param pass Render pass
 * @param shader Shader to use
 * @param texture Texture to sample (bound to slot 0)
 * @param params Uniform parameters (pushed to fragment slot 0)
 * @param params_size Size of params in bytes
 */
void agentite_shader_draw_fullscreen(Agentite_ShaderSystem *ss,
                                     SDL_GPUCommandBuffer *cmd,
                                     SDL_GPURenderPass *pass,
                                     Agentite_Shader *shader,
                                     SDL_GPUTexture *texture,
                                     const void *params,
                                     size_t params_size);

/**
 * Draw a fullscreen quad with a shader using two textures.
 * Renders to the current render pass with both textures bound.
 * Used for transition effects that blend between two scenes.
 *
 * @param ss Shader system
 * @param cmd Command buffer
 * @param pass Render pass
 * @param shader Shader to use (must have num_fragment_samplers = 2)
 * @param texture1 First texture (source, bound to slot 0)
 * @param texture2 Second texture (dest, bound to slot 1)
 * @param params Uniform parameters (pushed to fragment slot 0)
 * @param params_size Size of params in bytes
 */
void agentite_shader_draw_fullscreen_two_texture(Agentite_ShaderSystem *ss,
                                                  SDL_GPUCommandBuffer *cmd,
                                                  SDL_GPURenderPass *pass,
                                                  Agentite_Shader *shader,
                                                  SDL_GPUTexture *texture1,
                                                  SDL_GPUTexture *texture2,
                                                  const void *params,
                                                  size_t params_size);

/**
 * Get the fullscreen quad vertex buffer.
 * Useful for custom rendering that needs the standard fullscreen quad.
 *
 * @param ss Shader system
 * @return Quad vertex buffer (borrowed reference, do not destroy)
 */
SDL_GPUBuffer *agentite_shader_get_quad_buffer(Agentite_ShaderSystem *ss);

/**
 * Get the linear sampler.
 * Useful for custom rendering that needs the standard linear sampler.
 *
 * @param ss Shader system
 * @return Linear sampler (borrowed reference, do not destroy)
 */
SDL_GPUSampler *agentite_shader_get_linear_sampler(Agentite_ShaderSystem *ss);

/* ============================================================================
 * Built-in Effect Parameters
 * ============================================================================ */

/* Parameters for brightness/contrast/saturation adjustments */
typedef struct Agentite_ShaderParams_Adjust {
    float amount;       /* Adjustment amount (-1 to 1 for most, 0 = neutral) */
    float _pad[3];
} Agentite_ShaderParams_Adjust;

/* Parameters for blur effects */
typedef struct Agentite_ShaderParams_Blur {
    float radius;       /* Blur radius in pixels */
    float sigma;        /* Gaussian sigma (0 = auto from radius) */
    float _pad[2];
} Agentite_ShaderParams_Blur;

/* Parameters for vignette effect */
typedef struct Agentite_ShaderParams_Vignette {
    float intensity;    /* Edge darkening (0-1) */
    float softness;     /* Falloff softness (0-1) */
    float _pad[2];
} Agentite_ShaderParams_Vignette;

/* Parameters for chromatic aberration */
typedef struct Agentite_ShaderParams_Chromatic {
    float offset;       /* Color channel offset in pixels */
    float _pad[3];
} Agentite_ShaderParams_Chromatic;

/* Parameters for scanlines effect */
typedef struct Agentite_ShaderParams_Scanlines {
    float intensity;    /* Line visibility (0-1) */
    float count;        /* Lines per screen height */
    float _pad[2];
} Agentite_ShaderParams_Scanlines;

/* Parameters for pixelate effect */
typedef struct Agentite_ShaderParams_Pixelate {
    float pixel_size;   /* Size of each "pixel" */
    float _pad[3];
} Agentite_ShaderParams_Pixelate;

/* Parameters for outline effect */
typedef struct Agentite_ShaderParams_Outline {
    float thickness;    /* Outline thickness in pixels */
    float color[4];     /* Outline RGBA color */
    float _pad[3];
} Agentite_ShaderParams_Outline;

/* Parameters for glow/bloom effect */
typedef struct Agentite_ShaderParams_Glow {
    float threshold;    /* Brightness threshold (0-1) */
    float intensity;    /* Glow intensity */
    float _pad[2];
} Agentite_ShaderParams_Glow;

/* Parameters for flash effect */
typedef struct Agentite_ShaderParams_Flash {
    float color[4];     /* Flash RGBA color */
    float intensity;    /* Flash intensity (0-1) */
    float _pad[3];
} Agentite_ShaderParams_Flash;

/* Parameters for dissolve transition */
typedef struct Agentite_ShaderParams_Dissolve {
    float progress;     /* Dissolve progress (0-1) */
    float edge_width;   /* Width of dissolve edge */
    float _pad[2];
} Agentite_ShaderParams_Dissolve;

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Get supported shader formats for current GPU.
 * Returns bitmask of SDL_GPUShaderFormat values.
 */
SDL_GPUShaderFormat agentite_shader_get_formats(Agentite_ShaderSystem *ss);

/**
 * Check if a specific shader format is supported.
 */
bool agentite_shader_format_supported(Agentite_ShaderSystem *ss,
                                      SDL_GPUShaderFormat format);

/**
 * Get shader system statistics.
 */
typedef struct Agentite_ShaderStats {
    uint32_t shaders_loaded;
    uint32_t pipelines_created;
    uint32_t uniform_buffers;
    size_t uniform_memory;
} Agentite_ShaderStats;

void agentite_shader_get_stats(Agentite_ShaderSystem *ss, Agentite_ShaderStats *stats);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SHADER_H */
