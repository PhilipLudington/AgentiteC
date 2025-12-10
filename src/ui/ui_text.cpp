/*
 * Agentite UI - Text Rendering and Font Management
 *
 * Multi-font support with bitmap and SDF/MSDF fonts.
 * Uses the graphics text system for font loading and glyph data.
 */

#include "agentite/ui.h"
#include "agentite/text.h"
#include "agentite/error.h"

/* Include internal graphics text header for direct font access */
#include "../graphics/text_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ============================================================================
 * Internal Font Structure
 * ============================================================================ */

struct AUI_Font {
    AUI_FontType type;

    /* For bitmap fonts */
    Agentite_Font *bitmap_font;

    /* For SDF/MSDF fonts */
    Agentite_SDFFont *sdf_font;

    /* Cached metrics (common to all types) */
    float size;
    float line_height;
    float ascent;
    float descent;

    /* For tracking in registry */
    bool in_use;
};

/* Forward declaration from ui_draw.cpp */
extern void aui_draw_textured_quad_ex(AUI_Context *ctx,
                                       SDL_GPUTexture *texture,
                                       float x0, float y0, float x1, float y1,
                                       float u0, float v0, float u1, float v1,
                                       uint32_t color);

/* Forward declaration for SDF text drawing */
extern void aui_draw_sdf_quad(AUI_Context *ctx, AUI_Font *font,
                               float x0, float y0, float x1, float y1,
                               float u0, float v0, float u1, float v1,
                               uint32_t color, float scale);

/* ============================================================================
 * Font Loading - Bitmap
 * ============================================================================ */

/* Use agentite_font_load_memory from the graphics layer.
 * We need a temporary TextRenderer - create a minimal one for font loading. */
static Agentite_TextRenderer *g_aui_text_renderer = NULL;

static Agentite_TextRenderer *aui_get_text_renderer(SDL_GPUDevice *gpu, SDL_Window *window)
{
    if (!g_aui_text_renderer && gpu && window) {
        g_aui_text_renderer = agentite_text_init(gpu, window);
    }
    return g_aui_text_renderer;
}

static void aui_release_text_renderer(void)
{
    if (g_aui_text_renderer) {
        agentite_text_shutdown(g_aui_text_renderer);
        g_aui_text_renderer = NULL;
    }
}

