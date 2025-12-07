# Retained-Mode UI Node System Demo

This example demonstrates Carbon's Godot-inspired hybrid UI system, which provides a retained-mode scene tree on top of the immediate-mode UI.

## Features Demonstrated

- **Scene Tree**: Hierarchical UI nodes with parent-child relationships
- **Anchor-based Layout**: Godot-style anchors and offsets for responsive layouts
- **Signal System**: Connect callbacks to node events (clicked, value changed, etc.)
- **Tween Animations**: Fade, slide, scale, and shake animations with easing
- **Rich Styling**: Gradients, shadows, rounded corners, borders

## Building

```bash
# From the Carbon root directory
make examples/ui_node
```

## Running

```bash
./examples/ui_node/ui_node
```

## Controls

- **ESC**: Quit
- **F1**: Toggle main menu with slide animation
- **F2**: Shake the main menu panel
- **Click buttons**: See signal callbacks in console

## Key Concepts

### Scene Tree Structure

```
root (CUI_NODE_CONTROL)
├── title (CUI_NODE_LABEL)
├── main_menu (CUI_NODE_PANEL)
│   └── menu_buttons (CUI_NODE_VBOX)
│       ├── Start Game (CUI_NODE_BUTTON)
│       ├── Load Game (CUI_NODE_BUTTON)
│       ├── Settings (CUI_NODE_BUTTON)
│       └── Quit (CUI_NODE_BUTTON)
├── settings_panel (CUI_NODE_PANEL)
│   ├── settings_content (CUI_NODE_VBOX)
│   │   ├── audio_label (CUI_NODE_LABEL)
│   │   ├── volume (CUI_NODE_SLIDER)
│   │   ├── music (CUI_NODE_CHECKBOX)
│   │   └── ...
│   └── close_settings (CUI_NODE_BUTTON)
├── info_panel (CUI_NODE_PANEL)
└── status_bar (CUI_NODE_CONTAINER)
    └── fps_label (CUI_NODE_LABEL)
```

### Anchor Presets

The anchor system positions nodes relative to their parent:

```c
// Fill entire parent
cui_node_set_anchor_preset(node, CUI_ANCHOR_FULL_RECT);

// Center in parent with fixed size via offsets
cui_node_set_anchor_preset(node, CUI_ANCHOR_CENTER);
cui_node_set_offsets(node, -150, -100, 150, 100);  // 300x200 centered

// Attach to edge
cui_node_set_anchor_preset(node, CUI_ANCHOR_BOTTOM_LEFT);
cui_node_set_offsets(node, 20, -140, 220, -20);

// Span full width
cui_node_set_anchor_preset(node, CUI_ANCHOR_TOP_WIDE);
cui_node_set_offsets(node, 0, 0, 0, 30);  // 30px tall bar
```

### Signals

Connect callbacks to respond to user interactions:

```c
void on_button_clicked(CUI_Node *node, const CUI_Signal *signal, void *userdata)
{
    SDL_Log("Button clicked: %s", node->name);
}

// Connect the signal
cui_node_connect(button, CUI_SIGNAL_CLICKED, on_button_clicked, NULL);
```

### Tweens

Animate node properties with easing:

```c
// Fade in over 0.3 seconds
cui_tween_fade_in(tm, node, 0.3f);

// Slide in from left
cui_tween_slide_in(tm, node, CUI_DIR_LEFT, 0.5f);

// Custom property animation
cui_tween_property(tm, node, CUI_TWEEN_OPACITY, 1.0f, 0.5f, CUI_EASE_OUT_QUAD);
```

### Styling

Apply rich visual styles to nodes:

```c
CUI_Style style = cui_style_default();

// Gradient background
style.background = cui_bg_gradient(
    cui_gradient_linear(90.0f, 0x2A2A3AFF, 0x1A1A2AFF)
);

// Border with rounded corners
style.border = cui_border(2.0f, 0x5A5A7AFF);
style.corner_radius = cui_corners_uniform(12.0f);

// Drop shadow
style.shadow_count = 1;
style.shadows[0] = cui_shadow(0, 8, 20, 0x00000080);

cui_node_set_style(panel, &style);
```

## Hybrid Rendering

The retained-mode nodes integrate with the immediate-mode UI:

```c
// Each frame:
cui_scene_update(ui, root, dt);           // Update scene tree
cui_scene_process_event(ui, root, &ev);   // Handle events

cui_begin_frame(ui, dt);                  // Begin immediate-mode frame
// ... immediate-mode widgets if needed ...
cui_end_frame(ui);

cui_scene_render(ui, root);               // Render scene tree
cui_render(ui, cmd, pass);                // Final GPU render
```
