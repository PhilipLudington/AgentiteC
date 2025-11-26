#include "carbon/carbon.h"
#include <stdlib.h>
#include <stdio.h>

struct Carbon_Engine {
    SDL_Window *window;
    SDL_GPUDevice *gpu_device;

    bool running;
    uint64_t frame_count;
    uint64_t last_frame_time;
    float delta_time;

    // Current frame rendering state
    SDL_GPUCommandBuffer *cmd_buffer;
    SDL_GPURenderPass *render_pass;
};

Carbon_Engine *carbon_init(const Carbon_Config *config) {
    // Use default config if none provided
    Carbon_Config default_config = CARBON_DEFAULT_CONFIG;
    if (!config) {
        config = &default_config;
    }

    // Initialize SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD)) {
        SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
        return NULL;
    }

    // Allocate engine
    Carbon_Engine *engine = calloc(1, sizeof(Carbon_Engine));
    if (!engine) {
        SDL_Log("Failed to allocate engine");
        SDL_Quit();
        return NULL;
    }

    // Create window
    SDL_WindowFlags window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (config->fullscreen) {
        window_flags |= SDL_WINDOW_FULLSCREEN;
    }

    engine->window = SDL_CreateWindow(
        config->window_title,
        config->window_width,
        config->window_height,
        window_flags
    );

    if (!engine->window) {
        SDL_Log("Failed to create window: %s", SDL_GetError());
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
        SDL_Log("Failed to create GPU device: %s", SDL_GetError());
        SDL_DestroyWindow(engine->window);
        free(engine);
        SDL_Quit();
        return NULL;
    }

    // Claim window for GPU rendering
    if (!SDL_ClaimWindowForGPUDevice(engine->gpu_device, engine->window)) {
        SDL_Log("Failed to claim window for GPU: %s", SDL_GetError());
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
    SDL_Log("Carbon Engine initialized with GPU driver: %s", driver_name);

    engine->running = true;
    engine->frame_count = 0;
    engine->last_frame_time = SDL_GetPerformanceCounter();
    engine->delta_time = 0.0f;

    return engine;
}

void carbon_shutdown(Carbon_Engine *engine) {
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

    SDL_Log("Carbon Engine shutdown complete");
}

bool carbon_is_running(Carbon_Engine *engine) {
    return engine && engine->running;
}

void carbon_quit(Carbon_Engine *engine) {
    if (engine) {
        engine->running = false;
    }
}

void carbon_poll_events(Carbon_Engine *engine) {
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
            default:
                break;
        }
    }
}

void carbon_begin_frame(Carbon_Engine *engine) {
    if (!engine) return;

    // Calculate delta time
    uint64_t current_time = SDL_GetPerformanceCounter();
    uint64_t frequency = SDL_GetPerformanceFrequency();
    engine->delta_time = (float)(current_time - engine->last_frame_time) / (float)frequency;
    engine->last_frame_time = current_time;
}

void carbon_end_frame(Carbon_Engine *engine) {
    if (!engine) return;
    engine->frame_count++;
}

float carbon_get_delta_time(Carbon_Engine *engine) {
    return engine ? engine->delta_time : 0.0f;
}

uint64_t carbon_get_frame_count(Carbon_Engine *engine) {
    return engine ? engine->frame_count : 0;
}

SDL_GPUDevice *carbon_get_gpu_device(Carbon_Engine *engine) {
    return engine ? engine->gpu_device : NULL;
}

SDL_Window *carbon_get_window(Carbon_Engine *engine) {
    return engine ? engine->window : NULL;
}

SDL_GPUCommandBuffer *carbon_acquire_command_buffer(Carbon_Engine *engine) {
    if (!engine || !engine->gpu_device) return NULL;

    // Only acquire if we don't already have one
    if (!engine->cmd_buffer) {
        engine->cmd_buffer = SDL_AcquireGPUCommandBuffer(engine->gpu_device);
        if (!engine->cmd_buffer) {
            SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
            return NULL;
        }
    }
    return engine->cmd_buffer;
}

bool carbon_begin_render_pass(Carbon_Engine *engine, float r, float g, float b, float a) {
    if (!engine || !engine->gpu_device) return false;

    // Acquire command buffer if not already acquired
    if (!engine->cmd_buffer) {
        engine->cmd_buffer = SDL_AcquireGPUCommandBuffer(engine->gpu_device);
        if (!engine->cmd_buffer) {
            SDL_Log("Failed to acquire command buffer: %s", SDL_GetError());
            return false;
        }
    }

    // Acquire swapchain texture
    SDL_GPUTexture *swapchain_texture = NULL;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            engine->cmd_buffer,
            engine->window,
            &swapchain_texture,
            NULL,
            NULL)) {
        SDL_Log("Failed to acquire swapchain texture: %s", SDL_GetError());
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

    // Set up color target with clear color
    SDL_GPUColorTargetInfo color_target = {
        .texture = swapchain_texture,
        .load_op = SDL_GPU_LOADOP_CLEAR,
        .store_op = SDL_GPU_STOREOP_STORE,
        .clear_color = { r, g, b, a }
    };

    // Begin render pass
    engine->render_pass = SDL_BeginGPURenderPass(
        engine->cmd_buffer,
        &color_target,
        1,
        NULL
    );

    if (!engine->render_pass) {
        SDL_Log("Failed to begin render pass: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(engine->cmd_buffer);
        engine->cmd_buffer = NULL;
        return false;
    }

    return true;
}

void carbon_end_render_pass(Carbon_Engine *engine) {
    if (!engine) return;

    if (engine->render_pass) {
        SDL_EndGPURenderPass(engine->render_pass);
        engine->render_pass = NULL;
    }

    if (engine->cmd_buffer) {
        SDL_SubmitGPUCommandBuffer(engine->cmd_buffer);
        engine->cmd_buffer = NULL;
    }
}

SDL_GPURenderPass *carbon_get_render_pass(Carbon_Engine *engine) {
    return engine ? engine->render_pass : NULL;
}

SDL_GPUCommandBuffer *carbon_get_command_buffer(Carbon_Engine *engine) {
    return engine ? engine->cmd_buffer : NULL;
}
