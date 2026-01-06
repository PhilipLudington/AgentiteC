/**
 * Agentite Engine - Hot Reload Example
 *
 * Demonstrates the hot reload system for automatic asset reloading.
 * Modify the texture files in the assets/ directory while the program
 * is running to see them update in real-time.
 *
 * Features demonstrated:
 *   - File watcher setup
 *   - Hot reload manager configuration
 *   - Automatic texture reloading
 *   - Custom reload handler registration
 *   - Reload event subscription
 *
 * Controls:
 *   ESC - Quit
 *   R   - Manually trigger reload of all textures
 *   D   - Toggle debug info display
 */

#include "agentite/game_context.h"
#include "agentite/watch.h"
#include "agentite/hotreload.h"
#include "agentite/asset.h"
#include "agentite/error.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Window settings */
static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* Global state */
static bool s_show_debug = true;
static size_t s_reload_count = 0;
static char s_last_reload_path[256] = {0};
static float s_reload_flash = 0.5f;  /* Flash timer for visual feedback - start visible */

/* Texture state for custom reload handler */
static Agentite_SpriteRenderer *s_sprites = NULL;
static Agentite_Texture *s_reloadable_tex = NULL;
static const char *s_texture_path = "examples/hotreload/assets/test.tga";

/* Forward declarations */
static void on_reload_event(const Agentite_ReloadResult *result, void *userdata);
static bool on_png_reload(const char *path, Agentite_ReloadType type, void *userdata);
static Agentite_Texture *create_checkerboard_texture(Agentite_SpriteRenderer *sr, int size, uint32_t color1, uint32_t color2);
static void save_test_image(int style);

