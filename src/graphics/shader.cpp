/*
 * Agentite Shader System Implementation
 *
 * Provides shader loading, management, and post-processing pipeline.
 */

#include "agentite/shader.h"
#include "agentite/error.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>
#include <cstdio>

/* SPIRV bytecode for built-in post-processing shaders */
#include "postprocess_shaders_spirv.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_SHADERS 256
#define MAX_UNIFORM_BUFFERS 64
#define FULLSCREEN_QUAD_VERTICES 6

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Agentite_Shader {
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUShader *vertex_shader;
    SDL_GPUShader *fragment_shader;
    Agentite_ShaderDesc desc;
    bool is_builtin;
    bool is_valid;
};

struct Agentite_UniformBuffer {
    void *data;
    size_t size;
    bool dirty;
};

struct Agentite_ShaderSystem {
    SDL_GPUDevice *gpu;
    SDL_GPUShaderFormat formats;

    /* Loaded shaders */
    Agentite_Shader *shaders[MAX_SHADERS];
    uint32_t shader_count;

    /* Built-in shaders (lazy loaded) */
    Agentite_Shader *builtins[AGENTITE_SHADER_BUILTIN_COUNT];
    bool builtins_initialized;

    /* Fullscreen quad resources */
    SDL_GPUBuffer *quad_vertex_buffer;
    SDL_GPUSampler *linear_sampler;
    SDL_GPUSampler *nearest_sampler;

    /* Uniform buffers */
    Agentite_UniformBuffer *uniform_buffers[MAX_UNIFORM_BUFFERS];
    uint32_t uniform_buffer_count;

    /* Stats */
    Agentite_ShaderStats stats;
};

struct Agentite_PostProcess {
    Agentite_ShaderSystem *shader_system;
    SDL_GPUDevice *gpu;

    /* Render targets */
    SDL_GPUTexture *target_a;
    SDL_GPUTexture *target_b;
    int width;
    int height;

    /* State */
    SDL_GPUTexture *current_source;
    SDL_GPUTexture *current_dest;
    bool ping_pong;
};

/* Fullscreen quad vertex */
typedef struct {
    float pos[2];
    float uv[2];
} QuadVertex;

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

static bool init_fullscreen_quad(Agentite_ShaderSystem *ss);
static void cleanup_fullscreen_quad(Agentite_ShaderSystem *ss);
static SDL_GPUShader *create_shader_from_spirv(SDL_GPUDevice *gpu,
                                                const void *code, size_t size,
                                                SDL_GPUShaderStage stage,
                                                const Agentite_ShaderDesc *desc);
static SDL_GPUShader *create_shader_from_msl(SDL_GPUDevice *gpu,
                                              const char *code,
                                              SDL_GPUShaderStage stage,
                                              const char *entry,
                                              const Agentite_ShaderDesc *desc);
static SDL_GPUGraphicsPipeline *create_pipeline(SDL_GPUDevice *gpu,
                                                 SDL_GPUShader *vs,
                                                 SDL_GPUShader *fs,
                                                 const Agentite_ShaderDesc *desc);
static SDL_GPUVertexElementFormat convert_vertex_format(Agentite_VertexFormat fmt);
static void setup_blend_state(SDL_GPUColorTargetBlendState *blend, Agentite_BlendMode mode);
static void *load_file(const char *path, size_t *out_size);
static bool init_builtin_shaders(Agentite_ShaderSystem *ss);

/* ============================================================================
 * Shader System Lifecycle
 * ============================================================================ */

Agentite_ShaderSystem *agentite_shader_system_create(SDL_GPUDevice *gpu)
{
    if (!gpu) {
        agentite_set_error("Shader: GPU device is NULL");
        return NULL;
    }

    Agentite_ShaderSystem *ss = (Agentite_ShaderSystem *)calloc(1, sizeof(*ss));
    if (!ss) {
        agentite_set_error("Shader: Failed to allocate shader system");
        return NULL;
    }

    ss->gpu = gpu;
    ss->formats = SDL_GetGPUShaderFormats(gpu);

    /* Create samplers */
    SDL_GPUSamplerCreateInfo sampler_info = {};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    ss->linear_sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
    if (!ss->linear_sampler) {
        agentite_set_error_from_sdl("Shader: Failed to create linear sampler");
        free(ss);
        return NULL;
    }

    sampler_info.min_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mag_filter = SDL_GPU_FILTER_NEAREST;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    ss->nearest_sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
    if (!ss->nearest_sampler) {
        agentite_set_error_from_sdl("Shader: Failed to create nearest sampler");
        SDL_ReleaseGPUSampler(gpu, ss->linear_sampler);
        free(ss);
        return NULL;
    }

    /* Initialize fullscreen quad */
    if (!init_fullscreen_quad(ss)) {
        SDL_ReleaseGPUSampler(gpu, ss->linear_sampler);
        SDL_ReleaseGPUSampler(gpu, ss->nearest_sampler);
        free(ss);
        return NULL;
    }

    return ss;
}

