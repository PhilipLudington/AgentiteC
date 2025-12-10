#include "agentite/agentite.h"
#include "agentite/game_context.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>

Agentite_GameContext *agentite_game_context_create(const Agentite_GameContextConfig *config) {
    /* Use default config if none provided */
    Agentite_GameContextConfig default_config = AGENTITE_GAME_CONTEXT_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    /* Allocate context */
    Agentite_GameContext *ctx = AGENTITE_ALLOC(Agentite_GameContext);
    if (!ctx) {
        agentite_set_error("Failed to allocate game context");
        return NULL;
    }

    /* Cache window dimensions */
    ctx->window_width = config->window_width;
    ctx->window_height = config->window_height;

    /* 1. Initialize core engine */
    Agentite_Config engine_config = {
        .window_title = config->window_title,
        .window_width = config->window_width,
        .window_height = config->window_height,
        .fullscreen = config->fullscreen,
        .vsync = config->vsync
    };

    ctx->engine = agentite_init(&engine_config);
    if (!ctx->engine) {
        agentite_set_error("Failed to initialize engine");
        goto error;
    }

    /* 2. Initialize sprite renderer */
    ctx->sprites = agentite_sprite_init(
        agentite_get_gpu_device(ctx->engine),
        agentite_get_window(ctx->engine)
    );
    if (!ctx->sprites) {
        agentite_set_error("Failed to initialize sprite renderer");
        goto error;
    }

    /* 3. Initialize text renderer */
    ctx->text = agentite_text_init(
        agentite_get_gpu_device(ctx->engine),
        agentite_get_window(ctx->engine)
    );
    if (!ctx->text) {
        agentite_set_error("Failed to initialize text renderer");
        goto error;
    }

    /* 4. Initialize camera */
    ctx->camera = agentite_camera_create(
        (float)config->window_width,
        (float)config->window_height
    );
    if (!ctx->camera) {
        agentite_set_error("Failed to create camera");
        goto error;
    }

    /* Connect camera to sprite renderer */
    agentite_sprite_set_camera(ctx->sprites, ctx->camera);

    /* 5. Initialize input system */
    ctx->input = agentite_input_init();
    if (!ctx->input) {
        agentite_set_error("Failed to initialize input system");
        goto error;
    }

    /* 6. Initialize audio system (optional) */
    if (config->enable_audio) {
        ctx->audio = agentite_audio_init();
        if (!ctx->audio) {
            agentite_set_error("Failed to initialize audio system");
            goto error;
        }
    }

    /* 7. Initialize ECS world (optional) */
    if (config->enable_ecs) {
        ctx->ecs = agentite_ecs_init();
        if (!ctx->ecs) {
            agentite_set_error("Failed to initialize ECS world");
            goto error;
        }
    }

    /* 8. Initialize UI system (optional) */
    if (config->enable_ui) {
        const char *ui_font = config->ui_font_path ? config->ui_font_path : config->font_path;
        float ui_size = config->ui_font_size > 0 ? config->ui_font_size : 16.0f;

        /* UI requires a font - use a default path if none specified */
        if (ui_font) {
            ctx->ui = aui_init(
                agentite_get_gpu_device(ctx->engine),
                agentite_get_window(ctx->engine),
                config->window_width,
                config->window_height,
                ui_font,
                ui_size
            );
            if (!ctx->ui) {
                agentite_set_error("Failed to initialize UI system");
                goto error;
            }
        }
    }

    /* 9. Load fonts (optional) */
    if (config->font_path) {
        ctx->font = agentite_font_load(ctx->text, config->font_path, config->font_size);
        /* Don't fail if font doesn't load - just log warning */
        if (!ctx->font) {
            SDL_Log("Warning: Could not load font '%s'", config->font_path);
        }
    }

    if (config->sdf_font_atlas && config->sdf_font_json) {
        ctx->sdf_font = agentite_sdf_font_load(ctx->text,
                                              config->sdf_font_atlas,
                                              config->sdf_font_json);
        /* Don't fail if SDF font doesn't load - just log warning */
        if (!ctx->sdf_font) {
            SDL_Log("Warning: Could not load SDF font '%s'", config->sdf_font_atlas);
        }
    }

    return ctx;

error:
    agentite_game_context_destroy(ctx);
    return NULL;
}

