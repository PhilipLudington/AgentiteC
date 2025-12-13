/**
 * Agentite Engine - Pathfinding Example
 *
 * Demonstrates the A* pathfinding system:
 * - Creating a walkability grid
 * - Finding paths between points
 * - Visualizing paths and grid state
 * - Dynamic obstacle placement
 *
 * Controls:
 *   Left-click:  Set destination (finds path from agent)
 *   Right-click: Toggle wall at cursor position
 *   WASD:        Pan camera
 *   Scroll:      Zoom camera
 *   R:           Reset grid (clear all walls)
 *   D:           Toggle diagonal movement
 *   Space:       Step agent along current path
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/pathfinding.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Grid and rendering constants */
#define GRID_WIDTH 32
#define GRID_HEIGHT 24
#define TILE_SIZE 24

/* Colors (RGBA 0-1) */
static const float COLOR_WALKABLE[4]  = {0.2f, 0.5f, 0.2f, 1.0f};  /* Dark green */
static const float COLOR_BLOCKED[4]   = {0.3f, 0.2f, 0.2f, 1.0f};  /* Dark red */
static const float COLOR_PATH[4]      = {0.2f, 0.4f, 0.8f, 1.0f};  /* Blue */
static const float COLOR_AGENT[4]     = {0.9f, 0.7f, 0.1f, 1.0f};  /* Gold */
static const float COLOR_GOAL[4]      = {0.1f, 0.9f, 0.3f, 1.0f};  /* Green */
static const float COLOR_GRID[4]      = {0.1f, 0.1f, 0.1f, 1.0f};  /* Dark */

/* Application state */
typedef struct {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_TextRenderer *text;
    Agentite_Font *font;
    Agentite_Camera *camera;
    Agentite_Input *input;
    Agentite_Pathfinder *pathfinder;

    /* Textures for rendering tiles */
    Agentite_Texture *white_tex;

    /* Agent state */
    int agent_x, agent_y;
    int goal_x, goal_y;
    bool has_goal;

    /* Current path */
    Agentite_Path *path;
    int path_index;

    /* Settings */
    bool allow_diagonal;
} AppState;

/* Create a 1x1 white pixel texture for solid color rendering */
static Agentite_Texture *create_white_texture(Agentite_SpriteRenderer *sr)
{
    unsigned char pixels[4] = {255, 255, 255, 255};
    return agentite_texture_create(sr, 1, 1, pixels);
}

/* Draw a filled rectangle */
static void draw_rect(AppState *app, float x, float y, float w, float h,
                      float r, float g, float b, float a)
{
    Agentite_Sprite sprite = agentite_sprite_from_texture(app->white_tex);
    agentite_sprite_draw_full(app->sprites, &sprite, x, y, w, h, 0, 0, 0, r, g, b, a);
}

/* Convert screen coordinates to grid coordinates */
static bool screen_to_grid(AppState *app, float screen_x, float screen_y, int *grid_x, int *grid_y)
{
    float world_x, world_y;
    agentite_camera_screen_to_world(app->camera, screen_x, screen_y, &world_x, &world_y);

    int gx = (int)(world_x / TILE_SIZE);
    int gy = (int)(world_y / TILE_SIZE);

    if (gx >= 0 && gx < GRID_WIDTH && gy >= 0 && gy < GRID_HEIGHT) {
        *grid_x = gx;
        *grid_y = gy;
        return true;
    }
    return false;
}

/* Find a new path from agent to goal */
static void find_path(AppState *app)
{
    /* Clear existing path */
    if (app->path) {
        agentite_path_destroy(app->path);
        app->path = NULL;
    }
    app->path_index = 0;

    if (!app->has_goal) return;

    /* Configure pathfinding options */
    Agentite_PathOptions options = AGENTITE_PATH_OPTIONS_DEFAULT;
    options.allow_diagonal = app->allow_diagonal;

    /* Find path */
    app->path = agentite_pathfinder_find_ex(app->pathfinder,
                                           app->agent_x, app->agent_y,
                                           app->goal_x, app->goal_y,
                                           &options);

    if (app->path) {
        printf("Path found: %d steps, cost %.2f\n", app->path->length, app->path->total_cost);
    } else {
        printf("No path found!\n");
    }
}

/* Move agent one step along the path */
static void step_agent(AppState *app)
{
    if (!app->path || app->path_index >= app->path->length - 1) return;

    app->path_index++;
    const Agentite_PathPoint *pt = agentite_path_get_point(app->path, app->path_index);
    if (pt) {
        app->agent_x = pt->x;
        app->agent_y = pt->y;
    }

    /* Check if reached goal */
    if (app->agent_x == app->goal_x && app->agent_y == app->goal_y) {
        printf("Agent reached goal!\n");
        app->has_goal = false;
        agentite_path_destroy(app->path);
        app->path = NULL;
    }
}

