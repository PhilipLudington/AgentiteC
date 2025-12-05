/**
 * Carbon Engine - Animation Example
 *
 * Demonstrates sprite-based animation with the animation system.
 */

#include "carbon/carbon.h"
#include "carbon/sprite.h"
#include "carbon/animation.h"
#include "carbon/camera.h"
#include "carbon/input.h"
#include <stdio.h>
#include <stdlib.h>

/* Create a procedural sprite sheet (4 frames of a simple animation) */
static Carbon_Texture *create_animation_sheet(Carbon_SpriteRenderer *sr) {
    int frame_size = 64;
    int num_frames = 4;
    int width = frame_size * num_frames;
    int height = frame_size;

    unsigned char *pixels = malloc(width * height * 4);
    if (!pixels) return NULL;

    /* Create 4 frames showing a pulsing/spinning effect */
    for (int frame = 0; frame < num_frames; frame++) {
        int offset_x = frame * frame_size;

        for (int y = 0; y < frame_size; y++) {
            for (int x = 0; x < frame_size; x++) {
                int idx = ((y * width) + (offset_x + x)) * 4;

                /* Center of frame */
                float cx = x - frame_size / 2.0f;
                float cy = y - frame_size / 2.0f;
                float dist = sqrtf(cx * cx + cy * cy);

                /* Create a ring pattern that changes per frame */
                float ring_size = 20.0f + frame * 4.0f;
                float thickness = 8.0f;

                if (dist > ring_size - thickness && dist < ring_size + thickness) {
                    /* Ring color (varies by frame) */
                    pixels[idx + 0] = 100 + frame * 40;
                    pixels[idx + 1] = 200 - frame * 20;
                    pixels[idx + 2] = 255;
                    pixels[idx + 3] = 255;
                } else if (dist < 10.0f) {
                    /* Center dot */
                    pixels[idx + 0] = 255;
                    pixels[idx + 1] = 255;
                    pixels[idx + 2] = 100;
                    pixels[idx + 3] = 255;
                } else {
                    /* Transparent */
                    pixels[idx + 0] = 0;
                    pixels[idx + 1] = 0;
                    pixels[idx + 2] = 0;
                    pixels[idx + 3] = 0;
                }
            }
        }
    }

    Carbon_Texture *tex = carbon_texture_create(sr, width, height, pixels);
    free(pixels);
    return tex;
}

/* Animation completion callback */
static void on_animation_complete(void *userdata) {
    int *count = (int *)userdata;
    (*count)++;
    SDL_Log("Animation completed! Total: %d", *count);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Carbon_Config config = {
        .window_title = "Carbon - Animation Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    Carbon_SpriteRenderer *sprites = carbon_sprite_init(
        carbon_get_gpu_device(engine),
        carbon_get_window(engine)
    );

    Carbon_Camera *camera = carbon_camera_create(1280.0f, 720.0f);
    carbon_sprite_set_camera(sprites, camera);

    Carbon_Input *input = carbon_input_init();

    /* Create animation sprite sheet */
    Carbon_Texture *sheet = create_animation_sheet(sprites);

    /* Create animations with different settings */
    Carbon_Animation *anim_loop = carbon_animation_from_strip(sheet, 0, 0, 64, 64, 4);
    carbon_animation_set_fps(anim_loop, 8.0f);

    Carbon_Animation *anim_once = carbon_animation_from_strip(sheet, 0, 0, 64, 64, 4);
    carbon_animation_set_fps(anim_once, 4.0f);

    Carbon_Animation *anim_pingpong = carbon_animation_from_strip(sheet, 0, 0, 64, 64, 4);
    carbon_animation_set_fps(anim_pingpong, 6.0f);

    /* Create animation players */
    Carbon_AnimationPlayer player_loop, player_once, player_pingpong;

    carbon_animation_player_init(&player_loop, anim_loop);
    carbon_animation_player_set_mode(&player_loop, CARBON_ANIM_LOOP);
    carbon_animation_player_play(&player_loop);

    carbon_animation_player_init(&player_once, anim_once);
    carbon_animation_player_set_mode(&player_once, CARBON_ANIM_ONCE);

    carbon_animation_player_init(&player_pingpong, anim_pingpong);
    carbon_animation_player_set_mode(&player_pingpong, CARBON_ANIM_PING_PONG);
    carbon_animation_player_play(&player_pingpong);

    /* Callback for one-shot animation */
    int completion_count = 0;
    carbon_animation_player_set_callback(&player_once, on_animation_complete, &completion_count);

    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        float dt = carbon_get_delta_time(engine);

        carbon_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            carbon_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                carbon_quit(engine);
            }
        }
        carbon_input_update(input);

        /* Space to trigger one-shot animation */
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
            carbon_animation_player_restart(&player_once);
            carbon_animation_player_play(&player_once);
        }

        /* R to restart all animations */
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_R)) {
            carbon_animation_player_restart(&player_loop);
            carbon_animation_player_restart(&player_once);
            carbon_animation_player_restart(&player_pingpong);
            carbon_animation_player_play(&player_loop);
            carbon_animation_player_play(&player_pingpong);
        }

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            carbon_quit(engine);

        /* Update animations */
        carbon_animation_player_update(&player_loop, dt);
        carbon_animation_player_update(&player_once, dt);
        carbon_animation_player_update(&player_pingpong, dt);

        carbon_camera_update(camera);

        /* Render */
        carbon_sprite_begin(sprites, NULL);

        /* Draw looping animation */
        carbon_animation_draw_scaled(sprites, &player_loop, 300.0f, 300.0f, 2.0f, 2.0f);

        /* Draw one-shot animation */
        carbon_animation_draw_scaled(sprites, &player_once, 640.0f, 300.0f, 2.0f, 2.0f);

        /* Draw ping-pong animation */
        carbon_animation_draw_scaled(sprites, &player_pingpong, 980.0f, 300.0f, 2.0f, 2.0f);

        /* Draw multiple instances with different speeds */
        carbon_animation_player_set_speed(&player_loop, 0.5f);
        carbon_animation_draw(sprites, &player_loop, 300.0f, 500.0f);
        carbon_animation_player_set_speed(&player_loop, 1.0f);
        carbon_animation_draw(sprites, &player_loop, 400.0f, 500.0f);
        carbon_animation_player_set_speed(&player_loop, 2.0f);
        carbon_animation_draw(sprites, &player_loop, 500.0f, 500.0f);
        carbon_animation_player_set_speed(&player_loop, 1.0f);

        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            carbon_sprite_upload(sprites, cmd);

            if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(engine);
                carbon_sprite_render(sprites, cmd, pass);
                carbon_end_render_pass(engine);
            }
        }

        carbon_sprite_end(sprites, NULL, NULL);
        carbon_end_frame(engine);
    }

    /* Cleanup */
    carbon_animation_destroy(anim_loop);
    carbon_animation_destroy(anim_once);
    carbon_animation_destroy(anim_pingpong);
    carbon_texture_destroy(sprites, sheet);
    carbon_input_shutdown(input);
    carbon_camera_destroy(camera);
    carbon_sprite_shutdown(sprites);
    carbon_shutdown(engine);

    return 0;
}
