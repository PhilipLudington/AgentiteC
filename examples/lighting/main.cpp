/**
 * Agentite Engine - 2D Lighting Example
 *
 * Demonstrates the 2D lighting system with:
 * - Point lights with configurable radius, color, falloff
 * - Spot lights with direction and cone angle
 * - Ambient lighting
 * - Shadow casting from occluders
 * - Day/night cycle simulation
 *
 * Controls:
 *   Click      - Add point light at mouse
 *   1-4        - Change light color (white, warm, cool, colored)
 *   S          - Toggle spot light mode
 *   +/-        - Adjust light radius
 *   A          - Toggle ambient light
 *   D          - Toggle day/night cycle
 *   O          - Toggle shadow casting
 *   R          - Clear all lights
 *   TAB        - Toggle debug view
 *   ESC        - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/lighting.h"
#include "agentite/shader.h"
#include "agentite/gizmos.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

#define MAX_LIGHTS 32

typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Gizmos *gizmos;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    Agentite_ShaderSystem *shaders;
    Agentite_LightingSystem *lighting;

    /* Settings */
    float light_radius;
    int color_mode;  /* 0=white, 1=warm, 2=cool, 3=color */
    bool spot_mode;
    bool day_night;
    bool shadows_enabled;
    bool show_debug;
    float day_cycle;

    /* Scene texture */
    Agentite_Texture *scene_texture;

    /* Render target for scene (used by lighting composite) */
    SDL_GPUTexture *scene_target;
} AppState;

/* Color presets */
static Agentite_LightColor LIGHT_COLORS[] = {
    {1.0f, 1.0f, 1.0f, 1.0f},      /* White */
    {1.0f, 0.8f, 0.5f, 1.0f},      /* Warm */
    {0.5f, 0.7f, 1.0f, 1.0f},      /* Cool */
    {1.0f, 0.3f, 0.3f, 1.0f},      /* Red-ish */
};

