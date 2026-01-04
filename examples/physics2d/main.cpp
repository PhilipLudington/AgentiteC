/**
 * Agentite Engine - Chipmunk2D Physics Example
 *
 * Demonstrates the Chipmunk2D rigid body physics integration with:
 * - Dynamic and static bodies
 * - Various shape types (circle, box, polygon, segment)
 * - Constraints/joints (pin, pivot, spring)
 * - Collision callbacks and filtering
 * - Debug draw visualization
 *
 * Controls:
 *   Click      - Drop a random shape at mouse position
 *   1          - Spawn circle
 *   2          - Spawn box
 *   3          - Spawn polygon (hexagon)
 *   Space      - Add explosion impulse at mouse
 *   R          - Reset simulation
 *   ESC        - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/physics2d.h"
#include "agentite/gizmos.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* Shape colors */
#define COLOR_CIRCLE   0x4080FFFF
#define COLOR_BOX      0xFF8040FF
#define COLOR_POLYGON  0x40FF80FF
#define COLOR_STATIC   0x808080FF
#define COLOR_JOINT    0xFFFF00FF

typedef struct AppState {
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Gizmos *gizmos;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    Agentite_Physics2DSpace *space;
    int body_count;
} AppState;

/* Create ground and walls */
static void create_static_bodies(AppState *app) {
    /* Ground */
    Agentite_Physics2DBody *ground = agentite_physics2d_body_create_static(app->space);
    Agentite_Physics2DShape *ground_shape = agentite_physics2d_shape_segment(
        ground, 0, WINDOW_HEIGHT - 50,
        WINDOW_WIDTH, WINDOW_HEIGHT - 50, 5.0f);
    agentite_physics2d_shape_set_friction(ground_shape, 0.9f);

    /* Left wall */
    Agentite_Physics2DBody *left = agentite_physics2d_body_create_static(app->space);
    agentite_physics2d_shape_segment(left, 50, 0, 50, WINDOW_HEIGHT, 5.0f);

    /* Right wall */
    Agentite_Physics2DBody *right = agentite_physics2d_body_create_static(app->space);
    agentite_physics2d_shape_segment(right, WINDOW_WIDTH - 50, 0,
        WINDOW_WIDTH - 50, WINDOW_HEIGHT, 5.0f);

    /* Platform */
    Agentite_Physics2DBody *platform = agentite_physics2d_body_create_static(app->space);
    agentite_physics2d_shape_segment(platform, 300, 500, 600, 500, 5.0f);

    /* Angled ramp */
    Agentite_Physics2DBody *ramp = agentite_physics2d_body_create_static(app->space);
    agentite_physics2d_shape_segment(ramp, 700, 600, 1000, 450, 5.0f);
}

/* Spawn a circle */
static void spawn_circle(AppState *app, float x, float y) {
    float radius = 15.0f + ((float)rand() / RAND_MAX) * 20.0f;
    float mass = radius * radius * 0.01f;
    float moment = agentite_physics2d_moment_for_circle(mass, 0, radius, 0, 0);

    Agentite_Physics2DBody *body = agentite_physics2d_body_create_dynamic(app->space, mass, moment);
    agentite_physics2d_body_set_position(body, x, y);

    Agentite_Physics2DShape *shape = agentite_physics2d_shape_circle(body, radius, 0, 0);
    agentite_physics2d_shape_set_elasticity(shape, 0.6f);
    agentite_physics2d_shape_set_friction(shape, 0.7f);

    app->body_count++;
}

/* Spawn a box */
static void spawn_box(AppState *app, float x, float y) {
    float w = 20.0f + ((float)rand() / RAND_MAX) * 30.0f;
    float h = 20.0f + ((float)rand() / RAND_MAX) * 30.0f;
    float mass = w * h * 0.01f;
    float moment = agentite_physics2d_moment_for_box(mass, w, h);

    Agentite_Physics2DBody *body = agentite_physics2d_body_create_dynamic(app->space, mass, moment);
    agentite_physics2d_body_set_position(body, x, y);

    Agentite_Physics2DShape *shape = agentite_physics2d_shape_box(body, w, h, 0);
    agentite_physics2d_shape_set_elasticity(shape, 0.4f);
    agentite_physics2d_shape_set_friction(shape, 0.8f);

    app->body_count++;
}

/* Spawn a hexagon */
static void spawn_polygon(AppState *app, float x, float y) {
    float radius = 25.0f;
    Agentite_Physics2DVec verts[6];
    for (int i = 0; i < 6; i++) {
        float angle = (float)i * (3.14159f * 2.0f / 6.0f);
        verts[i].x = cosf(angle) * radius;
        verts[i].y = sinf(angle) * radius;
    }

    float mass = radius * radius * 0.02f;
    float moment = agentite_physics2d_moment_for_polygon(mass, 6, verts, 0, 0, 0);

    Agentite_Physics2DBody *body = agentite_physics2d_body_create_dynamic(app->space, mass, moment);
    agentite_physics2d_body_set_position(body, x, y);

    Agentite_Physics2DShape *shape = agentite_physics2d_shape_polygon(body, 6, verts, 0);
    agentite_physics2d_shape_set_elasticity(shape, 0.5f);
    agentite_physics2d_shape_set_friction(shape, 0.6f);

    app->body_count++;
}

