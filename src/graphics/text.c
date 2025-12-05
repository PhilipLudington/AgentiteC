/*
 * Carbon Text Rendering System Implementation
 */

#include "carbon/text.h"
#include "carbon/error.h"
#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* STB TrueType for font rasterization (implementation in ui_text.c) */
#include "stb_truetype.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TEXT_MAX_BATCH 2048             /* Max glyphs per batch */
#define TEXT_VERTS_PER_GLYPH 4
#define TEXT_INDICES_PER_GLYPH 6
#define TEXT_VERTEX_CAPACITY (TEXT_MAX_BATCH * TEXT_VERTS_PER_GLYPH)
#define TEXT_INDEX_CAPACITY (TEXT_MAX_BATCH * TEXT_INDICES_PER_GLYPH)
#define TEXT_MAX_QUEUED_BATCHES 8       /* Max batches that can be queued per frame */

#define ATLAS_SIZE 1024                 /* Font atlas texture size */
#define FIRST_CHAR 32                   /* Space */
#define LAST_CHAR 126                   /* Tilde */
#define NUM_CHARS (LAST_CHAR - FIRST_CHAR + 1)

/* SDF font constants */
#define SDF_MAX_GLYPHS 256              /* Max glyphs in SDF font */

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
"        float glow_alpha = smoothstep(glow_edge - aa * 2.0, edge, dist);\n"
"        glow_alpha = glow_alpha * (1.0 - result.a) * sdf.glow_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.glow_color.rgb, result.rgb, result.a),\n"
"            max(result.a, glow_alpha)\n"
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
"        float glow_alpha = smoothstep(glow_edge - aa * 2.0, edge, dist);\n"
"        glow_alpha = glow_alpha * (1.0 - result.a) * sdf.glow_color.a * in.color.a;\n"
"        result = float4(\n"
"            mix(sdf.glow_color.rgb, result.rgb, result.a),\n"
"            max(result.a, glow_alpha)\n"
"        );\n"
"    }\n"
"\n"
"    return result;\n"
"}\n";

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Vertex format for text rendering */
typedef struct TextVertex {
    float pos[2];       /* Screen position (x, y) */
    float uv[2];        /* Texture coordinates */
    float color[4];     /* RGBA color */
} TextVertex;

/* Packed glyph data for a character */
typedef struct GlyphInfo {
    float x0, y0, x1, y1;       /* Bounding box relative to baseline */
    float u0, v0, u1, v1;       /* UV coordinates in atlas */
    float advance_x;            /* Horizontal advance */
} GlyphInfo;

struct Carbon_Font {
    stbtt_fontinfo stb_font;
    unsigned char *font_data;    /* TTF file data (must keep alive) */
    GlyphInfo glyphs[NUM_CHARS];
    float size;
    float scale;
    float ascent;
    float descent;
    float line_height;
    SDL_GPUTexture *atlas_texture;
};

/* SDF glyph info (from JSON) */
typedef struct SDFGlyphInfo {
    uint32_t codepoint;
    float advance;                  /* Horizontal advance (em units) */
    float plane_left, plane_bottom; /* Quad bounds relative to baseline (em units) */
    float plane_right, plane_top;
    float atlas_left, atlas_bottom; /* Texture coordinates (pixels) */
    float atlas_right, atlas_top;
} SDFGlyphInfo;

/* SDF Font structure */
struct Carbon_SDFFont {
    Carbon_SDFFontType type;
    SDFGlyphInfo *glyphs;
    int glyph_count;

    /* Font metrics (em units) */
    float em_size;
    float font_size;            /* Size font was generated at */
    float distance_range;       /* SDF distance range in pixels */
    float line_height;
    float ascender;
    float descender;

    /* Atlas info */
    int atlas_width;
    int atlas_height;
    SDL_GPUTexture *atlas_texture;
};

/* SDF fragment shader uniform struct (must match shader layout)
 * Using float4 for alignment-safe layout */
typedef struct SDFFragmentUniforms {
    float params[4];           // distance_range, scale, weight, edge_threshold
    float outline_params[4];   // outline_width, pad, pad, pad
    float outline_color[4];    // RGBA
    float glow_params[4];      // glow_width, pad, pad, pad
    float glow_color[4];       // RGBA
    uint32_t flags;
    float _padding[3];
} SDFFragmentUniforms;

/* Queued batch - stores a completed batch ready for upload/render */
typedef enum TextBatchType {
    TEXT_BATCH_BITMAP,
    TEXT_BATCH_SDF,
    TEXT_BATCH_MSDF
} TextBatchType;

typedef struct QueuedTextBatch {
    TextBatchType type;
    uint32_t vertex_offset;     /* Offset into shared vertex buffer */
    uint32_t index_offset;      /* Offset into shared index buffer */
    uint32_t vertex_count;
    uint32_t index_count;

    /* For bitmap batches */
    SDL_GPUTexture *atlas_texture;

    /* For SDF/MSDF batches */
    Carbon_SDFFont *sdf_font;
    float sdf_scale;
    Carbon_TextEffects effects;
} QueuedTextBatch;

struct Carbon_TextRenderer {
    SDL_GPUDevice *gpu;
    SDL_Window *window;
    int screen_width;
    int screen_height;

    /* GPU resources */
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUGraphicsPipeline *sdf_pipeline;
    SDL_GPUGraphicsPipeline *msdf_pipeline;
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUSampler *sampler;

    /* CPU-side batch buffers (shared across all queued batches) */
    TextVertex *vertices;
    uint16_t *indices;
    uint32_t vertex_count;      /* Total vertices across all queued batches */
    uint32_t index_count;       /* Total indices across all queued batches */
    uint32_t glyph_count;       /* Glyphs in current batch being built */

    /* Batch queue for multi-batch rendering */
    QueuedTextBatch queued_batches[TEXT_MAX_QUEUED_BATCHES];
    uint32_t queued_batch_count;

    /* Current batch state (while building) */
    Carbon_Font *current_font;
    bool batch_started;
    uint32_t current_batch_vertex_start;
    uint32_t current_batch_index_start;

    /* SDF batch state */
    Carbon_SDFFont *current_sdf_font;
    bool is_sdf_batch;
    float current_sdf_scale;
    Carbon_TextEffects current_effects;
};

/* ============================================================================
 * Internal: Pipeline Creation
 * ============================================================================ */

