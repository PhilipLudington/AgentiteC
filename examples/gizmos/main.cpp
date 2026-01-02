/**
 * Agentite Engine - Gizmos Example
 *
 * Demonstrates the gizmo rendering system for:
 * - Transform gizmos (translate, rotate, scale)
 * - Debug drawing (lines, boxes, spheres, grids)
 * - Screen-space overlays (2D shapes)
 */

#include "agentite/agentite.h"
#include "agentite/gizmos.h"
#include "agentite/camera.h"
#include "agentite/input.h"
#include <stdio.h>
#include <math.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Gizmos Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine: %s\n", agentite_get_last_error());
        return 1;
    }

    /* Initialize gizmos */
    Agentite_GizmoConfig gizmo_config = AGENTITE_GIZMO_CONFIG_DEFAULT;
    Agentite_Gizmos *gizmos = agentite_gizmos_create(
        agentite_get_gpu_device(engine),
        &gizmo_config
    );
    if (!gizmos) {
        fprintf(stderr, "Failed to create gizmos: %s\n", agentite_get_last_error());
        agentite_shutdown(engine);
        return 1;
    }

    /* Initialize camera */
    Agentite_Camera *camera = agentite_camera_create(1280.0f, 720.0f);
    agentite_camera_set_position(camera, 640.0f, 360.0f);

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();

    /* Object position (controlled by translate gizmo) */
    vec3 object_position = {640.0f, 360.0f, 0.0f};

    /* Gizmo mode */
    Agentite_GizmoMode current_mode = AGENTITE_GIZMO_TRANSLATE;

    /* Animation time */
    float time = 0.0f;

    printf("Controls:\n");
    printf("  1/2/3 - Switch gizmo mode (Translate/Rotate/Scale)\n");
    printf("  WASD  - Pan camera\n");
    printf("  Scroll - Zoom\n");
    printf("  Mouse - Drag gizmo handles\n");
    printf("  ESC   - Quit\n");

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);
        time += dt;

        /* Process input */
        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Mode switching */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_1))
            current_mode = AGENTITE_GIZMO_TRANSLATE;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_2))
            current_mode = AGENTITE_GIZMO_ROTATE;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_3))
            current_mode = AGENTITE_GIZMO_SCALE;

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

        /* Zoom with mouse wheel */
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

        /* Get mouse state */
        float mouse_x, mouse_y;
        agentite_input_get_mouse_position(input, &mouse_x, &mouse_y);
        bool mouse_down = agentite_input_mouse_button(input, 0);      /* 0 = left button */
        bool mouse_pressed = agentite_input_mouse_button_pressed(input, 0);

        /* Begin gizmo frame */
        agentite_gizmos_begin(gizmos, camera);
        agentite_gizmos_set_screen_size(gizmos, 1280, 720);
        agentite_gizmos_update_input(gizmos, mouse_x, mouse_y, mouse_down, mouse_pressed);

        /* Draw grid */
        vec3 grid_center = {640.0f, 360.0f, 0.0f};
        vec3 grid_normal = {0.0f, 0.0f, 1.0f};
        agentite_gizmos_grid(gizmos, grid_center, grid_normal, 800.0f, 50.0f, 0x40404080);

        /* Draw coordinate axes at origin */
        vec3 origin = {100.0f, 100.0f, 0.0f};
        vec3 x_end = {200.0f, 100.0f, 0.0f};
        vec3 y_end = {100.0f, 200.0f, 0.0f};
        agentite_gizmos_arrow(gizmos, origin, x_end, 0xFF0000FF);
        agentite_gizmos_arrow(gizmos, origin, y_end, 0x00FF00FF);

        /* Draw transform gizmo on the object */
        Agentite_GizmoResult result = agentite_gizmo_transform(
            gizmos, current_mode, object_position, NULL);

        /* Apply gizmo result */
        if (result.active) {
            object_position[0] += result.delta[0];
            object_position[1] += result.delta[1];
            object_position[2] += result.delta[2];
        }

        /* Draw a box at the object position (representing the object) */
        vec3 box_size = {40.0f, 40.0f, 40.0f};
        agentite_gizmos_box(gizmos, object_position, box_size, 0xFFFFFFFF);

        /* Draw some debug shapes */
        /* Sphere */
        vec3 sphere_center = {300.0f, 500.0f, 0.0f};
        agentite_gizmos_sphere(gizmos, sphere_center, 30.0f, 0xFF00FFFF);

        /* Animated circle */
        vec3 circle_center = {500.0f, 500.0f, 0.0f};
        vec3 circle_normal = {0.0f, 0.0f, 1.0f};
        float radius = 20.0f + 10.0f * sinf(time * 2.0f);
        agentite_gizmos_circle(gizmos, circle_center, circle_normal, radius, 0xFFFF00FF);

        /* Ray */
        vec3 ray_origin = {700.0f, 500.0f, 0.0f};
        vec3 ray_dir = {cosf(time), sinf(time), 0.0f};
        agentite_gizmos_ray(gizmos, ray_origin, ray_dir, 50.0f, 0x00FFFFFF);

        /* Bounds */
        vec3 bounds_min = {900.0f, 450.0f, 0.0f};
        vec3 bounds_max = {1000.0f, 550.0f, 0.0f};
        agentite_gizmos_bounds(gizmos, bounds_min, bounds_max, 0x88FF88FF);

        /* 2D overlays (UI elements in screen space) */
        /* Selection box */
        agentite_gizmos_rect_2d(gizmos, 1100.0f, 50.0f, 150.0f, 100.0f, 0xFFFFFFFF);

        /* Mode indicator - draw colored rect based on mode */
        /* TODO: Add text rendering to show mode name */
        /* Draw a filled rect as mode indicator background */
        uint32_t mode_color = 0x00000080;
        if (current_mode == AGENTITE_GIZMO_TRANSLATE) mode_color = 0xFF000080;
        else if (current_mode == AGENTITE_GIZMO_ROTATE) mode_color = 0x00FF0080;
        else if (current_mode == AGENTITE_GIZMO_SCALE) mode_color = 0x0000FF80;
        agentite_gizmos_rect_filled_2d(gizmos, 10.0f, 10.0f, 120.0f, 30.0f, mode_color);
        agentite_gizmos_rect_2d(gizmos, 10.0f, 10.0f, 120.0f, 30.0f, 0xFFFFFFFF);

        /* Circle at mouse position when hovering */
        if (agentite_gizmos_is_hovered(gizmos)) {
            agentite_gizmos_circle_2d(gizmos, mouse_x, mouse_y, 15.0f, 0xFFFF00FF);
        }

        agentite_gizmos_end(gizmos);

        /* Acquire command buffer and render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload gizmo data before render pass */
            agentite_gizmos_upload(gizmos, cmd);

            /* Render */
            if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                agentite_gizmos_render(gizmos, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_input_shutdown(input);
    agentite_camera_destroy(camera);
    agentite_gizmos_destroy(gizmos);
    agentite_shutdown(engine);

    return 0;
}
