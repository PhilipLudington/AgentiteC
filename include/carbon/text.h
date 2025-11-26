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
 * Lifecycle Functions
 * ============================================================================ */

/* Initialize text rendering system */
Carbon_TextRenderer *carbon_text_init(SDL_GPUDevice *gpu, SDL_Window *window);

/* Shutdown text rendering system */
void carbon_text_shutdown(Carbon_TextRenderer *tr);

/* Set screen dimensions (call when window resizes) */
void carbon_text_set_screen_size(Carbon_TextRenderer *tr, int width, int height);

/* ============================================================================
 * Font Functions
 * ============================================================================ */

/* Load font from TTF file at specified size */
Carbon_Font *carbon_font_load(Carbon_TextRenderer *tr, const char *path, float size);

/* Load font from memory buffer at specified size */
Carbon_Font *carbon_font_load_memory(Carbon_TextRenderer *tr,
                                      const void *data, int data_size,
                                      float size);

/* Destroy font */
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

#ifdef __cplusplus
}
#endif

#endif /* CARBON_TEXT_H */
