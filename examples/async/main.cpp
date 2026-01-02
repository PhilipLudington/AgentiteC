/**
 * Agentite Engine - Async Loading Example
 *
 * Demonstrates background asset loading with:
 * - Thread pool for parallel I/O
 * - Progress tracking
 * - Completion callbacks
 * - Streaming regions for world chunks
 *
 * The example creates procedural textures and loads them asynchronously
 * to simulate loading multiple assets without blocking the main thread.
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/asset.h"
#include "agentite/async.h"
#include "agentite/text.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
#include <direct.h>
#define MKDIR(path) _mkdir(path)
#define RMDIR(path) _rmdir(path)
#else
#include <sys/stat.h>
#include <unistd.h>
#define MKDIR(path) mkdir(path, 0755)
#define RMDIR(path) rmdir(path)
#endif

/* Maximum textures to load */
#define MAX_TEXTURES 16

/* Loaded texture info */
typedef struct LoadedTexture {
    Agentite_AssetHandle handle;
    Agentite_Texture *texture;
    bool loaded;
    float x, y;
    float target_x, target_y;
    float scale;
    float rotation;
} LoadedTexture;

/* Application state */
typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Camera *camera;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;
    Agentite_AssetRegistry *registry;
    Agentite_AsyncLoader *loader;

    LoadedTexture textures[MAX_TEXTURES];
    int texture_count;
    int textures_loaded;
    bool all_loaded;
    float load_progress;

    float time;
} AppState;

/* Create a procedural gradient texture and save to disk for async loading demo */
static void create_test_texture_file(const char *path, int size, int hue_offset) {
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;

            /* Create a gradient with varying hue */
            float fx = (float)x / size;
            float fy = (float)y / size;

            /* Simple HSV to RGB-ish conversion */
            float hue = fmodf((fx + fy) * 360.0f + hue_offset, 360.0f);
            float r, g, b;

            if (hue < 60) {
                r = 1.0f; g = hue / 60.0f; b = 0.0f;
            } else if (hue < 120) {
                r = 1.0f - (hue - 60) / 60.0f; g = 1.0f; b = 0.0f;
            } else if (hue < 180) {
                r = 0.0f; g = 1.0f; b = (hue - 120) / 60.0f;
            } else if (hue < 240) {
                r = 0.0f; g = 1.0f - (hue - 180) / 60.0f; b = 1.0f;
            } else if (hue < 300) {
                r = (hue - 240) / 60.0f; g = 0.0f; b = 1.0f;
            } else {
                r = 1.0f; g = 0.0f; b = 1.0f - (hue - 300) / 60.0f;
            }

            /* Add some pattern */
            float pattern = sinf(x * 0.2f) * sinf(y * 0.2f) * 0.3f + 0.7f;
            r *= pattern;
            g *= pattern;
            b *= pattern;

            pixels[idx + 0] = (unsigned char)(r * 255);
            pixels[idx + 1] = (unsigned char)(g * 255);
            pixels[idx + 2] = (unsigned char)(b * 255);
            pixels[idx + 3] = 255;
        }
    }

    /* Write as simple TGA file (easy to implement, works with stb_image) */
    FILE *f = fopen(path, "wb");
    if (f) {
        /* TGA header */
        unsigned char header[18] = {0};
        header[2] = 2;  /* Uncompressed RGB */
        header[12] = size & 0xFF;
        header[13] = (size >> 8) & 0xFF;
        header[14] = size & 0xFF;
        header[15] = (size >> 8) & 0xFF;
        header[16] = 32;  /* 32-bit (RGBA) */
        header[17] = 0x28;  /* Origin top-left, 8 alpha bits */
        fwrite(header, 1, 18, f);

        /* Write pixels as BGRA */
        for (int i = 0; i < size * size; i++) {
            unsigned char bgra[4];
            bgra[0] = pixels[i * 4 + 2];  /* B */
            bgra[1] = pixels[i * 4 + 1];  /* G */
            bgra[2] = pixels[i * 4 + 0];  /* R */
            bgra[3] = pixels[i * 4 + 3];  /* A */
            fwrite(bgra, 1, 4, f);
        }
        fclose(f);
    }

    free(pixels);
}

