#include "agentite/agentite.h"
/*
 * Carbon Sprite/Texture System Implementation
 */

#include "agentite/sprite.h"
#include "agentite/camera.h"
#include "agentite/error.h"
#include "agentite/path.h"
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

/* Embedded SPIRV shaders for Vulkan/D3D12 (cross-platform) */
#include "sprite_shaders_spirv.h"

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
    Agentite_ScaleMode scale_mode;
    Agentite_TextureAddressMode address_mode;
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

    /* Samplers for different scale/address mode combinations */
    SDL_GPUSampler *sampler;              /* Legacy: nearest + clamp */
    SDL_GPUSampler *linear_sampler;       /* Legacy: linear + clamp (post-process) */
    SDL_GPUSampler *nearest_clamp;        /* Nearest filter, clamp to edge */
    SDL_GPUSampler *nearest_repeat;       /* Nearest filter, repeat/tile */
    SDL_GPUSampler *nearest_mirror;       /* Nearest filter, mirror */
    SDL_GPUSampler *linear_clamp;         /* Linear filter, clamp to edge */
    SDL_GPUSampler *linear_repeat;        /* Linear filter, repeat/tile */
    SDL_GPUSampler *linear_mirror;        /* Linear filter, mirror */

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
        /* Create vertex shader (Metal) */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = (const Uint8 *)sprite_shader_msl;
        vs_info.code_size = sizeof(sprite_shader_msl);
        vs_info.entrypoint = "sprite_vertex";
        vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(sr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("Sprite: Failed to create vertex shader");
            return false;
        }

        /* Create fragment shader (Metal) */
        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)sprite_shader_msl;
        fs_info.code_size = sizeof(sprite_shader_msl);
        fs_info.entrypoint = "sprite_fragment";
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(sr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("Sprite: Failed to create fragment shader");
            SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
            return false;
        }
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        /* Create vertex shader (Vulkan/D3D12 via SPIRV) */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = sprite_vert_spv;
        vs_info.code_size = sprite_vert_spv_len;
        vs_info.entrypoint = "main";
        vs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(sr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("Sprite: Failed to create SPIRV vertex shader");
            return false;
        }

        /* Create fragment shader (Vulkan/D3D12 via SPIRV) */
        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = sprite_frag_spv;
        fs_info.code_size = sprite_frag_spv_len;
        fs_info.entrypoint = "main";
        fs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(sr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("Sprite: Failed to create SPIRV fragment shader");
            SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
            return false;
        }
    } else {
        agentite_set_error("Sprite: No supported shader format (need MSL or SPIRV)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(Agentite_SpriteVertex, pos);
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(Agentite_SpriteVertex, uv);
    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[2].offset = offsetof(Agentite_SpriteVertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(Agentite_SpriteVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 3;

    /* Alpha blending */
    SDL_GPUColorTargetBlendState blend_state = {};
    blend_state.enable_blend = true;
    blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                   SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state = blend_state;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
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
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = (const Uint8 *)vignette_shader_msl;
        vs_info.code_size = sizeof(vignette_shader_msl);
        vs_info.entrypoint = "vignette_vertex";
        vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(sr->gpu, &vs_info);
        if (!vertex_shader) {
            SDL_Log("Vignette: Failed to create vertex shader: %s", SDL_GetError());
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)vignette_shader_msl;
        fs_info.code_size = sizeof(vignette_shader_msl);
        fs_info.entrypoint = "vignette_fragment";
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(sr->gpu, &fs_info);
        if (!fragment_shader) {
            SDL_Log("Vignette: Failed to create fragment shader: %s", SDL_GetError());
            SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
            return false;
        }
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = vignette_vert_spv;
        vs_info.code_size = vignette_vert_spv_len;
        vs_info.entrypoint = "main";
        vs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(sr->gpu, &vs_info);
        if (!vertex_shader) {
            SDL_Log("Vignette: Failed to create SPIRV vertex shader: %s", SDL_GetError());
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = vignette_frag_spv;
        fs_info.code_size = vignette_frag_spv_len;
        fs_info.entrypoint = "main";
        fs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(sr->gpu, &fs_info);
        if (!fragment_shader) {
            SDL_Log("Vignette: Failed to create SPIRV fragment shader: %s", SDL_GetError());
            SDL_ReleaseGPUShader(sr->gpu, vertex_shader);
            return false;
        }
    } else {
        return false;
    }

    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(Agentite_SpriteVertex, pos);
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(Agentite_SpriteVertex, uv);
    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[2].offset = offsetof(Agentite_SpriteVertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(Agentite_SpriteVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 3;

    SDL_GPUColorTargetBlendState blend_state = {};
    blend_state.enable_blend = false;
    blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
                                   SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;

    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    color_target.blend_state = blend_state;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vertex_shader;
    pipeline_info.fragment_shader = fragment_shader;
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
 * Internal: Sampler Helpers
 * ============================================================================ */

/* Create a sampler with specified filter and address modes */
static SDL_GPUSampler *create_sampler(SDL_GPUDevice *gpu,
                                       SDL_GPUFilter filter,
                                       SDL_GPUSamplerAddressMode address_mode)
{
    SDL_GPUSamplerCreateInfo info = {};
    info.min_filter = filter;
    info.mag_filter = filter;
    info.mipmap_mode = (filter == SDL_GPU_FILTER_LINEAR)
                       ? SDL_GPU_SAMPLERMIPMAPMODE_LINEAR
                       : SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    info.address_mode_u = address_mode;
    info.address_mode_v = address_mode;
    info.address_mode_w = address_mode;
    return SDL_CreateGPUSampler(gpu, &info);
}

/* Get the appropriate sampler for a texture based on its scale and address modes */
static SDL_GPUSampler *get_sampler_for_texture(Agentite_SpriteRenderer *sr,
                                                const Agentite_Texture *texture)
{
    if (!sr || !texture) return sr ? sr->sampler : NULL;

    /* PIXELART mode uses nearest filtering */
    bool use_linear = (texture->scale_mode == AGENTITE_SCALEMODE_LINEAR);

    switch (texture->address_mode) {
    case AGENTITE_ADDRESSMODE_REPEAT:
        return use_linear ? sr->linear_repeat : sr->nearest_repeat;
    case AGENTITE_ADDRESSMODE_MIRROR:
        return use_linear ? sr->linear_mirror : sr->nearest_mirror;
    case AGENTITE_ADDRESSMODE_CLAMP:
    default:
        return use_linear ? sr->linear_clamp : sr->nearest_clamp;
    }
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

Agentite_SpriteRenderer *agentite_sprite_init(SDL_GPUDevice *gpu, SDL_Window *window)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!gpu || !window) return NULL;

    Agentite_SpriteRenderer *sr = (Agentite_SpriteRenderer*)calloc(1, sizeof(Agentite_SpriteRenderer));
    if (!sr) {
        agentite_set_error("Sprite: Failed to allocate renderer");
        return NULL;
    }

    sr->gpu = gpu;
    sr->window = window;

    /* Get window size in logical coordinates (matches camera and text renderer) */
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
    SDL_GPUBufferCreateInfo vb_info = {};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size = (Uint32)(SPRITE_VERTEX_CAPACITY * sizeof(Agentite_SpriteVertex));
    vb_info.props = 0;
    sr->vertex_buffer = SDL_CreateGPUBuffer(gpu, &vb_info);
    if (!sr->vertex_buffer) {
        agentite_set_error_from_sdl("Sprite: Failed to create vertex buffer");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    SDL_GPUBufferCreateInfo ib_info = {};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size = (Uint32)(SPRITE_INDEX_CAPACITY * sizeof(uint16_t));
    ib_info.props = 0;
    sr->index_buffer = SDL_CreateGPUBuffer(gpu, &ib_info);
    if (!sr->index_buffer) {
        agentite_set_error_from_sdl("Sprite: Failed to create index buffer");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Create samplers for all scale/address mode combinations */
    sr->nearest_clamp = create_sampler(gpu, SDL_GPU_FILTER_NEAREST,
                                       SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
    sr->nearest_repeat = create_sampler(gpu, SDL_GPU_FILTER_NEAREST,
                                        SDL_GPU_SAMPLERADDRESSMODE_REPEAT);
    sr->nearest_mirror = create_sampler(gpu, SDL_GPU_FILTER_NEAREST,
                                        SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT);
    sr->linear_clamp = create_sampler(gpu, SDL_GPU_FILTER_LINEAR,
                                      SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE);
    sr->linear_repeat = create_sampler(gpu, SDL_GPU_FILTER_LINEAR,
                                       SDL_GPU_SAMPLERADDRESSMODE_REPEAT);
    sr->linear_mirror = create_sampler(gpu, SDL_GPU_FILTER_LINEAR,
                                       SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT);

    /* Check all samplers created successfully */
    if (!sr->nearest_clamp || !sr->nearest_repeat || !sr->nearest_mirror ||
        !sr->linear_clamp || !sr->linear_repeat || !sr->linear_mirror) {
        agentite_set_error_from_sdl("Sprite: Failed to create samplers");
        agentite_sprite_shutdown(sr);
        return NULL;
    }

    /* Legacy aliases for backwards compatibility */
    sr->sampler = sr->nearest_clamp;
    sr->linear_sampler = sr->linear_clamp;

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
    AGENTITE_ASSERT_MAIN_THREAD();
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
    /* Release all samplers (sampler and linear_sampler are aliases, not separate) */
    if (sr->nearest_clamp) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->nearest_clamp);
    }
    if (sr->nearest_repeat) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->nearest_repeat);
    }
    if (sr->nearest_mirror) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->nearest_mirror);
    }
    if (sr->linear_clamp) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->linear_clamp);
    }
    if (sr->linear_repeat) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->linear_repeat);
    }
    if (sr->linear_mirror) {
        SDL_ReleaseGPUSampler(sr->gpu, sr->linear_mirror);
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

/* Internal: Upload pixel data to an existing GPU texture */
static bool upload_pixels_to_gpu(Agentite_SpriteRenderer *sr,
                                 SDL_GPUTexture *gpu_texture,
                                 int width, int height,
                                 const void *pixels)
{
    if (!sr || !gpu_texture || !pixels || width <= 0 || height <= 0) {
        return false;
    }

    /* Create transfer buffer */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)(width * height * 4);
    transfer_info.props = 0;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);
    if (!transfer) {
        agentite_set_error_from_sdl("Sprite: Failed to create transfer buffer");
        return false;
    }

    /* Map and copy pixels */
    void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, pixels, width * height * 4);
        SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
    }

    /* Copy to texture */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(sr->gpu);
    if (!cmd) {
        agentite_set_error_from_sdl("Sprite: Failed to acquire command buffer for texture upload");
        SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);
        return false;
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        SDL_GPUTextureTransferInfo src = {};
        src.transfer_buffer = transfer;
        src.offset = 0;
        src.pixels_per_row = (Uint32)width;
        src.rows_per_layer = (Uint32)height;
        SDL_GPUTextureRegion dst = {};
        dst.texture = gpu_texture;
        dst.mip_level = 0;
        dst.layer = 0;
        dst.x = 0;
        dst.y = 0;
        dst.z = 0;
        dst.w = (Uint32)width;
        dst.h = (Uint32)height;
        dst.d = 1;
        SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
        SDL_EndGPUCopyPass(copy_pass);
    }
    SDL_SubmitGPUCommandBuffer(cmd);

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);
    return true;
}