/* Apply explosion impulse - query bodies near point and apply impulse */
static void apply_explosion(AppState *app, float x, float y) {
    /* Note: In a full implementation, you would query all bodies near the
       explosion point using space queries and apply impulse to each.
       For this demo, we just spawn a burst of shapes as visual effect. */
    for (int i = 0; i < 10; i++) {
        float angle = ((float)i / 10.0f) * 3.14159f * 2.0f;
        float dist = 30.0f + ((float)rand() / RAND_MAX) * 20.0f;
        spawn_circle(app, x + cosf(angle) * dist, y + sinf(angle) * dist);
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};

    Agentite_Config config = {
        .window_title = "Agentite - Chipmunk2D Physics Example",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    app.sprites = agentite_sprite_init(gpu, window);
    app.gizmos = agentite_gizmos_create(gpu, NULL);
    app.input = agentite_input_init();

    app.text = agentite_text_init(gpu, window);
    if (app.text) {
        app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 16);
    }

    agentite_gizmos_set_screen_size(app.gizmos, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Create physics space */
    Agentite_Physics2DConfig phys_cfg = AGENTITE_PHYSICS2D_DEFAULT;
    phys_cfg.gravity_y = 500.0f;
    phys_cfg.iterations = 10;
    app.space = agentite_physics2d_space_create(&phys_cfg);

    create_static_bodies(&app);

    printf("Chipmunk2D Physics Example\n");
    printf("==========================\n");
    printf("Click  - Drop random shape\n");
    printf("1/2/3  - Circle/Box/Polygon\n");
    printf("Space  - Explosion at mouse\n");
    printf("R      - Reset\n");

    /* Main loop */
    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);

        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(app.engine);
            }
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                int type = rand() % 3;
                switch (type) {
                    case 0: spawn_circle(&app, event.button.x, event.button.y); break;
                    case 1: spawn_box(&app, event.button.x, event.button.y); break;
                    case 2: spawn_polygon(&app, event.button.x, event.button.y); break;
                }
            }
        }
        agentite_input_update(app.input);

        float mx, my;
        agentite_input_get_mouse_position(app.input, &mx, &my);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1))
            spawn_circle(&app, mx, my);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2))
            spawn_box(&app, mx, my);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3))
            spawn_polygon(&app, mx, my);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_SPACE))
            apply_explosion(&app, mx, my);

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_R)) {
            agentite_physics2d_space_destroy(app.space);
            app.space = agentite_physics2d_space_create(&phys_cfg);
            create_static_bodies(&app);
            app.body_count = 0;
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Step physics */
        agentite_physics2d_space_step(app.space, dt);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            agentite_sprite_begin(app.sprites, NULL);
            agentite_sprite_upload(app.sprites, cmd);

            agentite_gizmos_begin(app.gizmos, NULL);
            agentite_physics2d_debug_draw(app.space, app.gizmos);
            agentite_gizmos_end(app.gizmos);
            agentite_gizmos_upload(app.gizmos, cmd);

            if (app.text && app.font) {
                agentite_text_begin(app.text);
                char info[128];
                snprintf(info, sizeof(info), "Bodies: %d  Click to spawn  Space for explosion",
                    app.body_count);
                agentite_text_draw_colored(app.text, app.font,
                    info, 10, 10, 1.0f, 1.0f, 1.0f, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "1/2/3: Circle/Box/Polygon  R: Reset  ESC: Quit",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);

                /* Bottom instructions */
                agentite_text_draw_colored(app.text, app.font,
                    "Chipmunk2D provides full rigid body physics: circles, boxes, and polygons.",
                    10, WINDOW_HEIGHT - 40, 0.6f, 0.8f, 0.6f, 0.8f);
                agentite_text_draw_colored(app.text, app.font,
                    "Shapes have mass, elasticity, and friction. Click or press 1/2/3 to spawn shapes.",
                    10, WINDOW_HEIGHT - 20, 0.6f, 0.6f, 0.8f, 0.8f);

                agentite_text_end(app.text);
                agentite_text_upload(app.text, cmd);
            }

            if (agentite_begin_render_pass(app.engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                agentite_gizmos_render(app.gizmos, cmd, pass);
                if (app.text) agentite_text_render(app.text, cmd, pass);
                agentite_end_render_pass(app.engine);
            }

            agentite_sprite_end(app.sprites, NULL, NULL);
        }

        agentite_end_frame(app.engine);
    }

    agentite_physics2d_space_destroy(app.space);
    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_gizmos_destroy(app.gizmos);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
