/**
 * Agentite Engine - Kinematic Physics Example
 *
 * Demonstrates the simple kinematic physics system with gravity, drag,
 * collision response (bounce, slide, stop), and trigger volumes.
 *
 * Controls:
 *   Click      - Spawn a bouncing ball at mouse position
 *   1          - Spawn ball with BOUNCE response
 *   2          - Spawn ball with SLIDE response
 *   3          - Spawn ball with STOP response
 *   Space      - Toggle gravity direction
 *   G          - Cycle gravity strength (off, low, normal, high)
 *   D          - Toggle drag on/off
 *   R          - Reset scene
 *   TAB        - Toggle debug visualization
 *   ESC        - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/physics.h"
#include "agentite/collision.h"
#include "agentite/gizmos.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* Collision layers */
#define LAYER_BALL      (1 << 0)
#define LAYER_WALL      (1 << 1)
#define LAYER_TRIGGER   (1 << 2)

/* Colors */
#define COLOR_BOUNCE    0x40FF80FF  /* Green */
#define COLOR_SLIDE     0x4080FFFF  /* Blue */
#define COLOR_STOP      0xFF8040FF  /* Orange */
#define COLOR_WALL      0x808080FF  /* Gray */
#define COLOR_TRIGGER   0xFFFF4080  /* Yellow-ish */
#define COLOR_VELOCITY  0xFFFF00FF  /* Yellow */

/* Maximum balls */
#define MAX_BALLS 64

typedef struct Ball {
    Agentite_PhysicsBody *body;
    Agentite_CollisionShape *shape;
    Agentite_CollisionResponse response;
    bool active;
    bool in_trigger;
    float flash_timer;  /* Flash when entering trigger */
} Ball;

typedef struct Wall {
    Agentite_PhysicsBody *body;
    Agentite_CollisionShape *shape;
    float x, y, w, h;
} Wall;

typedef struct Trigger {
    Agentite_PhysicsBody *body;
    Agentite_CollisionShape *shape;
    float x, y, radius;
    int count;  /* Objects inside */
} Trigger;

typedef struct AppState {
    /* Core systems */
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Gizmos *gizmos;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    /* Physics */
    Agentite_PhysicsWorld *physics;
    Agentite_CollisionWorld *collision;

    /* Game objects */
    Ball balls[MAX_BALLS];
    int ball_count;
    Wall walls[16];
    int wall_count;
    Trigger triggers[4];
    int trigger_count;

    /* Settings */
    int gravity_level;  /* 0=off, 1=low, 2=normal, 3=high */
    bool gravity_down;
    bool drag_enabled;
    bool show_debug;
    Agentite_CollisionResponse spawn_response;
} AppState;

/* Gravity levels */
static const float GRAVITY_LEVELS[] = {0.0f, 100.0f, 400.0f, 800.0f};
static const char *GRAVITY_NAMES[] = {"OFF", "LOW", "NORMAL", "HIGH"};

/* Trigger callback */
static void on_trigger(Agentite_PhysicsBody *trigger, Agentite_PhysicsBody *other,
                       bool is_enter, void *user_data) {
    AppState *app = (AppState *)user_data;

    /* Find the ball that triggered */
    for (int i = 0; i < app->ball_count; i++) {
        if (app->balls[i].body == other) {
            app->balls[i].in_trigger = is_enter;
            if (is_enter) {
                app->balls[i].flash_timer = 0.3f;
            }
            break;
        }
    }

    /* Find the trigger and update count */
    for (int i = 0; i < app->trigger_count; i++) {
        if (app->triggers[i].body == trigger) {
            app->triggers[i].count += is_enter ? 1 : -1;
            if (app->triggers[i].count < 0) app->triggers[i].count = 0;
            break;
        }
    }
}

/* Create a wall */
static void create_wall(AppState *app, float x, float y, float w, float h) {
    if (app->wall_count >= 16) return;

    Wall *wall = &app->walls[app->wall_count++];
    wall->x = x;
    wall->y = y;
    wall->w = w;
    wall->h = h;

    wall->shape = agentite_collision_shape_aabb(w, h);

    Agentite_PhysicsBodyConfig cfg = AGENTITE_PHYSICS_BODY_DEFAULT;
    cfg.type = AGENTITE_BODY_STATIC;
    wall->body = agentite_physics_body_create(app->physics, &cfg);
    agentite_physics_body_set_position(wall->body, x, y);
    agentite_physics_body_set_shape(wall->body, wall->shape);
    agentite_physics_body_set_layer(wall->body, LAYER_WALL);
    agentite_physics_body_set_mask(wall->body, LAYER_BALL);
}