Agentite_Texture *agentite_texture_load(Agentite_SpriteRenderer *sr, const char *path)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !path) return NULL;

    /* Validate path to prevent directory traversal attacks */
    if (!agentite_path_is_safe(path)) {
        agentite_set_error("Sprite: Invalid path (directory traversal rejected): '%s'", path);
        return NULL;
    }

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
    AGENTITE_ASSERT_MAIN_THREAD();
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
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || width <= 0 || height <= 0 || !pixels) return NULL;

    Agentite_Texture *texture = (Agentite_Texture*)calloc(1, sizeof(Agentite_Texture));
    if (!texture) return NULL;

    texture->width = width;
    texture->height = height;
    texture->scale_mode = AGENTITE_SCALEMODE_NEAREST;  /* Default: pixel-art friendly */
    texture->address_mode = AGENTITE_ADDRESSMODE_CLAMP;

    /* Create GPU texture */
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.props = 0;
    texture->gpu_texture = SDL_CreateGPUTexture(sr->gpu, &tex_info);
    if (!texture->gpu_texture) {
        agentite_set_error_from_sdl("Sprite: Failed to create GPU texture");
        free(texture);
        return NULL;
    }

    /* Upload pixel data using helper */
    if (!upload_pixels_to_gpu(sr, texture->gpu_texture, width, height, pixels)) {
        SDL_ReleaseGPUTexture(sr->gpu, texture->gpu_texture);
        free(texture);
        return NULL;
    }

    return texture;
}

