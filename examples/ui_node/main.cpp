/**
 * Agentite Engine - Retained-Mode UI Node System Demo
 *
 * Demonstrates the Godot-inspired hybrid UI system with:
 * - Scene tree of UI nodes
 * - Anchor-based layout (Godot-style)
 * - Signal-based event handling
 * - Tween animations
 * - Rich styling with gradients, shadows, rounded corners
 */

#include "agentite/agentite.h"
#include "agentite/ui.h"
#include "agentite/ui_node.h"
#include "agentite/ui_style.h"
#include "agentite/ui_tween.h"
#include "agentite/input.h"
#include <stdio.h>
#include <math.h>

/* ============================================================================
 * Signal Callbacks
 * ============================================================================ */

static void on_button_clicked(AUI_Node *node, const AUI_Signal *signal, void *userdata)
{
    (void)signal;
    (void)userdata;
    SDL_Log("Button clicked: %s", node->name);
}

static void on_start_clicked(AUI_Node *node, const AUI_Signal *signal, void *userdata)
{
    (void)node;
    (void)signal;
    AUI_TweenManager *tm = (AUI_TweenManager *)userdata;

    /* Find the settings panel and animate it */
    AUI_Node *root = aui_node_get_root(node);
    AUI_Node *panel = aui_node_find(root, "settings_panel");
    if (panel) {
        aui_tween_fade_in(tm, panel, 0.3f);
        aui_tween_scale_pop(tm, panel, 0.3f);
        aui_node_set_visible(panel, true);
    }

    SDL_Log("Start Game clicked - showing settings panel");
}

static void on_close_clicked(AUI_Node *node, const AUI_Signal *signal, void *userdata)
{
    (void)signal;
    AUI_TweenManager *tm = (AUI_TweenManager *)userdata;

    /* Find parent panel and fade it out */
    AUI_Node *panel = node->parent;
    while (panel && panel->type != AUI_NODE_PANEL) {
        panel = panel->parent;
    }

    if (panel) {
        aui_tween_fade_out(tm, panel, 0.2f);
        SDL_Log("Closing panel: %s", panel->name);
    }
}

static void on_slider_changed(AUI_Node *node, const AUI_Signal *signal, void *userdata)
{
    (void)userdata;
    if (signal->type == AUI_SIGNAL_VALUE_CHANGED) {
        SDL_Log("Slider %s changed: %.2f -> %.2f",
                node->name,
                signal->float_change.old_value,
                signal->float_change.new_value);
    }
}

static void on_checkbox_toggled(AUI_Node *node, const AUI_Signal *signal, void *userdata)
{
    (void)userdata;
    if (signal->type == AUI_SIGNAL_TOGGLED) {
        SDL_Log("Checkbox %s toggled: %s -> %s",
                node->name,
                signal->bool_change.old_value ? "ON" : "OFF",
                signal->bool_change.new_value ? "ON" : "OFF");
    }
}

/* ============================================================================
 * Style Helpers
 * ============================================================================ */

static AUI_Style create_button_style(void)
{
    AUI_Style style = aui_style_default();
    style.background = aui_bg_solid(0x3A3A5AFF);
    style.background_hover = aui_bg_solid(0x4A4A7AFF);
    style.background_active = aui_bg_solid(0x2A2A4AFF);
    style.background_disabled = aui_bg_solid(0x2A2A3AFF);
    style.border = aui_border(1.0f, 0x5A5A8AFF);
    style.corner_radius = aui_corners_uniform(6.0f);
    style.text_color = 0xFFFFFFFF;
    style.text_color_hover = 0xFFFFFFFF;
    style.text_color_disabled = 0x888888FF;
    style.padding = aui_edges_uniform(8.0f);

    /* Enable smooth hover/active transitions */
    style.transition = aui_transition(0.15f, AUI_TRANS_EASE_OUT_QUAD);

    return style;
}

/* ============================================================================
 * UI Tree Construction
 * ============================================================================ */

