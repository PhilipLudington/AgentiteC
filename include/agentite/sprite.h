/**
 * @file sprite.h
 * @brief Sprite and texture rendering system for Agentite.
 *
 * This module provides batched sprite rendering using SDL3 GPU. It supports:
 * - Texture loading from files and memory (PNG, JPG, BMP, etc.)
 * - Sprite regions within textures (sprite sheets/atlases)
 * - Transformations: position, scale, rotation, origin
 * - Color tinting and alpha blending
 * - Camera integration for world-space rendering
 * - Render-to-texture for post-processing effects
 * - Vignette post-processing effect
 *
 * @section sprite_usage Basic Usage
 * @code
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
 * @endcode
 *
 * @section sprite_thread_safety Thread Safety
 * All functions in this module are NOT thread-safe and must be called from
 * the main thread only. See AGENTITE_ASSERT_MAIN_THREAD() assertions.
 *
 * @section sprite_ownership Ownership
 * - SpriteRenderer: Created by agentite_sprite_init(), destroyed by agentite_sprite_shutdown()
 * - Texture: Created by agentite_texture_load/create(), destroyed by agentite_texture_destroy()
 * - Textures must outlive all sprites that reference them
 * - SpriteRenderer must outlive all textures created from it
 */

#ifndef AGENTITE_SPRITE_H
#define AGENTITE_SPRITE_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** @defgroup sprite_types Types
 *  @{ */

/**
 * @brief Opaque texture handle.
 *
 * Represents a GPU texture resource. Created via agentite_texture_load() or
 * agentite_texture_create(), destroyed via agentite_texture_destroy().
 *
 * @note Textures must outlive all Agentite_Sprite instances that reference them.
 */
typedef struct Agentite_Texture Agentite_Texture;

/**
 * @brief Texture scale mode for sampling.
 *
 * Controls how textures are filtered when scaled up or down during rendering.
 * Set via agentite_texture_set_scale_mode().
 */
typedef enum Agentite_ScaleMode {
    AGENTITE_SCALEMODE_NEAREST,   /**< Nearest-neighbor filtering (crisp pixels, good for pixel art) */
    AGENTITE_SCALEMODE_LINEAR,    /**< Bilinear filtering (smooth edges, good for photos/HD art) */
    AGENTITE_SCALEMODE_PIXELART   /**< Pixel-art mode (nearest + integer scaling hints) */
} Agentite_ScaleMode;

/**
 * @brief Texture address mode for UV coordinates outside [0,1].
 *
 * Controls how textures behave when UV coordinates exceed the normal 0-1 range.
 * Set via agentite_texture_set_address_mode().
 */
typedef enum Agentite_TextureAddressMode {
    AGENTITE_ADDRESSMODE_CLAMP,   /**< Clamp to edge color (default, prevents bleeding) */
    AGENTITE_ADDRESSMODE_REPEAT,  /**< Repeat/tile the texture (for seamless patterns) */
    AGENTITE_ADDRESSMODE_MIRROR   /**< Mirror at edges (for symmetric patterns) */
} Agentite_TextureAddressMode;

/**
 * @brief Sprite definition referencing a region of a texture.
 *
 * A sprite represents a rectangular portion of a texture that can be drawn
 * to the screen. Multiple sprites can reference the same texture (sprite sheet).
 *
 * Create via agentite_sprite_create() or agentite_sprite_from_texture().
 * Draw via agentite_sprite_draw() family of functions.
 *
 * @warning The referenced texture must remain valid for the lifetime of the sprite.
 */
typedef struct Agentite_Sprite {
    Agentite_Texture *texture;  /**< Texture containing the sprite image (borrowed, not owned) */
    float src_x, src_y;         /**< Top-left corner of source rectangle in pixels */
    float src_w, src_h;         /**< Width and height of source rectangle in pixels */
    float origin_x, origin_y;   /**< Origin for rotation, normalized 0-1 (default 0.5, 0.5 = center) */
} Agentite_Sprite;

/**
 * @brief Opaque sprite renderer context.
 *
 * Manages batched sprite rendering, GPU resources, and render state.
 * Created via agentite_sprite_init(), destroyed via agentite_sprite_shutdown().
 *
 * @note Must outlive all textures created from it.
 */
typedef struct Agentite_SpriteRenderer Agentite_SpriteRenderer;

/**
 * @brief Vertex format for sprite rendering.
 *
 * Internal vertex structure used by the sprite batch. Exposed for advanced
 * users who need custom vertex generation.
 */
typedef struct Agentite_SpriteVertex {
    float pos[2];       /**< Screen position (x, y) in pixels */
    float uv[2];        /**< Texture coordinates (0-1 normalized) */
    float color[4];     /**< RGBA color for tinting (0-1 per component) */
} Agentite_SpriteVertex;

/** @} */ /* end of sprite_types */

/** @defgroup sprite_lifecycle Lifecycle Functions
 *  @{ */

/**
 * @brief Initialize the sprite rendering system.
 *
 * Creates and initializes all GPU resources needed for sprite rendering,
 * including pipelines, buffers, and samplers.
 *
 * @param gpu    SDL GPU device (must not be NULL, borrowed reference)
 * @param window SDL window for determining initial screen size (must not be NULL)
 *
 * @return Sprite renderer on success, NULL on failure (check agentite_get_last_error())
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_sprite_shutdown().
 *
 * @note NOT thread-safe. Must be called from main thread.
 *
 * @code
 * Agentite_SpriteRenderer *sr = agentite_sprite_init(gpu, window);
 * if (!sr) {
 *     SDL_Log("Failed: %s", agentite_get_last_error());
 *     return 1;
 * }
 * @endcode
 */
Agentite_SpriteRenderer *agentite_sprite_init(SDL_GPUDevice *gpu, SDL_Window *window);

/**
 * @brief Shutdown sprite rendering system and free all resources.
 *
 * Releases all GPU resources including pipelines, buffers, and samplers.
 * All textures created from this renderer must be destroyed before calling this.
 *
 * @param sr Sprite renderer to shutdown (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 *
 * @warning Destroying the renderer while textures still exist causes undefined behavior.
 */
void agentite_sprite_shutdown(Agentite_SpriteRenderer *sr);

/**
 * @brief Set screen dimensions for coordinate mapping.
 *
 * Updates the projection matrix for screen-space rendering. Call this when
 * the window is resized to ensure correct sprite positioning.
 *
 * @param sr     Sprite renderer (must not be NULL)
 * @param width  Screen width in pixels (must be > 0)
 * @param height Screen height in pixels (must be > 0)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sprite_set_screen_size(Agentite_SpriteRenderer *sr, int width, int height);

/** @} */ /* end of sprite_lifecycle */

/** @defgroup sprite_texture Texture Functions
 *  @{ */

/**
 * @brief Load texture from an image file.
 *
 * Loads an image file (PNG, JPG, BMP, TGA, GIF, etc.) and creates a GPU texture.
 * Uses stb_image internally for decoding.
 *
 * @param sr   Sprite renderer (must not be NULL)
 * @param path Path to image file (must not be NULL, rejects path traversal)
 *
 * @return Texture on success, NULL on failure (check agentite_get_last_error())
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 * @note Path traversal (e.g., "../secret.png") is rejected for security.
 *
 * @code
 * Agentite_Texture *tex = agentite_texture_load(sr, "assets/player.png");
 * if (!tex) {
 *     SDL_Log("Load failed: %s", agentite_get_last_error());
 * }
 * @endcode
 */
Agentite_Texture *agentite_texture_load(Agentite_SpriteRenderer *sr, const char *path);

/**
 * @brief Load texture from memory buffer.
 *
 * Creates a GPU texture from encoded image data in memory (PNG, JPG, etc.).
 * Useful for loading embedded resources or network-fetched images.
 *
 * @param sr   Sprite renderer (must not be NULL)
 * @param data Pointer to encoded image data (must not be NULL)
 * @param size Size of data in bytes (must be > 0)
 *
 * @return Texture on success, NULL on failure (check agentite_get_last_error())
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 * @note The data buffer can be freed after this call returns.
 */
Agentite_Texture *agentite_texture_load_memory(Agentite_SpriteRenderer *sr,
                                               const void *data, int size);

/**
 * @brief Create texture from raw RGBA pixel data.
 *
 * Creates a GPU texture from uncompressed 32-bit RGBA pixels.
 * Useful for procedural textures or rendering to texture.
 *
 * @param sr     Sprite renderer (must not be NULL)
 * @param width  Texture width in pixels (must be > 0)
 * @param height Texture height in pixels (must be > 0)
 * @param pixels Pointer to RGBA pixel data (width * height * 4 bytes, must not be NULL)
 *
 * @return Texture on success, NULL on failure (check agentite_get_last_error())
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 * @note Pixel data is copied; the buffer can be freed after this call returns.
 */
Agentite_Texture *agentite_texture_create(Agentite_SpriteRenderer *sr,
                                          int width, int height,
                                          const void *pixels);

/**
 * @brief Destroy texture and free GPU resources.
 *
 * Releases the GPU texture resource. The texture pointer becomes invalid
 * and must not be used after this call.
 *
 * @param sr      Sprite renderer that created the texture
 * @param texture Texture to destroy (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 *
 * @warning Do not destroy textures while sprites referencing them are in use.
 */
void agentite_texture_destroy(Agentite_SpriteRenderer *sr, Agentite_Texture *texture);

/**
 * @brief Get texture dimensions.
 *
 * @param texture Texture to query (must not be NULL)
 * @param width   Output for width in pixels (may be NULL if not needed)
 * @param height  Output for height in pixels (may be NULL if not needed)
 */
void agentite_texture_get_size(const Agentite_Texture *texture, int *width, int *height);

/**
 * @brief Set texture scale mode (filtering).
 *
 * Controls how the texture is filtered when drawn at sizes different from
 * its native resolution.
 *
 * @param texture Texture to modify (must not be NULL)
 * @param mode    Scale mode to apply
 *
 * @note Default is AGENTITE_SCALEMODE_NEAREST for pixel-art friendly rendering.
 */
void agentite_texture_set_scale_mode(Agentite_Texture *texture, Agentite_ScaleMode mode);

/**
 * @brief Get texture scale mode.
 *
 * @param texture Texture to query (must not be NULL)
 * @return Current scale mode
 */
Agentite_ScaleMode agentite_texture_get_scale_mode(const Agentite_Texture *texture);

/**
 * @brief Set texture address mode (wrapping).
 *
 * Controls how UV coordinates outside the [0,1] range are handled.
 *
 * @param texture Texture to modify (must not be NULL)
 * @param mode    Address mode to apply
 *
 * @note Default is AGENTITE_ADDRESSMODE_CLAMP.
 */
void agentite_texture_set_address_mode(Agentite_Texture *texture, Agentite_TextureAddressMode mode);

/**
 * @brief Get texture address mode.
 *
 * @param texture Texture to query (must not be NULL)
 * @return Current address mode
 */
Agentite_TextureAddressMode agentite_texture_get_address_mode(const Agentite_Texture *texture);

/**
 * @brief Reload texture from disk, updating GPU contents in-place.
 *
 * Reloads the image file and updates the GPU texture. The texture pointer
 * remains valid. If dimensions change, the internal GPU texture is recreated.
 * Useful for hot-reloading assets during development.
 *
 * @param sr      Sprite renderer (must not be NULL)
 * @param texture Texture to reload (must not be NULL)
 * @param path    File path to reload from (must not be NULL, rejects path traversal)
 *
 * @return true on success, false on failure (check agentite_get_last_error())
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
bool agentite_texture_reload(Agentite_SpriteRenderer *sr,
                             Agentite_Texture *texture,
                             const char *path);

/** @} */ /* end of sprite_texture */

/** @defgroup sprite_creation Sprite Creation
 *  @{ */

/**
 * @brief Create a sprite from an entire texture.
 *
 * Creates a sprite that represents the full texture dimensions.
 * Origin is set to center (0.5, 0.5).
 *
 * @param texture Texture to create sprite from (must not be NULL)
 *
 * @return Sprite structure (value type, no cleanup needed)
 *
 * @code
 * Agentite_Texture *tex = agentite_texture_load(sr, "player.png");
 * Agentite_Sprite sprite = agentite_sprite_from_texture(tex);
 * @endcode
 */
Agentite_Sprite agentite_sprite_from_texture(Agentite_Texture *texture);

/**
 * @brief Create a sprite from a texture region (sprite sheet).
 *
 * Creates a sprite representing a rectangular portion of a texture.
 * Useful for sprite sheets and texture atlases.
 *
 * @param texture Texture containing the sprite (must not be NULL)
 * @param src_x   X position of source rectangle in pixels
 * @param src_y   Y position of source rectangle in pixels
 * @param src_w   Width of source rectangle in pixels
 * @param src_h   Height of source rectangle in pixels
 *
 * @return Sprite structure with origin at center (0.5, 0.5)
 *
 * @code
 * // Extract 32x32 sprite at position (64, 0) from a sprite sheet
 * Agentite_Sprite sprite = agentite_sprite_create(tex, 64, 0, 32, 32);
 * @endcode
 */
Agentite_Sprite agentite_sprite_create(Agentite_Texture *texture,
                                       float src_x, float src_y,
                                       float src_w, float src_h);

/**
 * @brief Set sprite origin point for transformations.
 *
 * The origin is the point around which rotations and scaling occur.
 * Coordinates are normalized: (0,0) = top-left, (1,1) = bottom-right.
 *
 * @param sprite Sprite to modify (must not be NULL)
 * @param ox     X origin, normalized 0-1 (0=left, 0.5=center, 1=right)
 * @param oy     Y origin, normalized 0-1 (0=top, 0.5=center, 1=bottom)
 *
 * @code
 * // Set origin to bottom-center (for character feet)
 * agentite_sprite_set_origin(&sprite, 0.5f, 1.0f);
 * @endcode
 */
void agentite_sprite_set_origin(Agentite_Sprite *sprite, float ox, float oy);

/** @} */ /* end of sprite_creation */

/** @defgroup sprite_rendering Rendering Functions
 *
 * Sprite rendering uses a batching system for performance. The workflow is:
 * 1. Call agentite_sprite_begin() to start a new batch
 * 2. Call agentite_sprite_draw*() functions to queue sprites
 * 3. Call agentite_sprite_upload() BEFORE the render pass
 * 4. Call agentite_sprite_render() DURING the render pass
 *
 * @{
 */

/**
 * @brief Begin a new sprite batch.
 *
 * Resets the sprite batch for a new frame. Must be called before any draw calls.
 *
 * @param sr  Sprite renderer (must not be NULL)
 * @param cmd Command buffer (optional, can be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sprite_begin(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/**
 * @brief Draw sprite at position with default transform.
 *
 * Draws the sprite at 1:1 scale with no rotation.
 *
 * @param sr     Sprite renderer (must not be NULL)
 * @param sprite Sprite to draw (must not be NULL)
 * @param x      X position in screen/world coordinates
 * @param y      Y position in screen/world coordinates
 *
 * @note Sprites are queued for batched rendering; actual drawing occurs in agentite_sprite_render().
 */
void agentite_sprite_draw(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                          float x, float y);

/**
 * @brief Draw sprite with scaling.
 *
 * @param sr      Sprite renderer (must not be NULL)
 * @param sprite  Sprite to draw (must not be NULL)
 * @param x       X position in screen/world coordinates
 * @param y       Y position in screen/world coordinates
 * @param scale_x Horizontal scale factor (1.0 = normal, 2.0 = double, -1.0 = flip)
 * @param scale_y Vertical scale factor (1.0 = normal, 2.0 = double, -1.0 = flip)
 */
void agentite_sprite_draw_scaled(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                                 float x, float y, float scale_x, float scale_y);

/**
 * @brief Draw sprite with full transformation.
 *
 * @param sr           Sprite renderer (must not be NULL)
 * @param sprite       Sprite to draw (must not be NULL)
 * @param x            X position in screen/world coordinates
 * @param y            Y position in screen/world coordinates
 * @param scale_x      Horizontal scale factor
 * @param scale_y      Vertical scale factor
 * @param rotation_deg Rotation in degrees (clockwise)
 * @param origin_x     X origin for rotation, normalized 0-1 (overrides sprite origin)
 * @param origin_y     Y origin for rotation, normalized 0-1 (overrides sprite origin)
 */
void agentite_sprite_draw_ex(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                             float x, float y,
                             float scale_x, float scale_y,
                             float rotation_deg,
                             float origin_x, float origin_y);

/**
 * @brief Draw sprite with color tint.
 *
 * The tint color is multiplied with the sprite's texture color.
 *
 * @param sr     Sprite renderer (must not be NULL)
 * @param sprite Sprite to draw (must not be NULL)
 * @param x      X position in screen/world coordinates
 * @param y      Y position in screen/world coordinates
 * @param r      Red component (0-1)
 * @param g      Green component (0-1)
 * @param b      Blue component (0-1)
 * @param a      Alpha component (0-1, affects transparency)
 */
void agentite_sprite_draw_tinted(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                                 float x, float y,
                                 float r, float g, float b, float a);

/**
 * @brief Draw sprite with all options.
 *
 * Most flexible draw function combining transform and color tinting.
 *
 * @param sr           Sprite renderer (must not be NULL)
 * @param sprite       Sprite to draw (must not be NULL)
 * @param x            X position in screen/world coordinates
 * @param y            Y position in screen/world coordinates
 * @param scale_x      Horizontal scale factor
 * @param scale_y      Vertical scale factor
 * @param rotation_deg Rotation in degrees (clockwise)
 * @param origin_x     X origin for rotation, normalized 0-1
 * @param origin_y     Y origin for rotation, normalized 0-1
 * @param r            Red tint (0-1)
 * @param g            Green tint (0-1)
 * @param b            Blue tint (0-1)
 * @param a            Alpha (0-1)
 */
void agentite_sprite_draw_full(Agentite_SpriteRenderer *sr, const Agentite_Sprite *sprite,
                               float x, float y,
                               float scale_x, float scale_y,
                               float rotation_deg,
                               float origin_x, float origin_y,
                               float r, float g, float b, float a);

/**
 * @brief Flush current batch during rendering.
 *
 * Forces immediate rendering of queued sprites. Use when changing textures
 * mid-frame or for layering effects.
 *
 * @param sr   Sprite renderer (must not be NULL)
 * @param cmd  Command buffer (must not be NULL)
 * @param pass Active render pass (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread during render pass.
 */
void agentite_sprite_flush(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                           SDL_GPURenderPass *pass);

/**
 * @brief Upload sprite batch data to GPU.
 *
 * Transfers queued sprite vertex data to GPU buffers. Must be called
 * BEFORE beginning the render pass.
 *
 * @param sr  Sprite renderer (must not be NULL)
 * @param cmd Command buffer (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 *
 * @warning Calling this during a render pass will fail. Always upload before
 *          calling SDL_BeginGPURenderPass() or agentite_begin_render_pass().
 */
void agentite_sprite_upload(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/**
 * @brief Render sprite batch.
 *
 * Issues GPU draw calls for all queued sprites. Must be called
 * DURING an active render pass.
 *
 * @param sr   Sprite renderer (must not be NULL)
 * @param cmd  Command buffer (must not be NULL)
 * @param pass Active render pass (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sprite_render(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd,
                            SDL_GPURenderPass *pass);

/** @} */ /* end of sprite_rendering */

/** @defgroup sprite_camera Camera Integration
 *  @{ */

/** @brief Forward declaration for camera type (see camera.h) */
typedef struct Agentite_Camera Agentite_Camera;

/**
 * @brief Set camera for world-space sprite rendering.
 *
 * When a camera is set, sprite positions are transformed from world
 * coordinates to screen coordinates using the camera's view matrix.
 * Pass NULL to use screen-space mode (direct pixel coordinates).
 *
 * @param sr     Sprite renderer (must not be NULL)
 * @param camera Camera for world-space transform (NULL for screen-space)
 *
 * @code
 * // World-space rendering (camera follows player)
 * agentite_sprite_set_camera(sr, game_camera);
 * agentite_sprite_draw(sr, &enemy_sprite, enemy.world_x, enemy.world_y);
 *
 * // Screen-space rendering (HUD elements)
 * agentite_sprite_set_camera(sr, NULL);
 * agentite_sprite_draw(sr, &health_bar, 10, 10);
 * @endcode
 */
void agentite_sprite_set_camera(Agentite_SpriteRenderer *sr, Agentite_Camera *camera);

/**
 * @brief Get current camera.
 *
 * @param sr Sprite renderer (must not be NULL)
 * @return Current camera, or NULL if in screen-space mode
 */
Agentite_Camera *agentite_sprite_get_camera(const Agentite_SpriteRenderer *sr);

/** @} */ /* end of sprite_camera */

/** @defgroup sprite_profiler Profiler Integration
 *
 * Optional profiler integration for performance monitoring.
 *
 * @{
 */

/* Forward declaration */
struct Agentite_Profiler;

/**
 * @brief Set profiler for sprite renderer performance tracking.
 *
 * When a profiler is set, the sprite renderer will report:
 * - "sprite_upload" scope: Time spent uploading vertex data to GPU
 * - "sprite_render" scope: Time spent in render pass
 * - Draw call and batch counts
 *
 * @param sr       Sprite renderer (must not be NULL)
 * @param profiler Profiler instance, or NULL to disable profiling
 */
void agentite_sprite_set_profiler(Agentite_SpriteRenderer *sr,
                                  struct Agentite_Profiler *profiler);

/** @} */ /* end of sprite_profiler */

/** @defgroup sprite_rtt Render-to-Texture Functions
 *
 * These functions enable rendering sprites to a texture instead of the screen,
 * useful for post-processing effects, minimap generation, or off-screen rendering.
 *
 * @{
 */

/**
 * @brief Create a render target texture.
 *
 * Creates a texture that can be used as a render target. Unlike regular textures,
 * render targets can be rendered to using agentite_sprite_begin_render_to_texture().
 *
 * @param sr     Sprite renderer (must not be NULL)
 * @param width  Texture width in pixels (must be > 0)
 * @param height Texture height in pixels (must be > 0)
 *
 * @return Render target texture on success, NULL on failure
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_texture_destroy().
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_Texture *agentite_texture_create_render_target(Agentite_SpriteRenderer *sr,
                                                        int width, int height);

/**
 * @brief Begin rendering to a texture.
 *
 * Starts a render pass targeting the specified texture instead of the screen.
 * All subsequent sprite rendering will go to this texture until
 * agentite_sprite_end_render_to_texture() is called.
 *
 * @param sr      Sprite renderer (must not be NULL)
 * @param target  Render target texture (must be created with agentite_texture_create_render_target())
 * @param cmd     Command buffer (must not be NULL)
 * @param clear_r Red component for clear color (0-1)
 * @param clear_g Green component for clear color (0-1)
 * @param clear_b Blue component for clear color (0-1)
 * @param clear_a Alpha component for clear color (0-1)
 *
 * @return Render pass handle, or NULL on failure
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
SDL_GPURenderPass *agentite_sprite_begin_render_to_texture(Agentite_SpriteRenderer *sr,
                                                           Agentite_Texture *target,
                                                           SDL_GPUCommandBuffer *cmd,
                                                           float clear_r, float clear_g,
                                                           float clear_b, float clear_a);

/**
 * @brief Render sprites to the current texture target.
 *
 * Issues draw calls to render queued sprites to the texture target.
 *
 * @param sr   Sprite renderer (must not be NULL)
 * @param cmd  Command buffer (must not be NULL)
 * @param pass Render pass from agentite_sprite_begin_render_to_texture() (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sprite_render_to_texture(Agentite_SpriteRenderer *sr,
                                       SDL_GPUCommandBuffer *cmd,
                                       SDL_GPURenderPass *pass);

/**
 * @brief End render-to-texture pass.
 *
 * Completes the render-to-texture operation. After this call, the texture
 * can be used as a regular texture for sampling.
 *
 * @param pass Render pass to end (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sprite_end_render_to_texture(SDL_GPURenderPass *pass);

/** @} */ /* end of sprite_rtt */

/** @defgroup sprite_vignette Vignette Post-Process
 *
 * Optional vignette post-processing effect that darkens screen edges.
 * Useful for creating focus effects or cinematic looks.
 *
 * @{
 */

/**
 * @brief Check if vignette effect is available.
 *
 * The vignette pipeline may not be available if shader compilation failed
 * or the GPU doesn't support the required features.
 *
 * @param sr Sprite renderer (must not be NULL)
 * @return true if vignette can be used, false otherwise
 */
bool agentite_sprite_has_vignette(const Agentite_SpriteRenderer *sr);

/**
 * @brief Render scene texture with vignette effect.
 *
 * Draws the provided scene texture to the current render target with
 * a vignette (darkened edges) effect applied.
 *
 * @param sr            Sprite renderer (must not be NULL)
 * @param cmd           Command buffer (must not be NULL)
 * @param pass          Active render pass (must not be NULL)
 * @param scene_texture Texture containing the scene to apply vignette to
 *
 * @note NOT thread-safe. Must be called from main thread.
 * @note Requires agentite_sprite_has_vignette() to return true.
 */
void agentite_sprite_render_vignette(Agentite_SpriteRenderer *sr,
                                     SDL_GPUCommandBuffer *cmd,
                                     SDL_GPURenderPass *pass,
                                     Agentite_Texture *scene_texture);

/**
 * @brief Prepare fullscreen quad for post-processing.
 *
 * Sets up vertex data for a fullscreen quad in CPU buffers.
 * Call this before agentite_sprite_upload_fullscreen_quad().
 *
 * @param sr Sprite renderer (must not be NULL)
 */
void agentite_sprite_prepare_fullscreen_quad(Agentite_SpriteRenderer *sr);

/**
 * @brief Upload fullscreen quad to GPU.
 *
 * Transfers fullscreen quad vertex data to GPU. Must be called
 * BEFORE the render pass that uses the fullscreen quad.
 *
 * @param sr  Sprite renderer (must not be NULL)
 * @param cmd Command buffer (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_sprite_upload_fullscreen_quad(Agentite_SpriteRenderer *sr, SDL_GPUCommandBuffer *cmd);

/** @} */ /* end of sprite_vignette */

/** @defgroup sprite_assets Asset Handle Integration
 *
 * Integration with the Agentite asset registry for reference-counted
 * texture management and automatic lifetime handling.
 *
 * @{
 */

#include "agentite/asset.h"

/**
 * @brief Load texture and register with asset registry.
 *
 * Loads a texture and registers it with the asset registry for automatic
 * lifetime management. The texture can be looked up later via
 * agentite_asset_lookup() using the path as the asset ID.
 *
 * @param sr       Sprite renderer (must not be NULL)
 * @param registry Asset registry for lifetime management (must not be NULL)
 * @param path     File path (also used as asset ID, rejects path traversal)
 *
 * @return Asset handle on success, AGENTITE_INVALID_ASSET_HANDLE on failure
 *
 * @note The registry manages texture lifetime via reference counting.
 * @note NOT thread-safe. Must be called from main thread.
 *
 * @code
 * Agentite_AssetHandle h = agentite_texture_load_asset(sr, registry, "player.png");
 * Agentite_Texture *tex = agentite_texture_from_handle(registry, h);
 * // Use texture...
 * agentite_asset_release(registry, h);  // Decrement refcount
 * @endcode
 */
Agentite_AssetHandle agentite_texture_load_asset(Agentite_SpriteRenderer *sr,
                                                 Agentite_AssetRegistry *registry,
                                                 const char *path);

/**
 * @brief Get texture pointer from asset handle.
 *
 * Retrieves the raw texture pointer for use with sprite drawing functions.
 *
 * @param registry Asset registry containing the texture
 * @param handle   Asset handle from agentite_texture_load_asset()
 *
 * @return Texture pointer, or NULL if handle is invalid or not a texture
 */
Agentite_Texture *agentite_texture_from_handle(Agentite_AssetRegistry *registry,
                                               Agentite_AssetHandle handle);

/**
 * @brief Texture destructor callback for asset registry.
 *
 * Destructor function that can be registered with the asset registry to
 * automatically destroy textures when their reference count reaches zero.
 *
 * @param data     Texture pointer (cast to void*)
 * @param type     Asset type identifier
 * @param userdata Sprite renderer pointer (must be passed when registering)
 *
 * @code
 * // Register destructor for automatic cleanup
 * agentite_asset_set_destructor(registry, agentite_texture_asset_destructor, sprite_renderer);
 * @endcode
 *
 * @note For mixed asset types (textures + audio), create a custom destructor
 *       that dispatches based on the type parameter.
 */
void agentite_texture_asset_destructor(void *data, int type, void *userdata);

/** @} */ /* end of sprite_assets */

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SPRITE_H */
