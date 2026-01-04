/**
 * Agentite Engine - Collision Detection Example
 *
 * Demonstrates the collision detection system with various shape primitives,
 * collision queries, raycasting, and point containment tests.
 *
 * Controls:
 *   WASD - Move the player (circle) shape
 *   1-5  - Switch player shape (Circle, AABB, OBB, Capsule, Polygon)
 *   Q/E  - Rotate player shape (for OBB)
 *   Click - Cast ray from player to mouse position
 *   R    - Reset positions
 *   ESC  - Quit
 */

#include "agentite/agentite.h"
#include "agentite/sprite.h"
#include "agentite/collision.h"
#include "agentite/gizmos.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/text.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static const int WINDOW_WIDTH = 1280;
static const int WINDOW_HEIGHT = 720;

/* Collision layers */
#define LAYER_PLAYER    (1 << 0)
#define LAYER_OBSTACLE  (1 << 1)
#define LAYER_TRIGGER   (1 << 2)

/* Shape colors (RGBA packed as 0xRRGGBBAA) */
#define COLOR_PLAYER      0x4080FFFF  /* Blue */
#define COLOR_OBSTACLE    0xFF8040FF  /* Orange */
#define COLOR_TRIGGER     0x40FF80FF  /* Green */
#define COLOR_COLLISION   0xFF4040FF  /* Red */
#define COLOR_RAYCAST      0xFFFF40FF  /* Yellow - ray line */
#define COLOR_RAYCAST_HIT  0xFF4040FF  /* Red - hit marker */
#define COLOR_RAYCAST_MISS 0x40FF40FF  /* Green - no collision */
#define COLOR_POINT_HIT   0xFF00FFFF  /* Magenta */

/* Demo shapes */
typedef struct DemoShape {
    Agentite_CollisionShape *shape;
    Agentite_ColliderId collider;
    const char *name;
    uint32_t layer;
    bool is_trigger;
} DemoShape;

/* Application state */
typedef struct AppState {
    /* Core systems */
    Agentite_Engine *engine;
    Agentite_SpriteRenderer *sprites;
    Agentite_Gizmos *gizmos;
    Agentite_Camera *camera;
    Agentite_Input *input;
    Agentite_TextRenderer *text;
    Agentite_Font *font;

    /* Collision system */
    Agentite_CollisionWorld *collision_world;

    /* Player */
    Agentite_CollisionShape *player_shapes[6];
    Agentite_ColliderId player_collider;
    int current_player_shape;
    float player_x, player_y;
    float player_rotation;

    /* Obstacles */
    DemoShape obstacles[16];
    int obstacle_count;

    /* Visualization state */
    bool raycast_active;
    float ray_start_x, ray_start_y;
    float ray_end_x, ray_end_y;
    Agentite_RaycastHit ray_hit;
    bool ray_hit_something;

    /* Point query */
    Agentite_ColliderId point_hits[8];
    int point_hit_count;

    /* Warning message */
    float aabb_warning_timer;
} AppState;

/* Helper to convert float color to packed uint32 */
static void color_to_float4(uint32_t packed, float out[4]) {
    out[0] = ((packed >> 24) & 0xFF) / 255.0f;  /* R */
    out[1] = ((packed >> 16) & 0xFF) / 255.0f;  /* G */
    out[2] = ((packed >> 8) & 0xFF) / 255.0f;   /* B */
    out[3] = (packed & 0xFF) / 255.0f;          /* A */
}

/* Initialize collision shapes */
static bool init_shapes(AppState *app) {
    /* Create player shape variants */
    app->player_shapes[0] = agentite_collision_shape_circle(24.0f);
    app->player_shapes[1] = agentite_collision_shape_aabb(48.0f, 48.0f);
    app->player_shapes[2] = agentite_collision_shape_obb(48.0f, 48.0f);  /* Square OBB */
    app->player_shapes[3] = agentite_collision_shape_capsule(16.0f, 32.0f, AGENTITE_CAPSULE_Y);

    /* Hexagon for polygon shape */
    Agentite_CollisionVec2 hex_verts[6];
    for (int i = 0; i < 6; i++) {
        float angle = (float)i * (3.14159f * 2.0f / 6.0f) - 3.14159f / 2.0f;
        hex_verts[i].x = cosf(angle) * 28.0f;
        hex_verts[i].y = sinf(angle) * 28.0f;
    }
    app->player_shapes[4] = agentite_collision_shape_polygon(hex_verts, 6);
    app->player_shapes[5] = agentite_collision_shape_obb(60.0f, 30.0f);  /* Rectangle OBB */

    for (int i = 0; i < 6; i++) {
        if (!app->player_shapes[i]) {
            fprintf(stderr, "Failed to create player shape %d\n", i);
            return false;
        }
    }

    return true;
}

