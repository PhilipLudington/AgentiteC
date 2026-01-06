/*
 * Carbon MSDF Atlas Generator
 *
 * Generates font texture atlases with MSDF glyphs at runtime.
 * Uses stb_rect_pack for efficient glyph packing.
 */

#include "agentite/msdf.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>
#include <SDL3/SDL.h>

/* stb libraries */
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#include "stb_truetype.h"

/* ============================================================================
 * Atlas Types (internal)
 * ============================================================================ */

/* Glyph data for atlas */
typedef struct MSDF_AtlasGlyph {
    uint32_t codepoint;
    int glyph_index;

    /* Metrics (in font units, scaled by atlas scale) */
    float advance;
    float left_bearing;

    /* Glyph bounds (em-space, relative to baseline) */
    float plane_left, plane_bottom;
    float plane_right, plane_top;

    /* Atlas UV coordinates (pixels) */
    int atlas_x, atlas_y;
    int atlas_w, atlas_h;

    /* Generated MSDF bitmap */
    MSDF_Bitmap bitmap;
    bool has_bitmap;
} MSDF_AtlasGlyph;

/* ============================================================================
 * Atlas Structure
 * ============================================================================ */

struct MSDF_Atlas {
    /* Font info */
    stbtt_fontinfo font;
    unsigned char *font_data;
    bool owns_font_data;

    /* Font metrics */
    float em_size;          /* Font em units */
    float ascender;         /* Ascent in em units */
    float descender;        /* Descent in em units */
    float line_height;      /* Line height in em units */

    /* Atlas configuration */
    int atlas_width;
    int atlas_height;
    float glyph_scale;      /* Scale for glyph rendering */
    float pixel_range;      /* SDF pixel range */
    int padding;            /* Padding around glyphs */
    MSDF_BitmapFormat format;

    /* Glyphs */
    MSDF_AtlasGlyph *glyphs;
    int glyph_count;
    int glyph_capacity;

    /* Output bitmap */
    MSDF_Bitmap atlas_bitmap;
    bool atlas_generated;
};

/* ============================================================================
 * Atlas Creation
 * ============================================================================ */

MSDF_Atlas *msdf_atlas_create(const MSDF_AtlasConfig *config)
{
    if (!config || !config->font_data || config->font_data_size <= 0) {
        agentite_set_error("Invalid atlas configuration");
        return NULL;
    }

    MSDF_Atlas *atlas = (MSDF_Atlas *)calloc(1, sizeof(MSDF_Atlas));
    if (!atlas) {
        agentite_set_error("Failed to allocate atlas");
        return NULL;
    }

    /* Copy font data if we don't own it */
    if (config->copy_font_data) {
        atlas->font_data = (unsigned char *)malloc(config->font_data_size);
        if (!atlas->font_data) {
            free(atlas);
            agentite_set_error("Failed to allocate font data");
            return NULL;
        }
        memcpy(atlas->font_data, config->font_data, config->font_data_size);
        atlas->owns_font_data = true;
    } else {
        atlas->font_data = (unsigned char *)config->font_data;
        atlas->owns_font_data = false;
    }

    /* Initialize font */
    if (!stbtt_InitFont(&atlas->font, atlas->font_data,
                        stbtt_GetFontOffsetForIndex(atlas->font_data, 0))) {
        if (atlas->owns_font_data) free(atlas->font_data);
        free(atlas);
        agentite_set_error("Failed to initialize font");
        return NULL;
    }

    /* Get font metrics */
    int ascent, descent, line_gap;
    stbtt_GetFontVMetrics(&atlas->font, &ascent, &descent, &line_gap);

    /* Calculate em size from font's unitsPerEm (typically 2048 or 1000)
     * This matches what msdf-atlas-gen uses for normalization */
    float scale_for_em = stbtt_ScaleForMappingEmToPixels(&atlas->font, 1.0f);
    atlas->em_size = 1.0f / scale_for_em;  /* This gives unitsPerEm */
    atlas->ascender = ascent / atlas->em_size;
    atlas->descender = descent / atlas->em_size;
    atlas->line_height = (ascent - descent + line_gap) / atlas->em_size;

    /* Store configuration - use improved defaults from msdf.h */
    atlas->atlas_width = config->atlas_width > 0 ? config->atlas_width : 1024;
    atlas->atlas_height = config->atlas_height > 0 ? config->atlas_height : 1024;
    atlas->glyph_scale = config->glyph_scale > 0 ? config->glyph_scale : (float)MSDF_DEFAULT_GLYPH_SCALE;
    atlas->pixel_range = config->pixel_range > 0 ? config->pixel_range : (float)MSDF_DEFAULT_PIXEL_RANGE;
    atlas->padding = config->padding > 0 ? config->padding : MSDF_DEFAULT_PADDING;
    atlas->format = config->format != 0 ? config->format : MSDF_BITMAP_RGB;

    /* Initialize glyph array */
    atlas->glyphs = NULL;
    atlas->glyph_count = 0;
    atlas->glyph_capacity = 0;

    atlas->atlas_generated = false;

    return atlas;
}