/* Test image colors */
static const uint32_t TEST_COLORS[][2] = {
    { 0xFF4040FF, 0xFF404040 },  /* 1: Red/Gray */
    { 0xFF40FF40, 0xFF404040 },  /* 2: Green/Gray */
    { 0xFFFF4040, 0xFF404040 },  /* 3: Blue/Gray */
    { 0xFF40FFFF, 0xFFFF40FF },  /* 4: Cyan/Magenta */
    { 0xFFFFFF40, 0xFF4040FF },  /* 5: Yellow/Red */
};

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure game context with hot reload enabled */
    Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
    config.window_title = "Hot Reload Example";
    config.window_width = WINDOW_WIDTH;
    config.window_height = WINDOW_HEIGHT;
    config.enable_hot_reload = true;
    config.font_path = "assets/fonts/Roboto-Regular.ttf";
    config.font_size = 32.0f;  /* Larger for visibility */
    config.ui_font_path = "assets/fonts/Roboto-Regular.ttf";
    config.ui_font_size = 16.0f;

    /* Watch the assets directory */
    const char *watch_paths[] = { "examples/hotreload/assets" };
    config.watch_paths = watch_paths;
    config.watch_path_count = 1;

    /* Create context (initializes hot reload automatically) */
    Agentite_GameContext *ctx = agentite_game_context_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to create game context: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Store sprite renderer globally for reload handler */
    s_sprites = ctx->sprites;

    /* Set up reload callback and custom handler */
    if (ctx->hotreload) {
        agentite_hotreload_set_callback(ctx->hotreload, on_reload_event, NULL);
        /* Register custom handler for image files to directly reload our texture */
        agentite_hotreload_register_handler(ctx->hotreload, ".png", on_png_reload, NULL);
        agentite_hotreload_register_handler(ctx->hotreload, ".tga", on_png_reload, NULL);
        SDL_Log("Hot reload enabled - watching: examples/hotreload/assets/");
    } else {
        SDL_Log("Hot reload not available");
    }

    /* Create a sample checkerboard texture as fallback */
    Agentite_Texture *sample_tex = create_checkerboard_texture(ctx->sprites, 128, 0xFF4040FF, 0xFF404040);
    if (!sample_tex) {
        SDL_Log("Warning: Could not create sample texture");
    }

    /* Create initial test image if it doesn't exist */
    save_test_image(0);

    /* Load the texture from the assets directory */
    s_reloadable_tex = agentite_texture_load(ctx->sprites, s_texture_path);
    if (!s_reloadable_tex) {
        SDL_Log("Warning: Could not load %s, using fallback", s_texture_path);
        s_reloadable_tex = sample_tex;
    } else {
        SDL_Log("Loaded texture: %s", s_texture_path);
    }

    /* Main loop */
    while (agentite_game_context_is_running(ctx)) {
        /* Begin frame (this also updates hot reload) */
        agentite_game_context_begin_frame(ctx);
        agentite_game_context_poll_events(ctx);

        /* Handle input */
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_ESCAPE)) {
            agentite_game_context_quit(ctx);
        }

        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_R)) {
            if (ctx->hotreload) {
                SDL_Log("Manual reload triggered");
                agentite_hotreload_reload_all(ctx->hotreload, AGENTITE_RELOAD_TEXTURE);
                s_reload_flash = 0.5f;  /* Flash for 0.5 seconds */
                s_reload_count++;
            }
        }

        /* Update flash timer */
        if (s_reload_flash > 0.0f) {
            s_reload_flash -= ctx->delta_time;
            if (s_reload_flash < 0.0f) s_reload_flash = 0.0f;
        }

        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_D)) {
            s_show_debug = !s_show_debug;
        }

        /* Number keys 1-5 generate different test images */
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_1)) {
            save_test_image(0);
        }
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_2)) {
            save_test_image(1);
        }
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_3)) {
            save_test_image(2);
        }
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_4)) {
            save_test_image(3);
        }
        if (agentite_input_key_pressed(ctx->input, SDL_SCANCODE_5)) {
            save_test_image(4);
        }

        /* Begin rendering */
        SDL_GPUCommandBuffer *cmd = agentite_game_context_begin_render(ctx);
        if (!cmd) continue;

        /* Upload sprite batch data */
        /* Use NULL camera for screen-space rendering (0,0 = top-left) */
        agentite_sprite_set_camera(ctx->sprites, NULL);
        agentite_sprite_begin(ctx->sprites, NULL);

        /* Draw the reloadable texture centered */
        if (s_reloadable_tex) {
            Agentite_Sprite sprite = agentite_sprite_from_texture(s_reloadable_tex);
            /* Origin is at center (0.5, 0.5), so just use screen center */
            float x = WINDOW_WIDTH / 2.0f;
            float y = WINDOW_HEIGHT / 2.0f;
            agentite_sprite_draw(ctx->sprites, &sprite, x, y);
        }

        agentite_sprite_upload(ctx->sprites, cmd);

        /* Upload text */
        agentite_text_begin(ctx->text);

        /* Draw title */
        if (ctx->font) {
            agentite_text_draw_colored(ctx->text, ctx->font, "Hot Reload Example", 20, 50, 1.0f, 1.0f, 0.0f, 1.0f);  /* Yellow */
            agentite_text_draw_colored(ctx->text, ctx->font, "Press 1-5 to generate different textures", 20, 100, 0.0f, 1.0f, 1.0f, 1.0f);  /* Cyan */

            /* Draw reload status with flash effect */
            if (s_reload_flash > 0.0f) {
                /* Flash green when reload triggered */
                agentite_text_draw_colored(ctx->text, ctx->font, "RELOAD TRIGGERED!", 20, 150, 0.0f, 1.0f, 0.0f, 1.0f);
            }

            /* Draw debug info */
            if (s_show_debug) {
                char buf[256];
                /* Color changes based on flash */
                float r = s_reload_flash > 0.0f ? 0.0f : 0.7f;
                float g = 1.0f;
                float b = s_reload_flash > 0.0f ? 0.0f : 0.7f;
                snprintf(buf, sizeof(buf), "Reload count: %zu", s_reload_count);
                agentite_text_draw_colored(ctx->text, ctx->font, buf, 20, 200, r, g, b, 1.0f);

                if (s_last_reload_path[0]) {
                    snprintf(buf, sizeof(buf), "Last reload: %s", s_last_reload_path);
                    agentite_text_draw_colored(ctx->text, ctx->font, buf, 20, 250, 0.7f, 0.7f, 1.0f, 1.0f);
                }
            }

            /* Draw controls at bottom */
            agentite_text_draw_colored(ctx->text, ctx->font, "1-5: Change texture | D: Debug | ESC: Quit",
                               20, WINDOW_HEIGHT - 50, 0.6f, 0.6f, 0.6f, 1.0f);
        }

        agentite_text_end(ctx->text);
        agentite_text_upload(ctx->text, cmd);

        /* Render pass */
        if (agentite_game_context_begin_render_pass(ctx, 0.2f, 0.2f, 0.3f, 1.0f)) {
            SDL_GPURenderPass *pass = agentite_get_render_pass(ctx->engine);
            agentite_sprite_render(ctx->sprites, cmd, pass);
            agentite_text_render(ctx->text, cmd, pass);
            agentite_game_context_end_render_pass(ctx);
        }

        agentite_game_context_end_frame(ctx);
    }

    /* Cleanup */
    if (sample_tex && sample_tex != s_reloadable_tex) {
        agentite_texture_destroy(ctx->sprites, sample_tex);
    }
    if (s_reloadable_tex && s_reloadable_tex != sample_tex) {
        agentite_texture_destroy(ctx->sprites, s_reloadable_tex);
    }

    agentite_game_context_destroy(ctx);
    return 0;
}