/* Create a simple scene with some visual elements */
static Agentite_Texture *create_scene(Agentite_SpriteRenderer *sr) {
    int size = 512;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;

            /* Checkerboard floor pattern */
            int tx = x / 32;
            int ty = y / 32;
            bool checker = ((tx + ty) % 2) == 0;

            /* Base floor color */
            if (checker) {
                pixels[idx + 0] = 80;
                pixels[idx + 1] = 80;
                pixels[idx + 2] = 90;
            } else {
                pixels[idx + 0] = 60;
                pixels[idx + 1] = 60;
                pixels[idx + 2] = 70;
            }
            pixels[idx + 3] = 255;

            /* Draw some walls/obstacles for shadows */
            /* Central pillar */
            if (x >= 200 && x <= 250 && y >= 200 && y <= 250) {
                pixels[idx + 0] = 100;
                pixels[idx + 1] = 80;
                pixels[idx + 2] = 60;
            }

            /* Wall segments */
            if ((x >= 50 && x <= 60 && y >= 100 && y <= 300) ||
                (x >= 400 && x <= 450 && y >= 50 && y <= 60) ||
                (x >= 300 && x <= 350 && y >= 350 && y <= 450)) {
                pixels[idx + 0] = 90;
                pixels[idx + 1] = 70;
                pixels[idx + 2] = 50;
            }
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Create shadow occluders matching the scene walls */
static void create_occluders(AppState *app) {
    float scene_offset_y = (WINDOW_HEIGHT - 512) / 2.0f + 300.0f;

    /* Central pillar */
    Agentite_Occluder pillar;
    pillar.type = AGENTITE_OCCLUDER_BOX;
    pillar.box.x = 200 + (WINDOW_WIDTH - 512) / 2.0f;
    pillar.box.y = 200 + scene_offset_y;
    pillar.box.w = 50;
    pillar.box.h = 50;
    agentite_lighting_add_occluder(app->lighting, &pillar);

    /* Left wall */
    Agentite_Occluder wall1;
    wall1.type = AGENTITE_OCCLUDER_BOX;
    wall1.box.x = 50 + (WINDOW_WIDTH - 512) / 2.0f;
    wall1.box.y = 100 + scene_offset_y;
    wall1.box.w = 10;
    wall1.box.h = 200;
    agentite_lighting_add_occluder(app->lighting, &wall1);

    /* Top wall */
    Agentite_Occluder wall2;
    wall2.type = AGENTITE_OCCLUDER_BOX;
    wall2.box.x = 400 + (WINDOW_WIDTH - 512) / 2.0f;
    wall2.box.y = 50 + scene_offset_y;
    wall2.box.w = 50;
    wall2.box.h = 10;
    agentite_lighting_add_occluder(app->lighting, &wall2);

    /* Bottom wall */
    Agentite_Occluder wall3;
    wall3.type = AGENTITE_OCCLUDER_BOX;
    wall3.box.x = 300 + (WINDOW_WIDTH - 512) / 2.0f;
    wall3.box.y = 350 + scene_offset_y;
    wall3.box.w = 50;
    wall3.box.h = 100;
    agentite_lighting_add_occluder(app->lighting, &wall3);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.light_radius = 150.0f;
    app.shadows_enabled = true;
    app.show_debug = false;

    Agentite_Config config = {
        .window_title = "Agentite - 2D Lighting Example",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) return 1;

    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    app.sprites = agentite_sprite_init(gpu, window);
    app.gizmos = agentite_gizmos_create(gpu, NULL);
    app.input = agentite_input_init();
    app.text = agentite_text_init(gpu, window);
    if (app.text) {
        app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 16);
    }

    /* Use logical window dimensions for gizmos to match light coordinates */
    agentite_gizmos_set_screen_size(app.gizmos, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Create shader and lighting systems */
    app.shaders = agentite_shader_system_create(gpu);

    Agentite_LightingConfig light_cfg = AGENTITE_LIGHTING_CONFIG_DEFAULT;
    light_cfg.max_point_lights = MAX_LIGHTS;
    light_cfg.lightmap_width = WINDOW_WIDTH;
    light_cfg.lightmap_height = WINDOW_HEIGHT;
    app.lighting = agentite_lighting_create(gpu, app.shaders, window, &light_cfg);

    /* Set initial ambient light (dark) */
    agentite_lighting_set_ambient(app.lighting, 0.15f, 0.15f, 0.2f, 1.0f);

    /* Create scene texture */
    app.scene_texture = create_scene(app.sprites);

    /* Create scene render target for lighting composite */
    {
        SDL_GPUTextureCreateInfo tex_info = {};
        tex_info.type = SDL_GPU_TEXTURETYPE_2D;
        tex_info.format = SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM;
        tex_info.width = WINDOW_WIDTH;
        tex_info.height = WINDOW_HEIGHT;
        tex_info.layer_count_or_depth = 1;
        tex_info.num_levels = 1;
        tex_info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        tex_info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;
        app.scene_target = SDL_CreateGPUTexture(gpu, &tex_info);
        if (!app.scene_target) {
            printf("WARNING: Failed to create scene render target\n");
        }
    }

    /* Create occluders */
    create_occluders(&app);

    /* Add initial light at scene center */
    /* Use logical coordinates (matching sprite/lightmap space, not physical pixels) */
    float scene_center_x = 420.0f;
    float scene_center_y = 400.0f;

    Agentite_PointLightDesc initial_light = AGENTITE_POINT_LIGHT_DEFAULT;
    initial_light.x = scene_center_x;
    initial_light.y = scene_center_y;
    initial_light.radius = 200.0f;
    initial_light.color = LIGHT_COLORS[1];  /* Warm */
    initial_light.casts_shadows = true;
    agentite_lighting_add_point_light(app.lighting, &initial_light);

    printf("2D Lighting Example\n");
    printf("===================\n");
    printf("Click: Add light  1-4: Color  S: Spot mode\n");
    printf("+/-: Radius  A: Ambient  D: Day/Night  O: Shadows\n");
    printf("R: Clear  TAB: Debug\n");

    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) agentite_quit(app.engine);

            /* Add light on click - use logical coordinates (matching lightmap space) */
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {

                /* event.button.x/y are already in logical coordinates */
                float mouse_x = event.button.x;
                float mouse_y = event.button.y;

                if (app.spot_mode) {
                    Agentite_SpotLightDesc spot = AGENTITE_SPOT_LIGHT_DEFAULT;
                    spot.x = mouse_x;
                    spot.y = mouse_y;
                    spot.radius = app.light_radius;
                    spot.direction_y = 1.0f;  /* Point down */
                    spot.color = LIGHT_COLORS[app.color_mode];
                    spot.casts_shadows = app.shadows_enabled;
                    agentite_lighting_add_spot_light(app.lighting, &spot);
                } else {
                    Agentite_PointLightDesc point = AGENTITE_POINT_LIGHT_DEFAULT;
                    point.x = mouse_x;
                    point.y = mouse_y;
                    point.radius = app.light_radius;
                    point.color = LIGHT_COLORS[app.color_mode];
                    point.casts_shadows = app.shadows_enabled;
                    agentite_lighting_add_point_light(app.lighting, &point);
                }
            }
        }
        agentite_input_update(app.input);

        /* Color selection */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1)) app.color_mode = 0;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2)) app.color_mode = 1;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3)) app.color_mode = 2;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4)) app.color_mode = 3;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_S))
            app.spot_mode = !app.spot_mode;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_EQUALS))
            app.light_radius = fminf(400.0f, app.light_radius + 20.0f);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_MINUS))
            app.light_radius = fmaxf(30.0f, app.light_radius - 20.0f);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_A)) {
            /* Toggle ambient brightness */
            static bool bright_ambient = false;
            bright_ambient = !bright_ambient;
            if (bright_ambient) {
                agentite_lighting_set_ambient(app.lighting, 0.4f, 0.4f, 0.45f, 1.0f);
            } else {
                agentite_lighting_set_ambient(app.lighting, 0.15f, 0.15f, 0.2f, 1.0f);
            }
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_D))
            app.day_night = !app.day_night;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_O))
            app.shadows_enabled = !app.shadows_enabled;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_R))
            agentite_lighting_clear_lights(app.lighting);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_TAB))
            app.show_debug = !app.show_debug;

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Day/night cycle */
        if (app.day_night) {
            app.day_cycle += dt * 0.2f;
            float brightness = (sinf(app.day_cycle) + 1.0f) * 0.3f + 0.1f;
            float warmth = (sinf(app.day_cycle + 0.5f) + 1.0f) * 0.2f;
            agentite_lighting_set_ambient(app.lighting,
                brightness + warmth * 0.3f,
                brightness,
                brightness + 0.05f,
                1.0f);
        }

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            /* Prepare sprite batch */
            agentite_sprite_begin(app.sprites, NULL);
            if (app.scene_texture) {
                Agentite_Sprite sprite = agentite_sprite_from_texture(app.scene_texture);
                float px = (WINDOW_WIDTH - 512) / 2.0f;
                float py = (WINDOW_HEIGHT - 512) / 2.0f + 300.0f;
                agentite_sprite_draw(app.sprites, &sprite, px, py);
            }

            /* Prepare text batch */
            if (app.text && app.font) {
                agentite_text_begin(app.text);
                static const char *COLOR_NAMES[] = {"White", "Warm", "Cool", "Colored"};
                char info[256];
                Agentite_LightingStats stats;
                agentite_lighting_get_stats(app.lighting, &stats);
                snprintf(info, sizeof(info),
                    "Lights: %d  Mode: %s  Color: %s  Radius: %.0f  Debug: %s",
                    stats.point_light_count + stats.spot_light_count,
                    app.spot_mode ? "Spot" : "Point",
                    COLOR_NAMES[app.color_mode],
                    app.light_radius,
                    app.show_debug ? "ON" : "OFF");
                agentite_text_draw_colored(app.text, app.font, info, 10, 10, 1, 1, 1, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "Click: Add  1-4: Color  S: Spot  +/-: Size  A: Ambient  D: Day/Night",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "O: Shadows  R: Clear  TAB: Debug  ESC: Quit",
                    10, WINDOW_HEIGHT - 30, 0.5f, 0.5f, 0.5f, 0.9f);

                agentite_text_end(app.text);
            }

            /* Upload ALL data BEFORE render pass */
            agentite_sprite_upload(app.sprites, cmd);
            if (app.text) agentite_text_upload(app.text, cmd);

            /* Prepare gizmos for debug visualization */
            if (app.show_debug) {
                agentite_gizmos_begin(app.gizmos, NULL);

                /* Draw light positions and radii */
                /* Note: Light IDs start at 1, not 0. Iterate through potential IDs. */
                Agentite_LightingStats debug_stats;
                agentite_lighting_get_stats(app.lighting, &debug_stats);

                uint32_t found = 0;
                for (uint32_t id = 1; found < debug_stats.point_light_count && id < 1000; id++) {
                    Agentite_PointLightDesc light;
                    if (agentite_lighting_get_point_light(app.lighting, id, &light)) {
                        found++;
                        /* Convert color to RGBA packed uint32 */
                        uint32_t r = (uint32_t)(light.color.r * 255) & 0xFF;
                        uint32_t g = (uint32_t)(light.color.g * 255) & 0xFF;
                        uint32_t b = (uint32_t)(light.color.b * 255) & 0xFF;
                        uint32_t color = (r << 24) | (g << 16) | (b << 8) | 0x80;

                        /* Draw light radius circle */
                        agentite_gizmos_circle_2d(app.gizmos,
                            light.x, light.y, light.radius, color);
                        /* Draw light center */
                        agentite_gizmos_circle_2d(app.gizmos,
                            light.x, light.y, 5.0f, 0xFFFFFFFF);
                    }
                }

                agentite_gizmos_end(app.gizmos);
                agentite_gizmos_upload(app.gizmos, cmd);
            }

            /* Step 1: Render lights to lightmap */
            agentite_lighting_begin(app.lighting);
            agentite_lighting_render_lights(app.lighting, cmd, NULL);

            /* Step 2: Render scene to intermediate texture */
            if (app.scene_target && agentite_begin_render_pass_to_texture(
                    app.engine, app.scene_target, 0.05f, 0.05f, 0.1f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                agentite_end_render_pass_no_submit(app.engine);
            }

            /* Step 3: Composite scene + lighting to swapchain */
            if (agentite_begin_render_pass(app.engine, 0.0f, 0.0f, 0.0f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);

                /* Apply lighting (composites scene_target with lightmap) */
                if (app.scene_target) {
                    agentite_lighting_apply(app.lighting, cmd, pass, app.scene_target);
                }

                /* Debug visualization (rendered on top, not affected by lighting) */
                if (app.show_debug) {
                    agentite_gizmos_render(app.gizmos, cmd, pass);
                }

                /* Render text UI (on top, not affected by lighting) */
                if (app.text) agentite_text_render(app.text, cmd, pass);

                agentite_end_render_pass(app.engine);
            }
        }

        agentite_end_frame(app.engine);
    }

    if (app.scene_target) {
        SDL_ReleaseGPUTexture(agentite_get_gpu_device(app.engine), app.scene_target);
    }
    if (app.scene_texture) agentite_texture_destroy(app.sprites, app.scene_texture);
    agentite_lighting_destroy(app.lighting);
    agentite_shader_system_destroy(app.shaders);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_gizmos_destroy(app.gizmos);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