void agentite_texture_destroy(Agentite_SpriteRenderer *sr, Agentite_Texture *texture)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !texture) return;

    if (texture->gpu_texture) {
        SDL_ReleaseGPUTexture(sr->gpu, texture->gpu_texture);
    }
    free(texture);
}

void agentite_texture_get_size(const Agentite_Texture *texture, int *width, int *height)
{
    if (!texture) {
        if (width) *width = 0;
        if (height) *height = 0;
        return;
    }
    if (width) *width = texture->width;
    if (height) *height = texture->height;
}

void agentite_texture_set_scale_mode(Agentite_Texture *texture, Agentite_ScaleMode mode)
{
    if (texture) {
        texture->scale_mode = mode;
    }
}

Agentite_ScaleMode agentite_texture_get_scale_mode(const Agentite_Texture *texture)
{
    return texture ? texture->scale_mode : AGENTITE_SCALEMODE_NEAREST;
}

void agentite_texture_set_address_mode(Agentite_Texture *texture, Agentite_TextureAddressMode mode)
{
    if (texture) {
        texture->address_mode = mode;
    }
}

Agentite_TextureAddressMode agentite_texture_get_address_mode(const Agentite_Texture *texture)
{
    return texture ? texture->address_mode : AGENTITE_ADDRESSMODE_CLAMP;
}

