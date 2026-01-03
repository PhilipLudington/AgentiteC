#ifndef AGENTITE_H
#define AGENTITE_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// C++ compatibility for memory allocation
#ifdef __cplusplus
#define AGENTITE_ALLOC(type) (type*)calloc(1, sizeof(type))
#define AGENTITE_ALLOC_ARRAY(type, count) (type*)calloc((count), sizeof(type))
#define AGENTITE_REALLOC(ptr, type, count) (type*)realloc((ptr), (count) * sizeof(type))
#define AGENTITE_MALLOC(size) malloc(size)
#define AGENTITE_CALLOC(count, size) calloc((count), (size))
#else
#define AGENTITE_ALLOC(type) (type*)calloc(1, sizeof(type))
#define AGENTITE_ALLOC_ARRAY(type, count) (type*)calloc((count), sizeof(type))
#define AGENTITE_REALLOC(ptr, type, count) (type*)realloc((ptr), (count) * sizeof(type))
#define AGENTITE_MALLOC(size) malloc(size)
#define AGENTITE_CALLOC(count, size) calloc((count), (size))
#endif

// Version info
#define AGENTITE_VERSION_MAJOR 0
#define AGENTITE_VERSION_MINOR 1
#define AGENTITE_VERSION_PATCH 0

/*============================================================================
 * Memory Ownership Conventions
 *============================================================================
 *
 * Carbon follows consistent memory ownership patterns across all APIs:
 *
 * 1. CREATE/DESTROY PAIRS:
 *    Functions named `agentite_*_create()` or `agentite_*_init()` that return
 *    pointers allocate memory. The caller OWNS the returned pointer and
 *    MUST call the corresponding `agentite_*_destroy()` or `agentite_*_shutdown()`
 *    function to free it.
 *
 *    Examples:
 *      Agentite_Engine *engine = agentite_init(&config);        // Caller owns
 *      agentite_shutdown(engine);                             // Must call
 *
 *      Agentite_TextRenderer *tr = agentite_text_init(gpu, win);  // Caller owns
 *      agentite_text_shutdown(tr);                              // Must call
 *
 * 2. LOAD FUNCTIONS:
 *    Functions named `agentite_*_load()` return allocated resources.
 *    The caller OWNS the returned pointer and MUST call the corresponding
 *    `agentite_*_destroy()` function.
 *
 *    Examples:
 *      Agentite_Texture *tex = agentite_texture_load(sr, path);  // Caller owns
 *      agentite_texture_destroy(sr, tex);                      // Must call
 *
 * 3. GET FUNCTIONS:
 *    Functions named `agentite_*_get_*()` return pointers to internally-owned
 *    data. The caller does NOT own these pointers and must NOT free them.
 *    The pointer is valid until the parent object is destroyed.
 *
 *    Examples:
 *      SDL_GPUDevice *gpu = agentite_get_gpu_device(engine);  // Engine owns
 *      // Do NOT call SDL_DestroyGPUDevice(gpu)
 *
 * 4. CONST CHAR* RETURNS:
 *    Functions returning `const char*` return either static strings or
 *    pointers to internal buffers. The caller must NOT free these.
 *
 * 5. NULL ON FAILURE:
 *    All allocating functions return NULL on failure. Always check return
 *    values. Use agentite_get_last_error() for error details.
 *
 *============================================================================*/

// Forward declarations
typedef struct Agentite_Engine Agentite_Engine;
typedef struct Agentite_Config Agentite_Config;

/**
 * Engine configuration.
 * Use AGENTITE_DEFAULT_CONFIG for sensible defaults.
 */
struct Agentite_Config {
    const char *window_title;   /* Window title bar text (copied) */
    int window_width;           /* Initial window width in pixels */
    int window_height;          /* Initial window height in pixels */
    bool fullscreen;            /* Start in fullscreen mode */
    bool resizable;             /* Allow window resizing */
    bool vsync;                 /* Enable vertical sync (recommended) */
};

// Default configuration
#define AGENTITE_DEFAULT_CONFIG { \
    .window_title = "Agentite Engine", \
    .window_width = 1280, \
    .window_height = 720, \
    .fullscreen = false, \
    .resizable = true, \
    .vsync = true \
}

/* ============================================================================
 * Core Engine Functions
 * ============================================================================ */

/**
 * Initialize the Agentite engine.
 * Creates window, initializes SDL3, and creates GPU device.
 * Caller OWNS the returned pointer and MUST call agentite_shutdown().
 *
 * @param config Configuration (NULL for defaults)
 * @return New engine instance, or NULL on failure. Check agentite_get_last_error().
 */
Agentite_Engine *agentite_init(const Agentite_Config *config);

/**
 * Shutdown the engine and free all resources.
 * Destroys GPU device, window, and deinitializes SDL.
 * Safe to call with NULL.
 *
 * @param engine Engine to shutdown
 */
void agentite_shutdown(Agentite_Engine *engine);

/**
 * Check if the engine is still running.
 *
 * @param engine Engine instance
 * @return false if quit was requested
 */
bool agentite_is_running(Agentite_Engine *engine);

/**
 * Request the engine to quit.
 * agentite_is_running() will return false after this call.
 *
 * @param engine Engine instance
 */
void agentite_quit(Agentite_Engine *engine);

/* ============================================================================
 * Main Loop Functions
 * ============================================================================ */

/**
 * Begin a new frame.
 * Call at the START of your game loop.
 * Updates timing (delta_time, frame_count).
 *
 * @param engine Engine instance
 */
