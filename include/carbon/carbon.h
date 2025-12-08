#ifndef CARBON_H
#define CARBON_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

// C++ compatibility for memory allocation
#ifdef __cplusplus
#define CARBON_ALLOC(type) (type*)calloc(1, sizeof(type))
#define CARBON_ALLOC_ARRAY(type, count) (type*)calloc((count), sizeof(type))
#define CARBON_REALLOC(ptr, type, count) (type*)realloc((ptr), (count) * sizeof(type))
#define CARBON_MALLOC(size) malloc(size)
#define CARBON_CALLOC(count, size) calloc((count), (size))
#else
#define CARBON_ALLOC(type) (type*)calloc(1, sizeof(type))
#define CARBON_ALLOC_ARRAY(type, count) (type*)calloc((count), sizeof(type))
#define CARBON_REALLOC(ptr, type, count) (type*)realloc((ptr), (count) * sizeof(type))
#define CARBON_MALLOC(size) malloc(size)
#define CARBON_CALLOC(count, size) calloc((count), (size))
#endif

// Version info
#define CARBON_VERSION_MAJOR 0
#define CARBON_VERSION_MINOR 1
#define CARBON_VERSION_PATCH 0

/*============================================================================
 * Memory Ownership Conventions
 *============================================================================
 *
 * Carbon follows consistent memory ownership patterns across all APIs:
 *
 * 1. CREATE/DESTROY PAIRS:
 *    Functions named `carbon_*_create()` or `carbon_*_init()` that return
 *    pointers allocate memory. The caller OWNS the returned pointer and
 *    MUST call the corresponding `carbon_*_destroy()` or `carbon_*_shutdown()`
 *    function to free it.
 *
 *    Examples:
 *      Carbon_Engine *engine = carbon_init(&config);        // Caller owns
 *      carbon_shutdown(engine);                             // Must call
 *
 *      Carbon_TextRenderer *tr = carbon_text_init(gpu, win);  // Caller owns
 *      carbon_text_shutdown(tr);                              // Must call
 *
 * 2. LOAD FUNCTIONS:
 *    Functions named `carbon_*_load()` return allocated resources.
 *    The caller OWNS the returned pointer and MUST call the corresponding
 *    `carbon_*_destroy()` function.
 *
 *    Examples:
 *      Carbon_Texture *tex = carbon_texture_load(sr, path);  // Caller owns
 *      carbon_texture_destroy(sr, tex);                      // Must call
 *
 * 3. GET FUNCTIONS:
 *    Functions named `carbon_*_get_*()` return pointers to internally-owned
 *    data. The caller does NOT own these pointers and must NOT free them.
 *    The pointer is valid until the parent object is destroyed.
 *
 *    Examples:
 *      SDL_GPUDevice *gpu = carbon_get_gpu_device(engine);  // Engine owns
 *      // Do NOT call SDL_DestroyGPUDevice(gpu)
 *
 * 4. CONST CHAR* RETURNS:
 *    Functions returning `const char*` return either static strings or
 *    pointers to internal buffers. The caller must NOT free these.
 *
 * 5. NULL ON FAILURE:
 *    All allocating functions return NULL on failure. Always check return
 *    values. Use carbon_get_last_error() for error details.
 *
 *============================================================================*/

// Forward declarations
typedef struct Carbon_Engine Carbon_Engine;
typedef struct Carbon_Config Carbon_Config;

// Engine configuration
struct Carbon_Config {
    const char *window_title;
    int window_width;
    int window_height;
    bool fullscreen;
    bool vsync;
};

// Default configuration
#define CARBON_DEFAULT_CONFIG { \
    .window_title = "Carbon Engine", \
    .window_width = 1280, \
    .window_height = 720, \
    .fullscreen = false, \
    .vsync = true \
}

// Core engine functions
Carbon_Engine *carbon_init(const Carbon_Config *config);
void carbon_shutdown(Carbon_Engine *engine);
bool carbon_is_running(Carbon_Engine *engine);
void carbon_quit(Carbon_Engine *engine);

// Main loop functions
void carbon_begin_frame(Carbon_Engine *engine);
void carbon_end_frame(Carbon_Engine *engine);
float carbon_get_delta_time(Carbon_Engine *engine);
uint64_t carbon_get_frame_count(Carbon_Engine *engine);

// Event handling
void carbon_poll_events(Carbon_Engine *engine);

// Graphics (SDL_GPU)
SDL_GPUDevice *carbon_get_gpu_device(Carbon_Engine *engine);
SDL_Window *carbon_get_window(Carbon_Engine *engine);

// Acquire command buffer for the frame (call before render pass for copy operations)
SDL_GPUCommandBuffer *carbon_acquire_command_buffer(Carbon_Engine *engine);

// Render pass management
bool carbon_begin_render_pass(Carbon_Engine *engine, float r, float g, float b, float a);
bool carbon_begin_render_pass_no_clear(Carbon_Engine *engine);
void carbon_end_render_pass_no_submit(Carbon_Engine *engine);  // End render pass but keep cmd buffer
void carbon_end_render_pass(Carbon_Engine *engine);  // End render pass and submit

// Get current render pass and command buffer (for UI rendering)
SDL_GPURenderPass *carbon_get_render_pass(Carbon_Engine *engine);
SDL_GPUCommandBuffer *carbon_get_command_buffer(Carbon_Engine *engine);

// Core infrastructure
#include "carbon/error.h"
#include "carbon/log.h"
#include "carbon/math_safe.h"
#include "carbon/line.h"
#include "carbon/event.h"
#include "carbon/validate.h"
#include "carbon/containers.h"

// Strategy game systems
#include "carbon/command.h"
#include "carbon/turn.h"
#include "carbon/resource.h"
#include "carbon/condition.h"
#include "carbon/finances.h"
#include "carbon/loan.h"
#include "carbon/demand.h"
#include "carbon/incident.h"
#include "carbon/modifier.h"
#include "carbon/threshold.h"
#include "carbon/history.h"
#include "carbon/data_config.h"
#include "carbon/save.h"
#include "carbon/game_event.h"
#include "carbon/unlock.h"
#include "carbon/blueprint.h"
#include "carbon/game_speed.h"

// UI utilities
#include "carbon/notification.h"

#endif // CARBON_H
