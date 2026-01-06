/**
 * Agentite Engine - Debug Tools Example
 *
 * Demonstrates the enhanced debug visualization system:
 * - Entity gizmo overlays (position markers, velocity arrows)
 * - Collision shape visualization
 * - AI path visualization
 * - Spatial grid overlay
 * - Fog of war debug view
 * - Turn/phase state inspector
 * - Console command system
 */

#include "agentite/agentite.h"
#include "agentite/debug.h"
#include "agentite/gizmos.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include "agentite/ecs.h"
#include "agentite/collision.h"
#include "agentite/pathfinding.h"
#include "agentite/fog.h"
#include "agentite/turn.h"
#include "agentite/spatial.h"
#include "agentite/ui.h"
#include <stdio.h>
#include <math.h>

/* Custom command example */
static void cmd_spawn(Agentite_DebugSystem *debug, int argc, const char **argv, void *userdata) {
    (void)userdata;
    if (argc < 3) {
        agentite_debug_print(debug, "Usage: spawn <x> <y>");
        return;
    }
    float x = (float)atof(argv[1]);
    float y = (float)atof(argv[2]);
    agentite_debug_print(debug, "Spawning entity at (%.1f, %.1f)", x, y);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Debug Tools Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Initialize subsystems */
    Agentite_GizmoConfig gizmo_config = AGENTITE_GIZMO_CONFIG_DEFAULT;
    Agentite_Gizmos *gizmos = agentite_gizmos_create(
        agentite_get_gpu_device(engine), &gizmo_config);
    if (!gizmos) {
        fprintf(stderr, "Failed to create gizmos: %s\n", agentite_get_last_error());
        agentite_shutdown(engine);
        return 1;
    }

    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_camera_set_position(camera, 640.0f, 360.0f);

    Agentite_Input *input = agentite_input_init();

    /* Initialize ECS world with some entities */
    Agentite_World *world = agentite_ecs_init();
    agentite_ecs_register_components(world);

    /* Create some test entities with positions and velocities */
    ecs_world_t *ecs = agentite_ecs_get_world(world);
    for (int i = 0; i < 10; i++) {
        ecs_entity_t e = ecs_new(ecs);
        float x = 200.0f + (float)(i % 5) * 150.0f;
        float y = 200.0f + (float)(i / 5) * 200.0f;
        C_Position pos = { x, y };
        ecs_set_id(ecs, e, ecs_id(C_Position), sizeof(C_Position), &pos);
        /* Some entities have velocity */
        if (i % 2 == 0) {
            float vx = 50.0f * cosf((float)i * 0.7f);
            float vy = 50.0f * sinf((float)i * 0.7f);
            C_Velocity vel = { vx, vy };
            ecs_set_id(ecs, e, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
        }
    }

    /* Initialize collision world with some shapes */
    Agentite_CollisionWorldConfig coll_config = AGENTITE_COLLISION_WORLD_DEFAULT;
    Agentite_CollisionWorld *collision = agentite_collision_world_create(&coll_config);

    /* Add some collision shapes (positioned to avoid entity overlap) */
    Agentite_CollisionShape *circle = agentite_collision_shape_circle(30.0f);
    Agentite_CollisionShape *box = agentite_collision_shape_aabb(50.0f, 30.0f);

    agentite_collision_add(collision, circle, 280.0f, 300.0f);
    agentite_collision_add(collision, box, 430.0f, 300.0f);
    agentite_collision_add(collision, circle, 580.0f, 300.0f);
    agentite_collision_add(collision, box, 730.0f, 300.0f);

    /* Initialize pathfinder and create a sample path */
    Agentite_Pathfinder *pathfinder = agentite_pathfinder_create(40, 22);
    /* Mark some obstacles */
    for (int x = 22; x < 27; x++) {
        for (int y = 8; y < 14; y++) {
            agentite_pathfinder_set_walkable(pathfinder, x, y, false);
        }
    }
    /* Path on the right side to avoid overlapping help text */
    Agentite_Path *path = agentite_pathfinder_find(pathfinder, 18, 12, 35, 6);

    /* Initialize fog of war */
    Agentite_FogOfWar *fog = agentite_fog_create(40, 22);
    /* Add a vision source */
    agentite_fog_add_source(fog, 20, 11, 8);
    agentite_fog_update(fog);

    /* Initialize turn manager */
    Agentite_TurnManager turn_mgr;
    agentite_turn_init(&turn_mgr);

    /* Initialize spatial index with some entities */
    Agentite_SpatialIndex *spatial = agentite_spatial_create(256);
    agentite_spatial_add(spatial, 5, 5, 1);
    agentite_spatial_add(spatial, 6, 5, 2);
    agentite_spatial_add(spatial, 10, 10, 3);
    agentite_spatial_add(spatial, 15, 8, 4);

    /* Initialize debug system */
    Agentite_DebugConfig debug_config = AGENTITE_DEBUG_CONFIG_DEFAULT;
    debug_config.spatial_cell_size = 32.0f;
    debug_config.fog_tile_width = 32.0f;
    debug_config.fog_tile_height = 32.0f;

    Agentite_DebugSystem *debug = agentite_debug_create(&debug_config);
    if (!debug) {
        fprintf(stderr, "Failed to create debug system: %s\n", agentite_get_last_error());
        agentite_shutdown(engine);
        return 1;
    }

    /* Bind systems for visualization */
    agentite_debug_bind_ecs(debug, world);
    agentite_debug_bind_collision(debug, collision);
    agentite_debug_bind_pathfinder(debug, pathfinder);
    agentite_debug_bind_fog(debug, fog);
    agentite_debug_bind_turn(debug, &turn_mgr);
    agentite_debug_bind_spatial(debug, spatial);

    /* Register custom command */
    agentite_debug_register_command(debug, "spawn", "Spawn entity: spawn <x> <y>", cmd_spawn, NULL);

    /* Create entity that will follow the path */
    ecs_entity_t path_follower = 0;
    int current_waypoint = 0;
    uint32_t path_viz_id = 0;
    float tile_size = 32.0f;

    if (path && path->length > 0) {
        /* Add path visualization */
        path_viz_id = agentite_debug_add_path(debug, path, 0);

        /* Create entity at path start */
        path_follower = ecs_new(ecs);
        float start_x = (float)path->points[0].x * tile_size + tile_size * 0.5f;
        float start_y = (float)path->points[0].y * tile_size + tile_size * 0.5f;
        C_Position pos = { start_x, start_y };
        ecs_set_id(ecs, path_follower, ecs_id(C_Position), sizeof(C_Position), &pos);
        /* Give it a velocity to show direction */
        C_Velocity vel = { 0, 0 };
        ecs_set_id(ecs, path_follower, ecs_id(C_Velocity), sizeof(C_Velocity), &vel);
    }

    /* Enable all debug visualizations by default */
    agentite_debug_set_flags(debug, AGENTITE_DEBUG_ALL);

    /* Initialize UI for console */
    AUI_Context *ui = aui_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine),
        1280, 720,
        "assets/fonts/Roboto-Regular.ttf",
        16.0f
    );

    printf("\n=== Debug Tools Example ===\n");
    printf("Controls:\n");
    printf("  F1     - Toggle entity gizmos\n");
    printf("  F2     - Toggle collision shapes\n");
    printf("  F3     - Toggle AI paths\n");
    printf("  F4     - Toggle spatial grid\n");
    printf("  F5     - Toggle fog of war debug\n");
    printf("  F6     - Toggle turn state\n");
    printf("  F7     - Toggle performance overlay\n");
    printf("  `      - Toggle debug console\n");
    printf("  SPACE  - Advance turn phase\n");
    printf("  WASD   - Pan camera\n");
    printf("  Scroll - Zoom\n");
    printf("  ESC    - Quit\n");
    printf("\nConsole commands: help, debug <flag>, clear, fps, flags, bind\n\n");

    float time = 0.0f;

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);
        time += dt;

        /* Process input */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Console gets first crack at events */
            if (agentite_debug_console_is_open(debug)) {
                if (agentite_debug_console_event(debug, &event)) {
                    continue;  /* Console consumed the event */
                }
            }

            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Toggle console with backtick */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_GRAVE)) {
            agentite_debug_toggle_console(debug);
        }

        /* Debug flag toggles */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F1)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_ENTITY_GIZMOS);
            printf("Entity gizmos: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_ENTITY_GIZMOS) ? "ON" : "OFF");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F2)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_COLLISION_SHAPES);
            printf("Collision shapes: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_COLLISION_SHAPES) ? "ON" : "OFF");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F3)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_AI_PATHS);
            printf("AI paths: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_AI_PATHS) ? "ON" : "OFF");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F4)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_SPATIAL_GRID);
            printf("Spatial grid: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_SPATIAL_GRID) ? "ON" : "OFF");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F5)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_FOG_OF_WAR);
            printf("Fog of war: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_FOG_OF_WAR) ? "ON" : "OFF");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F6)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_TURN_STATE);
            printf("Turn state: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_TURN_STATE) ? "ON" : "OFF");
        }
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F7)) {
            agentite_debug_toggle(debug, AGENTITE_DEBUG_PERFORMANCE);
            printf("Performance: %s\n",
                   agentite_debug_is_enabled(debug, AGENTITE_DEBUG_PERFORMANCE) ? "ON" : "OFF");
        }

        /* Advance turn on SPACE */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_SPACE)) {
            bool turn_complete = agentite_turn_advance(&turn_mgr);
            printf("Turn %d, Phase: %s%s\n",
                   agentite_turn_number(&turn_mgr),
                   agentite_turn_phase_name(agentite_turn_current_phase(&turn_mgr)),
                   turn_complete ? " (Turn Complete!)" : "");

            /* Move path follower to next waypoint when turn completes */
            if (turn_complete && path_follower && path && current_waypoint < path->length - 1) {
                current_waypoint++;

                /* Update entity position */
                float new_x = (float)path->points[current_waypoint].x * tile_size + tile_size * 0.5f;
                float new_y = (float)path->points[current_waypoint].y * tile_size + tile_size * 0.5f;
                C_Position *pos = (C_Position *)ecs_get_mut_id(ecs, path_follower, ecs_id(C_Position));
                if (pos) {
                    /* Calculate velocity as direction to next waypoint */
                    C_Velocity *vel = (C_Velocity *)ecs_get_mut_id(ecs, path_follower, ecs_id(C_Velocity));
                    if (vel) {
                        vel->vx = (new_x - pos->x) * 2.0f;
                        vel->vy = (new_y - pos->y) * 2.0f;
                    }
                    pos->x = new_x;
                    pos->y = new_y;
                }

                /* Update path visualization to show current waypoint */
                agentite_debug_set_path_waypoint(debug, path_viz_id, current_waypoint);

                printf("  -> Entity moved to waypoint %d/%d (%.0f, %.0f)\n",
                       current_waypoint + 1, path->length, new_x, new_y);
            }
        }

        /* Camera controls */
        float cam_speed = 200.0f * dt;
        if (agentite_input_key_pressed(input, SDL_SCANCODE_W))
            agentite_camera_move(camera, 0, -cam_speed);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_S))
            agentite_camera_move(camera, 0, cam_speed);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_A))
            agentite_camera_move(camera, -cam_speed, 0);
        if (agentite_input_key_pressed(input, SDL_SCANCODE_D))
            agentite_camera_move(camera, cam_speed, 0);

        /* Zoom with scroll */
        float scroll_x, scroll_y;
        agentite_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y != 0) {
            float zoom = agentite_camera_get_zoom(camera);
            zoom *= (scroll_y > 0) ? 1.1f : 0.9f;
            if (zoom < 0.1f) zoom = 0.1f;
            if (zoom > 10.0f) zoom = 10.0f;
            agentite_camera_set_zoom(camera, zoom);
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        agentite_camera_update(camera);

        /* Begin gizmo frame */
        agentite_gizmos_begin(gizmos, camera);
        agentite_gizmos_set_screen_size(gizmos, 1280, 720);

        /* Draw debug visualizations */
        agentite_debug_draw_ex(debug, gizmos, camera);

        /* Draw a reference grid */
        vec3 grid_center = {640.0f, 360.0f, 0.0f};
        vec3 grid_normal = {0.0f, 0.0f, 1.0f};
        agentite_gizmos_grid(gizmos, grid_center, grid_normal, 1200.0f, 100.0f, 0x20202040);

        agentite_gizmos_end(gizmos);

        /* Build UI frame (must happen before upload) */
        aui_begin_frame(ui, dt);
        agentite_debug_draw_ui(debug, ui);

        /* Draw help text in top-left corner */
        if (!agentite_debug_console_is_open(debug)) {
            float text_x = 10.0f;
            float text_y = 10.0f;
            float line_h = 18.0f;
            uint32_t text_color = 0xFFFFFFFF;
            uint32_t dim_color = 0xAAAAAAFF;

            aui_draw_text(ui, "=== Debug Tools Demo ===", text_x, text_y, text_color);
            text_y += line_h + 4;

            aui_draw_text(ui, "F1 - Entity Gizmos", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_ENTITY_GIZMOS) ? text_color : dim_color);
            text_y += line_h;
            aui_draw_text(ui, "F2 - Collision Shapes", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_COLLISION_SHAPES) ? text_color : dim_color);
            text_y += line_h;
            aui_draw_text(ui, "F3 - AI Paths", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_AI_PATHS) ? text_color : dim_color);
            text_y += line_h;
            aui_draw_text(ui, "F4 - Spatial Grid", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_SPATIAL_GRID) ? text_color : dim_color);
            text_y += line_h;
            aui_draw_text(ui, "F5 - Fog of War", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_FOG_OF_WAR) ? text_color : dim_color);
            text_y += line_h;
            aui_draw_text(ui, "F6 - Turn State", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_TURN_STATE) ? text_color : dim_color);
            text_y += line_h;
            aui_draw_text(ui, "F7 - Performance", text_x, text_y,
                agentite_debug_is_enabled(debug, AGENTITE_DEBUG_PERFORMANCE) ? text_color : dim_color);
            text_y += line_h + 4;

            aui_draw_text(ui, "` - Open Console", text_x, text_y, dim_color);
            text_y += line_h;
            aui_draw_text(ui, "SPACE - Advance Turn", text_x, text_y, dim_color);
            text_y += line_h;
            aui_draw_text(ui, "WASD - Pan Camera", text_x, text_y, dim_color);
            text_y += line_h;
            aui_draw_text(ui, "Scroll - Zoom", text_x, text_y, dim_color);
            text_y += line_h;
            aui_draw_text(ui, "ESC - Quit", text_x, text_y, dim_color);

            /* Show turn info below performance panel area */
            char turn_buf[64];
            snprintf(turn_buf, sizeof(turn_buf), "Turn %d - %s",
                     agentite_turn_number(&turn_mgr),
                     agentite_turn_phase_name(agentite_turn_current_phase(&turn_mgr)));
            aui_draw_text(ui, turn_buf, 1080, 70, 0x00FF00FF);
        }

        if (agentite_debug_console_is_open(debug)) {
            agentite_debug_console_panel(debug, ui, 0, 720 - 300, 1280, 300);
        }
        aui_end_frame(ui);

        /* Render - all uploads first, then render pass */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload all GPU data before render pass */
            agentite_gizmos_upload(gizmos, cmd);
            aui_upload(ui, cmd);

            /* Single render pass for everything */
            if (agentite_begin_render_pass(engine, 0.05f, 0.05f, 0.08f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_gizmos_render(gizmos, cmd, pass);
                aui_render(ui, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    aui_shutdown(ui);
    agentite_debug_destroy(debug);
    if (path) agentite_path_destroy(path);
    agentite_spatial_destroy(spatial);
    agentite_fog_destroy(fog);
    agentite_pathfinder_destroy(pathfinder);
    agentite_collision_shape_destroy(circle);
    agentite_collision_shape_destroy(box);
    agentite_collision_world_destroy(collision);
    agentite_ecs_shutdown(world);
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_gizmos_destroy(gizmos);
    agentite_shutdown(engine);

    return 0;
}
