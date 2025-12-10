#include "agentite/agentite.h"
/*
 * Carbon Sprite/Texture System Implementation
 */

#include "agentite/sprite.h"
#include "agentite/camera.h"
#include "agentite/error.h"
#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* STB image for texture loading */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_ONLY_BMP
#define STBI_ONLY_TGA
#include "stb_image.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define SPRITE_MAX_BATCH 4096           /* Max sprites per batch */
#define SPRITE_VERTS_PER_SPRITE 4
#define SPRITE_INDICES_PER_SPRITE 6
#define SPRITE_VERTEX_CAPACITY (SPRITE_MAX_BATCH * SPRITE_VERTS_PER_SPRITE)
#define SPRITE_INDEX_CAPACITY (SPRITE_MAX_BATCH * SPRITE_INDICES_PER_SPRITE)
#define SPRITE_MAX_SUB_BATCHES 64       /* Max texture switches per batch */

/* Sub-batch for tracking texture switches within a single batch */
typedef struct SpriteBatchSegment {
    Agentite_Texture *texture;
    uint32_t start_index;   /* Starting index in index buffer */
    uint32_t index_count;   /* Number of indices in this segment */
} SpriteBatchSegment;

/* ============================================================================
 * Embedded MSL Shader Source
 * ============================================================================ */

