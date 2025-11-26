/*
 * Carbon Text Rendering System Implementation
 */

#include "carbon/text.h"
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

#define ATLAS_SIZE 1024                 /* Font atlas texture size */
#define FIRST_CHAR 32                   /* Space */
#define LAST_CHAR 126                   /* Tilde */
#define NUM_CHARS (LAST_CHAR - FIRST_CHAR + 1)

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

struct Carbon_TextRenderer {
    SDL_GPUDevice *gpu;
    SDL_Window *window;
    int screen_width;
    int screen_height;

    /* GPU resources */
    SDL_GPUGraphicsPipeline *pipeline;
    SDL_GPUBuffer *vertex_buffer;
    SDL_GPUBuffer *index_buffer;
    SDL_GPUSampler *sampler;

    /* CPU-side batch buffers */
    TextVertex *vertices;
    uint16_t *indices;
    uint32_t vertex_count;
    uint32_t index_count;
    uint32_t glyph_count;

    /* Current batch state */
    Carbon_Font *current_font;
    bool batch_started;
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
            SDL_Log("Text: Failed to create vertex shader: %s", SDL_GetError());
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
            SDL_Log("Text: Failed to create fragment shader: %s", SDL_GetError());
            SDL_ReleaseGPUShader(tr->gpu, vertex_shader);
            return false;
        }
    } else {
        SDL_Log("Text: No supported shader format (need MSL for Metal)");
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
        SDL_Log("Text: Failed to create graphics pipeline: %s", SDL_GetError());
        return false;
    }

    SDL_Log("Text: Graphics pipeline created successfully");
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
        SDL_Log("Text: Failed to create atlas texture: %s", SDL_GetError());
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
        SDL_Log("Text: Failed to create transfer buffer");
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
        SDL_Log("Text: Failed to allocate renderer");
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
        SDL_Log("Text: Failed to allocate batch buffers");
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
        SDL_Log("Text: Failed to create vertex buffer");
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
        SDL_Log("Text: Failed to create index buffer");
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
        SDL_Log("Text: Failed to create sampler");
        carbon_text_shutdown(tr);
        return NULL;
    }

    /* Create pipeline */
    if (!text_create_pipeline(tr)) {
        carbon_text_shutdown(tr);
        return NULL;
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
        SDL_Log("Text: Failed to open font file '%s': %s", path, SDL_GetError());
        return NULL;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        SDL_Log("Text: Invalid font file size");
        SDL_CloseIO(file);
        return NULL;
    }

    unsigned char *font_data = malloc((size_t)file_size);
    if (!font_data) {
        SDL_Log("Text: Failed to allocate font data buffer");
        SDL_CloseIO(file);
        return NULL;
    }

    size_t read = SDL_ReadIO(file, font_data, (size_t)file_size);
    SDL_CloseIO(file);

    if (read != (size_t)file_size) {
        SDL_Log("Text: Failed to read font file");
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
        SDL_Log("Text: Failed to initialize font");
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
        SDL_Log("Text: Failed to allocate atlas bitmap");
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
        SDL_Log("Text: Font atlas baking failed (too many chars or atlas too small)");
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

    tr->vertex_count = 0;
    tr->index_count = 0;
    tr->glyph_count = 0;
    tr->current_font = NULL;
    tr->batch_started = true;
}

static void text_add_glyph(Carbon_TextRenderer *tr,
                            float x0, float y0, float x1, float y1,
                            float u0, float v0, float u1, float v1,
                            float r, float g, float b, float a)
{
    if (tr->glyph_count >= TEXT_MAX_BATCH) {
        SDL_Log("Text: Batch overflow, glyph dropped");
        return;
    }

    uint32_t base = tr->glyph_count * 4;
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
    tr->vertex_count = tr->glyph_count * 4;
    tr->index_count = tr->glyph_count * 6;
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
    if (!tr || !cmd || tr->glyph_count == 0) return;

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
    if (!tr || !cmd || !pass || tr->glyph_count == 0 || !tr->current_font) return;

    /* Bind pipeline */
    SDL_BindGPUGraphicsPipeline(pass, tr->pipeline);

    /* Bind vertex buffer */
    SDL_GPUBufferBinding vb_binding = {
        .buffer = tr->vertex_buffer,
        .offset = 0
    };
    SDL_BindGPUVertexBuffers(pass, 0, &vb_binding, 1);

    /* Bind index buffer */
    SDL_GPUBufferBinding ib_binding = {
        .buffer = tr->index_buffer,
        .offset = 0
    };
    SDL_BindGPUIndexBuffer(pass, &ib_binding, SDL_GPU_INDEXELEMENTSIZE_16BIT);

    /* Build uniforms: mat4 view_projection + vec2 screen_size + vec2 padding */
    struct {
        float view_projection[16];
        float screen_size[2];
        float padding[2];
    } uniforms;

    /* Use orthographic projection for screen-space text */
    mat4 ortho;
    glm_ortho(0.0f, (float)tr->screen_width,
              (float)tr->screen_height, 0.0f,
              -1.0f, 1.0f, ortho);
    memcpy(uniforms.view_projection, ortho, sizeof(float) * 16);

    uniforms.screen_size[0] = (float)tr->screen_width;
    uniforms.screen_size[1] = (float)tr->screen_height;
    uniforms.padding[0] = 0.0f;
    uniforms.padding[1] = 0.0f;

    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    /* Bind font atlas texture */
    SDL_GPUTextureSamplerBinding tex_binding = {
        .texture = tr->current_font->atlas_texture,
        .sampler = tr->sampler
    };
    SDL_BindGPUFragmentSamplers(pass, 0, &tex_binding, 1);

    /* Draw */
    SDL_DrawGPUIndexedPrimitives(pass, tr->index_count, 1, 0, 0, 0);
}

void carbon_text_end(Carbon_TextRenderer *tr)
{
    if (!tr) return;
    tr->batch_started = false;
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
