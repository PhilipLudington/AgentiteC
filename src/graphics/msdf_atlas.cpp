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

    /* Calculate em size from font units */
    float scale = stbtt_ScaleForPixelHeight(&atlas->font, 1.0f);
    atlas->em_size = 1.0f / scale;
    atlas->ascender = ascent / atlas->em_size;
    atlas->descender = descent / atlas->em_size;
    atlas->line_height = (ascent - descent + line_gap) / atlas->em_size;

    /* Store configuration */
    atlas->atlas_width = config->atlas_width > 0 ? config->atlas_width : 1024;
    atlas->atlas_height = config->atlas_height > 0 ? config->atlas_height : 1024;
    atlas->glyph_scale = config->glyph_scale > 0 ? config->glyph_scale : 48.0f;
    atlas->pixel_range = config->pixel_range > 0 ? config->pixel_range : 4.0f;
    atlas->padding = config->padding > 0 ? config->padding : 2;
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

    glyph->advance = advance_width / atlas->em_size;
    glyph->left_bearing = left_bearing / atlas->em_size;

    /* Get glyph bounding box */
    int x0, y0, x1, y1;
    stbtt_GetGlyphBox(&atlas->font, glyph_index, &x0, &y0, &x1, &y1);

    glyph->plane_left = x0 / atlas->em_size;
    glyph->plane_bottom = y0 / atlas->em_size;
    glyph->plane_right = x1 / atlas->em_size;
    glyph->plane_top = y1 / atlas->em_size;

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

    /* Simple UTF-8 decoding for ASCII (extend for full UTF-8 if needed) */
    while (*str) {
        uint32_t codepoint = (uint8_t)*str++;
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

    /* Calculate glyph size in pixels */
    float scale = atlas->glyph_scale / atlas->em_size;
    float glyph_w = (glyph->plane_right - glyph->plane_left) * scale;
    float glyph_h = (glyph->plane_top - glyph->plane_bottom) * scale;

    /* Add padding for SDF range */
    int padding = (int)ceil(atlas->pixel_range) + atlas->padding;
    int bitmap_w = (int)ceil(glyph_w) + 2 * padding;
    int bitmap_h = (int)ceil(glyph_h) + 2 * padding;

    /* Minimum size for empty glyphs (like space) */
    if (bitmap_w < 4) bitmap_w = 4;
    if (bitmap_h < 4) bitmap_h = 4;

    glyph->atlas_w = bitmap_w;
    glyph->atlas_h = bitmap_h;

    /* Check if glyph has actual geometry */
    if (glyph->plane_left >= glyph->plane_right ||
        glyph->plane_bottom >= glyph->plane_top) {
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

    /* Extract shape from glyph */
    MSDF_Shape *shape = msdf_shape_from_glyph(&atlas->font, glyph->glyph_index, scale);
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

    /* Create projection */
    MSDF_Projection proj;
    proj.scale_x = scale;
    proj.scale_y = scale;
    proj.translate_x = padding - glyph->plane_left * scale;
    proj.translate_y = padding - glyph->plane_bottom * scale;

    /* Generate MSDF */
    switch (atlas->format) {
        case MSDF_BITMAP_GRAY:
            msdf_generate_sdf(shape, &glyph->bitmap, &proj, atlas->pixel_range);
            break;
        case MSDF_BITMAP_RGB:
            msdf_generate_msdf(shape, &glyph->bitmap, &proj, atlas->pixel_range);
            break;
        case MSDF_BITMAP_RGBA:
            msdf_generate_mtsdf(shape, &glyph->bitmap, &proj, atlas->pixel_range);
            break;
    }

    msdf_shape_free(shape);
    glyph->has_bitmap = true;

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
            if (!rects[i].was_packed) failed_count++;
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
    for (int i = 0; i < atlas->glyph_count; i++) {
        MSDF_AtlasGlyph *glyph = &atlas->glyphs[i];
        if (!glyph->has_bitmap) continue;

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

            /* Atlas UV coordinates (normalized 0-1) */
            float inv_w = 1.0f / atlas->atlas_width;
            float inv_h = 1.0f / atlas->atlas_height;

            out_info->atlas_left = g->atlas_x * inv_w;
            out_info->atlas_bottom = g->atlas_y * inv_h;
            out_info->atlas_right = (g->atlas_x + g->atlas_w) * inv_w;
            out_info->atlas_top = (g->atlas_y + g->atlas_h) * inv_h;

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
                switch (bmp->format) {
                    case MSDF_BITMAP_GRAY:
                        dst[0] = dst[1] = dst[2] = (unsigned char)(src[0] * 255.0f);
                        dst[3] = 255;
                        break;
                    case MSDF_BITMAP_RGB:
                        dst[0] = (unsigned char)(src[0] * 255.0f);
                        dst[1] = (unsigned char)(src[1] * 255.0f);
                        dst[2] = (unsigned char)(src[2] * 255.0f);
                        dst[3] = 255;
                        break;
                    case MSDF_BITMAP_RGBA:
                        dst[0] = (unsigned char)(src[0] * 255.0f);
                        dst[1] = (unsigned char)(src[1] * 255.0f);
                        dst[2] = (unsigned char)(src[2] * 255.0f);
                        dst[3] = (unsigned char)(src[3] * 255.0f);
                        break;
                }
            } else {
                dst[0] = dst[1] = dst[2] = dst[3] = 0;
            }
        }
    }

    return true;
}