void agentite_shader_system_destroy(Agentite_ShaderSystem *ss)
{
    if (!ss) return;

    /* Destroy all loaded shaders (skip builtins - they're handled separately) */
    for (uint32_t i = 0; i < ss->shader_count; i++) {
        if (ss->shaders[i] && !ss->shaders[i]->is_builtin) {
            if (ss->shaders[i]->pipeline) {
                SDL_ReleaseGPUGraphicsPipeline(ss->gpu, ss->shaders[i]->pipeline);
            }
            if (ss->shaders[i]->vertex_shader) {
                SDL_ReleaseGPUShader(ss->gpu, ss->shaders[i]->vertex_shader);
            }
            if (ss->shaders[i]->fragment_shader) {
                SDL_ReleaseGPUShader(ss->gpu, ss->shaders[i]->fragment_shader);
            }
            free(ss->shaders[i]);
        }
    }

    /* Destroy built-in shaders */
    for (int i = 0; i < AGENTITE_SHADER_BUILTIN_COUNT; i++) {
        if (ss->builtins[i]) {
            if (ss->builtins[i]->pipeline) {
                SDL_ReleaseGPUGraphicsPipeline(ss->gpu, ss->builtins[i]->pipeline);
            }
            if (ss->builtins[i]->vertex_shader) {
                SDL_ReleaseGPUShader(ss->gpu, ss->builtins[i]->vertex_shader);
            }
            if (ss->builtins[i]->fragment_shader) {
                SDL_ReleaseGPUShader(ss->gpu, ss->builtins[i]->fragment_shader);
            }
            free(ss->builtins[i]);
        }
    }

    /* Destroy uniform buffers */
    for (uint32_t i = 0; i < ss->uniform_buffer_count; i++) {
        if (ss->uniform_buffers[i]) {
            free(ss->uniform_buffers[i]->data);
            free(ss->uniform_buffers[i]);
        }
    }

    /* Cleanup resources */
    cleanup_fullscreen_quad(ss);

    if (ss->linear_sampler) {
        SDL_ReleaseGPUSampler(ss->gpu, ss->linear_sampler);
    }
    if (ss->nearest_sampler) {
        SDL_ReleaseGPUSampler(ss->gpu, ss->nearest_sampler);
    }

    free(ss);
}

/* ============================================================================
 * Shader Loading
 * ============================================================================ */

Agentite_Shader *agentite_shader_load_spirv(Agentite_ShaderSystem *ss,
                                            const char *vert_path,
                                            const char *frag_path,
                                            const Agentite_ShaderDesc *desc)
{
    if (!ss || !vert_path || !frag_path) {
        agentite_set_error("Shader: Invalid parameters");
        return NULL;
    }

    /* Load vertex shader file */
    size_t vert_size = 0;
    void *vert_data = load_file(vert_path, &vert_size);
    if (!vert_data) {
        agentite_set_error("Shader: Failed to load vertex shader: %s", vert_path);
        return NULL;
    }

    /* Load fragment shader file */
    size_t frag_size = 0;
    void *frag_data = load_file(frag_path, &frag_size);
    if (!frag_data) {
        free(vert_data);
        agentite_set_error("Shader: Failed to load fragment shader: %s", frag_path);
        return NULL;
    }

    Agentite_Shader *shader = agentite_shader_load_memory(ss,
        vert_data, vert_size, frag_data, frag_size, desc);

    free(vert_data);
    free(frag_data);

    return shader;
}

Agentite_Shader *agentite_shader_load_memory(Agentite_ShaderSystem *ss,
                                             const void *vert_data, size_t vert_size,
                                             const void *frag_data, size_t frag_size,
                                             const Agentite_ShaderDesc *desc)
{
    if (!ss || !vert_data || !frag_data) {
        agentite_set_error("Shader: Invalid parameters");
        return NULL;
    }

    if (ss->shader_count >= MAX_SHADERS) {
        agentite_set_error("Shader: Maximum shader count reached (%d)", MAX_SHADERS);
        return NULL;
    }

    /* Check format support */
    if (!(ss->formats & SDL_GPU_SHADERFORMAT_SPIRV)) {
        agentite_set_error("Shader: SPIRV format not supported on this GPU");
        return NULL;
    }

    /* Use defaults if no desc provided */
    Agentite_ShaderDesc default_desc = AGENTITE_SHADER_DESC_DEFAULT;
    if (!desc) {
        desc = &default_desc;
    }

    /* Create shader object */
    Agentite_Shader *shader = (Agentite_Shader *)calloc(1, sizeof(*shader));
    if (!shader) {
        agentite_set_error("Shader: Failed to allocate shader");
        return NULL;
    }

    shader->desc = *desc;

    /* Create vertex shader */
    shader->vertex_shader = create_shader_from_spirv(ss->gpu,
        vert_data, vert_size, SDL_GPU_SHADERSTAGE_VERTEX, desc);
    if (!shader->vertex_shader) {
        free(shader);
        return NULL;
    }

    /* Create fragment shader */
    shader->fragment_shader = create_shader_from_spirv(ss->gpu,
        frag_data, frag_size, SDL_GPU_SHADERSTAGE_FRAGMENT, desc);
    if (!shader->fragment_shader) {
        SDL_ReleaseGPUShader(ss->gpu, shader->vertex_shader);
        free(shader);
        return NULL;
    }

    /* Create pipeline */
    shader->pipeline = create_pipeline(ss->gpu,
        shader->vertex_shader, shader->fragment_shader, desc);
    if (!shader->pipeline) {
        SDL_ReleaseGPUShader(ss->gpu, shader->vertex_shader);
        SDL_ReleaseGPUShader(ss->gpu, shader->fragment_shader);
        free(shader);
        return NULL;
    }

    shader->is_valid = true;
    shader->is_builtin = false;

    /* Track shader */
    ss->shaders[ss->shader_count++] = shader;
    ss->stats.shaders_loaded++;
    ss->stats.pipelines_created++;

    return shader;
}

