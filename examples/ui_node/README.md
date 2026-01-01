# Retained-Mode UI Node System Demo

This example demonstrates Agentite's Godot-inspired hybrid UI system, which provides a retained-mode scene tree on top of the immediate-mode UI.

## Features Demonstrated

- **Scene Tree**: Hierarchical UI nodes with parent-child relationships
- **Anchor-based Layout**: Godot-style anchors and offsets for responsive layouts
- **Signal System**: Connect callbacks to node events (clicked, value changed, etc.)
- **Tween Animations**: Fade, slide, scale, and shake animations with easing
- **Rich Styling**: Gradients, shadows, rounded corners, borders
- **Chart Widgets**: Line charts with animations and hover tracking
- **Rich Text**: BBCode-formatted text with animations
- **Gamepad Navigation**: D-pad focus navigation with automatic mode switching

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
- **F3**: Toggle charts panel
- **D-pad / Arrow keys**: Navigate focusable widgets (in gamepad mode)
- **A button / Enter**: Activate focused widget
- **Click buttons**: See signal callbacks in console

## Key Concepts

### Scene Tree Structure

```
root (AUI_NODE_CONTROL)
├── title (AUI_NODE_LABEL)
├── main_menu (AUI_NODE_PANEL)
│   └── menu_buttons (AUI_NODE_VBOX)
│       ├── Start Game (AUI_NODE_BUTTON)
│       ├── Load Game (AUI_NODE_BUTTON)
│       ├── Settings (AUI_NODE_BUTTON)
│       └── Quit (AUI_NODE_BUTTON)
├── settings_panel (AUI_NODE_PANEL)
│   ├── settings_content (AUI_NODE_VBOX)
│   │   ├── audio_label (AUI_NODE_LABEL)
│   │   ├── volume (AUI_NODE_SLIDER)
│   │   ├── music (AUI_NODE_CHECKBOX)
│   │   └── ...
│   └── close_settings (AUI_NODE_BUTTON)
├── info_panel (AUI_NODE_PANEL)
└── status_bar (AUI_NODE_CONTAINER)
    └── fps_label (AUI_NODE_LABEL)
```

### Anchor Presets

The anchor system positions nodes relative to their parent:

```c
// Fill entire parent
aui_node_set_anchor_preset(node, AUI_ANCHOR_FULL_RECT);

// Center in parent with fixed size via offsets
aui_node_set_anchor_preset(node, AUI_ANCHOR_CENTER);
aui_node_set_offsets(node, -150, -100, 150, 100);  // 300x200 centered

// Attach to edge
aui_node_set_anchor_preset(node, AUI_ANCHOR_BOTTOM_LEFT);
aui_node_set_offsets(node, 20, -140, 220, -20);

// Span full width
aui_node_set_anchor_preset(node, AUI_ANCHOR_TOP_WIDE);
aui_node_set_offsets(node, 0, 0, 0, 30);  // 30px tall bar
```

### Signals

Connect callbacks to respond to user interactions:

```c
void on_button_clicked(AUI_Node *node, const AUI_Signal *signal, void *userdata)
{
    SDL_Log("Button clicked: %s", node->name);
}

// Connect the signal
aui_node_connect(button, AUI_SIGNAL_CLICKED, on_button_clicked, NULL);
```

### Tweens

Animate node properties with easing:

```c
// Fade in over 0.3 seconds
aui_tween_fade_in(tm, node, 0.3f);

// Slide in from left
aui_tween_slide_in(tm, node, AUI_DIR_LEFT, 0.5f);

// Custom property animation
aui_tween_property(tm, node, AUI_TWEEN_OPACITY, 1.0f, 0.5f, AUI_EASE_OUT_QUAD);
```

### Styling

Apply rich visual styles to nodes:

```c
AUI_Style style = aui_style_default();

// Gradient background
style.background = aui_bg_gradient(
    aui_gradient_linear(90.0f, 0x2A2A3AFF, 0x1A1A2AFF)
);

// Border with rounded corners
style.border = aui_border(2.0f, 0x5A5A7AFF);
style.corner_radius = aui_corners_uniform(12.0f);

// Drop shadow
style.shadow_count = 1;
style.shadows[0] = aui_shadow(0, 8, 20, 0x00000080);

aui_node_set_style(panel, &style);
```

## Hybrid Rendering

The retained-mode nodes integrate with the immediate-mode UI:

```c
// Each frame:
aui_scene_update(ui, root, dt);           // Update scene tree
aui_scene_process_event(ui, root, &ev);   // Handle events

aui_begin_frame(ui, dt);                  // Begin immediate-mode frame
// ... immediate-mode widgets if needed ...
aui_end_frame(ui);

aui_scene_render(ui, root);               // Render scene tree
aui_render(ui, cmd, pass);                // Final GPU render
```