static const char sprite_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float4x4 view_projection;\n"
"    float2 screen_size;\n"
"    float2 padding;\n"
"};\n"
"\n"
"struct VertexIn {\n"
"    float2 position [[attribute(0)]];\n"
"    float2 texcoord [[attribute(1)]];\n"
"    float4 color [[attribute(2)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texcoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"vertex VertexOut sprite_vertex(\n"
"    VertexIn in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOut out;\n"
"    float4 world_pos = float4(in.position, 0.0, 1.0);\n"
"    out.position = uniforms.view_projection * world_pos;\n"
"    out.texcoord = in.texcoord;\n"
"    out.color = in.color;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 sprite_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> sprite_texture [[texture(0)]],\n"
"    sampler sprite_sampler [[sampler(0)]]\n"
") {\n"
"    float4 tex_color = sprite_texture.sample(sprite_sampler, in.texcoord);\n"
"    return tex_color * in.color;\n"
"}\n";

/* Vignette post-process shader */
static const char vignette_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float4x4 view_projection;\n"
"    float2 screen_size;\n"
"    float2 padding;\n"
"};\n"
"\n"
"struct VertexIn {\n"
"    float2 position [[attribute(0)]];\n"
"    float2 texcoord [[attribute(1)]];\n"
"    float4 color [[attribute(2)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texcoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"vertex VertexOut vignette_vertex(\n"
"    VertexIn in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOut out;\n"
"    float4 world_pos = float4(in.position, 0.0, 1.0);\n"
"    out.position = uniforms.view_projection * world_pos;\n"
"    out.texcoord = in.texcoord;\n"
"    out.color = in.color;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 vignette_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> scene_texture [[texture(0)]],\n"
"    sampler scene_sampler [[sampler(0)]]\n"
") {\n"
"    float4 scene_color = scene_texture.sample(scene_sampler, in.texcoord);\n"
"    \n"
"    /* Calculate vignette based on distance from center */\n"
"    float2 uv = in.texcoord - float2(0.5, 0.5);\n"
"    float dist = length(uv * float2(2.0, 2.0));\n"
"    \n"
"    /* Smooth vignette falloff: start darkening at 0.6, full effect at 1.4 */\n"
"    float vignette = 1.0 - smoothstep(0.6, 1.4, dist);\n"
"    \n"
"    /* Mix with max darkness of 0.4 */\n"
"    vignette = mix(0.6, 1.0, vignette);\n"
"    \n"
"    return float4(scene_color.rgb * vignette, scene_color.a);\n"
"}\n";

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Agentite_Texture {
    SDL_GPUTexture *gpu_texture;
    int width;
    int height;
};

struct Agentite_SpriteRenderer {
    SDL_GPUDevice *gpu;
    SDL_Window *window;
    int screen_width;
    int screen_height;

    /* GPU resources */
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUGraphicsPipeline *vignette_pipeline;  /* Post-process vignette */
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUSampler *sampler;
    SDL_GPUSampler *linear_sampler;  /* For post-process texture sampling */

    /* CPU-side batch buffers */
    Agentite_SpriteVertex *vertices;
    uint16_t *indices;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t sprite_count;

    /* Current batch state */
    Agentite_Texture *current_texture;
    bool batch_started;
    SDL_GPUCommandBuffer *current_cmd;  /* Command buffer for auto-flush */

    /* Sub-batch tracking for texture switches */
    SpriteBatchSegment segments[SPRITE_MAX_SUB_BATCHES];
    uint32_t segment_count;
    uint32_t current_segment_start;  /* Starting index for current segment */

    /* Camera (optional - NULL = screen-space mode) */
    Agentite_Camera *camera;
};

/* ============================================================================
 * Internal: Pipeline Creation
 * ============================================================================ */

static bool sprite_create_pipeline(Agentite_SpriteRenderer *sr)
{
    if (!sr || !sr->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(sr->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Create vertex shader */
        SDL_GPUShaderCreateInfo vs_info = {
            .code = (const Uint8 *)sprite_shader_msl,
            .code_size = sizeof(sprite_shader_msl),
            .entrypoint = "sprite_vertex",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers = 0,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 1,
        };
        vertex_shader = SDL_CreateGPUShader(sr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("Sprite: Failed to create vertex shader");
            return false;
        }

        /* Create fragment shader */
        SDL_GPUShaderCreateInfo fs_info = {
            .code = (const Uint8 *)sprite_shader_msl,
            .code_size = sizeof(sprite_shader_msl),
            .entrypoint = "sprite_fragment",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = 1,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 0,
        };
        fragment_shader = SDL_CreateGPUShader(sr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("Sprite: Failed to create fragment shader");
            SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
            return false;
        }
    } else {
        agentite_set_error("Sprite: No supported shader format (need MSL for Metal)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[] = {
        { /* position */
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(Agentite_SpriteVertex, pos)
        },
        { /* texcoord */
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(Agentite_SpriteVertex, uv)
        },
        { /* color */
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
            .offset = offsetof(Agentite_SpriteVertex, color)
        }
    };

    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(Agentite_SpriteVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    SDL_GPUVertexInputState vertex_input = {
        .vertex_buffer_descriptions = &vb_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = attributes,
        .num_vertex_attributes = 3
    };

    /* Alpha blending */
    SDL_GPUColorTargetBlendState blend_state = {
        .enable_blend = true,
        .src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA,
        .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .color_blend_op = SDL_GPU_BLENDOP_ADD,
        .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
        .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA,
        .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
        .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                           SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A
    };

    SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
        .blend_state = blend_state
    };

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = vertex_input,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .enable_depth_clip = false
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .sample_mask = 0
        },
        .depth_stencil_state = {
            .enable_depth_test = false,
            .enable_depth_write = false,
            .enable_stencil_test = false
        },
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
            .has_depth_stencil_target = false
        }
    };

    sr->pipeline = SDL_CreateGPUGraphicsPipeline(sr->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
    SDL_ReleaseGPUShader(sr->gpu, fragment_shader);

    if (!sr->pipeline) {
        agentite_set_error_from_sdl("Sprite: Failed to create graphics pipeline");
        return false;
    }

    SDL_Log("Sprite: Graphics pipeline created successfully");
    return true;
}

/* Create vignette post-process pipeline */
static bool sprite_create_vignette_pipeline(Agentite_SpriteRenderer *sr)
{
    if (!sr || !sr->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(sr->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_GPUShaderCreateInfo vs_info = {
            .code = (const Uint8 *)vignette_shader_msl,
            .code_size = sizeof(vignette_shader_msl),
            .entrypoint = "vignette_vertex",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers = 0,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 1,
        };
        vertex_shader = SDL_CreateGPUShader(sr->gpu, &vs_info);
        if (!vertex_shader) {
            SDL_Log("Vignette: Failed to create vertex shader: %s", SDL_GetError());
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {
            .code = (const Uint8 *)vignette_shader_msl,
            .code_size = sizeof(vignette_shader_msl),
            .entrypoint = "vignette_fragment",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = 1,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 0,
        };
        fragment_shader = SDL_CreateGPUShader(sr->gpu, &fs_info);
        if (!fragment_shader) {
            SDL_Log("Vignette: Failed to create fragment shader: %s", SDL_GetError());
            SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
            return false;
        }
    } else {
        return false;
    }

    SDL_GPUVertexAttribute attributes[] = {
        { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(Agentite_SpriteVertex, pos) },
        { .location = 1, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(Agentite_SpriteVertex, uv) },
        { .location = 2, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .offset = offsetof(Agentite_SpriteVertex, color) }
    };

    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(Agentite_SpriteVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    SDL_GPUVertexInputState vertex_input = {
        .vertex_buffer_descriptions = &vb_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = attributes,
        .num_vertex_attributes = 3
    };

    SDL_GPUColorTargetBlendState blend_state = {
        .enable_blend = false,
        .color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                           SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A
    };

    SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,
        .blend_state = blend_state
    };

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vertex_shader,
        .fragment_shader = fragment_shader,
        .vertex_input_state = vertex_input,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_NONE,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .enable_depth_clip = false
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .sample_mask = 0
        },
        .depth_stencil_state = {
            .enable_depth_test = false,
            .enable_depth_write = false,
            .enable_stencil_test = false
        },
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_INVALID,
            .has_depth_stencil_target = false
        }
    };

    sr->vignette_pipeline = SDL_CreateGPUGraphicsPipeline(sr->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
    SDL_ReleaseGPUShader(sr->gpu, fragment_shader);

    if (!sr->vignette_pipeline) {
        SDL_Log("Vignette: Failed to create graphics pipeline: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Vignette: Graphics pipeline created successfully");
    return true;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

Agentite_SpriteRenderer *agentite_sprite_init(SDL_GPUDevice *gpu, SDL_Window *window)
{
    if (!gpu || !window) return NULL;

    Agentite_SpriteRenderer *sr = (Agentite_SpriteRenderer*)calloc(1, sizeof(Agentite_SpriteRenderer));
    if (!sr) {
        agentite_set_error("Sprite: Failed to allocate renderer");
        return NULL;
    }

    sr->gpu = gpu;
    sr->window = window;

    /* Get window size */
    SDL_GetWindowSize(window, &sr->screen_width, &sr->screen_height);

    /* Allocate CPU-side buffers */
    sr->vertices = (Agentite_SpriteVertex*)malloc(SPRITE_VERTEX_CAPACITY * sizeof(Agentite_SpriteVertex));
    sr->indices = (uint16_t*)malloc(SPRITE_INDEX_CAPACITY * sizeof(uint16_t));
    if (!sr->vertices || !sr->indices) {
        agentite_set_error("Sprite: Failed to allocate batch buffers");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Pre-generate index buffer (quads always have same index pattern) */
    for (uint32_t i = 0; i < SPRITE_MAX_BATCH; i++) {
        uint32_t base_vertex = i * 4;
        uint32_t base_index = i * 6;
        sr->indices[base_index + 0] = (uint16_t)(base_vertex + 0);
        sr->indices[base_index + 1] = (uint16_t)(base_vertex + 1);
        sr->indices[base_index + 2] = (uint16_t)(base_vertex + 2);
        sr->indices[base_index + 3] = (uint16_t)(base_vertex + 0);
        sr->indices[base_index + 4] = (uint16_t)(base_vertex + 2);
        sr->indices[base_index + 5] = (uint16_t)(base_vertex + 3);
    }

    /* Create GPU buffers */
    SDL_GPUBufferCreateInfo vb_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = (Uint32)(SPRITE_VERTEX_CAPACITY * sizeof(Agentite_SpriteVertex)),
        .props = 0
    };
    sr->vertex_buffer = SDL_CreateGPUBuffer(gpu, &vb_info);
    if (!sr->vertex_buffer) {
        agentite_set_error_from_sdl("Sprite: Failed to create vertex buffer");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    SDL_GPUBufferCreateInfo ib_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = (Uint32)(SPRITE_INDEX_CAPACITY * sizeof(uint16_t)),
        .props = 0
    };
    sr->index_buffer = SDL_CreateGPUBuffer(gpu, &ib_info);
    if (!sr->index_buffer) {
        agentite_set_error_from_sdl("Sprite: Failed to create index buffer");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Create sampler (nearest for pixel art) */
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_NEAREST,  /* Pixel art friendly */
        .mag_filter = SDL_GPU_FILTER_NEAREST,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sr->sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
    if (!sr->sampler) {
        agentite_set_error_from_sdl("Sprite: Failed to create sampler");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Create linear sampler (for post-process effects) */
    SDL_GPUSamplerCreateInfo linear_sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    sr->linear_sampler = SDL_CreateGPUSampler(gpu, &linear_sampler_info);
    if (!sr->linear_sampler) {
        agentite_set_error_from_sdl("Sprite: Failed to create linear sampler");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Create pipeline */
    if (!sprite_create_pipeline(sr)) {
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Create vignette post-process pipeline */
    if (!sprite_create_vignette_pipeline(sr)) {
        SDL_Log("Sprite: Warning - vignette pipeline creation failed, effect disabled");
        /* Non-fatal - continue without vignette */
    }

    SDL_Log("Sprite: Renderer initialized (%dx%d)", sr->screen_width, sr->screen_height);
    return sr;
}

void agentite_sprite_shutdown(Agentite_SpriteRenderer *sr)
{
    if (!sr) return;

    if (sr->pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(sr->gpu, sr->pipeline);
    }
    if (sr->vignette_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(sr->gpu, sr->vignette_pipeline);
    }
    if (sr->vertex_buffer) {
        SDL_ReleaseGPUBuffer(sr->gpu, sr->vertex_buffer);
    }
    if (sr->index_buffer) {
        SDL_ReleaseGPUBuffer(sr->gpu, sr->index_buffer);
    }
    if (sr->sampler) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->sampler);
    }
    if (sr->linear_sampler) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->linear_sampler);
    }

    free(sr->vertices);
    free(sr->indices);
    free(sr);

    SDL_Log("Sprite: Renderer shutdown complete");
}

void agentite_sprite_set_screen_size(Agentite_SpriteRenderer *sr, int width, int height)
{
    if (!sr) return;
    sr->screen_width = width;
    sr->screen_height = height;
}

/* ============================================================================
 * Texture Functions
 * ============================================================================ */

Agentite_Texture *agentite_texture_load(Agentite_SpriteRenderer *sr, const char *path)
{
    if (!sr || !path) return NULL;

    /* Load image with stb_image */
    int width, height, channels;
    unsigned char *pixels = stbi_load(path, &width, &height, &channels, 4);  /* Force RGBA */
    if (!pixels) {
        agentite_set_error("Sprite: Failed to load image '%s': %s", path, stbi_failure_reason());
        return NULL;
    }

    Agentite_Texture *texture = agentite_texture_create(sr, width, height, pixels);
    stbi_image_free(pixels);

    if (texture) {
        SDL_Log("Sprite: Loaded texture '%s' (%dx%d)", path, width, height);
    }

    return texture;
}

Agentite_Texture *agentite_texture_load_memory(Agentite_SpriteRenderer *sr,
                                           const void *data, int size)
{
    if (!sr || !data || size <= 0) return NULL;

    int width, height, channels;
    unsigned char *pixels = stbi_load_from_memory((const unsigned char*)data, size, &width, &height, &channels, 4);
    if (!pixels) {
        agentite_set_error("Sprite: Failed to load image from memory: %s", stbi_failure_reason());
        return NULL;
    }

    Agentite_Texture *texture = agentite_texture_create(sr, width, height, pixels);
    stbi_image_free(pixels);

    return texture;
}

Agentite_Texture *agentite_texture_create(Agentite_SpriteRenderer *sr,
                                      int width, int height,
                                      const void *pixels)
{
    if (!sr || width <= 0 || height <= 0 || !pixels) return NULL;

    Agentite_Texture *texture = (Agentite_Texture*)calloc(1, sizeof(Agentite_Texture));
    if (!texture) return NULL;

    texture->width = width;
    texture->height = height;

    /* Create GPU texture */
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = (Uint32)width,
        .height = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0
    };
    texture->gpu_texture = SDL_CreateGPUTexture(sr->gpu, &tex_info);
    if (!texture->gpu_texture) {
        agentite_set_error_from_sdl("Sprite: Failed to create GPU texture");
        free(texture);
        return NULL;
    }

    /* Upload pixel data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(width * height * 4),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);
    if (!transfer) {
        agentite_set_error_from_sdl("Sprite: Failed to create transfer buffer");
        SDL_ReleaseGPUTexture(sr->gpu, texture->gpu_texture);
        free(texture);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, pixels, width * height * 4);
        SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
    }

    /* Copy to texture */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sr->gpu);
    if (cmd) {
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
        if (copy_pass) {
            SDL_GPUTextureTransferInfo src = {
                .transfer_buffer = transfer,
                .offset = 0,
                .pixels_per_row = (Uint32)width,
                .rows_per_layer = (Uint32)height
            };
            SDL_GPUTextureRegion dst = {
                .texture = texture->gpu_texture,
                .mip_level = 0,
                .layer = 0,
                .x = 0,
                .y = 0,
                .z = 0,
                .w = (Uint32)width,
                .h = (Uint32)height,
                .d = 1
            };
            SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
            SDL_EndGPUCopyPass(copy_pass);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
    }

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);

    return texture;
}

void agentite_texture_destroy(Agentite_SpriteRenderer *sr, Agentite_Texture *texture)
{
    if (!sr || !texture) return;

    if (texture->gpu_texture) {
        SDL_ReleaseGPUTexture(sr->gpu, texture->gpu_texture);
    }
    free(texture);
}

void agentite_texture_get_size(Agentite_Texture *texture, int *width, int *height)
{
    if (!texture) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = texture->width;
    if (height) *height = texture->height;
}

/* ============================================================================
 * Sprite Functions
 * ============================================================================ */

Agentite_Sprite agentite_sprite_from_texture(Agentite_Texture *texture)
{
    Agentite_Sprite sprite = {0};
    if (texture) {
        sprite.texture = texture;
        sprite.src_x = 0;
        sprite.src_y = 0;
        sprite.src_w = (float)texture->width;
        sprite.src_h = (float)texture->height;
        sprite.origin_x = 0.5f;
        sprite.origin_y = 0.5f;
    }
    return sprite;
}

Agentite_Sprite agentite_sprite_create(Agentite_Texture *texture,
                                   float src_x, float src_y,
                                   float src_w, float src_h)
{
    Agentite_Sprite sprite = {0};
    if (texture) {
        sprite.texture = texture;
        sprite.src_x = src_x;
        sprite.src_y = src_y;
        sprite.src_w = src_w;
        sprite.src_h = src_h;
        sprite.origin_x = 0.5f;
        sprite.origin_y = 0.5f;
    }
    return sprite;
}

void agentite_sprite_set_origin(Agentite_Sprite *sprite, float ox, float oy)
{
    if (!sprite) return;
    sprite->origin_x = ox;
    sprite->origin_y = oy;
}

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

void agentite_sprite_begin(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd)
{
    if (!sr) return;

    sr->vertex_count = 0;
    sr->index_count = 0;
    sr->sprite_count = 0;
    sr->current_texture = NULL;
    sr->current_cmd = cmd;  /* Store for reference */
    sr->segment_count = 0;
    sr->current_segment_start = 0;
    sr->batch_started = true;
}

/* Internal: Add a sprite quad to the batch */
static void sprite_add_quad(Agentite_SpriteRenderer *sr,
                            float x0, float y0, float x1, float y1,
                            float x2, float y2, float x3, float y3,
                            float u0, float v0, float u1, float v1,
                            float r, float g, float b, float a)
{
    if (sr->sprite_count >= SPRITE_MAX_BATCH) {
        SDL_Log("Sprite: Batch overflow, sprite dropped");
        return;
    }

    uint32_t base = sr->sprite_count * 4;
    Agentite_SpriteVertex *v = &sr->vertices[base];

    /* Top-left */
    v[0].pos[0] = x0; v[0].pos[1] = y0;
    v[0].uv[0] = u0; v[0].uv[1] = v0;
    v[0].color[0] = r; v[0].color[1] = g; v[0].color[2] = b; v[0].color[3] = a;

    /* Top-right */
    v[1].pos[0] = x1; v[1].pos[1] = y1;
    v[1].uv[0] = u1; v[1].uv[1] = v0;
    v[1].color[0] = r; v[1].color[1] = g; v[1].color[2] = b; v[1].color[3] = a;

    /* Bottom-right */
    v[2].pos[0] = x2; v[2].pos[1] = y2;
    v[2].uv[0] = u1; v[2].uv[1] = v1;
    v[2].color[0] = r; v[2].color[1] = g; v[2].color[2] = b; v[2].color[3] = a;

    /* Bottom-left */
    v[3].pos[0] = x3; v[3].pos[1] = y3;
    v[3].uv[0] = u0; v[3].uv[1] = v1;
    v[3].color[0] = r; v[3].color[1] = g; v[3].color[2] = b; v[3].color[3] = a;

    sr->sprite_count++;
    sr->vertex_count = sr->sprite_count * 4;
    sr->index_count = sr->sprite_count * 6;
}

void agentite_sprite_draw(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                        float x, float y)
{
    agentite_sprite_draw_full(sr, sprite, x, y, 1.0f, 1.0f, 0.0f,
                            sprite ? sprite->origin_x : 0.5f,
                            sprite ? sprite->origin_y : 0.5f,
                            1.0f, 1.0f, 1.0f, 1.0f);
}

void agentite_sprite_draw_scaled(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                               float x, float y, float scale_x, float scale_y)
{
    agentite_sprite_draw_full(sr, sprite, x, y, scale_x, scale_y, 0.0f,
                            sprite ? sprite->origin_x : 0.5f,
                            sprite ? sprite->origin_y : 0.5f,
                            1.0f, 1.0f, 1.0f, 1.0f);
}

void agentite_sprite_draw_ex(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                           float x, float y,
                           float scale_x, float scale_y,
                           float rotation_deg,
                           float origin_x, float origin_y)
{
    agentite_sprite_draw_full(sr, sprite, x, y, scale_x, scale_y, rotation_deg,
                            origin_x, origin_y, 1.0f, 1.0f, 1.0f, 1.0f);
}

void agentite_sprite_draw_tinted(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                               float x, float y,
                               float r, float g, float b, float a)
{
    agentite_sprite_draw_full(sr, sprite, x, y, 1.0f, 1.0f, 0.0f,
                            sprite ? sprite->origin_x : 0.5f,
                            sprite ? sprite->origin_y : 0.5f,
                            r, g, b, a);
}

void agentite_sprite_draw_full(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                             float x, float y,
                             float scale_x, float scale_y,
                             float rotation_deg,
                             float origin_x, float origin_y,
                             float r, float g, float b, float a)
{
    if (!sr || !sprite || !sprite->texture || !sr->batch_started) return;

    /* Handle texture changes by creating new batch segments */
    if (sr->current_texture && sr->current_texture != sprite->texture) {
        /* Save current segment if it has content */
        uint32_t current_indices = sr->index_count - sr->current_segment_start;
        if (current_indices > 0 && sr->segment_count < SPRITE_MAX_SUB_BATCHES) {
            sr->segments[sr->segment_count].texture = sr->current_texture;
            sr->segments[sr->segment_count].start_index = sr->current_segment_start;
            sr->segments[sr->segment_count].index_count = current_indices;
            sr->segment_count++;
            sr->current_segment_start = sr->index_count;
        } else if (sr->segment_count >= SPRITE_MAX_SUB_BATCHES) {
            SDL_Log("Sprite: Warning - too many texture switches, segment dropped");
        }
    }
    sr->current_texture = sprite->texture;

    Agentite_Texture *tex = sprite->texture;
    float tex_w = (float)tex->width;
    float tex_h = (float)tex->height;

    /* Calculate UV coordinates */
    float u0 = sprite->src_x / tex_w;
    float v0 = sprite->src_y / tex_h;
    float u1 = (sprite->src_x + sprite->src_w) / tex_w;
    float v1 = (sprite->src_y + sprite->src_h) / tex_h;

    /* Calculate sprite dimensions */
    float w = sprite->src_w * scale_x;
    float h = sprite->src_h * scale_y;

    /* Calculate origin offset */
    float ox = w * origin_x;
    float oy = h * origin_y;

    /* Calculate corner positions relative to origin */
    float x0 = -ox;
    float y0 = -oy;
    float x1 = w - ox;
    float y1 = h - oy;

    /* Apply rotation if needed */
    float px0, py0, px1, py1, px2, py2, px3, py3;

    if (rotation_deg != 0.0f) {
        float rad = rotation_deg * (3.14159265358979323846f / 180.0f);
        float cos_r = cosf(rad);
        float sin_r = sinf(rad);

        /* Rotate corners around origin */
        px0 = x + x0 * cos_r - y0 * sin_r;
        py0 = y + x0 * sin_r + y0 * cos_r;

        px1 = x + x1 * cos_r - y0 * sin_r;
        py1 = y + x1 * sin_r + y0 * cos_r;

        px2 = x + x1 * cos_r - y1 * sin_r;
        py2 = y + x1 * sin_r + y1 * cos_r;

        px3 = x + x0 * cos_r - y1 * sin_r;
        py3 = y + x0 * sin_r + y1 * cos_r;
    } else {
        /* No rotation - just translate */
        px0 = x + x0; py0 = y + y0;
        px1 = x + x1; py1 = y + y0;
        px2 = x + x1; py2 = y + y1;
        px3 = x + x0; py3 = y + y1;
    }

    sprite_add_quad(sr, px0, py0, px1, py1, px2, py2, px3, py3,
                    u0, v0, u1, v1, r, g, b, a);
}

void agentite_sprite_flush(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                         SDL_GPURenderPass *pass)
{
    if (!sr || !cmd || !pass || sr->sprite_count == 0) return;
    if (!sr->current_texture) return;

    /* Upload vertex data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex)),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);
    if (!transfer) return;

    void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, sr->vertices, sr->vertex_count * sizeof(Agentite_SpriteVertex));
        SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
    }

    /* We need to end the render pass, do the copy, then restart
       Actually, for simplicity we'll upload before the render pass in agentite_sprite_end */

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, sr->pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {
        .buffer = sr->vertex_buffer,
        .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {
        .buffer = sr->index_buffer,
        .offset = 0
    };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Push uniforms */
    float uniforms[4] = {(float)sr->screen_width, (float)sr->screen_height, 0, 0};
    SDL_PushGPUVertexUniformData(cmd, 0, uniforms, sizeof(uniforms));

    /* Bind texture */
    SDL_GPUTextureSamplerBinding tex_binding = {
        .texture = sr->current_texture->gpu_texture,
        .sampler = sr->sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw */
    SDL_DrawGPUIndexedPrimitives(pass, sr->index_count, 1, 0, 0, 0);

    /* Reset batch */
    sr->sprite_count = 0;
    sr->vertex_count = 0;
    sr->index_count = 0;
}

void agentite_sprite_end(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                       SDL_GPURenderPass *pass)
{
    if (!sr || !sr->batch_started) return;

    if (sr->sprite_count > 0 && sr->current_texture) {
        /* Upload vertex data before drawing */
        SDL_GPUTransferBufferCreateInfo transfer_info = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex) +
                    sr->index_count * sizeof(uint16_t)),
            .props = 0
        };
        SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);

        if (transfer) {
            void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
            if (mapped) {
                memcpy(mapped, sr->vertices,
                       sr->vertex_count * sizeof(Agentite_SpriteVertex));
                memcpy((uint8_t *)mapped + sr->vertex_count * sizeof(Agentite_SpriteVertex),
                       sr->indices, sr->index_count * sizeof(uint16_t));
                SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
            }

            /* Copy to GPU buffers - this must happen outside render pass */
            /* NOTE: For proper usage, upload should happen before render pass begins.
               The caller should call agentite_sprite_upload() before beginning render pass,
               then agentite_sprite_render() during render pass. For convenience, we combine here
               but this requires ending/restarting the render pass which is inefficient. */

            /* For now, assume the data is already uploaded or we're doing a simple flow */
            /* In practice, we do the copy pass before the render pass in main.c */

            SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);
        }

        /* Draw if we have a render pass */
        if (pass) {
            /* Bind pipeline */
            SDL_BindGPUGraphicsPipeline(pass, sr->pipeline);

            /* Bind vertex buffer */
            SDL_GPUBufferBinding vb_binding = {
                .buffer = sr->vertex_buffer,
                .offset = 0
            };
            SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

            /* Bind index buffer */
            SDL_GPUBufferBinding ib_binding = {
                .buffer = sr->index_buffer,
                .offset = 0
            };
            SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

            /* Build uniforms: mat4 view_projection + vec2 screen_size + vec2 padding */
            struct {
                float view_projection[16];
                float screen_size[2];
                float padding[2];
            } uniforms;

            if (sr->camera) {
                const float *vp = agentite_camera_get_vp_matrix(sr->camera);
                memcpy(uniforms.view_projection, vp, sizeof(float) * 16);
            } else {
                mat4 ortho;
                glm_ortho(0.0f, (float)sr->screen_width,
                          (float)sr->screen_height, 0.0f,
                          -1.0f, 1.0f, ortho);
                memcpy(uniforms.view_projection, ortho, sizeof(float) * 16);
            }

            uniforms.screen_size[0] = (float)sr->screen_width;
            uniforms.screen_size[1] = (float)sr->screen_height;
            uniforms.padding[0] = 0.0f;
            uniforms.padding[1] = 0.0f;

            SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

            /* Bind texture */
            SDL_GPUTextureSamplerBinding tex_binding = {
                .texture = sr->current_texture->gpu_texture,
                .sampler = sr->sampler
            };
            SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

            /* Draw */
            SDL_DrawGPUIndexedPrimitives(pass, sr->index_count, 1, 0, 0, 0);
        }
    }

    sr->batch_started = false;
}

