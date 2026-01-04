/**
 * Agentite Engine - Shader System Example
 *
 * Demonstrates the post-processing shader pipeline with built-in effects.
 * This example shows how to set up and use post-processing effects.
 *
 * Controls:
 *   1-5    - Toggle effects (grayscale, sepia, vignette, blur, scanlines)
 *   0      - Disable all effects
 *   Space  - Cycle through preset combinations
 *   ESC    - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/shader.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    Agentite_ShaderSystem *shaders;
    Agentite_PostProcess *postprocess;
    Agentite_Texture *scene_texture;

    float time;
} AppState;

/* Create a simple test scene texture */
static Agentite_Texture *create_test_scene(Agentite_SpriteRenderer *sr) {
    int size = 512;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;

            /* Gradient background with rings */
            float bx = (float)x / size;
            float by = (float)y / size;
            float cx = size / 2.0f;
            float cy = size / 2.0f;
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            float ring = sinf(dist * 0.1f) * 0.5f + 0.5f;

            pixels[idx + 0] = (unsigned char)((0.2f + ring * 0.3f + bx * 0.5f) * 255);
            pixels[idx + 1] = (unsigned char)((0.1f + ring * 0.4f + by * 0.4f) * 255);
            pixels[idx + 2] = (unsigned char)((0.3f + ring * 0.3f + (1.0f - bx) * 0.4f) * 255);
            pixels[idx + 3] = 255;

            /* Central bright spot */
            if (dist < 30.0f) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 200;
                pixels[idx + 2] = 100;
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}


int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};

    Agentite_Config config = {
        .window_title = "Agentite - Shader System Example",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) return 1;

    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    app.sprites = agentite_sprite_init(gpu, window);
    app.input = agentite_input_init();
    app.text = agentite_text_init(gpu, window);
    if (app.text) {
        app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 16);
    }

    /* Create shader system */
    app.shaders = agentite_shader_system_create(gpu);
    if (!app.shaders) {
        printf("ERROR: Failed to create shader system: %s\n", agentite_get_last_error());
        agentite_shutdown(app.engine);
        return 1;
    }

    /* TODO: Postprocess pipeline creation works, but effects can't be applied
     * until the engine supports rendering to custom target textures.
     * Uncomment when that API is available:
     *
     * Agentite_PostProcessConfig pp_cfg = AGENTITE_POSTPROCESS_CONFIG_DEFAULT;
     * pp_cfg.width = WINDOW_WIDTH;
     * pp_cfg.height = WINDOW_HEIGHT;
     * app.postprocess = agentite_postprocess_create(app.shaders, window, &pp_cfg);
     */
    app.postprocess = NULL;

    /* Create test scene */
    app.scene_texture = create_test_scene(app.sprites);

    printf("Shader System Example\n");
    printf("=====================\n");
    printf("Shader system initialized successfully.\n");
    printf("NOTE: Post-processing effects are not yet functional.\n");
    printf("      Requires engine API to render to custom targets.\n");
    printf("ESC: Quit\n");

    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);
        app.time += dt;

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) agentite_quit(app.engine);
        }
        agentite_input_update(app.input);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            /* Draw scene to sprite batch */
            agentite_sprite_begin(app.sprites, NULL);

            if (app.scene_texture) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.scene_texture);
                agentite_sprite_draw_scaled(app.sprites, &sprite,
                    (WINDOW_WIDTH - 512) / 2.0f, (WINDOW_HEIGHT - 512) / 2.0f,
                    1.0f, 1.0f);
            }

            /* Prepare UI text */
            if (app.text && app.font) {
                agentite_text_begin(app.text);
                agentite_text_draw_colored(app.text, app.font,
                    "Shader System Example", 10, 10, 1, 1, 1, 0.9f);
                agentite_text_draw_colored(app.text, app.font,
                    "Shader system initialized - postprocess effects pending",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);
                agentite_text_draw_colored(app.text, app.font,
                    "ESC: Quit",
                    10, WINDOW_HEIGHT - 30, 0.5f, 0.5f, 0.5f, 0.9f);
                agentite_text_end(app.text);
            }

            /* Upload all batched data BEFORE any render pass */
            agentite_sprite_upload(app.sprites, cmd);
            if (app.text) agentite_text_upload(app.text, cmd);

            /* Render scene to screen */
            if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                if (app.text) agentite_text_render(app.text, cmd, pass);
                agentite_end_render_pass(app.engine);
            }

            /* TODO: Post-processing effects require rendering to offscreen target first.
             * The current engine API (agentite_begin_render_pass) always targets swapchain.
             * To enable postprocess effects:
             * 1. Add API to render to custom target texture
             * 2. Render scene to postprocess target
             * 3. Apply postprocess shader to render processed result to swapchain
             */

            agentite_sprite_end(app.sprites, NULL, NULL);
        }

        agentite_end_frame(app.engine);
    }

    if (app.scene_texture) agentite_texture_destroy(app.sprites, app.scene_texture);
    agentite_postprocess_destroy(app.postprocess);
    agentite_shader_system_destroy(app.shaders);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
