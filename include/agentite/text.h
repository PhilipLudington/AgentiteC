/*
 * Carbon Text Rendering System
 *
 * Usage:
 *   Agentite_TextRenderer *tr = agentite_text_init(gpu, window);
 *
 *   Agentite_Font *font = agentite_font_load(tr, "assets/fonts/Roboto.ttf", 24.0f);
 *
 *   // Each frame:
 *   agentite_text_begin(tr);
 *   agentite_text_draw(tr, font, "Hello World!", 100.0f, 200.0f);
 *   agentite_text_draw_colored(tr, font, "Red text", x, y, 1.0f, 0.0f, 0.0f, 1.0f);
 *   agentite_text_upload(tr, cmd);
 *
 *   // During render pass:
 *   agentite_text_render(tr, cmd, pass);
 *
 *   agentite_font_destroy(tr, font);
 *   agentite_text_shutdown(tr);
 */

#ifndef AGENTITE_TEXT_H
#define AGENTITE_TEXT_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Opaque font handle */
typedef struct Agentite_Font Agentite_Font;

/* Text renderer context */
typedef struct Agentite_TextRenderer Agentite_TextRenderer;

/* Text alignment options */
typedef enum Agentite_TextAlign {
    AGENTITE_TEXT_ALIGN_LEFT   = 0,
    AGENTITE_TEXT_ALIGN_CENTER = 1,
    AGENTITE_TEXT_ALIGN_RIGHT  = 2
} Agentite_TextAlign;

/* ============================================================================
 * SDF/MSDF Types (Signed Distance Field Text)
 * ============================================================================ */

/* SDF font type */
typedef enum Agentite_SDFFontType {
    AGENTITE_SDF_TYPE_SDF  = 0,   /* Single-channel signed distance field */
    AGENTITE_SDF_TYPE_MSDF = 1    /* Multi-channel signed distance field */
} Agentite_SDFFontType;

/* Opaque SDF font handle */
typedef struct Agentite_SDFFont Agentite_SDFFont;

/* Text effects for SDF rendering */
typedef struct Agentite_TextEffects {
    /* Outline effect */
    bool outline_enabled;
    float outline_width;          /* 0.0-0.5 in SDF units */
    float outline_color[4];       /* RGBA */

    /* Shadow effect */
    bool shadow_enabled;
    float shadow_offset[2];       /* X, Y offset in pixels */
    float shadow_softness;        /* Blur amount (0.0-1.0) */
    float shadow_color[4];        /* RGBA */

    /* Glow effect */
    bool glow_enabled;
    float glow_width;             /* Extent in SDF units (0.0-0.5) */
    float glow_color[4];          /* RGBA */

    /* Weight adjustment (-0.5 to 0.5: thin to bold) */
    float weight;
} Agentite_TextEffects;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Initialize text rendering system.
 * Caller OWNS the returned pointer and MUST call agentite_text_shutdown().
 */
Agentite_TextRenderer *agentite_text_init(SDL_GPUDevice *gpu, SDL_Window *window);

/* Shutdown text rendering system and free all resources */
void agentite_text_shutdown(Agentite_TextRenderer *tr);

/* Set screen dimensions (call when window resizes) */
void agentite_text_set_screen_size(Agentite_TextRenderer *tr, int width, int height);

/* ============================================================================
 * Font Functions
 * ============================================================================ */

/**
 * Load font from TTF file at specified size.
 * Caller OWNS the returned pointer and MUST call agentite_font_destroy().
 */
Agentite_Font *agentite_font_load(Agentite_TextRenderer *tr, const char *path, float size);

/**
 * Load font from memory buffer at specified size.
 * Caller OWNS the returned pointer and MUST call agentite_font_destroy().
 */
Agentite_Font *agentite_font_load_memory(Agentite_TextRenderer *tr,
                                      const void *data, int data_size,
                                      float size);

