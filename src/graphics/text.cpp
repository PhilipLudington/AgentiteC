/*
 * Carbon Text Rendering System - Core Implementation
 *
 * Contains renderer lifecycle, GPU pipelines, and shared utilities.
 * Font loading is in text_font.cpp, rendering in text_render.cpp,
 * and SDF support in text_sdf.cpp.
 */

#include "text_internal.h"

/* Embedded SPIRV shaders for Vulkan/D3D12 (cross-platform) */
#include "text_shaders_spirv.h"

/* ============================================================================
 * Embedded MSL Shader Source (same as sprite shader but uses single channel)
 * ============================================================================ */

static const char text_shader_msl[] =
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
"vertex VertexOut text_vertex(\n"
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
"fragment float4 text_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> font_texture [[texture(0)]],\n"
"    sampler font_sampler [[sampler(0)]]\n"
") {\n"
"    float alpha = font_texture.sample(font_sampler, in.texcoord).r;\n"
"    return float4(in.color.rgb, in.color.a * alpha);\n"
"}\n";

/* ============================================================================
 * Embedded MSL Shader Source for SDF Text
 * ============================================================================ */

static const char sdf_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float4x4 view_projection;\n"
"    float2 screen_size;\n"
"    float2 padding;\n"
"};\n"
"\n"
"struct SDFUniforms {\n"
"    float4 params;          // distance_range, scale, weight, edge_threshold\n"
"    float4 outline_params;  // outline_width, pad, pad, pad\n"
"    float4 outline_color;   // RGBA\n"
"    float4 glow_params;     // glow_width, pad, pad, pad\n"
"    float4 glow_color;      // RGBA\n"
"    float4 shadow_params;   // shadow_offset_x, shadow_offset_y, shadow_softness, pad\n"
"    float4 shadow_color;    // RGBA\n"
"    uint flags;\n"
"    float3 _padding;\n"
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
"vertex VertexOut sdf_vertex(\n"
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
"fragment float4 sdf_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> font_texture [[texture(0)]],\n"
"    sampler font_sampler [[sampler(0)]],\n"
"    constant SDFUniforms& sdf [[buffer(0)]]\n"
") {\n"
"    float dist = font_texture.sample(font_sampler, in.texcoord).r;\n"
"\n"
"    // Extract parameters from packed float4s\n"
"    float distance_range = sdf.params.x;\n"
"    float scale = sdf.params.y;\n"
"    float weight = sdf.params.z;\n"
"    float edge_threshold = sdf.params.w;\n"
"    float outline_width = sdf.outline_params.x;\n"
"    float glow_width = sdf.glow_params.x;\n"
"    float2 shadow_offset = sdf.shadow_params.xy;\n"
"    float shadow_softness = sdf.shadow_params.z;\n"
"\n"
"    // Screen-space anti-aliasing\n"
"    float2 dxdy = fwidth(in.texcoord);\n"
"    float px_range = distance_range * scale / max(dxdy.x, dxdy.y);\n"
"    px_range = max(px_range, 1.0);\n"
"\n"
"    float edge = edge_threshold - weight;\n"
"    float aa = 0.5 / px_range;\n"
"    float alpha = smoothstep(edge - aa, edge + aa, dist);\n"
"\n"
"    float4 result = float4(in.color.rgb, in.color.a * alpha);\n"
"\n"
"    // Outline (behind text) - flag bit 0\n"
"    if ((sdf.flags & 1u) != 0u) {\n"
"        float outline_edge = edge - outline_width;\n"
"        float outline_alpha = smoothstep(outline_edge - aa, outline_edge + aa, dist);\n"
"        outline_alpha = outline_alpha * (1.0 - alpha) * sdf.outline_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.outline_color.rgb, result.rgb, result.a),\n"
"            max(result.a, outline_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    // Glow (behind outline) - flag bit 1\n"
"    if ((sdf.flags & 2u) != 0u) {\n"
"        float glow_edge = edge - glow_width;\n"
"        // Softer falloff for smoother glow\n"
"        float glow_aa = aa * (2.0 + glow_width * 4.0);\n"
"        float glow_alpha = smoothstep(glow_edge - glow_aa, edge, dist);\n"
"        glow_alpha = glow_alpha * (1.0 - result.a) * sdf.glow_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.glow_color.rgb, result.rgb, result.a),\n"
"            max(result.a, glow_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    // Shadow (behind everything) - flag bit 2\n"
"    if ((sdf.flags & 4u) != 0u) {\n"
"        // Sample SDF at offset position for shadow\n"
"        float2 shadow_uv = in.texcoord - shadow_offset * dxdy;\n"
"        float shadow_dist = font_texture.sample(font_sampler, shadow_uv).r;\n"
"        // Softer edge for shadow based on softness parameter\n"
"        float shadow_aa = aa * (1.0 + shadow_softness * 4.0);\n"
"        float shadow_alpha = smoothstep(edge - shadow_aa, edge + shadow_aa, shadow_dist);\n"
"        shadow_alpha = shadow_alpha * (1.0 - result.a) * sdf.shadow_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.shadow_color.rgb, result.rgb, result.a),\n"
"            max(result.a, shadow_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    return result;\n"
"}\n";

