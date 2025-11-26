#include "carbon/carbon.h"
#include "carbon/ui.h"
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
    cui_shutdown(ui);
    carbon_shutdown(engine);

    return 0;
}
