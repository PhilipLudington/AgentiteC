# UI Example

Demonstrates the immediate-mode UI system.

## What This Demonstrates

- Panels with title bars and borders
- Buttons, checkboxes, sliders
- Dropdowns and listboxes
- Text input (textbox)
- Labels and separators
- Progress bars
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
| Escape | Quit |

## Key Patterns

### UI Frame Structure
```c
// Begin UI frame
cui_begin_frame(ui, delta_time);

// Draw widgets inside panels
if (cui_begin_panel(ui, "Panel Name", x, y, width, height, flags)) {
    cui_label(ui, "Hello!");
    if (cui_button(ui, "Click Me")) {
        // Button was clicked
    }
    cui_end_panel(ui);
}

// End UI frame
cui_end_frame(ui);

// Upload and render (during GPU operations)
cui_upload(ui, cmd);
// ... during render pass ...
cui_render(ui, cmd, pass);
```

### Panel Flags
```c
CUI_PANEL_NONE        // No decorations
CUI_PANEL_TITLE_BAR   // Show title bar
CUI_PANEL_BORDER      // Show border
CUI_PANEL_TITLE_BAR | CUI_PANEL_BORDER  // Both
```

### Widgets

```c
// Labels and text
cui_label(ui, "Static text");
cui_spacing(ui, 10);  // Vertical space
cui_separator(ui);    // Horizontal line

// Buttons
if (cui_button(ui, "Click Me")) {
    // Handle click
}

// Checkboxes
bool enabled = true;
cui_checkbox(ui, "Enable Feature", &enabled);

// Sliders
float volume = 0.5f;
cui_slider_float(ui, "Volume", &volume, 0.0f, 1.0f);

// Dropdowns
int selection = 0;
const char *options[] = {"Low", "Medium", "High"};
cui_dropdown(ui, "Quality", &selection, options, 3);

// Text input
char buffer[64] = "Default";
cui_textbox(ui, "Name", buffer, sizeof(buffer));

// Listbox
int selected = 0;
const char *items[] = {"Item 1", "Item 2", "Item 3"};
cui_listbox(ui, "##id", &selected, items, 3, 100);  // height=100

// Progress bar (standalone, not in panel)
cui_progress_bar(ui, value, min, max);
```

### Event Handling
```c
// UI processes events first
SDL_Event event;
while (SDL_PollEvent(&event)) {
    if (cui_process_event(ui, &event)) {
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