static bool text_create_pipeline(Carbon_TextRenderer *tr)
{
    if (!tr || !tr->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(tr->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        /* Create vertex shader */
        SDL_GPUShaderCreateInfo vs_info = {
            .code = (const Uint8 *)text_shader_msl,
            .code_size = sizeof(text_shader_msl),
            .entrypoint = "text_vertex",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers = 0,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 1,
        };
        vertex_shader = SDL_CreateGPUShader(tr->gpu, &vs_info);
        if (!vertex_shader) {
            carbon_set_error_from_sdl("Text: Failed to create vertex shader");
            return false;
        }

        /* Create fragment shader */
        SDL_GPUShaderCreateInfo fs_info = {
            .code = (const Uint8 *)text_shader_msl,
            .code_size = sizeof(text_shader_msl),
            .entrypoint = "text_fragment",
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = 1,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 0,
        };
        fragment_shader = SDL_CreateGPUShader(tr->gpu, &fs_info);
        if (!fragment_shader) {
            carbon_set_error_from_sdl("Text: Failed to create fragment shader");
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else {
        carbon_set_error("Text: No supported shader format (need MSL for Metal)");
        return false;
    }

    /* Define vertex attributes */
    SDL_GPUVertexAttribute attributes[] = {
        { /* position */
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(TextVertex, pos)
        },
        { /* texcoord */
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = offsetof(TextVertex, uv)
        },
        { /* color */
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
            .offset = offsetof(TextVertex, color)
        }
    };

    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(TextVertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    SDL_GPUVertexInputState vertex_input = {
        .vertex_buffer_descriptions = &vb_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = attributes,
        .num_vertex_attributes = 3
    };

    /* Alpha blending for text */
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

    tr->pipeline = SDL_CreateGPUGraphicsPipeline(tr->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
    SDL_ReleaseGPUShader(tr->gpu, fragment_shader);

    if (!tr->pipeline) {
        carbon_set_error_from_sdl("Text: Failed to create graphics pipeline");
        return false;
    }

    SDL_Log("Text: Graphics pipeline created successfully");
    return true;
}

/* ============================================================================
 * Internal: SDF/MSDF Pipeline Creation
 * ============================================================================ */

static bool sdf_create_pipeline(Carbon_TextRenderer *tr, bool is_msdf)
{
    if (!tr || !tr->gpu) return false;

    SDL_GPUShaderFormat formats = SDL_GetGPUShaderFormats(tr->gpu);

    SDL_GPUShader *vertex_shader = NULL;
    SDL_GPUShader *fragment_shader = NULL;

    const char *shader_src = is_msdf ? msdf_shader_msl : sdf_shader_msl;
    size_t shader_size = is_msdf ? sizeof(msdf_shader_msl) : sizeof(sdf_shader_msl);
    const char *vs_entry = is_msdf ? "msdf_vertex" : "sdf_vertex";
    const char *fs_entry = is_msdf ? "msdf_fragment" : "sdf_fragment";

    if (formats & SDL_GPU_SHADERFORMAT_MSL) {
        SDL_GPUShaderCreateInfo vs_info = {
            .code = (const Uint8 *)shader_src,
            .code_size = shader_size,
            .entrypoint = vs_entry,
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_VERTEX,
            .num_samplers = 0,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 1,
        };
        vertex_shader = SDL_CreateGPUShader(tr->gpu, &vs_info);
        if (!vertex_shader) {
            carbon_set_error("Text: Failed to create %s vertex shader: %s",
                    is_msdf ? "MSDF" : "SDF", SDL_GetError());
            return false;
        }

        SDL_GPUShaderCreateInfo fs_info = {
            .code = (const Uint8 *)shader_src,
            .code_size = shader_size,
            .entrypoint = fs_entry,
            .format = SDL_GPU_SHADERFORMAT_MSL,
            .stage = SDL_GPU_SHADERSTAGE_FRAGMENT,
            .num_samplers = 1,
            .num_storage_textures = 0,
            .num_storage_buffers = 0,
            .num_uniform_buffers = 1,
        };
        fragment_shader = SDL_CreateGPUShader(tr->gpu, &fs_info);
        if (!fragment_shader) {
            carbon_set_error("Text: Failed to create %s fragment shader: %s",
                    is_msdf ? "MSDF" : "SDF", SDL_GetError());
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else {
        carbon_set_error("Text: No supported shader format for SDF (need MSL)");
        return false;
    }

    SDL_GPUVertexAttribute attributes[] = {
        { .location = 0, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(TextVertex, pos) },
        { .location = 1, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
          .offset = offsetof(TextVertex, uv) },
        { .location = 2, .buffer_slot = 0,
          .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4,
          .offset = offsetof(TextVertex, color) }
    };

    SDL_GPUVertexBufferDescription vb_desc = {
        .slot = 0,
        .pitch = sizeof(TextVertex),
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

    SDL_GPUGraphicsPipeline *pipeline = SDL_CreateGPUGraphicsPipeline(tr->gpu, &pipeline_info);

    SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
    SDL_ReleaseGPUShader(tr->gpu, fragment_shader);

    if (!pipeline) {
        carbon_set_error("Text: Failed to create %s pipeline: %s",
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
 * Internal: Minimal JSON Parser for msdf-atlas-gen format
 * ============================================================================ */

/* Skip whitespace */
static const char *json_skip_ws(const char *p) {
    while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
    return p;
}

/* Parse a string (returns pointer past closing quote, stores value) */
static const char *json_parse_string(const char *p, char *out, size_t max) {
    if (*p != '"') return NULL;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < max - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'n') out[i++] = '\n';
            else if (*p == 't') out[i++] = '\t';
            else out[i++] = *p;
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    if (*p == '"') p++;
    return p;
}

/* Parse a number */
static const char *json_parse_number(const char *p, double *out) {
    char *end;
    *out = strtod(p, &end);
    return end;
}

/* Skip a JSON value (string, number, object, array, bool, null) */
static const char *json_skip_value(const char *p) {
    p = json_skip_ws(p);
    if (*p == '"') {
        p++;
        while (*p && *p != '"') {
            if (*p == '\\' && p[1]) p++;
            p++;
        }
        if (*p == '"') p++;
    } else if (*p == '{') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '{') depth++;
            else if (*p == '}') depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && p[1]) p++;
                    p++;
                }
            }
            p++;
        }
    } else if (*p == '[') {
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '[') depth++;
            else if (*p == ']') depth--;
            else if (*p == '"') {
                p++;
                while (*p && *p != '"') {
                    if (*p == '\\' && p[1]) p++;
                    p++;
                }
            }
            p++;
        }
    } else {
        while (*p && *p != ',' && *p != '}' && *p != ']' &&
               *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') p++;
    }
    return p;
}

/* Find a key in current object (returns pointer to value) */
static const char *json_find_key(const char *p, const char *key) {
    p = json_skip_ws(p);
    if (*p != '{') return NULL;
    p++;

    while (*p) {
        p = json_skip_ws(p);
        if (*p == '}') return NULL;

        char found_key[64];
        p = json_parse_string(p, found_key, sizeof(found_key));
        if (!p) return NULL;

        p = json_skip_ws(p);
        if (*p != ':') return NULL;
        p++;
        p = json_skip_ws(p);

        if (strcmp(found_key, key) == 0) {
            return p;
        }

        p = json_skip_value(p);
        p = json_skip_ws(p);
        if (*p == ',') p++;
    }
    return NULL;
}

/* Parse SDF font JSON (msdf-atlas-gen format) */
static bool parse_sdf_json(const char *json, Carbon_SDFFont *font) {
    /* Parse atlas section */
    const char *atlas = json_find_key(json, "atlas");
    if (atlas) {
        const char *type_val = json_find_key(atlas, "type");
        if (type_val) {
            char type_str[32];
            json_parse_string(type_val, type_str, sizeof(type_str));
            if (strcmp(type_str, "msdf") == 0 || strcmp(type_str, "mtsdf") == 0) {
                font->type = CARBON_SDF_TYPE_MSDF;
            } else {
                font->type = CARBON_SDF_TYPE_SDF;
            }
        }

        const char *dist = json_find_key(atlas, "distanceRange");
        if (dist) {
            double val;
            json_parse_number(dist, &val);
            font->distance_range = (float)val;
        }

        const char *size = json_find_key(atlas, "size");
        if (size) {
            double val;
            json_parse_number(size, &val);
            font->font_size = (float)val;
        }

        const char *width = json_find_key(atlas, "width");
        if (width) {
            double val;
            json_parse_number(width, &val);
            font->atlas_width = (int)val;
        }

        const char *height = json_find_key(atlas, "height");
        if (height) {
            double val;
            json_parse_number(height, &val);
            font->atlas_height = (int)val;
        }
    }

    /* Parse metrics section */
    const char *metrics = json_find_key(json, "metrics");
    if (metrics) {
        const char *em = json_find_key(metrics, "emSize");
        if (em) {
            double val;
            json_parse_number(em, &val);
            font->em_size = (float)val;
        }

        const char *lh = json_find_key(metrics, "lineHeight");
        if (lh) {
            double val;
            json_parse_number(lh, &val);
            font->line_height = (float)val;
        }

        const char *asc = json_find_key(metrics, "ascender");
        if (asc) {
            double val;
            json_parse_number(asc, &val);
            font->ascender = (float)val;
        }

        const char *desc = json_find_key(metrics, "descender");
        if (desc) {
            double val;
            json_parse_number(desc, &val);
            font->descender = (float)val;
        }
    }

    /* Parse glyphs array */
    const char *glyphs = json_find_key(json, "glyphs");
    if (!glyphs) {
        carbon_set_error("Text: No glyphs array in JSON");
        return false;
    }

    glyphs = json_skip_ws(glyphs);
    if (*glyphs != '[') {
        carbon_set_error("Text: Glyphs is not an array");
        return false;
    }
    glyphs++;

    /* Count glyphs */
    int count = 0;
    const char *p = glyphs;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '{') { if (depth == 1) count++; depth++; }
        else if (*p == '}') depth--;
        else if (*p == '[') depth++;
        else if (*p == ']') depth--;
        p++;
    }

    font->glyphs = malloc(count * sizeof(SDFGlyphInfo));
    if (!font->glyphs) return false;
    font->glyph_count = 0;

    /* Parse each glyph */
    p = glyphs;
    while (*p && font->glyph_count < count) {
        p = json_skip_ws(p);
        if (*p == ']') break;
        if (*p == ',') { p++; continue; }
        if (*p != '{') { p++; continue; }

        SDFGlyphInfo *g = &font->glyphs[font->glyph_count];
        memset(g, 0, sizeof(*g));

        const char *unicode = json_find_key(p, "unicode");
        if (unicode) {
            double val;
            json_parse_number(unicode, &val);
            g->codepoint = (uint32_t)val;
        }

        const char *advance = json_find_key(p, "advance");
        if (advance) {
            double val;
            json_parse_number(advance, &val);
            g->advance = (float)val;
        }

        const char *plane = json_find_key(p, "planeBounds");
        if (plane) {
            const char *v;
            double val;
            if ((v = json_find_key(plane, "left"))) { json_parse_number(v, &val); g->plane_left = (float)val; }
            if ((v = json_find_key(plane, "bottom"))) { json_parse_number(v, &val); g->plane_bottom = (float)val; }
            if ((v = json_find_key(plane, "right"))) { json_parse_number(v, &val); g->plane_right = (float)val; }
            if ((v = json_find_key(plane, "top"))) { json_parse_number(v, &val); g->plane_top = (float)val; }
        }

        const char *atlas_bounds = json_find_key(p, "atlasBounds");
        if (atlas_bounds) {
            const char *v;
            double val;
            if ((v = json_find_key(atlas_bounds, "left"))) { json_parse_number(v, &val); g->atlas_left = (float)val; }
            if ((v = json_find_key(atlas_bounds, "bottom"))) { json_parse_number(v, &val); g->atlas_bottom = (float)val; }
            if ((v = json_find_key(atlas_bounds, "right"))) { json_parse_number(v, &val); g->atlas_right = (float)val; }
            if ((v = json_find_key(atlas_bounds, "top"))) { json_parse_number(v, &val); g->atlas_top = (float)val; }
        }

        font->glyph_count++;
        p = json_skip_value(p);
    }

    return true;
}

/* ============================================================================
 * Internal: Font Atlas Creation
 * ============================================================================ */

static SDL_GPUTexture *create_font_atlas(Carbon_TextRenderer *tr,
                                          unsigned char *atlas_bitmap)
{
    /* Create GPU texture for the atlas (single channel, but we upload as RGBA) */
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = ATLAS_SIZE,
        .height = ATLAS_SIZE,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0
    };
    SDL_GPUTexture *texture = SDL_CreateGPUTexture(tr->gpu, &tex_info);
    if (!texture) {
        carbon_set_error_from_sdl("Text: Failed to create atlas texture");
        return NULL;
    }

    /* Upload pixel data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = ATLAS_SIZE * ATLAS_SIZE,
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(tr->gpu, &transfer_info);
    if (!transfer) {
        carbon_set_error_from_sdl("Text: Failed to create transfer buffer");
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
            SDL_GPUTextureTransferInfo src = {
                .transfer_buffer = transfer,
                .offset = 0,
                .pixels_per_row = ATLAS_SIZE,
                .rows_per_layer = ATLAS_SIZE
            };
            SDL_GPUTextureRegion dst = {
                .texture = texture,
                .mip_level = 0,
                .layer = 0,
                .x = 0,
                .y = 0,
                .z = 0,
                .w = ATLAS_SIZE,
                .h = ATLAS_SIZE,
                .d = 1
            };
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

Carbon_TextRenderer *carbon_text_init(SDL_GPUDevice *gpu, SDL_Window *window)
{
    if (!gpu || !window) return NULL;

    Carbon_TextRenderer *tr = calloc(1, sizeof(Carbon_TextRenderer));
    if (!tr) {
        carbon_set_error("Text: Failed to allocate renderer");
        return NULL;
    }

    tr->gpu = gpu;
    tr->window = window;

    /* Get window size */
    SDL_GetWindowSize(window, &tr->screen_width, &tr->screen_height);

    /* Allocate CPU-side buffers */
    tr->vertices = malloc(TEXT_VERTEX_CAPACITY * sizeof(TextVertex));
    tr->indices = malloc(TEXT_INDEX_CAPACITY * sizeof(uint16_t));
    if (!tr->vertices || !tr->indices) {
        carbon_set_error("Text: Failed to allocate batch buffers");
        carbon_text_shutdown(tr);
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
    SDL_GPUBufferCreateInfo vb_info = {
        .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
        .size = TEXT_VERTEX_CAPACITY * sizeof(TextVertex),
        .props = 0
    };
    tr->vertex_buffer = SDL_CreateGPUBuffer(gpu, &vb_info);
    if (!tr->vertex_buffer) {
        carbon_set_error_from_sdl("Text: Failed to create vertex buffer");
        carbon_text_shutdown(tr);
        return NULL;
    }

    SDL_GPUBufferCreateInfo ib_info = {
        .usage = SDL_GPU_BUFFERUSAGE_INDEX,
        .size = TEXT_INDEX_CAPACITY * sizeof(uint16_t),
        .props = 0
    };
    tr->index_buffer = SDL_CreateGPUBuffer(gpu, &ib_info);
    if (!tr->index_buffer) {
        carbon_set_error_from_sdl("Text: Failed to create index buffer");
        carbon_text_shutdown(tr);
        return NULL;
    }

    /* Create sampler with linear filtering for smooth text */
    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE,
    };
    tr->sampler = SDL_CreateGPUSampler(gpu, &sampler_info);
    if (!tr->sampler) {
        carbon_set_error_from_sdl("Text: Failed to create sampler");
        carbon_text_shutdown(tr);
        return NULL;
    }

    /* Create bitmap pipeline */
    if (!text_create_pipeline(tr)) {
        carbon_text_shutdown(tr);
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

void carbon_text_shutdown(Carbon_TextRenderer *tr)
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

void carbon_text_set_screen_size(Carbon_TextRenderer *tr, int width, int height)
{
    if (!tr) return;
    tr->screen_width = width;
    tr->screen_height = height;
}

/* ============================================================================
 * Font Functions
 * ============================================================================ */

Carbon_Font *carbon_font_load(Carbon_TextRenderer *tr, const char *path, float size)
{
    if (!tr || !path) return NULL;

    /* Read TTF file */
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    if (!file) {
        carbon_set_error("Text: Failed to open font file '%s': %s", path, SDL_GetError());
        return NULL;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        carbon_set_error("Text: Invalid font file size");
        SDL_CloseIO(file);
        return NULL;
    }

    unsigned char *font_data = malloc((size_t)file_size);
    if (!font_data) {
        carbon_set_error("Text: Failed to allocate font data buffer");
        SDL_CloseIO(file);
        return NULL;
    }

    size_t read = SDL_ReadIO(file, font_data, (size_t)file_size);
    SDL_CloseIO(file);

    if (read != (size_t)file_size) {
        carbon_set_error("Text: Failed to read font file");
        free(font_data);
        return NULL;
    }

    Carbon_Font *font = carbon_font_load_memory(tr, font_data, (int)file_size, size);
    if (!font) {
        free(font_data);
        return NULL;
    }

    /* Transfer ownership of font_data to the font */
    font->font_data = font_data;

    SDL_Log("Text: Loaded font '%s' at size %.1f", path, size);
    return font;
}

Carbon_Font *carbon_font_load_memory(Carbon_TextRenderer *tr,
                                      const void *data, int data_size,
                                      float size)
{
    if (!tr || !data || data_size <= 0) return NULL;

    Carbon_Font *font = calloc(1, sizeof(Carbon_Font));
    if (!font) return NULL;

    /* Initialize stb_truetype */
    if (!stbtt_InitFont(&font->stb_font, (const unsigned char *)data, 0)) {
        carbon_set_error("Text: Failed to initialize font");
        free(font);
        return NULL;
    }

    font->size = size;
    font->scale = stbtt_ScaleForPixelHeight(&font->stb_font, size);

    /* Get font metrics */
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&font->stb_font, &ascent, &descent, &line_gap);
    font->ascent = (float)ascent * font->scale;
    font->descent = (float)descent * font->scale;
    font->line_height = (float)(ascent - descent + line_gap) * font->scale;

    /* Bake font atlas using stb_truetype's built-in packer */
    unsigned char *atlas_bitmap = malloc(ATLAS_SIZE * ATLAS_SIZE);
    if (!atlas_bitmap) {
        carbon_set_error("Text: Failed to allocate atlas bitmap");
        free(font);
        return NULL;
    }

    stbtt_bakedchar baked_chars[NUM_CHARS];
    int result = stbtt_BakeFontBitmap((const unsigned char *)data, 0,
                                       size, atlas_bitmap,
                                       ATLAS_SIZE, ATLAS_SIZE,
                                       FIRST_CHAR, NUM_CHARS,
                                       baked_chars);
    if (result <= 0) {
        carbon_set_error("Text: Font atlas baking failed (too many chars or atlas too small)");
        free(atlas_bitmap);
        free(font);
        return NULL;
    }

    /* Extract glyph info from baked chars */
    for (int i = 0; i < NUM_CHARS; i++) {
        stbtt_bakedchar *bc = &baked_chars[i];
        GlyphInfo *g = &font->glyphs[i];

        g->x0 = bc->xoff;
        g->y0 = bc->yoff;
        g->x1 = bc->xoff + (float)(bc->x1 - bc->x0);
        g->y1 = bc->yoff + (float)(bc->y1 - bc->y0);

        g->u0 = (float)bc->x0 / ATLAS_SIZE;
        g->v0 = (float)bc->y0 / ATLAS_SIZE;
        g->u1 = (float)bc->x1 / ATLAS_SIZE;
        g->v1 = (float)bc->y1 / ATLAS_SIZE;

        g->advance_x = bc->xadvance;
    }

    /* Create GPU texture from atlas */
    font->atlas_texture = create_font_atlas(tr, atlas_bitmap);
    free(atlas_bitmap);

    if (!font->atlas_texture) {
        free(font);
        return NULL;
    }

    return font;
}

void carbon_font_destroy(Carbon_TextRenderer *tr, Carbon_Font *font)
{
    if (!tr || !font) return;

    if (font->atlas_texture) {
        SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
    }
    free(font->font_data);
    free(font);
}

float carbon_font_get_size(Carbon_Font *font)
{
    return font ? font->size : 0.0f;
}

float carbon_font_get_line_height(Carbon_Font *font)
{
    return font ? font->line_height : 0.0f;
}

float carbon_font_get_ascent(Carbon_Font *font)
{
    return font ? font->ascent : 0.0f;
}

float carbon_font_get_descent(Carbon_Font *font)
{
    return font ? font->descent : 0.0f;
}

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

float carbon_text_measure(Carbon_Font *font, const char *text)
{
    if (!font || !text) return 0.0f;

    float width = 0.0f;
    const char *p = text;

    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c >= FIRST_CHAR && c <= LAST_CHAR) {
            GlyphInfo *g = &font->glyphs[c - FIRST_CHAR];
            width += g->advance_x;
        }
        p++;
    }

    return width;
}

