/*
 * Carbon Sprite/Texture System
 *
 * Usage:
 *   Agentite_SpriteRenderer *sr = agentite_sprite_init(gpu, window);
 *
 *   Agentite_Texture *tex = agentite_texture_load(sr, "assets/player.png");
 *   Agentite_Sprite sprite = agentite_sprite_create(tex, 0, 0, 64, 64);
 *
 *   // Each frame:
 *   agentite_sprite_begin(sr, NULL);
 *   agentite_sprite_draw(sr, &sprite, 100.0f, 200.0f);
 *   agentite_sprite_draw_ex(sr, &sprite, 300.0f, 200.0f, 2.0f, 2.0f, 45.0f, 0.5f, 0.5f);
 *   agentite_sprite_upload(sr, cmd);           // Before render pass (uses copy pass)
 *   // ... begin render pass ...
 *   agentite_sprite_render(sr, cmd, pass);     // During render pass
 *   // ... end render pass ...
 *
 *   agentite_texture_destroy(sr, tex);
 *   agentite_sprite_shutdown(sr);
 */

#ifndef AGENTITE_SPRITE_H
#define AGENTITE_SPRITE_H

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
typedef struct Agentite_Texture Agentite_Texture;

/**
 * Texture scale mode for sampling.
 * Controls how textures are filtered when scaled.
 */
typedef enum Agentite_ScaleMode {
    AGENTITE_SCALEMODE_NEAREST,   /**< Nearest-neighbor (crisp pixels) */
    AGENTITE_SCALEMODE_LINEAR,    /**< Bilinear filtering (smooth) */
    AGENTITE_SCALEMODE_PIXELART   /**< Pixel-art mode (nearest + integer scaling hints) */
} Agentite_ScaleMode;

/**
 * Texture address mode for UV coordinates outside [0,1].
 * Controls how textures wrap or clamp at edges.
 */
typedef enum Agentite_TextureAddressMode {
    AGENTITE_ADDRESSMODE_CLAMP,   /**< Clamp to edge (default) */
    AGENTITE_ADDRESSMODE_REPEAT,  /**< Repeat/tile the texture */
    AGENTITE_ADDRESSMODE_MIRROR   /**< Mirror at edges */
} Agentite_TextureAddressMode;

/* Sprite definition - references a region of a texture */
typedef struct Agentite_Sprite {
    Agentite_Texture *texture;
    float src_x, src_y;         /* Source rectangle in pixels */
    float src_w, src_h;
    float origin_x, origin_y;   /* Origin for rotation (0-1, default 0.5, 0.5) */
} Agentite_Sprite;

/* Sprite renderer context */
typedef struct Agentite_SpriteRenderer Agentite_SpriteRenderer;

/* Vertex format for sprite rendering */
typedef struct Agentite_SpriteVertex {
    float pos[2];       /* Screen position (x, y) */
    float uv[2];        /* Texture coordinates */
    float color[4];     /* RGBA color (for tinting) */
} Agentite_SpriteVertex;

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Initialize sprite rendering system.
 * Caller OWNS the returned pointer and MUST call agentite_sprite_shutdown().
 */
Agentite_SpriteRenderer *agentite_sprite_init(SDL_GPUDevice *gpu, SDL_Window *window);

/* Shutdown sprite rendering system and free all resources */
void agentite_sprite_shutdown(Agentite_SpriteRenderer *sr);

/* Set screen dimensions (call when window resizes) */
void agentite_sprite_set_screen_size(Agentite_SpriteRenderer *sr, int width, int height);

/* ============================================================================
 * Texture Functions
 * ============================================================================ */

/**
 * Load texture from file (PNG, JPG, BMP, etc.).
 * Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 */
Agentite_Texture *agentite_texture_load(Agentite_SpriteRenderer *sr, const char *path);

/**
 * Load texture from memory.
 * Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 */
Agentite_Texture *agentite_texture_load_memory(Agentite_SpriteRenderer *sr,
                                           const void *data, int size);