Agentite_Shader *agentite_shader_load_msl(Agentite_ShaderSystem *ss,
                                          const char *msl_source,
                                          const Agentite_ShaderDesc *desc)
{
    if (!ss || !msl_source) {
        agentite_set_error("Shader: Invalid parameters");
        return NULL;
    }

    if (!(ss->formats & SDL_GPU_SHADERFORMAT_MSL)) {
        agentite_set_error("Shader: MSL format not supported on this GPU");
        return NULL;
    }

    if (!desc || !desc->vertex_entry || !desc->fragment_entry) {
        agentite_set_error("Shader: MSL requires vertex_entry and fragment_entry");
        return NULL;
    }

    if (ss->shader_count >= MAX_SHADERS) {
        agentite_set_error("Shader: Maximum shader count reached (%d)", MAX_SHADERS);
        return NULL;
    }

    /* Create shader object */
    Agentite_Shader *shader = (Agentite_Shader *)calloc(1, sizeof(*shader));
    if (!shader) {
        agentite_set_error("Shader: Failed to allocate shader");
        return NULL;
    }

    shader->desc = *desc;

    /* Create vertex shader */
    shader->vertex_shader = create_shader_from_msl(ss->gpu,
        msl_source, SDL_GPU_SHADERSTAGE_VERTEX, desc->vertex_entry, desc);
    if (!shader->vertex_shader) {
        free(shader);
        return NULL;
    }

    /* Create fragment shader */
    shader->fragment_shader = create_shader_from_msl(ss->gpu,
        msl_source, SDL_GPU_SHADERSTAGE_FRAGMENT, desc->fragment_entry, desc);
    if (!shader->fragment_shader) {
        SDL_ReleaseGPUShader(ss->gpu, shader->vertex_shader);
        free(shader);
        return NULL;
    }

    /* Create pipeline */
    shader->pipeline = create_pipeline(ss->gpu,
        shader->vertex_shader, shader->fragment_shader, desc);
    if (!shader->pipeline) {
        SDL_ReleaseGPUShader(ss->gpu, shader->vertex_shader);
        SDL_ReleaseGPUShader(ss->gpu, shader->fragment_shader);
        free(shader);
        return NULL;
    }

    shader->is_valid = true;
    shader->is_builtin = false;

    /* Track shader */
    ss->shaders[ss->shader_count++] = shader;
    ss->stats.shaders_loaded++;
    ss->stats.pipelines_created++;

    return shader;
}

Agentite_Shader *agentite_shader_get_builtin(Agentite_ShaderSystem *ss,
                                             Agentite_BuiltinShader builtin)
{
    if (!ss) return NULL;
    if (builtin <= AGENTITE_SHADER_NONE || builtin >= AGENTITE_SHADER_BUILTIN_COUNT) {
        return NULL;
    }

    /* Lazy initialize built-ins */
    if (!ss->builtins_initialized) {
        if (!init_builtin_shaders(ss)) {
            return NULL;
        }
        ss->builtins_initialized = true;
    }

    return ss->builtins[builtin];
}

void agentite_shader_destroy(Agentite_ShaderSystem *ss, Agentite_Shader *shader)
{
    if (!ss || !shader || shader->is_builtin) return;

    /* Remove from tracked shaders */
    for (uint32_t i = 0; i < ss->shader_count; i++) {
        if (ss->shaders[i] == shader) {
            ss->shaders[i] = ss->shaders[--ss->shader_count];
            break;
        }
    }

    /* Release GPU resources */
    if (shader->pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(ss->gpu, shader->pipeline);
    }
    if (shader->vertex_shader) {
        SDL_ReleaseGPUShader(ss->gpu, shader->vertex_shader);
    }
    if (shader->fragment_shader) {
        SDL_ReleaseGPUShader(ss->gpu, shader->fragment_shader);
    }

    free(shader);
}

/* ============================================================================
 * Shader Properties
 * ============================================================================ */

SDL_GPUGraphicsPipeline *agentite_shader_get_pipeline(const Agentite_Shader *shader)
{
    return shader ? shader->pipeline : NULL;
}

bool agentite_shader_is_valid(const Agentite_Shader *shader)
{
    return shader && shader->is_valid;
}

/* ============================================================================
 * Uniform Buffer Management
 * ============================================================================ */

Agentite_UniformBuffer *agentite_uniform_create(Agentite_ShaderSystem *ss, size_t size)
{
    if (!ss || size == 0) {
        agentite_set_error("Shader: Invalid uniform buffer parameters");
        return NULL;
    }

    if (ss->uniform_buffer_count >= MAX_UNIFORM_BUFFERS) {
        agentite_set_error("Shader: Maximum uniform buffer count reached");
        return NULL;
    }

    /* Align size to 16 bytes */
    size = (size + 15) & ~15;

    Agentite_UniformBuffer *ub = (Agentite_UniformBuffer *)calloc(1, sizeof(*ub));
    if (!ub) {
        agentite_set_error("Shader: Failed to allocate uniform buffer");
        return NULL;
    }

    ub->data = calloc(1, size);
    if (!ub->data) {
        free(ub);
        agentite_set_error("Shader: Failed to allocate uniform buffer data");
        return NULL;
    }

    ub->size = size;
    ub->dirty = true;

    ss->uniform_buffers[ss->uniform_buffer_count++] = ub;
    ss->stats.uniform_buffers++;
    ss->stats.uniform_memory += size;

    return ub;
}

void agentite_uniform_destroy(Agentite_ShaderSystem *ss, Agentite_UniformBuffer *ub)
{
    if (!ss || !ub) return;

    /* Remove from tracked buffers */
    for (uint32_t i = 0; i < ss->uniform_buffer_count; i++) {
        if (ss->uniform_buffers[i] == ub) {
            ss->stats.uniform_memory -= ub->size;
            ss->stats.uniform_buffers--;
            ss->uniform_buffers[i] = ss->uniform_buffers[--ss->uniform_buffer_count];
            break;
        }
    }

    free(ub->data);
    free(ub);
}

bool agentite_uniform_update(Agentite_UniformBuffer *ub,
                             const void *data, size_t size, size_t offset)
{
    if (!ub || !data) return false;
    if (offset + size > ub->size) {
        agentite_set_error("Shader: Uniform update exceeds buffer size");
        return false;
    }

    memcpy((uint8_t *)ub->data + offset, data, size);
    ub->dirty = true;
    return true;
}