void carbon_text_measure_bounds(Carbon_Font *font, const char *text,
                                 float *out_width, float *out_height)
{
    if (!font || !text) {
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        return;
    }

    if (out_width) *out_width = carbon_text_measure(font, text);
    if (out_height) *out_height = font->line_height;
}

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

void carbon_text_begin(Carbon_TextRenderer *tr)
{
    if (!tr) return;

    /* If this is the first batch after upload/render, reset the queue */
    if (tr->queued_batch_count == 0) {
        tr->vertex_count = 0;
        tr->index_count = 0;
    }

    /* Track where this batch starts in the shared buffers */
    tr->current_batch_vertex_start = tr->vertex_count;
    tr->current_batch_index_start = tr->index_count;
    tr->glyph_count = 0;

    tr->current_font = NULL;
    tr->current_sdf_font = NULL;
    tr->is_sdf_batch = false;
    tr->current_sdf_scale = 1.0f;
    memset(&tr->current_effects, 0, sizeof(tr->current_effects));
    tr->batch_started = true;
}

static void text_add_glyph(Carbon_TextRenderer *tr,
                            float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1,
                            float r, float g, float b, float a)
{
    /* Check total capacity across all batches */
    if (tr->vertex_count + 4 > TEXT_VERTEX_CAPACITY) {
        SDL_Log("Text: Total vertex buffer overflow, glyph dropped");
        return;
    }

    uint32_t base = tr->vertex_count;
    TextVertex *v = &tr->vertices[base];

    /* Top-left */
    v[0].pos[0] = x0; v[0].pos[1] = y0;
    v[0].uv[0] = u0; v[0].uv[1] = v0;
    v[0].color[0] = r; v[0].color[1] = g; v[0].color[2] = b; v[0].color[3] = a;

    /* Top-right */
    v[1].pos[0] = x1; v[1].pos[1] = y0;
    v[1].uv[0] = u1; v[1].uv[1] = v0;
    v[1].color[0] = r; v[1].color[1] = g; v[1].color[2] = b; v[1].color[3] = a;

    /* Bottom-right */
    v[2].pos[0] = x1; v[2].pos[1] = y1;
    v[2].uv[0] = u1; v[2].uv[1] = v1;
    v[2].color[0] = r; v[2].color[1] = g; v[2].color[2] = b; v[2].color[3] = a;

    /* Bottom-left */
    v[3].pos[0] = x0; v[3].pos[1] = y1;
    v[3].uv[0] = u0; v[3].uv[1] = v1;
    v[3].color[0] = r; v[3].color[1] = g; v[3].color[2] = b; v[3].color[3] = a;

    tr->glyph_count++;
    tr->vertex_count += 4;
    tr->index_count += 6;
}

