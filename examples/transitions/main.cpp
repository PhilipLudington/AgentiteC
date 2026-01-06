/**
 * Agentite Engine - Screen Transitions Example
 *
 * Demonstrates scene transitions using the render-to-texture API.
 * Shows how to capture scenes to textures and blend between them.
 *
 * Controls:
 *   1-3    - Switch to scene 1/2/3 (with transition)
 *   T      - Cycle through transition effects
 *   E      - Cycle through easing functions
 *   +/-    - Adjust transition duration
 *   ESC    - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/shader.h"
#include "agentite/transition.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* All available transition effects */
static const Agentite_TransitionEffect DEMO_EFFECTS[] = {
    AGENTITE_TRANSITION_CROSSFADE,      /* Smooth blend between scenes */
    AGENTITE_TRANSITION_WIPE_LEFT,      /* Wipe from right to left */
    AGENTITE_TRANSITION_WIPE_RIGHT,     /* Wipe from left to right */
    AGENTITE_TRANSITION_WIPE_DOWN,      /* Wipe from top to bottom */
    AGENTITE_TRANSITION_WIPE_DIAGONAL,  /* Diagonal wipe */
    AGENTITE_TRANSITION_CIRCLE_OPEN,    /* Iris open from center */
    AGENTITE_TRANSITION_CIRCLE_CLOSE,   /* Iris close to center */
    AGENTITE_TRANSITION_SLIDE_LEFT,     /* Slide new scene from right */
    AGENTITE_TRANSITION_SLIDE_RIGHT,    /* Slide new scene from left */
    AGENTITE_TRANSITION_PUSH_LEFT,      /* Push old scene left */
    AGENTITE_TRANSITION_DISSOLVE,       /* Noise-based dissolve */
    AGENTITE_TRANSITION_PIXELATE,       /* Pixelate out/in */
    AGENTITE_TRANSITION_FADE,           /* Fade through black */
};
static const int NUM_DEMO_EFFECTS = sizeof(DEMO_EFFECTS) / sizeof(DEMO_EFFECTS[0]);

typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    /* Shader system for postprocess and transitions */
    Agentite_ShaderSystem *shaders;
    Agentite_PostProcess *postprocess;

    /* Scene textures (pre-rendered patterns) */
    Agentite_Texture *scene_textures[3];
    int current_scene;
    int pending_scene;  /* Scene to transition to (-1 if none) */
    int source_scene;   /* Scene we're transitioning FROM (preserved during transition) */

    /* Transition state */
    Agentite_Transition *transition;
    int current_effect_idx;
    Agentite_TransitionEasing current_easing;
    float transition_duration;

    /* Render targets for scene capture */
    SDL_GPUTexture *scene_target_a;  /* Source scene (outgoing) */
    SDL_GPUTexture *scene_target_b;  /* Destination scene (incoming) */

    /* UI background for text readability */
    Agentite_Texture *ui_bg_texture;

    float time;
} AppState;