void agentite_shader_push_uniform(SDL_GPUCommandBuffer *cmd,
                                  Agentite_ShaderStage stage,
                                  uint32_t slot,
                                  const void *data, size_t size)
{
    if (!cmd || !data || size == 0) return;

    if (stage == AGENTITE_SHADER_STAGE_VERTEX) {
        SDL_PushGPUVertexUniformData(cmd, slot, data, (Uint32)size);
    } else {
        SDL_PushGPUFragmentUniformData(cmd, slot, data, (Uint32)size);
    }
}

/* ============================================================================
 * Post-Processing Pipeline
 * ============================================================================ */

Agentite_PostProcess *agentite_postprocess_create(Agentite_ShaderSystem *ss,
                                                  SDL_Window *window,
                                                  const Agentite_PostProcessConfig *config)
{
    if (!ss) {
        agentite_set_error("PostProcess: Shader system is NULL");
        return NULL;
    }

    Agentite_PostProcessConfig default_config = AGENTITE_POSTPROCESS_CONFIG_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    int width = config->width;
    int height = config->height;

    if (width == 0 || height == 0) {
        if (window) {
            SDL_GetWindowSize(window, &width, &height);
        } else {
            agentite_set_error("PostProcess: Window required when size not specified");
            return NULL;
        }
    }

    Agentite_PostProcess *pp = (Agentite_PostProcess *)calloc(1, sizeof(*pp));
    if (!pp) {
        agentite_set_error("PostProcess: Failed to allocate");
        return NULL;
    }

    pp->shader_system = ss;
    pp->gpu = ss->gpu;
    pp->width = width;
    pp->height = height;

    /* Create render target A */
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = config->format;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    pp->target_a = SDL_CreateGPUTexture(pp->gpu, &tex_info);
    if (!pp->target_a) {
        agentite_set_error_from_sdl("PostProcess: Failed to create render target A");
        free(pp);
        return NULL;
    }

    /* Create render target B for ping-pong */
    if (config->use_intermediate) {
        pp->target_b = SDL_CreateGPUTexture(pp->gpu, &tex_info);
        if (!pp->target_b) {
            agentite_set_error_from_sdl("PostProcess: Failed to create render target B");
            SDL_ReleaseGPUTexture(pp->gpu, pp->target_a);
            free(pp);
            return NULL;
        }
    }

    return pp;
}

void agentite_postprocess_destroy(Agentite_PostProcess *pp)
{
    if (!pp) return;

    if (pp->target_a) {
        SDL_ReleaseGPUTexture(pp->gpu, pp->target_a);
    }
    if (pp->target_b) {
        SDL_ReleaseGPUTexture(pp->gpu, pp->target_b);
    }

    free(pp);
}

bool agentite_postprocess_resize(Agentite_PostProcess *pp, int width, int height)
{
    if (!pp || width <= 0 || height <= 0) return false;

    if (pp->width == width && pp->height == height) {
        return true;
    }

    /* Release old textures */
    if (pp->target_a) {
        SDL_ReleaseGPUTexture(pp->gpu, pp->target_a);
    }
    if (pp->target_b) {
        SDL_ReleaseGPUTexture(pp->gpu, pp->target_b);
    }

    pp->width = width;
    pp->height = height;

    /* Recreate textures */
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    pp->target_a = SDL_CreateGPUTexture(pp->gpu, &tex_info);
    if (!pp->target_a) {
        agentite_set_error_from_sdl("PostProcess: Failed to resize render target A");
        return false;
    }

    pp->target_b = SDL_CreateGPUTexture(pp->gpu, &tex_info);
    if (!pp->target_b) {
        agentite_set_error_from_sdl("PostProcess: Failed to resize render target B");
        SDL_ReleaseGPUTexture(pp->gpu, pp->target_a);
        pp->target_a = NULL;
        return false;
    }

    return true;
}

SDL_GPUTexture *agentite_postprocess_get_target(Agentite_PostProcess *pp)
{
    return pp ? pp->target_a : NULL;
}

void agentite_postprocess_begin(Agentite_PostProcess *pp,
                                SDL_GPUCommandBuffer *cmd,
                                SDL_GPUTexture *source)
{
    if (!pp || !cmd) return;

    pp->current_source = source ? source : pp->target_a;
    pp->current_dest = pp->target_b;
    pp->ping_pong = false;
}

void agentite_postprocess_apply(Agentite_PostProcess *pp,
                                SDL_GPUCommandBuffer *cmd,
                                SDL_GPURenderPass *pass,
                                Agentite_Shader *shader,
                                const void *params)
{
    if (!pp || !cmd || !pass || !shader) return;

    Agentite_ShaderSystem *ss = pp->shader_system;

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, shader->pipeline);

    /* Bind source texture */
    SDL_GPUTextureSamplerBinding tex_binding = {};
    tex_binding.texture = pp->current_source;
    tex_binding.sampler = ss->linear_sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Push parameters if provided */
    if (params && shader->desc.num_fragment_uniforms > 0) {
        /* Assume 16-byte aligned params, default size */
        SDL_PushGPUFragmentUniformData(cmd, 0, params, 16);
    }

    /* Bind fullscreen quad vertex buffer */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = ss->quad_vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Draw fullscreen quad */
    SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTICES, 1, 0, 0);

    /* Swap ping-pong targets */
    if (pp->target_b) {
        SDL_GPUTexture *temp = pp->current_source;
        pp->current_source = pp->current_dest;
        pp->current_dest = temp;
        pp->ping_pong = !pp->ping_pong;
    }
}

void agentite_postprocess_end(Agentite_PostProcess *pp,
                              SDL_GPUCommandBuffer *cmd,
                              SDL_GPURenderPass *pass)
{
    /* Currently just a placeholder for cleanup */
    (void)pp;
    (void)cmd;
    (void)pass;
}

