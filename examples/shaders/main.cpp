/**
 * Agentite Engine - Shader System Example
 *
 * Demonstrates the post-processing shader pipeline with built-in effects.
 * This example shows how to set up and use post-processing effects.
 *
 * Controls:
 *   1-7    - Select effect (grayscale, sepia, invert, vignette, scanlines, pixelate, contrast)
 *   0      - Disable all effects (passthrough)
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
    Agentite_Texture *ui_bg_texture;  /* Dark background for text readability */

    Agentite_BuiltinShader current_effect;
    float time;
} AppState;

static const char *get_effect_name(Agentite_BuiltinShader effect) {
    switch (effect) {
        case AGENTITE_SHADER_NONE: return "None (Passthrough)";
        case AGENTITE_SHADER_GRAYSCALE: return "Grayscale";
        case AGENTITE_SHADER_SEPIA: return "Sepia";
        case AGENTITE_SHADER_INVERT: return "Invert";
        case AGENTITE_SHADER_VIGNETTE: return "Vignette";
        case AGENTITE_SHADER_SCANLINES: return "Scanlines";
        case AGENTITE_SHADER_PIXELATE: return "Pixelate";
        case AGENTITE_SHADER_CONTRAST: return "High Contrast";
        default: return "Unknown";
    }
}

