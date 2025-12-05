/**
 * Carbon Engine - Strategy Game Example
 *
 * Demonstrates RTS-style patterns: unit selection, pathfinding, tilemap.
 */

#include "carbon/carbon.h"
#include "carbon/sprite.h"
#include "carbon/tilemap.h"
#include "carbon/camera.h"
#include "carbon/input.h"
#include "carbon/pathfinding.h"
#include "carbon/ui.h"
#include "carbon/ecs.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define MAX_UNITS 50
#define TILE_SIZE 32
#define MAP_WIDTH 40
#define MAP_HEIGHT 30

/* Unit structure */
typedef struct {
    float x, y;
    float target_x, target_y;
    bool selected;
    bool moving;
    Carbon_Path *path;
    int path_index;
} Unit;

/* Create simple unit texture */
static Carbon_Texture *create_unit_texture(Carbon_SpriteRenderer *sr) {
    int size = 24;
    unsigned char *pixels = malloc(size * size * 4);

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int idx = (y * size + x) * 4;
            float cx = x - size / 2.0f;
            float cy = y - size / 2.0f;
            float dist = sqrtf(cx * cx + cy * cy);

            if (dist < size / 2 - 2) {
                pixels[idx + 0] = 100;
                pixels[idx + 1] = 150;
                pixels[idx + 2] = 255;
                pixels[idx + 3] = 255;
            } else if (dist < size / 2) {
                pixels[idx + 0] = 50;
                pixels[idx + 1] = 80;
                pixels[idx + 2] = 150;
                pixels[idx + 3] = 255;
            } else {
                pixels[idx + 0] = 0;
                pixels[idx + 1] = 0;
                pixels[idx + 2] = 0;
                pixels[idx + 3] = 0;
            }
        }
    }

    Carbon_Texture *tex = carbon_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Create tileset */
static Carbon_Texture *create_tileset(Carbon_SpriteRenderer *sr) {
    int tile_size = TILE_SIZE;
    int size = tile_size * 4;
    unsigned char *pixels = malloc(size * size * 4);

    unsigned char colors[16][3] = {
        {34, 139, 34},    /* 0: Grass */
        {50, 205, 50},    /* 1: Light grass */
        {64, 64, 64},     /* 2: Stone (blocked) */
        {128, 128, 128},  /* 3: Road */
        {139, 69, 19},    /* 4: Dirt */
        {65, 105, 225},   /* 5: Water (blocked) */
        {34, 100, 34},    /* 6: Forest (slow) */
        {210, 180, 140},  /* 7: Sand */
        {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0},
        {0, 0, 0}, {0, 0, 0}, {0, 0, 0}, {0, 0, 0}
    };

    for (int ty = 0; ty < 4; ty++) {
        for (int tx = 0; tx < 4; tx++) {
            int idx = ty * 4 + tx;
            for (int py = 0; py < tile_size; py++) {
                for (int px = 0; px < tile_size; px++) {
                    int x = tx * tile_size + px;
                    int y = ty * tile_size + py;
                    int i = (y * size + x) * 4;
                    int noise = ((px ^ py) & 1) * 8;
                    pixels[i + 0] = colors[idx][0] + noise;
                    pixels[i + 1] = colors[idx][1] + noise;
                    pixels[i + 2] = colors[idx][2] + noise;
                    pixels[i + 3] = 255;
                }
            }
        }
    }

    Carbon_Texture *tex = carbon_texture_create(sr, size, size, pixels);
    free(pixels);
    return tex;
}

