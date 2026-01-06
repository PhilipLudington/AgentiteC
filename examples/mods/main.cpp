/**
 * Agentite Engine - Mod System Example
 *
 * Demonstrates the mod loading system with discovery, dependency resolution,
 * and virtual filesystem for asset overrides.
 *
 * Features demonstrated:
 *   - Mod discovery from directories
 *   - Mod manifest parsing (mod.toml)
 *   - Dependency and conflict checking
 *   - Load order resolution
 *   - Virtual filesystem path resolution
 *   - Asset overrides
 *
 * Directory structure:
 *   mods/
 *     sample_mod/
 *       mod.toml
 *       textures/
 *         player.png
 *
 * Controls:
 *   ESC   - Quit
 *   S     - Scan for mods
 *   L     - Load all enabled mods
 *   U     - Unload all mods
 *   1-9   - Toggle enable/disable for mod at index
 */

#include "agentite/game_context.h"
#include "agentite/mod.h"
#include "agentite/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Window settings */
static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* Input cooldown to prevent key repeat jitter */
static const float KEY_COOLDOWN = 0.2f;      /* 200ms for toggle keys */
static const float ACTION_COOLDOWN = 0.5f;   /* 500ms for load/unload (heavier operations) */
static float s_key_cooldowns[9] = {0};       /* Cooldown timer for keys 1-9 */
static float s_load_cooldown = 0;            /* Cooldown for L key */
static float s_unload_cooldown = 0;          /* Cooldown for U key */
static float s_scan_cooldown = 0;            /* Cooldown for S key */