void carbon_text_draw(Carbon_TextRenderer *tr, Carbon_Font *font,
                      const char *text, float x, float y)
{
    carbon_text_draw_ex(tr, font, text, x, y, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                        CARBON_TEXT_ALIGN_LEFT);
}

void carbon_text_draw_colored(Carbon_TextRenderer *tr, Carbon_Font *font,
                              const char *text, float x, float y,
                              float r, float g, float b, float a)
{
    carbon_text_draw_ex(tr, font, text, x, y, 1.0f, r, g, b, a,
                        CARBON_TEXT_ALIGN_LEFT);
}

void carbon_text_draw_scaled(Carbon_TextRenderer *tr, Carbon_Font *font,
                             const char *text, float x, float y,
                             float scale)
{
    carbon_text_draw_ex(tr, font, text, x, y, scale, 1.0f, 1.0f, 1.0f, 1.0f,
                        CARBON_TEXT_ALIGN_LEFT);
}

void carbon_text_draw_ex(Carbon_TextRenderer *tr, Carbon_Font *font,
                         const char *text, float x, float y,
                         float scale,
                         float r, float g, float b, float a,
                         Carbon_TextAlign align)
{
    if (!tr || !font || !text || !tr->batch_started) return;

    /* Warn if font changes (like sprite renderer) */
    if (tr->current_font && tr->current_font != font) {
        SDL_Log("Text: Warning - font changed mid-batch, results may be incorrect");
    }
    tr->current_font = font;

    /* Handle alignment */
    float offset_x = 0.0f;
    if (align != CARBON_TEXT_ALIGN_LEFT) {
        float text_width = carbon_text_measure(font, text) * scale;
        if (align == CARBON_TEXT_ALIGN_CENTER) {
            offset_x = -text_width / 2.0f;
        } else if (align == CARBON_TEXT_ALIGN_RIGHT) {
            offset_x = -text_width;
        }
    }

    float cursor_x = x + offset_x;
    float cursor_y = y;

    const char *p = text;
    while (*p) {
        unsigned char c = (unsigned char)*p;

        if (c == '\n') {
            cursor_x = x + offset_x;
            cursor_y += font->line_height * scale;
            p++;
            continue;
        }

        if (c >= FIRST_CHAR && c <= LAST_CHAR) {
            GlyphInfo *glyph = &font->glyphs[c - FIRST_CHAR];

            /* Calculate screen position */
            float gx0 = cursor_x + glyph->x0 * scale;
            float gy0 = cursor_y + glyph->y0 * scale;
            float gx1 = cursor_x + glyph->x1 * scale;
            float gy1 = cursor_y + glyph->y1 * scale;

            /* Add glyph quad */
            text_add_glyph(tr, gx0, gy0, gx1, gy1,
                          glyph->u0, glyph->v0, glyph->u1, glyph->v1,
                          r, g, b, a);

            cursor_x += glyph->advance_x * scale;
        }

        p++;
    }
}