bool agentite_texture_reload(Agentite_SpriteRenderer *sr,
                             Agentite_Texture *texture,
                             const char *path)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !texture || !path) {
        agentite_set_error("Sprite: Invalid parameters for texture reload");
        return false;
    }

    /* Validate path to prevent directory traversal attacks */
    if (!agentite_path_is_safe(path)) {
        agentite_set_error("Sprite: Invalid path (directory traversal rejected): '%s'", path);
        return false;
    }

    /* Load new pixel data from disk */
    int new_width, new_height, channels;
    unsigned char *pixels = stbi_load(path, &new_width, &new_height, &channels, 4);
    if (!pixels) {
        agentite_set_error("Sprite: Failed to reload texture '%s': %s", path, stbi_failure_reason());
        return false;
    }

    /* Check if dimensions changed */
    bool dimensions_changed = (new_width != texture->width || new_height != texture->height);

    /* If dimensions changed, recreate GPU texture */
    if (dimensions_changed) {
        /* Release old GPU texture */
        SDL_ReleaseGPUTexture(sr->gpu, texture->gpu_texture);

        /* Create new GPU texture with new dimensions */
        SDL_GPUTextureCreateInfo tex_info = {};
        tex_info.type = SDL_GPU_TEXTURETYPE_2D;
        tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
        tex_info.width = (Uint32)new_width;
        tex_info.height = (Uint32)new_height;
        tex_info.layer_count_or_depth = 1;
        tex_info.num_levels = 1;
        tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        tex_info.props = 0;
        texture->gpu_texture = SDL_CreateGPUTexture(sr->gpu, &tex_info);
        if (!texture->gpu_texture) {
            stbi_image_free(pixels);
            agentite_set_error_from_sdl("Sprite: Failed to recreate GPU texture");
            return false;
        }

        texture->width = new_width;
        texture->height = new_height;
    }

    /* Upload new pixel data to GPU */
    bool upload_ok = upload_pixels_to_gpu(sr, texture->gpu_texture,
                                          new_width, new_height, pixels);

    stbi_image_free(pixels);

    if (!upload_ok) {
        agentite_set_error("Sprite: Failed to upload reloaded texture data");
        return false;
    }

    SDL_Log("Sprite: Reloaded texture '%s' (%dx%d)%s", path, new_width, new_height,
            dimensions_changed ? " [dimensions changed]" : "");
    return true;
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
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !cmd || !pass || sr->sprite_count == 0) return;
    if (!sr->current_texture) return;

    /* Upload vertex data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex));
    transfer_info.props = 0;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(sr->gpu, &transfer_info);
    if (!transfer) return;

    void *mapped = SDL_MapGPUTransferBuffer(sr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, sr->vertices, sr->vertex_count * sizeof(Agentite_SpriteVertex));
        SDL_UnmapGPUTransferBuffer(sr->gpu, transfer);
    }

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, sr->pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = sr->vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {};
    ib_binding.buffer = sr->index_buffer;
    ib_binding.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Push uniforms */
    float uniforms[4] = {(float)sr->screen_width, (float)sr->screen_height, 0, 0};
    SDL_PushGPUVertexUniformData(cmd, 0, uniforms, sizeof(uniforms));

    /* Bind texture */
    SDL_GPUTextureSamplerBinding tex_binding = {};
    tex_binding.texture = sr->current_texture->gpu_texture;
    tex_binding.sampler = sr->sampler;
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw */
    SDL_DrawGPUIndexedPrimitives(pass, sr->index_count, 1, 0, 0, 0);

    /* Reset batch */
    sr->sprite_count = 0;
    sr->vertex_count = 0;
    sr->index_count = 0;
}