/* Create obstacle shapes in the world */
static void create_obstacles(AppState *app) {
    app->obstacle_count = 0;

    /* Central rotating box */
    DemoShape *obs = &app->obstacles[app->obstacle_count++];
    obs->shape = agentite_collision_shape_obb(80.0f, 40.0f);
    obs->collider = agentite_collision_add(app->collision_world, obs->shape,
        WINDOW_WIDTH / 2.0f, WINDOW_HEIGHT / 2.0f);
    obs->name = "OBB";
    obs->layer = LAYER_OBSTACLE;
    obs->is_trigger = false;
    agentite_collision_set_layer(app->collision_world, obs->collider, LAYER_OBSTACLE);
    agentite_collision_set_rotation(app->collision_world, obs->collider, 0.3f);

    /* Static boxes around the perimeter */
    struct { float x, y, w, h; } boxes[] = {
        {100, 150, 80, 60},
        {300, 100, 60, 80},
        {500, 180, 100, 40},
        {800, 120, 50, 50},
        {1000, 200, 70, 70},
        {150, 500, 60, 100},
        {400, 550, 120, 40},
        {700, 480, 80, 80},
        {1000, 550, 60, 60},
        {1150, 350, 50, 120},
    };

    for (size_t i = 0; i < sizeof(boxes) / sizeof(boxes[0]); i++) {
        obs = &app->obstacles[app->obstacle_count++];
        obs->shape = agentite_collision_shape_aabb(boxes[i].w, boxes[i].h);
        obs->collider = agentite_collision_add(app->collision_world, obs->shape,
            boxes[i].x, boxes[i].y);
        obs->name = "AABB";
        obs->layer = LAYER_OBSTACLE;
        obs->is_trigger = false;
        agentite_collision_set_layer(app->collision_world, obs->collider, LAYER_OBSTACLE);
    }

    /* Trigger circles */
    float trigger_positions[][2] = {
        {200, 300},
        {600, 400},
        {1000, 400},
    };

    for (size_t i = 0; i < sizeof(trigger_positions) / sizeof(trigger_positions[0]); i++) {
        obs = &app->obstacles[app->obstacle_count++];
        obs->shape = agentite_collision_shape_circle(40.0f);
        obs->collider = agentite_collision_add(app->collision_world, obs->shape,
            trigger_positions[i][0], trigger_positions[i][1]);
        obs->name = "Trigger";
        obs->layer = LAYER_TRIGGER;
        obs->is_trigger = true;
        agentite_collision_set_layer(app->collision_world, obs->collider, LAYER_TRIGGER);
    }

    /* Capsule obstacles */
    obs = &app->obstacles[app->obstacle_count++];
    obs->shape = agentite_collision_shape_capsule(20.0f, 60.0f, AGENTITE_CAPSULE_X);
    obs->collider = agentite_collision_add(app->collision_world, obs->shape, 850, 300);
    obs->name = "Capsule";
    obs->layer = LAYER_OBSTACLE;
    obs->is_trigger = false;
    agentite_collision_set_layer(app->collision_world, obs->collider, LAYER_OBSTACLE);
}

/* Initialize player collider */
static void init_player(AppState *app) {
    app->current_player_shape = 0;
    app->player_x = WINDOW_WIDTH / 2.0f;
    app->player_y = WINDOW_HEIGHT / 2.0f + 200;
    app->player_rotation = 0.0f;

    app->player_collider = agentite_collision_add(
        app->collision_world,
        app->player_shapes[app->current_player_shape],
        app->player_x, app->player_y
    );
    agentite_collision_set_layer(app->collision_world, app->player_collider, LAYER_PLAYER);
    agentite_collision_set_mask(app->collision_world, app->player_collider,
        LAYER_OBSTACLE | LAYER_TRIGGER);
}