void carbon_text_upload(Carbon_TextRenderer *tr, SDL_GPUCommandBuffer *cmd)
{
    if (!tr || !cmd || tr->queued_batch_count == 0 || tr->vertex_count == 0) return;

    /* Upload vertex and index data */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = tr->vertex_count * sizeof(TextVertex) +
                tr->index_count * sizeof(uint16_t),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(tr->gpu, &transfer_info);
    if (!transfer) return;

    void *mapped = SDL_MapGPUTransferBuffer(tr->gpu, transfer, false);
    if (mapped) {
        memcpy(mapped, tr->vertices,
               tr->vertex_count * sizeof(TextVertex));
        memcpy((uint8_t *)mapped + tr->vertex_count * sizeof(TextVertex),
               tr->indices, tr->index_count * sizeof(uint16_t));
        SDL_UnmapGPUTransferBuffer(tr->gpu, transfer);
    }

    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
    if (copy_pass) {
        /* Upload vertices */
        SDL_GPUTransferBufferLocation src_vert = {
            .transfer_buffer = transfer,
            .offset = 0
        };
        SDL_GPUBufferRegion dst_vert = {
            .buffer = tr->vertex_buffer,
            .offset = 0,
            .size = tr->vertex_count * sizeof(TextVertex)
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_vert, &dst_vert, false);

        /* Upload indices */
        SDL_GPUTransferBufferLocation src_idx = {
            .transfer_buffer = transfer,
            .offset = tr->vertex_count * sizeof(TextVertex)
        };
        SDL_GPUBufferRegion dst_idx = {
            .buffer = tr->index_buffer,
            .offset = 0,
            .size = tr->index_count * sizeof(uint16_t)
        };
        SDL_UploadToGPUBuffer(copy_pass, &src_idx, &dst_idx, false);

        SDL_EndGPUCopyPass(copy_pass);
    }

    SDL_ReleaseGPUTransferBuffer(tr->gpu, transfer);
}

