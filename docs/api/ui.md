# UI System

Immediate-mode GUI system with theming support.

## Quick Start

```c
#include "agentite/ui.h"

// Create UI context
AUI_Context *ui = aui_create(gpu, window);

// Load font for UI
Agentite_Font *font = agentite_font_load(text_renderer, "assets/fonts/ui.ttf", 16.0f);
aui_set_font(ui, font);

// In game loop
aui_begin_frame(ui);
aui_poll_events(ui);  // Or aui_process_event(ui, &event)

// Define UI
if (aui_button(ui, "Click Me")) {
    printf("Clicked!\n");
}

aui_end_frame(ui);
aui_render(ui, cmd, pass);
```

## Widgets

```c
// Buttons
if (aui_button(ui, "Normal")) { }
if (aui_button_primary(ui, "Primary")) { }  // Accent color
if (aui_button_danger(ui, "Delete")) { }    // Red

// Labels
aui_label(ui, "Text");
aui_label_colored(ui, "Red", aui_rgb(255, 0, 0));
aui_label_printf(ui, "Score: %d", score);

// Sliders
static float volume = 0.5f;
aui_slider(ui, "Volume", &volume, 0.0f, 1.0f);
static int quality = 2;
aui_slider_int(ui, "Quality", &quality, 0, 4);

// Checkboxes and toggles
static bool enabled = true;
aui_checkbox(ui, "Enable Sound", &enabled);
aui_toggle(ui, "Dark Mode", &dark_mode);

// Text input
static char name[64] = "";
aui_text_input(ui, "Name", name, sizeof(name));

// Progress bar
aui_progress_bar(ui, current, 0, max);
aui_progress_bar_colored(ui, health, 0, 100, theme->success);
```

## Layout

```c
// Panels
aui_panel_begin(ui, "Settings", x, y, width, height);
// ... widgets ...
aui_panel_end(ui);

// Horizontal layout
aui_row_begin(ui);
aui_button(ui, "A");
aui_button(ui, "B");
aui_row_end(ui);

// Spacing
aui_spacing(ui, 10);
aui_separator(ui);
```

## Theming

```c
// Predefined themes
AUI_Theme dark = aui_theme_dark();
AUI_Theme light = aui_theme_light();
aui_set_theme(ui, &dark);

// Customize accent
aui_theme_set_accent(&dark, aui_rgb(100, 150, 255));

// Set semantic colors
aui_theme_set_semantic_colors(&dark,
    aui_rgb(80, 200, 120),   // success
    aui_rgb(255, 180, 50),   // warning
    aui_rgb(240, 80, 80),    // danger
    aui_rgb(80, 150, 240));  // info
```

## Semantic Buttons

```c
aui_button_primary(ui, "Save");     // Accent color
aui_button_success(ui, "Confirm");  // Green
aui_button_warning(ui, "Caution");  // Orange
aui_button_danger(ui, "Delete");    // Red
aui_button_info(ui, "Help");        // Blue
```

## Color Helpers

```c
uint32_t lighter = aui_color_brighten(color, 0.2f);
uint32_t darker = aui_color_darken(color, 0.2f);
uint32_t faded = aui_color_alpha(color, 0.5f);
uint32_t blended = aui_color_lerp(color1, color2, 0.5f);
```

## Event Handling

```c
// In event loop - UI consumes events first
while (SDL_PollEvent(&event)) {
    if (aui_process_event(ui, &event)) continue;  // UI handled it
    // ... handle other events ...
}
```
