/**
 * Agentite Engine - Animation Example
 *
 * Demonstrates sprite-based animation with the animation system.
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/animation.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include <stdio.h>
#include <stdlib.h>

/* Create a procedural sprite sheet (4 frames of a simple animation) */
static Agentite_Texture *create_animation_sheet(Agentite_SpriteRenderer *sr) {
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

    Agentite_Texture *tex = agentite_texture_create(sr, width, height, pixels);
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

    Agentite_Config config = {
        .window_title = "Carbon - Animation Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    Agentite_SpriteRenderer *sprites = agentite_sprite_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine)
    );

    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_sprite_set_camera(sprites, camera);

    Agentite_Input *input = agentite_input_init();

    /* Create animation sprite sheet */
    Agentite_Texture *sheet = create_animation_sheet(sprites);

    /* Create animations with different settings */
    Agentite_Animation *anim_loop = agentite_animation_from_strip(sheet, 0, 0, 64, 64, 4);
    agentite_animation_set_fps(anim_loop, 8.0f);

    Agentite_Animation *anim_once = agentite_animation_from_strip(sheet, 0, 0, 64, 64, 4);
    agentite_animation_set_fps(anim_once, 4.0f);

    Agentite_Animation *anim_pingpong = agentite_animation_from_strip(sheet, 0, 0, 64, 64, 4);
    agentite_animation_set_fps(anim_pingpong, 6.0f);

    /* Create animation players */
    Agentite_AnimationPlayer player_loop, player_once, player_pingpong;

    agentite_animation_player_init(&player_loop, anim_loop);
    agentite_animation_player_set_mode(&player_loop, AGENTITE_ANIM_LOOP);
    agentite_animation_player_play(&player_loop);

    agentite_animation_player_init(&player_once, anim_once);
    agentite_animation_player_set_mode(&player_once, AGENTITE_ANIM_ONCE);

    agentite_animation_player_init(&player_pingpong, anim_pingpong);
    agentite_animation_player_set_mode(&player_pingpong, AGENTITE_ANIM_PING_PONG);
    agentite_animation_player_play(&player_pingpong);

    /* Callback for one-shot animation */
    int completion_count = 0;
    agentite_animation_player_set_callback(&player_once, on_animation_complete, &completion_count);

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Space to trigger one-shot animation */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
            agentite_animation_player_restart(&player_once);
            agentite_animation_player_play(&player_once);
        }

        /* R to restart all animations */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_R)) {
            agentite_animation_player_restart(&player_loop);
            agentite_animation_player_restart(&player_once);
            agentite_animation_player_restart(&player_pingpong);
            agentite_animation_player_play(&player_loop);
            agentite_animation_player_play(&player_pingpong);
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        /* Update animations */
        agentite_animation_player_update(&player_loop, dt);
        agentite_animation_player_update(&player_once, dt);
        agentite_animation_player_update(&player_pingpong, dt);

        agentite_camera_update(camera);

        /* Render */
        agentite_sprite_begin(sprites, NULL);

        /* Draw looping animation */
        agentite_animation_draw_scaled(sprites, &player_loop, 300.0f, 300.0f, 2.0f, 2.0f);

        /* Draw one-shot animation */
        agentite_animation_draw_scaled(sprites, &player_once, 640.0f, 300.0f, 2.0f, 2.0f);

        /* Draw ping-pong animation */
        agentite_animation_draw_scaled(sprites, &player_pingpong, 980.0f, 300.0f, 2.0f, 2.0f);

        /* Draw multiple instances with different speeds */
        agentite_animation_player_set_speed(&player_loop, 0.5f);
        agentite_animation_draw(sprites, &player_loop, 300.0f, 500.0f);
        agentite_animation_player_set_speed(&player_loop, 1.0f);
        agentite_animation_draw(sprites, &player_loop, 400.0f, 500.0f);
        agentite_animation_player_set_speed(&player_loop, 2.0f);
        agentite_animation_draw(sprites, &player_loop, 500.0f, 500.0f);
        agentite_animation_player_set_speed(&player_loop, 1.0f);

        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            agentite_sprite_upload(sprites, cmd);

            if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_sprite_render(sprites, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_sprite_end(sprites, NULL, NULL);
        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_animation_destroy(anim_loop);
    agentite_animation_destroy(anim_once);
    agentite_animation_destroy(anim_pingpong);
    agentite_texture_destroy(sprites, sheet);
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_sprite_shutdown(sprites);
    agentite_shutdown(engine);

    return 0;
}