void carbon_text_render(Carbon_TextRenderer *tr, SDL_GPUCommandBuffer *cmd,
                        SDL_GPURenderPass *pass)
{
    if (!tr || !cmd || !pass || tr->queued_batch_count == 0) return;

    /* Build vertex uniforms once (shared by all batches) */
    struct {
        float view_projection[16];
        float screen_size[2];
        float padding[2];
    } uniforms;

    mat4 ortho;
    glm_ortho(0.0f, (float)tr->screen_width,
              (float)tr->screen_height, 0.0f,
              -1.0f, 1.0f, ortho);
    memcpy(uniforms.view_projection, ortho, sizeof(float) * 16);

    uniforms.screen_size[0] = (float)tr->screen_width;
    uniforms.screen_size[1] = (float)tr->screen_height;
    uniforms.padding[0] = 0.0f;
    uniforms.padding[1] = 0.0f;

    /* Bind vertex buffer (shared by all batches) */
    SDL_GPUBufferBinding vb_binding = {
        .buffer = tr->vertex_buffer,
        .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer (shared by all batches) */
    SDL_GPUBufferBinding ib_binding = {
        .buffer = tr->index_buffer,
        .offset = 0
    };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Render each queued batch */
    for (uint32_t i = 0; i < tr->queued_batch_count; i++) {
        QueuedTextBatch *batch = &tr->queued_batches[i];

        /* Select pipeline based on batch type */
        SDL_GPUGraphicsPipeline *pipeline;
        switch (batch->type) {
            case TEXT_BATCH_MSDF:
                pipeline = tr->msdf_pipeline;
                break;
            case TEXT_BATCH_SDF:
                pipeline = tr->sdf_pipeline;
                break;
            case TEXT_BATCH_BITMAP:
            default:
                pipeline = tr->pipeline;
                break;
        }

        if (!pipeline || !batch->atlas_texture) continue;

        /* Bind pipeline */
        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        /* Push vertex uniforms */
        SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

        /* For SDF/MSDF batches, push fragment uniforms */
        if (batch->type == TEXT_BATCH_SDF || batch->type == TEXT_BATCH_MSDF) {
            SDFFragmentUniforms sdf_uniforms = {
                .params = {
                    batch->sdf_font->distance_range,
                    batch->sdf_scale,
                    batch->effects.weight,
                    0.5f  /* edge_threshold */
                },
                .outline_params = {
                    batch->effects.outline_width,
                    0.0f, 0.0f, 0.0f
                },
                .outline_color = {
                    batch->effects.outline_color[0],
                    batch->effects.outline_color[1],
                    batch->effects.outline_color[2],
                    batch->effects.outline_color[3]
                },
                .glow_params = {
                    batch->effects.glow_width,
                    0.0f, 0.0f, 0.0f
                },
                .glow_color = {
                    batch->effects.glow_color[0],
                    batch->effects.glow_color[1],
                    batch->effects.glow_color[2],
                    batch->effects.glow_color[3]
                },
                .flags = 0,
                ._padding = {0, 0, 0}
            };

            if (batch->effects.outline_enabled) sdf_uniforms.flags |= 1;
            if (batch->effects.glow_enabled) sdf_uniforms.flags |= 2;

            SDL_PushGPUFragmentUniformData(cmd, 0, &sdf_uniforms, sizeof(sdf_uniforms));
        }

        /* Bind atlas texture for this batch */
        SDL_GPUTextureSamplerBinding tex_binding = {
            .texture = batch->atlas_texture,
            .sampler = tr->sampler
        };
        SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

        /* Draw this batch
         * Note: We use vertex_offset to shift index references to the correct vertices.
         * The indices are pre-generated as 0,1,2,0,2,3 for glyph 0, 4,5,6,4,6,7 for glyph 1, etc.
         * So first_index should be batch->index_offset, and vertex_offset should be 0
         * because indices already reference absolute vertex positions.
         */
        SDL_DrawGPUIndexedPrimitives(pass, batch->index_count, 1,
                                      batch->index_offset, 0, 0);
    }

    /* Reset batch queue for next frame */
    tr->queued_batch_count = 0;
}

void carbon_text_end(Carbon_TextRenderer *tr)
{
    if (!tr) return;
    if (!tr->batch_started) return;

    tr->batch_started = false;

    /* Don't queue empty batches */
    uint32_t batch_vertex_count = tr->vertex_count - tr->current_batch_vertex_start;
    uint32_t batch_index_count = tr->index_count - tr->current_batch_index_start;
    if (batch_vertex_count == 0) return;

    /* Check if we have room in the queue */
    if (tr->queued_batch_count >= TEXT_MAX_QUEUED_BATCHES) {
        SDL_Log("Text: Batch queue full, batch dropped");
        return;
    }

    /* Queue this batch */
    QueuedTextBatch *batch = &tr->queued_batches[tr->queued_batch_count];
    batch->vertex_offset = tr->current_batch_vertex_start;
    batch->index_offset = tr->current_batch_index_start;
    batch->vertex_count = batch_vertex_count;
    batch->index_count = batch_index_count;

    if (tr->is_sdf_batch && tr->current_sdf_font) {
        batch->type = (tr->current_sdf_font->type == CARBON_SDF_TYPE_MSDF)
                      ? TEXT_BATCH_MSDF : TEXT_BATCH_SDF;
        batch->sdf_font = tr->current_sdf_font;
        batch->sdf_scale = tr->current_sdf_scale;
        batch->effects = tr->current_effects;
        batch->atlas_texture = tr->current_sdf_font->atlas_texture;
    } else if (tr->current_font) {
        batch->type = TEXT_BATCH_BITMAP;
        batch->atlas_texture = tr->current_font->atlas_texture;
        batch->sdf_font = NULL;
    } else {
        /* No font set - shouldn't happen but handle gracefully */
        return;
    }

    tr->queued_batch_count++;
}

/* ============================================================================
 * Formatted Text (printf-style)
 * ============================================================================ */

void carbon_text_printf(Carbon_TextRenderer *tr, Carbon_Font *font,
                        float x, float y,
                        const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    carbon_text_draw(tr, font, buffer, x, y);
}

void carbon_text_printf_colored(Carbon_TextRenderer *tr, Carbon_Font *font,
                                float x, float y,
                                float r, float g, float b, float a,
                                const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    carbon_text_draw_colored(tr, font, buffer, x, y, r, g, b, a);
}

/* ============================================================================
 * SDF/MSDF Font Functions
 * ============================================================================ */

/* stb_image already implemented in sprite.c */
#include "stb_image.h"

Carbon_SDFFont *carbon_sdf_font_load(Carbon_TextRenderer *tr,
                                      const char *atlas_path,
                                      const char *metrics_path)
{
    if (!tr || !atlas_path || !metrics_path) return NULL;

    /* Read JSON metrics file */
    SDL_IOStream *json_file = SDL_IOFromFile(metrics_path, "rb");
    if (!json_file) {
        carbon_set_error("Text: Failed to open SDF metrics file '%s': %s", metrics_path, SDL_GetError());
        return NULL;
    }

    Sint64 json_size = SDL_GetIOSize(json_file);
    if (json_size <= 0) {
        carbon_set_error("Text: Invalid metrics file size");
        SDL_CloseIO(json_file);
        return NULL;
    }

    char *json_data = malloc((size_t)json_size + 1);
    if (!json_data) {
        SDL_CloseIO(json_file);
        return NULL;
    }

    size_t read = SDL_ReadIO(json_file, json_data, (size_t)json_size);
    SDL_CloseIO(json_file);
    json_data[read] = '\0';

    /* Allocate font */
    Carbon_SDFFont *font = calloc(1, sizeof(Carbon_SDFFont));
    if (!font) {
        free(json_data);
        return NULL;
    }

    /* Set defaults */
    font->type = CARBON_SDF_TYPE_SDF;
    font->em_size = 1.0f;
    font->distance_range = 4.0f;
    font->font_size = 32.0f;
    font->line_height = 1.2f;
    font->ascender = 1.0f;
    font->descender = -0.2f;

    /* Parse JSON */
    if (!parse_sdf_json(json_data, font)) {
        carbon_set_error("Text: Failed to parse SDF JSON");
        free(json_data);
        free(font->glyphs);
        free(font);
        return NULL;
    }
    free(json_data);

    /* Load PNG atlas */
    int width, height, channels;
    unsigned char *pixels = stbi_load(atlas_path, &width, &height, &channels, 0);
    if (!pixels) {
        carbon_set_error("Text: Failed to load SDF atlas PNG '%s'", atlas_path);
        free(font->glyphs);
        free(font);
        return NULL;
    }

    /* Verify atlas dimensions match JSON */
    if (font->atlas_width == 0) font->atlas_width = width;
    if (font->atlas_height == 0) font->atlas_height = height;

    /* Create GPU texture */
    SDL_GPUTextureFormat format;
    uint32_t bytes_per_pixel;
    if (font->type == CARBON_SDF_TYPE_MSDF || channels >= 3) {
        format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        bytes_per_pixel = 4;
    } else {
        format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        bytes_per_pixel = 1;
    }

    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = format,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = (Uint32)width,
        .height = (Uint32)height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0
    };

    font->atlas_texture = SDL_CreateGPUTexture(tr->gpu, &tex_info);
    if (!font->atlas_texture) {
        carbon_set_error_from_sdl("Text: Failed to create SDF atlas texture");
        stbi_image_free(pixels);
        free(font->glyphs);
        free(font);
        return NULL;
    }

    /* Convert to target format if needed */
    unsigned char *upload_data = pixels;
    bool free_upload_data = false;

    if (bytes_per_pixel == 4 && channels < 4) {
        /* Need to expand to RGBA */
        upload_data = malloc(width * height * 4);
        free_upload_data = true;
        for (int i = 0; i < width * height; i++) {
            if (channels == 1) {
                upload_data[i*4 + 0] = pixels[i];
                upload_data[i*4 + 1] = pixels[i];
                upload_data[i*4 + 2] = pixels[i];
                upload_data[i*4 + 3] = 255;
            } else if (channels == 3) {
                upload_data[i*4 + 0] = pixels[i*3 + 0];
                upload_data[i*4 + 1] = pixels[i*3 + 1];
                upload_data[i*4 + 2] = pixels[i*3 + 2];
                upload_data[i*4 + 3] = 255;
            }
        }
    } else if (bytes_per_pixel == 1 && channels > 1) {
        /* Need to reduce to single channel */
        upload_data = malloc(width * height);
        free_upload_data = true;
        for (int i = 0; i < width * height; i++) {
            upload_data[i] = pixels[i * channels];
        }
    }

    /* Upload to GPU */
    uint32_t data_size = width * height * bytes_per_pixel;
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = data_size,
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(tr->gpu, &transfer_info);
    if (transfer) {
        void *mapped = SDL_MapGPUTransferBuffer(tr->gpu, transfer, false);
        if (mapped) {
            memcpy(mapped, upload_data, data_size);
            SDL_UnmapGPUTransferBuffer(tr->gpu, transfer);
        }

        SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(tr->gpu);
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
                    .texture = font->atlas_texture,
                    .mip_level = 0,
                    .layer = 0,
                    .x = 0, .y = 0, .z = 0,
                    .w = (Uint32)width,
                    .h = (Uint32)height,
                    .d = 1
                };
                SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
                SDL_EndGPUCopyPass(copy_pass);
            }
            SDL_SubmitGPUCommandBuffer(cmd);
        }
        SDL_ReleaseGPUTransferBuffer(tr->gpu, transfer);
    }

    if (free_upload_data) free(upload_data);
    stbi_image_free(pixels);

    SDL_Log("Text: Loaded %s font '%s' with %d glyphs (%dx%d atlas)",
            font->type == CARBON_SDF_TYPE_MSDF ? "MSDF" : "SDF",
            atlas_path, font->glyph_count, font->atlas_width, font->atlas_height);

    return font;
}