void agentite_postprocess_simple(Agentite_PostProcess *pp,
                                 SDL_GPUCommandBuffer *cmd,
                                 SDL_GPURenderPass *pass,
                                 SDL_GPUTexture *source,
                                 Agentite_Shader *shader,
                                 const void *params)
{
    agentite_postprocess_begin(pp, cmd, source);
    agentite_postprocess_apply(pp, cmd, pass, shader, params);
    agentite_postprocess_end(pp, cmd, pass);
}

/* ============================================================================
 * Fullscreen Quad Helper
 * ============================================================================ */

void agentite_shader_draw_fullscreen(Agentite_ShaderSystem *ss,
                                     SDL_GPUCommandBuffer *cmd,
                                     SDL_GPURenderPass *pass,
                                     Agentite_Shader *shader,
                                     SDL_GPUTexture *texture,
                                     const void *params,
                                     size_t params_size)
{
    if (!ss || !cmd || !pass || !shader) return;

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, shader->pipeline);

    /* Bind texture if provided */
    if (texture) {
        SDL_GPUTextureSamplerBinding tex_binding = {};
        tex_binding.texture = texture;
        tex_binding.sampler = ss->linear_sampler;
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);
    }

    /* Push params if provided */
    if (params && params_size > 0) {
        SDL_PushGPUFragmentUniformData(cmd, 0, params, (Uint32)params_size);
    }

    /* Bind quad vertex buffer */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = ss->quad_vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Draw */
    SDL_DrawGPUPrimitives(pass, FULLSCREEN_QUAD_VERTICES, 1, 0, 0);
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

SDL_GPUShaderFormat agentite_shader_get_formats(Agentite_ShaderSystem *ss)
{
    return ss ? ss->formats : (SDL_GPUShaderFormat)0;
}

bool agentite_shader_format_supported(Agentite_ShaderSystem *ss,
                                      SDL_GPUShaderFormat format)
{
    return ss && (ss->formats & format);
}

void agentite_shader_get_stats(Agentite_ShaderSystem *ss, Agentite_ShaderStats *stats)
{
    if (!ss || !stats) return;
    *stats = ss->stats;
}

/* ============================================================================
 * Internal Functions
 * ============================================================================ */

static bool init_fullscreen_quad(Agentite_ShaderSystem *ss)
{
    /* Fullscreen quad vertices (two triangles, CCW winding)
     * Positions are in clip space (-1 to 1)
     * UVs are standard (0,0 top-left to 1,1 bottom-right) */
    QuadVertex vertices[FULLSCREEN_QUAD_VERTICES] = {
        /* Triangle 1 */
        {{ -1.0f, -1.0f }, { 0.0f, 1.0f }},  /* Bottom-left */
        {{  1.0f, -1.0f }, { 1.0f, 1.0f }},  /* Bottom-right */
        {{  1.0f,  1.0f }, { 1.0f, 0.0f }},  /* Top-right */
        /* Triangle 2 */
        {{ -1.0f, -1.0f }, { 0.0f, 1.0f }},  /* Bottom-left */
        {{  1.0f,  1.0f }, { 1.0f, 0.0f }},  /* Top-right */
        {{ -1.0f,  1.0f }, { 0.0f, 0.0f }},  /* Top-left */
    };

    /* Create vertex buffer */
    SDL_GPUBufferCreateInfo buf_info = {};
    buf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    buf_info.size = sizeof(vertices);

    ss->quad_vertex_buffer = SDL_CreateGPUBuffer(ss->gpu, &buf_info);
    if (!ss->quad_vertex_buffer) {
        agentite_set_error_from_sdl("Shader: Failed to create quad vertex buffer");
        return false;
    }

    /* Upload vertex data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = sizeof(vertices);

    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ss->gpu, &transfer_info);
    if (!transfer) {
        agentite_set_error_from_sdl("Shader: Failed to create transfer buffer");
        SDL_ReleaseGPUBuffer(ss->gpu, ss->quad_vertex_buffer);
        ss->quad_vertex_buffer = NULL;
        return false;
    }

    void *map = SDL_MapGPUTransferBuffer(ss->gpu, transfer, false);
    if (!map) {
        agentite_set_error_from_sdl("Shader: Failed to map transfer buffer");
        SDL_ReleaseGPUTransferBuffer(ss->gpu, transfer);
        SDL_ReleaseGPUBuffer(ss->gpu, ss->quad_vertex_buffer);
        ss->quad_vertex_buffer = NULL;
        return false;
    }

    memcpy(map, vertices, sizeof(vertices));
    SDL_UnmapGPUTransferBuffer(ss->gpu, transfer);

    /* Copy to GPU buffer */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(ss->gpu);
    if (!cmd) {
        agentite_set_error_from_sdl("Shader: Failed to acquire command buffer");
        SDL_ReleaseGPUTransferBuffer(ss->gpu, transfer);
        SDL_ReleaseGPUBuffer(ss->gpu, ss->quad_vertex_buffer);
        ss->quad_vertex_buffer = NULL;
        return false;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (!copy_pass) {
        agentite_set_error_from_sdl("Shader: Failed to begin copy pass");
        SDL_CancelGPUCommandBuffer(cmd);
        SDL_ReleaseGPUTransferBuffer(ss->gpu, transfer);
        SDL_ReleaseGPUBuffer(ss->gpu, ss->quad_vertex_buffer);
        ss->quad_vertex_buffer = NULL;
        return false;
    }

    SDL_GPUTransferBufferLocation src = {};
    src.transfer_buffer = transfer;
    src.offset = 0;

    SDL_GPUBufferRegion dst = {};
    dst.buffer = ss->quad_vertex_buffer;
    dst.offset = 0;
    dst.size = sizeof(vertices);

    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, false);
    SDL_EndGPUCopyPass(copy_pass);

    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(ss->gpu, transfer);

    return true;
}

