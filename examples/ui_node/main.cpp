/**
 * Carbon Engine - Retained-Mode UI Node System Demo
 *
 * Demonstrates the Godot-inspired hybrid UI system with:
 * - Scene tree of UI nodes
 * - Anchor-based layout (Godot-style)
 * - Signal-based event handling
 * - Tween animations
 * - Rich styling with gradients, shadows, rounded corners
 */

#include "carbon/carbon.h"
#include "carbon/ui.h"
#include "carbon/ui_node.h"
#include "carbon/ui_style.h"
#include "carbon/ui_tween.h"
#include "carbon/input.h"
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Signal Callbacks
 * ============================================================================ */

static void on_button_clicked(CUI_Node *node, const CUI_Signal *signal, void *userdata)
{
    (void)signal;
    (void)userdata;
    SDL_Log("Button clicked: %s", node->name);
}

static void on_start_clicked(CUI_Node *node, const CUI_Signal *signal, void *userdata)
{
    (void)node;
    (void)signal;
    CUI_TweenManager *tm = (CUI_TweenManager *)userdata;

    /* Find the settings panel and animate it */
    CUI_Node *root = cui_node_get_root(node);
    CUI_Node *panel = cui_node_find(root, "settings_panel");
    if (panel) {
        cui_tween_fade_in(tm, panel, 0.3f);
        cui_tween_scale_pop(tm, panel, 0.3f);
        cui_node_set_visible(panel, true);
    }

    SDL_Log("Start Game clicked - showing settings panel");
}

static void on_close_clicked(CUI_Node *node, const CUI_Signal *signal, void *userdata)
{
    (void)signal;
    CUI_TweenManager *tm = (CUI_TweenManager *)userdata;

    /* Find parent panel and fade it out */
    CUI_Node *panel = node->parent;
    while (panel && panel->type != CUI_NODE_PANEL) {
        panel = panel->parent;
    }

    if (panel) {
        cui_tween_fade_out(tm, panel, 0.2f);
        SDL_Log("Closing panel: %s", panel->name);
    }
}

static void on_slider_changed(CUI_Node *node, const CUI_Signal *signal, void *userdata)
{
    (void)userdata;
    if (signal->type == CUI_SIGNAL_VALUE_CHANGED) {
        SDL_Log("Slider %s changed: %.2f -> %.2f",
                node->name,
                signal->float_change.old_value,
                signal->float_change.new_value);
    }
}

static void on_checkbox_toggled(CUI_Node *node, const CUI_Signal *signal, void *userdata)
{
    (void)userdata;
    if (signal->type == CUI_SIGNAL_TOGGLED) {
        SDL_Log("Checkbox %s toggled: %s -> %s",
                node->name,
                signal->bool_change.old_value ? "ON" : "OFF",
                signal->bool_change.new_value ? "ON" : "OFF");
    }
}

/* ============================================================================
 * Style Helpers
 * ============================================================================ */

static CUI_Style create_button_style(void)
{
    CUI_Style style = cui_style_default();
    style.background = cui_bg_solid(0x3A3A5AFF);
    style.background_hover = cui_bg_solid(0x4A4A7AFF);
    style.background_active = cui_bg_solid(0x2A2A4AFF);
    style.background_disabled = cui_bg_solid(0x2A2A3AFF);
    style.border = cui_border(1.0f, 0x5A5A8AFF);
    style.corner_radius = cui_corners_uniform(6.0f);
    style.text_color = 0xFFFFFFFF;
    style.text_color_hover = 0xFFFFFFFF;
    style.text_color_disabled = 0x888888FF;
    style.padding = cui_edges_uniform(8.0f);

    /* Enable smooth hover/active transitions */
    style.transition = cui_transition(0.15f, CUI_TRANS_EASE_OUT_QUAD);

    return style;
}

/* ============================================================================
 * UI Tree Construction
 * ============================================================================ */