/**
 * Reload callback - called when an asset is reloaded.
 */
static void on_reload_event(const Agentite_ReloadResult *result, void *userdata) {
    (void)userdata;

    if (result->success) {
        SDL_Log("Reloaded: %s (%s)", result->path,
                agentite_hotreload_type_name(result->type));
        strncpy(s_last_reload_path, result->path, sizeof(s_last_reload_path) - 1);
        s_reload_flash = 0.5f;
    } else {
        SDL_Log("Reload failed: %s - %s", result->path, result->error ? result->error : "unknown");
    }
}

/**
 * Custom reload handler - directly reloads our texture.
 */
static bool on_png_reload(const char *path, Agentite_ReloadType type, void *userdata) {
    (void)type;
    (void)userdata;

    /* Check if this is our texture file (test.tga or test.png) */
    if ((strstr(path, "test.tga") || strstr(path, "test.png")) && s_reloadable_tex && s_sprites) {
        /* Use full path, not just filename from watcher */
        SDL_Log("Hot reload: Reloading texture from %s", s_texture_path);
        bool success = agentite_texture_reload(s_sprites, s_reloadable_tex, s_texture_path);
        if (success) {
            SDL_Log("Hot reload: Texture reloaded successfully!");
            s_reload_count++;
            s_reload_flash = 0.5f;
        } else {
            SDL_Log("Hot reload: Failed to reload texture: %s", agentite_get_last_error());
        }
        return success;
    }

    return false;  /* Not our file, let default handler try */
}

/**
 * Create a checkerboard texture for testing.
 */
static Agentite_Texture *create_checkerboard_texture(Agentite_SpriteRenderer *sr,
                                                      int size,
                                                      uint32_t color1,
                                                      uint32_t color2) {
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    int cell_size = size / 8;
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int cx = x / cell_size;
            int cy = y / cell_size;
            uint32_t color = ((cx + cy) % 2 == 0) ? color1 : color2;

            int idx = (y * size + x) * 4;
            pixels[idx + 0] = (color >> 0) & 0xFF;   /* R */
            pixels[idx + 1] = (color >> 8) & 0xFF;   /* G */
            pixels[idx + 2] = (color >> 16) & 0xFF;  /* B */
            pixels[idx + 3] = (color >> 24) & 0xFF;  /* A */
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/**
 * Save a test TGA image to disk - triggers hot reload.
 * TGA format is simple: 18-byte header + raw BGRA pixels.
 */
static void save_test_image(int style) {
    const int SIZE = 128;

    if (style < 0 || style >= 5) style = 0;
    uint32_t color1 = TEST_COLORS[style][0];
    uint32_t color2 = TEST_COLORS[style][1];

    /* Generate checkerboard pixels (BGRA for TGA) */
    unsigned char *pixels = (unsigned char *)malloc(SIZE * SIZE * 4);
    if (!pixels) return;

    int cell_size = SIZE / 8;
    for (int y = 0; y < SIZE; y++) {
        for (int x = 0; x < SIZE; x++) {
            int cx = x / cell_size;
            int cy = y / cell_size;
            uint32_t color = ((cx + cy) % 2 == 0) ? color1 : color2;

            int idx = (y * SIZE + x) * 4;
            /* TGA uses BGRA order */
            pixels[idx + 0] = (color >> 16) & 0xFF;  /* B */
            pixels[idx + 1] = (color >> 8) & 0xFF;   /* G */
            pixels[idx + 2] = (color >> 0) & 0xFF;   /* R */
            pixels[idx + 3] = (color >> 24) & 0xFF;  /* A */
        }
    }

    /* Write TGA file */
    FILE *f = fopen(s_texture_path, "wb");
    if (f) {
        /* TGA header (18 bytes) */
        unsigned char header[18] = {0};
        header[2] = 2;              /* Uncompressed true-color */
        header[12] = SIZE & 0xFF;   /* Width low byte */
        header[13] = (SIZE >> 8);   /* Width high byte */
        header[14] = SIZE & 0xFF;   /* Height low byte */
        header[15] = (SIZE >> 8);   /* Height high byte */
        header[16] = 32;            /* Bits per pixel */
        header[17] = 0x28;          /* Image descriptor (top-left origin, 8 alpha bits) */

        fwrite(header, 1, 18, f);
        fwrite(pixels, 1, SIZE * SIZE * 4, f);
        fclose(f);

        SDL_Log("Saved test image: style %d to %s", style + 1, s_texture_path);
    } else {
        SDL_Log("Failed to save test image: %s", s_texture_path);
    }

    free(pixels);
}