void carbon_sdf_font_destroy(Carbon_TextRenderer *tr, Carbon_SDFFont *font)
{
    if (!tr || !font) return;

    if (font->atlas_texture) {
        SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
    }
    free(font->glyphs);
    free(font);
}

Carbon_SDFFontType carbon_sdf_font_get_type(Carbon_SDFFont *font)
{
    return font ? font->type : CARBON_SDF_TYPE_SDF;
}

float carbon_sdf_font_get_size(Carbon_SDFFont *font)
{
    return font ? font->font_size : 0.0f;
}

float carbon_sdf_font_get_line_height(Carbon_SDFFont *font)
{
    return font ? font->line_height * font->font_size : 0.0f;
}

float carbon_sdf_font_get_ascent(Carbon_SDFFont *font)
{
    return font ? font->ascender * font->font_size : 0.0f;
}

float carbon_sdf_font_get_descent(Carbon_SDFFont *font)
{
    return font ? font->descender * font->font_size : 0.0f;
}

/* ============================================================================
 * SDF Text Drawing
 * ============================================================================ */

/* Find glyph by codepoint */
static SDFGlyphInfo *sdf_find_glyph(Carbon_SDFFont *font, uint32_t codepoint)
{
    for (int i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].codepoint == codepoint) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

void carbon_sdf_text_draw_ex(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                              const char *text, float x, float y,
                              float scale,
                              float r, float g, float b, float a,
                              Carbon_TextAlign align)
{
    if (!tr || !font || !text || !tr->batch_started) return;

    /* Warn if mixing bitmap and SDF fonts */
    if (tr->current_font && !tr->is_sdf_batch) {
        SDL_Log("Text: Warning - mixing bitmap and SDF fonts in batch");
    }
    if (tr->current_sdf_font && tr->current_sdf_font != font) {
        SDL_Log("Text: Warning - SDF font changed mid-batch");
    }

    tr->current_sdf_font = font;
    tr->is_sdf_batch = true;
    tr->current_sdf_scale = scale;

    /* Calculate pixel size */
    float px_size = font->font_size * scale;

    /* Handle alignment */
    float offset_x = 0.0f;
    if (align != CARBON_TEXT_ALIGN_LEFT) {
        float text_width = carbon_sdf_text_measure(font, text, scale);
        if (align == CARBON_TEXT_ALIGN_CENTER) {
            offset_x = -text_width / 2.0f;
        } else if (align == CARBON_TEXT_ALIGN_RIGHT) {
            offset_x = -text_width;
        }
    }

    float cursor_x = x + offset_x;
    float cursor_y = y;

    const char *p = text;
    while (*p) {
        unsigned char c = (unsigned char)*p;

        if (c == '\n') {
            cursor_x = x + offset_x;
            cursor_y += font->line_height * px_size;
            p++;
            continue;
        }

        SDFGlyphInfo *glyph = sdf_find_glyph(font, c);
        if (glyph) {
            /* Calculate screen position from plane bounds (em units) */
            float gx0 = cursor_x + glyph->plane_left * px_size;
            float gy0 = cursor_y - glyph->plane_top * px_size;  /* Y flipped for screen coords */
            float gx1 = cursor_x + glyph->plane_right * px_size;
            float gy1 = cursor_y - glyph->plane_bottom * px_size;

            /* Calculate UV coordinates from atlas bounds (pixels)
             * msdf-atlas-gen uses yOrigin: "bottom", so atlas_bottom is low Y, atlas_top is high Y.
             * In standard UV space (Y=0 at top), we need to flip: v = 1 - (atlas_y / height)
             * Since atlas_top > atlas_bottom, after flipping:
             *   v0 (top of glyph) = 1 - atlas_top/height  (smaller v)
             *   v1 (bottom of glyph) = 1 - atlas_bottom/height  (larger v)
             */
            float u0 = glyph->atlas_left / font->atlas_width;
            float v0 = 1.0f - glyph->atlas_top / font->atlas_height;
            float u1 = glyph->atlas_right / font->atlas_width;
            float v1 = 1.0f - glyph->atlas_bottom / font->atlas_height;

            /* Add glyph quad */
            text_add_glyph(tr, gx0, gy0, gx1, gy1, u0, v0, u1, v1, r, g, b, a);

            cursor_x += glyph->advance * px_size;
        }

        p++;
    }
}

