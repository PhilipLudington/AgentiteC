/**
 * Carbon Engine - Sprites Example
 *
 * Demonstrates sprite rendering with transforms, batching, and camera.
 */

#include "carbon/carbon.h"
#include "carbon/sprite.h"
#include "carbon/camera.h"
#include "carbon/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Create a simple checkerboard texture procedurally */
static Carbon_Texture *create_checker_texture(Carbon_SpriteRenderer *sr, int size) {
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

    Carbon_Texture *tex = carbon_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Carbon_Config config = {
        .window_title = "Carbon - Sprites Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize sprite renderer */
    Carbon_SpriteRenderer *sprites = carbon_sprite_init(
        carbon_get_gpu_device(engine),
        carbon_get_window(engine)
    );

    /* Initialize camera */
    Carbon_Camera *camera = carbon_camera_create(1280.0f, 720.0f);
    carbon_sprite_set_camera(sprites, camera);
    carbon_camera_set_position(camera, 640.0f, 360.0f);

    /* Initialize input */
    Carbon_Input *input = carbon_input_init();

    /* Create test texture and sprite */
    Carbon_Texture *tex = create_checker_texture(sprites, 64);
    Carbon_Sprite sprite = carbon_sprite_from_texture(tex);

    /* Animation state */
    float time = 0.0f;
    float rotation = 0.0f;

    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        float dt = carbon_get_delta_time(engine);
        time += dt;
        rotation += 45.0f * dt;

        /* Process input */
        carbon_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            carbon_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                carbon_quit(engine);
            }
        }
        carbon_input_update(input);

        /* Camera controls */
        float cam_speed = 200.0f * dt;
        if (carbon_input_key_pressed(input, SDL_SCANCODE_W))
            carbon_camera_move(camera, 0, -cam_speed);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_S))
            carbon_camera_move(camera, 0, cam_speed);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_A))
            carbon_camera_move(camera, -cam_speed, 0);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_D))
            carbon_camera_move(camera, cam_speed, 0);

        /* Zoom with mouse wheel */
        float scroll_x, scroll_y;
        carbon_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y != 0) {
            float zoom = carbon_camera_get_zoom(camera);
            zoom *= (scroll_y > 0) ? 1.1f : 0.9f;
            carbon_camera_set_zoom(camera, zoom);
        }

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            carbon_quit(engine);

        carbon_camera_update(camera);

        /* Build sprite batch */
        carbon_sprite_begin(sprites, NULL);

        /* Grid of static sprites */
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 8; x++) {
                carbon_sprite_draw(sprites, &sprite,
                                   200.0f + x * 100.0f,
                                   150.0f + y * 100.0f);
            }
        }

        /* Rotating sprite in center */
        carbon_sprite_draw_ex(sprites, &sprite,
                              640.0f, 360.0f,      /* position */
                              2.0f, 2.0f,          /* scale */
                              rotation,            /* rotation */
                              0.5f, 0.5f);         /* origin */

        /* Pulsing sprite */
        float pulse = 1.0f + 0.3f * sinf(time * 3.0f);
        carbon_sprite_draw_scaled(sprites, &sprite,
                                  900.0f, 360.0f,
                                  pulse, pulse);

        /* Tinted sprites */
        carbon_sprite_draw_tinted(sprites, &sprite,
                                  400.0f, 500.0f,
                                  1.0f, 0.3f, 0.3f, 1.0f);  /* Red */
        carbon_sprite_draw_tinted(sprites, &sprite,
                                  500.0f, 500.0f,
                                  0.3f, 1.0f, 0.3f, 1.0f);  /* Green */
        carbon_sprite_draw_tinted(sprites, &sprite,
                                  600.0f, 500.0f,
                                  0.3f, 0.3f, 1.0f, 1.0f);  /* Blue */

        /* Acquire command buffer and upload */
        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            carbon_sprite_upload(sprites, cmd);

            if (carbon_begin_render_pass(engine, 0.15f, 0.15f, 0.2f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(engine);
                carbon_sprite_render(sprites, cmd, pass);
                carbon_end_render_pass(engine);
            }
        }

        carbon_sprite_end(sprites, NULL, NULL);
        carbon_end_frame(engine);
    }

    /* Cleanup */
    carbon_texture_destroy(sprites, tex);
    carbon_input_shutdown(input);
    carbon_camera_destroy(camera);
    carbon_sprite_shutdown(sprites);
    carbon_shutdown(engine);

    return 0;
}
