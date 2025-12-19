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
bool text_parse_sdf_json(const char *json, Agentite_SDFFont *font) {
    /* Parse atlas section */
    const char *atlas = json_find_key(json, "atlas");
    if (atlas) {
        const char *type_val = json_find_key(atlas, "type");
        if (type_val) {
            char type_str[32];
            json_parse_string(type_val, type_str, sizeof(type_str));
            if (strcmp(type_str, "msdf") == 0 || strcmp(type_str, "mtsdf") == 0) {
                font->type = AGENTITE_SDF_TYPE_MSDF;
            } else {
                font->type = AGENTITE_SDF_TYPE_SDF;
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
        agentite_set_error("Text: No glyphs array in JSON");
        return false;
    }

    glyphs = json_skip_ws(glyphs);
    if (*glyphs != '[') {
        agentite_set_error("Text: Glyphs is not an array");
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

Agentite_SDFFont *agentite_sdf_font_load(Agentite_TextRenderer *tr,
                                      const char *atlas_path,
                                      const char *metrics_path)
{
    if (!tr || !atlas_path || !metrics_path) return NULL;

    /* Read JSON metrics file */
    SDL_IOStream *json_file = SDL_IOFromFile(metrics_path, "rb");
    if (!json_file) {
        agentite_set_error("Text: Failed to open SDF metrics file '%s': %s", metrics_path, SDL_GetError());
        return NULL;
    }

    Sint64 json_size = SDL_GetIOSize(json_file);
    if (json_size <= 0) {
        agentite_set_error("Text: Invalid metrics file size");
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
    Agentite_SDFFont *font = AGENTITE_ALLOC(Agentite_SDFFont);
    if (!font) {
        free(json_data);
        return NULL;
    }

    /* Set defaults */
    font->type = AGENTITE_SDF_TYPE_SDF;
    font->em_size = 1.0f;
    font->distance_range = 4.0f;
    font->font_size = 32.0f;
    font->line_height = 1.2f;
    font->ascender = 1.0f;
    font->descender = -0.2f;

    /* Parse JSON */
    if (!text_parse_sdf_json(json_data, font)) {
        agentite_set_error("Text: Failed to parse SDF JSON");
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
        agentite_set_error("Text: Failed to load SDF atlas PNG '%s'", atlas_path);
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
    if (font->type == AGENTITE_SDF_TYPE_MSDF || channels >= 3) {
        format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
        bytes_per_pixel = 4;
    } else {
        format = SDL_GPU_TEXTUREFORMAT_R8_UNORM;
        bytes_per_pixel = 1;
    }

    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = format;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.props = 0;

    font->atlas_texture = SDL_CreateGPUTexture(tr->gpu, &tex_info);
    if (!font->atlas_texture) {
        agentite_set_error_from_sdl("Text: Failed to create SDF atlas texture");
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
            agentite_set_error("Text: Failed to allocate format conversion buffer");
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
            agentite_set_error("Text: Failed to allocate format conversion buffer");
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
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = data_size;
    transfer_info.props = 0;
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
                SDL_GPUTextureTransferInfo src = {};
                src.transfer_buffer = transfer;
                src.offset = 0;
                src.pixels_per_row = (Uint32)width;
                src.rows_per_layer = (Uint32)height;
                SDL_GPUTextureRegion dst = {};
                dst.texture = font->atlas_texture;
                dst.mip_level = 0;
                dst.layer = 0;
                dst.x = 0;
                dst.y = 0;
                dst.z = 0;
                dst.w = (Uint32)width;
                dst.h = (Uint32)height;
                dst.d = 1;
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
            font->type == AGENTITE_SDF_TYPE_MSDF ? "MSDF" : "SDF",
            atlas_path, font->glyph_count, font->atlas_width, font->atlas_height);

    return font;
}

void agentite_sdf_font_destroy(Agentite_TextRenderer *tr, Agentite_SDFFont *font)
{
    if (!tr || !font) return;

    if (font->atlas_texture) {
        SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
    }
    free(font->glyphs);
    free(font);
}

/* ============================================================================
 * Runtime MSDF Font Generation
 * ============================================================================ */

#include "agentite/msdf.h"

Agentite_SDFFont *agentite_sdf_font_generate(Agentite_TextRenderer *tr,
                                          const char *ttf_path,
                                          const Agentite_SDFFontGenConfig *config)
{
    if (!tr || !ttf_path) return NULL;

    /* Use defaults if no config provided */
    Agentite_SDFFontGenConfig default_config = AGENTITE_SDF_FONT_GEN_CONFIG_DEFAULT;
    if (!config) config = &default_config;

    /* Load TTF file */
    SDL_IOStream *file = SDL_IOFromFile(ttf_path, "rb");
    if (!file) {
        agentite_set_error("Text: Failed to open font file '%s': %s", ttf_path, SDL_GetError());
        return NULL;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        agentite_set_error("Text: Invalid font file size");
        SDL_CloseIO(file);
        return NULL;
    }

    unsigned char *font_data = (unsigned char *)malloc((size_t)file_size);
    if (!font_data) {
        SDL_CloseIO(file);
        return NULL;
    }

    size_t read_size = SDL_ReadIO(file, font_data, (size_t)file_size);
    SDL_CloseIO(file);

    if ((Sint64)read_size != file_size) {
        free(font_data);
        agentite_set_error("Text: Failed to read font file");
        return NULL;
    }

    /* Create MSDF atlas */
    MSDF_AtlasConfig atlas_config = MSDF_ATLAS_CONFIG_DEFAULT;
    atlas_config.font_data = font_data;
    atlas_config.font_data_size = (int)file_size;
    atlas_config.copy_font_data = false; /* We'll keep our copy */
    atlas_config.atlas_width = config->atlas_width;
    atlas_config.atlas_height = config->atlas_height;
    atlas_config.glyph_scale = config->glyph_scale;
    atlas_config.pixel_range = config->pixel_range;
    atlas_config.format = config->generate_msdf ? MSDF_BITMAP_RGB : MSDF_BITMAP_GRAY;

    MSDF_Atlas *atlas = msdf_atlas_create(&atlas_config);
    if (!atlas) {
        free(font_data);
        return NULL;
    }

    /* Add characters based on config */
    if (config->charset && config->charset[0]) {
        msdf_atlas_add_string(atlas, config->charset);
    } else {
        msdf_atlas_add_ascii(atlas);
    }

    /* Generate atlas */
    if (!msdf_atlas_generate(atlas)) {
        SDL_Log("Text: Atlas generation failed: %s", agentite_get_last_error());
        msdf_atlas_destroy(atlas);
        free(font_data);
        return NULL;
    }

    /* Get atlas bitmap and metrics */
    const MSDF_Bitmap *bitmap = msdf_atlas_get_bitmap(atlas);
    MSDF_FontMetrics metrics;
    msdf_atlas_get_metrics(atlas, &metrics);

    /* Allocate Agentite_SDFFont */
    Agentite_SDFFont *font = AGENTITE_ALLOC(Agentite_SDFFont);
    if (!font) {
        msdf_atlas_destroy(atlas);
        free(font_data);
        return NULL;
    }

    /* Set font properties */
    font->type = config->generate_msdf ? AGENTITE_SDF_TYPE_MSDF : AGENTITE_SDF_TYPE_SDF;
    font->em_size = metrics.em_size;
    font->font_size = config->glyph_scale;
    font->distance_range = config->pixel_range;
    font->line_height = metrics.line_height;
    font->ascender = metrics.ascender;
    font->descender = metrics.descender;
    font->atlas_width = metrics.atlas_width;
    font->atlas_height = metrics.atlas_height;

    /* Copy glyph data */
    int glyph_count = msdf_atlas_get_glyph_count(atlas);
    font->glyphs = (SDFGlyphInfo *)calloc(glyph_count, sizeof(SDFGlyphInfo));
    if (!font->glyphs) {
        free(font);
        msdf_atlas_destroy(atlas);
        free(font_data);
        return NULL;
    }
    font->glyph_count = glyph_count;

    /* Convert MSDF glyph info to Agentite format */
    int valid_glyphs = 0;

    /* Determine iteration bounds based on charset source */
    int iter_count;
    if (config->charset && config->charset[0]) {
        /* Custom charset: iterate through string characters */
        iter_count = (int)strlen(config->charset);
    } else {
        /* ASCII charset: 95 printable characters (32-126) */
        iter_count = 95;
    }

    for (int i = 0; i < iter_count; i++) {
        uint32_t codepoint = (config->charset && config->charset[0])
            ? (uint8_t)config->charset[i]
            : (uint32_t)(32 + i);

        MSDF_GlyphInfo msdf_glyph;
        if (msdf_atlas_get_glyph(atlas, codepoint, &msdf_glyph)) {
            font->glyphs[valid_glyphs].codepoint = msdf_glyph.codepoint;
            font->glyphs[valid_glyphs].advance = msdf_glyph.advance;
            font->glyphs[valid_glyphs].plane_left = msdf_glyph.plane_left;
            font->glyphs[valid_glyphs].plane_bottom = msdf_glyph.plane_bottom;
            font->glyphs[valid_glyphs].plane_right = msdf_glyph.plane_right;
            font->glyphs[valid_glyphs].plane_top = msdf_glyph.plane_top;

            /* Convert normalized UV to pixel coordinates for compatibility */
            font->glyphs[valid_glyphs].atlas_left = msdf_glyph.atlas_left * metrics.atlas_width;
            font->glyphs[valid_glyphs].atlas_bottom = msdf_glyph.atlas_bottom * metrics.atlas_height;
            font->glyphs[valid_glyphs].atlas_right = msdf_glyph.atlas_right * metrics.atlas_width;
            font->glyphs[valid_glyphs].atlas_top = msdf_glyph.atlas_top * metrics.atlas_height;

            valid_glyphs++;
        }
    }
    font->glyph_count = valid_glyphs;

    /* Create GPU texture */
    SDL_GPUTextureFormat format = config->generate_msdf
        ? SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM
        : SDL_GPU_TEXTUREFORMAT_R8_UNORM;
    uint32_t bytes_per_pixel = config->generate_msdf ? 4 : 1;

    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = format;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    tex_info.width = (Uint32)metrics.atlas_width;
    tex_info.height = (Uint32)metrics.atlas_height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    tex_info.props = 0;

    font->atlas_texture = SDL_CreateGPUTexture(tr->gpu, &tex_info);
    if (!font->atlas_texture) {
        agentite_set_error_from_sdl("Text: Failed to create generated atlas texture");
        free(font->glyphs);
        free(font);
        msdf_atlas_destroy(atlas);
        free(font_data);
        return NULL;
    }

    /* Convert bitmap to upload format */
    uint32_t data_size = metrics.atlas_width * metrics.atlas_height * bytes_per_pixel;
    unsigned char *upload_data = (unsigned char *)malloc(data_size);
    if (!upload_data) {
        SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
        free(font->glyphs);
        free(font);
        msdf_atlas_destroy(atlas);
        free(font_data);
        return NULL;
    }

    if (config->generate_msdf) {
        msdf_atlas_get_bitmap_rgba8(atlas, upload_data);
    } else {
        /* Single channel - just convert float to uint8 */
        for (int i = 0; i < metrics.atlas_width * metrics.atlas_height; i++) {
            float v = bitmap->data[i];
            if (v < 0) v = 0;
            if (v > 1) v = 1;
            upload_data[i] = (unsigned char)(v * 255.0f);
        }
    }

    /* Upload to GPU */
    SDL_GPUTransferBufferCreateInfo transfer_info = {};
    transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transfer_info.size = data_size;
    transfer_info.props = 0;
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
                SDL_GPUTextureTransferInfo src = {};
                src.transfer_buffer = transfer;
                src.offset = 0;
                src.pixels_per_row = (Uint32)metrics.atlas_width;
                src.rows_per_layer = (Uint32)metrics.atlas_height;
                SDL_GPUTextureRegion dst = {};
                dst.texture = font->atlas_texture;
                dst.mip_level = 0;
                dst.layer = 0;
                dst.x = 0;
                dst.y = 0;
                dst.z = 0;
                dst.w = (Uint32)metrics.atlas_width;
                dst.h = (Uint32)metrics.atlas_height;
                dst.d = 1;
                SDL_UploadToGPUTexture(copy_pass, &src, &dst, false);
                SDL_EndGPUCopyPass(copy_pass);
            }
            SDL_SubmitGPUCommandBuffer(cmd);
        }
        SDL_ReleaseGPUTransferBuffer(tr->gpu, transfer);
    }

    free(upload_data);
    msdf_atlas_destroy(atlas);
    free(font_data);

    SDL_Log("Text: Generated %s font from '%s' with %d glyphs (%dx%d atlas)",
            font->type == AGENTITE_SDF_TYPE_MSDF ? "MSDF" : "SDF",
            ttf_path, font->glyph_count, font->atlas_width, font->atlas_height);

    return font;
}

Agentite_SDFFontType agentite_sdf_font_get_type(Agentite_SDFFont *font)
{
    return font ? font->type : AGENTITE_SDF_TYPE_SDF;
}

float agentite_sdf_font_get_size(Agentite_SDFFont *font)
{
    return font ? font->font_size : 0.0f;
}

float agentite_sdf_font_get_line_height(Agentite_SDFFont *font)
{
    return font ? font->line_height * font->font_size : 0.0f;
}

float agentite_sdf_font_get_ascent(Agentite_SDFFont *font)
{
    return font ? font->ascender * font->font_size : 0.0f;
}

float agentite_sdf_font_get_descent(Agentite_SDFFont *font)
{
    return font ? font->descender * font->font_size : 0.0f;
}

/* ============================================================================
 * SDF Text Drawing
 * ============================================================================ */

/* Find glyph by codepoint */
SDFGlyphInfo *text_sdf_find_glyph(Agentite_SDFFont *font, uint32_t codepoint)
{
    for (int i = 0; i < font->glyph_count; i++) {
        if (font->glyphs[i].codepoint == codepoint) {
            return &font->glyphs[i];
        }
    }
    return NULL;
}

void agentite_sdf_text_draw_ex(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                              const char *text, float x, float y,
                              float scale,
                              float r, float g, float b, float a,
                              Agentite_TextAlign align)
{
    if (!tr || !font || !text || !tr->batch_started) return;

    /* Auto-batch: if switching between bitmap and SDF, or SDF font changes, start new batch */
    if ((tr->current_font && !tr->is_sdf_batch) ||
        (tr->current_sdf_font && tr->current_sdf_font != font)) {
        /* End current batch (queues it) */
        agentite_text_end(tr);
        /* Start new batch */
        agentite_text_begin(tr);
    }

    tr->current_sdf_font = font;
    tr->is_sdf_batch = true;
    tr->current_sdf_scale = scale;

    /* Calculate pixel size */
    float px_size = font->font_size * scale;

    /* Handle alignment */
    float offset_x = 0.0f;
    if (align != AGENTITE_TEXT_ALIGN_LEFT) {
        float text_width = agentite_sdf_text_measure(font, text, scale);
        if (align == AGENTITE_TEXT_ALIGN_CENTER) {
            offset_x = -text_width / 2.0f;
        } else if (align == AGENTITE_TEXT_ALIGN_RIGHT) {
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

void agentite_sdf_text_draw(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                          const char *text, float x, float y, float scale)
{
    agentite_sdf_text_draw_ex(tr, font, text, x, y, scale, 1.0f, 1.0f, 1.0f, 1.0f,
                            AGENTITE_TEXT_ALIGN_LEFT);
}

void agentite_sdf_text_draw_colored(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                                   const char *text, float x, float y, float scale,
                                   float r, float g, float b, float a)
{
    agentite_sdf_text_draw_ex(tr, font, text, x, y, scale, r, g, b, a,
                            AGENTITE_TEXT_ALIGN_LEFT);
}

void agentite_sdf_text_printf(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                            float x, float y, float scale,
                            const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    agentite_sdf_text_draw(tr, font, buffer, x, y, scale);
}

void agentite_sdf_text_printf_colored(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                                     float x, float y, float scale,
                                     float r, float g, float b, float a,
                                     const char *fmt, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    agentite_sdf_text_draw_colored(tr, font, buffer, x, y, scale, r, g, b, a);
}

/* ============================================================================
 * SDF Text Effects
 * ============================================================================ */

void agentite_sdf_text_set_effects(Agentite_TextRenderer *tr, const Agentite_TextEffects *effects)
{
    if (!tr || !effects) return;
    tr->current_effects = *effects;
}

void agentite_sdf_text_clear_effects(Agentite_TextRenderer *tr)
{
    if (!tr) return;
    memset(&tr->current_effects, 0, sizeof(Agentite_TextEffects));
}

void agentite_sdf_text_set_outline(Agentite_TextRenderer *tr, float width,
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

void agentite_sdf_text_set_shadow(Agentite_TextRenderer *tr,
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

void agentite_sdf_text_set_glow(Agentite_TextRenderer *tr, float width,
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

void agentite_sdf_text_set_weight(Agentite_TextRenderer *tr, float weight)
{
    if (!tr) return;
    tr->current_effects.weight = weight;
}

/* ============================================================================
 * SDF Text Measurement
 * ============================================================================ */

float agentite_sdf_text_measure(Agentite_SDFFont *font, const char *text, float scale)
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

void agentite_sdf_text_measure_bounds(Agentite_SDFFont *font, const char *text, float scale,
                                     float *out_width, float *out_height)
{
    if (!font || !text) {
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        return;
    }

    if (out_width) *out_width = agentite_sdf_text_measure(font, text, scale);
    if (out_height) *out_height = font->line_height * font->font_size * scale;
}
