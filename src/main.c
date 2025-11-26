#include "carbon/carbon.h"
#include "carbon/ui.h"
#include "carbon/ecs.h"
#include "carbon/sprite.h"
#include "carbon/camera.h"
#include "carbon/input.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/* Helper: Create a procedural checkerboard texture */
static Carbon_Texture *create_test_texture(Carbon_SpriteRenderer *sr, int size, int tile_size)
{
    unsigned char *pixels = malloc(size * size * 4);
    if (!pixels) return NULL;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            int tx = x / tile_size;
            int ty = y / tile_size;
            bool white = ((tx + ty) % 2) == 0;

            int idx = (y * size + x) * 4;
            if (white) {
                pixels[idx + 0] = 255;  /* R */
                pixels[idx + 1] = 200;  /* G */
                pixels[idx + 2] = 100;  /* B */
                pixels[idx + 3] = 255;  /* A */
            } else {
                pixels[idx + 0] = 100;  /* R */
                pixels[idx + 1] = 150;  /* G */
                pixels[idx + 2] = 255;  /* B */
                pixels[idx + 3] = 255;  /* A */
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

    /* Configure engine */
    Carbon_Config config = {
        .window_title = "Carbon Engine - Sprite Demo",
        .window_width = 1280,
        .window_height = 720,
        .fullscreen = false,
        .vsync = true
    };

    /* Initialize engine */
    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize Carbon Engine\n");
        return 1;
    }

    /* Initialize UI system */
    CUI_Context *ui = cui_init(
        carbon_get_gpu_device(engine),
        carbon_get_window(engine),
        config.window_width,
        config.window_height,
        "assets/fonts/Roboto-Regular.ttf",  /* Font path */
        16.0f                                /* Font size */
    );

    if (!ui) {
        fprintf(stderr, "Failed to initialize UI system\n");
        carbon_shutdown(engine);
        return 1;
    }

    /* Initialize sprite renderer */
    Carbon_SpriteRenderer *sprites = carbon_sprite_init(
        carbon_get_gpu_device(engine),
        carbon_get_window(engine)
    );

    if (!sprites) {
        fprintf(stderr, "Failed to initialize sprite renderer\n");
        cui_shutdown(ui);
        carbon_shutdown(engine);
        return 1;
    }

    /* Initialize camera */
    Carbon_Camera *camera = carbon_camera_create(
        (float)config.window_width,
        (float)config.window_height
    );
    if (!camera) {
        fprintf(stderr, "Failed to create camera\n");
        carbon_sprite_shutdown(sprites);
        cui_shutdown(ui);
        carbon_shutdown(engine);
        return 1;
    }

    /* Connect camera to sprite renderer */
    carbon_sprite_set_camera(sprites, camera);

    /* Center camera on the sprite demo area */
    carbon_camera_set_position(camera, 900.0f, 500.0f);

    SDL_Log("Camera initialized at (900, 500)");

    /* Create test texture */
    Carbon_Texture *tex_checker = create_test_texture(sprites, 64, 8);

    if (!tex_checker) {
        fprintf(stderr, "Failed to create test texture\n");
        carbon_sprite_shutdown(sprites);
        cui_shutdown(ui);
        carbon_shutdown(engine);
        return 1;
    }

    /* Create sprite from texture */
    Carbon_Sprite sprite_checker = carbon_sprite_from_texture(tex_checker);

    SDL_Log("Sprite system initialized with test textures");

    /* Initialize ECS world */
    Carbon_World *ecs_world = carbon_ecs_init();
    if (!ecs_world) {
        fprintf(stderr, "Failed to initialize ECS world\n");
        carbon_texture_destroy(sprites, tex_checker);
        carbon_sprite_shutdown(sprites);
        cui_shutdown(ui);
        carbon_shutdown(engine);
        return 1;
    }

    /* Create some demo entities */
    ecs_world_t *w = carbon_ecs_get_world(ecs_world);

    ecs_entity_t player = carbon_ecs_entity_new_named(ecs_world, "Player");
    ecs_set(w, player, C_Position, { .x = 100.0f, .y = 100.0f });
    ecs_set(w, player, C_Velocity, { .vx = 0.0f, .vy = 0.0f });
    ecs_set(w, player, C_Health, { .health = 100, .max_health = 100 });

    ecs_entity_t enemy = carbon_ecs_entity_new_named(ecs_world, "Enemy");
    ecs_set(w, enemy, C_Position, { .x = 500.0f, .y = 300.0f });
    ecs_set(w, enemy, C_Velocity, { .vx = -10.0f, .vy = 5.0f });
    ecs_set(w, enemy, C_Health, { .health = 50, .max_health = 50 });

    SDL_Log("Created player entity: %llu", (unsigned long long)player);
    SDL_Log("Created enemy entity: %llu", (unsigned long long)enemy);

    /* Initialize input system */
    Carbon_Input *input = carbon_input_init();
    if (!input) {
        fprintf(stderr, "Failed to initialize input system\n");
        carbon_ecs_shutdown(ecs_world);
        carbon_texture_destroy(sprites, tex_checker);
        carbon_camera_destroy(camera);
        carbon_sprite_shutdown(sprites);
        cui_shutdown(ui);
        carbon_shutdown(engine);
        return 1;
    }

    /* Register input actions and bind keys */
    int action_cam_up = carbon_input_register_action(input, "cam_up");
    int action_cam_down = carbon_input_register_action(input, "cam_down");
    int action_cam_left = carbon_input_register_action(input, "cam_left");
    int action_cam_right = carbon_input_register_action(input, "cam_right");
    int action_cam_rot_left = carbon_input_register_action(input, "cam_rot_left");
    int action_cam_rot_right = carbon_input_register_action(input, "cam_rot_right");
    int action_cam_reset = carbon_input_register_action(input, "cam_reset");
    int action_zoom_in = carbon_input_register_action(input, "zoom_in");
    int action_zoom_out = carbon_input_register_action(input, "zoom_out");
    int action_quit = carbon_input_register_action(input, "quit");

    /* Bind keyboard keys */
    carbon_input_bind_key(input, action_cam_up, SDL_SCANCODE_W);
    carbon_input_bind_key(input, action_cam_up, SDL_SCANCODE_UP);
    carbon_input_bind_key(input, action_cam_down, SDL_SCANCODE_S);
    carbon_input_bind_key(input, action_cam_down, SDL_SCANCODE_DOWN);
    carbon_input_bind_key(input, action_cam_left, SDL_SCANCODE_A);
    carbon_input_bind_key(input, action_cam_left, SDL_SCANCODE_LEFT);
    carbon_input_bind_key(input, action_cam_right, SDL_SCANCODE_D);
    carbon_input_bind_key(input, action_cam_right, SDL_SCANCODE_RIGHT);
    carbon_input_bind_key(input, action_cam_rot_left, SDL_SCANCODE_Q);
    carbon_input_bind_key(input, action_cam_rot_right, SDL_SCANCODE_E);
    carbon_input_bind_key(input, action_cam_reset, SDL_SCANCODE_R);
    carbon_input_bind_key(input, action_quit, SDL_SCANCODE_ESCAPE);

    /* Bind gamepad (if connected) */
    carbon_input_bind_gamepad_axis(input, action_cam_left, SDL_GAMEPAD_AXIS_LEFTX, 0.3f, false);
    carbon_input_bind_gamepad_axis(input, action_cam_right, SDL_GAMEPAD_AXIS_LEFTX, 0.3f, true);
    carbon_input_bind_gamepad_axis(input, action_cam_up, SDL_GAMEPAD_AXIS_LEFTY, 0.3f, false);
    carbon_input_bind_gamepad_axis(input, action_cam_down, SDL_GAMEPAD_AXIS_LEFTY, 0.3f, true);
    carbon_input_bind_gamepad_button(input, action_cam_rot_left, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    carbon_input_bind_gamepad_button(input, action_cam_rot_right, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    carbon_input_bind_gamepad_button(input, action_cam_reset, SDL_GAMEPAD_BUTTON_SOUTH);
    carbon_input_bind_gamepad_axis(input, action_zoom_in, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.1f, true);
    carbon_input_bind_gamepad_axis(input, action_zoom_out, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, 0.1f, true);
    carbon_input_bind_gamepad_button(input, action_quit, SDL_GAMEPAD_BUTTON_BACK);

    SDL_Log("Input system initialized with action bindings");

    /* Demo state */
    bool checkbox_value = false;
    float slider_value = 0.5f;
    int dropdown_selection = 0;
    const char *dropdown_items[] = {"Easy", "Medium", "Hard", "Extreme"};
    char textbox_buffer[128] = "Player 1";
    int listbox_selection = 0;
    const char *listbox_items[] = {
        "Infantry", "Cavalry", "Archers", "Siege",
        "Navy", "Air Force", "Special Ops"
    };

    /* Sprite demo state */
    float sprite_rotation = 0.0f;
    float sprite_time = 0.0f;

    /* Camera control state */
    float target_zoom = 1.0f;
    float mouse_world_x = 0.0f, mouse_world_y = 0.0f;

    /* Main game loop */
    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);

        /* Begin input frame (reset per-frame state) */
        carbon_input_begin_frame(input);

        /* Process events - UI gets first chance, then input system */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Let UI process the event first */
            if (cui_process_event(ui, &event)) {
                continue;  /* UI consumed the event */
            }

            /* Let input system process the event */
            carbon_input_process_event(input, &event);

            /* Handle quit event */
            if (event.type == SDL_EVENT_QUIT) {
                carbon_quit(engine);
            }
        }

        /* Update input state (compute just_pressed/released) */
        carbon_input_update(input);

        /* Get delta time */
        float dt = carbon_get_delta_time(engine);

        /* Handle quit action */
        if (carbon_input_action_just_pressed(input, action_quit)) {
            carbon_quit(engine);
        }

        /* Handle mouse wheel zoom */
        float scroll_x, scroll_y;
        carbon_input_get_scroll(input, &scroll_x, &scroll_y);
        if (scroll_y > 0) {
            target_zoom *= 1.15f;
        } else if (scroll_y < 0) {
            target_zoom /= 1.15f;
        }

        /* Handle gamepad trigger zoom */
        if (carbon_input_action_pressed(input, action_zoom_in)) {
            float val = carbon_input_action_value(input, action_zoom_in);
            target_zoom *= 1.0f + 0.5f * val * dt;
        }
        if (carbon_input_action_pressed(input, action_zoom_out)) {
            float val = carbon_input_action_value(input, action_zoom_out);
            target_zoom /= 1.0f + 0.5f * val * dt;
        }

        /* Clamp zoom */
        if (target_zoom < 0.1f) target_zoom = 0.1f;
        if (target_zoom > 10.0f) target_zoom = 10.0f;

        /* Camera controls - using actions (supports keyboard + gamepad) */
        float cam_speed = 300.0f / carbon_camera_get_zoom(camera);  /* Faster when zoomed out */

        if (carbon_input_action_pressed(input, action_cam_up))
            carbon_camera_move(camera, 0, -cam_speed * dt);
        if (carbon_input_action_pressed(input, action_cam_down))
            carbon_camera_move(camera, 0, cam_speed * dt);
        if (carbon_input_action_pressed(input, action_cam_left))
            carbon_camera_move(camera, -cam_speed * dt, 0);
        if (carbon_input_action_pressed(input, action_cam_right))
            carbon_camera_move(camera, cam_speed * dt, 0);
        if (carbon_input_action_pressed(input, action_cam_rot_left)) {
            float rot = carbon_camera_get_rotation(camera);
            carbon_camera_set_rotation(camera, rot - 60.0f * dt);
        }
        if (carbon_input_action_pressed(input, action_cam_rot_right)) {
            float rot = carbon_camera_get_rotation(camera);
            carbon_camera_set_rotation(camera, rot + 60.0f * dt);
        }
        if (carbon_input_action_just_pressed(input, action_cam_reset)) {
            /* Reset camera */
            carbon_camera_set_position(camera, 900.0f, 500.0f);
            carbon_camera_set_rotation(camera, 0.0f);
            target_zoom = 1.0f;
        }

        /* Smooth zoom interpolation */
        float current_zoom = carbon_camera_get_zoom(camera);
        float new_zoom = current_zoom + (target_zoom - current_zoom) * 5.0f * dt;
        carbon_camera_set_zoom(camera, new_zoom);

        /* Update camera matrices */
        carbon_camera_update(camera);

        /* Get mouse position in world coordinates */
        float mouse_x, mouse_y;
        carbon_input_get_mouse_position(input, &mouse_x, &mouse_y);
        carbon_camera_screen_to_world(camera, mouse_x, mouse_y, &mouse_world_x, &mouse_world_y);

        /* Update sprite animation */
        sprite_time += dt;
        sprite_rotation += 45.0f * dt;  /* Rotate 45 degrees per second */
        if (sprite_rotation > 360.0f) sprite_rotation -= 360.0f;

        /* Progress ECS systems */
        carbon_ecs_progress(ecs_world, dt);

        /* Update enemy position (simple demo movement) */
        const C_Position *enemy_pos = ecs_get(w, enemy, C_Position);
        const C_Velocity *enemy_vel = ecs_get(w, enemy, C_Velocity);
        if (enemy_pos && enemy_vel) {
            float new_x = enemy_pos->x + enemy_vel->vx * dt;
            float new_y = enemy_pos->y + enemy_vel->vy * dt;
            /* Bounce off edges */
            float new_vx = enemy_vel->vx;
            float new_vy = enemy_vel->vy;
            if (new_x < 0 || new_x > 1280) new_vx = -enemy_vel->vx;
            if (new_y < 0 || new_y > 720) new_vy = -enemy_vel->vy;
            ecs_set(w, enemy, C_Position, { .x = new_x, .y = new_y });
            ecs_set(w, enemy, C_Velocity, { .vx = new_vx, .vy = new_vy });
        }

        /* Begin UI frame */
        cui_begin_frame(ui, dt);

        /* Draw a demo panel */
        if (cui_begin_panel(ui, "Game Settings", 50, 50, 300, 400,
                           CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

            cui_label(ui, "Welcome to Carbon UI!");
            cui_spacing(ui, 10);

            if (cui_button(ui, "Start Game")) {
                SDL_Log("Start Game clicked!");
            }

            if (cui_button(ui, "Load Game")) {
                SDL_Log("Load Game clicked!");
            }

            cui_separator(ui);

            cui_checkbox(ui, "Enable Music", &checkbox_value);

            cui_slider_float(ui, "Volume", &slider_value, 0.0f, 1.0f);

            cui_spacing(ui, 5);

            cui_dropdown(ui, "Difficulty", &dropdown_selection,
                        dropdown_items, 4);

            cui_spacing(ui, 5);

            cui_textbox(ui, "Name", textbox_buffer, sizeof(textbox_buffer));

            cui_end_panel(ui);
        }

        /* Draw a second panel for unit selection */
        if (cui_begin_panel(ui, "Units", 400, 50, 250, 300,
                           CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

            cui_label(ui, "Select Unit Type:");
            cui_listbox(ui, "##units", &listbox_selection,
                       listbox_items, 7, 150);

            cui_spacing(ui, 10);

            if (cui_button(ui, "Deploy Unit")) {
                SDL_Log("Deploying: %s", listbox_items[listbox_selection]);
            }

            cui_end_panel(ui);
        }

        /* Draw ECS entity info panel */
        if (cui_begin_panel(ui, "ECS Entities", 700, 50, 280, 200,
                           CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

            cui_label(ui, "Player Entity:");
            const C_Position *p_pos = ecs_get(w, player, C_Position);
            const C_Health *p_hp = ecs_get(w, player, C_Health);
            if (p_pos) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  Pos: (%.0f, %.0f)", p_pos->x, p_pos->y);
                cui_label(ui, buf);
            }
            if (p_hp) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  HP: %d/%d", p_hp->health, p_hp->max_health);
                cui_label(ui, buf);
            }

            cui_separator(ui);

            cui_label(ui, "Enemy Entity:");
            const C_Health *e_hp = ecs_get(w, enemy, C_Health);
            if (enemy_pos) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  Pos: (%.0f, %.0f)", enemy_pos->x, enemy_pos->y);
                cui_label(ui, buf);
            }
            if (e_hp) {
                char buf[64];
                snprintf(buf, sizeof(buf), "  HP: %d/%d", e_hp->health, e_hp->max_health);
                cui_label(ui, buf);
            }

            cui_end_panel(ui);
        }

        /* Draw Camera controls panel */
        if (cui_begin_panel(ui, "Camera", 700, 260, 280, 180,
                           CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

            float cam_x, cam_y;
            carbon_camera_get_position(camera, &cam_x, &cam_y);
            float cam_zoom = carbon_camera_get_zoom(camera);
            float cam_rot = carbon_camera_get_rotation(camera);

            char buf[64];
            snprintf(buf, sizeof(buf), "Position: (%.0f, %.0f)", cam_x, cam_y);
            cui_label(ui, buf);
            snprintf(buf, sizeof(buf), "Zoom: %.2fx", cam_zoom);
            cui_label(ui, buf);
            snprintf(buf, sizeof(buf), "Rotation: %.1f deg", cam_rot);
            cui_label(ui, buf);

            cui_separator(ui);

            snprintf(buf, sizeof(buf), "Mouse World: (%.0f, %.0f)", mouse_world_x, mouse_world_y);
            cui_label(ui, buf);

            cui_spacing(ui, 5);
            cui_label(ui, "WASD: Pan | Wheel: Zoom");
            cui_label(ui, "Q/E: Rotate | R: Reset");

            cui_end_panel(ui);
        }

        /* Draw some standalone widgets */
        cui_progress_bar(ui, slider_value, 0.0f, 1.0f);

        /* End UI frame */
        cui_end_frame(ui);

        /* Build sprite batch */
        carbon_sprite_begin(sprites, NULL);

        /* Draw some demo sprites in the background area */
        /* Row of static checkerboard sprites */
        for (int i = 0; i < 8; i++) {
            carbon_sprite_draw(sprites, &sprite_checker,
                               700.0f + i * 70.0f, 400.0f);
        }

        /* Rotating sprite */
        carbon_sprite_draw_ex(sprites, &sprite_checker,
                              800.0f, 500.0f,
                              2.0f, 2.0f,
                              sprite_rotation,
                              0.5f, 0.5f);

        /* Bouncing/pulsing sprite */
        float pulse = 1.0f + 0.3f * sinf(sprite_time * 3.0f);
        carbon_sprite_draw_scaled(sprites, &sprite_checker,
                                  950.0f, 500.0f,
                                  pulse, pulse);

        /* Tinted sprites - using same texture for consistent batching */
        carbon_sprite_draw_tinted(sprites, &sprite_checker,
                                  1050.0f, 450.0f,
                                  1.0f, 0.3f, 0.3f, 1.0f);  /* Red tint */
        carbon_sprite_draw_tinted(sprites, &sprite_checker,
                                  1050.0f, 550.0f,
                                  0.3f, 1.0f, 0.3f, 1.0f);  /* Green tint */

        /* Acquire command buffer for GPU operations */
        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload sprite data to GPU (must be done BEFORE render pass) */
            carbon_sprite_upload(sprites, cmd);

            /* Upload UI data to GPU (must be done BEFORE render pass) */
            cui_upload(ui, cmd);

            /* Begin render pass */
            if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(engine);

                /* Render sprites first (background) */
                carbon_sprite_render(sprites, cmd, pass);

                /* Render UI on top */
                cui_render(ui, cmd, pass);

                carbon_end_render_pass(engine);
            }
        }

        /* End sprite batch (cleanup state) */
        carbon_sprite_end(sprites, NULL, NULL);

        carbon_end_frame(engine);
    }

    /* Cleanup */
    carbon_input_shutdown(input);
    carbon_ecs_shutdown(ecs_world);
    carbon_texture_destroy(sprites, tex_checker);
    carbon_camera_destroy(camera);
    carbon_sprite_shutdown(sprites);
    cui_shutdown(ui);
    carbon_shutdown(engine);

    return 0;
}