/* Switch player shape */
static void switch_player_shape(AppState *app, int shape_index) {
    if (shape_index < 0 || shape_index >= 6) return;
    if (shape_index == app->current_player_shape) return;

    /* Remove old collider */
    agentite_collision_remove(app->collision_world, app->player_collider);

    /* Add new collider with same position */
    app->current_player_shape = shape_index;
    app->player_collider = agentite_collision_add(
        app->collision_world,
        app->player_shapes[app->current_player_shape],
        app->player_x, app->player_y
    );
    agentite_collision_set_layer(app->collision_world, app->player_collider, LAYER_PLAYER);
    agentite_collision_set_mask(app->collision_world, app->player_collider,
        LAYER_OBSTACLE | LAYER_TRIGGER);
    agentite_collision_set_rotation(app->collision_world, app->player_collider, app->player_rotation);
}

/* Draw all shapes using gizmos */
static void draw_collision_shapes(AppState *app) {
    float color[4];

    /* Draw obstacles */
    for (int i = 0; i < app->obstacle_count; i++) {
        DemoShape *obs = &app->obstacles[i];

        /* Check if player is colliding with this obstacle */
        Agentite_CollisionResult result;
        bool colliding = agentite_collision_test(
            app->collision_world,
            app->player_collider, obs->collider,
            &result
        );

        if (colliding) {
            color_to_float4(COLOR_COLLISION, color);
        } else if (obs->is_trigger) {
            color_to_float4(COLOR_TRIGGER, color);
        } else {
            color_to_float4(COLOR_OBSTACLE, color);
        }

        agentite_collision_debug_draw_collider(
            app->collision_world, obs->collider,
            app->gizmos, color
        );

        /* Draw collision normal if colliding - arrow shows push direction from player */
        if (colliding && result.is_colliding) {
            /* Negate normal to get push direction (away from obstacle) */
            vec3 from = {app->player_x, app->player_y, 0};
            vec3 to = {
                app->player_x - result.normal.x * 50.0f,
                app->player_y - result.normal.y * 50.0f,
                0
            };
            agentite_gizmos_arrow(app->gizmos, from, to, COLOR_RAYCAST);
        }
    }

    /* Draw player */
    color_to_float4(COLOR_PLAYER, color);
    agentite_collision_debug_draw_collider(
        app->collision_world, app->player_collider,
        app->gizmos, color
    );

    /* Draw raycast if active */
    if (app->raycast_active) {
        vec3 from = {app->ray_start_x, app->ray_start_y, 0};
        vec3 to = {app->ray_end_x, app->ray_end_y, 0};

        if (app->ray_hit_something) {
            /* Draw line to hit point */
            vec3 hit_point = {app->ray_hit.point.x, app->ray_hit.point.y, 0};
            agentite_gizmos_line(app->gizmos, from, hit_point, COLOR_RAYCAST);

            /* Draw hit point marker */
            vec3 size = {10, 10, 10};
            agentite_gizmos_box(app->gizmos, hit_point, size, COLOR_RAYCAST_HIT);

            /* Draw hit normal */
            vec3 normal_end = {
                app->ray_hit.point.x + app->ray_hit.normal.x * 30.0f,
                app->ray_hit.point.y + app->ray_hit.normal.y * 30.0f,
                0
            };
            agentite_gizmos_arrow(app->gizmos, hit_point, normal_end, COLOR_RAYCAST_HIT);
        } else {
            /* Draw full ray - green for miss */
            agentite_gizmos_line(app->gizmos, from, to, COLOR_RAYCAST_MISS);
        }
    }

    /* Draw point query hits */
    for (int i = 0; i < app->point_hit_count; i++) {
        color_to_float4(COLOR_POINT_HIT, color);
        agentite_collision_debug_draw_collider(
            app->collision_world, app->point_hits[i],
            app->gizmos, color
        );
    }
}

