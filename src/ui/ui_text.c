/*
 * Carbon UI - Text Rendering and Font Management
 */

#include "carbon/ui.h"
#include "carbon/error.h"

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Forward declaration from ui_draw.c */
extern void cui_draw_textured_quad(CUI_Context *ctx,
                                   float x0, float y0, float x1, float y1,
                                   float u0, float v0, float u1, float v1,
                                   uint32_t color);

/* ============================================================================
 * Font Loading
 * ============================================================================ */

bool cui_load_font(CUI_Context *ctx, const char *path, float size)
{
    if (!ctx || !ctx->gpu || !path) return false;

    /* Read font file */
    FILE *f = fopen(path, "rb");
    if (!f) {
        carbon_set_error("CUI: Cannot open font file '%s'", path);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *font_data = (unsigned char *)malloc(file_size);
    if (!font_data) {
        fclose(f);
        return false;
    }

    if (fread(font_data, 1, file_size, f) != (size_t)file_size) {
        free(font_data);
        fclose(f);
        return false;
    }
    fclose(f);

    /* Allocate glyph data */
    ctx->glyphs = (stbtt_bakedchar *)calloc(96, sizeof(stbtt_bakedchar));
    if (!ctx->glyphs) {
        free(font_data);
        return false;
    }

    /* Create font atlas bitmap */
    ctx->atlas_width = 512;
    ctx->atlas_height = 512;
    unsigned char *atlas_bitmap = (unsigned char *)calloc(1,
        ctx->atlas_width * ctx->atlas_height);
    if (!atlas_bitmap) {
        free(ctx->glyphs);
        ctx->glyphs = NULL;
        free(font_data);
        return false;
    }

    /* Bake font glyphs (ASCII 32-127) */
    int result = stbtt_BakeFontBitmap(font_data, 0, size,
                                       atlas_bitmap,
                                       ctx->atlas_width, ctx->atlas_height,
                                       32, 96,
                                       (stbtt_bakedchar *)ctx->glyphs);
    if (result <= 0) {
        carbon_set_error("CUI: Failed to bake font (result=%d)", result);
        free(atlas_bitmap);
        free(ctx->glyphs);
        ctx->glyphs = NULL;
        free(font_data);
        return false;
    }

    /* Get font metrics */
    stbtt_fontinfo font_info;
    if (stbtt_InitFont(&font_info, font_data, 0)) {
        int ascent, descent, line_gap;
        stbtt_GetFontVMetrics(&font_info, &ascent, &descent, &line_gap);
        float scale = stbtt_ScaleForPixelHeight(&font_info, size);
        ctx->ascent = ascent * scale;
        ctx->line_height = (ascent - descent + line_gap) * scale;
    } else {
        ctx->ascent = size * 0.8f;
        ctx->line_height = size * 1.2f;
    }
    ctx->font_size = size;

    free(font_data);

    /* Set pixel (0,0) to white for solid-color rectangles */
    atlas_bitmap[0] = 255;

    /* Create GPU texture for font atlas */
    SDL_GPUTextureCreateInfo tex_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8_UNORM,
        .width = (uint32_t)ctx->atlas_width,
        .height = (uint32_t)ctx->atlas_height,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER
    };
    ctx->font_atlas = SDL_CreateGPUTexture(ctx->gpu, &tex_info);
    if (!ctx->font_atlas) {
        carbon_set_error_from_sdl("CUI: Failed to create font atlas texture");
        free(atlas_bitmap);
        free(ctx->glyphs);
        ctx->glyphs = NULL;
        return false;
    }

    /* Upload atlas bitmap to GPU */
    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = (uint32_t)(ctx->atlas_width * ctx->atlas_height),
        .props = 0
    };
    SDL_GPUTransferBuffer *transfer = SDL_CreateGPUTransferBuffer(ctx->gpu,
                                                                   &transfer_info);
    if (transfer) {
        void *mapped = SDL_MapGPUTransferBuffer(ctx->gpu, transfer, false);
        if (mapped) {
            memcpy(mapped, atlas_bitmap, ctx->atlas_width * ctx->atlas_height);
            SDL_UnmapGPUTransferBuffer(ctx->gpu, transfer);

            /* Use a command buffer for the upload */
            SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(ctx->gpu);
            if (cmd) {
                SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd);
                if (copy_pass) {
                    SDL_GPUTextureTransferInfo src = {
                        .transfer_buffer = transfer,
                        .offset = 0,
                        .pixels_per_row = (uint32_t)ctx->atlas_width,
                        .rows_per_layer = (uint32_t)ctx->atlas_height
                    };
                    SDL_GPUTextureRegion dst = {
                        .texture = ctx->font_atlas,
                        .x = 0, .y = 0, .z = 0,
                        .w = (uint32_t)ctx->atlas_width,
                        .h = (uint32_t)ctx->atlas_height,
                        .d = 1
                    };
                    SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
                    SDL_EndGPUCopyPass(copy_pass);
                }
                SDL_SubmitGPUCommandBuffer(cmd);
            }
        }
        SDL_ReleaseGPUTransferBuffer(ctx->gpu, transfer);
    }

    free(atlas_bitmap);

    SDL_Log("CUI: Loaded font '%s' at %.0fpx", path, size);
    return true;
}