/* Create a trigger zone */
static void create_trigger(AppState *app, float x, float y, float radius) {
    if (app->trigger_count >= 4) return;

    Trigger *trig = &app->triggers[app->trigger_count++];
    trig->x = x;
    trig->y = y;
    trig->radius = radius;
    trig->count = 0;

    trig->shape = agentite_collision_shape_circle(radius);

    Agentite_PhysicsBodyConfig cfg = AGENTITE_PHYSICS_BODY_DEFAULT;
    cfg.type = AGENTITE_BODY_STATIC;
    cfg.is_trigger = true;
    cfg.response = AGENTITE_RESPONSE_NONE;
    trig->body = agentite_physics_body_create(app->physics, &cfg);
    agentite_physics_body_set_position(trig->body, x, y);
    agentite_physics_body_set_shape(trig->body, trig->shape);
    agentite_physics_body_set_layer(trig->body, LAYER_TRIGGER);
    agentite_physics_body_set_mask(trig->body, LAYER_BALL);
}

/* Spawn a ball */
static void spawn_ball(AppState *app, float x, float y, Agentite_CollisionResponse response) {
    if (app->ball_count >= MAX_BALLS) return;

    Ball *ball = &app->balls[app->ball_count++];
    ball->active = true;
    ball->response = response;
    ball->in_trigger = false;
    ball->flash_timer = 0;

    ball->shape = agentite_collision_shape_circle(16.0f);

    Agentite_PhysicsBodyConfig cfg = AGENTITE_PHYSICS_BODY_DEFAULT;
    cfg.type = AGENTITE_BODY_DYNAMIC;
    cfg.mass = 1.0f;
    cfg.response = response;
    cfg.bounce = (response == AGENTITE_RESPONSE_BOUNCE) ? 0.8f : 0.0f;
    cfg.friction = 0.3f;
    cfg.drag = app->drag_enabled ? 0.02f : 0.0f;

    ball->body = agentite_physics_body_create(app->physics, &cfg);
    agentite_physics_body_set_position(ball->body, x, y);
    agentite_physics_body_set_shape(ball->body, ball->shape);
    agentite_physics_body_set_layer(ball->body, LAYER_BALL);
    agentite_physics_body_set_mask(ball->body, LAYER_WALL | LAYER_TRIGGER);

    /* Give initial random velocity */
    float angle = ((float)rand() / RAND_MAX) * 3.14159f * 2.0f;
    float speed = 100.0f + ((float)rand() / RAND_MAX) * 200.0f;
    agentite_physics_body_set_velocity(ball->body, cosf(angle) * speed, sinf(angle) * speed);
}

/* Initialize scene */
static void init_scene(AppState *app) {
    /* Create boundary walls */
    float margin = 20.0f;
    float wall_thickness = 20.0f;

    /* Top wall */
    create_wall(app, WINDOW_WIDTH / 2.0f, margin,
                WINDOW_WIDTH - 2 * margin, wall_thickness);
    /* Bottom wall */
    create_wall(app, WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT - margin,
                WINDOW_WIDTH - 2 * margin, wall_thickness);
    /* Left wall */
    create_wall(app, margin, WINDOW_HEIGHT / 2.0f,
                wall_thickness, WINDOW_HEIGHT - 2 * margin);
    /* Right wall */
    create_wall(app, WINDOW_WIDTH - margin, WINDOW_HEIGHT / 2.0f,
                wall_thickness, WINDOW_HEIGHT - 2 * margin);

    /* Interior obstacles */
    create_wall(app, 400, 300, 100, 20);
    create_wall(app, 800, 400, 20, 150);
    create_wall(app, 600, 550, 200, 20);
    create_wall(app, 200, 500, 80, 80);

    /* Trigger zones */
    create_trigger(app, 300, 200, 60);
    create_trigger(app, 900, 300, 80);
    create_trigger(app, 600, 400, 50);
}

/* Clear all balls */
static void clear_balls(AppState *app) {
    for (int i = 0; i < app->ball_count; i++) {
        if (app->balls[i].active) {
            agentite_physics_body_destroy(app->balls[i].body);
            agentite_collision_shape_destroy(app->balls[i].shape);
        }
    }
    app->ball_count = 0;

    /* Reset trigger counts */
    for (int i = 0; i < app->trigger_count; i++) {
        app->triggers[i].count = 0;
    }
}

/* Update gravity based on settings */
static void update_gravity(AppState *app) {
    float g = GRAVITY_LEVELS[app->gravity_level];
    if (app->gravity_down) {
        agentite_physics_set_gravity(app->physics, 0, g);
    } else {
        agentite_physics_set_gravity(app->physics, 0, -g);
    }
}

/* Get response name */
static const char *get_response_name(Agentite_CollisionResponse r) {
    switch (r) {
        case AGENTITE_RESPONSE_BOUNCE: return "BOUNCE";
        case AGENTITE_RESPONSE_SLIDE:  return "SLIDE";
        case AGENTITE_RESPONSE_STOP:   return "STOP";
        default: return "NONE";
    }
}

