/**
 * Carbon Engine - UI Example
 *
 * Demonstrates the immediate-mode UI system.
 */

#include "carbon/carbon.h"
#include "carbon/ui.h"
#include "carbon/input.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Carbon_Config config = {
        .window_title = "Carbon - UI Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Carbon_Engine *engine = carbon_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize UI system */
    CUI_Context *ui = cui_init(
        carbon_get_gpu_device(engine),
        carbon_get_window(engine),
        config.window_width,
        config.window_height,
        "assets/fonts/Roboto-Regular.ttf",
        16.0f
    );

    if (!ui) {
        fprintf(stderr, "Failed to initialize UI (make sure font exists)\n");
        carbon_shutdown(engine);
        return 1;
    }

    Carbon_Input *input = carbon_input_init();

    /* UI state */
    bool checkbox_music = true;
    bool checkbox_sound = true;
    float slider_volume = 0.75f;
    float slider_brightness = 0.5f;
    int dropdown_quality = 1;
    const char *quality_options[] = {"Low", "Medium", "High", "Ultra"};
    int dropdown_resolution = 2;
    const char *resolution_options[] = {"1280x720", "1600x900", "1920x1080", "2560x1440"};
    char player_name[64] = "Player";
    int listbox_selection = 0;
    const char *listbox_items[] = {
        "Warrior", "Mage", "Rogue", "Archer",
        "Paladin", "Necromancer", "Bard"
    };

    /* Panel visibility */
    bool show_settings = true;
    bool show_character = true;
    bool show_debug = false;

    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        float dt = carbon_get_delta_time(engine);

        carbon_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* UI gets events first */
            if (cui_process_event(ui, &event)) {
                continue;
            }
            carbon_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                carbon_quit(engine);
            }
        }
        carbon_input_update(input);

        /* Toggle panels with keys */
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_F1))
            show_settings = !show_settings;
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_F2))
            show_character = !show_character;
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_F3))
            show_debug = !show_debug;
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            carbon_quit(engine);

        /* Begin UI frame */
        cui_begin_frame(ui, dt);

        /* Settings Panel */
        if (show_settings) {
            if (cui_begin_panel(ui, "Settings", 50, 50, 300, 350,
                               CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

                cui_label(ui, "Audio");
                cui_separator(ui);

                cui_checkbox(ui, "Music", &checkbox_music);
                cui_checkbox(ui, "Sound Effects", &checkbox_sound);

                cui_spacing(ui, 5);
                cui_slider_float(ui, "Volume", &slider_volume, 0.0f, 1.0f);

                cui_spacing(ui, 15);
                cui_label(ui, "Graphics");
                cui_separator(ui);

                cui_slider_float(ui, "Brightness", &slider_brightness, 0.0f, 1.0f);
                cui_dropdown(ui, "Quality", &dropdown_quality, quality_options, 4);
                cui_dropdown(ui, "Resolution", &dropdown_resolution, resolution_options, 4);

                cui_spacing(ui, 15);

                if (cui_button(ui, "Apply Settings")) {
                    SDL_Log("Settings applied!");
                    SDL_Log("  Music: %s, Sound: %s", checkbox_music ? "ON" : "OFF",
                            checkbox_sound ? "ON" : "OFF");
                    SDL_Log("  Volume: %.0f%%", slider_volume * 100);
                    SDL_Log("  Quality: %s", quality_options[dropdown_quality]);
                }

                if (cui_button(ui, "Reset Defaults")) {
                    checkbox_music = true;
                    checkbox_sound = true;
                    slider_volume = 0.75f;
                    slider_brightness = 0.5f;
                    dropdown_quality = 1;
                    dropdown_resolution = 2;
                }

                cui_end_panel(ui);
            }
        }

        /* Character Panel */
        if (show_character) {
            if (cui_begin_panel(ui, "Character", 400, 50, 280, 350,
                               CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

                cui_textbox(ui, "Name", player_name, sizeof(player_name));

                cui_spacing(ui, 10);
                cui_label(ui, "Select Class:");
                cui_listbox(ui, "##class", &listbox_selection, listbox_items, 7, 140);

                cui_spacing(ui, 10);

                char info[64];
                snprintf(info, sizeof(info), "Selected: %s", listbox_items[listbox_selection]);
                cui_label(ui, info);

                cui_spacing(ui, 10);

                if (cui_button(ui, "Create Character")) {
                    SDL_Log("Creating character: %s the %s",
                            player_name, listbox_items[listbox_selection]);
                }

                cui_end_panel(ui);
            }
        }

        /* Debug Panel */
        if (show_debug) {
            if (cui_begin_panel(ui, "Debug Info", 730, 50, 250, 150,
                               CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER)) {

                char buf[64];
                snprintf(buf, sizeof(buf), "FPS: %.0f", 1.0f / dt);
                cui_label(ui, buf);

                snprintf(buf, sizeof(buf), "Frame Time: %.2f ms", dt * 1000.0f);
                cui_label(ui, buf);

                snprintf(buf, sizeof(buf), "Frame: %llu",
                         (unsigned long long)carbon_get_frame_count(engine));
                cui_label(ui, buf);

                cui_end_panel(ui);
            }
        }

        /* Help panel (always visible) */
        if (cui_begin_panel(ui, "Controls", 50, 420, 200, 100, CUI_PANEL_BORDER)) {
            cui_label(ui, "F1: Toggle Settings");
            cui_label(ui, "F2: Toggle Character");
            cui_label(ui, "F3: Toggle Debug");
            cui_label(ui, "ESC: Quit");
            cui_end_panel(ui);
        }

        /* Progress bar example */
        cui_progress_bar(ui, slider_volume, 0.0f, 1.0f);

        cui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            cui_upload(ui, cmd);

            if (carbon_begin_render_pass(engine, 0.15f, 0.15f, 0.2f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(engine);
                cui_render(ui, cmd, pass);
                carbon_end_render_pass(engine);
            }
        }

        carbon_end_frame(engine);
    }

    /* Cleanup */
    carbon_input_shutdown(input);
    cui_shutdown(ui);
    carbon_shutdown(engine);

    return 0;
}
