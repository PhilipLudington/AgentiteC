/**
 * Agentite Engine - Dialog System Example
 *
 * Demonstrates modal dialogs, context menus, file dialogs, and notifications:
 * - Message dialogs (OK, Yes/No, custom buttons)
 * - Confirmation dialogs
 * - Input dialogs with validation
 * - Native file open/save/folder dialogs
 * - Context menus with submenus
 * - Toast notifications
 */

#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/ui_dialog.h"
#include "agentite/input.h"
#include <stdio.h>
#include <string.h>

/* Dialog result callbacks */
static void on_message_closed(AUI_DialogResult result, void *userdata) {
    (void)userdata;
    const char *result_name = "Unknown";
    switch (result) {
        case AUI_DIALOG_OK: result_name = "OK"; break;
        case AUI_DIALOG_CANCEL: result_name = "Cancel"; break;
        case AUI_DIALOG_YES: result_name = "Yes"; break;
        case AUI_DIALOG_NO: result_name = "No"; break;
        case AUI_DIALOG_CLOSE: result_name = "Close (X)"; break;
        default: break;
    }
    SDL_Log("Dialog closed with: %s", result_name);
}

static void on_confirm_result(bool confirmed, void *userdata) {
    (void)userdata;
    SDL_Log("Confirmation result: %s", confirmed ? "Yes" : "No");
}

static void on_input_result(bool confirmed, const char *text, void *userdata) {
    (void)userdata;
    if (confirmed) {
        SDL_Log("Input confirmed: '%s'", text);
    } else {
        SDL_Log("Input canceled");
    }
}

static void on_file_selected(const char *path, void *userdata) {
    (void)userdata;
    if (path) {
        SDL_Log("File selected: %s", path);
    } else {
        SDL_Log("File selection canceled");
    }
}

static void on_folder_selected(const char *path, void *userdata) {
    (void)userdata;
    if (path) {
        SDL_Log("Folder selected: %s", path);
    } else {
        SDL_Log("Folder selection canceled");
    }
}

/* ============================================================================
 * Editor-style "Open with Unsaved Changes" scenario
 * This tests chained dialogs: confirm dialog -> file dialog
 * ============================================================================ */
static bool s_is_dirty = false;
static AUI_Context *s_ui_context = NULL;

static AUI_FileFilter s_scene_filters[] = {
    {"Scene Files", "scene;json"},
    {"All Files", "*"},
};

static void on_open_file_result(const char *path, void *userdata) {
    (void)userdata;
    if (path) {
        SDL_Log("Scene opened: %s", path);
        s_is_dirty = false;  /* Reset dirty after opening */
    } else {
        SDL_Log("Open canceled");
    }
}

static void on_unsaved_changes_result(bool confirmed, void *userdata) {
    (void)userdata;
    SDL_Log("Unsaved changes dialog: %s", confirmed ? "Yes (save first)" : "No (discard)");

    /* Regardless of save choice, proceed to open file dialog */
    /* In a real app, "Yes" would save first, "No" would just open */
    if (s_ui_context) {
        aui_file_dialog_open(s_ui_context, "Open Scene",
                              NULL, s_scene_filters, 2,
                              on_open_file_result, NULL);
    }
}

/* Context menu callbacks */
static void on_menu_cut(void *userdata) {
    (void)userdata;
    SDL_Log("Cut selected");
}

static void on_menu_copy(void *userdata) {
    (void)userdata;
    SDL_Log("Copy selected");
}

static void on_menu_paste(void *userdata) {
    (void)userdata;
    SDL_Log("Paste selected");
}