/* Get response color */
static uint32_t get_response_color(Agentite_CollisionResponse r) {
    switch (r) {
        case AGENTITE_RESPONSE_BOUNCE: return COLOR_BOUNCE;
        case AGENTITE_RESPONSE_SLIDE:  return COLOR_SLIDE;
        case AGENTITE_RESPONSE_STOP:   return COLOR_STOP;
        default: return 0xFFFFFFFF;
    }
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};
    app.gravity_level = 2;  /* Normal */
    app.gravity_down = true;
    app.drag_enabled = true;
    app.show_debug = true;
    app.spawn_response = AGENTITE_RESPONSE_BOUNCE;

    /* Initialize engine */
    Agentite_Config config = {
        .window_title = "Agentite - Kinematic Physics Example",
        .window_width = WINDOW_WIDTH,
        .window_height = WINDOW_HEIGHT,
        .vsync = true
    };

    app.engine = agentite_init(&config);
    if (!app.engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize graphics */
    SDL_GPUDevice *gpu = agentite_get_gpu_device(app.engine);
    SDL_Window *window = agentite_get_window(app.engine);

    app.sprites = agentite_sprite_init(gpu, window);
    app.gizmos = agentite_gizmos_create(gpu, NULL);
    app.input = agentite_input_init();

    app.text = agentite_text_init(gpu, window);
    if (app.text) {
        app.font = agentite_font_load(app.text, "assets/fonts/ProggyClean.ttf", 16);
    }

    agentite_gizmos_set_screen_size(app.gizmos, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Create collision world */
    Agentite_CollisionWorldConfig col_cfg = AGENTITE_COLLISION_WORLD_DEFAULT;
    app.collision = agentite_collision_world_create(&col_cfg);

    /* Create physics world */
    Agentite_PhysicsWorldConfig phys_cfg = AGENTITE_PHYSICS_WORLD_DEFAULT;
    phys_cfg.max_bodies = 256;
    app.physics = agentite_physics_world_create(&phys_cfg);
    agentite_physics_set_collision_world(app.physics, app.collision);
    agentite_physics_set_trigger_callback(app.physics, on_trigger, &app);

    update_gravity(&app);
    init_scene(&app);

    printf("Kinematic Physics Example\n");
    printf("=========================\n");
    printf("Click  - Spawn ball\n");
    printf("1/2/3  - Set response: Bounce/Slide/Stop\n");
    printf("Space  - Flip gravity\n");
    printf("G      - Cycle gravity strength\n");
    printf("D      - Toggle drag\n");
    printf("R      - Reset\n");
    printf("TAB    - Toggle debug\n");
    printf("ESC    - Quit\n");

    /* Main loop */
    while (agentite_is_running(app.engine)) {
        agentite_begin_frame(app.engine);
        float dt = agentite_get_delta_time(app.engine);

        /* Process input */
        agentite_input_begin_frame(app.input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(app.input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(app.engine);
            }
            /* Spawn ball on click */
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                spawn_ball(&app, event.button.x, event.button.y, app.spawn_response);
            }
        }
        agentite_input_update(app.input);

        /* Response selection */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1))
            app.spawn_response = AGENTITE_RESPONSE_BOUNCE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2))
            app.spawn_response = AGENTITE_RESPONSE_SLIDE;
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3))
            app.spawn_response = AGENTITE_RESPONSE_STOP;

        /* Gravity controls */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_SPACE)) {
            app.gravity_down = !app.gravity_down;
            update_gravity(&app);
        }
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_G)) {
            app.gravity_level = (app.gravity_level + 1) % 4;
            update_gravity(&app);
        }

        /* Drag toggle */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_D)) {
            app.drag_enabled = !app.drag_enabled;
            /* Update existing balls */
            for (int i = 0; i < app.ball_count; i++) {
                if (app.balls[i].active) {
                    agentite_physics_body_set_drag(app.balls[i].body,
                        app.drag_enabled ? 0.02f : 0.0f);
                }
            }
        }

        /* Reset */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_R)) {
            clear_balls(&app);
        }

        /* Debug toggle */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_TAB)) {
            app.show_debug = !app.show_debug;
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(app.engine);
        }

        /* Update flash timers */
        for (int i = 0; i < app.ball_count; i++) {
            if (app.balls[i].flash_timer > 0) {
                app.balls[i].flash_timer -= dt;
            }
        }

        /* Step physics */
        agentite_physics_world_step(app.physics, dt);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            agentite_sprite_begin(app.sprites, NULL);
            agentite_sprite_upload(app.sprites, cmd);

            /* Draw gizmos */
            agentite_gizmos_begin(app.gizmos, NULL);

            if (app.show_debug) {
                /* Draw walls */
                for (int i = 0; i < app.wall_count; i++) {
                    Wall *w = &app.walls[i];
                    vec3 center = {w->x, w->y, 0};
                    vec3 size = {w->w, w->h, 1};
                    agentite_gizmos_box(app.gizmos, center, size, COLOR_WALL);
                }

                /* Draw triggers */
                for (int i = 0; i < app.trigger_count; i++) {
                    Trigger *t = &app.triggers[i];
                    uint32_t col = (t->count > 0) ? 0x00FF00FF : COLOR_TRIGGER;
                    vec3 center = {t->x, t->y, 0};
                    vec3 normal = {0, 0, 1};
                    agentite_gizmos_circle(app.gizmos, center, normal, t->radius, col);
                }

                /* Draw balls */
                for (int i = 0; i < app.ball_count; i++) {
                    Ball *b = &app.balls[i];
                    if (!b->active) continue;

                    float px, py;
                    agentite_physics_body_get_position(b->body, &px, &py);

                    /* Ball color based on response, flash when in trigger */
                    uint32_t ball_color = get_response_color(b->response);
                    if (b->flash_timer > 0) {
                        ball_color = 0xFFFFFFFF;  /* White flash */
                    }

                    vec3 center = {px, py, 0};
                    vec3 normal = {0, 0, 1};
                    agentite_gizmos_circle(app.gizmos, center, normal, 16.0f, ball_color);

                    /* Draw velocity vector */
                    float vx, vy;
                    agentite_physics_body_get_velocity(b->body, &vx, &vy);
                    float speed = sqrtf(vx*vx + vy*vy);
                    if (speed > 10.0f) {
                        vec3 end = {px + vx * 0.2f, py + vy * 0.2f, 0};
                        agentite_gizmos_arrow(app.gizmos, center, end, COLOR_VELOCITY);
                    }
                }

                /* Draw gravity indicator */
                vec3 gravity_pos = {60, 100, 0};
                vec3 gravity_end = {
                    60,
                    app.gravity_down ? 140.0f : 60.0f,
                    0
                };
                if (app.gravity_level > 0) {
                    agentite_gizmos_arrow(app.gizmos, gravity_pos, gravity_end, 0xFFFF00FF);
                }
            }

            agentite_gizmos_end(app.gizmos);
            agentite_gizmos_upload(app.gizmos, cmd);

            /* Draw text */
            if (app.text && app.font) {
                agentite_text_begin(app.text);

                char info[256];
                snprintf(info, sizeof(info),
                    "Balls: %d  Spawn: %s  Gravity: %s %s  Drag: %s",
                    app.ball_count,
                    get_response_name(app.spawn_response),
                    GRAVITY_NAMES[app.gravity_level],
                    app.gravity_down ? "DOWN" : "UP",
                    app.drag_enabled ? "ON" : "OFF");
                agentite_text_draw_colored(app.text, app.font,
                    info, 10, 10, 1.0f, 1.0f, 1.0f, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "Click: Spawn  1/2/3: Response  Space: Flip  G: Gravity  D: Drag  R: Reset",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);

                /* Show trigger counts */
                for (int i = 0; i < app.trigger_count; i++) {
                    Trigger *t = &app.triggers[i];
                    if (t->count > 0) {
                        snprintf(info, sizeof(info), "%d", t->count);
                        agentite_text_draw_colored(app.text, app.font,
                            info, t->x - 5, t->y - 8, 0.0f, 1.0f, 0.0f, 1.0f);
                    }
                }

                agentite_text_upload(app.text, cmd);
            }

            /* Render pass */
            if (agentite_begin_render_pass(app.engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(app.engine);
                agentite_sprite_render(app.sprites, cmd, pass);
                agentite_gizmos_render(app.gizmos, cmd, pass);
                if (app.text) {
                    agentite_text_render(app.text, cmd, pass);
                }
                agentite_end_render_pass(app.engine);
            }

            agentite_sprite_end(app.sprites, NULL, NULL);
        }

        agentite_end_frame(app.engine);
    }

    /* Cleanup */
    clear_balls(&app);

    for (int i = 0; i < app.wall_count; i++) {
        agentite_physics_body_destroy(app.walls[i].body);
        agentite_collision_shape_destroy(app.walls[i].shape);
    }
    for (int i = 0; i < app.trigger_count; i++) {
        agentite_physics_body_destroy(app.triggers[i].body);
        agentite_collision_shape_destroy(app.triggers[i].shape);
    }

    agentite_physics_world_destroy(app.physics);
    agentite_collision_world_destroy(app.collision);

    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_gizmos_destroy(app.gizmos);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
