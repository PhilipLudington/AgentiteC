/**
 * Carbon Engine - Tilemap Example
 *
 * Demonstrates chunk-based tilemap rendering with camera scrolling.
 */

#include "carbon/carbon.h"
#include "carbon/sprite.h"
#include "carbon/tilemap.h"
#include "carbon/camera.h"
#include "carbon/input.h"
#include <stdio.h>
#include <stdlib.h>

/* Create a procedural tileset texture (4x4 grid of tiles) */
static Carbon_Texture *create_tileset_texture(Carbon_SpriteRenderer *sr, int tile_size) {
    int cols = 4, rows = 4;
    int size = tile_size * cols;
    unsigned char *pixels = malloc(size * size * 4);
    if (!pixels) return NULL;

    /* Tile colors */
    unsigned char colors[16][3] = {
        {34, 139, 34},    /* 0: Forest green (grass) */
        {50, 205, 50},    /* 1: Light grass */
        {107, 142, 35},   /* 2: Dark grass */
        {144, 238, 144},  /* 3: Meadow */
        {64, 64, 64},     /* 4: Stone */
        {128, 128, 128},  /* 5: Cobblestone */
        {169, 169, 169},  /* 6: Gravel */
        {192, 192, 192},  /* 7: Marble */
        {139, 69, 19},    /* 8: Dirt */
        {160, 82, 45},    /* 9: Path */
        {210, 180, 140},  /* 10: Sand */
        {244, 164, 96},   /* 11: Desert */
        {65, 105, 225},   /* 12: Water */
        {30, 144, 255},   /* 13: Shallow water */
        {139, 0, 0},      /* 14: Lava */
        {255, 215, 0}     /* 15: Gold */
    };

    for (int ty = 0; ty < rows; ty++) {
        for (int tx = 0; tx < cols; tx++) {
            int tile_idx = ty * cols + tx;
            unsigned char r = colors[tile_idx][0];
            unsigned char g = colors[tile_idx][1];
            unsigned char b = colors[tile_idx][2];

            for (int py = 0; py < tile_size; py++) {
                for (int px = 0; px < tile_size; px++) {
                    int x = tx * tile_size + px;
                    int y = ty * tile_size + py;
                    int idx = (y * size + x) * 4;

                    /* Add subtle checkerboard pattern */
                    int noise = ((px ^ py) & 1) * 8;

                    pixels[idx + 0] = (r + noise > 255) ? 255 : r + noise;
                    pixels[idx + 1] = (g + noise > 255) ? 255 : g + noise;
                    pixels[idx + 2] = (b + noise > 255) ? 255 : b + noise;
                    pixels[idx + 3] = 255;
                }
            }
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
        .window_title = "Carbon - Tilemap Example",
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

    /* Create tileset */
    int tile_size = 32;
    Carbon_Texture *tileset_tex = create_tileset_texture(sprites, tile_size);
    Carbon_Tileset *tileset = carbon_tileset_create(tileset_tex, tile_size, tile_size);

    /* Create tilemap (100x100 tiles) */
    int map_width = 100;
    int map_height = 100;
    Carbon_Tilemap *tilemap = carbon_tilemap_create(tileset, map_width, map_height);

    /* Add layers */
    int ground_layer = carbon_tilemap_add_layer(tilemap, "ground");
    int decor_layer = carbon_tilemap_add_layer(tilemap, "decorations");

    /* Fill ground with grass */
    carbon_tilemap_fill(tilemap, ground_layer, 0, 0, map_width, map_height, 1);

    /* Add water lake */
    carbon_tilemap_fill(tilemap, ground_layer, 30, 30, 20, 15, 13);
    carbon_tilemap_fill(tilemap, ground_layer, 33, 33, 14, 9, 12);

    /* Sand beach around water */
    carbon_tilemap_fill(tilemap, ground_layer, 29, 29, 22, 1, 11);
    carbon_tilemap_fill(tilemap, ground_layer, 29, 45, 22, 1, 11);
    carbon_tilemap_fill(tilemap, ground_layer, 29, 29, 1, 17, 11);
    carbon_tilemap_fill(tilemap, ground_layer, 50, 29, 1, 17, 11);

    /* Stone path */
    carbon_tilemap_fill(tilemap, ground_layer, 48, 0, 3, 100, 6);

    /* Dirt patches */
    carbon_tilemap_fill(tilemap, ground_layer, 60, 40, 10, 10, 9);
    carbon_tilemap_fill(tilemap, ground_layer, 75, 70, 8, 8, 9);

    /* Forest areas (dark grass) */
    carbon_tilemap_fill(tilemap, ground_layer, 10, 60, 15, 15, 3);
    carbon_tilemap_fill(tilemap, ground_layer, 70, 10, 20, 20, 3);

    /* Add some decorations (gold markers) */
    carbon_tilemap_set_tile(tilemap, decor_layer, 50, 50, 16);
    carbon_tilemap_set_tile(tilemap, decor_layer, 25, 75, 16);
    carbon_tilemap_set_tile(tilemap, decor_layer, 80, 20, 16);

    /* Set decoration layer slightly transparent */
    carbon_tilemap_set_layer_opacity(tilemap, decor_layer, 0.9f);

    /* Center camera on map */
    float world_width = map_width * tile_size;
    float world_height = map_height * tile_size;
    carbon_camera_set_position(camera, world_width / 2, world_height / 2);

    float target_zoom = 1.0f;

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

        /* Camera movement */
        float cam_speed = 400.0f / carbon_camera_get_zoom(camera) * dt;
        if (carbon_input_key_pressed(input, SDL_SCANCODE_W))
            carbon_camera_move(camera, 0, -cam_speed);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_S))
            carbon_camera_move(camera, 0, cam_speed);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_A))
            carbon_camera_move(camera, -cam_speed, 0);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_D))
            carbon_camera_move(camera, cam_speed, 0);

        /* Zoom */
        float scroll_x, scroll_y;
        carbon_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y > 0) target_zoom *= 1.15f;
        if (scroll_y < 0) target_zoom /= 1.15f;
        if (target_zoom < 0.25f) target_zoom = 0.25f;
        if (target_zoom > 4.0f) target_zoom = 4.0f;

        /* Smooth zoom */
        float zoom = carbon_camera_get_zoom(camera);
        zoom += (target_zoom - zoom) * 5.0f * dt;
        carbon_camera_set_zoom(camera, zoom);

        /* Rotation */
        if (carbon_input_key_pressed(input, SDL_SCANCODE_Q)) {
            float rot = carbon_camera_get_rotation(camera);
            carbon_camera_set_rotation(camera, rot - 60.0f * dt);
        }
        if (carbon_input_key_pressed(input, SDL_SCANCODE_E)) {
            float rot = carbon_camera_get_rotation(camera);
            carbon_camera_set_rotation(camera, rot + 60.0f * dt);
        }

        /* Reset camera */
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_R)) {
            carbon_camera_set_position(camera, world_width / 2, world_height / 2);
            carbon_camera_set_rotation(camera, 0);
            target_zoom = 1.0f;
        }

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            carbon_quit(engine);

        carbon_camera_update(camera);

        /* Render */
        carbon_sprite_begin(sprites, NULL);

        /* Render tilemap (automatically frustum culled) */
        carbon_tilemap_render(tilemap, sprites, camera);

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
    carbon_tilemap_destroy(tilemap);
    carbon_tileset_destroy(tileset);
    carbon_texture_destroy(sprites, tileset_tex);
    carbon_input_shutdown(input);
    carbon_camera_destroy(camera);
    carbon_sprite_shutdown(sprites);
    carbon_shutdown(engine);

    return 0;
}