/* Upload sprite batch to GPU (call BEFORE render pass begins) */
void agentite_sprite_upload(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !cmd || sr->sprite_count == 0) return;

    /* Upload vertex and index data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex) +
            sr->index_count * sizeof(uint16_t));
    transfer_info.props = 0;
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
        SDL_GPUTransferBufferLocation src_vert = {};
        src_vert.transfer_buffer = transfer;
        src_vert.offset = 0;
        SDL_GPUBufferRegion dst_vert = {};
        dst_vert.buffer = sr->vertex_buffer;
        dst_vert.offset = 0;
        dst_vert.size = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex));
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        /* Upload indices */
        SDL_GPUTransferBufferLocation src_idx = {};
        src_idx.transfer_buffer = transfer;
        src_idx.offset = (Uint32)(sr->vertex_count * sizeof(Agentite_SpriteVertex));
        SDL_GPUBufferRegion dst_idx = {};
        dst_idx.buffer = sr->index_buffer;
        dst_idx.offset = 0;
        dst_idx.size = (Uint32)(sr->index_count * sizeof(uint16_t));
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
    /* Bind texture with appropriate sampler based on texture's scale/address modes */
    SDL_GPUTextureSamplerBinding tex_binding = {};
    tex_binding.texture = texture->gpu_texture;
    tex_binding.sampler = get_sampler_for_texture(sr, texture);
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw this segment */
    SDL_DrawGPUIndexedPrimitives(pass, index_count, 1, start_index, 0, 0);
}

/* Separate render function for proper render pass management */
void agentite_sprite_render(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                          SDL_GPURenderPass *pass)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !cmd || !pass || sr->sprite_count == 0) return;

    static bool logged_render = false;
    if (!logged_render) {
        SDL_Log("DEBUG: agentite_sprite_render - camera=%p, screen=%dx%d, sprites=%d",
                (void*)sr->camera, sr->screen_width, sr->screen_height, sr->sprite_count);
        logged_render = true;
    }

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, sr->pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = sr->vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {};
    ib_binding.buffer = sr->index_buffer;
    ib_binding.offset = 0;
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
Agentite_Camera *agentite_sprite_get_camera(const Agentite_SpriteRenderer *sr)
{
    return sr ? sr->camera : NULL;
}

/* ============================================================================
 * Render-to-Texture Functions
 * ============================================================================ */

Agentite_Texture *agentite_texture_create_render_target(Agentite_SpriteRenderer *sr,
                                                     int width, int height)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || width <= 0 || height <= 0) return NULL;

    Agentite_Texture *texture = (Agentite_Texture *)calloc(1, sizeof(Agentite_Texture));
    if (!texture) return NULL;

    texture->width = width;
    texture->height = height;
    texture->scale_mode = AGENTITE_SCALEMODE_LINEAR;  /* Render targets typically use linear */
    texture->address_mode = AGENTITE_ADDRESSMODE_CLAMP;

    /* Create GPU texture with render target usage */
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.props = 0;
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
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !target || !cmd) return NULL;

    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = target->gpu_texture;
    color_target.mip_level = 0;
    color_target.layer_or_depth_plane = 0;
    color_target.clear_color.r = clear_r;
    color_target.clear_color.g = clear_g;
    color_target.clear_color.b = clear_b;
    color_target.clear_color.a = clear_a;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.resolve_texture = NULL;
    color_target.resolve_mip_level = 0;
    color_target.resolve_layer = 0;
    color_target.cycle = false;
    color_target.cycle_resolve_texture = false;

    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color_target, 1, NULL);
    if (!pass) {
        SDL_Log("Sprite: Failed to begin render-to-texture pass: %s", SDL_GetError());
        return NULL;
    }

    /* Set viewport to texture size */
    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = (float)target->width;
    viewport.h = (float)target->height;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
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
    AGENTITE_ASSERT_MAIN_THREAD();
    if (pass) {
        SDL_EndGPURenderPass(pass);
    }
}