AUI_Font *aui_font_load(AUI_Context *ctx, const char *path, float size)
{
    if (!ctx || !ctx->gpu || !path) return NULL;

    /* Find empty slot in font registry */
    int slot = -1;
    for (int i = 0; i < AUI_MAX_FONTS; i++) {
        if (ctx->fonts[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("CUI: Font registry full (max %d fonts)", AUI_MAX_FONTS);
        return NULL;
    }

    /* Get or create the shared text renderer for font loading */
    Agentite_TextRenderer *tr = aui_get_text_renderer(ctx->gpu, ctx->window);
    if (!tr) {
        agentite_set_error("CUI: Failed to create text renderer for font loading");
        return NULL;
    }

    /* Load bitmap font via the graphics layer */
    Agentite_Font *bitmap_font = agentite_font_load(tr, path, size);
    if (!bitmap_font) {
        return NULL;
    }

    /* Allocate AUI_Font wrapper */
    AUI_Font *font = (AUI_Font *)calloc(1, sizeof(AUI_Font));
    if (!font) {
        agentite_font_destroy(tr, bitmap_font);
        return NULL;
    }

    font->type = AUI_FONT_BITMAP;
    font->bitmap_font = bitmap_font;
    font->sdf_font = NULL;
    font->size = size;
    font->line_height = bitmap_font->line_height;
    font->ascent = bitmap_font->ascent;
    font->descent = bitmap_font->descent;
    font->in_use = true;

    /* Add to registry */
    ctx->fonts[slot] = font;
    ctx->font_count++;

    /* Set as default if first font */
    if (ctx->default_font == NULL) {
        ctx->default_font = font;
        ctx->current_font = font;

        /* Legacy compatibility */
        ctx->glyphs = bitmap_font->glyphs;
        ctx->font_atlas = bitmap_font->atlas_texture;
        ctx->font_size = font->size;
        ctx->line_height = font->line_height;
        ctx->ascent = font->ascent;
        ctx->atlas_width = ATLAS_SIZE;
        ctx->atlas_height = ATLAS_SIZE;
    }

    SDL_Log("CUI: Loaded bitmap font '%s' at %.0fpx (slot %d)", path, size, slot);
    return font;
}

/* ============================================================================
 * Font Loading - SDF/MSDF
 * ============================================================================ */

AUI_Font *aui_font_load_sdf(AUI_Context *ctx, const char *atlas_path,
                            const char *metrics_path)
{
    if (!ctx || !ctx->gpu || !atlas_path || !metrics_path) return NULL;

    /* Find empty slot in font registry */
    int slot = -1;
    for (int i = 0; i < AUI_MAX_FONTS; i++) {
        if (ctx->fonts[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("CUI: Font registry full (max %d fonts)", AUI_MAX_FONTS);
        return NULL;
    }

    /* Get or create the shared text renderer for font loading */
    Agentite_TextRenderer *tr = aui_get_text_renderer(ctx->gpu, ctx->window);
    if (!tr) {
        agentite_set_error("CUI: Failed to create text renderer for SDF font loading");
        return NULL;
    }

    /* Load SDF font via the graphics layer */
    Agentite_SDFFont *sdf_font = agentite_sdf_font_load(tr, atlas_path, metrics_path);
    if (!sdf_font) {
        return NULL;
    }

    /* Allocate AUI_Font wrapper */
    AUI_Font *font = (AUI_Font *)calloc(1, sizeof(AUI_Font));
    if (!font) {
        agentite_sdf_font_destroy(tr, sdf_font);
        return NULL;
    }

    font->type = (sdf_font->type == AGENTITE_SDF_TYPE_MSDF) ? AUI_FONT_MSDF : AUI_FONT_SDF;
    font->bitmap_font = NULL;
    font->sdf_font = sdf_font;
    font->size = sdf_font->font_size;
    font->line_height = sdf_font->line_height * sdf_font->font_size;
    font->ascent = sdf_font->ascender * sdf_font->font_size;
    font->descent = sdf_font->descender * sdf_font->font_size;
    font->in_use = true;

    /* Add to registry */
    ctx->fonts[slot] = font;
    ctx->font_count++;

    /* Set as default if first font */
    if (ctx->default_font == NULL) {
        ctx->default_font = font;
        ctx->current_font = font;
    }

    SDL_Log("CUI: Loaded %s font '%s' (slot %d)",
            font->type == AUI_FONT_MSDF ? "MSDF" : "SDF",
            atlas_path, slot);
    return font;
}

/* ============================================================================
 * Font Unloading
 * ============================================================================ */

void aui_font_unload(AUI_Context *ctx, AUI_Font *font)
{
    if (!ctx || !font) return;

    /* Find and remove from registry */
    for (int i = 0; i < AUI_MAX_FONTS; i++) {
        if (ctx->fonts[i] == font) {
            ctx->fonts[i] = NULL;
            ctx->font_count--;
            break;
        }
    }

    /* Clear default/current if this was it */
    if (ctx->default_font == font) {
        ctx->default_font = NULL;
        ctx->glyphs = NULL;
        ctx->font_atlas = NULL;

        /* Find another font to be default */
        for (int i = 0; i < AUI_MAX_FONTS; i++) {
            if (ctx->fonts[i]) {
                ctx->default_font = ctx->fonts[i];
                if (ctx->fonts[i]->type == AUI_FONT_BITMAP && ctx->fonts[i]->bitmap_font) {
                    ctx->glyphs = ctx->fonts[i]->bitmap_font->glyphs;
                    ctx->font_atlas = ctx->fonts[i]->bitmap_font->atlas_texture;
                }
                ctx->font_size = ctx->fonts[i]->size;
                ctx->line_height = ctx->fonts[i]->line_height;
                ctx->ascent = ctx->fonts[i]->ascent;
                break;
            }
        }
    }
    if (ctx->current_font == font) {
        ctx->current_font = ctx->default_font;
    }

    /* Release GPU resources */
    Agentite_TextRenderer *tr = aui_get_text_renderer(ctx->gpu, ctx->window);
    if (font->bitmap_font && tr) {
        agentite_font_destroy(tr, font->bitmap_font);
    }
    if (font->sdf_font && tr) {
        agentite_sdf_font_destroy(tr, font->sdf_font);
    }

    free(font);
}

/* Legacy API */
bool aui_load_font(AUI_Context *ctx, const char *path, float size)
{
    return aui_font_load(ctx, path, size) != NULL;
}

void aui_free_font(AUI_Context *ctx)
{
    if (!ctx) return;

    for (int i = 0; i < AUI_MAX_FONTS; i++) {
        if (ctx->fonts[i]) {
            aui_font_unload(ctx, ctx->fonts[i]);
        }
    }

    /* Release the shared text renderer */
    aui_release_text_renderer();
}

/* ============================================================================
 * Font Management
 * ============================================================================ */

AUI_FontType aui_font_get_type(AUI_Font *font)
{
    return font ? font->type : AUI_FONT_BITMAP;
}

void aui_set_default_font(AUI_Context *ctx, AUI_Font *font)
{
    if (!ctx || !font) return;
    ctx->default_font = font;

    /* Update legacy fields for bitmap fonts */
    if (font->type == AUI_FONT_BITMAP && font->bitmap_font) {
        ctx->glyphs = font->bitmap_font->glyphs;
        ctx->font_atlas = font->bitmap_font->atlas_texture;
        ctx->atlas_width = ATLAS_SIZE;
        ctx->atlas_height = ATLAS_SIZE;
    }
    ctx->font_size = font->size;
    ctx->line_height = font->line_height;
    ctx->ascent = font->ascent;
}

AUI_Font *aui_get_default_font(AUI_Context *ctx)
{
    return ctx ? ctx->default_font : NULL;
}

void aui_set_font(AUI_Context *ctx, AUI_Font *font)
{
    if (!ctx) return;
    ctx->current_font = font ? font : ctx->default_font;
}

AUI_Font *aui_get_font(AUI_Context *ctx)
{
    if (!ctx) return NULL;
    return ctx->current_font ? ctx->current_font : ctx->default_font;
}

float aui_font_size(AUI_Font *font)
{
    return font ? font->size : 0;
}

float aui_font_line_height(AUI_Font *font)
{
    return font ? font->line_height : 0;
}

float aui_font_ascent(AUI_Font *font)
{
    return font ? font->ascent : 0;
}

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

float aui_text_width_font(AUI_Font *font, const char *text)
{
    if (!font || !text) return 0;

    if (font->type == AUI_FONT_BITMAP && font->bitmap_font) {
        float width = 0;
        while (*text) {
            unsigned char c = (unsigned char)*text;
            if (c >= FIRST_CHAR && c <= LAST_CHAR) {
                GlyphInfo *g = &font->bitmap_font->glyphs[c - FIRST_CHAR];
                width += g->advance_x;
            }
            text++;
        }
        return width;
    } else if (font->sdf_font) {
        float width = 0;
        float scale = 1.0f;  /* Default scale */
        while (*text) {
            unsigned char c = (unsigned char)*text;
            SDFGlyphInfo *g = text_sdf_find_glyph(font->sdf_font, c);
            if (g) {
                width += g->advance * font->sdf_font->font_size * scale;
            }
            text++;
        }
        return width;
    }

    return 0;
}

float aui_text_height_font(AUI_Font *font)
{
    return font ? font->line_height : 0;
}

void aui_text_size_font(AUI_Font *font, const char *text, float *out_w, float *out_h)
{
    if (out_w) *out_w = aui_text_width_font(font, text);
    if (out_h) *out_h = aui_text_height_font(font);
}

float aui_text_width_font_scaled(AUI_Font *font, const char *text, float scale)
{
    return aui_text_width_font(font, text) * scale;
}

float aui_text_height_font_scaled(AUI_Font *font, float scale)
{
    return aui_text_height_font(font) * scale;
}

void aui_text_size_font_scaled(AUI_Font *font, const char *text, float scale,
                               float *out_w, float *out_h)
{
    if (out_w) *out_w = aui_text_width_font(font, text) * scale;
    if (out_h) *out_h = aui_text_height_font(font) * scale;
}

float aui_text_width(AUI_Context *ctx, const char *text)
{
    return aui_text_width_font(aui_get_font(ctx), text);
}

float aui_text_height(AUI_Context *ctx)
{
    return aui_text_height_font(aui_get_font(ctx));
}

void aui_text_size(AUI_Context *ctx, const char *text, float *out_w, float *out_h)
{
    aui_text_size_font(aui_get_font(ctx), text, out_w, out_h);
}

/* ============================================================================
 * Text Rendering - Bitmap
 * ============================================================================ */

static float aui_draw_bitmap_text(AUI_Context *ctx, AUI_Font *font, const char *text,
                                   float x, float y, float scale, uint32_t color)
{
    if (!ctx || !font || !font->bitmap_font || !text) return 0;

    Agentite_Font *bf = font->bitmap_font;
    float start_x = x;

    /* y is the top of the line box. Glyphs are positioned using their yoff
     * which is relative to the baseline. To convert:
     * - baseline = y + ascent
     * - glyph_top = baseline + yoff = y + ascent + yoff
     * Since g->y0 = yoff, glyph_top = y + ascent + g->y0
     */

    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= FIRST_CHAR && c <= LAST_CHAR) {
            GlyphInfo *g = &bf->glyphs[c - FIRST_CHAR];

            float x0 = x + g->x0 * scale;
            float y0 = y + (bf->ascent + g->y0) * scale;
            float x1 = x + g->x1 * scale;
            float y1 = y + (bf->ascent + g->y1) * scale;

            aui_draw_textured_quad_ex(ctx, bf->atlas_texture,
                                       x0, y0, x1, y1,
                                       g->u0, g->v0, g->u1, g->v1, color);

            x += g->advance_x * scale;
        }
        text++;
    }

    return x - start_x;
}

/* ============================================================================
 * Text Rendering - SDF/MSDF
 * ============================================================================ */

static float aui_draw_sdf_text_internal(AUI_Context *ctx, AUI_Font *font, const char *text,
                                         float x, float y, float scale, uint32_t color)
{
    if (!ctx || !font || !font->sdf_font || !text) return 0;

    Agentite_SDFFont *sf = font->sdf_font;
    float start_x = x;
    float font_scale = sf->font_size * scale;
    float inv_atlas_w = 1.0f / sf->atlas_width;
    float inv_atlas_h = 1.0f / sf->atlas_height;

    /* Adjust y to baseline */
    y += sf->ascender * font_scale;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        SDFGlyphInfo *g = text_sdf_find_glyph(sf, c);

        if (g) {
            /* Calculate screen position from em-space coordinates */
            float x0 = x + g->plane_left * font_scale;
            float y0 = y - g->plane_top * font_scale;
            float x1 = x + g->plane_right * font_scale;
            float y1 = y - g->plane_bottom * font_scale;

            /* Calculate texture coordinates */
            float u0 = g->atlas_left * inv_atlas_w;
            float v0 = g->atlas_top * inv_atlas_h;
            float u1 = g->atlas_right * inv_atlas_w;
            float v1 = g->atlas_bottom * inv_atlas_h;

            aui_draw_sdf_quad(ctx, font, x0, y0, x1, y1, u0, v0, u1, v1, color, scale);

            x += g->advance * font_scale;
        }
        text++;
    }

    return x - start_x;
}

/* ============================================================================
 * Text Rendering - Public API
 * ============================================================================ */

float aui_draw_text_font_scaled(AUI_Context *ctx, AUI_Font *font, const char *text,
                                float x, float y, float scale, uint32_t color)
{
    if (!ctx || !font || !text) return 0;

    if (font->type == AUI_FONT_BITMAP) {
        return aui_draw_bitmap_text(ctx, font, text, x, y, scale, color);
    } else {
        return aui_draw_sdf_text_internal(ctx, font, text, x, y, scale, color);
    }
}

float aui_draw_text_font(AUI_Context *ctx, AUI_Font *font, const char *text,
                         float x, float y, uint32_t color)
{
    return aui_draw_text_font_scaled(ctx, font, text, x, y, 1.0f, color);
}

void aui_draw_text_font_clipped(AUI_Context *ctx, AUI_Font *font, const char *text,
                                AUI_Rect bounds, uint32_t color)
{
    if (!ctx || !font || !text) return;

    aui_push_scissor(ctx, bounds.x, bounds.y, bounds.w, bounds.h);
    aui_draw_text_font(ctx, font, text, bounds.x, bounds.y, color);
    aui_pop_scissor(ctx);
}

float aui_draw_text_scaled(AUI_Context *ctx, const char *text, float x, float y,
                           float scale, uint32_t color)
{
    return aui_draw_text_font_scaled(ctx, aui_get_font(ctx), text, x, y, scale, color);
}

float aui_draw_text(AUI_Context *ctx, const char *text, float x, float y,
                    uint32_t color)
{
    return aui_draw_text_font_scaled(ctx, aui_get_font(ctx), text, x, y, 1.0f, color);
}

void aui_draw_text_clipped(AUI_Context *ctx, const char *text,
                           AUI_Rect bounds, uint32_t color)
{
    aui_draw_text_font_clipped(ctx, aui_get_font(ctx), text, bounds, color);
}
