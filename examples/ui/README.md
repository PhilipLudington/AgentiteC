# UI Example

Demonstrates the immediate-mode UI system.

## What This Demonstrates

- Panels with title bars and borders
- Buttons, checkboxes, sliders
- Dropdowns and listboxes
- Text input (textbox)
- Labels and separators
- Progress bars
- Spinbox widgets (integer and float)
- Tooltips on hover
- Gamepad mode detection
- UI event handling

## Running

```bash
make example-ui
```

## Controls

| Key | Action |
|-----|--------|
| F1 | Toggle Settings panel |
| F2 | Toggle Character panel |
| F3 | Toggle Debug panel |
| F4 | Toggle New Widgets panel |
| Escape | Quit |

## Key Patterns

### UI Frame Structure
```c
// Begin UI frame
aui_begin_frame(ui, delta_time);

// Draw widgets inside panels
if (aui_begin_panel(ui, "Panel Name", x, y, width, height, flags)) {
    aui_label(ui, "Hello!");
    if (aui_button(ui, "Click Me")) {
        // Button was clicked
    }
    aui_end_panel(ui);
}

// End UI frame
aui_end_frame(ui);

// Upload and render (during GPU operations)
aui_upload(ui, cmd);
// ... during render pass ...
aui_render(ui, cmd, pass);
```

### Panel Flags
```c
AUI_PANEL_NONE        // No decorations
AUI_PANEL_TITLE_BAR   // Show title bar
AUI_PANEL_BORDER      // Show border
AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER  // Both
```

### Widgets

```c
// Labels and text
aui_label(ui, "Static text");
aui_spacing(ui, 10);  // Vertical space
aui_separator(ui);    // Horizontal line

// Buttons
if (aui_button(ui, "Click Me")) {
    // Handle click
}

// Checkboxes
bool enabled = true;
aui_checkbox(ui, "Enable Feature", &enabled);

// Sliders
float volume = 0.5f;
aui_slider_float(ui, "Volume", &volume, 0.0f, 1.0f);

// Dropdowns
int selection = 0;
const char *options[] = {"Low", "Medium", "High"};
aui_dropdown(ui, "Quality", &selection, options, 3);

// Text input
char buffer[64] = "Default";
aui_textbox(ui, "Name", buffer, sizeof(buffer));

// Spinbox (integer)
int level = 1;
aui_spinbox_int(ui, "Level", &level, 1, 100, 1);

// Spinbox (float)
float speed = 5.0f;
aui_spinbox_float(ui, "Speed", &speed, 1.0f, 20.0f, 0.5f);

// Tooltip (attach to previous widget)
aui_tooltip(ui, "This is a helpful tooltip");

// Listbox
int selected = 0;
const char *items[] = {"Item 1", "Item 2", "Item 3"};
aui_listbox(ui, "##id", &selected, items, 3, 100);  // height=100

// Progress bar (standalone, not in panel)
aui_progress_bar(ui, value, min, max);
```

### Event Handling
```c
// UI processes events first
SDL_Event event;
while (SDL_PollEvent(&event)) {
    if (aui_process_event(ui, &event)) {
        continue;  // UI consumed the event
    }
    // Handle remaining events for game
}
```

## Notes

- Widget IDs: Use `##id` prefix for widgets that don't need visible labels
- Dropdowns appear above other widgets (managed z-order)
- Textbox captures keyboard input when focused
- All widgets are laid out vertically within panels