/* Destroy font and free resources */
void agentite_font_destroy(Agentite_TextRenderer *tr, Agentite_Font *font);

/* Get font metrics */
float agentite_font_get_size(const Agentite_Font *font);
float agentite_font_get_line_height(const Agentite_Font *font);
float agentite_font_get_ascent(const Agentite_Font *font);
float agentite_font_get_descent(const Agentite_Font *font);

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

/* Measure text dimensions (returns width) */
float agentite_text_measure(const Agentite_Font *font, const char *text);

/* Measure text bounds (full rectangle) */
void agentite_text_measure_bounds(const Agentite_Font *font, const char *text,
                                 float *out_width, float *out_height);

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

/* Begin text batch (call before drawing text) */
void agentite_text_begin(Agentite_TextRenderer *tr);

/* Draw text at position (white, no scale) */
void agentite_text_draw(Agentite_TextRenderer *tr, Agentite_Font *font,
                      const char *text, float x, float y);

/* Draw text with color */
void agentite_text_draw_colored(Agentite_TextRenderer *tr, Agentite_Font *font,
                              const char *text, float x, float y,
                              float r, float g, float b, float a);

/* Draw text with scale */
void agentite_text_draw_scaled(Agentite_TextRenderer *tr, Agentite_Font *font,
                             const char *text, float x, float y,
                             float scale);

/* Draw text with full options */
void agentite_text_draw_ex(Agentite_TextRenderer *tr, Agentite_Font *font,
                         const char *text, float x, float y,
                         float scale,
                         float r, float g, float b, float a,
                         Agentite_TextAlign align);

/* Upload text batch to GPU (call BEFORE render pass begins) */
void agentite_text_upload(Agentite_TextRenderer *tr, SDL_GPUCommandBuffer *cmd);

/* Render text batch (call DURING render pass) */
void agentite_text_render(Agentite_TextRenderer *tr, SDL_GPUCommandBuffer *cmd,
                        SDL_GPURenderPass *pass);

/* End text batch (cleanup, optional) */
void agentite_text_end(Agentite_TextRenderer *tr);

/* ============================================================================
 * Formatted Text (printf-style)
 * ============================================================================ */

/* Draw formatted text (printf-style) */
void agentite_text_printf(Agentite_TextRenderer *tr, Agentite_Font *font,
                        float x, float y,
                        const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/* Draw formatted text with color */
void agentite_text_printf_colored(Agentite_TextRenderer *tr, Agentite_Font *font,
                                float x, float y,
                                float r, float g, float b, float a,
                                const char *fmt, ...) __attribute__((format(printf, 9, 10)));

/* ============================================================================
 * SDF/MSDF Font Functions
 * ============================================================================ */

/**
 * Load SDF/MSDF font from pre-generated atlas files (msdf-atlas-gen format).
 * atlas_path: Path to PNG atlas image
 * metrics_path: Path to JSON metrics file
 * Caller OWNS the returned pointer and MUST call agentite_sdf_font_destroy().
 */
Agentite_SDFFont *agentite_sdf_font_load(Agentite_TextRenderer *tr,
                                      const char *atlas_path,
                                      const char *metrics_path);

/* Configuration for runtime MSDF font generation */
typedef struct Agentite_SDFFontGenConfig {
    int atlas_width;            /* Atlas texture width (default: 1024) */
    int atlas_height;           /* Atlas texture height (default: 1024) */
    float glyph_scale;          /* Glyph rendering size in pixels (default: 48) */
    float pixel_range;          /* SDF range in pixels (default: 4) */
    bool generate_msdf;         /* True for MSDF, false for single-channel SDF */
    const char *charset;        /* Custom character set (NULL for ASCII) */
} Agentite_SDFFontGenConfig;

/* Default configuration for runtime font generation */
#define AGENTITE_SDF_FONT_GEN_CONFIG_DEFAULT { \
    .atlas_width = 1024, \
    .atlas_height = 1024, \
    .glyph_scale = 48.0f, \
    .pixel_range = 4.0f, \
    .generate_msdf = true, \
    .charset = NULL \
}