static AUI_Node *create_main_menu(AUI_Context *ctx, AUI_TweenManager *tm)
{
    /* Create reusable button style */
    AUI_Style button_style = create_button_style();
    /* Root control - fills entire screen */
    AUI_Node *root = aui_node_create(ctx, AUI_NODE_CONTROL, "root");
    aui_node_set_anchor_preset(root, AUI_ANCHOR_FULL_RECT);

    /* ========== Title at top center ========== */
    AUI_Node *title = aui_label_create(ctx, "title", "Agentite UI Node Demo");
    aui_node_set_anchor_preset(title, AUI_ANCHOR_TOP_WIDE);
    aui_node_set_offsets(title, 0, 30, 0, 60);
    aui_node_set_h_size_flags(title, AUI_SIZE_SHRINK_CENTER);
    title->label.color = 0xFFD700FF;  /* Gold */
    aui_node_add_child(root, title);

    /* ========== Main menu panel - centered ========== */
    AUI_Node *menu_panel = aui_panel_create(ctx, "main_menu", "Main Menu");
    aui_node_set_anchor_preset(menu_panel, AUI_ANCHOR_CENTER);
    aui_node_set_offsets(menu_panel, -150, -180, 150, 120);

    /* Style the panel with solid background */
    AUI_Style panel_style = aui_style_default();
    panel_style.background = aui_bg_solid(0x2A2A3AFF);
    panel_style.border = aui_border(2.0f, 0x6A6A8AFF);
    panel_style.corner_radius = aui_corners_uniform(12.0f);
    panel_style.text_color = 0xFFFFFFFF;
    aui_node_set_style(menu_panel, &panel_style);
    aui_node_add_child(root, menu_panel);

    /* VBox for menu buttons */
    AUI_Node *menu_vbox = aui_vbox_create(ctx, "menu_buttons");
    aui_node_set_anchor_preset(menu_vbox, AUI_ANCHOR_FULL_RECT);
    aui_node_set_offsets(menu_vbox, 20, 20, -20, -20);  /* 20px padding all sides */
    aui_box_set_separation(menu_vbox, 12.0f);
    aui_node_add_child(menu_panel, menu_vbox);

    /* Menu buttons */
    const char *button_labels[] = {"Start Game", "Load Game", "Settings", "Quit"};
    for (int i = 0; i < 4; i++) {
        AUI_Node *btn = aui_button_create(ctx, button_labels[i], button_labels[i]);
        aui_node_set_h_size_flags(btn, AUI_SIZE_FILL);
        aui_node_set_custom_min_size(btn, 0, 40);
        aui_node_set_style(btn, &button_style);
        aui_node_add_child(menu_vbox, btn);

        if (i == 0) {
            /* Start Game button opens settings panel */
            aui_node_connect(btn, AUI_SIGNAL_CLICKED, on_start_clicked, tm);
        } else {
            aui_node_connect(btn, AUI_SIGNAL_CLICKED, on_button_clicked, NULL);
        }
    }

    /* ========== Settings panel - starts hidden ========== */
    AUI_Node *settings = aui_panel_create(ctx, "settings_panel", "Settings");
    aui_node_set_anchor_preset(settings, AUI_ANCHOR_CENTER_RIGHT);
    aui_node_set_offsets(settings, -320, -200, -20, 200);
    aui_node_set_visible(settings, false);
    settings->opacity = 0.0f;

    /* Style settings panel */
    AUI_Style settings_style = aui_style_default();
    settings_style.background = aui_bg_solid(0x3A3A4AFF);
    settings_style.border = aui_border(2.0f, 0x5A5A7AFF);
    settings_style.corner_radius = aui_corners_uniform(8.0f);
    settings_style.text_color = 0xFFFFFFFF;
    aui_node_set_style(settings, &settings_style);
    aui_node_add_child(root, settings);

    /* Settings content VBox */
    AUI_Node *settings_vbox = aui_vbox_create(ctx, "settings_content");
    aui_node_set_anchor_preset(settings_vbox, AUI_ANCHOR_FULL_RECT);
    aui_node_set_offsets(settings_vbox, 15, 45, -15, -50);
    aui_box_set_separation(settings_vbox, 10.0f);
    aui_node_add_child(settings, settings_vbox);

    /* Audio section label */
    AUI_Node *audio_label = aui_label_create(ctx, "audio_label", "Audio");
    audio_label->label.color = 0xAAAAAAFF;
    aui_node_add_child(settings_vbox, audio_label);

    /* Volume slider */
    AUI_Node *volume = aui_node_create(ctx, AUI_NODE_SLIDER, "volume");
    volume->slider.value = 0.75f;
    volume->slider.min_value = 0.0f;
    volume->slider.max_value = 1.0f;
    volume->slider.show_value = true;
    aui_node_set_h_size_flags(volume, AUI_SIZE_FILL);
    aui_node_set_custom_min_size(volume, 0, 24);
    aui_node_connect(volume, AUI_SIGNAL_VALUE_CHANGED, on_slider_changed, NULL);
    aui_node_add_child(settings_vbox, volume);

    /* Checkboxes */
    AUI_Node *music_cb = aui_node_create(ctx, AUI_NODE_CHECKBOX, "music");
    music_cb->checkbox.checked = true;
    strncpy(music_cb->checkbox.text, "Enable Music", sizeof(music_cb->checkbox.text) - 1);
    aui_node_connect(music_cb, AUI_SIGNAL_TOGGLED, on_checkbox_toggled, NULL);
    aui_node_add_child(settings_vbox, music_cb);

    AUI_Node *sfx_cb = aui_node_create(ctx, AUI_NODE_CHECKBOX, "sfx");
    sfx_cb->checkbox.checked = true;
    strncpy(sfx_cb->checkbox.text, "Enable Sound FX", sizeof(sfx_cb->checkbox.text) - 1);
    aui_node_connect(sfx_cb, AUI_SIGNAL_TOGGLED, on_checkbox_toggled, NULL);
    aui_node_add_child(settings_vbox, sfx_cb);

    /* Graphics section label */
    AUI_Node *graphics_label = aui_label_create(ctx, "graphics_label", "Graphics");
    graphics_label->label.color = 0xAAAAAAFF;
    aui_node_add_child(settings_vbox, graphics_label);

    /* Brightness slider */
    AUI_Node *brightness = aui_node_create(ctx, AUI_NODE_SLIDER, "brightness");
    brightness->slider.value = 0.5f;
    brightness->slider.min_value = 0.0f;
    brightness->slider.max_value = 1.0f;
    aui_node_set_h_size_flags(brightness, AUI_SIZE_FILL);
    aui_node_set_custom_min_size(brightness, 0, 24);
    aui_node_connect(brightness, AUI_SIGNAL_VALUE_CHANGED, on_slider_changed, NULL);
    aui_node_add_child(settings_vbox, brightness);

    /* Close button at bottom of settings - positioned inside panel's rounded corners */
    AUI_Node *close_btn = aui_button_create(ctx, "close_settings", "Close");
    aui_node_set_anchor_preset(close_btn, AUI_ANCHOR_BOTTOM_CENTER);
    aui_node_set_offsets(close_btn, -50, -45, 50, -15);  /* 15px from bottom to clear rounded corners */
    aui_node_set_style(close_btn, &button_style);
    aui_node_connect(close_btn, AUI_SIGNAL_CLICKED, on_close_clicked, tm);
    aui_node_add_child(settings, close_btn);

    /* ========== Info panel at bottom left ========== */
    AUI_Node *info_panel = aui_panel_create(ctx, "info_panel", "Controls");
    aui_node_set_anchor_preset(info_panel, AUI_ANCHOR_BOTTOM_LEFT);
    aui_node_set_offsets(info_panel, 20, -140, 220, -20);

    AUI_Style info_style = aui_style_default();
    info_style.background = aui_bg_solid(0x1A1A2AFF);
    info_style.border = aui_border(1.0f, 0x3A3A5AFF);
    info_style.corner_radius = aui_corners_uniform(6.0f);
    info_style.text_color = 0xCCCCCCFF;
    aui_node_set_style(info_panel, &info_style);
    aui_node_add_child(root, info_panel);

    /* Info content */
    AUI_Node *info_vbox = aui_vbox_create(ctx, "info_content");
    aui_node_set_anchor_preset(info_vbox, AUI_ANCHOR_FULL_RECT);
    aui_node_set_offsets(info_vbox, 10, 35, -10, -10);
    aui_box_set_separation(info_vbox, 4.0f);
    aui_node_add_child(info_panel, info_vbox);

    const char *info_lines[] = {
        "ESC: Quit",
        "F1: Toggle Menu",
        "F2: Animate Panel"
    };
    for (int i = 0; i < 3; i++) {
        AUI_Node *line = aui_label_create(ctx, NULL, info_lines[i]);
        line->label.color = 0xCCCCCCFF;
        aui_node_add_child(info_vbox, line);
    }

    /* ========== Status bar at top (wide anchor) ========== */
    AUI_Node *status_bar = aui_node_create(ctx, AUI_NODE_CONTAINER, "status_bar");
    aui_node_set_anchor_preset(status_bar, AUI_ANCHOR_TOP_WIDE);
    aui_node_set_offsets(status_bar, 0, 0, 0, 25);

    AUI_Style bar_style = aui_style_default();
    bar_style.background = aui_bg_solid(0x1A1A2AFF);
    bar_style.text_color = 0x88FF88FF;
    aui_node_set_style(status_bar, &bar_style);
    aui_node_add_child(root, status_bar);

    /* FPS label in status bar */
    AUI_Node *fps_label = aui_label_create(ctx, "fps_label", "FPS: --");
    aui_node_set_anchor_preset(fps_label, AUI_ANCHOR_CENTER_RIGHT);
    aui_node_set_offsets(fps_label, -80, -10, -10, 10);
    fps_label->label.color = 0x88FF88FF;
    aui_node_add_child(status_bar, fps_label);

    return root;
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    Agentite_Config config = {
        .window_title = "Agentite - Retained-Mode UI Demo",
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
        fprintf(stderr, "Failed to initialize UI (ensure font exists)\n");
        agentite_shutdown(engine);
        return 1;
    }

    /* Set DPI scale for input coordinate conversion (logical coords used throughout) */
    float dpi_scale = agentite_get_dpi_scale(engine);
    aui_set_dpi_scale(ui, dpi_scale);

    /* Initialize tween manager for animations */
    AUI_TweenManager *tweens = aui_tween_manager_create();

    /* Initialize input */
    Agentite_Input *input = agentite_input_init();

    /* Create the UI scene tree */
    AUI_Node *ui_root = create_main_menu(ui, tweens);

    /* Demo state */
    bool show_menu = true;

    SDL_Log("UI Node demo initialized");
    SDL_Log("  Root node: %s (id=%u)", ui_root->name, ui_root->id);
    SDL_Log("  Child count: %d", ui_root->child_count);

    /* Force initial layout and debug */
    aui_scene_layout(ui, ui_root);
    SDL_Log("  Root rect: (%.0f, %.0f, %.0f, %.0f)",
            ui_root->global_rect.x, ui_root->global_rect.y,
            ui_root->global_rect.w, ui_root->global_rect.h);

    AUI_Node *menu = aui_node_find(ui_root, "main_menu");
    if (menu) {
        SDL_Log("  Menu rect: (%.0f, %.0f, %.0f, %.0f)",
                menu->global_rect.x, menu->global_rect.y,
                menu->global_rect.w, menu->global_rect.h);
        SDL_Log("  Menu bg type: %d (SOLID=%d)", menu->style.background.type, AUI_BG_SOLID);
        SDL_Log("  Menu visible: %d, opacity: %.2f", menu->visible, menu->opacity);

        /* Check first button */
        AUI_Node *btn = aui_node_find(ui_root, "Start Game");
        if (btn) {
            SDL_Log("  Button rect: (%.0f, %.0f, %.0f, %.0f)",
                    btn->global_rect.x, btn->global_rect.y,
                    btn->global_rect.w, btn->global_rect.h);
            SDL_Log("  Button bg type: %d", btn->style.background.type);
        }
    }

    while (agentite_is_running(engine)) {
        agentite_begin_frame(engine);
        float dt = agentite_get_delta_time(engine);

        agentite_input_begin_frame(input);

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            /* Always let input system see the event first for global shortcuts */
            agentite_input_process_event(input, &event);

            /* Process events through the UI scene tree */
            aui_scene_process_event(ui, ui_root, &event);

            if (event.type == SDL_EVENT_QUIT) {
                agentite_quit(engine);
            }
        }

        agentite_input_update(input);

        /* Handle key shortcuts */
        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_ESCAPE)) {
            agentite_quit(engine);
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F1)) {
            /* Toggle main menu visibility with animation */
            AUI_Node *menu = aui_node_find(ui_root, "main_menu");
            if (menu) {
                show_menu = !show_menu;
                if (show_menu) {
                    aui_node_set_visible(menu, true);
                    aui_tween_fade_in(tweens, menu, 0.25f);
                    aui_tween_slide_in(tweens, menu, AUI_DIR_LEFT, 0.25f);
                } else {
                    aui_tween_fade_out(tweens, menu, 0.2f);
                    aui_tween_slide_out(tweens, menu, AUI_DIR_LEFT, 0.2f);
                }
            }
        }

        if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F2)) {
            /* Trigger a shake animation on the main menu */
            AUI_Node *menu = aui_node_find(ui_root, "main_menu");
            if (menu) {
                aui_tween_shake(tweens, menu, 10.0f, 0.3f);
            }
        }

        /* Update tweens */
        aui_tween_manager_update(tweens, dt);

        /* Update the scene tree (processes layout, state, etc.) */
        aui_scene_update(ui, ui_root, dt);

        /* Update FPS display */
        AUI_Node *fps_label = aui_node_find(ui_root, "fps_label");
        if (fps_label && fps_label->type == AUI_NODE_LABEL) {
            char fps_text[32];
            snprintf(fps_text, sizeof(fps_text), "FPS: %.0f", 1.0f / dt);
            aui_label_set_text(fps_label, fps_text);
        }

        /* Begin immediate-mode frame for hybrid rendering */
        aui_begin_frame(ui, dt);

        /* Render the retained-mode UI scene tree */
        aui_scene_render(ui, ui_root);

        aui_end_frame(ui);

        /* Render */
        SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
        if (cmd) {
            /* Upload UI draw data to GPU */
            aui_upload(ui, cmd);

            if (agentite_begin_render_pass(engine, 0.08f, 0.08f, 0.12f, 1.0f)) {
                SDL_GPURenderPass *pass = agentite_get_render_pass(engine);

                /* Render UI (both immediate-mode and scene tree use same context) */
                aui_render(ui, cmd, pass);

                agentite_end_render_pass(engine);
            }
        }

        agentite_end_frame(engine);
    }

    /* Cleanup */
    aui_node_destroy(ui_root);
    aui_tween_manager_destroy(tweens);
    agentite_input_shutdown(input);
    aui_shutdown(ui);
    agentite_shutdown(engine);

    return 0;
}
