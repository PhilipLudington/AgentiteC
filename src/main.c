#include "carbon/carbon.h"
#include "carbon/ui.h"
#include "carbon/ecs.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure engine */
    Carbon_Config config = {
        .window_title = "Carbon Engine - UI Demo",
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

    /* Initialize ECS world */
    Carbon_World *ecs_world = carbon_ecs_init();
    if (!ecs_world) {
        fprintf(stderr, "Failed to initialize ECS world\n");
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

    /* Main game loop */
    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);

        /* Process events - UI gets first chance */
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Let UI process the event first */
            if (cui_process_event(ui, &event)) {
                continue;  /* UI consumed the event */
            }

            /* Handle remaining events */
            switch (event.type) {
            case SDL_EVENT_QUIT:
                carbon_quit(engine);
                break;
            case SDL_EVENT_KEY_DOWN:
                if (event.key.key == SDLK_ESCAPE) {
                    carbon_quit(engine);
                }
                break;
            }
        }

        /* Get delta time */
        float dt = carbon_get_delta_time(engine);

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

        /* Draw some standalone widgets */
        cui_progress_bar(ui, slider_value, 0.0f, 1.0f);

        /* End UI frame */
        cui_end_frame(ui);

        /* Acquire command buffer for GPU operations */
        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload UI data to GPU (must be done BEFORE render pass) */
            cui_upload(ui, cmd);

            /* Begin render pass */
            if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
                /* Render UI */
                cui_render(ui, cmd, carbon_get_render_pass(engine));

                carbon_end_render_pass(engine);
            }
        }

        carbon_end_frame(engine);
    }

    /* Cleanup */
    carbon_ecs_shutdown(ecs_world);
    cui_shutdown(ui);
    carbon_shutdown(engine);

    return 0;
}