/* Forward declarations */
static void draw_mod_list(Agentite_GameContext *ctx, Agentite_ModManager *mods);
static void on_mod_state_changed(const char *mod_id, Agentite_ModState state, void *userdata);

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure game context */
    Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
    config.window_title = "Mod System Example";
    config.window_width = WINDOW_WIDTH;
    config.window_height = WINDOW_HEIGHT;
    config.font_path = "assets/fonts/Roboto-Regular.ttf";
    config.font_size = 24.0f;
    config.ui_font_path = "assets/fonts/Roboto-Regular.ttf";
    config.ui_font_size = 16.0f;

    /* Enable mod system */
    config.enable_mods = true;
    const char *mod_paths[] = { "examples/mods/mods" };
    config.mod_paths = mod_paths;
    config.mod_path_count = 1;
    config.allow_mod_overrides = true;

    /* Create context */
    Agentite_GameContext *ctx = agentite_game_context_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to create game context: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Get mod manager */
    Agentite_ModManager *mods = ctx->mods;
    if (!mods) {
        /* Create standalone mod manager if not created by context */
        Agentite_ModManagerConfig mod_config = AGENTITE_MOD_MANAGER_CONFIG_DEFAULT;
        mod_config.allow_overrides = true;
        mods = agentite_mod_manager_create(&mod_config);
        if (mods) {
            agentite_mod_add_search_path(mods, "examples/mods/mods");
        }
    }

    if (mods) {
        /* Set up mod state callback */
        agentite_mod_set_callback(mods, on_mod_state_changed, NULL);

        /* Initial scan for mods */
        size_t found = agentite_mod_scan(mods);
        SDL_Log("Found %zu mods", found);
    } else {
        SDL_Log("Mod system not available");
    }

    /* Main loop */
    while (agentite_game_context_is_running(ctx)) {
        agentite_game_context_begin_frame(ctx);
        agentite_game_context_poll_events(ctx);

        /* Handle input */
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_ESCAPE)) {
            agentite_game_context_quit(ctx);
        }

        if (mods) {
            /* Update cooldowns */
            if (s_scan_cooldown > 0) s_scan_cooldown -= ctx->delta_time;
            if (s_load_cooldown > 0) s_load_cooldown -= ctx->delta_time;
            if (s_unload_cooldown > 0) s_unload_cooldown -= ctx->delta_time;

            /* Scan for mods */
            if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_S) && s_scan_cooldown <= 0) {
                size_t found = agentite_mod_scan(mods);
                s_scan_cooldown = KEY_COOLDOWN;
                SDL_Log("Scan complete: found %zu mods", found);
            }

            /* Load all enabled mods */
            if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_L) && s_load_cooldown <= 0) {
                /* Build list of enabled mod IDs */
                size_t count = agentite_mod_count(mods);
                const char **enabled = (const char **)malloc(count * sizeof(const char *));
                size_t enabled_count = 0;

                for (size_t i = 0; i < count; i++) {
                    const Agentite_ModInfo *info = agentite_mod_get_info(mods, i);
                    if (info && agentite_mod_is_enabled(mods, info->id)) {
                        enabled[enabled_count++] = info->id;
                    }
                }

                if (enabled_count > 0) {
                    if (agentite_mod_load_all(mods, enabled, enabled_count)) {
                        SDL_Log("Loaded %zu mods", enabled_count);
                    } else {
                        SDL_Log("Failed to load some mods: %s", agentite_get_last_error());
                    }
                } else {
                    SDL_Log("No mods enabled to load");
                }

                free(enabled);
                s_load_cooldown = KEY_COOLDOWN;
            }

            /* Unload all mods */
            if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_U) && s_unload_cooldown <= 0) {
                agentite_mod_unload_all(mods);
                s_unload_cooldown = KEY_COOLDOWN;
                SDL_Log("All mods unloaded");
            }

            /* Toggle mods 1-9 (with cooldown to prevent key repeat jitter) */
            for (int i = 0; i < 9; i++) {
                /* Update cooldown */
                if (s_key_cooldowns[i] > 0) {
                    s_key_cooldowns[i] -= ctx->delta_time;
                }

                SDL_Scancode key = (SDL_Scancode)(SDL_SCANCODE_1 + i);
                if (agentite_input_key_pressed(ctx->input, key) && s_key_cooldowns[i] <= 0) {
                    const Agentite_ModInfo *info = agentite_mod_get_info(mods, i);
                    if (info) {
                        bool currently_enabled = agentite_mod_is_enabled(mods, info->id);
                        agentite_mod_set_enabled(mods, info->id, !currently_enabled);
                        s_key_cooldowns[i] = KEY_COOLDOWN;  /* Reset cooldown */
                        SDL_Log("Mod '%s' %s", info->name,
                                !currently_enabled ? "enabled" : "disabled");
                    }
                }
            }

            /* Test path resolution */
            if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_P)) {
                const char *test_path = "textures/player.png";
                const char *resolved = agentite_mod_resolve_path(mods, test_path);
                SDL_Log("Path resolution: '%s' -> '%s'", test_path, resolved);

                const char *source = agentite_mod_get_override_source(mods, test_path);
                if (source) {
                    SDL_Log("Override provided by mod: %s", source);
                } else {
                    SDL_Log("No override - using base game asset");
                }
            }
        }

        /* Begin rendering */
        SDL_GPUCommandBuffer *cmd = agentite_game_context_begin_render(ctx);
        if (!cmd) {
            agentite_game_context_end_frame(ctx);
            continue;
        }

        /* Upload text */
        agentite_text_begin(ctx->text);

        /* Draw title */
        if (ctx->font) {
            agentite_text_draw_colored(ctx->text, ctx->font, "Mod System Example", 20, 30, 1.0f, 1.0f, 0.0f, 1.0f);

            /* Draw controls - expanded for clarity */
            float cy = 70;
            agentite_text_draw_colored(ctx->text, ctx->font, "Workflow: S to scan -> 1-9 to select -> L to load", 20, cy, 0.0f, 1.0f, 0.5f, 1.0f);
            cy += 35;
            agentite_text_draw_colored(ctx->text, ctx->font, "Controls:", 20, cy, 0.0f, 1.0f, 1.0f, 1.0f);
            cy += 28;
            agentite_text_draw_colored(ctx->text, ctx->font, "  S = Scan for mods", 20, cy, 0.8f, 0.8f, 0.8f, 1.0f);
            cy += 24;
            agentite_text_draw_colored(ctx->text, ctx->font, "  1 = Select/deselect mod #1, 2 = mod #2, etc.", 20, cy, 1.0f, 1.0f, 0.5f, 1.0f);
            cy += 24;
            agentite_text_draw_colored(ctx->text, ctx->font, "  L = Load selected mods    U = Unload all", 20, cy, 0.8f, 0.8f, 0.8f, 1.0f);
            cy += 24;
            agentite_text_draw_colored(ctx->text, ctx->font, "  P = Test asset override   ESC = Quit", 20, cy, 0.8f, 0.8f, 0.8f, 1.0f);
        }

        /* Draw mod list */
        draw_mod_list(ctx, mods);

        agentite_text_end(ctx->text);
        agentite_text_upload(ctx->text, cmd);

        /* Render pass */
        if (agentite_game_context_begin_render_pass(ctx, 0.15f, 0.15f, 0.2f, 1.0f)) {
            agentite_text_render(ctx->text, cmd, agentite_get_render_pass(ctx->engine));
            agentite_game_context_end_render_pass(ctx);
        }

        agentite_game_context_end_frame(ctx);
    }

    /* Cleanup */
    if (mods && mods != ctx->mods) {
        agentite_mod_manager_destroy(mods);
    }

    agentite_game_context_destroy(ctx);
    return 0;
}