/* ============================================================================
 * Embedded MSL Shader Source for MSDF Text
 * ============================================================================ */

static const char msdf_shader_msl[] =
"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct Uniforms {\n"
"    float4x4 view_projection;\n"
"    float2 screen_size;\n"
"    float2 padding;\n"
"};\n"
"\n"
"struct SDFUniforms {\n"
"    float4 params;          // distance_range, scale, weight, edge_threshold\n"
"    float4 outline_params;  // outline_width, pad, pad, pad\n"
"    float4 outline_color;   // RGBA\n"
"    float4 glow_params;     // glow_width, pad, pad, pad\n"
"    float4 glow_color;      // RGBA\n"
"    float4 shadow_params;   // shadow_offset_x, shadow_offset_y, shadow_softness, pad\n"
"    float4 shadow_color;    // RGBA\n"
"    uint flags;\n"
"    float3 _padding;\n"
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
"// Median of three values for MSDF (named to avoid Metal's median3 builtin)\n"
"float msdf_median(float r, float g, float b) {\n"
"    return max(min(r, g), min(max(r, g), b));\n"
"}\n"
"\n"
"vertex VertexOut msdf_vertex(\n"
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
"fragment float4 msdf_fragment(\n"
"    VertexOut in [[stage_in]],\n"
"    texture2d<float> font_texture [[texture(0)]],\n"
"    sampler font_sampler [[sampler(0)]],\n"
"    constant SDFUniforms& sdf [[buffer(0)]]\n"
") {\n"
"    float3 msd = font_texture.sample(font_sampler, in.texcoord).rgb;\n"
"    float dist = msdf_median(msd.r, msd.g, msd.b);\n"
"\n"
"    // Extract parameters from packed float4s\n"
"    float distance_range = sdf.params.x;\n"
"    float scale = sdf.params.y;\n"
"    float weight = sdf.params.z;\n"
"    float edge_threshold = sdf.params.w;\n"
"    float outline_width = sdf.outline_params.x;\n"
"    float glow_width = sdf.glow_params.x;\n"
"    float2 shadow_offset = sdf.shadow_params.xy;\n"
"    float shadow_softness = sdf.shadow_params.z;\n"
"\n"
"    // Screen-space anti-aliasing\n"
"    float2 dxdy = fwidth(in.texcoord);\n"
"    float px_range = distance_range * scale / max(dxdy.x, dxdy.y);\n"
"    px_range = max(px_range, 1.0);\n"
"\n"
"    float edge = edge_threshold - weight;\n"
"    float aa = 0.5 / px_range;\n"
"    float alpha = smoothstep(edge - aa, edge + aa, dist);\n"
"\n"
"    float4 result = float4(in.color.rgb, in.color.a * alpha);\n"
"\n"
"    // Outline (behind text) - flag bit 0\n"
"    if ((sdf.flags & 1u) != 0u) {\n"
"        float outline_edge = edge - outline_width;\n"
"        float outline_alpha = smoothstep(outline_edge - aa, outline_edge + aa, dist);\n"
"        outline_alpha = outline_alpha * (1.0 - alpha) * sdf.outline_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.outline_color.rgb, result.rgb, result.a),\n"
"            max(result.a, outline_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    // Glow (behind outline) - flag bit 1\n"
"    if ((sdf.flags & 2u) != 0u) {\n"
"        float glow_edge = edge - glow_width;\n"
"        // Softer falloff for smoother glow\n"
"        float glow_aa = aa * (2.0 + glow_width * 4.0);\n"
"        float glow_alpha = smoothstep(glow_edge - glow_aa, edge, dist);\n"
"        glow_alpha = glow_alpha * (1.0 - result.a) * sdf.glow_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.glow_color.rgb, result.rgb, result.a),\n"
"            max(result.a, glow_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    // Shadow (behind everything) - flag bit 2\n"
"    if ((sdf.flags & 4u) != 0u) {\n"
"        // Sample MSDF at offset position for shadow\n"
"        float2 shadow_uv = in.texcoord - shadow_offset * dxdy;\n"
"        float3 shadow_msd = font_texture.sample(font_sampler, shadow_uv).rgb;\n"
"        float shadow_dist = msdf_median(shadow_msd.r, shadow_msd.g, shadow_msd.b);\n"
"        // Softer edge for shadow based on softness parameter\n"
"        float shadow_aa = aa * (1.0 + shadow_softness * 4.0);\n"
"        float shadow_alpha = smoothstep(edge - shadow_aa, edge + shadow_aa, shadow_dist);\n"
"        shadow_alpha = shadow_alpha * (1.0 - result.a) * sdf.shadow_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.shadow_color.rgb, result.rgb, result.a),\n"
"            max(result.a, shadow_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    return result;\n"
"}\n";