static void cleanup_fullscreen_quad(Agentite_ShaderSystem *ss)
{
    if (ss->quad_vertex_buffer) {
        SDL_ReleaseGPUBuffer(ss->gpu, ss->quad_vertex_buffer);
        ss->quad_vertex_buffer = NULL;
    }
}

static SDL_GPUShader *create_shader_from_spirv(SDL_GPUDevice *gpu,
                                                const void *code, size_t size,
                                                SDL_GPUShaderStage stage,
                                                const Agentite_ShaderDesc *desc)
{
    SDL_GPUShaderCreateInfo info = {};
    info.code = (const Uint8 *)code;
    info.code_size = size;
    info.entrypoint = desc->vertex_entry ? desc->vertex_entry : "main";
    if (stage == SDL_GPU_SHADERSTAGE_FRAGMENT && desc->fragment_entry) {
        info.entrypoint = desc->fragment_entry;
    }
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;

    if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
        info.num_uniform_buffers = desc->num_vertex_uniforms;
        info.num_samplers = desc->num_vertex_samplers;
    } else {
        info.num_uniform_buffers = desc->num_fragment_uniforms;
        info.num_samplers = desc->num_fragment_samplers;
    }

    info.num_storage_textures = 0;
    info.num_storage_buffers = 0;

    SDL_GPUShader *shader = SDL_CreateGPUShader(gpu, &info);
    if (!shader) {
        agentite_set_error_from_sdl("Shader: Failed to create SPIRV shader");
    }
    return shader;
}

static SDL_GPUShader *create_shader_from_msl(SDL_GPUDevice *gpu,
                                              const char *code,
                                              SDL_GPUShaderStage stage,
                                              const char *entry,
                                              const Agentite_ShaderDesc *desc)
{
    SDL_GPUShaderCreateInfo info = {};
    info.code = (const Uint8 *)code;
    info.code_size = strlen(code) + 1;
    info.entrypoint = entry;
    info.format = SDL_GPU_SHADERFORMAT_MSL;
    info.stage = stage;

    if (stage == SDL_GPU_SHADERSTAGE_VERTEX) {
        info.num_uniform_buffers = desc->num_vertex_uniforms;
        info.num_samplers = desc->num_vertex_samplers;
    } else {
        info.num_uniform_buffers = desc->num_fragment_uniforms;
        info.num_samplers = desc->num_fragment_samplers;
    }

    info.num_storage_textures = 0;
    info.num_storage_buffers = 0;

    SDL_GPUShader *shader = SDL_CreateGPUShader(gpu, &info);
    if (!shader) {
        agentite_set_error_from_sdl("Shader: Failed to create MSL shader");
    }
    return shader;
}

static SDL_GPUVertexElementFormat convert_vertex_format(Agentite_VertexFormat fmt)
{
    switch (fmt) {
        case AGENTITE_VERTEX_FLOAT:       return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
        case AGENTITE_VERTEX_FLOAT2:      return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        case AGENTITE_VERTEX_FLOAT3:      return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
        case AGENTITE_VERTEX_FLOAT4:      return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
        case AGENTITE_VERTEX_INT:         return SDL_GPU_VERTEXELEMENTFORMAT_INT;
        case AGENTITE_VERTEX_INT2:        return SDL_GPU_VERTEXELEMENTFORMAT_INT2;
        case AGENTITE_VERTEX_INT3:        return SDL_GPU_VERTEXELEMENTFORMAT_INT3;
        case AGENTITE_VERTEX_INT4:        return SDL_GPU_VERTEXELEMENTFORMAT_INT4;
        case AGENTITE_VERTEX_UBYTE4_NORM: return SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM;
        default:                          return SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    }
}

static void setup_blend_state(SDL_GPUColorTargetBlendState *blend, Agentite_BlendMode mode)
{
    memset(blend, 0, sizeof(*blend));

    blend->color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                              SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    switch (mode) {
        case AGENTITE_BLEND_NONE:
            blend->enable_blend = false;
            break;

        case AGENTITE_BLEND_ALPHA:
            blend->enable_blend = true;
            blend->src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            blend->dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            blend->color_blend_op = SDL_GPU_BLENDOP_ADD;
            blend->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            blend->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            break;

        case AGENTITE_BLEND_ADDITIVE:
            blend->enable_blend = true;
            blend->src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
            blend->dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend->color_blend_op = SDL_GPU_BLENDOP_ADD;
            blend->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            break;

        case AGENTITE_BLEND_MULTIPLY:
            blend->enable_blend = true;
            blend->src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
            blend->dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            blend->color_blend_op = SDL_GPU_BLENDOP_ADD;
            blend->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA;
            blend->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
            blend->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            break;

        case AGENTITE_BLEND_PREMULTIPLIED:
            blend->enable_blend = true;
            blend->src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend->dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            blend->color_blend_op = SDL_GPU_BLENDOP_ADD;
            blend->src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
            blend->dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
            blend->alpha_blend_op = SDL_GPU_BLENDOP_ADD;
            break;
    }
}

