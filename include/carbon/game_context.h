#ifndef CARBON_GAME_CONTEXT_H
#define CARBON_GAME_CONTEXT_H

#include "carbon/carbon.h"
#include "carbon/sprite.h"
#include "carbon/text.h"
#include "carbon/camera.h"
#include "carbon/input.h"
#include "carbon/audio.h"
#include "carbon/ecs.h"
#include "carbon/ui.h"
#include "carbon/tilemap.h"
#include "carbon/pathfinding.h"
#include "carbon/animation.h"

/**
 * Carbon Game Context
 *
 * Provides unified access to all engine systems. Create once at startup
 * and pass to game code. Handles proper initialization and cleanup order.
 *
 * Usage:
 *   Carbon_GameContextConfig cfg = CARBON_GAME_CONTEXT_DEFAULT;
 *   cfg.window_title = "My Game";
 *   cfg.window_width = 1920;
 *
 *   Carbon_GameContext *ctx = carbon_game_context_create(&cfg);
 *   if (!ctx) {
 *       printf("Failed: %s\n", carbon_get_last_error());
 *       return 1;
 *   }
 *
 *   while (carbon_is_running(ctx->engine)) {
 *       carbon_game_context_begin_frame(ctx);
 *       // ... game logic ...
 *       carbon_game_context_end_frame(ctx);
 *   }
 *
 *   carbon_game_context_destroy(ctx);
 */

/* Forward declarations */
typedef struct Carbon_GameContext Carbon_GameContext;
typedef struct Carbon_GameContextConfig Carbon_GameContextConfig;

/**
 * Configuration for creating a game context.
 * Use CARBON_GAME_CONTEXT_DEFAULT for sensible defaults.
 */
struct Carbon_GameContextConfig {
    /* Window settings */
    const char *window_title;
    int window_width;
    int window_height;
    bool fullscreen;
    bool vsync;

    /* Font settings (optional - pass NULL to skip text rendering) */
    const char *font_path;          /* Path to TTF font for bitmap text */
    float font_size;                /* Font size in pixels */
    const char *ui_font_path;       /* Path to TTF font for UI (can be same as font_path) */
    float ui_font_size;             /* UI font size in pixels */

    /* SDF font settings (optional - pass NULL to skip SDF text) */
    const char *sdf_font_atlas;     /* Path to SDF/MSDF font atlas PNG */
    const char *sdf_font_json;      /* Path to SDF/MSDF font metrics JSON */

    /* Feature flags */
    bool enable_ecs;                /* Initialize ECS world */
    bool enable_audio;              /* Initialize audio system */
    bool enable_ui;                 /* Initialize UI system */
};

/**
 * Default configuration with sensible defaults.
 * Enables all systems with 1280x720 window.
 */
#define CARBON_GAME_CONTEXT_DEFAULT { \
    .window_title = "Carbon Game", \
    .window_width = 1280, \
    .window_height = 720, \
    .fullscreen = false, \
    .vsync = true, \
    .font_path = NULL, \
    .font_size = 16.0f, \
    .ui_font_path = NULL, \
    .ui_font_size = 16.0f, \
    .sdf_font_atlas = NULL, \
    .sdf_font_json = NULL, \
    .enable_ecs = true, \
    .enable_audio = true, \
    .enable_ui = true \
}

/**
 * Game context containing all engine systems.
 * All pointers are valid after successful carbon_game_context_create().
 * Optional systems may be NULL if disabled in config.
 */
struct Carbon_GameContext {
    /* Core engine (always valid) */
    Carbon_Engine *engine;

    /* Graphics systems (always valid) */
    Carbon_SpriteRenderer *sprites;
    Carbon_TextRenderer *text;
    Carbon_Camera *camera;

    /* Input system (always valid) */
    Carbon_Input *input;

    /* Optional systems (may be NULL if disabled) */
    Carbon_Audio *audio;            /* NULL if enable_audio = false */
    Carbon_World *ecs;              /* NULL if enable_ecs = false */
    CUI_Context *ui;                /* NULL if enable_ui = false */