/* Create colored scene textures with distinct patterns */
static Agentite_Texture *create_scene(Agentite_SpriteRenderer *sr, int scene_id) {
    int size = 512;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    /* Different color schemes and patterns for each scene */
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

            float fx = (float)x / size;
            float fy = (float)y / size;

            /* Different patterns per scene */
            float pattern;
            switch (scene_id) {
                case 0:  /* Concentric circles */
                    pattern = sinf(sqrtf((fx-0.5f)*(fx-0.5f) + (fy-0.5f)*(fy-0.5f)) * 30.0f) * 0.15f + 0.85f;
                    break;
                case 1:  /* Diagonal stripes */
                    pattern = sinf((fx + fy) * 20.0f) * 0.15f + 0.85f;
                    break;
                case 2:  /* Grid pattern */
                    pattern = (sinf(fx * 25.0f) * sinf(fy * 25.0f)) * 0.15f + 0.85f;
                    break;
                default:
                    pattern = 1.0f;
            }

            float gradient = 1.0f - (fx + fy) * 0.2f;

            pixels[idx + 0] = (unsigned char)((base_r * pattern * gradient) * 255);
            pixels[idx + 1] = (unsigned char)((base_g * pattern * gradient) * 255);
            pixels[idx + 2] = (unsigned char)((base_b * pattern * gradient) * 255);
            pixels[idx + 3] = 255;

            /* Draw scene number indicator (white circle in center) */
            float cx = size / 2.0f;
            float cy = size / 2.0f;
            float dx = x - cx;
            float dy = y - cy;
            float dist = sqrtf(dx * dx + dy * dy);
            if (dist < 50.0f) {
                float ring = dist < 45.0f ? 1.0f : 0.0f;
                pixels[idx + 0] = (unsigned char)(255 * ring);
                pixels[idx + 1] = (unsigned char)(255 * ring);
                pixels[idx + 2] = (unsigned char)(255 * ring);
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Create a solid color texture for UI backgrounds */
static Agentite_Texture *create_solid_texture(Agentite_SpriteRenderer *sr,
                                               unsigned char r, unsigned char g,
                                               unsigned char b, unsigned char a) {
    unsigned char pixels[4] = { r, g, b, a };
    return agentite_texture_create(sr, 1, 1, pixels);
}

/* Create a GPU render target texture */
static SDL_GPUTexture *create_render_target(SDL_GPUDevice *gpu, int width, int height) {
    SDL_GPUTextureCreateInfo tex_info = {};
    tex_info.type = SDL_GPU_TEXTURETYPE_2D;
    tex_info.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
    tex_info.width = (Uint32)width;
    tex_info.height = (Uint32)height;
    tex_info.layer_count_or_depth = 1;
    tex_info.num_levels = 1;
    tex_info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;

    return SDL_CreateGPUTexture(gpu, &tex_info);
}

/* Render a scene to a target texture */
static void render_scene_to_target(AppState *app, SDL_GPUCommandBuffer *cmd,
                                    SDL_GPUTexture *target, int scene_idx,
                                    float r, float g, float b) {
    /* Prepare sprite batch */
    agentite_sprite_begin(app->sprites, NULL);

    if (app->scene_textures[scene_idx]) {
        Agentite_Sprite sprite = agentite_sprite_from_texture(app->scene_textures[scene_idx]);
        /* Sprite uses centered origin (0.5, 0.5), so position is the CENTER */
        float px = WINDOW_WIDTH / 2.0f;
        float py = WINDOW_HEIGHT / 2.0f;
        agentite_sprite_draw(app->sprites, &sprite, px, py);
    }

    agentite_sprite_upload(app->sprites, cmd);

    /* Render to target texture */
    if (agentite_begin_render_pass_to_texture(app->engine, target,
            WINDOW_WIDTH, WINDOW_HEIGHT, r, g, b, 1.0f)) {
        SDL_GPURenderPass *pass = agentite_get_render_pass(app->engine);
        agentite_sprite_render(app->sprites, cmd, pass);
        agentite_end_render_pass_no_submit(app->engine);
    }
}

/* Debug: track which path we used */
static const char *s_last_effect_path = "none";

/* Render the transition effect using the transition system */
static void render_transition_effect(AppState *app, SDL_GPUCommandBuffer *cmd,
                                      SDL_GPURenderPass *pass,
                                      SDL_GPUTexture *source, SDL_GPUTexture *dest,
                                      float progress) {
    Agentite_TransitionEffect effect = DEMO_EFFECTS[app->current_effect_idx];

    /* Use the transition system's blend function for all effects */
    agentite_transition_set_effect(app->transition, effect);
    agentite_transition_render_blend(app->transition, cmd, pass, source, dest, progress);
    s_last_effect_path = agentite_transition_effect_name(effect);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.pending_scene = -1;
    app.transition_duration = 0.5f;
    app.current_easing = AGENTITE_EASING_EASE_IN_OUT;

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

    /* Create shader system */
    app.shaders = agentite_shader_system_create(gpu);
    if (!app.shaders) {
        printf("ERROR: Failed to create shader system\n");
        agentite_shutdown(app.engine);
        return 1;
    }

    /* Create postprocess for render-to-texture capability */
    Agentite_PostProcessConfig pp_cfg = AGENTITE_POSTPROCESS_CONFIG_DEFAULT;
    pp_cfg.width = WINDOW_WIDTH;
    pp_cfg.height = WINDOW_HEIGHT;
    app.postprocess = agentite_postprocess_create(app.shaders, window, &pp_cfg);

    /* Create transition system */
    Agentite_TransitionConfig trans_cfg = AGENTITE_TRANSITION_CONFIG_DEFAULT;
    trans_cfg.duration = app.transition_duration;
    trans_cfg.width = WINDOW_WIDTH;
    trans_cfg.height = WINDOW_HEIGHT;
    app.transition = agentite_transition_create(app.shaders, window, &trans_cfg);
    if (!app.transition) {
        printf("WARNING: Failed to create transition system: %s\n", agentite_get_last_error());
    }

    /* Create render targets for scene capture */
    app.scene_target_a = create_render_target(gpu, WINDOW_WIDTH, WINDOW_HEIGHT);
    app.scene_target_b = create_render_target(gpu, WINDOW_WIDTH, WINDOW_HEIGHT);
    if (!app.scene_target_a || !app.scene_target_b) {
        printf("ERROR: Failed to create render targets\n");
    }

    /* Create scene textures */
    for (int i = 0; i < 3; i++) {
        app.scene_textures[i] = create_scene(app.sprites, i);
    }

    /* Create UI background texture */
    app.ui_bg_texture = create_solid_texture(app.sprites, 0, 0, 0, 200);

    printf("Screen Transitions Example\n");
    printf("==========================\n");
    printf("1-3: Switch scenes (with transition)\n");
    printf("T: Cycle transition effect\n");
    printf("E: Cycle easing function\n");
    printf("+/-: Adjust duration\n");
    printf("ESC: Quit\n\n");

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

        /* Handle scene switching (only if not already transitioning) */
        bool is_transitioning = app.transition && agentite_transition_is_running(app.transition);

        if (!is_transitioning) {
            int new_scene = -1;
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1)) new_scene = 0;
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2)) new_scene = 1;
            if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3)) new_scene = 2;

            if (new_scene >= 0 && new_scene != app.current_scene) {
                /* Store source scene before we start transitioning */
                app.source_scene = app.current_scene;
                app.pending_scene = new_scene;

                /* Start transition */
                if (app.transition) {
                    agentite_transition_set_effect(app.transition, DEMO_EFFECTS[app.current_effect_idx]);
                    agentite_transition_set_easing(app.transition, app.current_easing);
                    agentite_transition_set_duration(app.transition, app.transition_duration);
                    agentite_transition_start(app.transition);
                }
            }
        }

        /* Cycle effects with T key */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_T)) {
            app.current_effect_idx = (app.current_effect_idx + 1) % NUM_DEMO_EFFECTS;
            printf("Effect: %s\n", agentite_transition_effect_name(DEMO_EFFECTS[app.current_effect_idx]));
        }

        /* Cycle easing with E key */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_E)) {
            app.current_easing = (Agentite_TransitionEasing)((app.current_easing + 1) % AGENTITE_EASING_COUNT);
            printf("Easing: %s\n", agentite_transition_easing_name(app.current_easing));
        }

        /* Adjust duration */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_EQUALS) ||
            agentite_input_key_just_pressed(app.input, SDL_SCANCODE_KP_PLUS)) {
            app.transition_duration += 0.1f;
            if (app.transition_duration > 3.0f) app.transition_duration = 3.0f;
            printf("Duration: %.1fs\n", app.transition_duration);
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_MINUS) ||
            agentite_input_key_just_pressed(app.input, SDL_SCANCODE_KP_MINUS)) {
            app.transition_duration -= 0.1f;
            if (app.transition_duration < 0.1f) app.transition_duration = 0.1f;
            printf("Duration: %.1fs\n", app.transition_duration);
        }

        /* Update transition state */
        if (app.transition) {
            agentite_transition_update(app.transition, dt);

            /* Check for transition completion - switch scene when done */
            if (agentite_transition_is_complete(app.transition)) {
                if (app.pending_scene >= 0) {
                    app.current_scene = app.pending_scene;
                    app.pending_scene = -1;
                }
            }
        }

        is_transitioning = app.transition && agentite_transition_is_running(app.transition);
        float progress = app.transition ? agentite_transition_get_eased_progress(app.transition) : 0.0f;

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            if (is_transitioning && app.scene_target_a && app.scene_target_b) {
                /* Transitioning: render both scenes to targets, then blend */

                /* Source is always the scene we started from (preserved in source_scene) */
                /* Dest is always the scene we're transitioning to (pending or current after midpoint) */
                int from_scene = app.source_scene;
                int to_scene = app.pending_scene >= 0 ? app.pending_scene : app.current_scene;

                /* Render source scene (old scene) */
                render_scene_to_target(&app, cmd, app.scene_target_a, from_scene, 0.1f, 0.1f, 0.15f);

                /* Render destination scene (new scene) */
                render_scene_to_target(&app, cmd, app.scene_target_b, to_scene, 0.1f, 0.1f, 0.15f);

                /* Prepare UI text */
                if (app.text && app.font) {
                    agentite_text_begin(app.text);

                    char info[256];
                    snprintf(info, sizeof(info), "Scene: %d | Effect: %s | Easing: %s",
                        app.current_scene + 1,
                        agentite_transition_effect_name(DEMO_EFFECTS[app.current_effect_idx]),
                        agentite_transition_easing_name(app.current_easing));
                    agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);

                    snprintf(info, sizeof(info), "Duration: %.1fs | Progress: %.0f%% | Path: %s",
                        app.transition_duration, progress * 100.0f, s_last_effect_path);
                    agentite_text_draw_colored(app.text, app.font, info, 10, 30, 0.7f, 1.0f, 0.7f, 0.9f);

                    agentite_text_draw_colored(app.text, app.font,
                        "1-3: Scenes | T: Effect | E: Easing | +/-: Duration",
                        10, WINDOW_HEIGHT - 30, 0.5f, 0.5f, 0.5f, 0.9f);
                    agentite_text_end(app.text);
                    agentite_text_upload(app.text, cmd);
                }

                /* Prepare UI backgrounds */
                agentite_sprite_begin(app.sprites, NULL);
                if (app.ui_bg_texture) {
                    Agentite_Sprite ui_bg = agentite_sprite_from_texture(app.ui_bg_texture);
                    agentite_sprite_draw_scaled(app.sprites, &ui_bg, 5, 5, 600, 55);
                    agentite_sprite_draw_scaled(app.sprites, &ui_bg, 5, WINDOW_HEIGHT - 35, 450, 26);
                }
                agentite_sprite_upload(app.sprites, cmd);

                /* Render transition to swapchain */
                if (agentite_begin_render_pass(app.engine, 0.0f, 0.0f, 0.0f, 1.0f)) {
                    SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);

                    /* Apply transition effect */
                    render_transition_effect(&app, cmd, pass,
                        app.scene_target_a, app.scene_target_b, progress);

                    /* Render UI on top */
                    agentite_sprite_render(app.sprites, cmd, pass);
                    if (app.text) agentite_text_render(app.text, cmd, pass);

                    agentite_end_render_pass(app.engine);
                }
            } else {
                /* Not transitioning: render current scene directly */

                /* Prepare sprite batch */
                agentite_sprite_begin(app.sprites, NULL);
                if (app.scene_textures[app.current_scene]) {
                    Agentite_Sprite sprite = agentite_sprite_from_texture(app.scene_textures[app.current_scene]);
                    /* Sprite uses centered origin (0.5, 0.5), so position is the CENTER */
                    float px = WINDOW_WIDTH / 2.0f;
                    float py = WINDOW_HEIGHT / 2.0f;
                    agentite_sprite_draw(app.sprites, &sprite, px, py);
                }

                /* Prepare text batch */
                if (app.text && app.font) {
                    agentite_text_begin(app.text);

                    char info[256];
                    snprintf(info, sizeof(info), "Scene: %d | Effect: %s | Easing: %s",
                        app.current_scene + 1,
                        agentite_transition_effect_name(DEMO_EFFECTS[app.current_effect_idx]),
                        agentite_transition_easing_name(app.current_easing));
                    agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);

                    snprintf(info, sizeof(info), "Duration: %.1fs | Ready",
                        app.transition_duration);
                    agentite_text_draw_colored(app.text, app.font, info, 10, 30, 0.7f, 1.0f, 0.7f, 0.9f);

                    agentite_text_draw_colored(app.text, app.font,
                        "1-3: Scenes | T: Effect | E: Easing | +/-: Duration",
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
        }

        agentite_end_frame(app.engine);
    }

    /* Wait for GPU before cleanup */
    SDL_WaitForGPUIdle(gpu);

    /* Cleanup render targets */
    if (app.scene_target_a) SDL_ReleaseGPUTexture(gpu, app.scene_target_a);
    if (app.scene_target_b) SDL_ReleaseGPUTexture(gpu, app.scene_target_b);

    /* Cleanup textures */
    for (int i = 0; i < 3; i++) {
        if (app.scene_textures[i]) agentite_texture_destroy(app.sprites, app.scene_textures[i]);
    }
    if (app.ui_bg_texture) agentite_texture_destroy(app.sprites, app.ui_bg_texture);

    /* Cleanup systems */
    agentite_transition_destroy(app.transition);
    agentite_postprocess_destroy(app.postprocess);
    agentite_shader_system_destroy(app.shaders);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