/* Create a solid color texture for UI backgrounds */
static Agentite_Texture *create_solid_texture(Agentite_SpriteRenderer *sr,
                                               unsigned char r, unsigned char g,
                                               unsigned char b, unsigned char a) {
    unsigned char pixels[4] = { r, g, b, a };
    return agentite_texture_create(sr, 1, 1, pixels);
}

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

    /* Create postprocess pipeline */
    Agentite_PostProcessConfig pp_cfg = AGENTITE_POSTPROCESS_CONFIG_DEFAULT;
    pp_cfg.width = WINDOW_WIDTH;
    pp_cfg.height = WINDOW_HEIGHT;
    app.postprocess = agentite_postprocess_create(app.shaders, window, &pp_cfg);
    if (!app.postprocess) {
        printf("WARNING: Failed to create postprocess pipeline: %s\n", agentite_get_last_error());
        printf("Effects will be disabled.\n");
    }

    /* Create test scene */
    app.scene_texture = create_test_scene(app.sprites);

    /* Create dark background texture for UI text readability */
    app.ui_bg_texture = create_solid_texture(app.sprites, 0, 0, 0, 200);

    /* Start with grayscale effect to demonstrate postprocess */
    app.current_effect = AGENTITE_SHADER_GRAYSCALE;

    printf("Shader System Example\n");
    printf("=====================\n");
    printf("Controls:\n");
    printf("  0: No effect (passthrough)\n");
    printf("  1: Grayscale\n");
    printf("  2: Sepia\n");
    printf("  3: Invert\n");
    printf("  4: Vignette\n");
    printf("  5: Scanlines\n");
    printf("  6: Pixelate\n");
    printf("  7: High Contrast\n");
    printf("  ESC: Quit\n\n");

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

        /* Handle effect selection */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_0))
            app.current_effect = AGENTITE_SHADER_NONE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1))
            app.current_effect = AGENTITE_SHADER_GRAYSCALE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2))
            app.current_effect = AGENTITE_SHADER_SEPIA;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3))
            app.current_effect = AGENTITE_SHADER_INVERT;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4))
            app.current_effect = AGENTITE_SHADER_VIGNETTE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_5))
            app.current_effect = AGENTITE_SHADER_SCANLINES;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_6))
            app.current_effect = AGENTITE_SHADER_PIXELATE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_7))
            app.current_effect = AGENTITE_SHADER_CONTRAST;

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            /* Get postprocess target and effect shader */
            SDL_GPUTexture *pp_target = app.postprocess ? agentite_postprocess_get_target(app.postprocess) : NULL;
            Agentite_Shader *effect_shader = (app.current_effect != AGENTITE_SHADER_NONE)
                ? agentite_shader_get_builtin(app.shaders, app.current_effect)
                : NULL;

            bool use_postprocess = pp_target && effect_shader;

            if (app.current_effect != AGENTITE_SHADER_NONE && !effect_shader) {
                /* Shader not available for this platform */
                use_postprocess = false;
            }

            /* Prepare sprite batch */
            agentite_sprite_begin(app.sprites, NULL);

            if (app.scene_texture) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.scene_texture);
                agentite_sprite_draw_scaled(app.sprites, &sprite,
                    (WINDOW_WIDTH - 512) / 2.0f, (WINDOW_HEIGHT - 512) / 2.0f + 200.0f,
                    1.0f, 1.0f);
            }

            /* Prepare text batch (same for both paths) */
            if (app.text && app.font) {
                agentite_text_begin(app.text);
                agentite_text_draw_colored(app.text, app.font,
                    "Shader System Example", 10, 10, 1, 1, 1, 0.9f);

                char effect_text[64];
                if (app.current_effect == AGENTITE_SHADER_NONE) {
                    snprintf(effect_text, sizeof(effect_text), "Effect: None (Passthrough)");
                } else if (use_postprocess) {
                    snprintf(effect_text, sizeof(effect_text), "Effect: %s", get_effect_name(app.current_effect));
                } else {
                    snprintf(effect_text, sizeof(effect_text), "Effect: %s (N/A on Metal)", get_effect_name(app.current_effect));
                }
                agentite_text_draw_colored(app.text, app.font,
                    effect_text, 10, 30, 0.7f, 1.0f, 0.7f, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "0-7: Select effect | ESC: Quit",
                    10, WINDOW_HEIGHT - 30, 0.5f, 0.5f, 0.5f, 0.9f);
                agentite_text_end(app.text);
            }

            /* Upload ALL data BEFORE any render pass */
            agentite_sprite_upload(app.sprites, cmd);
            if (app.text) agentite_text_upload(app.text, cmd);

            if (use_postprocess) {
                /* Pass 1: Render scene to postprocess target texture */
                if (agentite_begin_render_pass_to_texture(app.engine, pp_target, 0.1f, 0.1f, 0.15f, 1.0f)) {
                    SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                    agentite_sprite_render(app.sprites, cmd, pass);
                    agentite_end_render_pass_no_submit(app.engine);
                }

                /* Prepare UI background sprites (rendered AFTER postprocess) */
                agentite_sprite_begin(app.sprites, NULL);
                if (app.ui_bg_texture) {
                    Agentite_Sprite ui_bg = agentite_sprite_from_texture(app.ui_bg_texture);
                    /* Top text area background - text at (10,10) and (10,30), font 16px */
                    agentite_sprite_draw_scaled(app.sprites, &ui_bg, 5, 5, 360, 55);
                    /* Bottom text area background - text at (10, WINDOW_HEIGHT-30) */
                    agentite_sprite_draw_scaled(app.sprites, &ui_bg, 5, WINDOW_HEIGHT - 35, 320, 26);
                }
                agentite_sprite_upload(app.sprites, cmd);

                /* Pass 2: Apply postprocess effect and render to swapchain */
                if (agentite_begin_render_pass(app.engine, 0.0f, 0.0f, 0.0f, 1.0f)) {
                    SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);

                    /* Set up effect parameters */
                    void *params = NULL;
                    Agentite_ShaderParams_Vignette vignette_params = { .intensity = 0.8f, .softness = 0.4f };
                    Agentite_ShaderParams_Scanlines scanline_params = { .intensity = 0.3f, .count = 240.0f };
                    Agentite_ShaderParams_Pixelate pixelate_params = { .pixel_size = 8.0f };
                    Agentite_ShaderParams_Adjust contrast_params = { .amount = 0.5f };

                    switch (app.current_effect) {
                        case AGENTITE_SHADER_VIGNETTE: params = &vignette_params; break;
                        case AGENTITE_SHADER_SCANLINES: params = &scanline_params; break;
                        case AGENTITE_SHADER_PIXELATE: params = &pixelate_params; break;
                        case AGENTITE_SHADER_CONTRAST: params = &contrast_params; break;
                        default: break;
                    }

                    /* Apply postprocess effect */
                    agentite_postprocess_begin(app.postprocess, cmd, pp_target);
                    agentite_postprocess_apply(app.postprocess, cmd, pass, effect_shader, params);
                    agentite_postprocess_end(app.postprocess, cmd, pass);

                    /* Render UI backgrounds on top of postprocess (not affected by effect) */
                    agentite_sprite_render(app.sprites, cmd, pass);

                    /* Render text on top (text is NOT postprocessed) */
                    if (app.text) agentite_text_render(app.text, cmd, pass);

                    agentite_end_render_pass(app.engine);
                }
                agentite_sprite_end(app.sprites, NULL, NULL);
            } else {
                /* No postprocess - direct render to swapchain */
                if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                    SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                    agentite_sprite_render(app.sprites, cmd, pass);
                    if (app.text) agentite_text_render(app.text, cmd, pass);
                    agentite_end_render_pass(app.engine);
                }
                agentite_sprite_end(app.sprites, NULL, NULL);
            }
        }

        agentite_end_frame(app.engine);
    }

    /* Wait for GPU to finish before cleanup */
    SDL_WaitForGPUIdle(gpu);

    if (app.scene_texture) agentite_texture_destroy(app.sprites, app.scene_texture);
    if (app.ui_bg_texture) agentite_texture_destroy(app.sprites, app.ui_bg_texture);
    agentite_postprocess_destroy(app.postprocess);
    agentite_shader_system_destroy(app.shaders);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
