/*
 * Agentite UI - Drawing and GPU Pipeline
 *
 * Implements a draw command queue with layer sorting and multi-texture batching.
 */

#include "agentite/ui.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

/* ============================================================================
 * Embedded MSL Shader Source
 * ============================================================================ */

/* Bitmap font / solid primitives shader */
static const char ui_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float2 screen_size;\n"
"    float2 padding;\n"
"};\n"
"\n"
"struct VertexIn {\n"
"    float2 position [[attribute(0)]];\n"
"    float2 texcoord [[attribute(1)]];\n"
"    uint color [[attribute(2)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texcoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"vertex VertexOut ui_vertex(\n"
"    VertexIn in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOut out;\n"
"    float2 ndc;\n"
"    ndc.x = (in.position.x / uniforms.screen_size.x) * 2.0 - 1.0;\n"
"    ndc.y = 1.0 - (in.position.y / uniforms.screen_size.y) * 2.0;\n"
"    out.position = float4(ndc, 0.0, 1.0);\n"
"    out.texcoord = in.texcoord;\n"
"    out.color.r = float((in.color >> 0) & 0xFF) / 255.0;\n"
"    out.color.g = float((in.color >> 8) & 0xFF) / 255.0;\n"
"    out.color.b = float((in.color >> 16) & 0xFF) / 255.0;\n"
"    out.color.a = float((in.color >> 24) & 0xFF) / 255.0;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 ui_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> font_atlas [[texture(0)]],\n"
"    sampler font_sampler [[sampler(0)]]\n"
") {\n"
"    float alpha = font_atlas.sample(font_sampler, in.texcoord).r;\n"
"    return float4(in.color.rgb, in.color.a * alpha);\n"
"}\n";

/* SDF (single-channel) font shader */
static const char ui_sdf_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float2 screen_size;\n"
"    float2 sdf_params;\n"  /* x = distance_range, y = scale */
"};\n"
"\n"
"struct VertexIn {\n"
"    float2 position [[attribute(0)]];\n"
"    float2 texcoord [[attribute(1)]];\n"
"    uint color [[attribute(2)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texcoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"vertex VertexOut ui_sdf_vertex(\n"
"    VertexIn in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOut out;\n"
"    float2 ndc;\n"
"    ndc.x = (in.position.x / uniforms.screen_size.x) * 2.0 - 1.0;\n"
"    ndc.y = 1.0 - (in.position.y / uniforms.screen_size.y) * 2.0;\n"
"    out.position = float4(ndc, 0.0, 1.0);\n"
"    out.texcoord = in.texcoord;\n"
"    out.color.r = float((in.color >> 0) & 0xFF) / 255.0;\n"
"    out.color.g = float((in.color >> 8) & 0xFF) / 255.0;\n"
"    out.color.b = float((in.color >> 16) & 0xFF) / 255.0;\n"
"    out.color.a = float((in.color >> 24) & 0xFF) / 255.0;\n"
"    return out;\n"
"}\n"
"\n"
"fragment float4 ui_sdf_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> sdf_atlas [[texture(0)]],\n"
"    sampler sdf_sampler [[sampler(0)]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    float distance = sdf_atlas.sample(sdf_sampler, in.texcoord).r;\n"
"    float screen_px_distance = uniforms.sdf_params.x * (distance - 0.5);\n"
"    float opacity = clamp(screen_px_distance * uniforms.sdf_params.y + 0.5, 0.0, 1.0);\n"
"    return float4(in.color.rgb, in.color.a * opacity);\n"
"}\n";

/* MSDF (multi-channel) font shader */
static const char ui_msdf_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float2 screen_size;\n"
"    float2 sdf_params;\n"  /* x = distance_range, y = scale */
"};\n"
"\n"
"struct VertexIn {\n"
"    float2 position [[attribute(0)]];\n"
"    float2 texcoord [[attribute(1)]];\n"
"    uint color [[attribute(2)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texcoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"vertex VertexOut ui_msdf_vertex(\n"
"    VertexIn in [[stage_in]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    VertexOut out;\n"
"    float2 ndc;\n"
"    ndc.x = (in.position.x / uniforms.screen_size.x) * 2.0 - 1.0;\n"
"    ndc.y = 1.0 - (in.position.y / uniforms.screen_size.y) * 2.0;\n"
"    out.position = float4(ndc, 0.0, 1.0);\n"
"    out.texcoord = in.texcoord;\n"
"    out.color.r = float((in.color >> 0) & 0xFF) / 255.0;\n"
"    out.color.g = float((in.color >> 8) & 0xFF) / 255.0;\n"
"    out.color.b = float((in.color >> 16) & 0xFF) / 255.0;\n"
"    out.color.a = float((in.color >> 24) & 0xFF) / 255.0;\n"
"    return out;\n"
"}\n"
"\n"
"float median(float r, float g, float b) {\n"
"    return max(min(r, g), min(max(r, g), b));\n"
"}\n"
"\n"
"fragment float4 ui_msdf_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> msdf_atlas [[texture(0)]],\n"
"    sampler msdf_sampler [[sampler(0)]],\n"
"    constant Uniforms& uniforms [[buffer(0)]]\n"
") {\n"
"    float3 msd = msdf_atlas.sample(msdf_sampler, in.texcoord).rgb;\n"
"    float sd = median(msd.r, msd.g, msd.b);\n"
"    float screen_px_distance = uniforms.sdf_params.x * (sd - 0.5);\n"
"    float opacity = clamp(screen_px_distance * uniforms.sdf_params.y + 0.5, 0.0, 1.0);\n"
"    return float4(in.color.rgb, in.color.a * opacity);\n"
"}\n";

/* ============================================================================
 * GPU Pipeline Creation
 * ============================================================================ */

/* Create the graphics pipeline for UI rendering */
static bool aui_create_graphics_pipeline(AUI_Context *ctx)
{
    if (!ctx || !ctx->gpu) return false;

    /* Check what shader formats this device supports */
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(ctx->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Create vertex shader from MSL source */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = (const Uint8 *)ui_shader_msl;
        vs_info.code_size = sizeof(ui_shader_msl);
        vs_info.entrypoint = "ui_vertex";
        vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(ctx->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("CUI: Failed to create vertex shader");
            return false;
        }

        /* Create fragment shader from MSL source */
        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)ui_shader_msl;
        fs_info.code_size = sizeof(ui_shader_msl);
        fs_info.entrypoint = "ui_fragment";
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(ctx->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("CUI: Failed to create fragment shader");
            SDL_ReleaseGPUShader(ctx->gpu, vertex_shader);
            return false;
        }
    } else {
        agentite_set_error("CUI: No supported shader format (need MSL for Metal)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(AUI_Vertex, pos);

    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(AUI_Vertex, uv);

    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    attributes[2].offset = offsetof(AUI_Vertex, color);

    /* Define vertex buffer layout */
    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(AUI_Vertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    /* Define vertex input state */
    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 3;

    /* Define blend state for alpha blending */
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

    /* Define color target description */
    SDL_GPUColorTargetDescription color_target = {};
    color_target.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;  /* Swapchain format */
    color_target.blend_state = blend_state;

    /* Create the graphics pipeline */
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

    ctx->pipeline = SDL_CreateGPUGraphicsPipeline(ctx->gpu, &pipeline_info);

    /* Release shaders (pipeline holds references) */
    SDL_ReleaseGPUShader(ctx->gpu, vertex_shader);
    SDL_ReleaseGPUShader(ctx->gpu, fragment_shader);

    if (!ctx->pipeline) {
        agentite_set_error_from_sdl("CUI: Failed to create graphics pipeline");
        return false;
    }

    SDL_Log("CUI: Graphics pipeline created successfully");
    return true;
}

/* Create an SDF or MSDF pipeline */
static SDL_GPUGraphicsPipeline *aui_create_sdf_pipeline(AUI_Context *ctx, bool is_msdf)
{
    if (!ctx || !ctx->gpu) return NULL;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(ctx->gpu);
    if (!(formats & SDL_GPU_SHADERFORMAT_MSL)) {
        return NULL;
    }

    const char *shader_src = is_msdf ? ui_msdf_shader_msl : ui_sdf_shader_msl;
    size_t shader_size = is_msdf ? sizeof(ui_msdf_shader_msl) : sizeof(ui_sdf_shader_msl);
    const char *vs_entry = is_msdf ? "ui_msdf_vertex" : "ui_sdf_vertex";
    const char *fs_entry = is_msdf ? "ui_msdf_fragment" : "ui_sdf_fragment";

    /* Create vertex shader */
    SDL_GPUShaderCreateInfo vs_info = {};
    vs_info.code = (const Uint8 *)shader_src;
    vs_info.code_size = shader_size;
    vs_info.entrypoint = vs_entry;
    vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
    vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
    vs_info.num_samplers = 0;
    vs_info.num_storage_textures = 0;
    vs_info.num_storage_buffers = 0;
    vs_info.num_uniform_buffers = 1;
    SDL_GPUShader *vertex_shader = SDL_CreateGPUShader(ctx->gpu, &vs_info);
    if (!vertex_shader) {
        return NULL;
    }

    /* Create fragment shader */
    SDL_GPUShaderCreateInfo fs_info = {};
    fs_info.code = (const Uint8 *)shader_src;
    fs_info.code_size = shader_size;
    fs_info.entrypoint = fs_entry;
    fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
    fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fs_info.num_samplers = 1;
    fs_info.num_storage_textures = 0;
    fs_info.num_storage_buffers = 0;
    fs_info.num_uniform_buffers = 1;  /* SDF shaders need fragment uniforms */
    SDL_GPUShader *fragment_shader = SDL_CreateGPUShader(ctx->gpu, &fs_info);
    if (!fragment_shader) {
        SDL_ReleaseGPUShader(ctx->gpu, vertex_shader);
        return NULL;
    }

    /* Define vertex attributes (same as bitmap pipeline) */
    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(AUI_Vertex, pos);

    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(AUI_Vertex, uv);

    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_UINT;
    attributes[2].offset = offsetof(AUI_Vertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(AUI_Vertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 3;

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

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(ctx->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(ctx->gpu, vertex_shader);
    SDL_ReleaseGPUShader(ctx->gpu, fragment_shader);

    return pipeline;
}

/* Create 1x1 white texture for solid color primitives */
static bool aui_create_white_texture(AUI_Context *ctx)
{
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    tex_info.width = 1;
    tex_info.height = 1;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    ctx->white_texture = SDL_CreateGPUTexture(ctx->gpu, &tex_info);
    if (!ctx->white_texture) {
        agentite_set_error_from_sdl("CUI: Failed to create white texture");
        return false;
    }

    /* Upload white pixel */
    unsigned char white = 255;
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = 1;
    transfer_info.props = 0;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ctx->gpu, &transfer_info);
    if (transfer) {
        void *mapped = SDL_MapGPUTransferBuffer(ctx->gpu, transfer, false);
        if (mapped) {
            memcpy(mapped, &white, 1);
            SDL_UnmapGPUTransferBuffer(ctx->gpu, transfer);

            SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(ctx->gpu);
            if (cmd) {
                SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
                if (copy_pass) {
                    SDL_GPUTextureTransferInfo src = {};
                    src.transfer_buffer = transfer;
                    src.offset = 0;
                    src.pixels_per_row = 1;
                    src.rows_per_layer = 1;

                    SDL_GPUTextureRegion dst = {};
                    dst.texture = ctx->white_texture;
                    dst.x = 0;
                    dst.y = 0;
                    dst.z = 0;
                    dst.w = 1;
                    dst.h = 1;
                    dst.d = 1;

                    SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
                    SDL_EndGPUCopyPass(copy_pass);
                }
                SDL_SubmitGPUCommandBuffer(cmd);
            }
        }
        SDL_ReleaseGPUTransferBuffer(ctx->gpu, transfer);
    }

    return true;
}

bool aui_create_pipeline(AUI_Context *ctx)
{
    if (!ctx || !ctx->gpu) return false;

    /* Create vertex buffer (GPU side) */
    SDL_GPUBufferCreateInfo vb_info = {};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size = (Uint32)(ctx->vertex_capacity * sizeof(AUI_Vertex));
    vb_info.props = 0;
    ctx->vertex_buffer = SDL_CreateGPUBuffer(ctx->gpu, &vb_info);
    if (!ctx->vertex_buffer) {
        agentite_set_error_from_sdl("CUI: Failed to create vertex buffer");
        return false;
    }

    /* Create index buffer (GPU side) */
    SDL_GPUBufferCreateInfo ib_info = {};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size = (Uint32)(ctx->index_capacity * sizeof(uint16_t));
    ib_info.props = 0;
    ctx->index_buffer = SDL_CreateGPUBuffer(ctx->gpu, &ib_info);
    if (!ctx->index_buffer) {
        agentite_set_error_from_sdl("CUI: Failed to create index buffer");
        return false;
    }

    /* Create sampler for font atlas */
    SDL_GPUSamplerCreateInfo sampler_info = {};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    ctx->sampler = SDL_CreateGPUSampler(ctx->gpu, &sampler_info);
    if (!ctx->sampler) {
        agentite_set_error_from_sdl("CUI: Failed to create sampler");
        return false;
    }

    /* Create white texture for solid primitives */
    if (!aui_create_white_texture(ctx)) {
        return false;
    }

    /* Allocate draw command queue */
    ctx->draw_cmd_capacity = AUI_MAX_DRAW_CMDS;
    ctx->draw_cmds = (AUI_DrawCmd *)calloc(ctx->draw_cmd_capacity, sizeof(AUI_DrawCmd));
    if (!ctx->draw_cmds) {
        agentite_set_error("CUI: Failed to allocate draw command queue");
        return false;
    }

    /* Create graphics pipeline with shaders */
    if (!aui_create_graphics_pipeline(ctx)) {
        agentite_set_error("CUI: Failed to create graphics pipeline");
        return false;
    }

    /* Create SDF and MSDF pipelines (optional - created lazily if needed) */
    ctx->sdf_pipeline = aui_create_sdf_pipeline(ctx, false);
    ctx->msdf_pipeline = aui_create_sdf_pipeline(ctx, true);

    if (ctx->sdf_pipeline) {
        SDL_Log("CUI: SDF pipeline created successfully");
    }
    if (ctx->msdf_pipeline) {
        SDL_Log("CUI: MSDF pipeline created successfully");
    }

    SDL_Log("CUI: GPU resources created successfully");
    return true;
}

void aui_destroy_pipeline(AUI_Context *ctx)
{
    if (!ctx) return;

    if (ctx->pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(ctx->gpu, ctx->pipeline);
        ctx->pipeline = NULL;
    }
    if (ctx->sdf_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(ctx->gpu, ctx->sdf_pipeline);
        ctx->sdf_pipeline = NULL;
    }
    if (ctx->msdf_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(ctx->gpu, ctx->msdf_pipeline);
        ctx->msdf_pipeline = NULL;
    }
    if (ctx->vertex_buffer) {
        SDL_ReleaseGPUBuffer(ctx->gpu, ctx->vertex_buffer);
        ctx->vertex_buffer = NULL;
    }
    if (ctx->index_buffer) {
        SDL_ReleaseGPUBuffer(ctx->gpu, ctx->index_buffer);
        ctx->index_buffer = NULL;
    }
    if (ctx->sampler) {
        SDL_ReleaseGPUSampler(ctx->gpu, ctx->sampler);
        ctx->sampler = NULL;
    }
    if (ctx->white_texture) {
        SDL_ReleaseGPUTexture(ctx->gpu, ctx->white_texture);
        ctx->white_texture = NULL;
    }
    free(ctx->draw_cmds);
    ctx->draw_cmds = NULL;
}

/* ============================================================================
 * Draw Command Queue Management
 * ============================================================================ */

/* Flush current batch to draw command queue */
static void aui_flush_draw_cmd(AUI_Context *ctx)
{
    if (!ctx) return;

    uint32_t vertex_count = ctx->vertex_count - ctx->cmd_vertex_start;
    uint32_t index_count = ctx->index_count - ctx->cmd_index_start;

    /* Don't create empty commands */
    if (vertex_count == 0 || index_count == 0) return;

    /* Check capacity */
    if (ctx->draw_cmd_count >= ctx->draw_cmd_capacity) {
        SDL_Log("CUI: Draw command queue full");
        return;
    }

    /* Create draw command */
    AUI_DrawCmd *cmd = &ctx->draw_cmds[ctx->draw_cmd_count++];
    /* Detect solid primitives: white_texture or no texture means solid color */
    bool is_solid = !ctx->current_texture || (ctx->current_texture == ctx->white_texture);
    cmd->type = is_solid ? AUI_DRAW_CMD_SOLID : AUI_DRAW_CMD_BITMAP_TEXT;
    cmd->texture = ctx->current_texture ? ctx->current_texture : ctx->white_texture;
    cmd->layer = ctx->current_layer;
    cmd->vertex_offset = ctx->cmd_vertex_start;
    cmd->index_offset = ctx->cmd_index_start;
    cmd->vertex_count = vertex_count;
    cmd->index_count = index_count;
    cmd->sdf_scale = 1.0f;
    cmd->sdf_distance_range = 0.0f;

    /* Update start positions for next command */
    ctx->cmd_vertex_start = ctx->vertex_count;
    ctx->cmd_index_start = ctx->index_count;
}

/* Ensure we're batching with the correct texture */
static void aui_ensure_texture(AUI_Context *ctx, SDL_GPUTexture *texture)
{
    if (!ctx) return;

    /* If texture or layer changes, flush current batch */
    if (ctx->current_texture != texture) {
        aui_flush_draw_cmd(ctx);
        ctx->current_texture = texture;
    }
}

/* Reset draw state for new frame */
void aui_reset_draw_state(AUI_Context *ctx)
{
    if (!ctx) return;

    ctx->vertex_count = 0;
    ctx->index_count = 0;
    ctx->draw_cmd_count = 0;
    ctx->current_texture = NULL;
    ctx->current_layer = AUI_DEFAULT_LAYER;
    ctx->cmd_vertex_start = 0;
    ctx->cmd_index_start = 0;
    ctx->layer_stack_depth = 0;
}

/* ============================================================================
 * Layer System
 * ============================================================================ */

void aui_set_layer(AUI_Context *ctx, int layer)
{
    if (!ctx) return;

    if (ctx->current_layer != layer) {
        aui_flush_draw_cmd(ctx);
        ctx->current_layer = layer;
    }
}

int aui_get_layer(AUI_Context *ctx)
{
    return ctx ? ctx->current_layer : 0;
}

void aui_push_layer(AUI_Context *ctx, int layer)
{
    if (!ctx || ctx->layer_stack_depth >= 16) return;

    ctx->layer_stack[ctx->layer_stack_depth++] = ctx->current_layer;
    aui_set_layer(ctx, layer);
}

void aui_pop_layer(AUI_Context *ctx)
{
    if (!ctx || ctx->layer_stack_depth <= 0) return;

    int layer = ctx->layer_stack[--ctx->layer_stack_depth];
    aui_set_layer(ctx, layer);
}

/* Legacy channel API - maps to layer system */
void aui_draw_split_begin(AUI_Context *ctx, int channel_count)
{
    (void)ctx;
    (void)channel_count;
    /* No-op - layers are automatic now */
}

void aui_draw_set_channel(AUI_Context *ctx, int channel)
{
    aui_set_layer(ctx, channel);
}

void aui_draw_split_merge(AUI_Context *ctx)
{
    (void)ctx;
    /* No-op - sorting happens at render time */
}

/* ============================================================================
 * Vertex/Index Buffer Management
 * ============================================================================ */

/* Reserve space for vertices and indices, returns base vertex index */
static bool aui_reserve(AUI_Context *ctx, uint32_t vert_count, uint32_t idx_count,
                        uint32_t *out_vert_base)
{
    if (ctx->vertex_count + vert_count > ctx->vertex_capacity ||
        ctx->index_count + idx_count > ctx->index_capacity) {
        SDL_Log("CUI: Draw buffer overflow");
        return false;
    }

    *out_vert_base = ctx->vertex_count;
    ctx->vertex_count += vert_count;
    ctx->index_count += idx_count;
    return true;
}

/* Add a quad with specified texture */
static void aui_add_quad_textured(AUI_Context *ctx, SDL_GPUTexture *texture,
                                   float x0, float y0, float x1, float y1,
                                   float u0, float v0, float u1, float v1,
                                   uint32_t color)
{
    /* Ensure we're batching with this texture */
    aui_ensure_texture(ctx, texture);

    /* Apply scissor clipping if active */
    if (ctx->scissor_depth > 0) {
        AUI_Rect scissor = ctx->scissor_stack[ctx->scissor_depth - 1];
        float sx0 = scissor.x;
        float sy0 = scissor.y;
        float sx1 = scissor.x + scissor.w;
        float sy1 = scissor.y + scissor.h;

        /* Check if completely outside scissor */
        if (x1 <= sx0 || x0 >= sx1 || y1 <= sy0 || y0 >= sy1) {
            return;  /* Quad is fully clipped */
        }

        /* Clip quad to scissor rect and adjust UVs proportionally */
        float orig_w = x1 - x0;
        float orig_h = y1 - y0;
        float uv_w = u1 - u0;
        float uv_h = v1 - v0;

        if (x0 < sx0) {
            float t = (sx0 - x0) / orig_w;
            u0 += uv_w * t;
            x0 = sx0;
        }
        if (x1 > sx1) {
            float t = (x1 - sx1) / orig_w;
            u1 -= uv_w * t;
            x1 = sx1;
        }
        if (y0 < sy0) {
            float t = (sy0 - y0) / orig_h;
            v0 += uv_h * t;
            y0 = sy0;
        }
        if (y1 > sy1) {
            float t = (y1 - sy1) / orig_h;
            v1 -= uv_h * t;
            y1 = sy1;
        }

        /* Check if quad collapsed to zero */
        if (x1 <= x0 || y1 <= y0) {
            return;
        }
    }

    uint32_t base;
    if (!aui_reserve(ctx, 4, 6, &base)) return;

    AUI_Vertex *v = &ctx->vertices[base];
    uint16_t *i = &ctx->indices[ctx->index_count - 6];

    /* Vertices: top-left, top-right, bottom-right, bottom-left */
    v[0].pos[0] = x0; v[0].pos[1] = y0; v[0].uv[0] = u0; v[0].uv[1] = v0; v[0].color = color;
    v[1].pos[0] = x1; v[1].pos[1] = y0; v[1].uv[0] = u1; v[1].uv[1] = v0; v[1].color = color;
    v[2].pos[0] = x1; v[2].pos[1] = y1; v[2].uv[0] = u1; v[2].uv[1] = v1; v[2].color = color;
    v[3].pos[0] = x0; v[3].pos[1] = y1; v[3].uv[0] = u0; v[3].uv[1] = v1; v[3].color = color;

    /* Two triangles */
    i[0] = (uint16_t)base;
    i[1] = (uint16_t)(base + 1);
    i[2] = (uint16_t)(base + 2);
    i[3] = (uint16_t)base;
    i[4] = (uint16_t)(base + 2);
    i[5] = (uint16_t)(base + 3);
}

/* Add a quad using the white texture (for solid colors) */
static void aui_add_quad(AUI_Context *ctx,
                         float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1,
                         uint32_t color)
{
    /* Use white texture for solid primitives, or font atlas if legacy mode */
    SDL_GPUTexture *tex = ctx->white_texture ? ctx->white_texture : ctx->font_atlas;
    aui_add_quad_textured(ctx, tex, x0, y0, x1, y1, u0, v0, u1, v1, color);
}

/* ============================================================================
 * Drawing Primitives
 * ============================================================================ */

void aui_draw_rect(AUI_Context *ctx, float x, float y, float w, float h,
                   uint32_t color)
{
    if (!ctx || w <= 0 || h <= 0) return;

    /* UV at (0,0) for white texture gives solid white */
    aui_add_quad(ctx, x, y, x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f, color);
}

void aui_draw_rect_outline(AUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float thickness)
{
    if (!ctx || w <= 0 || h <= 0) return;

    float t = thickness;
    /* Top */
    aui_draw_rect(ctx, x, y, w, t, color);
    /* Bottom */
    aui_draw_rect(ctx, x, y + h - t, w, t, color);
    /* Left */
    aui_draw_rect(ctx, x, y + t, t, h - 2 * t, color);
    /* Right */
    aui_draw_rect(ctx, x + w - t, y + t, t, h - 2 * t, color);
}

void aui_draw_rect_rounded(AUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float radius)
{
    if (!ctx || w <= 0 || h <= 0) return;

    /* For now, just draw a regular rect (rounded corners need more vertices) */
    (void)radius;
    aui_draw_rect(ctx, x, y, w, h, color);
}

void aui_draw_line(AUI_Context *ctx, float x1, float y1, float x2, float y2,
                   uint32_t color, float thickness)
{
    if (!ctx) return;

    /* Ensure solid texture */
    SDL_GPUTexture *tex = ctx->white_texture ? ctx->white_texture : ctx->font_atlas;
    aui_ensure_texture(ctx, tex);

    /* Calculate perpendicular offset for line thickness */
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    uint32_t base;
    if (!aui_reserve(ctx, 4, 6, &base)) return;

    AUI_Vertex *v = &ctx->vertices[base];
    uint16_t *i = &ctx->indices[ctx->index_count - 6];

    v[0].pos[0] = x1 + nx; v[0].pos[1] = y1 + ny; v[0].uv[0] = 0; v[0].uv[1] = 0; v[0].color = color;
    v[1].pos[0] = x2 + nx; v[1].pos[1] = y2 + ny; v[1].uv[0] = 0; v[1].uv[1] = 0; v[1].color = color;
    v[2].pos[0] = x2 - nx; v[2].pos[1] = y2 - ny; v[2].uv[0] = 0; v[2].uv[1] = 0; v[2].color = color;
    v[3].pos[0] = x1 - nx; v[3].pos[1] = y1 - ny; v[3].uv[0] = 0; v[3].uv[1] = 0; v[3].color = color;

    i[0] = (uint16_t)base;
    i[1] = (uint16_t)(base + 1);
    i[2] = (uint16_t)(base + 2);
    i[3] = (uint16_t)base;
    i[4] = (uint16_t)(base + 2);
    i[5] = (uint16_t)(base + 3);
}

void aui_draw_triangle(AUI_Context *ctx,
                       float x0, float y0, float x1, float y1, float x2, float y2,
                       uint32_t color)
{
    if (!ctx) return;

    /* Ensure solid texture */
    SDL_GPUTexture *tex = ctx->white_texture ? ctx->white_texture : ctx->font_atlas;
    aui_ensure_texture(ctx, tex);

    uint32_t base;
    if (!aui_reserve(ctx, 3, 3, &base)) return;

    AUI_Vertex *v = &ctx->vertices[base];
    uint16_t *idx = &ctx->indices[ctx->index_count - 3];

    v[0].pos[0] = x0; v[0].pos[1] = y0; v[0].uv[0] = 0; v[0].uv[1] = 0; v[0].color = color;
    v[1].pos[0] = x1; v[1].pos[1] = y1; v[1].uv[0] = 0; v[1].uv[1] = 0; v[1].color = color;
    v[2].pos[0] = x2; v[2].pos[1] = y2; v[2].uv[0] = 0; v[2].uv[1] = 0; v[2].color = color;

    idx[0] = (uint16_t)base;
    idx[1] = (uint16_t)(base + 1);
    idx[2] = (uint16_t)(base + 2);
}

/* ============================================================================
 * Textured Quad (for text rendering with explicit texture)
 * ============================================================================ */

void aui_draw_textured_quad_ex(AUI_Context *ctx,
                                SDL_GPUTexture *texture,
                                float x0, float y0, float x1, float y1,
                                float u0, float v0, float u1, float v1,
                                uint32_t color)
{
    if (!ctx || !texture) return;
    aui_add_quad_textured(ctx, texture, x0, y0, x1, y1, u0, v0, u1, v1, color);
}

/* Legacy function - uses font_atlas */
void aui_draw_textured_quad(AUI_Context *ctx,
                            float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1,
                            uint32_t color)
{
    if (!ctx) return;
    SDL_GPUTexture *tex = ctx->font_atlas ? ctx->font_atlas : ctx->white_texture;
    aui_add_quad_textured(ctx, tex, x0, y0, x1, y1, u0, v0, u1, v1, color);
}

/* ============================================================================
 * SDF Text Drawing
 * ============================================================================ */

/* Forward declarations of static functions used here */
static void aui_flush_draw_cmd(AUI_Context *ctx);
static bool aui_reserve(AUI_Context *ctx, uint32_t vert_count, uint32_t idx_count,
                        uint32_t *out_vert_base);

/* We need access to AUI_Font internals for SDF rendering */
struct AUI_Font;  /* Forward declaration */

/* Get font type - defined in ui_text.cpp */
extern AUI_FontType aui_font_get_type(AUI_Font *font);

/* Internal structure access - we need the sdf_font pointer */
/* This is a bit of a hack but avoids exposing Agentite_SDFFont in the public header */
typedef struct AUI_FontInternal {
    AUI_FontType type;
    void *bitmap_font;
    void *sdf_font;  /* Actually Agentite_SDFFont* */
} AUI_FontInternal;

/* Include internal header for Agentite_SDFFont access */
#include "../graphics/text_internal.h"

void aui_draw_sdf_quad(AUI_Context *ctx, AUI_Font *font,
                        float x0, float y0, float x1, float y1,
                        float u0, float v0, float u1, float v1,
                        uint32_t color, float scale)
{
    if (!ctx || !font) return;

    /* Get font internals */
    AUI_FontInternal *fi = (AUI_FontInternal *)font;
    if (fi->type != AUI_FONT_SDF && fi->type != AUI_FONT_MSDF) return;

    Agentite_SDFFont *sdf_font = (Agentite_SDFFont *)fi->sdf_font;
    if (!sdf_font || !sdf_font->atlas_texture) return;

    /* Flush current batch if texture or type changes */
    bool is_msdf = (fi->type == AUI_FONT_MSDF);
    AUI_DrawCmdType needed_type = is_msdf ? AUI_DRAW_CMD_MSDF_TEXT : AUI_DRAW_CMD_SDF_TEXT;

    /* Check if we need to start a new batch */
    if (ctx->current_texture != sdf_font->atlas_texture ||
        (ctx->draw_cmd_count > 0 &&
         ctx->draw_cmds[ctx->draw_cmd_count - 1].type != needed_type)) {
        /* Flush current batch */
        aui_flush_draw_cmd(ctx);
        ctx->current_texture = sdf_font->atlas_texture;
    }

    /* Apply scissor clipping if active */
    if (ctx->scissor_depth > 0) {
        AUI_Rect scissor = ctx->scissor_stack[ctx->scissor_depth - 1];
        float sx0 = scissor.x;
        float sy0 = scissor.y;
        float sx1 = scissor.x + scissor.w;
        float sy1 = scissor.y + scissor.h;

        if (x1 <= sx0 || x0 >= sx1 || y1 <= sy0 || y0 >= sy1) {
            return;  /* Fully clipped */
        }

        /* Clip and adjust UVs */
        float orig_w = x1 - x0;
        float orig_h = y1 - y0;
        float uv_w = u1 - u0;
        float uv_h = v1 - v0;

        if (x0 < sx0) { float t = (sx0 - x0) / orig_w; u0 += uv_w * t; x0 = sx0; }
        if (x1 > sx1) { float t = (x1 - sx1) / orig_w; u1 -= uv_w * t; x1 = sx1; }
        if (y0 < sy0) { float t = (sy0 - y0) / orig_h; v0 += uv_h * t; y0 = sy0; }
        if (y1 > sy1) { float t = (y1 - sy1) / orig_h; v1 -= uv_h * t; y1 = sy1; }

        if (x1 <= x0 || y1 <= y0) return;
    }

    /* Reserve vertices and indices */
    uint32_t base;
    if (!aui_reserve(ctx, 4, 6, &base)) return;

    AUI_Vertex *v = &ctx->vertices[base];
    uint16_t *i = &ctx->indices[ctx->index_count - 6];

    v[0].pos[0] = x0; v[0].pos[1] = y0; v[0].uv[0] = u0; v[0].uv[1] = v0; v[0].color = color;
    v[1].pos[0] = x1; v[1].pos[1] = y0; v[1].uv[0] = u1; v[1].uv[1] = v0; v[1].color = color;
    v[2].pos[0] = x1; v[2].pos[1] = y1; v[2].uv[0] = u1; v[2].uv[1] = v1; v[2].color = color;
    v[3].pos[0] = x0; v[3].pos[1] = y1; v[3].uv[0] = u0; v[3].uv[1] = v1; v[3].color = color;

    i[0] = (uint16_t)base;
    i[1] = (uint16_t)(base + 1);
    i[2] = (uint16_t)(base + 2);
    i[3] = (uint16_t)base;
    i[4] = (uint16_t)(base + 2);
    i[5] = (uint16_t)(base + 3);

    /* Store SDF parameters in the current command (will be finalized in flush) */
    /* We need to track this for the batch - store in context temporarily */
    /* The flush function will pick up these values */
    ctx->draw_cmds[ctx->draw_cmd_count].sdf_scale = scale;
    ctx->draw_cmds[ctx->draw_cmd_count].sdf_distance_range = sdf_font->distance_range;
    ctx->draw_cmds[ctx->draw_cmd_count].type = needed_type;
}

/* ============================================================================
 * Scissor Stack
 * ============================================================================ */

void aui_push_scissor(AUI_Context *ctx, float x, float y, float w, float h)
{
    if (!ctx || ctx->scissor_depth >= 16) return;

    AUI_Rect rect = {x, y, w, h};

    /* Intersect with current scissor if any */
    if (ctx->scissor_depth > 0) {
        rect = aui_rect_intersect(rect, ctx->scissor_stack[ctx->scissor_depth - 1]);
    }

    ctx->scissor_stack[ctx->scissor_depth++] = rect;
}

void aui_pop_scissor(AUI_Context *ctx)
{
    if (!ctx || ctx->scissor_depth <= 0) return;
    ctx->scissor_depth--;
}

/* ============================================================================
 * Draw Command Sorting (for layer ordering)
 * ============================================================================ */

static int aui_draw_cmd_compare(const void *a, const void *b)
{
    const AUI_DrawCmd *cmd_a = (const AUI_DrawCmd *)a;
    const AUI_DrawCmd *cmd_b = (const AUI_DrawCmd *)b;

    /* Sort by layer first (lower layers render first) */
    if (cmd_a->layer != cmd_b->layer) {
        return cmd_a->layer - cmd_b->layer;
    }

    /* Within same layer, preserve original draw order to maintain correct
     * front-to-back ordering (painter's algorithm). Texture batching would
     * break this ordering and cause primitives to overdraw incorrectly. */
    if (cmd_a->vertex_offset < cmd_b->vertex_offset) return -1;
    if (cmd_a->vertex_offset > cmd_b->vertex_offset) return 1;
    return 0;
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

/* Upload UI vertex/index data to GPU - call BEFORE render pass */
void aui_upload(AUI_Context *ctx, SDL_GPUCommandBuffer *cmd)
{
    if (!ctx || !cmd) return;

    /* Flush any pending draw command */
    aui_flush_draw_cmd(ctx);

    if (ctx->vertex_count == 0 || ctx->index_count == 0) return;

    /* Upload vertex data to GPU */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = (Uint32)(ctx->vertex_count * sizeof(AUI_Vertex) +
            ctx->index_count * sizeof(uint16_t));
    transfer_info.props = 0;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ctx->gpu, &transfer_info);
    if (!transfer) {
        agentite_set_error_from_sdl("CUI: Failed to create transfer buffer");
        return;
    }

    /* Map and copy data */
    void *mapped = SDL_MapGPUTransferBuffer(ctx->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, ctx->vertices, ctx->vertex_count * sizeof(AUI_Vertex));
        memcpy((uint8_t *)mapped + ctx->vertex_count * sizeof(AUI_Vertex),
               ctx->indices, ctx->index_count * sizeof(uint16_t));
        SDL_UnmapGPUTransferBuffer(ctx->gpu, transfer);
    }

    /* Copy from transfer buffer to GPU buffers */
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        SDL_GPUTransferBufferLocation src_vert = {};
        src_vert.transfer_buffer = transfer;
        src_vert.offset = 0;

        SDL_GPUBufferRegion dst_vert = {};
        dst_vert.buffer = ctx->vertex_buffer;
        dst_vert.offset = 0;
        dst_vert.size = (Uint32)(ctx->vertex_count * sizeof(AUI_Vertex));

        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        SDL_GPUTransferBufferLocation src_idx = {};
        src_idx.transfer_buffer = transfer;
        src_idx.offset = (Uint32)(ctx->vertex_count * sizeof(AUI_Vertex));

        SDL_GPUBufferRegion dst_idx = {};
        dst_idx.buffer = ctx->index_buffer;
        dst_idx.offset = 0;
        dst_idx.size = (Uint32)(ctx->index_count * sizeof(uint16_t));

        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(ctx->gpu, transfer);
}

void aui_render(AUI_Context *ctx, SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass)
{
    if (!ctx || !cmd || !pass) return;
    if (ctx->draw_cmd_count == 0) return;
    if (!ctx->pipeline) {
        return;
    }

    /* Sort draw commands by layer, then by type, then by texture */
    qsort(ctx->draw_cmds, ctx->draw_cmd_count, sizeof(AUI_DrawCmd), aui_draw_cmd_compare);

    /* Bind vertex buffer (shared by all pipelines) */
    SDL_GPUBufferBinding vb_binding = {};
    vb_binding.buffer = ctx->vertex_buffer;
    vb_binding.offset = 0;
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer (shared by all pipelines) */
    SDL_GPUBufferBinding ib_binding = {};
    ib_binding.buffer = ctx->index_buffer;
    ib_binding.offset = 0;
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Track current state */
    SDL_GPUGraphicsPipeline *current_pipeline = NULL;
    SDL_GPUTexture *current_texture = NULL;
    AUI_DrawCmdType current_type = AUI_DRAW_CMD_SOLID;

    /* Render each draw command */
    for (uint32_t i = 0; i < ctx->draw_cmd_count; i++) {
        AUI_DrawCmd *draw_cmd = &ctx->draw_cmds[i];

        /* Select pipeline based on command type */
        SDL_GPUGraphicsPipeline *pipeline;
        switch (draw_cmd->type) {
            case AUI_DRAW_CMD_SDF_TEXT:
                pipeline = ctx->sdf_pipeline;
                break;
            case AUI_DRAW_CMD_MSDF_TEXT:
                pipeline = ctx->msdf_pipeline;
                break;
            case AUI_DRAW_CMD_SOLID:
            case AUI_DRAW_CMD_BITMAP_TEXT:
            default:
                pipeline = ctx->pipeline;
                break;
        }

        if (!pipeline) {
            pipeline = ctx->pipeline;  /* Fallback to bitmap pipeline */
        }

        /* Bind pipeline if changed */
        if (pipeline != current_pipeline) {
            SDL_BindGPUGraphicsPipeline(pass, pipeline);
            current_pipeline = pipeline;
        }

        /* Push uniforms */
        float uniforms[4] = {
            (float)ctx->width,
            (float)ctx->height,
            draw_cmd->sdf_distance_range,  /* For SDF shaders */
            draw_cmd->sdf_scale            /* For SDF shaders */
        };
        SDL_PushGPUVertexUniformData(cmd, 0, uniforms, sizeof(uniforms));

        /* Bind texture if changed */
        if (draw_cmd->texture != current_texture) {
            SDL_GPUTextureSamplerBinding tex_binding = {};
            tex_binding.texture = draw_cmd->texture;
            tex_binding.sampler = ctx->sampler;
            SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);
            current_texture = draw_cmd->texture;
        }

        /* Draw this command */
        if (draw_cmd->index_count > 0 && draw_cmd->texture) {
            SDL_DrawGPUIndexedPrimitives(pass, draw_cmd->index_count, 1,
                                          draw_cmd->index_offset, 0, 0);
        }

        current_type = draw_cmd->type;
    }
}

/* ============================================================================
 * Bezier Curve Drawing
 * ============================================================================ */

/* De Casteljau's algorithm to evaluate a cubic bezier at parameter t */
static void aui_bezier_cubic_point(float x1, float y1,
                                    float cx1, float cy1,
                                    float cx2, float cy2,
                                    float x2, float y2,
                                    float t, float *out_x, float *out_y)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;
    float uuu = uu * u;
    float ttt = tt * t;

    *out_x = uuu * x1 + 3.0f * uu * t * cx1 + 3.0f * u * tt * cx2 + ttt * x2;
    *out_y = uuu * y1 + 3.0f * uu * t * cy1 + 3.0f * u * tt * cy2 + ttt * y2;
}

/* De Casteljau's algorithm to evaluate a quadratic bezier at parameter t */
static void aui_bezier_quadratic_point(float x1, float y1,
                                        float cx, float cy,
                                        float x2, float y2,
                                        float t, float *out_x, float *out_y)
{
    float u = 1.0f - t;
    float tt = t * t;
    float uu = u * u;

    *out_x = uu * x1 + 2.0f * u * t * cx + tt * x2;
    *out_y = uu * y1 + 2.0f * u * t * cy + tt * y2;
}

/* Calculate the number of segments needed based on curve length/curvature */
static int aui_bezier_segment_count(float x1, float y1, float x2, float y2,
                                     float cx1, float cy1, float cx2, float cy2)
{
    /* Estimate curve length using control polygon */
    float d1 = sqrtf((cx1 - x1) * (cx1 - x1) + (cy1 - y1) * (cy1 - y1));
    float d2 = sqrtf((cx2 - cx1) * (cx2 - cx1) + (cy2 - cy1) * (cy2 - cy1));
    float d3 = sqrtf((x2 - cx2) * (x2 - cx2) + (y2 - cy2) * (y2 - cy2));
    float length = d1 + d2 + d3;

    /* Use ~1 segment per 10 pixels, with min 4 and max 64 */
    int segments = (int)(length / 10.0f);
    if (segments < 4) segments = 4;
    if (segments > 64) segments = 64;
    return segments;
}

void aui_draw_bezier_cubic(AUI_Context *ctx,
                           float x1, float y1,
                           float cx1, float cy1,
                           float cx2, float cy2,
                           float x2, float y2,
                           uint32_t color, float thickness)
{
    if (!ctx) return;

    int segments = aui_bezier_segment_count(x1, y1, x2, y2, cx1, cy1, cx2, cy2);
    float step = 1.0f / (float)segments;

    float prev_x = x1;
    float prev_y = y1;

    for (int i = 1; i <= segments; i++) {
        float t = step * (float)i;
        float curr_x, curr_y;
        aui_bezier_cubic_point(x1, y1, cx1, cy1, cx2, cy2, x2, y2, t, &curr_x, &curr_y);

        aui_draw_line(ctx, prev_x, prev_y, curr_x, curr_y, color, thickness);

        prev_x = curr_x;
        prev_y = curr_y;
    }
}

void aui_draw_bezier_quadratic(AUI_Context *ctx,
                               float x1, float y1,
                               float cx, float cy,
                               float x2, float y2,
                               uint32_t color, float thickness)
{
    if (!ctx) return;

    /* Estimate segment count for quadratic */
    float d1 = sqrtf((cx - x1) * (cx - x1) + (cy - y1) * (cy - y1));
    float d2 = sqrtf((x2 - cx) * (x2 - cx) + (y2 - cy) * (y2 - cy));
    float length = d1 + d2;

    int segments = (int)(length / 10.0f);
    if (segments < 4) segments = 4;
    if (segments > 64) segments = 64;

    float step = 1.0f / (float)segments;

    float prev_x = x1;
    float prev_y = y1;

    for (int i = 1; i <= segments; i++) {
        float t = step * (float)i;
        float curr_x, curr_y;
        aui_bezier_quadratic_point(x1, y1, cx, cy, x2, y2, t, &curr_x, &curr_y);

        aui_draw_line(ctx, prev_x, prev_y, curr_x, curr_y, color, thickness);

        prev_x = curr_x;
        prev_y = curr_y;
    }
}

/* ============================================================================
 * Path API
 * ============================================================================ */

#define AUI_PATH_INITIAL_CAPACITY 64

static void aui_path_ensure_capacity(AUI_Context *ctx, uint32_t needed)
{
    if (ctx->path_capacity >= needed) return;

    uint32_t new_capacity = ctx->path_capacity ? ctx->path_capacity * 2 : AUI_PATH_INITIAL_CAPACITY;
    while (new_capacity < needed) new_capacity *= 2;

    float *new_points = (float *)realloc(ctx->path_points, new_capacity * 2 * sizeof(float));
    if (new_points) {
        ctx->path_points = new_points;
        ctx->path_capacity = new_capacity;
    }
}

void aui_path_begin(AUI_Context *ctx)
{
    if (!ctx) return;
    ctx->path_count = 0;
}

static void aui_path_add_point(AUI_Context *ctx, float x, float y)
{
    aui_path_ensure_capacity(ctx, ctx->path_count + 1);
    if (ctx->path_count < ctx->path_capacity) {
        ctx->path_points[ctx->path_count * 2] = x;
        ctx->path_points[ctx->path_count * 2 + 1] = y;
        ctx->path_count++;
    }
}

void aui_path_line_to(AUI_Context *ctx, float x, float y)
{
    if (!ctx) return;
    aui_path_add_point(ctx, x, y);
}

void aui_path_bezier_cubic_to(AUI_Context *ctx, float cx1, float cy1,
                               float cx2, float cy2, float x, float y)
{
    if (!ctx || ctx->path_count == 0) return;

    /* Get current point */
    float x1 = ctx->path_points[(ctx->path_count - 1) * 2];
    float y1 = ctx->path_points[(ctx->path_count - 1) * 2 + 1];

    /* Tessellate the bezier curve */
    int segments = aui_bezier_segment_count(x1, y1, x, y, cx1, cy1, cx2, cy2);
    float step = 1.0f / (float)segments;

    for (int i = 1; i <= segments; i++) {
        float t = step * (float)i;
        float px, py;
        aui_bezier_cubic_point(x1, y1, cx1, cy1, cx2, cy2, x, y, t, &px, &py);
        aui_path_add_point(ctx, px, py);
    }
}

void aui_path_bezier_quadratic_to(AUI_Context *ctx, float cx, float cy,
                                   float x, float y)
{
    if (!ctx || ctx->path_count == 0) return;

    /* Get current point */
    float x1 = ctx->path_points[(ctx->path_count - 1) * 2];
    float y1 = ctx->path_points[(ctx->path_count - 1) * 2 + 1];

    /* Estimate segment count */
    float d1 = sqrtf((cx - x1) * (cx - x1) + (cy - y1) * (cy - y1));
    float d2 = sqrtf((x - cx) * (x - cx) + (y - cy) * (y - cy));
    float length = d1 + d2;

    int segments = (int)(length / 10.0f);
    if (segments < 4) segments = 4;
    if (segments > 64) segments = 64;

    float step = 1.0f / (float)segments;

    for (int i = 1; i <= segments; i++) {
        float t = step * (float)i;
        float px, py;
        aui_bezier_quadratic_point(x1, y1, cx, cy, x, y, t, &px, &py);
        aui_path_add_point(ctx, px, py);
    }
}

void aui_path_stroke(AUI_Context *ctx, uint32_t color, float thickness)
{
    if (!ctx || ctx->path_count < 2) return;

    for (uint32_t i = 0; i < ctx->path_count - 1; i++) {
        float x1 = ctx->path_points[i * 2];
        float y1 = ctx->path_points[i * 2 + 1];
        float x2 = ctx->path_points[(i + 1) * 2];
        float y2 = ctx->path_points[(i + 1) * 2 + 1];

        aui_draw_line(ctx, x1, y1, x2, y2, color, thickness);
    }

    ctx->path_count = 0;
}

void aui_path_fill(AUI_Context *ctx, uint32_t color)
{
    if (!ctx || ctx->path_count < 3) return;

    /* Simple fan triangulation from first vertex */
    float x0 = ctx->path_points[0];
    float y0 = ctx->path_points[1];

    for (uint32_t i = 1; i < ctx->path_count - 1; i++) {
        float x1 = ctx->path_points[i * 2];
        float y1 = ctx->path_points[i * 2 + 1];
        float x2 = ctx->path_points[(i + 1) * 2];
        float y2 = ctx->path_points[(i + 1) * 2 + 1];

        aui_draw_triangle(ctx, x0, y0, x1, y1, x2, y2, color);
    }

    ctx->path_count = 0;
}
