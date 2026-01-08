/*
 * Carbon Text Font Management
 *
 * Handles bitmap font loading and metrics.
 */

/* Define STB_TRUETYPE_IMPLEMENTATION before including the header
 * to generate the function implementations in this compilation unit */
#define STB_TRUETYPE_IMPLEMENTATION
#include "text_internal.h"
#include "agentite/path.h"

/* ============================================================================
 * Font Functions
 * ============================================================================ */

Agentite_Font *agentite_font_load(Agentite_TextRenderer *tr, const char *path, float size)
{
    if (!tr || !path) return NULL;

    /* Validate path to prevent directory traversal attacks */
    if (!agentite_path_is_safe(path)) {
        agentite_set_error("Text: Invalid path (directory traversal rejected): '%s'", path);
        return NULL;
    }

    /* Read TTF file */
    SDL_IOStream *file = SDL_IOFromFile(path, "rb");
    if (!file) {
        agentite_set_error("Text: Failed to open font file '%s': %s", path, SDL_GetError());
        return NULL;
    }

    Sint64 file_size = SDL_GetIOSize(file);
    if (file_size <= 0) {
        agentite_set_error("Text: Invalid font file size");
        SDL_CloseIO(file);
        return NULL;
    }

    unsigned char *font_data = (unsigned char*)malloc((size_t)file_size);
    if (!font_data) {
        agentite_set_error("Text: Failed to allocate font data buffer");
        SDL_CloseIO(file);
        return NULL;
    }

    size_t read = SDL_ReadIO(file, font_data, (size_t)file_size);
    SDL_CloseIO(file);

    if (read != (size_t)file_size) {
        agentite_set_error("Text: Failed to read font file");
        free(font_data);
        return NULL;
    }

    Agentite_Font *font = agentite_font_load_memory(tr, font_data, (int)file_size, size);
    if (!font) {
        free(font_data);
        return NULL;
    }

    /* Transfer ownership of font_data to the font */
    font->font_data = font_data;

    SDL_Log("Text: Loaded font '%s' at size %.1f", path, size);
    return font;
}

Agentite_Font *agentite_font_load_memory(Agentite_TextRenderer *tr,
                                      const void *data, int data_size,
                                      float size)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!tr || !data || data_size <= 0) return NULL;

    Agentite_Font *font = AGENTITE_ALLOC(Agentite_Font);
    if (!font) return NULL;

    const unsigned char *font_data = (const unsigned char *)data;

    /* Get font offset - handles TTC (TrueType Collection) files
     * For single TTF files, this returns 0
     * For TTC files, this returns the offset to the first font in the collection */
    int font_offset = stbtt_GetFontOffsetForIndex(font_data, 0);
    if (font_offset < 0) {
        agentite_set_error("Text: Invalid font data or unsupported format");
        free(font);
        return NULL;
    }

    /* Initialize stb_truetype with the correct offset */
    if (!stbtt_InitFont(&font->stb_font, font_data, font_offset)) {
        agentite_set_error("Text: Failed to initialize font");
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
    unsigned char *atlas_bitmap = (unsigned char*)malloc(ATLAS_SIZE * ATLAS_SIZE);
    if (!atlas_bitmap) {
        agentite_set_error("Text: Failed to allocate atlas bitmap");
        free(font);
        return NULL;
    }

    stbtt_bakedchar baked_chars[NUM_CHARS];
    int result = stbtt_BakeFontBitmap(font_data, font_offset,
                                       size, atlas_bitmap,
                                       ATLAS_SIZE, ATLAS_SIZE,
                                       FIRST_CHAR, NUM_CHARS,
                                       baked_chars);
    if (result <= 0) {
        agentite_set_error("Text: Font atlas baking failed (too many chars or atlas too small)");
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
    font->atlas_texture = text_create_font_atlas(tr, atlas_bitmap);
    free(atlas_bitmap);

    if (!font->atlas_texture) {
        free(font);
        return NULL;
    }

    return font;
}

void agentite_font_destroy(Agentite_TextRenderer *tr, Agentite_Font *font)
{
    AGENTITE_ASSERT_MAIN_THREAD();
    if (!tr || !font) return;

    if (font->atlas_texture) {
        SDL_ReleaseGPUTexture(tr->gpu, font->atlas_texture);
    }
    free(font->font_data);
    free(font);
}

float agentite_font_get_size(const Agentite_Font *font)
{
    return font ? font->size : 0.0f;
}

float agentite_font_get_line_height(const Agentite_Font *font)
{
    return font ? font->line_height : 0.0f;
}

float agentite_font_get_ascent(const Agentite_Font *font)
{
    return font ? font->ascent : 0.0f;
}

float agentite_font_get_descent(const Agentite_Font *font)
{
    return font ? font->descent : 0.0f;
}

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

float agentite_text_measure(const Agentite_Font *font, const char *text)
{
    if (!font || !text) return 0.0f;

    float width = 0.0f;
    const char *p = text;

    while (*p) {
        unsigned char c = (unsigned char)*p;
        if (c >= FIRST_CHAR && c <= LAST_CHAR) {
            const GlyphInfo *g = &font->glyphs[c - FIRST_CHAR];
            width += g->advance_x;
        }
        p++;
    }

    return width;
}

void agentite_text_measure_bounds(const Agentite_Font *font, const char *text,
                                 float *out_width, float *out_height)
{
    if (!font || !text) {
        if (out_width) *out_width = 0.0f;
        if (out_height) *out_height = 0.0f;
        return;
    }

    if (out_width) *out_width = agentite_text_measure(font, text);
    if (out_height) *out_height = font->line_height;
}