/* Handle input */
static void handle_input(AppState *app, float dt)
{
    float mouse_x, mouse_y;
    agentite_input_get_mouse_position(app->input, &mouse_x, &mouse_y);

    /* Camera pan with WASD */
    float pan_speed = 300.0f * dt;
    float cam_x, cam_y;
    agentite_camera_get_position(app->camera, &cam_x, &cam_y);

    if (agentite_input_key_pressed(app->input, SDL_SCANCODE_W)) cam_y -= pan_speed;
    if (agentite_input_key_pressed(app->input, SDL_SCANCODE_S)) cam_y += pan_speed;
    if (agentite_input_key_pressed(app->input, SDL_SCANCODE_A)) cam_x -= pan_speed;
    if (agentite_input_key_pressed(app->input, SDL_SCANCODE_D)) cam_x += pan_speed;
    agentite_camera_set_position(app->camera, cam_x, cam_y);

    /* Camera zoom with scroll */
    float scroll_x, scroll_y;
    agentite_input_get_scroll(app->input, &scroll_x, &scroll_y);
    if (scroll_y != 0) {
        float zoom = agentite_camera_get_zoom(app->camera);
        zoom *= (scroll_y > 0) ? 1.1f : 0.9f;
        if (zoom < 0.25f) zoom = 0.25f;
        if (zoom > 4.0f) zoom = 4.0f;
        agentite_camera_set_zoom(app->camera, zoom);
    }

    /* Left-click: set destination */
    if (agentite_input_mouse_button_pressed(app->input, 0)) {
        int gx, gy;
        if (screen_to_grid(app, mouse_x, mouse_y, &gx, &gy)) {
            if (agentite_pathfinder_is_walkable(app->pathfinder, gx, gy)) {
                app->goal_x = gx;
                app->goal_y = gy;
                app->has_goal = true;
                find_path(app);
            }
        }
    }

    /* Right-click: toggle wall */
    if (agentite_input_mouse_button_pressed(app->input, 2)) {
        int gx, gy;
        if (screen_to_grid(app, mouse_x, mouse_y, &gx, &gy)) {
            /* Don't block agent or goal */
            if (!((gx == app->agent_x && gy == app->agent_y) ||
                  (app->has_goal && gx == app->goal_x && gy == app->goal_y))) {
                bool walkable = agentite_pathfinder_is_walkable(app->pathfinder, gx, gy);
                agentite_pathfinder_set_walkable(app->pathfinder, gx, gy, !walkable);
                /* Recalculate path if it exists */
                if (app->has_goal) find_path(app);
            }
        }
    }

    /* R: Reset grid */
    if (agentite_input_key_just_pressed(app->input, SDL_SCANCODE_R)) {
        agentite_pathfinder_clear(app->pathfinder);
        app->has_goal = false;
        if (app->path) {
            agentite_path_destroy(app->path);
            app->path = NULL;
        }
        printf("Grid reset\n");
    }

    /* Toggle diagonal with number key 1 */
    if (agentite_input_key_just_pressed(app->input, SDL_SCANCODE_1)) {
        app->allow_diagonal = !app->allow_diagonal;
        printf("Diagonal movement: %s\n", app->allow_diagonal ? "enabled" : "disabled");
        if (app->has_goal) find_path(app);
    }

    /* Space: Step agent */
    if (agentite_input_key_just_pressed(app->input, SDL_SCANCODE_SPACE)) {
        step_agent(app);
    }
}

/* Render the grid */
static void render_grid(AppState *app)
{
    /* Draw tiles */
    for (int y = 0; y < GRID_HEIGHT; y++) {
        for (int x = 0; x < GRID_WIDTH; x++) {
            float px = x * TILE_SIZE + 1;
            float py = y * TILE_SIZE + 1;
            float size = TILE_SIZE - 2;

            bool walkable = agentite_pathfinder_is_walkable(app->pathfinder, x, y);
            const float *color = walkable ? COLOR_WALKABLE : COLOR_BLOCKED;

            draw_rect(app, px, py, size, size, color[0], color[1], color[2], color[3]);
        }
    }

    /* Draw path */
    if (app->path) {
        for (int i = 0; i < app->path->length; i++) {
            const Agentite_PathPoint *pt = agentite_path_get_point(app->path, i);
            if (!pt) continue;

            /* Highlight remaining path brighter */
            float brightness = (i >= app->path_index) ? 1.0f : 0.5f;
            float px = pt->x * TILE_SIZE + TILE_SIZE / 4;
            float py = pt->y * TILE_SIZE + TILE_SIZE / 4;
            float size = TILE_SIZE / 2;

            draw_rect(app, px, py, size, size,
                     COLOR_PATH[0] * brightness, COLOR_PATH[1] * brightness,
                     COLOR_PATH[2] * brightness, COLOR_PATH[3]);
        }
    }

    /* Draw goal */
    if (app->has_goal) {
        float px = app->goal_x * TILE_SIZE + 2;
        float py = app->goal_y * TILE_SIZE + 2;
        float size = TILE_SIZE - 4;
        draw_rect(app, px, py, size, size, COLOR_GOAL[0], COLOR_GOAL[1], COLOR_GOAL[2], COLOR_GOAL[3]);
    }

    /* Draw agent */
    float ax = app->agent_x * TILE_SIZE + 2;
    float ay = app->agent_y * TILE_SIZE + 2;
    float asize = TILE_SIZE - 4;
    draw_rect(app, ax, ay, asize, asize, COLOR_AGENT[0], COLOR_AGENT[1], COLOR_AGENT[2], COLOR_AGENT[3]);
}