/* Check if point is in rectangle */
static bool point_in_rect(float px, float py, float x, float y, float w, float h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Carbon_Config config = {
        .window_title = "Carbon - Strategy Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) return 1;

    Carbon_SpriteRenderer *sprites = carbon_sprite_init(
        carbon_get_gpu_device(engine), carbon_get_window(engine));
    Carbon_Camera *camera = carbon_camera_create(1280.0f, 720.0f);
    carbon_sprite_set_camera(sprites, camera);
    Carbon_Input *input = carbon_input_init();

    CUI_Context *ui = cui_init(
        carbon_get_gpu_device(engine), carbon_get_window(engine),
        1280, 720, "assets/fonts/Roboto-Regular.ttf", 14.0f);

    /* Create textures */
    Carbon_Texture *unit_tex = create_unit_texture(sprites);
    Carbon_Sprite unit_sprite = carbon_sprite_from_texture(unit_tex);

    Carbon_Texture *tileset_tex = create_tileset(sprites);
    Carbon_Tileset *tileset = carbon_tileset_create(tileset_tex, TILE_SIZE, TILE_SIZE);
    Carbon_Tilemap *tilemap = carbon_tilemap_create(tileset, MAP_WIDTH, MAP_HEIGHT);

    int ground_layer = carbon_tilemap_add_layer(tilemap, "ground");

    /* Fill with grass */
    carbon_tilemap_fill(tilemap, ground_layer, 0, 0, MAP_WIDTH, MAP_HEIGHT, 1);

    /* Add obstacles */
    carbon_tilemap_fill(tilemap, ground_layer, 10, 5, 5, 10, 3);   /* Stone wall */
    carbon_tilemap_fill(tilemap, ground_layer, 25, 10, 8, 8, 6);   /* Water */
    carbon_tilemap_fill(tilemap, ground_layer, 5, 20, 10, 3, 3);   /* Another wall */

    /* Road */
    carbon_tilemap_fill(tilemap, ground_layer, 0, 14, 40, 2, 4);

    /* Create pathfinder */
    Carbon_Pathfinder *pathfinder = carbon_pathfinder_create(MAP_WIDTH, MAP_HEIGHT);

    /* Set blocked tiles (stone = 3, water = 6) */
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            Carbon_TileID tile = carbon_tilemap_get_tile(tilemap, ground_layer, x, y);
            if (tile == 3 || tile == 6) {
                carbon_pathfinder_set_walkable(pathfinder, x, y, false);
            }
        }
    }

    /* Create units */
    Unit units[MAX_UNITS] = {0};
    int num_units = 5;
    for (int i = 0; i < num_units; i++) {
        units[i].x = 100.0f + i * 40.0f;
        units[i].y = 400.0f;
        units[i].target_x = units[i].x;
        units[i].target_y = units[i].y;
    }

    /* Center camera */
    float world_w = MAP_WIDTH * TILE_SIZE;
    float world_h = MAP_HEIGHT * TILE_SIZE;
    carbon_camera_set_position(camera, world_w / 2, world_h / 2);

    /* Selection box */
    bool selecting = false;
    float sel_start_x, sel_start_y;
    float sel_end_x, sel_end_y;

    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        float dt = carbon_get_delta_time(engine);

        carbon_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (ui && cui_process_event(ui, &event)) continue;
            carbon_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) carbon_quit(engine);
        }
        carbon_input_update(input);

        /* Camera controls */
        float cam_speed = 300.0f * dt;
        if (carbon_input_key_pressed(input, SDL_SCANCODE_W))
            carbon_camera_move(camera, 0, -cam_speed);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_S))
            carbon_camera_move(camera, 0, cam_speed);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_A))
            carbon_camera_move(camera, -cam_speed, 0);
        if (carbon_input_key_pressed(input, SDL_SCANCODE_D))
            carbon_camera_move(camera, cam_speed, 0);

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            carbon_quit(engine);

        carbon_camera_update(camera);

        /* Get mouse world position */
        float mouse_x, mouse_y;
        carbon_input_get_mouse_position(input, &mouse_x, &mouse_y);
        float world_x, world_y;
        carbon_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

        /* Selection box (left click + drag) */
        if (carbon_input_mouse_button_just_pressed(input, 0)) {
            selecting = true;
            sel_start_x = world_x;
            sel_start_y = world_y;
            sel_end_x = world_x;
            sel_end_y = world_y;
        }
        if (selecting && carbon_input_mouse_button(input, 0)) {
            sel_end_x = world_x;
            sel_end_y = world_y;
        }
        if (selecting && carbon_input_mouse_button_just_released(input, 0)) {
            selecting = false;

            /* Select units in box */
            float x1 = fminf(sel_start_x, sel_end_x);
            float y1 = fminf(sel_start_y, sel_end_y);
            float x2 = fmaxf(sel_start_x, sel_end_x);
            float y2 = fmaxf(sel_start_y, sel_end_y);

            for (int i = 0; i < num_units; i++) {
                units[i].selected = (units[i].x >= x1 && units[i].x <= x2 &&
                                    units[i].y >= y1 && units[i].y <= y2);
            }
        }

        /* Right click: move selected units */
        if (carbon_input_mouse_button_just_pressed(input, 2)) {
            int tile_x = (int)(world_x / TILE_SIZE);
            int tile_y = (int)(world_y / TILE_SIZE);

            for (int i = 0; i < num_units; i++) {
                if (!units[i].selected) continue;

                int unit_tile_x = (int)(units[i].x / TILE_SIZE);
                int unit_tile_y = (int)(units[i].y / TILE_SIZE);

                /* Find path */
                if (units[i].path) {
                    carbon_path_destroy(units[i].path);
                    units[i].path = NULL;
                }

                units[i].path = carbon_pathfinder_find(pathfinder,
                    unit_tile_x, unit_tile_y, tile_x, tile_y);

                if (units[i].path && units[i].path->length > 0) {
                    units[i].path_index = 0;
                    units[i].moving = true;
                } else {
                    units[i].moving = false;
                }
            }
        }

        /* Update units */
        float move_speed = 100.0f;
        for (int i = 0; i < num_units; i++) {
            if (!units[i].moving || !units[i].path) continue;

            if (units[i].path_index >= units[i].path->length) {
                units[i].moving = false;
                continue;
            }

            float target_x = units[i].path->points[units[i].path_index].x * TILE_SIZE + TILE_SIZE / 2;
            float target_y = units[i].path->points[units[i].path_index].y * TILE_SIZE + TILE_SIZE / 2;

            float dx = target_x - units[i].x;
            float dy = target_y - units[i].y;
            float dist = sqrtf(dx * dx + dy * dy);

            if (dist < 5.0f) {
                units[i].path_index++;
            } else {
                units[i].x += (dx / dist) * move_speed * dt;
                units[i].y += (dy / dist) * move_speed * dt;
            }
        }

        /* Render */
        carbon_sprite_begin(sprites, NULL);

        /* Draw tilemap */
        carbon_tilemap_render(tilemap, sprites, camera);

        /* Draw units */
        for (int i = 0; i < num_units; i++) {
            if (units[i].selected) {
                carbon_sprite_draw_tinted(sprites, &unit_sprite,
                    units[i].x - 12, units[i].y - 12,
                    0.5f, 1.0f, 0.5f, 1.0f);
            } else {
                carbon_sprite_draw(sprites, &unit_sprite,
                    units[i].x - 12, units[i].y - 12);
            }
        }

        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            carbon_sprite_upload(sprites, cmd);

            if (ui) {
                cui_begin_frame(ui, dt);

                /* Info panel */
                if (cui_begin_panel(ui, "Info", 10, 10, 200, 100, CUI_PANEL_BORDER)) {
                    char buf[64];
                    int selected_count = 0;
                    for (int i = 0; i < num_units; i++)
                        if (units[i].selected) selected_count++;

                    snprintf(buf, sizeof(buf), "Units: %d", num_units);
                    cui_label(ui, buf);
                    snprintf(buf, sizeof(buf), "Selected: %d", selected_count);
                    cui_label(ui, buf);
                    snprintf(buf, sizeof(buf), "Tile: %d, %d",
                        (int)(world_x / TILE_SIZE), (int)(world_y / TILE_SIZE));
                    cui_label(ui, buf);
                    cui_end_panel(ui);
                }

                cui_end_frame(ui);
                cui_upload(ui, cmd);
            }

            if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(engine);
                carbon_sprite_render(sprites, cmd, pass);
                if (ui) cui_render(ui, cmd, pass);
                carbon_end_render_pass(engine);
            }
        }

        carbon_sprite_end(sprites, NULL, NULL);
        carbon_end_frame(engine);
    }

    /* Cleanup */
    for (int i = 0; i < num_units; i++) {
        if (units[i].path) carbon_path_destroy(units[i].path);
    }
    carbon_pathfinder_destroy(pathfinder);
    carbon_tilemap_destroy(tilemap);
    carbon_tileset_destroy(tileset);
    carbon_texture_destroy(sprites, tileset_tex);
    carbon_texture_destroy(sprites, unit_tex);
    carbon_input_shutdown(input);
    if (ui) cui_shutdown(ui);
    carbon_camera_destroy(camera);
    carbon_sprite_shutdown(sprites);
    carbon_shutdown(engine);

    return 0;
}
