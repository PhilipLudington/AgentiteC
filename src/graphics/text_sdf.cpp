/*
 * Carbon Text SDF/MSDF Font Implementation
 *
 * Handles signed distance field font loading, rendering, effects, and measurement.
 */

#include "text_internal.h"

/* stb_image already implemented in sprite.c */
#include "stb_image.h"

/* ============================================================================
 * Internal: JSON Parser for msdf-atlas-gen format
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
bool text_parse_sdf_json(const char *json, Carbon_SDFFont *font) {
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

    font->glyphs = (SDFGlyphInfo*)malloc(count * sizeof(SDFGlyphInfo));
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
 * SDF/MSDF Font Functions
 * ============================================================================ */

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

    char *json_data = (char*)malloc((size_t)json_size + 1);
    if (!json_data) {
        SDL_CloseIO(json_file);
        return NULL;
    }

    size_t read = SDL_ReadIO(json_file, json_data, (size_t)json_size);
    SDL_CloseIO(json_file);
    json_data[read] = '\0';

    /* Allocate font */
    Carbon_SDFFont *font = CARBON_ALLOC(Carbon_SDFFont);
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
    if (!text_parse_sdf_json(json_data, font)) {
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
        upload_data = (unsigned char*)malloc(width * height * 4);
        if (!upload_data) {
            carbon_set_error("Text: Failed to allocate format conversion buffer");
            SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
            stbi_image_free(pixels);
            free(font->glyphs);
            free(font);
            return NULL;
        }
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
        upload_data = (unsigned char*)malloc(width * height);
        if (!upload_data) {
            carbon_set_error("Text: Failed to allocate format conversion buffer");
            SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
            stbi_image_free(pixels);
            free(font->glyphs);
            free(font);
            return NULL;
        }
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
SDFGlyphInfo *text_sdf_find_glyph(Carbon_SDFFont *font, uint32_t codepoint)
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

        SDFGlyphInfo *glyph = text_sdf_find_glyph(font, c);
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
        SDFGlyphInfo *glyph = text_sdf_find_glyph(font, c);
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