/* Get shape name */
static const char *get_shape_name(int index) {
    static const char *names[] = {"Circle", "AABB", "Square", "Capsule", "Polygon", "Rectangle"};
    if (index >= 0 && index < 6) return names[index];
    return "Unknown";
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    AppState app = {0};

    /* Initialize engine */
    Agentite_Config config = {
        .window_title = "Agentite - Collision Detection Example",
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
    app.camera = agentite_camera_create((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
    app.input = agentite_input_init();

    /* Initialize text */
    app.text = agentite_text_init(gpu, window);
    if (app.text) {
        app.font = agentite_font_load(app.text, "assets/fonts/Roboto-Regular.ttf", 18);
    }

    agentite_gizmos_set_screen_size(app.gizmos, WINDOW_WIDTH, WINDOW_HEIGHT);

    /* Create collision world */
    Agentite_CollisionWorldConfig world_config = AGENTITE_COLLISION_WORLD_DEFAULT;
    app.collision_world = agentite_collision_world_create(&world_config);
    if (!app.collision_world) {
        fprintf(stderr, "Failed to create collision world\n");
        agentite_shutdown(app.engine);
        return 1;
    }

    /* Initialize shapes */
    if (!init_shapes(&app)) {
        agentite_shutdown(app.engine);
        return 1;
    }

    create_obstacles(&app);
    init_player(&app);

    printf("Collision Detection Example\n");
    printf("===========================\n");
    printf("WASD - Move player\n");
    printf("1-6  - Switch shape (Circle, AABB, Square, Capsule, Polygon, Rectangle)\n");
    printf("Q/E  - Rotate (for Square/Polygon/Rectangle - not AABB!)\n");
    printf("Click - Raycast from player to mouse\n");
    printf("R    - Reset position\n");
    printf("ESC  - Quit\n");

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
            /* Raycast on mouse click */
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_LEFT) {
                app.raycast_active = true;
                app.ray_start_x = app.player_x;
                app.ray_start_y = app.player_y;
                app.ray_end_x = event.button.x;
                app.ray_end_y = event.button.y;

                /* Perform raycast */
                float dx = app.ray_end_x - app.ray_start_x;
                float dy = app.ray_end_y - app.ray_start_y;
                float len = sqrtf(dx * dx + dy * dy);

                app.ray_hit_something = agentite_collision_raycast(
                    app.collision_world,
                    app.ray_start_x, app.ray_start_y,
                    dx, dy,
                    len,
                    LAYER_OBSTACLE | LAYER_TRIGGER,
                    &app.ray_hit
                );
            }
        }
        agentite_input_update(app.input);

        /* Movement */
        float speed = 200.0f * dt;
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_W)) app.player_y -= speed;
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_S)) app.player_y += speed;
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_A)) app.player_x -= speed;
        if (agentite_input_key_pressed(app.input, SDL_SCANCODE_D)) app.player_x += speed;

        /* Rotation */
        float rot_speed = 2.0f * dt;
        bool trying_to_rotate = agentite_input_key_pressed(app.input, SDL_SCANCODE_Q) ||
                                agentite_input_key_pressed(app.input, SDL_SCANCODE_E);
        if (trying_to_rotate && app.current_player_shape == 1) {
            /* Show warning for AABB */
            app.aabb_warning_timer = 2.0f;
        } else {
            if (agentite_input_key_pressed(app.input, SDL_SCANCODE_Q)) app.player_rotation -= rot_speed;
            if (agentite_input_key_pressed(app.input, SDL_SCANCODE_E)) app.player_rotation += rot_speed;
        }

        /* Update warning timer */
        if (app.aabb_warning_timer > 0) {
            app.aabb_warning_timer -= dt;
        }

        /* Update player collider */
        agentite_collision_set_position(app.collision_world, app.player_collider,
            app.player_x, app.player_y);
        agentite_collision_set_rotation(app.collision_world, app.player_collider,
            app.player_rotation);

        /* Shape switching */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_1)) switch_player_shape(&app, 0);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_2)) switch_player_shape(&app, 1);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_3)) switch_player_shape(&app, 2);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_4)) switch_player_shape(&app, 3);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_5)) switch_player_shape(&app, 4);
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_6)) switch_player_shape(&app, 5);

        /* Reset */
        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_R)) {
            app.player_x = WINDOW_WIDTH / 2.0f;
            app.player_y = WINDOW_HEIGHT / 2.0f + 200;
            app.player_rotation = 0.0f;
            app.raycast_active = false;
        }

        if (agentite_input_key_just_pressed(app.input, SDL_SCANCODE_ESCAPE))
            agentite_quit(app.engine);

        /* Point query at mouse position */
        float mx, my;
        agentite_input_get_mouse_position(app.input, &mx, &my);
        app.point_hit_count = agentite_collision_query_point(
            app.collision_world, mx, my,
            AGENTITE_COLLISION_LAYER_ALL,
            app.point_hits, 8
        );

        /* Query collisions for player */
        Agentite_CollisionResult results[16];
        int collision_count = agentite_collision_query_collider(
            app.collision_world, app.player_collider,
            results, 16
        );

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(app.engine);
        if (cmd) {
            agentite_sprite_begin(app.sprites, NULL);
            agentite_sprite_upload(app.sprites, cmd);

            /* Build gizmos */
            agentite_gizmos_begin(app.gizmos, NULL);
            draw_collision_shapes(&app);
            agentite_gizmos_end(app.gizmos);
            agentite_gizmos_upload(app.gizmos, cmd);

            /* Build text */
            if (app.text && app.font) {
                agentite_text_begin(app.text);

                char info[256];
                snprintf(info, sizeof(info),
                    "Shape: %s (1-6 to switch)  Collisions: %d  Point hits: %d",
                    get_shape_name(app.current_player_shape),
                    collision_count,
                    app.point_hit_count);
                agentite_text_draw_colored(app.text, app.font,
                    info, 10, 10, 1.0f, 1.0f, 1.0f, 0.9f);

                agentite_text_draw_colored(app.text, app.font,
                    "WASD: Move  Q/E: Rotate  Click: Raycast  R: Reset  ESC: Quit",
                    10, 30, 0.7f, 0.7f, 0.7f, 0.9f);

                if (app.raycast_active && app.ray_hit_something) {
                    snprintf(info, sizeof(info), "Ray hit at distance: %.1f", app.ray_hit.distance);
                    agentite_text_draw_colored(app.text, app.font,
                        info, 10, 50, 1.0f, 1.0f, 0.3f, 1.0f);
                }

                /* Show AABB rotation warning */
                if (app.aabb_warning_timer > 0) {
                    agentite_text_draw_colored(app.text, app.font,
                        "AABB cannot rotate - it's Axis-Aligned! Use Square (3) or Rectangle (6) instead.",
                        10, 70, 1.0f, 0.4f, 0.4f, 1.0f);
                }

                /* Draw explanation at bottom of screen */
                agentite_text_draw_colored(app.text, app.font,
                    "Move the player shape with WASD. Collisions are detected against obstacles.",
                    10, WINDOW_HEIGHT - 50,
                    0.7f, 0.7f, 0.7f, 0.9f);
                agentite_text_draw_colored(app.text, app.font,
                    "Click to cast a ray. Green = no collision, Red/Yellow = collision detected.",
                    10, WINDOW_HEIGHT - 30,
                    0.7f, 0.7f, 0.7f, 0.9f);

                agentite_text_end(app.text);
                agentite_text_upload(app.text, cmd);
            }

            /* Render pass */
            if (agentite_begin_render_pass(app.engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
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
    for (int i = 0; i < app.obstacle_count; i++) {
        agentite_collision_shape_destroy(app.obstacles[i].shape);
    }
    for (int i = 0; i < 6; i++) {
        agentite_collision_shape_destroy(app.player_shapes[i]);
    }
    agentite_collision_world_destroy(app.collision_world);

    if (app.font) agentite_font_destroy(app.text, app.font);
    if (app.text) agentite_text_shutdown(app.text);
    agentite_camera_destroy(app.camera);
    agentite_gizmos_destroy(app.gizmos);
    agentite_input_shutdown(app.input);
    agentite_sprite_shutdown(app.sprites);
    agentite_shutdown(app.engine);

    return 0;
}
