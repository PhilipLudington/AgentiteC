#include "agentite/agentite.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <stdio.h>

struct Agentite_Engine {
    SDL_Window *window;
    SDL_GPUDevice *gpu_device;

    bool running;
    uint64_t frame_count;
    uint64_t last_frame_time;
    float delta_time;

    // Window dimensions and DPI scaling
    int logical_width;      // Window size in logical (CSS-like) pixels
    int logical_height;
    int physical_width;     // Drawable size in actual screen pixels
    int physical_height;
    float dpi_scale;        // physical / logical ratio (1.0 on standard, 2.0 on retina)

    // Current frame rendering state
    SDL_GPUCommandBuffer *cmd_buffer;
    SDL_GPURenderPass *render_pass;
    SDL_GPUTexture *swapchain_texture;  // Cached for multiple render passes
};

Agentite_Engine *agentite_init(const Agentite_Config *config) {
    // Use default config if none provided
    Agentite_Config default_config = AGENTITE_DEFAULT_CONFIG;
    if (!config) {
        config = &default_config;
    }

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        agentite_set_error_from_sdl("Failed to initialize SDL");
        return NULL;
    }

    // Allocate engine
    Agentite_Engine *engine = AGENTITE_ALLOC(Agentite_Engine);
    if (!engine) {
        agentite_set_error("Failed to allocate engine");
        SDL_Quit();
        return NULL;
    }

    // Create window
    SDL_WindowFlags window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config->fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }
    if (config->resizable) {
        window_flags |= SDL_WINDOW_RESIZABLE;
    }

    engine->window = SDL_CreateWindow(
        config->window_title,
        config->window_width,
        config->window_height,
        window_flags
    );

    if (!engine->window) {
        agentite_set_error_from_sdl("Failed to create window");
        free(engine);
        SDL_Quit();
        return NULL;
    }

    // Create GPU device - SDL3 will pick the best backend (Metal on macOS, Vulkan/D3D12 elsewhere)
    engine->gpu_device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_MSL | SDL_GPU_SHADERFORMAT_DXIL,
        true,  // debug mode
        NULL   // let SDL pick the best driver
    );

    if (!engine->gpu_device) {
        agentite_set_error_from_sdl("Failed to create GPU device");
        SDL_DestroyWindow(engine->window);
        free(engine);
        SDL_Quit();
        return NULL;
    }

    // Claim window for GPU rendering
    if (!SDL_ClaimWindowForGPUDevice(engine->gpu_device, engine->window)) {
        agentite_set_error_from_sdl("Failed to claim window for GPU");
        SDL_DestroyGPUDevice(engine->gpu_device);
        SDL_DestroyWindow(engine->window);
        free(engine);
        SDL_Quit();
        return NULL;
    }

    // Set vsync
    SDL_SetGPUSwapchainParameters(
        engine->gpu_device,
        engine->window,
        SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
        config->vsync ? SDL_GPU_PRESENTMODE_VSYNC : SDL_GPU_PRESENTMODE_IMMEDIATE
    );

    // Log GPU backend info
    const char *driver_name = SDL_GetGPUDeviceDriver(engine->gpu_device);
    SDL_Log("Agentite Engine initialized with GPU driver: %s", driver_name);

    // Query window dimensions and DPI scale
    SDL_GetWindowSize(engine->window, &engine->logical_width, &engine->logical_height);
    SDL_GetWindowSizeInPixels(engine->window, &engine->physical_width, &engine->physical_height);

    // Calculate DPI scale (avoid division by zero)
    if (engine->logical_width > 0) {
        engine->dpi_scale = (float)engine->physical_width / (float)engine->logical_width;
    } else {
        engine->dpi_scale = 1.0f;
    }

    SDL_Log("Window: %dx%d logical, %dx%d physical, DPI scale: %.2f",
            engine->logical_width, engine->logical_height,
            engine->physical_width, engine->physical_height,
            engine->dpi_scale);

    engine->running = true;
    engine->frame_count = 0;
    engine->last_frame_time = SDL_GetPerformanceCounter();
    engine->delta_time = 0.0f;

    return engine;
}