static CUI_Node *create_main_menu(CUI_Context *ctx, CUI_TweenManager *tm)
{
    /* Create reusable button style */
    CUI_Style button_style = create_button_style();
    /* Root control - fills entire screen */
    CUI_Node *root = cui_node_create(ctx, CUI_NODE_CONTROL, "root");
    cui_node_set_anchor_preset(root, CUI_ANCHOR_FULL_RECT);

    /* ========== Title at top center ========== */
    CUI_Node *title = cui_label_create(ctx, "title", "Carbon UI Node Demo");
    cui_node_set_anchor_preset(title, CUI_ANCHOR_TOP_WIDE);
    cui_node_set_offsets(title, 0, 30, 0, 60);
    cui_node_set_h_size_flags(title, CUI_SIZE_SHRINK_CENTER);
    title->label.color = 0xFFD700FF;  /* Gold */
    cui_node_add_child(root, title);

    /* ========== Main menu panel - centered ========== */
    CUI_Node *menu_panel = cui_panel_create(ctx, "main_menu", "Main Menu");
    cui_node_set_anchor_preset(menu_panel, CUI_ANCHOR_CENTER);
    cui_node_set_offsets(menu_panel, -150, -180, 150, 120);

    /* Style the panel with solid background */
    CUI_Style panel_style = cui_style_default();
    panel_style.background = cui_bg_solid(0x2A2A3AFF);
    panel_style.border = cui_border(2.0f, 0x6A6A8AFF);
    panel_style.corner_radius = cui_corners_uniform(12.0f);
    panel_style.text_color = 0xFFFFFFFF;
    cui_node_set_style(menu_panel, &panel_style);
    cui_node_add_child(root, menu_panel);

    /* VBox for menu buttons */
    CUI_Node *menu_vbox = cui_vbox_create(ctx, "menu_buttons");
    cui_node_set_anchor_preset(menu_vbox, CUI_ANCHOR_FULL_RECT);
    cui_node_set_offsets(menu_vbox, 20, 20, -20, -20);  /* 20px padding all sides */
    cui_box_set_separation(menu_vbox, 12.0f);
    cui_node_add_child(menu_panel, menu_vbox);

    /* Menu buttons */
    const char *button_labels[] = {"Start Game", "Load Game", "Settings", "Quit"};
    for (int i = 0; i < 4; i++) {
        CUI_Node *btn = cui_button_create(ctx, button_labels[i], button_labels[i]);
        cui_node_set_h_size_flags(btn, CUI_SIZE_FILL);
        cui_node_set_custom_min_size(btn, 0, 40);
        cui_node_set_style(btn, &button_style);
        cui_node_add_child(menu_vbox, btn);

        if (i == 0) {
            /* Start Game button opens settings panel */
            cui_node_connect(btn, CUI_SIGNAL_CLICKED, on_start_clicked, tm);
        } else {
            cui_node_connect(btn, CUI_SIGNAL_CLICKED, on_button_clicked, NULL);
        }
    }

    /* ========== Settings panel - starts hidden ========== */
    CUI_Node *settings = cui_panel_create(ctx, "settings_panel", "Settings");
    cui_node_set_anchor_preset(settings, CUI_ANCHOR_CENTER_RIGHT);
    cui_node_set_offsets(settings, -320, -200, -20, 200);
    cui_node_set_visible(settings, false);
    settings->opacity = 0.0f;

    /* Style settings panel */
    CUI_Style settings_style = cui_style_default();
    settings_style.background = cui_bg_solid(0x3A3A4AFF);
    settings_style.border = cui_border(2.0f, 0x5A5A7AFF);
    settings_style.corner_radius = cui_corners_uniform(8.0f);
    settings_style.text_color = 0xFFFFFFFF;
    cui_node_set_style(settings, &settings_style);
    cui_node_add_child(root, settings);

    /* Settings content VBox */
    CUI_Node *settings_vbox = cui_vbox_create(ctx, "settings_content");
    cui_node_set_anchor_preset(settings_vbox, CUI_ANCHOR_FULL_RECT);
    cui_node_set_offsets(settings_vbox, 15, 45, -15, -50);
    cui_box_set_separation(settings_vbox, 10.0f);
    cui_node_add_child(settings, settings_vbox);

    /* Audio section label */
    CUI_Node *audio_label = cui_label_create(ctx, "audio_label", "Audio");
    audio_label->label.color = 0xAAAAAAFF;
    cui_node_add_child(settings_vbox, audio_label);

    /* Volume slider */
    CUI_Node *volume = cui_node_create(ctx, CUI_NODE_SLIDER, "volume");
    volume->slider.value = 0.75f;
    volume->slider.min_value = 0.0f;
    volume->slider.max_value = 1.0f;
    volume->slider.show_value = true;
    cui_node_set_h_size_flags(volume, CUI_SIZE_FILL);
    cui_node_set_custom_min_size(volume, 0, 24);
    cui_node_connect(volume, CUI_SIGNAL_VALUE_CHANGED, on_slider_changed, NULL);
    cui_node_add_child(settings_vbox, volume);

    /* Checkboxes */
    CUI_Node *music_cb = cui_node_create(ctx, CUI_NODE_CHECKBOX, "music");
    music_cb->checkbox.checked = true;
    strncpy(music_cb->checkbox.text, "Enable Music", sizeof(music_cb->checkbox.text) - 1);
    cui_node_connect(music_cb, CUI_SIGNAL_TOGGLED, on_checkbox_toggled, NULL);
    cui_node_add_child(settings_vbox, music_cb);

    CUI_Node *sfx_cb = cui_node_create(ctx, CUI_NODE_CHECKBOX, "sfx");
    sfx_cb->checkbox.checked = true;
    strncpy(sfx_cb->checkbox.text, "Enable Sound FX", sizeof(sfx_cb->checkbox.text) - 1);
    cui_node_connect(sfx_cb, CUI_SIGNAL_TOGGLED, on_checkbox_toggled, NULL);
    cui_node_add_child(settings_vbox, sfx_cb);

    /* Graphics section label */
    CUI_Node *graphics_label = cui_label_create(ctx, "graphics_label", "Graphics");
    graphics_label->label.color = 0xAAAAAAFF;
    cui_node_add_child(settings_vbox, graphics_label);

    /* Brightness slider */
    CUI_Node *brightness = cui_node_create(ctx, CUI_NODE_SLIDER, "brightness");
    brightness->slider.value = 0.5f;
    brightness->slider.min_value = 0.0f;
    brightness->slider.max_value = 1.0f;
    cui_node_set_h_size_flags(brightness, CUI_SIZE_FILL);
    cui_node_set_custom_min_size(brightness, 0, 24);
    cui_node_connect(brightness, CUI_SIGNAL_VALUE_CHANGED, on_slider_changed, NULL);
    cui_node_add_child(settings_vbox, brightness);

    /* Close button at bottom of settings - positioned inside panel's rounded corners */
    CUI_Node *close_btn = cui_button_create(ctx, "close_settings", "Close");
    cui_node_set_anchor_preset(close_btn, CUI_ANCHOR_BOTTOM_CENTER);
    cui_node_set_offsets(close_btn, -50, -45, 50, -15);  /* 15px from bottom to clear rounded corners */
    cui_node_set_style(close_btn, &button_style);
    cui_node_connect(close_btn, CUI_SIGNAL_CLICKED, on_close_clicked, tm);
    cui_node_add_child(settings, close_btn);

    /* ========== Info panel at bottom left ========== */
    CUI_Node *info_panel = cui_panel_create(ctx, "info_panel", "Controls");
    cui_node_set_anchor_preset(info_panel, CUI_ANCHOR_BOTTOM_LEFT);
    cui_node_set_offsets(info_panel, 20, -140, 220, -20);

    CUI_Style info_style = cui_style_default();
    info_style.background = cui_bg_solid(0x1A1A2AFF);
    info_style.border = cui_border(1.0f, 0x3A3A5AFF);
    info_style.corner_radius = cui_corners_uniform(6.0f);
    info_style.text_color = 0xCCCCCCFF;
    cui_node_set_style(info_panel, &info_style);
    cui_node_add_child(root, info_panel);

    /* Info content */
    CUI_Node *info_vbox = cui_vbox_create(ctx, "info_content");
    cui_node_set_anchor_preset(info_vbox, CUI_ANCHOR_FULL_RECT);
    cui_node_set_offsets(info_vbox, 10, 35, -10, -10);
    cui_box_set_separation(info_vbox, 4.0f);
    cui_node_add_child(info_panel, info_vbox);

    const char *info_lines[] = {
        "ESC: Quit",
        "F1: Toggle Menu",
        "F2: Animate Panel"
    };
    for (int i = 0; i < 3; i++) {
        CUI_Node *line = cui_label_create(ctx, NULL, info_lines[i]);
        line->label.color = 0xCCCCCCFF;
        cui_node_add_child(info_vbox, line);
    }

    /* ========== Status bar at top (wide anchor) ========== */
    CUI_Node *status_bar = cui_node_create(ctx, CUI_NODE_CONTAINER, "status_bar");
    cui_node_set_anchor_preset(status_bar, CUI_ANCHOR_TOP_WIDE);
    cui_node_set_offsets(status_bar, 0, 0, 0, 25);

    CUI_Style bar_style = cui_style_default();
    bar_style.background = cui_bg_solid(0x1A1A2AFF);
    bar_style.text_color = 0x88FF88FF;
    cui_node_set_style(status_bar, &bar_style);
    cui_node_add_child(root, status_bar);

    /* FPS label in status bar */
    CUI_Node *fps_label = cui_label_create(ctx, "fps_label", "FPS: --");
    cui_node_set_anchor_preset(fps_label, CUI_ANCHOR_CENTER_RIGHT);
    cui_node_set_offsets(fps_label, -80, -10, -10, 10);
    fps_label->label.color = 0x88FF88FF;
    cui_node_add_child(status_bar, fps_label);

    return root;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    Carbon_Config config = {
        .window_title = "Carbon - Retained-Mode UI Demo",
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
        fprintf(stderr, "Failed to initialize UI (ensure font exists)\n");
        carbon_shutdown(engine);
        return 1;
    }

    /* Initialize tween manager for animations */
    CUI_TweenManager *tweens = cui_tween_manager_create();

    /* Initialize input */
    Carbon_Input *input = carbon_input_init();

    /* Create the UI scene tree */
    CUI_Node *ui_root = create_main_menu(ui, tweens);

    /* Demo state */
    bool show_menu = true;

    SDL_Log("UI Node demo initialized");
    SDL_Log("  Root node: %s (id=%u)", ui_root->name, ui_root->id);
    SDL_Log("  Child count: %d", ui_root->child_count);

    /* Force initial layout and debug */
    cui_scene_layout(ui, ui_root);
    SDL_Log("  Root rect: (%.0f, %.0f, %.0f, %.0f)",
            ui_root->global_rect.x, ui_root->global_rect.y,
            ui_root->global_rect.w, ui_root->global_rect.h);

    CUI_Node *menu = cui_node_find(ui_root, "main_menu");
    if (menu) {
        SDL_Log("  Menu rect: (%.0f, %.0f, %.0f, %.0f)",
                menu->global_rect.x, menu->global_rect.y,
                menu->global_rect.w, menu->global_rect.h);
        SDL_Log("  Menu bg type: %d (SOLID=%d)", menu->style.background.type, CUI_BG_SOLID);
        SDL_Log("  Menu visible: %d, opacity: %.2f", menu->visible, menu->opacity);

        /* Check first button */
        CUI_Node *btn = cui_node_find(ui_root, "Start Game");
        if (btn) {
            SDL_Log("  Button rect: (%.0f, %.0f, %.0f, %.0f)",
                    btn->global_rect.x, btn->global_rect.y,
                    btn->global_rect.w, btn->global_rect.h);
            SDL_Log("  Button bg type: %d", btn->style.background.type);
        }
    }

    while (carbon_is_running(engine)) {
        carbon_begin_frame(engine);
        float dt = carbon_get_delta_time(engine);

        carbon_input_begin_frame(input);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Always let input system see the event first for global shortcuts */
            carbon_input_process_event(input, &event);

            /* Process events through the UI scene tree */
            cui_scene_process_event(ui, ui_root, &event);

            if (event.type == SDL_EVENT_QUIT) {
                carbon_quit(engine);
            }
        }

        carbon_input_update(input);

        /* Handle key shortcuts */
        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            carbon_quit(engine);
        }

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_F1)) {
            /* Toggle main menu visibility with animation */
            CUI_Node *menu = cui_node_find(ui_root, "main_menu");
            if (menu) {
                show_menu = !show_menu;
                if (show_menu) {
                    cui_node_set_visible(menu, true);
                    cui_tween_fade_in(tweens, menu, 0.25f);
                    cui_tween_slide_in(tweens, menu, CUI_DIR_LEFT, 0.25f);
                } else {
                    cui_tween_fade_out(tweens, menu, 0.2f);
                    cui_tween_slide_out(tweens, menu, CUI_DIR_LEFT, 0.2f);
                }
            }
        }

        if (carbon_input_key_just_pressed(input, SDL_SCANCODE_F2)) {
            /* Trigger a shake animation on the main menu */
            CUI_Node *menu = cui_node_find(ui_root, "main_menu");
            if (menu) {
                cui_tween_shake(tweens, menu, 10.0f, 0.3f);
            }
        }

        /* Update tweens */
        cui_tween_manager_update(tweens, dt);

        /* Update the scene tree (processes layout, state, etc.) */
        cui_scene_update(ui, ui_root, dt);

        /* Update FPS display */
        CUI_Node *fps_label = cui_node_find(ui_root, "fps_label");
        if (fps_label && fps_label->type == CUI_NODE_LABEL) {
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.0f", 1.0f / dt);
            cui_label_set_text(fps_label, fps_text);
        }

        /* Begin immediate-mode frame for hybrid rendering */
        cui_begin_frame(ui, dt);

        /* Render the retained-mode UI scene tree */
        cui_scene_render(ui, ui_root);

        cui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload UI draw data to GPU */
            cui_upload(ui, cmd);

            if (carbon_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(engine);

                /* Render UI (both immediate-mode and scene tree use same context) */
                cui_render(ui, cmd, pass);

                carbon_end_render_pass(engine);
            }
        }

        carbon_end_frame(engine);
    }

    /* Cleanup */
    cui_node_destroy(ui_root);
    cui_tween_manager_destroy(tweens);
    carbon_input_shutdown(input);
    cui_shutdown(ui);
    carbon_shutdown(engine);

    return 0;
}