/**
 * Draw the list of discovered mods.
 */
static void draw_mod_list(Agentite_GameContext *ctx, Agentite_ModManager *mods) {
    if (!ctx->font) return;

    /* Start below the controls section */
    const float START_Y = 230;

    if (!mods) {
        agentite_text_draw_colored(ctx->text, ctx->font, "Mod system not available", 20, START_Y, 1.0f, 0.5f, 0.5f, 1.0f);
        return;
    }

    size_t count = agentite_mod_count(mods);
    if (count == 0) {
        agentite_text_draw_colored(ctx->text, ctx->font, "No mods discovered yet.", 20, START_Y, 0.8f, 0.8f, 0.8f, 1.0f);
        agentite_text_draw_colored(ctx->text, ctx->font, "Press S to scan the mods/ directory.", 20, START_Y + 28, 0.6f, 0.6f, 0.6f, 1.0f);
        return;
    }

    char buf[512];
    snprintf(buf, sizeof(buf), "Discovered Mods (%zu):", count);
    agentite_text_draw_colored(ctx->text, ctx->font, buf, 20, START_Y, 0.0f, 1.0f, 1.0f, 1.0f);

    /* Legend */
    agentite_text_draw_colored(ctx->text, ctx->font, "[X] = enabled, [ ] = disabled", 300, START_Y, 0.5f, 0.5f, 0.5f, 1.0f);

    float y = START_Y + 35;
    for (size_t i = 0; i < count && i < 9; i++) {
        const Agentite_ModInfo *info = agentite_mod_get_info(mods, i);
        if (!info) continue;

        bool enabled = agentite_mod_is_enabled(mods, info->id);
        const char *state_str = agentite_mod_state_name(info->state);

        /* Compact format: "1. [X] ModName v1.0.0 - STATE" */
        snprintf(buf, sizeof(buf), "%zu. [%s] %s v%s (%s)",
                 i + 1,
                 enabled ? "X" : " ",
                 info->name,
                 info->version,
                 state_str);

        /* Green if enabled, gray if disabled */
        float r = enabled ? 0.3f : 0.6f;
        float g = enabled ? 1.0f : 0.6f;
        float b = enabled ? 0.3f : 0.6f;
        agentite_text_draw_colored(ctx->text, ctx->font, buf, 40, y, r, g, b, 1.0f);
        y += 32;
    }

    /* Show status at bottom */
    size_t loaded = agentite_mod_loaded_count(mods);
    snprintf(buf, sizeof(buf), "Status: %zu mod(s) loaded, %zu discovered total", loaded, count);
    agentite_text_draw_colored(ctx->text, ctx->font, buf, 20, WINDOW_HEIGHT - 60, 0.0f, 1.0f, 0.0f, 1.0f);

    /* Workflow hint */
    if (loaded == 0) {
        agentite_text_draw_colored(ctx->text, ctx->font, "Tip: Press 1 to enable mod, then L to load it",
                                   20, WINDOW_HEIGHT - 30, 0.6f, 0.6f, 0.3f, 1.0f);
    }
}

/**
 * Callback when mod state changes.
 */
static void on_mod_state_changed(const char *mod_id, Agentite_ModState state, void *userdata) {
    (void)userdata;
    SDL_Log("Mod '%s' state changed to: %s", mod_id, agentite_mod_state_name(state));
}