/* Render HUD */
static void render_hud(AppState *app)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
             "Agent: (%d, %d)  |  Goal: %s  |  Diagonal: %s  |  Path: %d steps",
             app->agent_x, app->agent_y,
             app->has_goal ? "yes" : "none",
             app->allow_diagonal ? "ON" : "OFF",
             app->path ? app->path->length : 0);

    agentite_text_draw_colored(app->text, app->font, buf, 10, 10, 1.0f, 1.0f, 1.0f, 1.0f);

    agentite_text_draw_colored(app->text, app->font,
                              "LMB: Set goal | RMB: Toggle wall | WASD: Pan | Scroll: Zoom | 1: Toggle diagonal | Space: Step | R: Reset",
                              10, 30, 0.7f, 0.7f, 0.7f, 1.0f);
}

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.allow_diagonal = true;
    app.agent_x = 2;
    app.agent_y = 2;

    /* Initialize engine */
    Agentite_Config config = AGENTITE_DEFAULT_CONFIG;
    config.window_title = "Agentite - Pathfinding Example";
    config.window_width = 1024;
    config.window_height = 768;

    app.engine = agentite_init(&config);
    if (!app.engine) {
        fprintf(stderr, "Failed to initialize engine: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Get GPU device and window */
    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    /* Initialize subsystems */
    app.sprites = agentite_sprite_init(gpu, window);
    app.text = agentite_text_init(gpu, window);
    app.camera = agentite_camera_create(1024, 768);
    app.input = agentite_input_init();
    app.pathfinder = agentite_pathfinder_create(GRID_WIDTH, GRID_HEIGHT);

    if (!app.sprites || !app.text || !app.camera || !app.input || !app.pathfinder) {
        fprintf(stderr, "Failed to initialize subsystems\n");
        return 1;
    }

    /* Load font - try common locations */
    app.font = agentite_font_load(app.text, "assets/fonts/NotoSans-Regular.ttf", 16);
    if (!app.font) {
        app.font = agentite_font_load(app.text, "/System/Library/Fonts/Helvetica.ttc", 16);
    }
    if (!app.font) {
        fprintf(stderr, "Warning: Could not load font, HUD text will not display\n");
    }

    /* Create white texture for rectangles */
    app.white_tex = create_white_texture(app.sprites);
    if (!app.white_tex) {
        fprintf(stderr, "Failed to create texture\n");
        return 1;
    }

    /* Set camera to center on grid */
    agentite_camera_set_position(app.camera,
                                GRID_WIDTH * TILE_SIZE / 2.0f,
                                GRID_HEIGHT * TILE_SIZE / 2.0f);

    /* Add some initial obstacles for interest */
    for (int y = 8; y < 16; y++) {
        agentite_pathfinder_set_walkable(app.pathfinder, 10, y, false);
        agentite_pathfinder_set_walkable(app.pathfinder, 20, y, false);
    }
    for (int x = 10; x <= 20; x++) {
        agentite_pathfinder_set_walkable(app.pathfinder, x, 8, false);
    }

    printf("Pathfinding Example\n");
    printf("===================\n");
    printf("Left-click to set destination\n");
    printf("Right-click to toggle walls\n");
    printf("Space to step agent along path\n");
    printf("1 to toggle diagonal movement\n");
    printf("R to reset grid\n\n");

    /* Main loop */
    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);

        /* Process events */
        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(app.engine);
            }
            agentite_input_process_event(app.input, &event);
        }
        agentite_input_update(app.input);

        /* Handle input */
        handle_input(&app, dt);

        /* Update camera */
        agentite_camera_update(app.camera);
        agentite_sprite_set_camera(app.sprites, app.camera);

        /* Acquire command buffer */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);

        /* Begin sprite batch for grid (world space) */
        agentite_sprite_begin(app.sprites, cmd);
        render_grid(&app);
        agentite_sprite_upload(app.sprites, cmd);

        /* Begin text batch for HUD (screen space) */
        if (app.font) {
            agentite_text_begin(app.text);
            render_hud(&app);
            agentite_text_upload(app.text, cmd);
        }

        /* Render */
        if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
            agentite_sprite_render(app.sprites, cmd, agentite_get_render_pass(app.engine));
            if (app.font) {
                agentite_text_render(app.text, cmd, agentite_get_render_pass(app.engine));
            }
            agentite_end_render_pass(app.engine);
        }

        agentite_end_frame(app.engine);
    }

    /* Cleanup */
    if (app.path) agentite_path_destroy(app.path);
    agentite_pathfinder_destroy(app.pathfinder);
    agentite_texture_destroy(app.sprites, app.white_tex);
    if (app.font) agentite_font_destroy(app.text, app.font);
    agentite_input_shutdown(app.input);
    agentite_camera_destroy(app.camera);
    agentite_text_shutdown(app.text);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