void agentite_shutdown(Agentite_Engine *engine) {
    if (!engine) return;

    if (engine->gpu_device) {
        SDL_ReleaseWindowFromGPUDevice(engine->gpu_device, engine->window);
        SDL_DestroyGPUDevice(engine->gpu_device);
    }

    if (engine->window) {
        SDL_DestroyWindow(engine->window);
    }

    free(engine);
    SDL_Quit();

    SDL_Log("Agentite Engine shutdown complete");
}

bool agentite_is_running(Agentite_Engine *engine) {
    return engine && engine->running;
}

void agentite_quit(Agentite_Engine *engine) {
    if (engine) {
        engine->running = false;
    }
}

void agentite_poll_events(Agentite_Engine *engine) {
    if (!engine) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_EVENT_QUIT:
                engine->running = false;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    engine->running = false;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                // Logical window size changed
                engine->logical_width = event.window.data1;
                engine->logical_height = event.window.data2;
                // Recalculate DPI scale
                if (engine->logical_width > 0) {
                    engine->dpi_scale = (float)engine->physical_width / (float)engine->logical_width;
                }
                SDL_Log("Window resized: %dx%d logical", engine->logical_width, engine->logical_height);
                break;
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                // Physical pixel size changed (DPI change, display change, etc.)
                engine->physical_width = event.window.data1;
                engine->physical_height = event.window.data2;
                // Recalculate DPI scale
                if (engine->logical_width > 0) {
                    engine->dpi_scale = (float)engine->physical_width / (float)engine->logical_width;
                }
                SDL_Log("Pixel size changed: %dx%d physical, DPI scale: %.2f",
                        engine->physical_width, engine->physical_height, engine->dpi_scale);
                break;
            default:
                break;
        }
    }
}

void agentite_begin_frame(Agentite_Engine *engine) {
    if (!engine) return;

    // Calculate delta time
    uint64_t current_time = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    engine->delta_time = (float)(current_time - engine->last_frame_time) / (float)frequency;
    engine->last_frame_time = current_time;
}

void agentite_end_frame(Agentite_Engine *engine) {
    if (!engine) return;
    engine->frame_count++;
}

float agentite_get_delta_time(Agentite_Engine *engine) {
    return engine ? engine->delta_time : 0.0f;
}

uint64_t agentite_get_frame_count(Agentite_Engine *engine) {
    return engine ? engine->frame_count : 0;
}

SDL_GPUDevice *agentite_get_gpu_device(Agentite_Engine *engine) {
    return engine ? engine->gpu_device : NULL;
}

SDL_Window *agentite_get_window(Agentite_Engine *engine) {
    return engine ? engine->window : NULL;
}

SDL_GPUCommandBuffer *agentite_acquire_command_buffer(Agentite_Engine *engine) {
    if (!engine || !engine->gpu_device) return NULL;

    // Only acquire if we don't already have one
    if (!engine->cmd_buffer) {
        engine->cmd_buffer = SDL_AcquireGPUCommandBuffer(engine->gpu_device);
        if (!engine->cmd_buffer) {
            agentite_set_error_from_sdl("Failed to acquire command buffer");
            return NULL;
        }
    }
    return engine->cmd_buffer;
}