/**
 * Create texture from raw RGBA pixels.
 * Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 */
Agentite_Texture *agentite_texture_create(Agentite_SpriteRenderer *sr,
                                      int width, int height,
                                      const void *pixels);

/* Destroy texture and free GPU resources */
void agentite_texture_destroy(Agentite_SpriteRenderer *sr, Agentite_Texture *texture);

/* Get texture dimensions */
void agentite_texture_get_size(const Agentite_Texture *texture, int *width, int *height);

/**
 * Set texture scale mode.
 * Controls filtering when texture is scaled up or down.
 * Default is AGENTITE_SCALEMODE_NEAREST for pixel-art friendly rendering.
 *
 * @param texture Texture to modify
 * @param mode    Scale mode (NEAREST, LINEAR, or PIXELART)
 */
void agentite_texture_set_scale_mode(Agentite_Texture *texture, Agentite_ScaleMode mode);

/**
 * Get texture scale mode.
 */
Agentite_ScaleMode agentite_texture_get_scale_mode(const Agentite_Texture *texture);

/**
 * Set texture address mode.
 * Controls wrapping behavior for UV coordinates outside [0,1].
 * Default is AGENTITE_ADDRESSMODE_CLAMP.
 *
 * @param texture Texture to modify
 * @param mode    Address mode (CLAMP, REPEAT, or MIRROR)
 */
void agentite_texture_set_address_mode(Agentite_Texture *texture, Agentite_TextureAddressMode mode);

/**
 * Get texture address mode.
 */
Agentite_TextureAddressMode agentite_texture_get_address_mode(const Agentite_Texture *texture);

/**
 * Reload texture from disk, updating GPU contents in-place.
 * The texture pointer remains valid. If dimensions change, the GPU
 * texture is recreated.
 *
 * @param sr      Sprite renderer
 * @param texture Texture to reload (must not be NULL)
 * @param path    File path to reload from
 * @return true on success, false on failure (check agentite_get_last_error())
 */
bool agentite_texture_reload(Agentite_SpriteRenderer *sr,
                             Agentite_Texture *texture,
                             const char *path);

/* ============================================================================
 * Sprite Functions
 * ============================================================================ */

/* Create sprite from entire texture */
Agentite_Sprite agentite_sprite_from_texture(Agentite_Texture *texture);

/* Create sprite from texture region */
Agentite_Sprite agentite_sprite_create(Agentite_Texture *texture,
                                   float src_x, float src_y,
                                   float src_w, float src_h);

/* Set sprite origin (for rotation, 0-1 normalized, default center) */
void agentite_sprite_set_origin(Agentite_Sprite *sprite, float ox, float oy);

/* ============================================================================
 * Rendering Functions
 * ============================================================================ */