void agentite_game_context_destroy(Agentite_GameContext *ctx) {
    if (!ctx) return;

    /* Cleanup in reverse initialization order */

    /* Fonts */
    if (ctx->sdf_font && ctx->text) {
        agentite_sdf_font_destroy(ctx->text, ctx->sdf_font);
    }
    if (ctx->font && ctx->text) {
        agentite_font_destroy(ctx->text, ctx->font);
    }

    /* UI system */
    if (ctx->ui) {
        aui_shutdown(ctx->ui);
    }

    /* ECS world */
    if (ctx->ecs) {
        agentite_ecs_shutdown(ctx->ecs);
    }

    /* Audio system */
    if (ctx->audio) {
        agentite_audio_shutdown(ctx->audio);
    }

    /* Input system */
    if (ctx->input) {
        agentite_input_shutdown(ctx->input);
    }

    /* Camera */
    if (ctx->camera) {
        agentite_camera_destroy(ctx->camera);
    }

    /* Text renderer */
    if (ctx->text) {
        agentite_text_shutdown(ctx->text);
    }

    /* Sprite renderer */
    if (ctx->sprites) {
        agentite_sprite_shutdown(ctx->sprites);
    }

    /* Core engine */
    if (ctx->engine) {
        agentite_shutdown(ctx->engine);
    }

    free(ctx);
}

void agentite_game_context_begin_frame(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;

    agentite_begin_frame(ctx->engine);
    agentite_input_begin_frame(ctx->input);

    /* Cache timing info */
    ctx->delta_time = agentite_get_delta_time(ctx->engine);
    ctx->frame_count = agentite_get_frame_count(ctx->engine);
}

void agentite_game_context_poll_events(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        /* Let UI process the event first (if enabled) */
        if (ctx->ui && aui_process_event(ctx->ui, &event)) {
            continue;  /* UI consumed the event */
        }

        /* Let input system process the event */
        agentite_input_process_event(ctx->input, &event);

        /* Handle quit event */
        if (event.type == SDL_EVENT_QUIT) {
            agentite_quit(ctx->engine);
        }
    }

    /* Update input state (compute just_pressed/released) */
    agentite_input_update(ctx->input);
}

void agentite_game_context_end_frame(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;
    agentite_end_frame(ctx->engine);
}

SDL_GPUCommandBuffer *agentite_game_context_begin_render(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return NULL;

    /* Update camera matrices */
    agentite_camera_update(ctx->camera);

    /* Update audio system */
    if (ctx->audio) {
        agentite_audio_update(ctx->audio);
    }

    /* Acquire command buffer */
    return agentite_acquire_command_buffer(ctx->engine);
}

bool agentite_game_context_begin_render_pass(Agentite_GameContext *ctx,
                                            float r, float g, float b, float a) {
    if (!ctx || !ctx->engine) return false;
    return agentite_begin_render_pass(ctx->engine, r, g, b, a);
}

bool agentite_game_context_begin_render_pass_no_clear(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return false;
    return agentite_begin_render_pass_no_clear(ctx->engine);
}

void agentite_game_context_end_render_pass_no_submit(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;
    agentite_end_render_pass_no_submit(ctx->engine);
}

void agentite_game_context_end_render_pass(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;
    agentite_end_render_pass(ctx->engine);
}

bool agentite_game_context_is_running(Agentite_GameContext *ctx) {
    return ctx && ctx->engine && agentite_is_running(ctx->engine);
}

void agentite_game_context_quit(Agentite_GameContext *ctx) {
    if (ctx && ctx->engine) {
        agentite_quit(ctx->engine);
    }
}