void carbon_sdf_text_draw(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                          const char *text, float x, float y, float scale)
{
    carbon_sdf_text_draw_ex(tr, font, text, x, y, scale, 1.0f, 1.0f, 1.0f, 1.0f,
                            CARBON_TEXT_ALIGN_LEFT);
}

void carbon_sdf_text_draw_colored(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                                   const char *text, float x, float y, float scale,
                                   float r, float g, float b, float a)
{
    carbon_sdf_text_draw_ex(tr, font, text, x, y, scale, r, g, b, a,
                            CARBON_TEXT_ALIGN_LEFT);
}

void carbon_sdf_text_printf(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                            float x, float y, float scale,
                            const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    carbon_sdf_text_draw(tr, font, buffer, x, y, scale);
}

void carbon_sdf_text_printf_colored(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                                     float x, float y, float scale,
                                     float r, float g, float b, float a,
                                     const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    carbon_sdf_text_draw_colored(tr, font, buffer, x, y, scale, r, g, b, a);
}

/* ============================================================================
 * SDF Text Effects
 * ============================================================================ */

void carbon_sdf_text_set_effects(Carbon_TextRenderer *tr, const Carbon_TextEffects *effects)
{
    if (!tr || !effects) return;
    tr->current_effects = *effects;
}

void carbon_sdf_text_clear_effects(Carbon_TextRenderer *tr)
{
    if (!tr) return;
    memset(&tr->current_effects, 0, sizeof(Carbon_TextEffects));
}

void carbon_sdf_text_set_outline(Carbon_TextRenderer *tr, float width,
                                  float r, float g, float b, float a)
{
    if (!tr) return;
    tr->current_effects.outline_enabled = true;
    tr->current_effects.outline_width = width;
    tr->current_effects.outline_color[0] = r;
    tr->current_effects.outline_color[1] = g;
    tr->current_effects.outline_color[2] = b;
    tr->current_effects.outline_color[3] = a;
}

void carbon_sdf_text_set_shadow(Carbon_TextRenderer *tr,
                                 float offset_x, float offset_y, float softness,
                                 float r, float g, float b, float a)
{
    if (!tr) return;
    tr->current_effects.shadow_enabled = true;
    tr->current_effects.shadow_offset[0] = offset_x;
    tr->current_effects.shadow_offset[1] = offset_y;
    tr->current_effects.shadow_softness = softness;
    tr->current_effects.shadow_color[0] = r;
    tr->current_effects.shadow_color[1] = g;
    tr->current_effects.shadow_color[2] = b;
    tr->current_effects.shadow_color[3] = a;
}

void carbon_sdf_text_set_glow(Carbon_TextRenderer *tr, float width,
                               float r, float g, float b, float a)
{
    if (!tr) return;
    tr->current_effects.glow_enabled = true;
    tr->current_effects.glow_width = width;
    tr->current_effects.glow_color[0] = r;
    tr->current_effects.glow_color[1] = g;
    tr->current_effects.glow_color[2] = b;
    tr->current_effects.glow_color[3] = a;
}

void carbon_sdf_text_set_weight(Carbon_TextRenderer *tr, float weight)
{
    if (!tr) return;
    tr->current_effects.weight = weight;
}

/* ============================================================================
 * SDF Text Measurement
 * ============================================================================ */

float carbon_sdf_text_measure(Carbon_SDFFont *font, const char *text, float scale)
{
    if (!font || !text) return 0.0f;

    float px_size = font->font_size * scale;
    float width = 0.0f;
    const char *p = text;

    while (*p) {
        unsigned char c = (unsigned char)*p;
        SDFGlyphInfo *glyph = sdf_find_glyph(font, c);
        if (glyph) {
            width += glyph->advance * px_size;
        }
        p++;
    }

    return width;
}

void carbon_sdf_text_measure_bounds(Carbon_SDFFont *font, const char *text, float scale,
                                     float *out_width, float *out_height)
{
    if (!font || !text) {
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        return;
    }

    if (out_width) *out_width = carbon_sdf_text_measure(font, text, scale);
    if (out_height) *out_height = font->line_height * font->font_size * scale;
}