/* ============================================================================
 * Vignette Post-Process Functions
 * ============================================================================ */

bool agentite_sprite_has_vignette(const Agentite_SpriteRenderer *sr)
{
    return sr && sr->vignette_pipeline != NULL;
}

void agentite_sprite_render_vignette(Agentite_SpriteRenderer *sr,
                                    SDL_GPUCommandBuffer *cmd,
                                    SDL_GPURenderPass *pass,
                                    Agentite_Texture *scene_texture)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !cmd || !pass || !scene_texture || !sr->vignette_pipeline) return;

    /* Bind vignette pipeline */
    SDL_BindGPUGraphicsPipeline(pass, sr->vignette_pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = sr->vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {};
    ib_binding.buffer = sr->index_buffer;
    ib_binding.offset = 0;
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
    SDL_GPUTextureSamplerBinding tex_binding = {};
    tex_binding.texture = scene_texture->gpu_texture;
    tex_binding.sampler = sr->linear_sampler;
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
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!sr || !cmd) return;

    /* Upload vertex data for fullscreen quad */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)(4 * sizeof(Agentite_SpriteVertex) + 6 * sizeof(uint16_t));
    transfer_info.props = 0;
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
        SDL_GPUTransferBufferLocation src_vert = {};
        src_vert.transfer_buffer = transfer;
        src_vert.offset = 0;
        SDL_GPUBufferRegion dst_vert = {};
        dst_vert.buffer = sr->vertex_buffer;
        dst_vert.offset = 0;
        dst_vert.size = (Uint32)(4 * sizeof(Agentite_SpriteVertex));
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        /* Upload indices */
        SDL_GPUTransferBufferLocation src_idx = {};
        src_idx.transfer_buffer = transfer;
        src_idx.offset = (Uint32)(4 * sizeof(Agentite_SpriteVertex));
        SDL_GPUBufferRegion dst_idx = {};
        dst_idx.buffer = sr->index_buffer;
        dst_idx.offset = 0;
        dst_idx.size = (Uint32)(6 * sizeof(uint16_t));
        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(sr->gpu, transfer);
}

/* ============================================================================
 * Asset Handle Integration
 * ============================================================================ */

#include "agentite/asset.h"

Agentite_AssetHandle agentite_texture_load_asset(Agentite_SpriteRenderer *sr,
                                                  Agentite_AssetRegistry *registry,
                                                  const char *path)
{
    if (!sr || !registry || !path) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Check if already loaded */
    Agentite_AssetHandle existing = agentite_asset_lookup(registry, path);
    if (agentite_asset_is_valid(existing)) {
        /* Already loaded - add reference and return */
        agentite_asset_addref(registry, existing);
        return existing;
    }

    /* Load the texture */
    Agentite_Texture *texture = agentite_texture_load(sr, path);
    if (!texture) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Register with asset system */
    Agentite_AssetHandle handle = agentite_asset_register(
        registry, path, AGENTITE_ASSET_TEXTURE, texture);

    if (!agentite_asset_is_valid(handle)) {
        /* Registration failed - clean up texture */
        agentite_texture_destroy(sr, texture);
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    return handle;
}

Agentite_Texture *agentite_texture_from_handle(Agentite_AssetRegistry *registry,
                                                Agentite_AssetHandle handle)
{
    if (!registry) return NULL;

    /* Verify it's a texture type */
    if (agentite_asset_get_type(registry, handle) != AGENTITE_ASSET_TEXTURE) {
        return NULL;
    }

    return (Agentite_Texture *)agentite_asset_get_data(registry, handle);
}

void agentite_texture_asset_destructor(void *data, Agentite_AssetType type, void *userdata)
{
    if (type != AGENTITE_ASSET_TEXTURE) return;

    Agentite_SpriteRenderer *sr = (Agentite_SpriteRenderer *)userdata;
    Agentite_Texture *texture = (Agentite_Texture *)data;

    if (sr && texture) {
        agentite_texture_destroy(sr, texture);
    }
}
