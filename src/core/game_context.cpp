#include "agentite/agentite.h"
#include "agentite/game_context.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Helper Functions
 * ============================================================================ */

/* Initialize hot reload subsystem */
static bool init_hot_reload(Agentite_GameContext *ctx,
                            const Agentite_GameContextConfig *config)
{
    /* Create file watcher */
    Agentite_FileWatcherConfig watcher_config = AGENTITE_FILE_WATCHER_CONFIG_DEFAULT;
    ctx->watcher = agentite_watch_create(&watcher_config);
    if (!ctx->watcher) {
        agentite_set_error("Failed to initialize file watcher");
        return false;
    }

    /* Add watch paths */
    for (size_t i = 0; i < config->watch_path_count; i++) {
        agentite_watch_add_path(ctx->watcher, config->watch_paths[i]);
    }

    /* Create hot reload manager */
    Agentite_HotReloadConfig hr_config = AGENTITE_HOT_RELOAD_CONFIG_DEFAULT;
    hr_config.watcher = ctx->watcher;
    hr_config.sprites = ctx->sprites;
    hr_config.audio = ctx->audio;
    ctx->hotreload = agentite_hotreload_create(&hr_config);
    if (!ctx->hotreload) {
        agentite_set_error("Failed to initialize hot reload manager");
        return false;
    }

    return true;
}

/* Initialize mod system */
static bool init_mod_system(Agentite_GameContext *ctx,
                            const Agentite_GameContextConfig *config)
{
    Agentite_ModManagerConfig mod_config = AGENTITE_MOD_MANAGER_CONFIG_DEFAULT;
    mod_config.hotreload = ctx->hotreload;
    mod_config.allow_overrides = config->allow_mod_overrides;
    ctx->mods = agentite_mod_manager_create(&mod_config);
    if (!ctx->mods) {
        agentite_set_error("Failed to initialize mod manager");
        return false;
    }

    /* Add mod search paths */
    for (size_t i = 0; i < config->mod_path_count; i++) {
        agentite_mod_add_search_path(ctx->mods, config->mod_paths[i]);
    }

    /* Scan for mods */
    agentite_mod_scan(ctx->mods);
    return true;
}

/* ============================================================================
 * Public API
 * ============================================================================ */

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

        /* Get logical dimensions and DPI scale */
        int logical_w, logical_h;
        agentite_get_window_size(ctx->engine, &logical_w, &logical_h);
        float dpi_scale = agentite_get_dpi_scale(ctx->engine);

        /* UI uses logical coordinates (matches camera and sprite renderer) */
        if (ui_font) {
            ctx->ui = aui_init(
                agentite_get_gpu_device(ctx->engine),
                agentite_get_window(ctx->engine),
                logical_w,
                logical_h,
                ui_font,
                ui_size
            );
            if (!ctx->ui) {
                agentite_set_error("Failed to initialize UI system");
                goto error;
            }

            /* Set DPI scale for high-DPI awareness (used for input scaling) */
            aui_set_dpi_scale(ctx->ui, dpi_scale);
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

    /* 10. Initialize hot reload system (optional) */
    if (config->enable_hot_reload) {
        if (!init_hot_reload(ctx, config)) {
            goto error;
        }
    }

    /* 11. Initialize mod system (optional) */
    if (config->enable_mods) {
        if (!init_mod_system(ctx, config)) {
            goto error;
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

    /* Mod system */
    if (ctx->mods) {
        agentite_mod_manager_destroy(ctx->mods);
    }

    /* Hot reload system */
    if (ctx->hotreload) {
        agentite_hotreload_destroy(ctx->hotreload);
    }
    if (ctx->watcher) {
        agentite_watch_destroy(ctx->watcher);
    }

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

    /* Update hot reload system */
    if (ctx->hotreload) {
        agentite_hotreload_update(ctx->hotreload);
    }

    /* Begin UI frame (resets draw state for new frame) */
    if (ctx->ui) {
        aui_begin_frame(ctx->ui, agentite_get_delta_time(ctx->engine));
    }

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

        /* Handle window events */
        switch (event.type) {
            case SDL_EVENT_QUIT:
                agentite_quit(ctx->engine);
                break;

            case SDL_EVENT_WINDOW_RESIZED: {
                /* Logical window size changed - update all renderers */
                int logical_w = event.window.data1;
                int logical_h = event.window.data2;

                /* All renderers use logical coordinates for consistency */
                if (ctx->sprites) {
                    agentite_sprite_set_screen_size(ctx->sprites, logical_w, logical_h);
                }

                if (ctx->text) {
                    agentite_text_set_screen_size(ctx->text, logical_w, logical_h);
                }

                if (ctx->ui) {
                    aui_set_screen_size(ctx->ui, logical_w, logical_h);
                }

                if (ctx->camera) {
                    agentite_camera_set_viewport(ctx->camera, (float)logical_w, (float)logical_h);
                }

                /* Update cached dimensions */
                ctx->window_width = logical_w;
                ctx->window_height = logical_h;
                break;
            }
        }
    }

    /* Update input state (compute just_pressed/released) */
    agentite_input_update(ctx->input);
}

void agentite_game_context_end_frame(Agentite_GameContext *ctx) {
    if (!ctx || !ctx->engine) return;

    /* End UI frame (clears per-frame input state) */
    if (ctx->ui) {
        aui_end_frame(ctx->ui);
    }

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