bool agentite_begin_render_pass(Agentite_Engine *engine, float r, float g, float b, float a) {
    if (!engine || !engine->gpu_device) return false;

    // Acquire command buffer if not already acquired
    if (!engine->cmd_buffer) {
        engine->cmd_buffer = SDL_AcquireGPUCommandBuffer(engine->gpu_device);
        if (!engine->cmd_buffer) {
            agentite_set_error_from_sdl("Failed to acquire command buffer");
            return false;
        }
    }

    // Acquire swapchain texture
    SDL_GPUTexture *swapchain_texture = NULL;
    Uint32 swapchain_w = 0, swapchain_h = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            engine->cmd_buffer,
            engine->window,
            &swapchain_texture,
            &swapchain_w,
            &swapchain_h)) {
        agentite_set_error_from_sdl("Failed to acquire swapchain texture");
        SDL_CancelGPUCommandBuffer(engine->cmd_buffer);
        engine->cmd_buffer = NULL;
        return false;
    }

    if (!swapchain_texture) {
        // Window minimized or not ready, skip this frame
        SDL_CancelGPUCommandBuffer(engine->cmd_buffer);
        engine->cmd_buffer = NULL;
        return false;
    }

    // Cache texture for subsequent render passes
    engine->swapchain_texture = swapchain_texture;

    // Debug: Log swapchain dimensions (first time only)
    static bool swapchain_logged = false;
    if (!swapchain_logged) {
        SDL_Log("DEBUG: Swapchain texture actual size: %u x %u", swapchain_w, swapchain_h);
        SDL_Log("DEBUG: physical_width/height we're using: %d x %d", engine->physical_width, engine->physical_height);
        swapchain_logged = true;
    }

    // Set up color target with clear color
    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = swapchain_texture;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = { r, g, b, a };

    // Begin render pass
    engine->render_pass = SDL_BeginGPURenderPass(
        engine->cmd_buffer,
        &color_target,
        1,
        NULL
    );

    if (!engine->render_pass) {
        agentite_set_error_from_sdl("Failed to begin render pass");
        SDL_CancelGPUCommandBuffer(engine->cmd_buffer);
        engine->cmd_buffer = NULL;
        engine->swapchain_texture = NULL;
        return false;
    }

    // Set viewport to PHYSICAL dimensions to match swapchain texture size
    // Ortho projections use logical coords, viewport maps to physical pixels
    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = (float)engine->physical_width;
    viewport.h = (float)engine->physical_height;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(engine->render_pass, &viewport);

    SDL_Rect scissor = {};
    scissor.x = 0;
    scissor.y = 0;
    scissor.w = engine->physical_width;
    scissor.h = engine->physical_height;
    SDL_SetGPUScissor(engine->render_pass, &scissor);

    static bool logged = false;
    if (!logged) {
        SDL_Log("Swapchain viewport set to: %d x %d (PHYSICAL)", engine->physical_width, engine->physical_height);
        logged = true;
    }

    return true;
}

bool agentite_begin_render_pass_no_clear(Agentite_Engine *engine) {
    if (!engine || !engine->gpu_device) return false;

    // Must have command buffer and swapchain texture from previous render pass
    if (!engine->cmd_buffer || !engine->swapchain_texture) {
        agentite_set_error("No command buffer or swapchain texture - call agentite_begin_render_pass first");
        return false;
    }

    // Set up color target with LOAD (preserve existing content)
    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = engine->swapchain_texture;
    color_target.load_op = SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    // Begin render pass
    engine->render_pass = SDL_BeginGPURenderPass(
        engine->cmd_buffer,
        &color_target,
        1,
        NULL
    );

    if (!engine->render_pass) {
        agentite_set_error_from_sdl("Failed to begin render pass (no clear)");
        return false;
    }

    // Set viewport to physical dimensions (critical after render-to-texture passes)
    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = (float)engine->physical_width;
    viewport.h = (float)engine->physical_height;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(engine->render_pass, &viewport);

    // Also set scissor rect to match viewport
    SDL_Rect scissor = {};
    scissor.x = 0;
    scissor.y = 0;
    scissor.w = engine->physical_width;
    scissor.h = engine->physical_height;
    SDL_SetGPUScissor(engine->render_pass, &scissor);

    return true;
}

bool agentite_begin_render_pass_to_texture(Agentite_Engine *engine,
                                            SDL_GPUTexture *target,
                                            int width, int height,
                                            float r, float g, float b, float a) {
    if (!engine || !engine->gpu_device) return false;
    if (!target) {
        agentite_set_error("Target texture is NULL");
        return false;
    }

    // Acquire command buffer if not already acquired
    if (!engine->cmd_buffer) {
        engine->cmd_buffer = SDL_AcquireGPUCommandBuffer(engine->gpu_device);
        if (!engine->cmd_buffer) {
            agentite_set_error_from_sdl("Failed to acquire command buffer");
            return false;
        }
    }

    // Set up color target with clear color
    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = target;
    color_target.load_op = SDL_GPU_LOADOP_CLEAR;
    color_target.store_op = SDL_GPU_STOREOP_STORE;
    color_target.clear_color = { r, g, b, a };

    // Begin render pass
    engine->render_pass = SDL_BeginGPURenderPass(
        engine->cmd_buffer,
        &color_target,
        1,
        NULL
    );

    if (!engine->render_pass) {
        agentite_set_error_from_sdl("Failed to begin render pass to texture");
        return false;
    }

    // Set viewport to match target texture dimensions
    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = (float)width;
    viewport.h = (float)height;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(engine->render_pass, &viewport);

    // Also set scissor rect to match viewport
    SDL_Rect scissor = {};
    scissor.x = 0;
    scissor.y = 0;
    scissor.w = width;
    scissor.h = height;
    SDL_SetGPUScissor(engine->render_pass, &scissor);

    static bool logged_rtt = false;
    if (!logged_rtt) {
        SDL_Log("Render-to-texture viewport set to: %d x %d", width, height);
        logged_rtt = true;
    }

    return true;
}

