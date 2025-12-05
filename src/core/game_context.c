#include "carbon/game_context.h"
#include "carbon/error.h"
#include <stdlib.h>
#include <string.h>

Carbon_GameContext *carbon_game_context_create(const Carbon_GameContextConfig *config) {
    /* Use default config if none provided */
    Carbon_GameContextConfig default_config = CARBON_GAME_CONTEXT_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    /* Allocate context */
    Carbon_GameContext *ctx = calloc(1, sizeof(Carbon_GameContext));
    if (!ctx) {
        carbon_set_error("Failed to allocate game context");
        return NULL;
    }

    /* Cache window dimensions */
    ctx->window_width = config->window_width;
    ctx->window_height = config->window_height;

    /* 1. Initialize core engine */
    Carbon_Config engine_config = {
        .window_title = config->window_title,
        .window_width = config->window_width,
        .window_height = config->window_height,
        .fullscreen = config->fullscreen,
        .vsync = config->vsync
    };

    ctx->engine = carbon_init(&engine_config);
    if (!ctx->engine) {
        carbon_set_error("Failed to initialize engine");
        goto error;
    }

    /* 2. Initialize sprite renderer */
    ctx->sprites = carbon_sprite_init(
        carbon_get_gpu_device(ctx->engine),
        carbon_get_window(ctx->engine)
    );
    if (!ctx->sprites) {
        carbon_set_error("Failed to initialize sprite renderer");
        goto error;
    }

    /* 3. Initialize text renderer */
    ctx->text = carbon_text_init(
        carbon_get_gpu_device(ctx->engine),
        carbon_get_window(ctx->engine)
    );
    if (!ctx->text) {
        carbon_set_error("Failed to initialize text renderer");
        goto error;
    }

    /* 4. Initialize camera */
    ctx->camera = carbon_camera_create(
        (float)config->window_width,
        (float)config->window_height
    );
    if (!ctx->camera) {
        carbon_set_error("Failed to create camera");
        goto error;
    }

    /* Connect camera to sprite renderer */
    carbon_sprite_set_camera(ctx->sprites, ctx->camera);

    /* 5. Initialize input system */
    ctx->input = carbon_input_init();
    if (!ctx->input) {
        carbon_set_error("Failed to initialize input system");
        goto error;
    }

    /* 6. Initialize audio system (optional) */
    if (config->enable_audio) {
        ctx->audio = carbon_audio_init();
        if (!ctx->audio) {
            carbon_set_error("Failed to initialize audio system");
            goto error;
        }
    }

    /* 7. Initialize ECS world (optional) */
    if (config->enable_ecs) {
        ctx->ecs = carbon_ecs_init();
        if (!ctx->ecs) {
            carbon_set_error("Failed to initialize ECS world");
            goto error;
        }
    }

    /* 8. Initialize UI system (optional) */
    if (config->enable_ui) {
        const char *ui_font = config->ui_font_path ? config->ui_font_path : config->font_path;
        float ui_size = config->ui_font_size > 0 ? config->ui_font_size : 16.0f;

        /* UI requires a font - use a default path if none specified */
        if (ui_font) {
            ctx->ui = cui_init(
                carbon_get_gpu_device(ctx->engine),
                carbon_get_window(ctx->engine),
                config->window_width,
                config->window_height,
                ui_font,
                ui_size
            );
            if (!ctx->ui) {
                carbon_set_error("Failed to initialize UI system");
                goto error;
            }
        }
    }

    /* 9. Load fonts (optional) */
    if (config->font_path) {
        ctx->font = carbon_font_load(ctx->text, config->font_path, config->font_size);
        /* Don't fail if font doesn't load - just log warning */
        if (!ctx->font) {
            SDL_Log("Warning: Could not load font '%s'", config->font_path);
        }
    }

    if (config->sdf_font_atlas && config->sdf_font_json) {
        ctx->sdf_font = carbon_sdf_font_load(ctx->text,
                                              config->sdf_font_atlas,
                                              config->sdf_font_json);
        /* Don't fail if SDF font doesn't load - just log warning */
        if (!ctx->sdf_font) {
            SDL_Log("Warning: Could not load SDF font '%s'", config->sdf_font_atlas);
        }
    }

    return ctx;

error:
    carbon_game_context_destroy(ctx);
    return NULL;
}