/* Begin sprite batch (call before drawing sprites) */
void agentite_sprite_begin(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/* Draw sprite at position (1:1 scale, no rotation) */
void agentite_sprite_draw(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                        float x, float y);

/* Draw sprite with scale */
void agentite_sprite_draw_scaled(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                               float x, float y, float scale_x, float scale_y);

/* Draw sprite with full transform */
void agentite_sprite_draw_ex(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                           float x, float y,
                           float scale_x, float scale_y,
                           float rotation_deg,
                           float origin_x, float origin_y);

/* Draw sprite with tint color */
void agentite_sprite_draw_tinted(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                               float x, float y,
                               float r, float g, float b, float a);

/* Draw sprite with full options */
void agentite_sprite_draw_full(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                             float x, float y,
                             float scale_x, float scale_y,
                             float rotation_deg,
                             float origin_x, float origin_y,
                             float r, float g, float b, float a);

/* Flush current batch (for texture changes mid-frame) */
void agentite_sprite_flush(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                         SDL_GPURenderPass *pass);

/* Upload sprite batch to GPU (call BEFORE render pass begins) */
void agentite_sprite_upload(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/* Render sprite batch (call DURING render pass) */
void agentite_sprite_render(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                          SDL_GPURenderPass *pass);

/* ============================================================================
 * Camera Integration
 * ============================================================================ */

/* Forward declaration */
typedef struct Agentite_Camera Agentite_Camera;

/* Set camera for sprite rendering (NULL for screen-space mode) */
void agentite_sprite_set_camera(Agentite_SpriteRenderer *sr, Agentite_Camera *camera);

/* Get current camera */
Agentite_Camera *agentite_sprite_get_camera(const Agentite_SpriteRenderer *sr);

/* ============================================================================
 * Render-to-Texture Functions
 * ============================================================================ */

/**
 * Create a render target texture (can be rendered to).
 * Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 */
Agentite_Texture *agentite_texture_create_render_target(Agentite_SpriteRenderer *sr,
                                                     int width, int height);

/* Begin rendering to a texture (instead of screen) */
SDL_GPURenderPass *agentite_sprite_begin_render_to_texture(Agentite_SpriteRenderer *sr,
                                                          Agentite_Texture *target,
                                                          SDL_GPUCommandBuffer *cmd,
                                                          float clear_r, float clear_g,
                                                          float clear_b, float clear_a);

/* Render sprites to the current texture target */
void agentite_sprite_render_to_texture(Agentite_SpriteRenderer *sr,
                                      SDL_GPUCommandBuffer *cmd,
                                      SDL_GPURenderPass *pass);

/* End render-to-texture pass */
void agentite_sprite_end_render_to_texture(SDL_GPURenderPass *pass);

/* ============================================================================
 * Vignette Post-Process Functions
 * ============================================================================ */

/* Check if vignette pipeline is available */
bool agentite_sprite_has_vignette(const Agentite_SpriteRenderer *sr);

/* Render scene texture with vignette effect applied */
void agentite_sprite_render_vignette(Agentite_SpriteRenderer *sr,
                                    SDL_GPUCommandBuffer *cmd,
                                    SDL_GPURenderPass *pass,
                                    Agentite_Texture *scene_texture);

/* Prepare a fullscreen quad in CPU buffers (call before upload) */
void agentite_sprite_prepare_fullscreen_quad(Agentite_SpriteRenderer *sr);

/* Upload fullscreen quad to GPU (call before render pass) */
void agentite_sprite_upload_fullscreen_quad(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/* ============================================================================
 * Asset Handle Integration
 * ============================================================================ */

/* Asset system integration */
#include "agentite/asset.h"

/**
 * Load texture and register with asset registry.
 * The texture is automatically registered with the given path and can be
 * looked up later via agentite_asset_lookup(). The registry manages lifetime
 * via reference counting.
 *
 * @param sr       Sprite renderer
 * @param registry Asset registry (must not be NULL)
 * @param path     File path (also used as asset ID)
 * @return Asset handle, or AGENTITE_INVALID_ASSET_HANDLE on failure
 */
Agentite_AssetHandle agentite_texture_load_asset(Agentite_SpriteRenderer *sr,
                                                  Agentite_AssetRegistry *registry,
                                                  const char *path);

/**
 * Get texture pointer from asset handle.
 * Returns the raw texture pointer for use with sprite functions.
 *
 * @param registry Asset registry
 * @param handle   Asset handle from agentite_texture_load_asset()
 * @return Texture pointer, or NULL if handle is invalid
 */
Agentite_Texture *agentite_texture_from_handle(Agentite_AssetRegistry *registry,
                                                Agentite_AssetHandle handle);

/**
 * Texture destructor callback for asset registry.
 * Pass this to agentite_asset_set_destructor() with SpriteRenderer as userdata.
 *
 * Example:
 *   agentite_asset_set_destructor(registry, agentite_texture_asset_destructor, sprite_renderer);
 *
 * Note: This only handles textures. For mixed asset types (textures + audio),
 * create a custom destructor that dispatches by type.
 */
void agentite_texture_asset_destructor(void *data, int type, void *userdata);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SPRITE_H */