    /* Fonts (may be NULL if paths not provided) */
    Carbon_Font *font;              /* Bitmap font for text rendering */
    Carbon_SDFFont *sdf_font;       /* SDF/MSDF font for sharp text */

    /* Frame timing */
    float delta_time;               /* Time since last frame in seconds */
    uint64_t frame_count;           /* Total frames rendered */

    /* Window dimensions (cached for convenience) */
    int window_width;
    int window_height;
};

/**
 * Create a game context with all engine systems initialized.
 *
 * Initialization order:
 * 1. Core engine (SDL, window, GPU)
 * 2. Sprite renderer
 * 3. Text renderer
 * 4. Camera
 * 5. Input system
 * 6. Audio system (if enabled)
 * 7. ECS world (if enabled)
 * 8. UI system (if enabled)
 * 9. Fonts (if paths provided)
 *
 * On failure, all partially initialized systems are cleaned up,
 * and the error can be retrieved via carbon_get_last_error().
 *
 * @param config Configuration for the context (use NULL for defaults)
 * @return New game context, or NULL on failure
 */
Carbon_GameContext *carbon_game_context_create(const Carbon_GameContextConfig *config);

/**
 * Destroy a game context and all its systems.
 *
 * Cleanup order is the reverse of initialization.
 * Safe to call with NULL.
 *
 * @param ctx Game context to destroy
 */
void carbon_game_context_destroy(Carbon_GameContext *ctx);

/**
 * Begin a new frame.
 * Call this at the start of your game loop.
 *
 * This function:
 * - Calls carbon_begin_frame() for timing
 * - Calls carbon_input_begin_frame() to reset input state
 *
 * @param ctx Game context
 */
void carbon_game_context_begin_frame(Carbon_GameContext *ctx);

/**
 * Poll and dispatch events.
 * Call this after begin_frame to process input.
 *
 * This function:
 * - Polls all SDL events
 * - Dispatches to UI first (if enabled)
 * - Dispatches remaining events to input system
 * - Updates input state
 * - Handles quit events automatically
 *
 * @param ctx Game context
 */
void carbon_game_context_poll_events(Carbon_GameContext *ctx);

/**
 * End the current frame.
 * Call this at the end of your game loop.
 *
 * This function:
 * - Calls carbon_end_frame() to increment frame counter
 *
 * @param ctx Game context
 */
void carbon_game_context_end_frame(Carbon_GameContext *ctx);

/**
 * Begin rendering.
 * Call this before any render operations.
 *
 * This function:
 * - Updates the camera
 * - Acquires the GPU command buffer
 * - Updates the context's cached command buffer
 *
 * @param ctx Game context
 * @return Command buffer for GPU operations, or NULL on failure
 */
SDL_GPUCommandBuffer *carbon_game_context_begin_render(Carbon_GameContext *ctx);

/**
 * Begin the render pass with a clear color.
 * Call this after begin_render and uploading any GPU data.
 *
 * @param ctx Game context
 * @param r Red component (0-1)
 * @param g Green component (0-1)
 * @param b Blue component (0-1)
 * @param a Alpha component (0-1)
 * @return true on success, false on failure
 */
bool carbon_game_context_begin_render_pass(Carbon_GameContext *ctx,
                                            float r, float g, float b, float a);

/**
 * End the render pass and submit.
 * Call this after all render operations are complete.
 *
 * @param ctx Game context
 */
void carbon_game_context_end_render_pass(Carbon_GameContext *ctx);

/**
 * Check if the game is still running.
 *
 * @param ctx Game context
 * @return true if running, false if quit was requested
 */
bool carbon_game_context_is_running(Carbon_GameContext *ctx);

/**
 * Request the game to quit.
 *
 * @param ctx Game context
 */
void carbon_game_context_quit(Carbon_GameContext *ctx);

#endif /* CARBON_GAME_CONTEXT_H */
