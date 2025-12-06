/*
 * Carbon Text Rendering System
 *
 * Usage:
 *   Carbon_TextRenderer *tr = carbon_text_init(gpu, window);
 *
 *   Carbon_Font *font = carbon_font_load(tr, "assets/fonts/Roboto.ttf", 24.0f);
 *
 *   // Each frame:
 *   carbon_text_begin(tr);
 *   carbon_text_draw(tr, font, "Hello World!", 100.0f, 200.0f);
 *   carbon_text_draw_colored(tr, font, "Red text", x, y, 1.0f, 0.0f, 0.0f, 1.0f);
 *   carbon_text_upload(tr, cmd);
 *
 *   // During render pass:
 *   carbon_text_render(tr, cmd, pass);
 *
 *   carbon_font_destroy(tr, font);
 *   carbon_text_shutdown(tr);
 */

#ifndef CARBON_TEXT_H
#define CARBON_TEXT_H

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
typedef struct Carbon_Font Carbon_Font;

/* Text renderer context */
typedef struct Carbon_TextRenderer Carbon_TextRenderer;

/* Text alignment options */
typedef enum Carbon_TextAlign {
    CARBON_TEXT_ALIGN_LEFT   = 0,
    CARBON_TEXT_ALIGN_CENTER = 1,
    CARBON_TEXT_ALIGN_RIGHT  = 2
} Carbon_TextAlign;

/* ============================================================================
 * SDF/MSDF Types (Signed Distance Field Text)
 * ============================================================================ */

/* SDF font type */
typedef enum Carbon_SDFFontType {
    CARBON_SDF_TYPE_SDF  = 0,   /* Single-channel signed distance field */
    CARBON_SDF_TYPE_MSDF = 1    /* Multi-channel signed distance field */
} Carbon_SDFFontType;

/* Opaque SDF font handle */
typedef struct Carbon_SDFFont Carbon_SDFFont;

/* Text effects for SDF rendering */
typedef struct Carbon_TextEffects {
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
} Carbon_TextEffects;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Initialize text rendering system.
 * Caller OWNS the returned pointer and MUST call carbon_text_shutdown().
 */
Carbon_TextRenderer *carbon_text_init(SDL_GPUDevice *gpu, SDL_Window *window);

/* Shutdown text rendering system and free all resources */
void carbon_text_shutdown(Carbon_TextRenderer *tr);

/* Set screen dimensions (call when window resizes) */
void carbon_text_set_screen_size(Carbon_TextRenderer *tr, int width, int height);

/* ============================================================================
 * Font Functions
 * ============================================================================ */

/**
 * Load font from TTF file at specified size.
 * Caller OWNS the returned pointer and MUST call carbon_font_destroy().
 */
Carbon_Font *carbon_font_load(Carbon_TextRenderer *tr, const char *path, float size);

/**
 * Load font from memory buffer at specified size.
 * Caller OWNS the returned pointer and MUST call carbon_font_destroy().
 */
Carbon_Font *carbon_font_load_memory(Carbon_TextRenderer *tr,
                                      const void *data, int data_size,
                                      float size);

/* Destroy font and free resources */
void carbon_font_destroy(Carbon_TextRenderer *tr, Carbon_Font *font);

/* Get font metrics */
float carbon_font_get_size(Carbon_Font *font);
float carbon_font_get_line_height(Carbon_Font *font);
float carbon_font_get_ascent(Carbon_Font *font);
float carbon_font_get_descent(Carbon_Font *font);

/* ============================================================================
 * Text Measurement
 * ============================================================================ */

/* Measure text dimensions (returns width) */
float carbon_text_measure(Carbon_Font *font, const char *text);

/* Measure text bounds (full rectangle) */
void carbon_text_measure_bounds(Carbon_Font *font, const char *text,
                                 float *out_width, float *out_height);

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

/* Begin text batch (call before drawing text) */
void carbon_text_begin(Carbon_TextRenderer *tr);

/* Draw text at position (white, no scale) */
void carbon_text_draw(Carbon_TextRenderer *tr, Carbon_Font *font,
                      const char *text, float x, float y);

/* Draw text with color */
void carbon_text_draw_colored(Carbon_TextRenderer *tr, Carbon_Font *font,
                              const char *text, float x, float y,
                              float r, float g, float b, float a);

/* Draw text with scale */
void carbon_text_draw_scaled(Carbon_TextRenderer *tr, Carbon_Font *font,
                             const char *text, float x, float y,
                             float scale);