/* Callback when texture finishes loading */
static void on_texture_loaded(Agentite_AssetHandle handle,
                               Agentite_LoadResult result,
                               void *userdata) {
    AppState *app = (AppState *)userdata;

    if (result.success) {
        /* Find the slot for this handle and mark as loaded */
        for (int i = 0; i < app->texture_count; i++) {
            if (!app->textures[i].loaded) {
                app->textures[i].handle = handle;
                app->textures[i].texture = agentite_texture_from_handle(
                    app->registry, handle);
                app->textures[i].loaded = true;
                app->textures_loaded++;

                /* Animate from center to target position */
                app->textures[i].x = 640.0f;
                app->textures[i].y = 360.0f;
                app->textures[i].scale = 0.1f;

                printf("Loaded texture %d/%d\n", app->textures_loaded, app->texture_count);
                break;
            }
        }
    } else {
        printf("Failed to load texture: %s\n", result.error ? result.error : "Unknown error");
    }

    /* Update progress */
    app->load_progress = (float)app->textures_loaded / app->texture_count;
    if (app->textures_loaded >= app->texture_count) {
        app->all_loaded = true;
        printf("All textures loaded!\n");
    }
}

/* (Progress is shown inline with text rendering) */

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Set simulated delay so loading progress is visible */
    if (!SDL_getenv("AGENTITE_ASYNC_DELAY_MS")) {
        #ifdef _WIN32
        _putenv_s("AGENTITE_ASYNC_DELAY_MS", "800");
        #else
        setenv("AGENTITE_ASYNC_DELAY_MS", "800", 0);
        #endif
    }

    AppState app = {0};

    /* Configure engine */
    Agentite_Config config = {
        .window_title = "Agentite - Async Loading Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize subsystems */
    app.sprites = agentite_sprite_init(
        agentite_get_gpu_device(app.engine),
        agentite_get_window(app.engine)
    );

    app.camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_sprite_set_camera(app.sprites, app.camera);
    agentite_camera_set_position(app.camera, 640.0f, 360.0f);

    app.input = agentite_input_init();

    /* Initialize text rendering */
    app.text = agentite_text_init(
        agentite_get_gpu_device(app.engine),
        agentite_get_window(app.engine)
    );
    app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 24.0f);
    if (!app.font) {
        fprintf(stderr, "Warning: Could not load font, text will not display\n");
    }

    /* Initialize asset system */
    app.registry = agentite_asset_registry_create();
    if (!app.registry) {
        fprintf(stderr, "Failed to create asset registry\n");
        return 1;
    }

    /* Initialize async loader with 2 worker threads */
    Agentite_AsyncLoaderConfig loader_config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
    loader_config.num_threads = 2;
    app.loader = agentite_async_loader_create(&loader_config);
    if (!app.loader) {
        fprintf(stderr, "Failed to create async loader\n");
        return 1;
    }

    printf("Async loader created with %d worker threads\n", loader_config.num_threads);

    /* Create test textures on disk */
    printf("Creating test texture files...\n");
    app.texture_count = 8;  /* Load 8 textures */

    char temp_dir[] = "/tmp/agentite_async_test";
    char path[256];

    /* Create temp directory */
    MKDIR(temp_dir);

    for (int i = 0; i < app.texture_count; i++) {
        snprintf(path, sizeof(path), "%s/texture_%d.tga", temp_dir, i);
        create_test_texture_file(path, 128, i * 45);  /* 128x128, varying hue */

        /* Set target positions in a grid */
        int col = i % 4;
        int row = i / 4;
        app.textures[i].target_x = 300.0f + col * 200.0f;
        app.textures[i].target_y = 250.0f + row * 250.0f;
    }

    /* Track if we've started loading yet */
    bool loading_started = false;
    int frames_before_load = 3;  /* Render a few frames before starting loads */

    /* Main loop */
    while (agentite_is_running(app.engine)) {
        /* Start loading after a few frames so progress is visible */
        if (!loading_started) {
            frames_before_load--;
            if (frames_before_load <= 0) {
                loading_started = true;
                printf("Starting async texture loads...\n");
                for (int i = 0; i < app.texture_count; i++) {
                    snprintf(path, sizeof(path), "%s/texture_%d.tga", temp_dir, i);
                    Agentite_LoadRequest request = agentite_texture_load_async(
                        app.loader,
                        app.sprites,
                        app.registry,
                        path,
                        on_texture_loaded,
                        &app
                    );
                    if (!agentite_load_request_is_valid(request)) {
                        printf("Failed to queue load for %s\n", path);
                    }
                }
            }
        }
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);
        app.time += dt;

        /* Process input */
        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(app.engine);
            }
        }
        agentite_input_update(app.input);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(app.engine);
        }

        /* CRITICAL: Process async loader callbacks on main thread */
        agentite_async_loader_update(app.loader);

        /* Animate loaded textures */
        for (int i = 0; i < app.texture_count; i++) {
            if (app.textures[i].loaded) {
                /* Lerp position to target */
                float lerp_speed = 5.0f * dt;
                app.textures[i].x += (app.textures[i].target_x - app.textures[i].x) * lerp_speed;
                app.textures[i].y += (app.textures[i].target_y - app.textures[i].y) * lerp_speed;

                /* Grow scale */
                if (app.textures[i].scale < 1.0f) {
                    app.textures[i].scale += dt * 3.0f;
                    if (app.textures[i].scale > 1.0f) {
                        app.textures[i].scale = 1.0f;
                    }
                }

                /* Gentle rotation */
                app.textures[i].rotation = sinf(app.time + i * 0.5f) * 5.0f;
            }
        }

        agentite_camera_update(app.camera);

        /* Build sprite batch */
        agentite_sprite_begin(app.sprites, NULL);

        /* Draw loaded textures */
        for (int i = 0; i < app.texture_count; i++) {
            if (app.textures[i].loaded && app.textures[i].texture) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.textures[i].texture);
                agentite_sprite_draw_ex(
                    app.sprites, &sprite,
                    app.textures[i].x, app.textures[i].y,
                    app.textures[i].scale, app.textures[i].scale,
                    app.textures[i].rotation,
                    0.5f, 0.5f
                );
            }
        }

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            /* Upload sprites BEFORE render pass */
            agentite_sprite_upload(app.sprites, cmd);

            /* Prepare text BEFORE render pass */
            if (app.text && app.font) {
                agentite_text_begin(app.text);

                if (!app.all_loaded) {
                    /* Show loading progress - yellow text for visibility */
                    char buf[128];
                    snprintf(buf, sizeof(buf),
                             "Loading textures: %d / %d (%.0f%%)",
                             app.textures_loaded, app.texture_count,
                             app.load_progress * 100.0f);
                    agentite_text_draw_colored(app.text, app.font, buf,
                                               20.0f, 30.0f,
                                               1.0f, 1.0f, 0.0f, 1.0f);  /* Yellow */

                    size_t pending = agentite_async_pending_count(app.loader);
                    snprintf(buf, sizeof(buf), "Pending in queue: %zu", pending);
                    agentite_text_draw_colored(app.text, app.font, buf,
                                               20.0f, 60.0f,
                                               0.5f, 1.0f, 0.5f, 1.0f);  /* Light green */
                } else {
                    agentite_text_draw_colored(app.text, app.font,
                                              "All textures loaded! Press ESC to exit.",
                                              20.0f, 30.0f,
                                              0.0f, 1.0f, 0.0f, 1.0f);  /* Green */
                }

                /* Instructions - white */
                agentite_text_draw_colored(app.text, app.font,
                                  "Async Loading Demo - Textures load in background threads",
                                  20.0f, 680.0f,
                                  1.0f, 1.0f, 1.0f, 1.0f);

                agentite_text_end(app.text);
                agentite_text_upload(app.text, cmd);
            }

            /* Dark background during loading, lighter when done */
            float bg = app.all_loaded ? 0.2f : 0.1f;
            if (agentite_begin_render_pass(app.engine, bg, bg, bg + 0.05f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);

                /* Render sprites */
                agentite_sprite_render(app.sprites, cmd, pass);

                /* Render text on top */
                if (app.text && app.font) {
                    agentite_text_render(app.text, cmd, pass);
                }

                agentite_end_render_pass(app.engine);
            }
        }

        agentite_sprite_end(app.sprites, NULL, NULL);
        agentite_end_frame(app.engine);
    }

    /* Cleanup */
    printf("Shutting down...\n");

    /* Clean up temp files */
    for (int i = 0; i < app.texture_count; i++) {
        snprintf(path, sizeof(path), "%s/texture_%d.tga", temp_dir, i);
        remove(path);
    }
    RMDIR(temp_dir);

    /* Destroy in reverse order of creation */
    agentite_async_loader_destroy(app.loader);
    agentite_asset_registry_destroy(app.registry);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_camera_destroy(app.camera);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    printf("Done!\n");
    return 0;
}