/* Separate upload function for proper render pass management */
void agentite_sprite_upload(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd)
{
    if (!sr || !cmd || sr->sprite_count == 0) return;

    /* Upload vertex and index data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex) +
                sr->index_count * sizeof(uint16_t)),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);
    if (!transfer) return;

    void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, sr->vertices,
               sr->vertex_count * sizeof(Agentite_SpriteVertex));
        memcpy((uint8_t *)mapped + sr->vertex_count * sizeof(Agentite_SpriteVertex),
               sr->indices, sr->index_count * sizeof(uint16_t));
        SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        /* Upload vertices */
        SDL_GPUTransferBufferLocation src_vert = {
            .transfer_buffer = transfer,
            .offset = 0
        };
        SDL_GPUBufferRegion dst_vert = {
            .buffer = sr->vertex_buffer,
            .offset = 0,
            .size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex))
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        /* Upload indices */
        SDL_GPUTransferBufferLocation src_idx = {
            .transfer_buffer = transfer,
            .offset = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex))
        };
        SDL_GPUBufferRegion dst_idx = {
            .buffer = sr->index_buffer,
            .offset = 0,
            .size = (Uint32)(sr->index_count * sizeof(uint16_t))
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);
}

/* Internal: Render a single batch segment with specific texture and index range */
static void sprite_render_segment(Agentite_SpriteRenderer *sr,
                                   SDL_GPURenderPass *pass, Agentite_Texture *texture,
                                   uint32_t start_index, uint32_t index_count)
{
    /* Bind texture */
    SDL_GPUTextureSamplerBinding tex_binding = {
        .texture = texture->gpu_texture,
        .sampler = sr->sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw this segment */
    SDL_DrawGPUIndexedPrimitives(pass, index_count, 1, start_index, 0, 0);
}

/* Separate render function for proper render pass management */
void agentite_sprite_render(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                          SDL_GPURenderPass *pass)
{
    if (!sr || !cmd || !pass || sr->sprite_count == 0) return;

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, sr->pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {
        .buffer = sr->vertex_buffer,
        .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {
        .buffer = sr->index_buffer,
        .offset = 0
    };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Build uniforms: mat4 view_projection + vec2 screen_size + vec2 padding */
    struct {
        float view_projection[16];
        float screen_size[2];
        float padding[2];
    } uniforms;

    if (sr->camera) {
        /* Use camera's view-projection matrix */
        const float *vp = agentite_camera_get_vp_matrix(sr->camera);
        memcpy(uniforms.view_projection, vp, sizeof(float) * 16);
    } else {
        /* Fallback: identity ortho projection for screen-space rendering */
        mat4 ortho;
        glm_ortho(0.0f, (float)sr->screen_width,
                  (float)sr->screen_height, 0.0f,
                  -1.0f, 1.0f, ortho);
        memcpy(uniforms.view_projection, ortho, sizeof(float) * 16);
    }

    uniforms.screen_size[0] = (float)sr->screen_width;
    uniforms.screen_size[1] = (float)sr->screen_height;
    uniforms.padding[0] = 0.0f;
    uniforms.padding[1] = 0.0f;

    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* Render all saved segments (from texture switches) */
    for (uint32_t i = 0; i < sr->segment_count; i++) {
        sprite_render_segment(sr, pass,
                              sr->segments[i].texture,
                              sr->segments[i].start_index,
                              sr->segments[i].index_count);
    }

    /* Render the final/current segment if it has content */
    if (sr->current_texture) {
        uint32_t final_indices = sr->index_count - sr->current_segment_start;
        if (final_indices > 0) {
            sprite_render_segment(sr, pass,
                                  sr->current_texture,
                                  sr->current_segment_start,
                                  final_indices);
        }
    }
}

/* Set camera for sprite rendering (NULL for screen-space mode) */
void agentite_sprite_set_camera(Agentite_SpriteRenderer *sr, Agentite_Camera *camera)
{
    if (sr) {
        sr->camera = camera;
    }
}

/* Get current camera */
Agentite_Camera *agentite_sprite_get_camera(Agentite_SpriteRenderer *sr)
{
    return sr ? sr->camera : NULL;
}

/* ============================================================================
 * Render-to-Texture Functions
 * ============================================================================ */

Agentite_Texture *agentite_texture_create_render_target(Agentite_SpriteRenderer *sr,
                                                     int width, int height)
{
    if (!sr || width <= 0 || height <= 0) return NULL;

    Agentite_Texture *texture = (Agentite_Texture *)calloc(1, sizeof(Agentite_Texture));
    if (!texture) return NULL;

    texture->width = width;
    texture->height = height;

    /* Create GPU texture with render target usage */
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET,
        .width = (Uint32)width,
        .height = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0
    };
    texture->gpu_texture = SDL_CreateGPUTexture(sr->gpu, &tex_info);
    if (!texture->gpu_texture) {
        SDL_Log("Sprite: Failed to create render target texture: %s", SDL_GetError());
        free(texture);
        return NULL;
    }

    SDL_Log("Sprite: Created render target texture (%dx%d)", width, height);
    return texture;
}

SDL_GPURenderPass *agentite_sprite_begin_render_to_texture(Agentite_SpriteRenderer *sr,
                                                          Agentite_Texture *target,
                                                          SDL_GPUCommandBuffer *cmd,
                                                          float clear_r, float clear_g,
                                                          float clear_b, float clear_a)
{
    if (!sr || !target || !cmd) return NULL;

    SDL_GPUColorTargetInfo color_target = {
        .texture = target->gpu_texture,
        .mip_level = 0,
        .layer_or_depth_plane = 0,
        .clear_color = { clear_r, clear_g, clear_b, clear_a },
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .resolve_texture = NULL,
        .resolve_mip_level = 0,
        .resolve_layer = 0,
        .cycle = false,
        .cycle_resolve_texture = false
    };

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    if (!pass) {
        SDL_Log("Sprite: Failed to begin render-to-texture pass: %s", SDL_GetError());
        return NULL;
    }

    /* Set viewport to texture size */
    SDL_GPUViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .w = (float)target->width,
        .h = (float)target->height,
        .min_depth = 0.0f,
        .max_depth = 1.0f
    };
    SDL_SetGPUViewport(pass, &viewport);

    return pass;
}