static SDL_GPUGraphicsPipeline *create_pipeline(SDL_GPUDevice *gpu,
                                                 SDL_GPUShader *vs,
                                                 SDL_GPUShader *fs,
                                                 const Agentite_ShaderDesc *desc)
{
    /* Set up vertex input state */
    SDL_GPUVertexInputState vertex_input = {};

    SDL_GPUVertexAttribute attrs[16] = {};
    SDL_GPUVertexBufferDescription vb_desc = {};

    if (desc->vertex_layout) {
        uint32_t attr_count = desc->vertex_layout->attr_count;
        if (attr_count > 16) attr_count = 16;

        for (uint32_t i = 0; i < attr_count; i++) {
            attrs[i].location = desc->vertex_layout->attrs[i].location;
            attrs[i].buffer_slot = 0;
            attrs[i].format = convert_vertex_format(desc->vertex_layout->attrs[i].format);
            attrs[i].offset = desc->vertex_layout->attrs[i].offset;
        }

        vb_desc.slot = 0;
        vb_desc.pitch = desc->vertex_layout->stride;
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_desc.instance_step_rate = 0;

        vertex_input.vertex_buffer_descriptions = &vb_desc;
        vertex_input.num_vertex_buffers = 1;
        vertex_input.vertex_attributes = attrs;
        vertex_input.num_vertex_attributes = attr_count;
    } else {
        /* Default fullscreen quad layout: position (float2), uv (float2) */
        attrs[0].location = 0;
        attrs[0].buffer_slot = 0;
        attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[0].offset = 0;

        attrs[1].location = 1;
        attrs[1].buffer_slot = 0;
        attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
        attrs[1].offset = sizeof(float) * 2;

        vb_desc.slot = 0;
        vb_desc.pitch = sizeof(float) * 4;
        vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
        vb_desc.instance_step_rate = 0;

        vertex_input.vertex_buffer_descriptions = &vb_desc;
        vertex_input.num_vertex_buffers = 1;
        vertex_input.vertex_attributes = attrs;
        vertex_input.num_vertex_attributes = 2;
    }

    /* Set up blend state */
    SDL_GPUColorTargetBlendState blend_state = {};
    setup_blend_state(&blend_state, desc->blend_mode);

    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = desc->target_format != SDL_GPU_TEXTUREFORMAT_INVALID
                          ? desc->target_format
                          : SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state = blend_state;

    /* Create pipeline */
    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vs;
    pipeline_info.fragment_shader = fs;
    pipeline_info.vertex_input_state = vertex_input;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pipeline_info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pipeline_info.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pipeline_info.rasterizer_state.enable_depth_clip = false;
    pipeline_info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    pipeline_info.multisample_state.sample_mask = 0;
    pipeline_info.depth_stencil_state.enable_depth_test = false;
    pipeline_info.depth_stencil_state.enable_depth_write = false;
    pipeline_info.depth_stencil_state.enable_stencil_test = false;
    pipeline_info.target_info.color_target_descriptions = &color_target;
    pipeline_info.target_info.num_color_targets = 1;
    pipeline_info.target_info.depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID;
    pipeline_info.target_info.has_depth_stencil_target = false;

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(gpu, &pipeline_info);
    if (!pipeline) {
        agentite_set_error_from_sdl("Shader: Failed to create graphics pipeline");
    }
    return pipeline;
}

static void *load_file(const char *path, size_t *out_size)
{
    if (!path || !out_size) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) {
        return NULL;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return NULL;
    }
    fseek(f, 0, SEEK_SET);

    void *data = malloc((size_t)size);
    if (!data) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(data, 1, (size_t)size, f);
    fclose(f);

    if (read != (size_t)size) {
        free(data);
        return NULL;
    }

    *out_size = (size_t)size;
    return data;
}

/* Built-in shader source - simple fullscreen passthrough vertex shader */
static const char *builtin_vertex_msl = R"(
#include <metal_stdlib>
using namespace metal;

struct VertexIn {
    float2 position [[attribute(0)]];
    float2 texcoord [[attribute(1)]];
};

struct VertexOut {
    float4 position [[position]];
    float2 texcoord;
};

vertex VertexOut fullscreen_vertex(VertexIn in [[stage_in]]) {
    VertexOut out;
    out.position = float4(in.position, 0.0, 1.0);
    out.texcoord = in.texcoord;
    return out;
}
)";

/* Built-in shader sources - fragment shaders (no VertexOut - defined in vertex shader) */
static const char *builtin_grayscale_msl = R"(
fragment float4 grayscale_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler samp [[sampler(0)]])
{
    float4 color = tex.sample(samp, in.texcoord);
    float gray = dot(color.rgb, float3(0.299, 0.587, 0.114));
    return float4(gray, gray, gray, color.a);
}
)";

static const char *builtin_sepia_msl = R"(
fragment float4 sepia_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler samp [[sampler(0)]])
{
    float4 color = tex.sample(samp, in.texcoord);
    float3 sepia;
    sepia.r = dot(color.rgb, float3(0.393, 0.769, 0.189));
    sepia.g = dot(color.rgb, float3(0.349, 0.686, 0.168));
    sepia.b = dot(color.rgb, float3(0.272, 0.534, 0.131));
    return float4(sepia, color.a);
}
)";

static const char *builtin_invert_msl = R"(
fragment float4 invert_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler samp [[sampler(0)]])
{
    float4 color = tex.sample(samp, in.texcoord);
    return float4(1.0 - color.rgb, color.a);
}
)";

static const char *builtin_vignette_msl = R"(
struct Params {
    float intensity;
    float softness;
    float2 _pad;
};

fragment float4 vignette_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler samp [[sampler(0)]],
    constant Params& params [[buffer(0)]])
{
    float4 color = tex.sample(samp, in.texcoord);
    float2 uv = in.texcoord - 0.5;
    float dist = length(uv * 2.0);
    float start = 1.0 - params.softness;
    float vignette = 1.0 - smoothstep(start, 1.4, dist);
    vignette = mix(1.0 - params.intensity, 1.0, vignette);
    return float4(color.rgb * vignette, color.a);
}
)";

static const char *builtin_pixelate_msl = R"(
struct Params {
    float pixel_size;
    float3 _pad;
};

fragment float4 pixelate_fragment(
    VertexOut in [[stage_in]],
    texture2d<float> tex [[texture(0)]],
    sampler samp [[sampler(0)]],
    constant Params& params [[buffer(0)]])
{
    float2 tex_size = float2(tex.get_width(), tex.get_height());
    float2 uv = floor(in.texcoord * tex_size / params.pixel_size) * params.pixel_size / tex_size;
    return tex.sample(samp, uv);
}
)";

