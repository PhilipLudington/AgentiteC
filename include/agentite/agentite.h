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

// Engine configuration
struct Agentite_Config {
    const char *window_title;
    int window_width;
    int window_height;
    bool fullscreen;
    bool resizable;
    bool vsync;
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

// Core engine functions
Agentite_Engine *agentite_init(const Agentite_Config *config);
void agentite_shutdown(Agentite_Engine *engine);
bool agentite_is_running(Agentite_Engine *engine);
void agentite_quit(Agentite_Engine *engine);

// Main loop functions
void agentite_begin_frame(Agentite_Engine *engine);
void agentite_end_frame(Agentite_Engine *engine);
float agentite_get_delta_time(Agentite_Engine *engine);
uint64_t agentite_get_frame_count(Agentite_Engine *engine);

// Event handling
void agentite_poll_events(Agentite_Engine *engine);

// Graphics (SDL_GPU)
SDL_GPUDevice *agentite_get_gpu_device(Agentite_Engine *engine);
SDL_Window *agentite_get_window(Agentite_Engine *engine);

// Acquire command buffer for the frame (call before render pass for copy operations)
SDL_GPUCommandBuffer *agentite_acquire_command_buffer(Agentite_Engine *engine);

// Render pass management
bool agentite_begin_render_pass(Agentite_Engine *engine, float r, float g, float b, float a);
bool agentite_begin_render_pass_no_clear(Agentite_Engine *engine);
void agentite_end_render_pass_no_submit(Agentite_Engine *engine);  // End render pass but keep cmd buffer
void agentite_end_render_pass(Agentite_Engine *engine);  // End render pass and submit

// Get current render pass and command buffer (for UI rendering)
SDL_GPURenderPass *agentite_get_render_pass(Agentite_Engine *engine);
SDL_GPUCommandBuffer *agentite_get_command_buffer(Agentite_Engine *engine);

// DPI and screen dimension functions
// Returns the DPI scale factor (1.0 on standard displays, 2.0 on retina/high-DPI)
float agentite_get_dpi_scale(Agentite_Engine *engine);
// Get logical window size (use for game coordinates, UI layout)
void agentite_get_window_size(Agentite_Engine *engine, int *w, int *h);
// Get physical drawable size in actual pixels (use for rendering, GPU operations)
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

#endif // AGENTITE_H
