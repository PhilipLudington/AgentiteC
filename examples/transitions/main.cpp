/**
 * Agentite Engine - Screen Transitions Example
 *
 * Demonstrates scene management with colored test scenes.
 *
 * NOTE: Transition effects are currently disabled pending engine API updates.
 * The transition system requires the ability to render to custom target textures,
 * which is the same architectural limitation as the postprocess system.
 *
 * Controls:
 *   1-3    - Switch to scene 1/2/3
 *   ESC    - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
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

    Agentite_Texture *scene_textures[3];
    int current_scene;
} AppState;

/* Create colored scene textures */
static Agentite_Texture *create_scene(Agentite_SpriteRenderer *sr, int scene_id) {
    int size = 512;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    /* Different color schemes for each scene */
    float base_r, base_g, base_b;
    const char *name;
    switch (scene_id) {
        case 0: base_r = 0.2f; base_g = 0.4f; base_b = 0.8f; name = "Blue"; break;
        case 1: base_r = 0.8f; base_g = 0.3f; base_b = 0.2f; name = "Red"; break;
        case 2: base_r = 0.2f; base_g = 0.7f; base_b = 0.3f; name = "Green"; break;
        default: base_r = 0.5f; base_g = 0.5f; base_b = 0.5f; name = "Gray"; break;
    }
    (void)name;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;

            /* Gradient with pattern */
            float fx = (float)x / size;
            float fy = (float)y / size;

            /* Add some visual interest */
            float pattern = sinf(fx * 20.0f) * cosf(fy * 20.0f) * 0.1f + 0.9f;
            float gradient = 1.0f - (fx + fy) * 0.3f;

            pixels[idx + 0] = (unsigned char)((base_r * pattern * gradient) * 255);
            pixels[idx + 1] = (unsigned char)((base_g * pattern * gradient) * 255);
            pixels[idx + 2] = (unsigned char)((base_b * pattern * gradient) * 255);
            pixels[idx + 3] = 255;

            /* Draw scene number indicator (white circle in center) */
            float cx = size / 2.0f;
            float cy = size / 2.0f;
            float dx = x - cx;
            float dy = y - cy;
            if (sqrtf(dx * dx + dy * dy) < 50.0f) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 255;
                pixels[idx + 2] = 255;
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
        .window_title = "Agentite - Screen Transitions Example",
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

    /* TODO: Transition system creation works, but effects can't be applied
     * until the engine supports rendering to custom target textures.
     * This is the same architectural limitation as the postprocess system.
     *
     * Agentite_ShaderSystem *shaders = agentite_shader_system_create(gpu);
     * Agentite_TransitionConfig trans_cfg = AGENTITE_TRANSITION_CONFIG_DEFAULT;
     * trans_cfg.width = WINDOW_WIDTH;
     * trans_cfg.height = WINDOW_HEIGHT;
     * Agentite_Transition *transition = agentite_transition_create(shaders, window, &trans_cfg);
     */

    /* Create scene textures */
    for (int i = 0; i < 3; i++) {
        app.scene_textures[i] = create_scene(app.sprites, i);
    }

    printf("Screen Transitions Example\n");
    printf("==========================\n");
    printf("1-3: Switch scenes\n");
    printf("NOTE: Transition effects pending engine API updates.\n");
    printf("ESC: Quit\n");

    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) agentite_quit(app.engine);
        }
        agentite_input_update(app.input);

        /* Switch scenes with number keys */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1))
            app.current_scene = 0;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2))
            app.current_scene = 1;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3))
            app.current_scene = 2;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            /* Prepare sprite batch */
            agentite_sprite_begin(app.sprites, NULL);

            /* Draw current scene */
            if (app.scene_textures[app.current_scene]) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.scene_textures[app.current_scene]);
                float px = (WINDOW_WIDTH - 512) / 2.0f;
                float py = (WINDOW_HEIGHT - 512) / 2.0f;
                agentite_sprite_draw(app.sprites, &sprite, px, py);
            }

            /* Prepare text batch */
            if (app.text && app.font) {
                agentite_text_begin(app.text);

                char info[128];
                snprintf(info, sizeof(info), "Scene: %d (Blue=1, Red=2, Green=3)",
                    app.current_scene + 1);
                agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "Transition effects pending - requires render-to-texture API",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "1-3: Switch scenes | ESC: Quit",
                    10, WINDOW_HEIGHT - 30, 0.5f, 0.5f, 0.5f, 0.9f);

                agentite_text_end(app.text);
            }

            /* Upload ALL data BEFORE render pass */
            agentite_sprite_upload(app.sprites, cmd);
            if (app.text) agentite_text_upload(app.text, cmd);

            /* Render pass */
            if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                if (app.text) agentite_text_render(app.text, cmd, pass);
                agentite_end_render_pass(app.engine);
            }
        }

        agentite_end_frame(app.engine);
    }

    for (int i = 0; i < 3; i++) {
        if (app.scene_textures[i]) agentite_texture_destroy(app.sprites, app.scene_textures[i]);
    }
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