void carbon_game_context_destroy(Carbon_GameContext *ctx) {
    if (!ctx) return;

    /* Cleanup in reverse initialization order */

    /* Fonts */
    if (ctx->sdf_font && ctx->text) {
        carbon_sdf_font_destroy(ctx->text, ctx->sdf_font);
    }
    if (ctx->font && ctx->text) {
        carbon_font_destroy(ctx->text, ctx->font);
    }

    /* UI system */
    if (ctx->ui) {
        cui_shutdown(ctx->ui);
    }

    /* ECS world */
    if (ctx->ecs) {
        carbon_ecs_shutdown(ctx->ecs);
    }

    /* Audio system */
    if (ctx->audio) {
        carbon_audio_shutdown(ctx->audio);
    }

    /* Input system */
    if (ctx->input) {
        carbon_input_shutdown(ctx->input);
    }

    /* Camera */
    if (ctx->camera) {
        carbon_camera_destroy(ctx->camera);
    }

    /* Text renderer */
    if (ctx->text) {
        carbon_text_shutdown(ctx->text);
    }

    /* Sprite renderer */
    if (ctx->sprites) {
        carbon_sprite_shutdown(ctx->sprites);
    }

    /* Core engine */
    if (ctx->engine) {
        carbon_shutdown(ctx->engine);
    }

    free(ctx);
}

void carbon_game_context_begin_frame(Carbon_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;

    carbon_begin_frame(ctx->engine);
    carbon_input_begin_frame(ctx->input);

    /* Cache timing info */
    ctx->delta_time = carbon_get_delta_time(ctx->engine);
    ctx->frame_count = carbon_get_frame_count(ctx->engine);
}

void carbon_game_context_poll_events(Carbon_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        /* Let UI process the event first (if enabled) */
        if (ctx->ui && cui_process_event(ctx->ui, &event)) {
            continue;  /* UI consumed the event */
        }

        /* Let input system process the event */
        carbon_input_process_event(ctx->input, &event);

        /* Handle quit event */
        if (event.type == SDL_EVENT_QUIT) {
            carbon_quit(ctx->engine);
        }
    }

    /* Update input state (compute just_pressed/released) */
    carbon_input_update(ctx->input);
}

void carbon_game_context_end_frame(Carbon_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;
    carbon_end_frame(ctx->engine);
}

SDL_GPUCommandBuffer *carbon_game_context_begin_render(Carbon_GameContext *ctx) {
    if (!ctx || !ctx->engine) return NULL;

    /* Update camera matrices */
    carbon_camera_update(ctx->camera);

    /* Update audio system */
    if (ctx->audio) {
        carbon_audio_update(ctx->audio);
    }

    /* Acquire command buffer */
    return carbon_acquire_command_buffer(ctx->engine);
}

bool carbon_game_context_begin_render_pass(Carbon_GameContext *ctx,
                                            float r, float g, float b, float a) {
    if (!ctx || !ctx->engine) return false;
    return carbon_begin_render_pass(ctx->engine, r, g, b, a);
}

void carbon_game_context_end_render_pass(Carbon_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;
    carbon_end_render_pass(ctx->engine);
}

bool carbon_game_context_is_running(Carbon_GameContext *ctx) {
    return ctx && ctx->engine && carbon_is_running(ctx->engine);
}

void carbon_game_context_quit(Carbon_GameContext *ctx) {
    if (ctx && ctx->engine) {
        carbon_quit(ctx->engine);
    }
}