void msdf_atlas_destroy(MSDF_Atlas *atlas)
{
    if (!atlas) return;

    /* Free glyph bitmaps */
    for (int i = 0; i < atlas->glyph_count; i++) {
        if (atlas->glyphs[i].has_bitmap) {
            msdf_bitmap_free(&atlas->glyphs[i].bitmap);
        }
    }
    free(atlas->glyphs);

    /* Free atlas bitmap */
    msdf_bitmap_free(&atlas->atlas_bitmap);

    /* Free font data if owned */
    if (atlas->owns_font_data && atlas->font_data) {
        free(atlas->font_data);
    }

    free(atlas);
}

/* ============================================================================
 * Glyph Management
 * ============================================================================ */

static bool atlas_grow_glyphs(MSDF_Atlas *atlas)
{
    int new_capacity = atlas->glyph_capacity == 0 ? 128 : atlas->glyph_capacity * 2;
    MSDF_AtlasGlyph *new_glyphs = (MSDF_AtlasGlyph *)realloc(
        atlas->glyphs, new_capacity * sizeof(MSDF_AtlasGlyph));

    if (!new_glyphs) {
        agentite_set_error("Failed to grow glyph array");
        return false;
    }

    atlas->glyphs = new_glyphs;
    atlas->glyph_capacity = new_capacity;
    return true;
}

bool msdf_atlas_add_codepoint(MSDF_Atlas *atlas, uint32_t codepoint)
{
    if (!atlas) return false;

    /* Check if already added */
    for (int i = 0; i < atlas->glyph_count; i++) {
        if (atlas->glyphs[i].codepoint == codepoint) {
            return true; /* Already present */
        }
    }

    /* Grow array if needed */
    if (atlas->glyph_count >= atlas->glyph_capacity) {
        if (!atlas_grow_glyphs(atlas)) return false;
    }

    /* Get glyph index */
    int glyph_index = stbtt_FindGlyphIndex(&atlas->font, codepoint);

    /* Initialize glyph entry */
    MSDF_AtlasGlyph *glyph = &atlas->glyphs[atlas->glyph_count];
    memset(glyph, 0, sizeof(MSDF_AtlasGlyph));

    glyph->codepoint = codepoint;
    glyph->glyph_index = glyph_index;
    glyph->has_bitmap = false;

    /* Get glyph metrics */
    int advance_width, left_bearing;
    stbtt_GetGlyphHMetrics(&atlas->font, glyph_index, &advance_width, &left_bearing);

    glyph->advance = (float)advance_width / atlas->em_size;
    glyph->left_bearing = (float)left_bearing / atlas->em_size;

    /* Get glyph bounding box
     * Note: stbtt_GetGlyphBox may not write values for glyphs without outlines (e.g., space)
     * Initialize to 0 to handle this case */
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBox(&atlas->font, glyph_index, &x0, &y0, &x1, &y1);

    /* Convert to em-normalized coordinates
     * Note: We store raw glyph bounds here. The SDF range expansion is handled
     * during MSDF generation (via bitmap padding) and added to plane bounds
     * after generation for correct rendering. */
    glyph->plane_left = (float)x0 / atlas->em_size;
    glyph->plane_bottom = (float)y0 / atlas->em_size;
    glyph->plane_right = (float)x1 / atlas->em_size;
    glyph->plane_top = (float)y1 / atlas->em_size;

    atlas->glyph_count++;
    atlas->atlas_generated = false; /* Need to regenerate */

    return true;
}

