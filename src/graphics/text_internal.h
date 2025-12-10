/*
 * Carbon Text Rendering System - Internal Header
 *
 * Shared types and declarations for text_*.cpp modules.
 * This header is NOT part of the public API.
 */

#ifndef CARBON_TEXT_INTERNAL_H
#define CARBON_TEXT_INTERNAL_H

#include "carbon/carbon.h"
#include "carbon/text.h"
#include "carbon/error.h"
#include <cglm/cglm.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/* STB rect_pack must be included BEFORE stb_truetype to ensure the proper
 * rectangle packing implementation is used (not the internal fallback) */
#include "stb_rect_pack.h"

/* STB TrueType for font rasterization */
#include "stb_truetype.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define TEXT_MAX_BATCH 2048             /* Max glyphs per batch */
#define TEXT_VERTS_PER_GLYPH 4
#define TEXT_INDICES_PER_GLYPH 6
#define TEXT_VERTEX_CAPACITY (TEXT_MAX_BATCH * TEXT_VERTS_PER_GLYPH)
#define TEXT_INDEX_CAPACITY (TEXT_MAX_BATCH * TEXT_INDICES_PER_GLYPH)
#define TEXT_MAX_QUEUED_BATCHES 64      /* Max batches that can be queued per frame */

#define ATLAS_SIZE 1024                 /* Font atlas texture size */
#define FIRST_CHAR 32                   /* Space */
#define LAST_CHAR 126                   /* Tilde */
#define NUM_CHARS (LAST_CHAR - FIRST_CHAR + 1)

/* SDF font constants */
#define SDF_MAX_GLYPHS 256              /* Max glyphs in SDF font */

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
 * Internal Functions (shared between modules)
 * ============================================================================ */

/* Create font atlas GPU texture from bitmap data */
SDL_GPUTexture *text_create_font_atlas(Carbon_TextRenderer *tr,
                                        unsigned char *atlas_bitmap);

/* Add a glyph quad to the current batch */
void text_add_glyph(Carbon_TextRenderer *tr,
                    float x0, float y0, float x1, float y1,
                    float u0, float v0, float u1, float v1,
                    float r, float g, float b, float a);

/* Parse SDF font JSON (msdf-atlas-gen format) */
bool text_parse_sdf_json(const char *json, Carbon_SDFFont *font);

/* Find glyph by codepoint in SDF font */
SDFGlyphInfo *text_sdf_find_glyph(Carbon_SDFFont *font, uint32_t codepoint);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_TEXT_INTERNAL_H */
