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

    /* Active effects */
    Agentite_BuiltinShader current_effect;
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

static const char *get_effect_name(Agentite_BuiltinShader effect) {
    switch (effect) {
        case AGENTITE_SHADER_NONE: return "None";
        case AGENTITE_SHADER_GRAYSCALE: return "Grayscale";
        case AGENTITE_SHADER_SEPIA: return "Sepia";
        case AGENTITE_SHADER_VIGNETTE: return "Vignette";
        case AGENTITE_SHADER_BLUR_BOX: return "Box Blur";
        case AGENTITE_SHADER_SCANLINES: return "Scanlines";
        case AGENTITE_SHADER_INVERT: return "Invert";
        case AGENTITE_SHADER_PIXELATE: return "Pixelate";
        default: return "Unknown";
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.current_effect = AGENTITE_SHADER_VIGNETTE;

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
        app.font = agentite_font_load(app.text, "assets/fonts/ProggyClean.ttf", 16);
    }

    /* Create shader system and post-process pipeline */
    app.shaders = agentite_shader_system_create(gpu);
    Agentite_PostProcessConfig pp_cfg = AGENTITE_POSTPROCESS_CONFIG_DEFAULT;
    pp_cfg.width = WINDOW_WIDTH;
    pp_cfg.height = WINDOW_HEIGHT;
    app.postprocess = agentite_postprocess_create(app.shaders, window, &pp_cfg);

    /* Create test scene */
    app.scene_texture = create_test_scene(app.sprites);

    printf("Shader System Example\n");
    printf("=====================\n");
    printf("1: Grayscale  2: Sepia  3: Vignette  4: Blur  5: Scanlines\n");
    printf("6: Invert     7: Pixelate  0: None\n");

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

        /* Select effect */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_0)) app.current_effect = AGENTITE_SHADER_NONE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1)) app.current_effect = AGENTITE_SHADER_GRAYSCALE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2)) app.current_effect = AGENTITE_SHADER_SEPIA;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3)) app.current_effect = AGENTITE_SHADER_VIGNETTE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4)) app.current_effect = AGENTITE_SHADER_BLUR_BOX;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_5)) app.current_effect = AGENTITE_SHADER_SCANLINES;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_6)) app.current_effect = AGENTITE_SHADER_INVERT;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_7)) app.current_effect = AGENTITE_SHADER_PIXELATE;

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

            agentite_sprite_upload(app.sprites, cmd);

            /* Get render target for post-processing */
            SDL_GPUTexture *target = agentite_postprocess_get_target(app.postprocess);

            /* Render scene to post-process target */
            if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                agentite_end_render_pass(app.engine);
            }

            /* Apply post-processing if effect is selected */
            if (app.current_effect != AGENTITE_SHADER_NONE && target) {
                Agentite_Shader *shader = agentite_shader_get_builtin(app.shaders, app.current_effect);
                if (shader) {
                    agentite_postprocess_begin(app.postprocess, cmd, target);
                    agentite_postprocess_apply(app.postprocess, cmd, NULL, shader, NULL);
                    agentite_postprocess_end(app.postprocess, cmd, NULL);
                }
            }

            /* Draw UI */
            if (app.text && app.font) {
                agentite_text_begin(app.text);
                char info[128];
                snprintf(info, sizeof(info), "Effect: %s", get_effect_name(app.current_effect));
                agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);
                agentite_text_draw_colored(app.text, app.font,
                    "0-7: Select effect",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);
                agentite_text_upload(app.text, cmd);

                /* Render UI in a separate pass or inline */
                if (agentite_begin_render_pass(app.engine, -1, -1, -1, -1)) {
                    SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                    agentite_text_render(app.text, cmd, pass);
                    agentite_end_render_pass(app.engine);
                }
            }

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
