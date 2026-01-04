/**
 * Agentite Engine - Screen Transitions Example
 *
 * Demonstrates various screen transition effects:
 * - Fade through color
 * - Crossfade between scenes
 * - Wipe (directional)
 * - Dissolve
 * - Slide/Push
 * - Circle open/close (iris)
 *
 * Controls:
 *   1-9    - Trigger different transition types
 *   Space  - Toggle auto-transition demo
 *   +/-    - Adjust transition duration
 *   ESC    - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/transition.h"
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
    Agentite_Transition *transition;

    Agentite_Texture *scene_textures[3];
    int current_scene;
    float duration;
    bool auto_demo;
    float auto_timer;
} AppState;

/* Create colored scene textures */
static Agentite_Texture *create_scene(Agentite_SpriteRenderer *sr, int scene_id) {
    int size = 512;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    /* Different color schemes for each scene */
    float base_r, base_g, base_b;
    switch (scene_id) {
        case 0: base_r = 0.2f; base_g = 0.4f; base_b = 0.8f; break;  /* Blue */
        case 1: base_r = 0.8f; base_g = 0.3f; base_b = 0.2f; break;  /* Red */
        case 2: base_r = 0.2f; base_g = 0.7f; base_b = 0.3f; break;  /* Green */
        default: base_r = 0.5f; base_g = 0.5f; base_b = 0.5f; break;
    }

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

            /* Draw scene number indicator */
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

/* Start a transition to the next scene */
static void start_transition(AppState *app, Agentite_TransitionEffect effect) {
    agentite_transition_set_effect(app->transition, effect);
    agentite_transition_set_duration(app->transition, app->duration);

    /* Customize fade color for fade transitions */
    if (effect == AGENTITE_TRANSITION_FADE) {
        agentite_transition_set_fade_color(app->transition, 0.0f, 0.0f, 0.0f, 1.0f);
    }

    agentite_transition_start(app->transition);

    /* Change to next scene at midpoint */
    app->current_scene = (app->current_scene + 1) % 3;
}

static const char *get_effect_name(Agentite_TransitionEffect e) {
    switch (e) {
        case AGENTITE_TRANSITION_FADE: return "Fade";
        case AGENTITE_TRANSITION_CROSSFADE: return "Crossfade";
        case AGENTITE_TRANSITION_WIPE_LEFT: return "Wipe Left";
        case AGENTITE_TRANSITION_WIPE_RIGHT: return "Wipe Right";
        case AGENTITE_TRANSITION_WIPE_DOWN: return "Wipe Down";
        case AGENTITE_TRANSITION_DISSOLVE: return "Dissolve";
        case AGENTITE_TRANSITION_SLIDE_LEFT: return "Slide Left";
        case AGENTITE_TRANSITION_PUSH_LEFT: return "Push Left";
        case AGENTITE_TRANSITION_CIRCLE_CLOSE: return "Circle Close";
        default: return "Unknown";
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.duration = 0.5f;

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

    /* Create shader system for transitions */
    app.shaders = agentite_shader_system_create(gpu);

    /* Create transition system */
    Agentite_TransitionConfig trans_cfg = AGENTITE_TRANSITION_CONFIG_DEFAULT;
    trans_cfg.width = WINDOW_WIDTH;
    trans_cfg.height = WINDOW_HEIGHT;
    app.transition = agentite_transition_create(app.shaders, window, &trans_cfg);

    /* Create scene textures */
    for (int i = 0; i < 3; i++) {
        app.scene_textures[i] = create_scene(app.sprites, i);
    }

    printf("Screen Transitions Example\n");
    printf("==========================\n");
    printf("1: Fade        2: Crossfade  3: Wipe Left\n");
    printf("4: Wipe Right  5: Wipe Down  6: Dissolve\n");
    printf("7: Slide       8: Push       9: Circle\n");
    printf("+/-: Duration  Space: Auto demo\n");

    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) agentite_quit(app.engine);
        }
        agentite_input_update(app.input);

        /* Trigger transitions */
        if (!agentite_transition_is_active(app.transition)) {
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1))
                start_transition(&app, AGENTITE_TRANSITION_FADE);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2))
                start_transition(&app, AGENTITE_TRANSITION_CROSSFADE);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3))
                start_transition(&app, AGENTITE_TRANSITION_WIPE_LEFT);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4))
                start_transition(&app, AGENTITE_TRANSITION_WIPE_RIGHT);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_5))
                start_transition(&app, AGENTITE_TRANSITION_WIPE_DOWN);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_6))
                start_transition(&app, AGENTITE_TRANSITION_DISSOLVE);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_7))
                start_transition(&app, AGENTITE_TRANSITION_SLIDE_LEFT);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_8))
                start_transition(&app, AGENTITE_TRANSITION_PUSH_LEFT);
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_9))
                start_transition(&app, AGENTITE_TRANSITION_CIRCLE_CLOSE);
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_SPACE))
            app.auto_demo = !app.auto_demo;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_EQUALS))
            app.duration = fminf(2.0f, app.duration + 0.1f);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_MINUS))
            app.duration = fmaxf(0.1f, app.duration - 0.1f);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Auto demo mode */
        if (app.auto_demo && !agentite_transition_is_active(app.transition)) {
            app.auto_timer += dt;
            if (app.auto_timer > 2.0f) {
                app.auto_timer = 0;
                static int effect_idx = 0;
                Agentite_TransitionEffect effects[] = {
                    AGENTITE_TRANSITION_FADE,
                    AGENTITE_TRANSITION_CROSSFADE,
                    AGENTITE_TRANSITION_WIPE_LEFT,
                    AGENTITE_TRANSITION_DISSOLVE,
                    AGENTITE_TRANSITION_CIRCLE_CLOSE
                };
                start_transition(&app, effects[effect_idx % 5]);
                effect_idx++;
            }
        }

        /* Update transition */
        agentite_transition_update(app.transition, dt);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            agentite_sprite_begin(app.sprites, NULL);

            /* Draw current scene */
            if (app.scene_textures[app.current_scene]) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.scene_textures[app.current_scene]);
                float px = (WINDOW_WIDTH - 512) / 2.0f;
                float py = (WINDOW_HEIGHT - 512) / 2.0f;
                agentite_sprite_draw(app.sprites, &sprite, px, py);
            }

            agentite_sprite_upload(app.sprites, cmd);

            /* Build text batch before render pass */
            if (app.text && app.font) {
                agentite_text_begin(app.text);
                char info[128];
                snprintf(info, sizeof(info), "Scene: %d  Duration: %.1fs  Auto: %s",
                    app.current_scene + 1, app.duration, app.auto_demo ? "ON" : "OFF");
                agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);

                if (agentite_transition_is_active(app.transition)) {
                    agentite_text_draw_colored(app.text, app.font,
                        "Transitioning...", 10, 30, 1.0f, 1.0f, 0.3f, 1.0f);
                } else {
                    agentite_text_draw_colored(app.text, app.font,
                        "1-9: Transitions  Space: Auto  +/-: Duration",
                        10, 30, 0.7f, 0.7f, 0.7f, 0.9f);
                }

                agentite_text_end(app.text);
                agentite_text_upload(app.text, cmd);
            }

            if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);

                /* Render transition overlay if active */
                if (agentite_transition_is_active(app.transition)) {
                    agentite_transition_render(app.transition, cmd, pass, NULL);
                }

                /* Render text UI */
                if (app.text && app.font) {
                    agentite_text_render(app.text, cmd, pass);
                }

                agentite_end_render_pass(app.engine);
            }

            agentite_sprite_end(app.sprites, NULL, NULL);
        }

        agentite_end_frame(app.engine);
    }

    for (int i = 0; i < 3; i++) {
        if (app.scene_textures[i]) agentite_texture_destroy(app.sprites, app.scene_textures[i]);
    }
    agentite_transition_destroy(app.transition);
    agentite_shader_system_destroy(app.shaders);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
