/*
 * Carbon Sprite/Texture System
 *
 * Usage:
 *   Carbon_SpriteRenderer *sr = carbon_sprite_init(gpu, window);
 *
 *   Carbon_Texture *tex = carbon_texture_load(sr, "assets/player.png");
 *   Carbon_Sprite sprite = carbon_sprite_create(tex, 0, 0, 64, 64);
 *
 *   // Each frame:
 *   carbon_sprite_begin(sr, cmd);
 *   carbon_sprite_draw(sr, &sprite, 100.0f, 200.0f);
 *   carbon_sprite_draw_ex(sr, &sprite, 300.0f, 200.0f, 2.0f, 2.0f, 45.0f, 0.5f, 0.5f);
 *   carbon_sprite_end(sr, cmd, render_pass);
 *
 *   carbon_texture_destroy(sr, tex);
 *   carbon_sprite_shutdown(sr);
 */

#ifndef CARBON_SPRITE_H
#define CARBON_SPRITE_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/* Opaque texture handle */
typedef struct Carbon_Texture Carbon_Texture;

/* Sprite definition - references a region of a texture */
typedef struct Carbon_Sprite {
    Carbon_Texture *texture;
    float src_x, src_y;         /* Source rectangle in pixels */
    float src_w, src_h;
    float origin_x, origin_y;   /* Origin for rotation (0-1, default 0.5, 0.5) */
} Carbon_Sprite;

/* Sprite renderer context */
typedef struct Carbon_SpriteRenderer Carbon_SpriteRenderer;

/* Vertex format for sprite rendering */
typedef struct Carbon_SpriteVertex {
    float pos[2];       /* Screen position (x, y) */
    float uv[2];        /* Texture coordinates */
    float color[4];     /* RGBA color (for tinting) */
} Carbon_SpriteVertex;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Initialize sprite rendering system.
 * Caller OWNS the returned pointer and MUST call carbon_sprite_shutdown().
 */
Carbon_SpriteRenderer *carbon_sprite_init(SDL_GPUDevice *gpu, SDL_Window *window);

/* Shutdown sprite rendering system and free all resources */
void carbon_sprite_shutdown(Carbon_SpriteRenderer *sr);

/* Set screen dimensions (call when window resizes) */
void carbon_sprite_set_screen_size(Carbon_SpriteRenderer *sr, int width, int height);

/* ============================================================================
 * Texture Functions
 * ============================================================================ */

/**
 * Load texture from file (PNG, JPG, BMP, etc.).
 * Caller OWNS the returned pointer and MUST call carbon_texture_destroy().
 */
Carbon_Texture *carbon_texture_load(Carbon_SpriteRenderer *sr, const char *path);

/**
 * Load texture from memory.
 * Caller OWNS the returned pointer and MUST call carbon_texture_destroy().
 */
Carbon_Texture *carbon_texture_load_memory(Carbon_SpriteRenderer *sr,
                                           const void *data, int size);

/**
 * Create texture from raw RGBA pixels.
 * Caller OWNS the returned pointer and MUST call carbon_texture_destroy().
 */
Carbon_Texture *carbon_texture_create(Carbon_SpriteRenderer *sr,
                                      int width, int height,
                                      const void *pixels);

/* Destroy texture and free GPU resources */
void carbon_texture_destroy(Carbon_SpriteRenderer *sr, Carbon_Texture *texture);

/* Get texture dimensions */
void carbon_texture_get_size(Carbon_Texture *texture, int *width, int *height);

/* ============================================================================
 * Sprite Functions
 * ============================================================================ */

/* Create sprite from entire texture */
Carbon_Sprite carbon_sprite_from_texture(Carbon_Texture *texture);

/* Create sprite from texture region */
Carbon_Sprite carbon_sprite_create(Carbon_Texture *texture,
                                   float src_x, float src_y,
                                   float src_w, float src_h);

/* Set sprite origin (for rotation, 0-1 normalized, default center) */
void carbon_sprite_set_origin(Carbon_Sprite *sprite, float ox, float oy);

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

/* Begin sprite batch (call before drawing sprites) */
void carbon_sprite_begin(Carbon_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/* Draw sprite at position (1:1 scale, no rotation) */
void carbon_sprite_draw(Carbon_SpriteRenderer *sr, const Carbon_Sprite *sprite,
                        float x, float y);

/* Draw sprite with scale */
void carbon_sprite_draw_scaled(Carbon_SpriteRenderer *sr, const Carbon_Sprite *sprite,
                               float x, float y, float scale_x, float scale_y);

/* Draw sprite with full transform */
void carbon_sprite_draw_ex(Carbon_SpriteRenderer *sr, const Carbon_Sprite *sprite,
                           float x, float y,
                           float scale_x, float scale_y,
                           float rotation_deg,
                           float origin_x, float origin_y);

/* Draw sprite with tint color */
void carbon_sprite_draw_tinted(Carbon_SpriteRenderer *sr, const Carbon_Sprite *sprite,
                               float x, float y,
                               float r, float g, float b, float a);

/* Draw sprite with full options */
void carbon_sprite_draw_full(Carbon_SpriteRenderer *sr, const Carbon_Sprite *sprite,
                             float x, float y,
                             float scale_x, float scale_y,
                             float rotation_deg,
                             float origin_x, float origin_y,
                             float r, float g, float b, float a);

/* End sprite batch and render (uploads data + draws) */
void carbon_sprite_end(Carbon_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                       SDL_GPURenderPass *pass);

/* Flush current batch (for texture changes mid-frame) */
void carbon_sprite_flush(Carbon_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                         SDL_GPURenderPass *pass);

/* Upload sprite batch to GPU (call BEFORE render pass begins) */
void carbon_sprite_upload(Carbon_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/* Render sprite batch (call DURING render pass) */
void carbon_sprite_render(Carbon_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                          SDL_GPURenderPass *pass);

/* ============================================================================
 * Camera Integration
 * ============================================================================ */

/* Forward declaration */
typedef struct Carbon_Camera Carbon_Camera;

/* Set camera for sprite rendering (NULL for screen-space mode) */
void carbon_sprite_set_camera(Carbon_SpriteRenderer *sr, Carbon_Camera *camera);

/* Get current camera */
Carbon_Camera *carbon_sprite_get_camera(Carbon_SpriteRenderer *sr);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_SPRITE_H */
