/**
 * Agentite Engine - Sprites Example
 *
 * Demonstrates sprite rendering with transforms, batching, and camera.
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Create a simple checkerboard texture procedurally */
static Agentite_Texture *create_checker_texture(Agentite_SpriteRenderer *sr, int size) {
    unsigned char *pixels = malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int tile_x = x / 8;
            int tile_y = y / 8;
            bool white = ((tile_x + tile_y) % 2) == 0;
            int idx = (y * size + x) * 4;

            if (white) {
                pixels[idx + 0] = 255;
                pixels[idx + 1] = 220;
                pixels[idx + 2] = 180;
            } else {
                pixels[idx + 0] = 100;
                pixels[idx + 1] = 80;
                pixels[idx + 2] = 60;
            }
            pixels[idx + 3] = 255;
        }
    }

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Carbon - Sprites Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize sprite renderer */
    Agentite_SpriteRenderer *sprites = agentite_sprite_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );

    /* Initialize camera */
    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_sprite_set_camera(sprites, camera);
    agentite_camera_set_position(camera, 640.0f, 360.0f);

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();

    /* Create test texture and sprite */
    Agentite_Texture *tex = create_checker_texture(sprites, 64);
    Agentite_Sprite sprite = agentite_sprite_from_texture(tex);

    /* Animation state */
    float time = 0.0f;
    float rotation = 0.0f;

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);
        time += dt;
        rotation += 45.0f * dt;

        /* Process input */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Camera controls */
        float cam_speed = 200.0f * dt;
        if (agentite_input_key_pressed(input, SDL_SCANCODE_W))
            agentite_camera_move(camera, 0, -cam_speed);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_S))
            agentite_camera_move(camera, 0, cam_speed);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_A))
            agentite_camera_move(camera, -cam_speed, 0);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_D))
            agentite_camera_move(camera, cam_speed, 0);

        /* Zoom with mouse wheel */
        float scroll_x, scroll_y;
        agentite_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y != 0) {
            float zoom = agentite_camera_get_zoom(camera);
            zoom *= (scroll_y > 0) ? 1.1f : 0.9f;
            agentite_camera_set_zoom(camera, zoom);
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        agentite_camera_update(camera);

        /* Build sprite batch */
        agentite_sprite_begin(sprites, NULL);

        /* Grid of static sprites */
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 8; x++) {
                agentite_sprite_draw(sprites, &sprite,
                                   200.0f + x * 100.0f,
                                   150.0f + y * 100.0f);
            }
        }

        /* Rotating sprite in center */
        agentite_sprite_draw_ex(sprites, &sprite,
                              640.0f, 360.0f,      /* position */
                              2.0f, 2.0f,          /* scale */
                              rotation,            /* rotation */
                              0.5f, 0.5f);         /* origin */

        /* Pulsing sprite */
        float pulse = 1.0f + 0.3f * sinf(time * 3.0f);
        agentite_sprite_draw_scaled(sprites, &sprite,
                                  900.0f, 360.0f,
                                  pulse, pulse);

        /* Tinted sprites */
        agentite_sprite_draw_tinted(sprites, &sprite,
                                  400.0f, 500.0f,
                                  1.0f, 0.3f, 0.3f, 1.0f);  /* Red */
        agentite_sprite_draw_tinted(sprites, &sprite,
                                  500.0f, 500.0f,
                                  0.3f, 1.0f, 0.3f, 1.0f);  /* Green */
        agentite_sprite_draw_tinted(sprites, &sprite,
                                  600.0f, 500.0f,
                                  0.3f, 0.3f, 1.0f, 1.0f);  /* Blue */

        /* Acquire command buffer and upload */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            agentite_sprite_upload(sprites, cmd);

            if (agentite_begin_render_pass(engine, 0.15f, 0.15f, 0.2f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_sprite_render(sprites, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_sprite_end(sprites, NULL, NULL);
        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_texture_destroy(sprites, tex);
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_sprite_shutdown(sprites);
    agentite_shutdown(engine);

    return 0;
}
