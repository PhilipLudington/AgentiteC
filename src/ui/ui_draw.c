/*
 * Carbon UI - Drawing and GPU Pipeline
 */

#include "carbon/ui.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <math.h>

/* ============================================================================
 * Embedded MSL Shader Source
 * ============================================================================ */

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

/* ============================================================================
 * GPU Pipeline Creation
 * ============================================================================ */

/* Create the graphics pipeline for UI rendering */
static bool cui_create_graphics_pipeline(CUI_Context *ctx)
{
    if (!ctx || !ctx->gpu) return false;

    /* Check what shader formats this device supports */
    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(ctx->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Create vertex shader from MSL source */
        SDL_GPUShaderCreateInfo vs_info = {
            .code = (const Uint8 *)ui_shader_msl,
            .code_size = sizeof(ui_shader_msl),
            .entrypoint = "ui_vertex",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers = 0,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 1,
        };
        vertex_shader = SDL_CreateGPUShader(ctx->gpu, &vs_info);
        if (!vertex_shader) {
            SDL_Log("CUI: Failed to create vertex shader: %s", SDL_GetError());
            return false;
        }

        /* Create fragment shader from MSL source */
        SDL_GPUShaderCreateInfo fs_info = {
            .code = (const Uint8 *)ui_shader_msl,
            .code_size = sizeof(ui_shader_msl),
            .entrypoint = "ui_fragment",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = 1,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 0,
        };
        fragment_shader = SDL_CreateGPUShader(ctx->gpu, &fs_info);
        if (!fragment_shader) {
            SDL_Log("CUI: Failed to create fragment shader: %s", SDL_GetError());
            SDL_ReleaseGPUShader(ctx->gpu, vertex_shader);
            return false;
        }
    } else {
        SDL_Log("CUI: No supported shader format (need MSL for Metal)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[] = {
        { /* position */
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(CUI_Vertex, pos)
        },
        { /* texcoord */
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(CUI_Vertex, uv)
        },
        { /* color */
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_UINT,
            .offset = offsetof(CUI_Vertex, color)
        }
    };

    /* Define vertex buffer layout */
    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(CUI_Vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    /* Define vertex input state */
    SDL_GPUVertexInputState vertex_input = {
        .vertex_buffer_descriptions = &vb_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = attributes,
        .num_vertex_attributes = 3
    };

    /* Define blend state for alpha blending */
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

    /* Define color target description */
    SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM,  /* Swapchain format */
        .blend_state = blend_state
    };

    /* Create the graphics pipeline */
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

    ctx->pipeline = SDL_CreateGPUGraphicsPipeline(ctx->gpu, &pipeline_info);

    /* Release shaders (pipeline holds references) */
    SDL_ReleaseGPUShader(ctx->gpu, vertex_shader);
    SDL_ReleaseGPUShader(ctx->gpu, fragment_shader);

    if (!ctx->pipeline) {
        SDL_Log("CUI: Failed to create graphics pipeline: %s", SDL_GetError());
        return false;
    }

    SDL_Log("CUI: Graphics pipeline created successfully");
    return true;
}

bool cui_create_pipeline(CUI_Context *ctx)
{
    if (!ctx || !ctx->gpu) return false;

    /* Create vertex buffer (GPU side) */
    SDL_GPUBufferCreateInfo vb_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = ctx->vertex_capacity * sizeof(CUI_Vertex),
        .props = 0
    };
    ctx->vertex_buffer = SDL_CreateGPUBuffer(ctx->gpu, &vb_info);
    if (!ctx->vertex_buffer) {
        SDL_Log("CUI: Failed to create vertex buffer");
        return false;
    }

    /* Create index buffer (GPU side) */
    SDL_GPUBufferCreateInfo ib_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = ctx->index_capacity * sizeof(uint16_t),
        .props = 0
    };
    ctx->index_buffer = SDL_CreateGPUBuffer(ctx->gpu, &ib_info);
    if (!ctx->index_buffer) {
        SDL_Log("CUI: Failed to create index buffer");
        return false;
    }

    /* Create sampler for font atlas */
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    ctx->sampler = SDL_CreateGPUSampler(ctx->gpu, &sampler_info);
    if (!ctx->sampler) {
        SDL_Log("CUI: Failed to create sampler");
        return false;
    }

    /* Create graphics pipeline with shaders */
    if (!cui_create_graphics_pipeline(ctx)) {
        SDL_Log("CUI: Failed to create graphics pipeline");
        return false;
    }

    SDL_Log("CUI: GPU resources created successfully");
    return true;
}