void cui_free_font(CUI_Context *ctx)
{
    if (!ctx) return;

    if (ctx->font_atlas) {
        SDL_ReleaseGPUTexture(ctx->gpu, ctx->font_atlas);
        ctx->font_atlas = NULL;
    }

    free(ctx->glyphs);
    ctx->glyphs = NULL;
}

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

float cui_text_width(CUI_Context *ctx, const char *text)
{
    if (!ctx || !ctx->glyphs || !text) return 0;

    float width = 0;
    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= 32 && c < 128) {
            stbtt_bakedchar *g = &((stbtt_bakedchar *)ctx->glyphs)[c - 32];
            width += g->xadvance;
        }
        text++;
    }
    return width;
}

float cui_text_height(CUI_Context *ctx)
{
    return ctx ? ctx->line_height : 0;
}

void cui_text_size(CUI_Context *ctx, const char *text, float *out_w, float *out_h)
{
    if (out_w) *out_w = cui_text_width(ctx, text);
    if (out_h) *out_h = cui_text_height(ctx);
}

/* ============================================================================
 * Text Rendering
 * ============================================================================ */

float cui_draw_text(CUI_Context *ctx, const char *text, float x, float y,
                    uint32_t color)
{
    if (!ctx || !ctx->glyphs || !text) return x;

    float start_x = x;
    float inv_w = 1.0f / ctx->atlas_width;
    float inv_h = 1.0f / ctx->atlas_height;

    /* Adjust y to baseline */
    y += ctx->ascent;

    while (*text) {
        unsigned char c = (unsigned char)*text;
        if (c >= 32 && c < 128) {
            stbtt_bakedchar *g = &((stbtt_bakedchar *)ctx->glyphs)[c - 32];

            float x0 = x + g->xoff;
            float y0 = y + g->yoff;
            float x1 = x0 + (g->x1 - g->x0);
            float y1 = y0 + (g->y1 - g->y0);

            float u0 = g->x0 * inv_w;
            float v0 = g->y0 * inv_h;
            float u1 = g->x1 * inv_w;
            float v1 = g->y1 * inv_h;

            cui_draw_textured_quad(ctx, x0, y0, x1, y1, u0, v0, u1, v1, color);

            x += g->xadvance;
        }
        text++;
    }

    return x - start_x;  /* Return width of rendered text */
}

void cui_draw_text_clipped(CUI_Context *ctx, const char *text,
                           CUI_Rect bounds, uint32_t color)
{
    if (!ctx || !text) return;

    /* For now, just draw at bounds position */
    /* TODO: Implement proper clipping */
    cui_push_scissor(ctx, bounds.x, bounds.y, bounds.w, bounds.h);
    cui_draw_text(ctx, text, bounds.x, bounds.y, color);
    cui_pop_scissor(ctx);
}