void agentite_begin_frame(Agentite_Engine *engine);

/**
 * End the current frame.
 * Call at the END of your game loop.
 *
 * @param engine Engine instance
 */
void agentite_end_frame(Agentite_Engine *engine);

/**
 * Get time elapsed since last frame.
 *
 * @param engine Engine instance
 * @return Delta time in seconds
 */
float agentite_get_delta_time(Agentite_Engine *engine);

/**
 * Get total number of frames rendered.
 *
 * @param engine Engine instance
 * @return Frame count
 */
uint64_t agentite_get_frame_count(Agentite_Engine *engine);

/* ============================================================================
 * Event Handling
 * ============================================================================ */

/**
 * Poll and process all pending SDL events.
 * Handles quit events automatically.
 * Call after agentite_begin_frame().
 *
 * @param engine Engine instance
 */
void agentite_poll_events(Agentite_Engine *engine);

/* ============================================================================
 * Graphics (SDL_GPU)
 * ============================================================================ */

/**
 * Get the GPU device for custom rendering.
 * The device is owned by the engine; do NOT destroy it.
 *
 * @param engine Engine instance
 * @return GPU device (borrowed)
 */
SDL_GPUDevice *agentite_get_gpu_device(Agentite_Engine *engine);

/**
 * Get the window handle.
 * The window is owned by the engine; do NOT destroy it.
 *
 * @param engine Engine instance
 * @return Window handle (borrowed)
 */
SDL_Window *agentite_get_window(Agentite_Engine *engine);

/**
 * Acquire a command buffer for this frame.
 * Call BEFORE upload operations (sprite_upload, text_upload).
 * The command buffer is valid until agentite_end_render_pass().
 *
 * @param engine Engine instance
 * @return Command buffer (borrowed)
 */
SDL_GPUCommandBuffer *agentite_acquire_command_buffer(Agentite_Engine *engine);

/* ============================================================================
 * Render Pass Management
 * ============================================================================ */

/**
 * Begin a render pass with a clear color.
 * Call AFTER all upload operations.
 *
 * @param engine Engine instance
 * @param r Red component (0.0-1.0)
 * @param g Green component (0.0-1.0)
 * @param b Blue component (0.0-1.0)
 * @param a Alpha component (0.0-1.0)
 * @return true on success
 */
bool agentite_begin_render_pass(Agentite_Engine *engine, float r, float g, float b, float a);

/**
 * Begin a render pass without clearing (preserves existing content).
 * Use for additional render passes after the first.
 *
 * @param engine Engine instance
 * @return true on success
 */
bool agentite_begin_render_pass_no_clear(Agentite_Engine *engine);

/**
 * End the render pass but keep the command buffer open.
 * Use when you need multiple render passes per frame.
 *
 * @param engine Engine instance
 */
void agentite_end_render_pass_no_submit(Agentite_Engine *engine);

/**
 * End the render pass and submit the command buffer.
 * Call at the end of rendering, presents to screen.
 *
 * @param engine Engine instance
 */
void agentite_end_render_pass(Agentite_Engine *engine);

/**
 * Get the current render pass (during rendering).
 *
 * @param engine Engine instance
 * @return Current render pass, or NULL if not in a render pass
 */
SDL_GPURenderPass *agentite_get_render_pass(Agentite_Engine *engine);

/**
 * Get the current command buffer.
 *
 * @param engine Engine instance
 * @return Current command buffer, or NULL if not acquired
 */
SDL_GPUCommandBuffer *agentite_get_command_buffer(Agentite_Engine *engine);

/* ============================================================================
 * DPI and Screen Dimensions
 * ============================================================================ */

/**
 * Get the DPI scale factor.
 *
 * @param engine Engine instance
 * @return Scale factor (1.0 = standard, 2.0 = retina/high-DPI)
 */
float agentite_get_dpi_scale(Agentite_Engine *engine);

/**
 * Get the logical window size.
 * Use for game coordinates and UI layout.
 *
 * @param engine Engine instance
 * @param w Output width (can be NULL)
 * @param h Output height (can be NULL)
 */
void agentite_get_window_size(Agentite_Engine *engine, int *w, int *h);

/**
 * Get the physical drawable size in pixels.
 * Use for GPU operations and rendering.
 *
 * @param engine Engine instance
 * @param w Output width (can be NULL)
 * @param h Output height (can be NULL)
 */
void agentite_get_drawable_size(Agentite_Engine *engine, int *w, int *h);

// Core infrastructure
#include "agentite/error.h"
#include "agentite/log.h"
#include "agentite/math_safe.h"
#include "agentite/line.h"
#include "agentite/event.h"
#include "agentite/validate.h"
#include "agentite/containers.h"

// Strategy game systems
#include "agentite/command.h"
#include "agentite/turn.h"
#include "agentite/resource.h"
#include "agentite/condition.h"
#include "agentite/finances.h"
#include "agentite/loan.h"
#include "agentite/demand.h"
#include "agentite/incident.h"
#include "agentite/modifier.h"
#include "agentite/threshold.h"
#include "agentite/history.h"
#include "agentite/data_config.h"
#include "agentite/save.h"
#include "agentite/game_event.h"
#include "agentite/unlock.h"
#include "agentite/blueprint.h"
#include "agentite/game_speed.h"

// UI utilities
#include "agentite/notification.h"

// Graphics
#include "agentite/gizmos.h"

// Developer tools
#include "agentite/profiler.h"

#endif // AGENTITE_H