bool agentite_begin_render_pass_to_texture_no_clear(Agentite_Engine *engine,
                                                     SDL_GPUTexture *target,
                                                     int width, int height) {
    if (!engine || !engine->gpu_device) return false;
    if (!target) {
        agentite_set_error("Target texture is NULL");
        return false;
    }

    // Acquire command buffer if not already acquired
    if (!engine->cmd_buffer) {
        engine->cmd_buffer = SDL_AcquireGPUCommandBuffer(engine->gpu_device);
        if (!engine->cmd_buffer) {
            agentite_set_error_from_sdl("Failed to acquire command buffer");
            return false;
        }
    }

    // Set up color target with LOAD (preserve existing content)
    SDL_GPUColorTargetInfo color_target = {};
    color_target.texture = target;
    color_target.load_op = SDL_GPU_LOADOP_LOAD;
    color_target.store_op = SDL_GPU_STOREOP_STORE;

    // Begin render pass
    engine->render_pass = SDL_BeginGPURenderPass(
        engine->cmd_buffer,
        &color_target,
        1,
        NULL
    );

    if (!engine->render_pass) {
        agentite_set_error_from_sdl("Failed to begin render pass to texture (no clear)");
        return false;
    }

    // Set viewport to match target texture dimensions
    SDL_GPUViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.w = (float)width;
    viewport.h = (float)height;
    viewport.min_depth = 0.0f;
    viewport.max_depth = 1.0f;
    SDL_SetGPUViewport(engine->render_pass, &viewport);

    // Also set scissor rect to match viewport
    SDL_Rect scissor = {};
    scissor.x = 0;
    scissor.y = 0;
    scissor.w = width;
    scissor.h = height;
    SDL_SetGPUScissor(engine->render_pass, &scissor);

    return true;
}

void agentite_end_render_pass_no_submit(Agentite_Engine *engine) {
    if (!engine) return;

    if (engine->render_pass) {
        SDL_EndGPURenderPass(engine->render_pass);
        engine->render_pass = NULL;
    }
    // Keep cmd_buffer and swapchain_texture for next render pass
}

void agentite_end_render_pass(Agentite_Engine *engine) {
    if (!engine) return;

    if (engine->render_pass) {
        SDL_EndGPURenderPass(engine->render_pass);
        engine->render_pass = NULL;
    }

    if (engine->cmd_buffer) {
        SDL_SubmitGPUCommandBuffer(engine->cmd_buffer);
        engine->cmd_buffer = NULL;
    }
    engine->swapchain_texture = NULL;
}

SDL_GPURenderPass *agentite_get_render_pass(Agentite_Engine *engine) {
    return engine ? engine->render_pass : NULL;
}

SDL_GPUCommandBuffer *agentite_get_command_buffer(Agentite_Engine *engine) {
    return engine ? engine->cmd_buffer : NULL;
}

/* DPI and dimension functions */

float agentite_get_dpi_scale(Agentite_Engine *engine) {
    return engine ? engine->dpi_scale : 1.0f;
}

void agentite_get_window_size(Agentite_Engine *engine, int *w, int *h) {
    if (engine) {
        if (w) *w = engine->logical_width;
        if (h) *h = engine->logical_height;
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}

void agentite_get_drawable_size(Agentite_Engine *engine, int *w, int *h) {
    if (engine) {
        if (w) *w = engine->physical_width;
        if (h) *h = engine->physical_height;
    } else {
        if (w) *w = 0;
        if (h) *h = 0;
    }
}