/* Draw text with full options */
void carbon_text_draw_ex(Carbon_TextRenderer *tr, Carbon_Font *font,
                         const char *text, float x, float y,
                         float scale,
                         float r, float g, float b, float a,
                         Carbon_TextAlign align);

/* Upload text batch to GPU (call BEFORE render pass begins) */
void carbon_text_upload(Carbon_TextRenderer *tr, SDL_GPUCommandBuffer *cmd);

/* Render text batch (call DURING render pass) */
void carbon_text_render(Carbon_TextRenderer *tr, SDL_GPUCommandBuffer *cmd,
                        SDL_GPURenderPass *pass);

/* End text batch (cleanup, optional) */
void carbon_text_end(Carbon_TextRenderer *tr);

/* ============================================================================
 * Formatted Text (printf-style)
 * ============================================================================ */

/* Draw formatted text (printf-style) */
void carbon_text_printf(Carbon_TextRenderer *tr, Carbon_Font *font,
                        float x, float y,
                        const char *fmt, ...) __attribute__((format(printf, 5, 6)));

/* Draw formatted text with color */
void carbon_text_printf_colored(Carbon_TextRenderer *tr, Carbon_Font *font,
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
 * Caller OWNS the returned pointer and MUST call carbon_sdf_font_destroy().
 */
Carbon_SDFFont *carbon_sdf_font_load(Carbon_TextRenderer *tr,
                                      const char *atlas_path,
                                      const char *metrics_path);

/* Destroy SDF font and free resources */
void carbon_sdf_font_destroy(Carbon_TextRenderer *tr, Carbon_SDFFont *font);

/* Get SDF font type (SDF or MSDF) */
Carbon_SDFFontType carbon_sdf_font_get_type(Carbon_SDFFont *font);

/* Get SDF font metrics */
float carbon_sdf_font_get_size(Carbon_SDFFont *font);
float carbon_sdf_font_get_line_height(Carbon_SDFFont *font);
float carbon_sdf_font_get_ascent(Carbon_SDFFont *font);
float carbon_sdf_font_get_descent(Carbon_SDFFont *font);

/* ============================================================================
 * SDF Text Drawing
 * ============================================================================ */

/* Draw SDF text at position (white, scale 1.0) */
void carbon_sdf_text_draw(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                          const char *text, float x, float y, float scale);

/* Draw SDF text with color */
void carbon_sdf_text_draw_colored(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                                   const char *text, float x, float y, float scale,
                                   float r, float g, float b, float a);

/* Draw SDF text with full options */
void carbon_sdf_text_draw_ex(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                              const char *text, float x, float y,
                              float scale,
                              float r, float g, float b, float a,
                              Carbon_TextAlign align);

/* Printf-style SDF text drawing */
void carbon_sdf_text_printf(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                            float x, float y, float scale,
                            const char *fmt, ...) __attribute__((format(printf, 6, 7)));

/* Printf-style SDF text with color */
void carbon_sdf_text_printf_colored(Carbon_TextRenderer *tr, Carbon_SDFFont *font,
                                     float x, float y, float scale,
                                     float r, float g, float b, float a,
                                     const char *fmt, ...) __attribute__((format(printf, 10, 11)));

/* ============================================================================
 * SDF Text Effects
 * ============================================================================ */

/* Set effects for subsequent SDF text draws */
void carbon_sdf_text_set_effects(Carbon_TextRenderer *tr, const Carbon_TextEffects *effects);

/* Clear all effects */
void carbon_sdf_text_clear_effects(Carbon_TextRenderer *tr);

/* Convenience: Set outline effect */
void carbon_sdf_text_set_outline(Carbon_TextRenderer *tr, float width,
                                  float r, float g, float b, float a);

/* Convenience: Set shadow effect */
void carbon_sdf_text_set_shadow(Carbon_TextRenderer *tr,
                                 float offset_x, float offset_y, float softness,
                                 float r, float g, float b, float a);

/* Convenience: Set glow effect */
void carbon_sdf_text_set_glow(Carbon_TextRenderer *tr, float width,
                               float r, float g, float b, float a);

/* Convenience: Set weight adjustment */
void carbon_sdf_text_set_weight(Carbon_TextRenderer *tr, float weight);

/* ============================================================================
 * SDF Text Measurement
 * ============================================================================ */

/* Measure SDF text width at given scale */
float carbon_sdf_text_measure(Carbon_SDFFont *font, const char *text, float scale);

/* Measure SDF text bounds at given scale */
void carbon_sdf_text_measure_bounds(Carbon_SDFFont *font, const char *text, float scale,
                                     float *out_width, float *out_height);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_TEXT_H */