/* Helper to create a builtin shader from SPIRV bytecode */
static Agentite_Shader *create_builtin_spirv(Agentite_ShaderSystem *ss,
                                              const uint8_t *frag_spv, size_t frag_size,
                                              bool needs_uniforms)
{
    Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
    desc.num_fragment_samplers = 1;
    desc.num_fragment_uniforms = needs_uniforms ? 1 : 0;
    desc.blend_mode = AGENTITE_BLEND_NONE;

    Agentite_Shader *shader = agentite_shader_load_memory(ss,
        fullscreen_vert_spv, fullscreen_vert_spv_len,
        frag_spv, frag_size, &desc);

    if (shader) {
        shader->is_builtin = true;
    }
    return shader;
}

static bool init_builtin_shaders(Agentite_ShaderSystem *ss)
{
    /* Try SPIRV first (works on Vulkan, D3D12) */
    if (ss->formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        ss->builtins[AGENTITE_SHADER_GRAYSCALE] = create_builtin_spirv(ss,
            grayscale_frag_spv, grayscale_frag_spv_len, false);

        ss->builtins[AGENTITE_SHADER_SEPIA] = create_builtin_spirv(ss,
            sepia_frag_spv, sepia_frag_spv_len, false);

        ss->builtins[AGENTITE_SHADER_INVERT] = create_builtin_spirv(ss,
            invert_frag_spv, invert_frag_spv_len, false);

        ss->builtins[AGENTITE_SHADER_BRIGHTNESS] = create_builtin_spirv(ss,
            brightness_frag_spv, brightness_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_CONTRAST] = create_builtin_spirv(ss,
            contrast_frag_spv, contrast_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_SATURATION] = create_builtin_spirv(ss,
            saturation_frag_spv, saturation_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_BLUR_BOX] = create_builtin_spirv(ss,
            blur_box_frag_spv, blur_box_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_VIGNETTE] = create_builtin_spirv(ss,
            vignette_pp_frag_spv, vignette_pp_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_CHROMATIC] = create_builtin_spirv(ss,
            chromatic_frag_spv, chromatic_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_SCANLINES] = create_builtin_spirv(ss,
            scanlines_frag_spv, scanlines_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_PIXELATE] = create_builtin_spirv(ss,
            pixelate_frag_spv, pixelate_frag_spv_len, true);

        ss->builtins[AGENTITE_SHADER_SOBEL] = create_builtin_spirv(ss,
            sobel_frag_spv, sobel_frag_spv_len, false);

        ss->builtins[AGENTITE_SHADER_FLASH] = create_builtin_spirv(ss,
            flash_frag_spv, flash_frag_spv_len, true);

        return true;
    }

    /* Fall back to MSL for Metal */
    if (ss->formats & SDL_GPU_SHADERFORMAT_MSL) {
        char combined[8192];

        Agentite_ShaderDesc desc = AGENTITE_SHADER_DESC_DEFAULT;
        desc.num_fragment_samplers = 1;
        desc.blend_mode = AGENTITE_BLEND_NONE;

        /* Grayscale */
        snprintf(combined, sizeof(combined), "%s\n%s", builtin_vertex_msl, builtin_grayscale_msl);
        desc.vertex_entry = "fullscreen_vertex";
        desc.fragment_entry = "grayscale_fragment";
        ss->builtins[AGENTITE_SHADER_GRAYSCALE] = agentite_shader_load_msl(ss, combined, &desc);
        if (ss->builtins[AGENTITE_SHADER_GRAYSCALE]) {
            ss->builtins[AGENTITE_SHADER_GRAYSCALE]->is_builtin = true;
        }

        /* Sepia */
        snprintf(combined, sizeof(combined), "%s\n%s", builtin_vertex_msl, builtin_sepia_msl);
        desc.fragment_entry = "sepia_fragment";
        ss->builtins[AGENTITE_SHADER_SEPIA] = agentite_shader_load_msl(ss, combined, &desc);
        if (ss->builtins[AGENTITE_SHADER_SEPIA]) {
            ss->builtins[AGENTITE_SHADER_SEPIA]->is_builtin = true;
        }

        /* Invert */
        snprintf(combined, sizeof(combined), "%s\n%s", builtin_vertex_msl, builtin_invert_msl);
        desc.fragment_entry = "invert_fragment";
        ss->builtins[AGENTITE_SHADER_INVERT] = agentite_shader_load_msl(ss, combined, &desc);
        if (ss->builtins[AGENTITE_SHADER_INVERT]) {
            ss->builtins[AGENTITE_SHADER_INVERT]->is_builtin = true;
        }

        /* Vignette (needs uniform buffer) */
        snprintf(combined, sizeof(combined), "%s\n%s", builtin_vertex_msl, builtin_vignette_msl);
        desc.fragment_entry = "vignette_fragment";
        desc.num_fragment_uniforms = 1;
        ss->builtins[AGENTITE_SHADER_VIGNETTE] = agentite_shader_load_msl(ss, combined, &desc);
        if (ss->builtins[AGENTITE_SHADER_VIGNETTE]) {
            ss->builtins[AGENTITE_SHADER_VIGNETTE]->is_builtin = true;
        }

        /* Pixelate (needs uniform buffer) */
        snprintf(combined, sizeof(combined), "%s\n%s", builtin_vertex_msl, builtin_pixelate_msl);
        desc.fragment_entry = "pixelate_fragment";
        ss->builtins[AGENTITE_SHADER_PIXELATE] = agentite_shader_load_msl(ss, combined, &desc);
        if (ss->builtins[AGENTITE_SHADER_PIXELATE]) {
            ss->builtins[AGENTITE_SHADER_PIXELATE]->is_builtin = true;
        }

        return true;
    }

    SDL_Log("Shader: No supported shader format for built-in shaders");
    return true;  /* Not an error, just not available */
}