bool msdf_atlas_add_ascii(MSDF_Atlas *atlas)
{
    if (!atlas) return false;

    /* Add printable ASCII characters (32-126) */
    for (uint32_t c = 32; c <= 126; c++) {
        if (!msdf_atlas_add_codepoint(atlas, c)) {
            return false;
        }
    }
    return true;
}

bool msdf_atlas_add_range(MSDF_Atlas *atlas, uint32_t first, uint32_t last)
{
    if (!atlas || first > last) return false;

    for (uint32_t c = first; c <= last; c++) {
        if (!msdf_atlas_add_codepoint(atlas, c)) {
            return false;
        }
    }
    return true;
}

bool msdf_atlas_add_string(MSDF_Atlas *atlas, const char *str)
{
    if (!atlas || !str) return false;

    /* Full UTF-8 decoding */
    const unsigned char *p = (const unsigned char *)str;
    while (*p) {
        uint32_t codepoint;
        unsigned char c = *p++;

        if (c < 0x80) {
            /* 1-byte ASCII: 0xxxxxxx */
            codepoint = c;
        } else if ((c & 0xE0) == 0xC0) {
            /* 2-byte sequence: 110xxxxx 10xxxxxx */
            if ((*p & 0xC0) != 0x80) continue; /* Invalid continuation */
            codepoint = ((c & 0x1F) << 6) | (*p++ & 0x3F);
        } else if ((c & 0xF0) == 0xE0) {
            /* 3-byte sequence: 1110xxxx 10xxxxxx 10xxxxxx (includes CJK) */
            if ((*p & 0xC0) != 0x80) continue;
            codepoint = ((c & 0x0F) << 12) | ((*p++ & 0x3F) << 6);
            if ((*p & 0xC0) != 0x80) continue;
            codepoint |= (*p++ & 0x3F);
        } else if ((c & 0xF8) == 0xF0) {
            /* 4-byte sequence: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx */
            if ((*p & 0xC0) != 0x80) continue;
            codepoint = ((c & 0x07) << 18) | ((*p++ & 0x3F) << 12);
            if ((*p & 0xC0) != 0x80) continue;
            codepoint |= ((*p++ & 0x3F) << 6);
            if ((*p & 0xC0) != 0x80) continue;
            codepoint |= (*p++ & 0x3F);
        } else {
            /* Invalid UTF-8 start byte, skip */
            continue;
        }

        if (!msdf_atlas_add_codepoint(atlas, codepoint)) {
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * MSDF Generation for Individual Glyphs
 * ============================================================================ */

static bool generate_glyph_msdf(MSDF_Atlas *atlas, MSDF_AtlasGlyph *glyph)
{
    if (glyph->has_bitmap) return true; /* Already generated */

    /* Calculate glyph size in pixels
     * Plane bounds are em-normalized (divided by em_size in add_codepoint),
     * so multiply by glyph_scale to get pixel dimensions */
    float pixel_scale = atlas->glyph_scale;
    float glyph_w = (glyph->plane_right - glyph->plane_left) * pixel_scale;
    float glyph_h = (glyph->plane_top - glyph->plane_bottom) * pixel_scale;

    /* Add padding for SDF range */
    int padding = (int)ceil(atlas->pixel_range) + atlas->padding;
    int bitmap_w = (int)ceil(glyph_w) + 2 * padding;
    int bitmap_h = (int)ceil(glyph_h) + 2 * padding;

    /* Minimum size for empty glyphs (like space) */
    if (bitmap_w < 4) bitmap_w = 4;
    if (bitmap_h < 4) bitmap_h = 4;

    glyph->atlas_w = bitmap_w;
    glyph->atlas_h = bitmap_h;

    /* Check if glyph has actual geometry (using raw bounds, not expanded) */
    int raw_x0 = 0, raw_y0 = 0, raw_x1 = 0, raw_y1 = 0;
    stbtt_GetGlyphBox(&atlas->font, glyph->glyph_index, &raw_x0, &raw_y0, &raw_x1, &raw_y1);
    if (raw_x0 >= raw_x1 || raw_y0 >= raw_y1) {
        /* Empty glyph (e.g., space) - create empty bitmap */
        if (!msdf_bitmap_alloc(&glyph->bitmap, bitmap_w, bitmap_h, atlas->format)) {
            return false;
        }
        /* Fill with "outside" value (0.0 for SDF, which maps to < 0.5) */
        for (int i = 0; i < bitmap_w * bitmap_h * atlas->format; i++) {
            glyph->bitmap.data[i] = 0.0f;
        }
        glyph->has_bitmap = true;
        return true;
    }

    /* Scale for converting font units to pixels (for shape extraction)
     * This is different from pixel_scale because shape uses raw font units */
    float shape_scale = atlas->glyph_scale / atlas->em_size;

    /* Extract shape from glyph */
    MSDF_Shape *shape = msdf_shape_from_glyph(&atlas->font, glyph->glyph_index, shape_scale);
    if (!shape) {
        agentite_set_error("Failed to extract glyph shape");
        return false;
    }

    /* Apply edge coloring */
    msdf_edge_coloring_simple(shape, MSDF_DEFAULT_ANGLE_THRESHOLD, glyph->codepoint);

    /* Allocate bitmap */
    if (!msdf_bitmap_alloc(&glyph->bitmap, bitmap_w, bitmap_h, atlas->format)) {
        msdf_shape_free(shape);
        return false;
    }

    /* Create projection
     * The shape coordinates are in pixels (scaled by shape_scale).
     * Font shapes use Y-up (Y=0 at baseline, positive Y goes up),
     * but bitmap uses Y-down (Y=0 at top, positive Y goes down).
     * We need to flip Y: bitmap_y = bitmap_h - shape_y
     *
     * For a shape point at (shape_x, shape_y):
     *   bitmap_x = shape_x - plane_left*scale + padding
     *   bitmap_y = bitmap_h - (shape_y - plane_bottom*scale + padding)
     *            = bitmap_h - padding - shape_y + plane_bottom*scale
     *
     * Using negative scale_y achieves the flip:
     *   unproject gives: shape_y = (bitmap_y - translate_y) / scale_y
     *   With scale_y = -1 and translate_y = bitmap_h - padding + plane_bottom*scale:
     *   shape_y = (bitmap_y - (bitmap_h - padding + plane_bottom*scale)) / (-1)
     *           = -bitmap_y + bitmap_h - padding + plane_bottom*scale
     */
    MSDF_Projection proj;
    proj.scale_x = 1.0;
    proj.scale_y = -1.0;  /* Flip Y for bitmap coordinates */
    proj.translate_x = padding - glyph->plane_left * pixel_scale;
    proj.translate_y = bitmap_h - padding + glyph->plane_bottom * pixel_scale;

    /* Generate MSDF with error correction */
    MSDF_GeneratorConfig gen_config = MSDF_GENERATOR_CONFIG_DEFAULT;
    gen_config.error_correction.mode = MSDF_ERROR_CORRECTION_EDGE_PRIORITY;
    gen_config.error_correction.min_deviation_ratio = 1.11;
    gen_config.error_correction.min_improve_ratio = 1.11;

    msdf_generate_ex(shape, &glyph->bitmap, &proj, atlas->pixel_range, &gen_config);

    msdf_shape_free(shape);
    glyph->has_bitmap = true;

    /* Expand plane bounds to include SDF padding region for correct rendering.
     * The bitmap includes 'padding' pixels on each side for SDF bleed,
     * so the plane bounds (used for screen quad positioning) must be expanded
     * to match the full bitmap region. */
    float padding_em = (float)padding / pixel_scale;
    glyph->plane_left -= padding_em;
    glyph->plane_bottom -= padding_em;
    glyph->plane_right += padding_em;
    glyph->plane_top += padding_em;

    return true;
}

/* ============================================================================
 * Atlas Packing
 * ============================================================================ */

bool msdf_atlas_generate(MSDF_Atlas *atlas)
{
    if (!atlas) return false;
    if (atlas->glyph_count == 0) {
        agentite_set_error("No glyphs to pack");
        return false;
    }

    /* Generate MSDF for each glyph */
    for (int i = 0; i < atlas->glyph_count; i++) {
        if (!generate_glyph_msdf(atlas, &atlas->glyphs[i])) {
            return false;
        }
    }

    /* Prepare rectangles for packing */
    stbrp_rect *rects = (stbrp_rect *)malloc(atlas->glyph_count * sizeof(stbrp_rect));
    if (!rects) {
        agentite_set_error("Failed to allocate packing rects");
        return false;
    }

    for (int i = 0; i < atlas->glyph_count; i++) {
        rects[i].id = i;
        rects[i].w = atlas->glyphs[i].atlas_w;
        rects[i].h = atlas->glyphs[i].atlas_h;
    }

    /* Initialize packer */
    stbrp_context ctx;
    int num_nodes = atlas->atlas_width;
    stbrp_node *nodes = (stbrp_node *)malloc(num_nodes * sizeof(stbrp_node));
    if (!nodes) {
        free(rects);
        agentite_set_error("Failed to allocate packing nodes");
        return false;
    }

    stbrp_init_target(&ctx, atlas->atlas_width, atlas->atlas_height, nodes, num_nodes);

    /* Pack rectangles */
    int pack_result = stbrp_pack_rects(&ctx, rects, atlas->glyph_count);
    if (!pack_result) {
        /* Not all glyphs fit - log which ones failed */
        int failed_count = 0;
        for (int i = 0; i < atlas->glyph_count; i++) {
            if (!rects[i].was_packed) {
                failed_count++;
            }
        }
        free(nodes);
        free(rects);
        agentite_set_error("Atlas too small: %d glyphs did not fit", failed_count);
        return false;
    }

    /* Store packed positions */
    for (int i = 0; i < atlas->glyph_count; i++) {
        int glyph_idx = rects[i].id;
        atlas->glyphs[glyph_idx].atlas_x = rects[i].x;
        atlas->glyphs[glyph_idx].atlas_y = rects[i].y;
    }

    free(nodes);
    free(rects);

    /* Allocate atlas bitmap */
    msdf_bitmap_free(&atlas->atlas_bitmap);
    if (!msdf_bitmap_alloc(&atlas->atlas_bitmap, atlas->atlas_width, atlas->atlas_height,
                           atlas->format)) {
        return false;
    }

    /* Clear atlas to black/outside */
    memset(atlas->atlas_bitmap.data, 0,
           atlas->atlas_width * atlas->atlas_height * atlas->format * sizeof(float));

    /* Copy glyph bitmaps into atlas */
    int copied = 0, skipped = 0;
    for (int i = 0; i < atlas->glyph_count; i++) {
        MSDF_AtlasGlyph *glyph = &atlas->glyphs[i];
        if (!glyph->has_bitmap) {
            skipped++;
            continue;
        }
        copied++;

        int dst_x = glyph->atlas_x;
        int dst_y = glyph->atlas_y;

        for (int y = 0; y < glyph->atlas_h; y++) {
            for (int x = 0; x < glyph->atlas_w; x++) {
                const float *src = msdf_bitmap_pixel_const(&glyph->bitmap, x, y);
                float *dst = msdf_bitmap_pixel(&atlas->atlas_bitmap, dst_x + x, dst_y + y);
                if (src && dst) {
                    for (int c = 0; c < atlas->format; c++) {
                        dst[c] = src[c];
                    }
                }
            }
        }
    }

    SDL_Log("MSDF Atlas: Copied %d glyphs, skipped %d without bitmaps", copied, skipped);
    atlas->atlas_generated = true;
    return true;
}

/* ============================================================================
 * Atlas Query
 * ============================================================================ */

bool msdf_atlas_get_glyph(const MSDF_Atlas *atlas, uint32_t codepoint,
                          MSDF_GlyphInfo *out_info)
{
    if (!atlas || !out_info) return false;

    for (int i = 0; i < atlas->glyph_count; i++) {
        if (atlas->glyphs[i].codepoint == codepoint) {
            MSDF_AtlasGlyph *g = &atlas->glyphs[i];

            out_info->codepoint = g->codepoint;
            out_info->advance = g->advance;

            /* Plane bounds (em units) */
            out_info->plane_left = g->plane_left;
            out_info->plane_bottom = g->plane_bottom;
            out_info->plane_right = g->plane_right;
            out_info->plane_top = g->plane_top;

            /* Atlas UV coordinates (normalized 0-1)
             * The atlas bitmap is stored Y-down (row 0 is top), same as SDL_GPU textures.
             * So no Y-flip is needed - just convert pixel coords to normalized UVs.
             * atlas_bottom/top naming follows msdf convention where bottom < top in Y value,
             * but in our Y-down coords, atlas_bottom is the TOP edge (lower v) and
             * atlas_top is the BOTTOM edge (higher v). */
            float inv_w = 1.0f / atlas->atlas_width;
            float inv_h = 1.0f / atlas->atlas_height;

            out_info->atlas_left = g->atlas_x * inv_w;
            out_info->atlas_right = (g->atlas_x + g->atlas_w) * inv_w;
            /* Y-down coords: atlas_y is top of glyph, atlas_y + atlas_h is bottom */
            out_info->atlas_bottom = g->atlas_y * inv_h;  /* Top edge in texture (lower v) */
            out_info->atlas_top = (g->atlas_y + g->atlas_h) * inv_h;  /* Bottom edge in texture (higher v) */

            return true;
        }
    }

    return false;
}

int msdf_atlas_get_glyph_count(const MSDF_Atlas *atlas)
{
    return atlas ? atlas->glyph_count : 0;
}

const MSDF_Bitmap *msdf_atlas_get_bitmap(const MSDF_Atlas *atlas)
{
    return (atlas && atlas->atlas_generated) ? &atlas->atlas_bitmap : NULL;
}

void msdf_atlas_get_metrics(const MSDF_Atlas *atlas, MSDF_FontMetrics *out_metrics)
{
    if (!atlas || !out_metrics) return;

    out_metrics->em_size = atlas->em_size;
    out_metrics->ascender = atlas->ascender;
    out_metrics->descender = atlas->descender;
    out_metrics->line_height = atlas->line_height;
    out_metrics->atlas_width = atlas->atlas_width;
    out_metrics->atlas_height = atlas->atlas_height;
}

/* ============================================================================
 * Atlas Export (for caching)
 * ============================================================================ */

bool msdf_atlas_get_bitmap_rgba8(const MSDF_Atlas *atlas, unsigned char *out_data)
{
    if (!atlas || !atlas->atlas_generated || !out_data) return false;

    const MSDF_Bitmap *bmp = &atlas->atlas_bitmap;

    for (int y = 0; y < bmp->height; y++) {
        for (int x = 0; x < bmp->width; x++) {
            const float *src = msdf_bitmap_pixel_const(bmp, x, y);
            unsigned char *dst = &out_data[(y * bmp->width + x) * 4];

            if (src) {
                /* Helper to clamp and convert float [0,1] to uint8 [0,255] */
                #define FLOAT_TO_UINT8(f) ((unsigned char)(fminf(fmaxf((f), 0.0f), 1.0f) * 255.0f + 0.5f))
                switch (bmp->format) {
                    case MSDF_BITMAP_GRAY:
                        dst[0] = dst[1] = dst[2] = FLOAT_TO_UINT8(src[0]);
                        dst[3] = 255;
                        break;
                    case MSDF_BITMAP_RGB:
                        dst[0] = FLOAT_TO_UINT8(src[0]);
                        dst[1] = FLOAT_TO_UINT8(src[1]);
                        dst[2] = FLOAT_TO_UINT8(src[2]);
                        dst[3] = 255;
                        break;
                    case MSDF_BITMAP_RGBA:
                        dst[0] = FLOAT_TO_UINT8(src[0]);
                        dst[1] = FLOAT_TO_UINT8(src[1]);
                        dst[2] = FLOAT_TO_UINT8(src[2]);
                        dst[3] = FLOAT_TO_UINT8(src[3]);
                        break;
                }
                #undef FLOAT_TO_UINT8
            } else {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            }
        }
    }

    return true;
}
