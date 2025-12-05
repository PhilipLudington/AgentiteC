#ifndef CARBON_H
#define CARBON_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

// Version info
#define CARBON_VERSION_MAJOR 0
#define CARBON_VERSION_MINOR 1
#define CARBON_VERSION_PATCH 0

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
void carbon_end_render_pass(Carbon_Engine *engine);

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