void agentite_sprite_render_to_texture(Agentite_SpriteRenderer *sr,
                                      SDL_GPUCommandBuffer *cmd,
                                      SDL_GPURenderPass *pass)
{
    /* Same as agentite_sprite_render */
    agentite_sprite_render(sr, cmd, pass);
}

void agentite_sprite_end_render_to_texture(SDL_GPURenderPass *pass)
{
    if (pass) {
        SDL_EndGPURenderPass(pass);
    }
}

/* ============================================================================
 * Vignette Post-Process Functions
 * ============================================================================ */

bool agentite_sprite_has_vignette(Agentite_SpriteRenderer *sr)
{
    return sr && sr->vignette_pipeline != NULL;
}

void agentite_sprite_render_vignette(Agentite_SpriteRenderer *sr,
                                    SDL_GPUCommandBuffer *cmd,
                                    SDL_GPURenderPass *pass,
                                    Agentite_Texture *scene_texture)
{
    if (!sr || !cmd || !pass || !scene_texture || !sr->vignette_pipeline) return;

    /* Bind vignette pipeline */
    SDL_BindGPUGraphicsPipeline(pass, sr->vignette_pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {
        .buffer = sr->vertex_buffer,
        .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {
        .buffer = sr->index_buffer,
        .offset = 0
    };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Build uniforms: ortho projection for screen-space quad */
    struct {
        float view_projection[16];
        float screen_size[2];
        float padding[2];
    } uniforms;

    glm_ortho(0.0f, (float)sr->screen_width, (float)sr->screen_height, 0.0f,
              -1.0f, 1.0f, (vec4 *)uniforms.view_projection);

    uniforms.screen_size[0] = (float)sr->screen_width;
    uniforms.screen_size[1] = (float)sr->screen_height;
    uniforms.padding[0] = 0.0f;
    uniforms.padding[1] = 0.0f;

    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* Bind scene texture with linear sampler */
    SDL_GPUTextureSamplerBinding tex_binding = {
        .texture = scene_texture->gpu_texture,
        .sampler = sr->linear_sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw a single fullscreen quad (first 6 indices) */
    SDL_DrawGPUIndexedPrimitives(pass, 6, 1, 0, 0, 0);
}

void agentite_sprite_prepare_fullscreen_quad(Agentite_SpriteRenderer *sr)
{
    if (!sr) return;

    /* Set up a single fullscreen quad in the vertex buffer */
    sr->vertex_count = 4;
    sr->index_count = 6;
    sr->sprite_count = 1;

    float w = (float)sr->screen_width;
    float h = (float)sr->screen_height;

    /* Fullscreen quad vertices (position, UV, color) */
    Agentite_SpriteVertex *v = sr->vertices;

    /* Top-left */
    v[0].pos[0] = 0.0f; v[0].pos[1] = 0.0f;
    v[0].uv[0] = 0.0f; v[0].uv[1] = 0.0f;
    v[0].color[0] = 1.0f; v[0].color[1] = 1.0f; v[0].color[2] = 1.0f; v[0].color[3] = 1.0f;

    /* Top-right */
    v[1].pos[0] = w; v[1].pos[1] = 0.0f;
    v[1].uv[0] = 1.0f; v[1].uv[1] = 0.0f;
    v[1].color[0] = 1.0f; v[1].color[1] = 1.0f; v[1].color[2] = 1.0f; v[1].color[3] = 1.0f;

    /* Bottom-right */
    v[2].pos[0] = w; v[2].pos[1] = h;
    v[2].uv[0] = 1.0f; v[2].uv[1] = 1.0f;
    v[2].color[0] = 1.0f; v[2].color[1] = 1.0f; v[2].color[2] = 1.0f; v[2].color[3] = 1.0f;

    /* Bottom-left */
    v[3].pos[0] = 0.0f; v[3].pos[1] = h;
    v[3].uv[0] = 0.0f; v[3].uv[1] = 1.0f;
    v[3].color[0] = 1.0f; v[3].color[1] = 1.0f; v[3].color[2] = 1.0f; v[3].color[3] = 1.0f;
}

void agentite_sprite_upload_fullscreen_quad(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd)
{
    if (!sr || !cmd) return;

    /* Upload vertex data for fullscreen quad */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (Uint32)(4 * sizeof(Agentite_SpriteVertex) + 6 * sizeof(uint16_t)),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);
    if (!transfer) return;

    void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, sr->vertices, 4 * sizeof(Agentite_SpriteVertex));
        memcpy((uint8_t *)mapped + 4 * sizeof(Agentite_SpriteVertex),
               sr->indices, 6 * sizeof(uint16_t));
        SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        /* Upload vertices */
        SDL_GPUTransferBufferLocation src_vert = { .transfer_buffer = transfer, .offset = 0 };
        SDL_GPUBufferRegion dst_vert = {
            .buffer = sr->vertex_buffer,
            .offset = 0,
            .size = (Uint32)(4 * sizeof(Agentite_SpriteVertex))
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        /* Upload indices */
        SDL_GPUTransferBufferLocation src_idx = {
            .transfer_buffer = transfer,
            .offset = (Uint32)(4 * sizeof(Agentite_SpriteVertex))
        };
        SDL_GPUBufferRegion dst_idx = {
            .buffer = sr->index_buffer,
            .offset = 0,
            .size = (Uint32)(6 * sizeof(uint16_t))
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);
}
