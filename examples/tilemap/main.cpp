/**
 * Agentite Engine - Tilemap Example
 *
 * Demonstrates chunk-based tilemap rendering with camera scrolling.
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/tilemap.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include <stdio.h>
#include <stdlib.h>

/* Create a procedural tileset texture (4x4 grid of tiles) */
static Agentite_Texture *create_tileset_texture(Agentite_SpriteRenderer *sr, int tile_size) {
    int cols = 4, rows = 4;
    int size = tile_size * cols;
    unsigned char *pixels = (unsigned char *)malloc(size * size * 4);
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

    Agentite_Texture *tex = agentite_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Tilemap Example",
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

    /* Create tileset */
    int tile_size = 32;
    Agentite_Texture *tileset_tex = create_tileset_texture(sprites, tile_size);
    Agentite_Tileset *tileset = agentite_tileset_create(tileset_tex, tile_size, tile_size);

    /* Create tilemap (100x100 tiles) */
    int map_width = 100;
    int map_height = 100;
    Agentite_Tilemap *tilemap = agentite_tilemap_create(tileset, map_width, map_height);

    /* Add layers */
    int ground_layer = agentite_tilemap_add_layer(tilemap, "ground");
    int decor_layer = agentite_tilemap_add_layer(tilemap, "decorations");

    /* Fill ground with grass */
    agentite_tilemap_fill(tilemap, ground_layer, 0, 0, map_width, map_height, 1);

    /* Add water lake */
    agentite_tilemap_fill(tilemap, ground_layer, 30, 30, 20, 15, 13);
    agentite_tilemap_fill(tilemap, ground_layer, 33, 33, 14, 9, 12);

    /* Sand beach around water */
    agentite_tilemap_fill(tilemap, ground_layer, 29, 29, 22, 1, 11);
    agentite_tilemap_fill(tilemap, ground_layer, 29, 45, 22, 1, 11);
    agentite_tilemap_fill(tilemap, ground_layer, 29, 29, 1, 17, 11);
    agentite_tilemap_fill(tilemap, ground_layer, 50, 29, 1, 17, 11);

    /* Stone path */
    agentite_tilemap_fill(tilemap, ground_layer, 48, 0, 3, 100, 6);

    /* Dirt patches */
    agentite_tilemap_fill(tilemap, ground_layer, 60, 40, 10, 10, 9);
    agentite_tilemap_fill(tilemap, ground_layer, 75, 70, 8, 8, 9);

    /* Forest areas (dark grass) */
    agentite_tilemap_fill(tilemap, ground_layer, 10, 60, 15, 15, 3);
    agentite_tilemap_fill(tilemap, ground_layer, 70, 10, 20, 20, 3);

    /* Add some decorations (gold markers) */
    agentite_tilemap_set_tile(tilemap, decor_layer, 50, 50, 16);
    agentite_tilemap_set_tile(tilemap, decor_layer, 25, 75, 16);
    agentite_tilemap_set_tile(tilemap, decor_layer, 80, 20, 16);

    /* Set decoration layer slightly transparent */
    agentite_tilemap_set_layer_opacity(tilemap, decor_layer, 0.9f);

    /* Center camera on map */
    float world_width = map_width * tile_size;
    float world_height = map_height * tile_size;
    agentite_camera_set_position(camera, world_width / 2, world_height / 2);

    float target_zoom = 1.0f;

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

        /* Camera movement */
        float cam_speed = 400.0f / agentite_camera_get_zoom(camera) * dt;
        if (agentite_input_key_pressed(input, SDL_SCANCODE_W))
            agentite_camera_move(camera, 0, -cam_speed);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_S))
            agentite_camera_move(camera, 0, cam_speed);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_A))
            agentite_camera_move(camera, -cam_speed, 0);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_D))
            agentite_camera_move(camera, cam_speed, 0);

        /* Zoom */
        float scroll_x, scroll_y;
        agentite_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y > 0) target_zoom *= 1.15f;
        if (scroll_y < 0) target_zoom /= 1.15f;
        if (target_zoom < 0.25f) target_zoom = 0.25f;
        if (target_zoom > 4.0f) target_zoom = 4.0f;

        /* Smooth zoom */
        float zoom = agentite_camera_get_zoom(camera);
        zoom += (target_zoom - zoom) * 5.0f * dt;
        agentite_camera_set_zoom(camera, zoom);

        /* Rotation */
        if (agentite_input_key_pressed(input, SDL_SCANCODE_Q)) {
            float rot = agentite_camera_get_rotation(camera);
            agentite_camera_set_rotation(camera, rot - 60.0f * dt);
        }
        if (agentite_input_key_pressed(input, SDL_SCANCODE_E)) {
            float rot = agentite_camera_get_rotation(camera);
            agentite_camera_set_rotation(camera, rot + 60.0f * dt);
        }

        /* Reset camera */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_R)) {
            agentite_camera_set_position(camera, world_width / 2, world_height / 2);
            agentite_camera_set_rotation(camera, 0);
            target_zoom = 1.0f;
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        agentite_camera_update(camera);

        /* Render */
        agentite_sprite_begin(sprites, NULL);

        /* Render tilemap (automatically frustum culled) */
        agentite_tilemap_render(tilemap, sprites, camera);

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
    agentite_tilemap_destroy(tilemap);
    agentite_tileset_destroy(tileset);
    agentite_texture_destroy(sprites, tileset_tex);
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_sprite_shutdown(sprites);
    agentite_shutdown(engine);

    return 0;
}