/* ============================================================================
 * Internal: Pipeline Creation
 * ============================================================================ */

static bool text_create_pipeline(Agentite_TextRenderer *tr)
{
    if (!tr || !tr->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(tr->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Create vertex shader (Metal) */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = (const Uint8 *)text_shader_msl;
        vs_info.code_size = sizeof(text_shader_msl);
        vs_info.entrypoint = "text_vertex";
        vs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(tr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("Text: Failed to create vertex shader");
            return false;
        }

        /* Create fragment shader (Metal) */
        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)text_shader_msl;
        fs_info.code_size = sizeof(text_shader_msl);
        fs_info.entrypoint = "text_fragment";
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(tr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("Text: Failed to create fragment shader");
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        /* Create vertex shader (Vulkan/D3D12 via SPIRV) */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = text_vert_spv;
        vs_info.code_size = text_vert_spv_len;
        vs_info.entrypoint = "main";
        vs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(tr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error_from_sdl("Text: Failed to create SPIRV vertex shader");
            return false;
        }

        /* Create fragment shader (Vulkan/D3D12 via SPIRV) */
        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = text_frag_spv;
        fs_info.code_size = text_frag_spv_len;
        fs_info.entrypoint = "main";
        fs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 0;
        fragment_shader = SDL_CreateGPUShader(tr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error_from_sdl("Text: Failed to create SPIRV fragment shader");
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else {
        agentite_set_error("Text: No supported shader format (need MSL or SPIRV)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(TextVertex, pos);

    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(TextVertex, uv);

    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[2].offset = offsetof(TextVertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(TextVertex);
    vb_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;
    vb_desc.instance_step_rate = 0;

    SDL_GPUVertexInputState vertex_input = {};
    vertex_input.vertex_buffer_descriptions = &vb_desc;
    vertex_input.num_vertex_buffers = 1;
    vertex_input.vertex_attributes = attributes;
    vertex_input.num_vertex_attributes = 3;

    /* Alpha blending for text */
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

    tr->pipeline = SDL_CreateGPUGraphicsPipeline(tr->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
    SDL_ReleaseGPUShader(tr->gpu, fragment_shader);

    if (!tr->pipeline) {
        agentite_set_error_from_sdl("Text: Failed to create graphics pipeline");
        return false;
    }

    SDL_Log("Text: Graphics pipeline created successfully");
    return true;
}

/* ============================================================================
 * Internal: SDF/MSDF Pipeline Creation
 * ============================================================================ */

static bool sdf_create_pipeline(Agentite_TextRenderer *tr, bool is_msdf)
{
    if (!tr || !tr->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(tr->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    const char *shader_src = is_msdf ? msdf_shader_msl : sdf_shader_msl;
    size_t shader_size = is_msdf ? sizeof(msdf_shader_msl) : sizeof(sdf_shader_msl);
    const char *vs_entry = is_msdf ? "msdf_vertex" : "sdf_vertex";
    const char *fs_entry = is_msdf ? "msdf_fragment" : "sdf_fragment";

    /* SPIRV fragment shader selection */
    const unsigned char *spirv_fs = is_msdf ? msdf_frag_spv : sdf_frag_spv;
    unsigned int spirv_fs_len = is_msdf ? msdf_frag_spv_len : sdf_frag_spv_len;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
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
        vertex_shader = SDL_CreateGPUShader(tr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error("Text: Failed to create %s vertex shader: %s",
                    is_msdf ? "MSDF" : "SDF", SDL_GetError());
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = (const Uint8 *)shader_src;
        fs_info.code_size = shader_size;
        fs_info.entrypoint = fs_entry;
        fs_info.format = SDL_GPU_SHADERFORMAT_MSL;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 1;
        fragment_shader = SDL_CreateGPUShader(tr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error("Text: Failed to create %s fragment shader: %s",
                    is_msdf ? "MSDF" : "SDF", SDL_GetError());
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else if (formats & SDL_GPU_SHADERFORMAT_SPIRV) {
        /* Use text vertex shader (same for SDF/MSDF) */
        SDL_GPUShaderCreateInfo vs_info = {};
        vs_info.code = text_vert_spv;
        vs_info.code_size = text_vert_spv_len;
        vs_info.entrypoint = "main";
        vs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        vs_info.stage = SDL_GPU_SHADERSTAGE_VERTEX;
        vs_info.num_samplers = 0;
        vs_info.num_storage_textures = 0;
        vs_info.num_storage_buffers = 0;
        vs_info.num_uniform_buffers = 1;
        vertex_shader = SDL_CreateGPUShader(tr->gpu, &vs_info);
        if (!vertex_shader) {
            agentite_set_error("Text: Failed to create %s SPIRV vertex shader: %s",
                    is_msdf ? "MSDF" : "SDF", SDL_GetError());
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {};
        fs_info.code = spirv_fs;
        fs_info.code_size = spirv_fs_len;
        fs_info.entrypoint = "main";
        fs_info.format = SDL_GPU_SHADERFORMAT_SPIRV;
        fs_info.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
        fs_info.num_samplers = 1;
        fs_info.num_storage_textures = 0;
        fs_info.num_storage_buffers = 0;
        fs_info.num_uniform_buffers = 1;
        fragment_shader = SDL_CreateGPUShader(tr->gpu, &fs_info);
        if (!fragment_shader) {
            agentite_set_error("Text: Failed to create %s SPIRV fragment shader: %s",
                    is_msdf ? "MSDF" : "SDF", SDL_GetError());
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else {
        agentite_set_error("Text: No supported shader format for SDF (need MSL or SPIRV)");
        return false;
    }

    SDL_GPUVertexAttribute attributes[3] = {};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[0].offset = offsetof(TextVertex, pos);

    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = offsetof(TextVertex, uv);

    attributes[2].location = 2;
    attributes[2].buffer_slot = 0;
    attributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4;
    attributes[2].offset = offsetof(TextVertex, color);

    SDL_GPUVertexBufferDescription vb_desc = {};
    vb_desc.slot = 0;
    vb_desc.pitch = sizeof(TextVertex);
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

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(tr->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
    SDL_ReleaseGPUShader(tr->gpu, fragment_shader);

    if (!pipeline) {
        agentite_set_error("Text: Failed to create %s pipeline: %s",
                is_msdf ? "MSDF" : "SDF", SDL_GetError());
        return false;
    }

    if (is_msdf) {
        tr->msdf_pipeline = pipeline;
    } else {
        tr->sdf_pipeline = pipeline;
    }

    SDL_Log("Text: %s pipeline created successfully", is_msdf ? "MSDF" : "SDF");
    return true;
}

/* ============================================================================
 * Internal: Font Atlas Creation
 * ============================================================================ */

SDL_GPUTexture *text_create_font_atlas(Agentite_TextRenderer *tr,
                                        unsigned char *atlas_bitmap)
{
    /* Create GPU texture for the atlas (single channel, but we upload as RGBA) */
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = ATLAS_SIZE;
    tex_info.height = ATLAS_SIZE;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.props = 0;
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(tr->gpu, &tex_info);
    if (!texture) {
        agentite_set_error_from_sdl("Text: Failed to create atlas texture");
        return NULL;
    }

    /* Upload pixel data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = ATLAS_SIZE * ATLAS_SIZE;
    transfer_info.props = 0;
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(tr->gpu, &transfer_info);
    if (!transfer) {
        agentite_set_error_from_sdl("Text: Failed to create transfer buffer");
        SDL_ReleaseGPUTexture(tr->gpu, texture);
        return NULL;
    }

    void *mapped = SDL_MapGPUTransferBuffer(tr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, atlas_bitmap, ATLAS_SIZE * ATLAS_SIZE);
        SDL_UnmapGPUTransferBuffer(tr->gpu, transfer);
    }

    /* Copy to texture */
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(tr->gpu);
    if (cmd) {
        SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
        if (copy_pass) {
            SDL_GPUTextureTransferInfo src = {};
            src.transfer_buffer = transfer;
            src.offset = 0;
            src.pixels_per_row = ATLAS_SIZE;
            src.rows_per_layer = ATLAS_SIZE;

            SDL_GPUTextureRegion dst = {};
            dst.texture = texture;
            dst.mip_level = 0;
            dst.layer = 0;
            dst.x = 0;
            dst.y = 0;
            dst.z = 0;
            dst.w = ATLAS_SIZE;
            dst.h = ATLAS_SIZE;
            dst.d = 1;

            SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
            SDL_EndGPUCopyPass(copy_pass);
        }
        SDL_SubmitGPUCommandBuffer(cmd);
    }

    SDL_ReleaseGPUTransferBuffer(tr->gpu, transfer);

    return texture;
}

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

Agentite_TextRenderer *agentite_text_init(SDL_GPUDevice *gpu, SDL_Window *window)
{
    if (!gpu || !window) return NULL;

    Agentite_TextRenderer *tr = AGENTITE_ALLOC(Agentite_TextRenderer);
    if (!tr) {
        agentite_set_error("Text: Failed to allocate renderer");
        return NULL;
    }

    tr->gpu = gpu;
    tr->window = window;

    /* Get logical window size - user coordinates are in logical space, not pixel space.
     * This ensures text positioned at y=700 in a 720-point window appears near the bottom,
     * regardless of Retina/HiDPI scaling. */
    SDL_GetWindowSize(window, &tr->screen_width, &tr->screen_height);

    /* Allocate CPU-side buffers */
    tr->vertices = (TextVertex*)malloc(TEXT_VERTEX_CAPACITY * sizeof(TextVertex));
    tr->indices = (uint16_t*)malloc(TEXT_INDEX_CAPACITY * sizeof(uint16_t));
    if (!tr->vertices || !tr->indices) {
        agentite_set_error("Text: Failed to allocate batch buffers");
        agentite_text_shutdown(tr);
        return NULL;
    }

    /* Pre-generate index buffer (quads always have same index pattern) */
    for (uint32_t i = 0; i < TEXT_MAX_BATCH; i++) {
        uint32_t base_vertex = i * 4;
        uint32_t base_index = i * 6;
        tr->indices[base_index + 0] = (uint16_t)(base_vertex + 0);
        tr->indices[base_index + 1] = (uint16_t)(base_vertex + 1);
        tr->indices[base_index + 2] = (uint16_t)(base_vertex + 2);
        tr->indices[base_index + 3] = (uint16_t)(base_vertex + 0);
        tr->indices[base_index + 4] = (uint16_t)(base_vertex + 2);
        tr->indices[base_index + 5] = (uint16_t)(base_vertex + 3);
    }

    /* Create GPU buffers */
    SDL_GPUBufferCreateInfo vb_info = {};
    vb_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vb_info.size = (Uint32)(TEXT_VERTEX_CAPACITY * sizeof(TextVertex));
    vb_info.props = 0;
    tr->vertex_buffer = SDL_CreateGPUBuffer(gpu, &vb_info);
    if (!tr->vertex_buffer) {
        agentite_set_error_from_sdl("Text: Failed to create vertex buffer");
        agentite_text_shutdown(tr);
        return NULL;
    }

    SDL_GPUBufferCreateInfo ib_info = {};
    ib_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ib_info.size = (Uint32)(TEXT_INDEX_CAPACITY * sizeof(uint16_t));
    ib_info.props = 0;
    tr->index_buffer = SDL_CreateGPUBuffer(gpu, &ib_info);
    if (!tr->index_buffer) {
        agentite_set_error_from_sdl("Text: Failed to create index buffer");
        agentite_text_shutdown(tr);
        return NULL;
    }

    /* Create sampler with linear filtering for smooth text */
    SDL_GPUSamplerCreateInfo sampler_info = {};
    sampler_info.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler_info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    sampler_info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    tr->sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
    if (!tr->sampler) {
        agentite_set_error_from_sdl("Text: Failed to create sampler");
        agentite_text_shutdown(tr);
        return NULL;
    }

    /* Create bitmap pipeline */
    if (!text_create_pipeline(tr)) {
        agentite_text_shutdown(tr);
        return NULL;
    }

    /* Create SDF and MSDF pipelines */
    if (!sdf_create_pipeline(tr, false)) {
        SDL_Log("Text: Warning - SDF pipeline creation failed");
    }
    if (!sdf_create_pipeline(tr, true)) {
        SDL_Log("Text: Warning - MSDF pipeline creation failed");
    }

    SDL_Log("Text: Renderer initialized (%dx%d)", tr->screen_width, tr->screen_height);
    return tr;
}

void agentite_text_shutdown(Agentite_TextRenderer *tr)
{
    if (!tr) return;

    if (tr->pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(tr->gpu, tr->pipeline);
    }
    if (tr->sdf_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(tr->gpu, tr->sdf_pipeline);
    }
    if (tr->msdf_pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(tr->gpu, tr->msdf_pipeline);
    }
    if (tr->vertex_buffer) {
        SDL_ReleaseGPUBuffer(tr->gpu, tr->vertex_buffer);
    }
    if (tr->index_buffer) {
        SDL_ReleaseGPUBuffer(tr->gpu, tr->index_buffer);
    }
    if (tr->sampler) {
        SDL_ReleaseGPUSampler(tr->gpu, tr->sampler);
    }

    free(tr->vertices);
    free(tr->indices);
    free(tr);

    SDL_Log("Text: Renderer shutdown complete");
}

void agentite_text_set_screen_size(Agentite_TextRenderer *tr, int width, int height)
{
    if (!tr) return;
    tr->screen_width = width;
    tr->screen_height = height;
}