void cui_destroy_pipeline(CUI_Context *ctx)
{
    if (!ctx) return;

    if (ctx->pipeline) {
        SDL_ReleaseGPUGraphicsPipeline(ctx->gpu, ctx->pipeline);
        ctx->pipeline = NULL;
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
}

/* ============================================================================
 * Vertex/Index Buffer Management
 * ============================================================================ */

/* Reserve space for vertices and indices, returns base vertex index */
static bool cui_reserve(CUI_Context *ctx, uint32_t vert_count, uint32_t idx_count,
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

/* Add a quad (4 vertices, 6 indices) */
static void cui_add_quad(CUI_Context *ctx,
                         float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1,
                         uint32_t color)
{
    uint32_t base;
    if (!cui_reserve(ctx, 4, 6, &base)) return;

    CUI_Vertex *v = &ctx->vertices[base];
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

/* ============================================================================
 * Drawing Primitives
 * ============================================================================ */

void cui_draw_rect(CUI_Context *ctx, float x, float y, float w, float h,
                   uint32_t color)
{
    if (!ctx || w <= 0 || h <= 0) return;

    /* UV at (0,0) is white pixel in font atlas for solid colors */
    cui_add_quad(ctx, x, y, x + w, y + h, 0.0f, 0.0f, 0.0f, 0.0f, color);
}

void cui_draw_rect_outline(CUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float thickness)
{
    if (!ctx || w <= 0 || h <= 0) return;

    float t = thickness;
    /* Top */
    cui_draw_rect(ctx, x, y, w, t, color);
    /* Bottom */
    cui_draw_rect(ctx, x, y + h - t, w, t, color);
    /* Left */
    cui_draw_rect(ctx, x, y + t, t, h - 2 * t, color);
    /* Right */
    cui_draw_rect(ctx, x + w - t, y + t, t, h - 2 * t, color);
}

void cui_draw_rect_rounded(CUI_Context *ctx, float x, float y, float w, float h,
                           uint32_t color, float radius)
{
    if (!ctx || w <= 0 || h <= 0) return;

    /* For now, just draw a regular rect (rounded corners need more vertices) */
    /* TODO: Implement proper rounded corners with arc segments */
    (void)radius;
    cui_draw_rect(ctx, x, y, w, h, color);
}

void cui_draw_line(CUI_Context *ctx, float x1, float y1, float x2, float y2,
                   uint32_t color, float thickness)
{
    if (!ctx) return;

    /* Calculate perpendicular offset for line thickness */
    float dx = x2 - x1;
    float dy = y2 - y1;
    float len = sqrtf(dx * dx + dy * dy);
    if (len < 0.001f) return;

    float nx = -dy / len * thickness * 0.5f;
    float ny = dx / len * thickness * 0.5f;

    uint32_t base;
    if (!cui_reserve(ctx, 4, 6, &base)) return;

    CUI_Vertex *v = &ctx->vertices[base];
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

/* ============================================================================
 * Scissor Stack
 * ============================================================================ */

void cui_push_scissor(CUI_Context *ctx, float x, float y, float w, float h)
{
    if (!ctx || ctx->scissor_depth >= 16) return;

    CUI_Rect rect = {x, y, w, h};

    /* Intersect with current scissor if any */
    if (ctx->scissor_depth > 0) {
        rect = cui_rect_intersect(rect, ctx->scissor_stack[ctx->scissor_depth - 1]);
    }

    ctx->scissor_stack[ctx->scissor_depth++] = rect;
}

void cui_pop_scissor(CUI_Context *ctx)
{
    if (!ctx || ctx->scissor_depth <= 0) return;
    ctx->scissor_depth--;
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

/* Upload UI vertex/index data to GPU - call BEFORE render pass */
void cui_upload(CUI_Context *ctx, SDL_GPUCommandBuffer *cmd)
{
    if (!ctx || !cmd) return;
    if (ctx->vertex_count == 0 || ctx->index_count == 0) return;

    /* Upload vertex data to GPU */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = ctx->vertex_count * sizeof(CUI_Vertex) +
                ctx->index_count * sizeof(uint16_t),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ctx->gpu, &transfer_info);
    if (!transfer) {
        SDL_Log("CUI: Failed to create transfer buffer");
        return;
    }

    /* Map and copy data */
    void *mapped = SDL_MapGPUTransferBuffer(ctx->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, ctx->vertices, ctx->vertex_count * sizeof(CUI_Vertex));
        memcpy((uint8_t *)mapped + ctx->vertex_count * sizeof(CUI_Vertex),
               ctx->indices, ctx->index_count * sizeof(uint16_t));
        SDL_UnmapGPUTransferBuffer(ctx->gpu, transfer);
    }

    /* Copy from transfer buffer to GPU buffers */
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        SDL_GPUTransferBufferLocation src_vert = {
            .transfer_buffer = transfer,
            .offset = 0
        };
        SDL_GPUBufferRegion dst_vert = {
            .buffer = ctx->vertex_buffer,
            .offset = 0,
            .size = ctx->vertex_count * sizeof(CUI_Vertex)
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        SDL_GPUTransferBufferLocation src_idx = {
            .transfer_buffer = transfer,
            .offset = ctx->vertex_count * sizeof(CUI_Vertex)
        };
        SDL_GPUBufferRegion dst_idx = {
            .buffer = ctx->index_buffer,
            .offset = 0,
            .size = ctx->index_count * sizeof(uint16_t)
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(ctx->gpu, transfer);
}

void cui_render(CUI_Context *ctx, SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass)
{
    if (!ctx || !cmd || !pass) return;
    if (ctx->vertex_count == 0 || ctx->index_count == 0) return;
    if (!ctx->pipeline || !ctx->font_atlas) {
        /* Pipeline not ready yet */
        return;
    }

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, ctx->pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {
        .buffer = ctx->vertex_buffer,
        .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {
        .buffer = ctx->index_buffer,
        .offset = 0
    };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Push uniform data (screen size) */
    float uniforms[4] = {(float)ctx->width, (float)ctx->height, 0, 0};
    SDL_PushGPUVertexUniformData(cmd, 0, uniforms, sizeof(uniforms));

    /* Bind font atlas texture and sampler */
    SDL_GPUTextureSamplerBinding tex_binding = {
        .texture = ctx->font_atlas,
        .sampler = ctx->sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw all UI elements */
    SDL_DrawGPUIndexedPrimitives(pass, ctx->index_count, 1, 0, 0, 0);
}

/* ============================================================================
 * Internal: Add textured quad (used by text rendering)
 * ============================================================================ */

void cui_draw_textured_quad(CUI_Context *ctx,
                            float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1,
                            uint32_t color)
{
    cui_add_quad(ctx, x0, y0, x1, y1, u0, v0, u1, v1, color);
}