static void on_menu_delete(void *userdata) {
    (void)userdata;
    SDL_Log("Delete selected");
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Dialog System Example",
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

    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    /* Store UI context for chained dialog callbacks */
    s_ui_context = ui;

    Agentite_Input *input = agentite_input_init();

    /* File dialog filters - SDL3 expects just extensions, no "*." prefix */
    AUI_FileFilter image_filters[] = {
        {"Image Files", "png;jpg;jpeg;bmp"},
        {"PNG Images", "png"},
        {"All Files", "*"},
    };

    AUI_FileFilter scene_filters[] = {
        {"Scene Files", "scene;json"},
        {"All Files", "*"},
    };

    /* Notification position */
    AUI_NotifyPosition notify_pos = AUI_NOTIFY_TOP_RIGHT;

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Process dialog events first (modal dialogs block other input) */
            if (aui_dialogs_process_event(ui, &event)) {
                continue;
            }
            if (aui_process_event(ui, &event)) {
                continue;
            }
            agentite_input_process_event(input, &event);
            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }

            /* Right-click for context menu */
            if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN &&
                event.button.button == SDL_BUTTON_RIGHT) {
                AUI_MenuItem items[] = {
                    {.label = "Cut", .shortcut = "Ctrl+X", .enabled = true,
                     .on_select = on_menu_cut},
                    {.label = "Copy", .shortcut = "Ctrl+C", .enabled = true,
                     .on_select = on_menu_copy},
                    {.label = "Paste", .shortcut = "Ctrl+V", .enabled = true,
                     .on_select = on_menu_paste},
                    {.label = NULL},  /* Separator */
                    {.label = "Delete", .shortcut = "Del", .enabled = true,
                     .on_select = on_menu_delete},
                };
                /* SDL3 returns logical coordinates on macOS, no DPI scaling needed */
                aui_context_menu_show(ui, (float)event.button.x,
                                      (float)event.button.y,
                                      items, 5);
            }
        }
        agentite_input_update(input);

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE))
            agentite_quit(engine);

        /* Begin UI frame */
        aui_begin_frame(ui, dt);

        /* Main panel with dialog buttons */
        if (aui_begin_panel(ui, "Dialog Examples", 50, 50, 350, 620,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            aui_label(ui, "Message Dialogs");
            aui_separator(ui);

            if (aui_button(ui, "Show Alert (OK)")) {
                aui_dialog_alert(ui, "Alert", "This is a simple alert message.");
            }

            if (aui_button(ui, "Show Message (OK/Cancel)")) {
                aui_dialog_message(ui, "Save Changes?",
                    "Do you want to save your changes before closing?",
                    AUI_BUTTONS_OK_CANCEL, on_message_closed, NULL);
            }

            if (aui_button(ui, "Show Confirm (Yes/No)")) {
                aui_dialog_confirm(ui, "Delete Item",
                    "Are you sure you want to delete this item?\nThis action cannot be undone.",
                    on_confirm_result, NULL);
            }

            if (aui_button(ui, "Show Yes/No/Cancel")) {
                aui_dialog_message(ui, "Unsaved Changes",
                    "You have unsaved changes. Save before closing?",
                    AUI_BUTTONS_YES_NO_CANCEL, on_message_closed, NULL);
            }

            aui_spacing(ui, 15);
            aui_label(ui, "Input Dialog");
            aui_separator(ui);

            if (aui_button(ui, "Show Text Input")) {
                aui_dialog_input(ui, "Enter Name",
                    "Please enter your character name:",
                    "Hero", on_input_result, NULL);
            }

            aui_spacing(ui, 15);
            aui_label(ui, "File Dialogs");
            aui_separator(ui);

            if (aui_button(ui, "Open File...")) {
                aui_file_dialog_open(ui, "Open Image",
                    NULL, image_filters, 3, on_file_selected, NULL);
            }

            if (aui_button(ui, "Save File...")) {
                aui_file_dialog_save(ui, "Save Scene",
                    "untitled.scene", scene_filters, 2, on_file_selected, NULL);
            }

            if (aui_button(ui, "Select Folder...")) {
                aui_file_dialog_folder(ui, "Select Project Folder",
                    NULL, on_folder_selected, NULL);
            }

            aui_spacing(ui, 15);
            aui_label(ui, "Editor Scenario (Chained Dialogs)");
            aui_separator(ui);

            /* Show dirty state */
            char dirty_label[64];
            snprintf(dirty_label, sizeof(dirty_label), "Scene dirty: %s",
                     s_is_dirty ? "YES" : "no");
            aui_label(ui, dirty_label);

            if (aui_button(ui, "Make Scene Dirty")) {
                s_is_dirty = true;
                SDL_Log("Scene marked as dirty");
            }

            if (aui_button(ui, "Open Scene... (tests fix)")) {
                if (s_is_dirty) {
                    /* Show confirm dialog first, then file dialog in callback */
                    aui_dialog_confirm(ui, "Unsaved Changes",
                        "Save changes before opening another scene?",
                        on_unsaved_changes_result, NULL);
                } else {
                    /* No unsaved changes, go straight to file dialog */
                    aui_file_dialog_open(ui, "Open Scene",
                        NULL, s_scene_filters, 2, on_open_file_result, NULL);
                }
            }

            aui_end_panel(ui);
        }

        /* Notifications panel */
        if (aui_begin_panel(ui, "Notifications", 450, 50, 300, 320,
                           AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

            aui_label(ui, "Toast Notifications");
            aui_separator(ui);

            if (aui_button(ui, "Info Toast")) {
                aui_notify(ui, "This is an info message.", AUI_NOTIFY_INFO);
            }

            if (aui_button(ui, "Success Toast")) {
                aui_notify(ui, "Operation completed successfully!", AUI_NOTIFY_SUCCESS);
            }

            if (aui_button(ui, "Warning Toast")) {
                aui_notify(ui, "Warning: Low disk space.", AUI_NOTIFY_WARNING);
            }

            if (aui_button(ui, "Error Toast")) {
                aui_notify(ui, "Error: Failed to save file.", AUI_NOTIFY_ERROR);
            }

            aui_spacing(ui, 10);

            if (aui_button(ui, "Clear All")) {
                aui_notify_clear_all(ui);
            }

            aui_spacing(ui, 10);
            aui_label(ui, "Position:");

            const char *positions[] = {
                "Top Left", "Top Center", "Top Right",
                "Bottom Left", "Bottom Center", "Bottom Right"
            };
            /* Use static so the value persists across frames - the popup
               writes to this at end of frame, dropdown reads at start of next */
            static int pos_idx = (int)AUI_NOTIFY_TOP_RIGHT;
            if (aui_dropdown(ui, "##pos", &pos_idx, positions, 6)) {
                notify_pos = (AUI_NotifyPosition)pos_idx;
                aui_notify_set_position(ui, notify_pos);
            }

            aui_end_panel(ui);
        }

        /* Help panel */
        if (aui_begin_panel(ui, "Controls", 450, 390, 300, 100,
                           AUI_PANEL_BORDER)) {
            aui_label(ui, "Right-click: Context menu");
            aui_label(ui, "ESC: Quit");
            aui_end_panel(ui);
        }

        /* FPS display */
        if (aui_begin_panel(ui, "Info", 800, 50, 150, 60, AUI_PANEL_BORDER)) {
            char buf[32];
            snprintf(buf, sizeof(buf), "FPS: %.0f", 1.0f / dt);
            aui_label(ui, buf);
            aui_end_panel(ui);
        }

        /* Update dialogs and notifications */
        aui_dialogs_update(ui, dt);

        /* Render dialogs/notifications into UI batch before ending frame */
        aui_dialogs_render(ui);

        aui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            aui_upload(ui, cmd);

            if (agentite_begin_render_pass(engine, 0.12f, 0.12f, 0.15f, 1.0f)) {
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
