# UI System

Immediate-mode GUI system with theming support.

## Quick Start

```c
#include "carbon/ui.h"

// Create UI context
CUI_Context *ui = cui_create(gpu, window);

// Load font for UI
Carbon_Font *font = carbon_font_load(text_renderer, "assets/fonts/ui.ttf", 16.0f);
cui_set_font(ui, font);

// In game loop
cui_begin_frame(ui);
cui_poll_events(ui);  // Or cui_process_event(ui, &event)

// Define UI
if (cui_button(ui, "Click Me")) {
    printf("Clicked!\n");
}

cui_end_frame(ui);
cui_render(ui, cmd, pass);
```

## Widgets

```c
// Buttons
if (cui_button(ui, "Normal")) { }
if (cui_button_primary(ui, "Primary")) { }  // Accent color
if (cui_button_danger(ui, "Delete")) { }    // Red

// Labels
cui_label(ui, "Text");
cui_label_colored(ui, "Red", cui_rgb(255, 0, 0));
cui_label_printf(ui, "Score: %d", score);

// Sliders
static float volume = 0.5f;
cui_slider(ui, "Volume", &volume, 0.0f, 1.0f);
static int quality = 2;
cui_slider_int(ui, "Quality", &quality, 0, 4);

// Checkboxes and toggles
static bool enabled = true;
cui_checkbox(ui, "Enable Sound", &enabled);
cui_toggle(ui, "Dark Mode", &dark_mode);

// Text input
static char name[64] = "";
cui_text_input(ui, "Name", name, sizeof(name));

// Progress bar
cui_progress_bar(ui, current, 0, max);
cui_progress_bar_colored(ui, health, 0, 100, theme->success);
```

## Layout

```c
// Panels
cui_panel_begin(ui, "Settings", x, y, width, height);
// ... widgets ...
cui_panel_end(ui);

// Horizontal layout
cui_row_begin(ui);
cui_button(ui, "A");
cui_button(ui, "B");
cui_row_end(ui);

// Spacing
cui_spacing(ui, 10);
cui_separator(ui);
```

## Theming

```c
// Predefined themes
CUI_Theme dark = cui_theme_dark();
CUI_Theme light = cui_theme_light();
cui_set_theme(ui, &dark);

// Customize accent
cui_theme_set_accent(&dark, cui_rgb(100, 150, 255));

// Set semantic colors
cui_theme_set_semantic_colors(&dark,
    cui_rgb(80, 200, 120),   // success
    cui_rgb(255, 180, 50),   // warning
    cui_rgb(240, 80, 80),    // danger
    cui_rgb(80, 150, 240));  // info
```

## Semantic Buttons

```c
cui_button_primary(ui, "Save");     // Accent color
cui_button_success(ui, "Confirm");  // Green
cui_button_warning(ui, "Caution");  // Orange
cui_button_danger(ui, "Delete");    // Red
cui_button_info(ui, "Help");        // Blue
```

## Color Helpers

```c
uint32_t lighter = cui_color_brighten(color, 0.2f);
uint32_t darker = cui_color_darken(color, 0.2f);
uint32_t faded = cui_color_alpha(color, 0.5f);
uint32_t blended = cui_color_lerp(color1, color2, 0.5f);
```

## Event Handling

```c
// In event loop - UI consumes events first
while (SDL_PollEvent(&event)) {
    if (cui_process_event(ui, &event)) continue;  // UI handled it
    // ... handle other events ...
}
```
