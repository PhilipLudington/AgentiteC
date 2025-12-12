/**
 * Agentite Engine - UI Example
 *
 * Demonstrates the immediate-mode UI system.
 */

#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/input.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - UI Example",
        .window_width = 1280,
        .window_height = 720,
        .vsync = true
    };

    Agentite_Engine *engine = agentite_init(&config);
    if (!engine) {
        fprintf(stderr, "Failed to initialize engine\n");
        return 1;
    }

    /* Initialize UI system */
    AUI_Context *ui = aui_init(
        agentite_get_gpu_device(engine),
        agentite_get_window(engine),
        config.window_width,
        config.window_height,
        "assets/fonts/Roboto-Regular.ttf",
        16.0f
    );

    if (!ui) {
        fprintf(stderr, "Failed to initialize UI (make sure font exists)\n");
        agentite_shutdown(engine);
        return 1;
    }

    /* Set DPI scale for input coordinate conversion (logical coords used throughout) */
    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    Agentite_Input *input = agentite_input_init();

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

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* UI gets events first */
            if (aui_process_event(ui, &event)) {
                continue;
            }
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }
        agentite_input_update(input);

        /* Toggle panels with keys */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F1))
            show_settings = !show_settings;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F2))
            show_character = !show_character;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F3))
            show_debug = !show_debug;
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        /* Begin UI frame */
        aui_begin_frame(ui, dt);

        /* Settings Panel */
        if (show_settings) {
            if (aui_begin_panel(ui, "Settings", 50, 50, 300, 350,
                               AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

                aui_label(ui, "Audio");
                aui_separator(ui);

                aui_checkbox(ui, "Music", &checkbox_music);
                aui_checkbox(ui, "Sound Effects", &checkbox_sound);

                aui_spacing(ui, 5);
                aui_slider_float(ui, "Volume", &slider_volume, 0.0f, 1.0f);

                aui_spacing(ui, 15);
                aui_label(ui, "Graphics");
                aui_separator(ui);

                aui_slider_float(ui, "Brightness", &slider_brightness, 0.0f, 1.0f);
                aui_dropdown(ui, "Quality", &dropdown_quality, quality_options, 4);
                aui_dropdown(ui, "Resolution", &dropdown_resolution, resolution_options, 4);

                aui_spacing(ui, 15);

                if (aui_button(ui, "Apply Settings")) {
                    SDL_Log("Settings applied!");
                    SDL_Log("  Music: %s, Sound: %s", checkbox_music ? "ON" : "OFF",
                            checkbox_sound ? "ON" : "OFF");
                    SDL_Log("  Volume: %.0f%%", slider_volume * 100);
                    SDL_Log("  Quality: %s", quality_options[dropdown_quality]);
                }

                if (aui_button(ui, "Reset Defaults")) {
                    checkbox_music = true;
                    checkbox_sound = true;
                    slider_volume = 0.75f;
                    slider_brightness = 0.5f;
                    dropdown_quality = 1;
                    dropdown_resolution = 2;
                }

                aui_end_panel(ui);
            }
        }

        /* Character Panel */
        if (show_character) {
            if (aui_begin_panel(ui, "Character", 400, 50, 280, 350,
                               AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

                aui_textbox(ui, "Name", player_name, sizeof(player_name));

                aui_spacing(ui, 10);
                aui_label(ui, "Select Class:");
                aui_listbox(ui, "##class", &listbox_selection, listbox_items, 7, 140);

                aui_spacing(ui, 10);

                char info[64];
                snprintf(info, sizeof(info), "Selected: %s", listbox_items[listbox_selection]);
                aui_label(ui, info);

                aui_spacing(ui, 10);

                if (aui_button(ui, "Create Character")) {
                    SDL_Log("Creating character: %s the %s",
                            player_name, listbox_items[listbox_selection]);
                }

                aui_end_panel(ui);
            }
        }

        /* Debug Panel */
        if (show_debug) {
            if (aui_begin_panel(ui, "Debug Info", 730, 50, 250, 150,
                               AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

                char buf[64];
                snprintf(buf, sizeof(buf), "FPS: %.0f", 1.0f / dt);
                aui_label(ui, buf);

                snprintf(buf, sizeof(buf), "Frame Time: %.2f ms", dt * 1000.0f);
                aui_label(ui, buf);

                snprintf(buf, sizeof(buf), "Frame: %llu",
                         (unsigned long long)agentite_get_frame_count(engine));
                aui_label(ui, buf);

                aui_end_panel(ui);
            }
        }

        /* Help panel (always visible) */
        if (aui_begin_panel(ui, "Controls", 50, 420, 200, 100, AUI_PANEL_BORDER)) {
            aui_label(ui, "F1: Toggle Settings");
            aui_label(ui, "F2: Toggle Character");
            aui_label(ui, "F3: Toggle Debug");
            aui_label(ui, "ESC: Quit");
            aui_end_panel(ui);
        }

        /* Progress bar example */
        aui_progress_bar(ui, slider_volume, 0.0f, 1.0f);

        aui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            aui_upload(ui, cmd);

            if (agentite_begin_render_pass(engine, 0.15f, 0.15f, 0.2f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
                aui_render(ui, cmd, pass);
                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    agentite_input_shutdown(input);
    aui_shutdown(ui);
    agentite_shutdown(engine);

    return 0;
}