/**
 * Generate SDF/MSDF font at runtime from a TTF file.
 * This eliminates the need for external tools like msdf-atlas-gen.
 *
 * @param tr  Text renderer
 * @param ttf_path  Path to TTF font file
 * @param config  Generation configuration (NULL for defaults)
 * @return  New font, or NULL on failure. Caller must free with agentite_sdf_font_destroy().
 */
Agentite_SDFFont *agentite_sdf_font_generate(Agentite_TextRenderer *tr,
                                          const char *ttf_path,
                                          const Agentite_SDFFontGenConfig *config);

/* Destroy SDF font and free resources */
void agentite_sdf_font_destroy(Agentite_TextRenderer *tr, Agentite_SDFFont *font);

/* Get SDF font type (SDF or MSDF) */
Agentite_SDFFontType agentite_sdf_font_get_type(const Agentite_SDFFont *font);

/* Get SDF font metrics */
float agentite_sdf_font_get_size(const Agentite_SDFFont *font);
float agentite_sdf_font_get_line_height(const Agentite_SDFFont *font);
float agentite_sdf_font_get_ascent(const Agentite_SDFFont *font);
float agentite_sdf_font_get_descent(const Agentite_SDFFont *font);

/* ============================================================================
 * SDF Text Drawing
 * ============================================================================ */

/* Draw SDF text at position (white, scale 1.0) */
void agentite_sdf_text_draw(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                          const char *text, float x, float y, float scale);

/* Draw SDF text with color */
void agentite_sdf_text_draw_colored(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                                   const char *text, float x, float y, float scale,
                                   float r, float g, float b, float a);

/* Draw SDF text with full options */
void agentite_sdf_text_draw_ex(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                              const char *text, float x, float y,
                              float scale,
                              float r, float g, float b, float a,
                              Agentite_TextAlign align);

/* Printf-style SDF text drawing */
void agentite_sdf_text_printf(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                            float x, float y, float scale,
                            const char *fmt, ...) __attribute__((format(printf, 6, 7)));

/* Printf-style SDF text with color */
void agentite_sdf_text_printf_colored(Agentite_TextRenderer *tr, Agentite_SDFFont *font,
                                     float x, float y, float scale,
                                     float r, float g, float b, float a,
                                     const char *fmt, ...) __attribute__((format(printf, 10, 11)));

/* ============================================================================
 * SDF Text Effects
 * ============================================================================ */

/* Set effects for subsequent SDF text draws */
void agentite_sdf_text_set_effects(Agentite_TextRenderer *tr, const Agentite_TextEffects *effects);

/* Clear all effects */
void agentite_sdf_text_clear_effects(Agentite_TextRenderer *tr);

/* Convenience: Set outline effect */
void agentite_sdf_text_set_outline(Agentite_TextRenderer *tr, float width,
                                  float r, float g, float b, float a);

/* Convenience: Set shadow effect */
void agentite_sdf_text_set_shadow(Agentite_TextRenderer *tr,
                                 float offset_x, float offset_y, float softness,
                                 float r, float g, float b, float a);

/* Convenience: Set glow effect */
void agentite_sdf_text_set_glow(Agentite_TextRenderer *tr, float width,
                               float r, float g, float b, float a);

/* Convenience: Set weight adjustment */
void agentite_sdf_text_set_weight(Agentite_TextRenderer *tr, float weight);

/* ============================================================================
 * SDF Text Measurement
 * ============================================================================ */

/* Measure SDF text width at given scale */
float agentite_sdf_text_measure(const Agentite_SDFFont *font, const char *text, float scale);

/* Measure SDF text bounds at given scale */
void agentite_sdf_text_measure_bounds(const Agentite_SDFFont *font, const char *text, float scale,
                                     float *out_width, float *out_height);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_TEXT_H */
