# Virtual Resolution System

Provides a fixed coordinate space (default 1920x1080) that automatically scales to fit any window size with letterboxing for aspect ratio preservation. Includes HiDPI/Retina display support.

## Quick Start

```c
#include "agentite/virtual_resolution.h"

// Create with default 1920x1080 virtual resolution
Agentite_VirtualResolution *vr = agentite_vres_create_default();

// Or specify custom resolution
Agentite_VirtualResolution *vr = agentite_vres_create(1280, 720);

// Update when window resizes
void on_window_resize(int width, int height) {
    float dpi_scale = SDL_GetWindowDisplayScale(window);
    agentite_vres_update(vr, width, height, dpi_scale);
}

// Convert game coordinates to screen for rendering
float screen_x, screen_y;
agentite_vres_to_screen(vr, game_x, game_y, &screen_x, &screen_y);

// Convert mouse input to game coordinates
float game_x, game_y;
agentite_vres_to_virtual(vr, mouse_x, mouse_y, &game_x, &game_y);

// Get viewport for SDL rendering
Agentite_Viewport viewport = agentite_vres_get_viewport(vr);
// Use viewport.rect for SDL_SetRenderViewport

agentite_vres_destroy(vr);
```

## Key Concepts

### Virtual vs Screen Space

- **Virtual Space**: Fixed coordinate system your game uses (e.g., 1920x1080)
- **Screen Space**: Actual window pixels that change with window size

All game logic uses virtual coordinates. The system handles conversion:

```c
// Player position in virtual space (always consistent)
player.x = 960;  // Center of 1920-wide virtual space
player.y = 540;  // Center of 1080-tall virtual space

// For rendering, convert to screen space
float screen_x, screen_y;
agentite_vres_to_screen(vr, player.x, player.y, &screen_x, &screen_y);
draw_sprite_at(screen_x, screen_y);
```

### Scaling Modes

```c
agentite_vres_set_scale_mode(vr, AGENTITE_SCALE_LETTERBOX);
```

| Mode | Description |
|------|-------------|
| `AGENTITE_SCALE_LETTERBOX` | Preserve aspect ratio, add black bars (default) |
| `AGENTITE_SCALE_STRETCH` | Fill entire window, distorts if aspect differs |
| `AGENTITE_SCALE_PIXEL_PERFECT` | Integer scaling only (for pixel art) |
| `AGENTITE_SCALE_OVERSCAN` | Fill screen, crop edges if needed |

### Letterboxing

With letterbox mode, the viewport may not fill the entire window:

```c
Agentite_Viewport vp = agentite_vres_get_viewport(vr);

// vp.letterbox_x: horizontal bar width (each side)
// vp.letterbox_y: vertical bar height (each side)

if (vp.letterbox_x > 0) {
    // Window is wider than virtual aspect ratio
    // Black bars on left and right
}
if (vp.letterbox_y > 0) {
    // Window is taller than virtual aspect ratio
    // Black bars on top and bottom
}
```

### Input Handling

Mouse coordinates from SDL are in screen space. Convert to virtual:

```c
void handle_mouse(int screen_x, int screen_y) {
    // Convert to game coordinates
    float game_x, game_y;
    agentite_vres_to_virtual(vr, screen_x, screen_y, &game_x, &game_y);

    // Check if click is within game area (not in letterbox)
    if (!agentite_vres_is_in_bounds(vr, game_x, game_y)) {
        return;  // Click was in letterbox area
    }

    // Use game_x, game_y for game logic
    check_button_click(game_x, game_y);
}
```

### Size Scaling

Scale sizes (not just positions) between coordinate spaces:

```c
// Convert a radius from virtual to screen space
float virtual_radius = 50.0f;
float screen_radius = agentite_vres_scale_size(vr, virtual_radius);

// Convert from screen to virtual (e.g., for touch radius)
float touch_radius = agentite_vres_unscale_size(vr, 20.0f);
```

## Key Functions

| Function | Description |
|----------|-------------|
| `agentite_vres_create(w, h)` | Create with custom resolution |
| `agentite_vres_create_default()` | Create with 1920x1080 |
| `agentite_vres_destroy(vr)` | Free virtual resolution |
| `agentite_vres_update(vr, w, h, dpi)` | Update with window size |
| `agentite_vres_set_scale_mode(vr, mode)` | Set scaling mode |
| `agentite_vres_to_screen(vr, vx, vy, out_x, out_y)` | Virtual → Screen |
| `agentite_vres_to_virtual(vr, sx, sy, out_x, out_y)` | Screen → Virtual |
| `agentite_vres_scale_size(vr, size)` | Scale size to screen |
| `agentite_vres_unscale_size(vr, size)` | Scale size to virtual |
| `agentite_vres_get_viewport(vr)` | Get viewport info |
| `agentite_vres_get_scale(vr)` | Get current scale factor |
| `agentite_vres_is_in_viewport(vr, sx, sy)` | Check if screen pos in viewport |
| `agentite_vres_is_in_bounds(vr, vx, vy)` | Check if virtual pos in bounds |
| `agentite_vres_clamp_to_bounds(vr, vx, vy)` | Clamp to virtual bounds |

## Rectangle Conversion

Convert entire rectangles between spaces:

```c
// Convert button rect from virtual to screen for hit testing
Agentite_Rect virtual_button = {100, 200, 150, 50};
Agentite_Rect screen_button = agentite_vres_rect_to_screen(vr, virtual_button);

// Convert selection box from screen to virtual
Agentite_Rect screen_selection = {start_x, start_y, width, height};
Agentite_Rect virtual_selection = agentite_vres_rect_to_virtual(vr, screen_selection);
```

## HiDPI/Retina Support

Pass the DPI scale when updating:

```c
void on_window_event(SDL_WindowEvent *event) {
    if (event->type == SDL_EVENT_WINDOW_RESIZED ||
        event->type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {

        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        float dpi = SDL_GetWindowDisplayScale(window);

        agentite_vres_update(vr, w, h, dpi);
    }
}
```

The system automatically accounts for DPI when converting coordinates.

## Example: Complete Setup

```c
// Initialization
Agentite_VirtualResolution *vr = agentite_vres_create(1920, 1080);
agentite_vres_set_scale_mode(vr, AGENTITE_SCALE_LETTERBOX);

int w, h;
SDL_GetWindowSize(window, &w, &h);
float dpi = SDL_GetWindowDisplayScale(window);
agentite_vres_update(vr, w, h, dpi);

// Game loop
while (running) {
    // Handle events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            agentite_vres_update(vr, event.window.data1, event.window.data2,
                                  SDL_GetWindowDisplayScale(window));
        }
        else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            float gx, gy;
            agentite_vres_to_virtual(vr, event.button.x, event.button.y, &gx, &gy);
            handle_click(gx, gy);
        }
    }

    // Clear (including letterbox areas)
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    // Set viewport to game area
    Agentite_Viewport vp = agentite_vres_get_viewport(vr);
    SDL_Rect sdl_vp = {vp.rect.x, vp.rect.y, vp.rect.w, vp.rect.h};
    SDL_SetRenderViewport(renderer, &sdl_vp);

    // Render game (all in virtual coordinates, system handles scaling)
    render_game();

    // Reset viewport for UI that spans full screen
    SDL_SetRenderViewport(renderer, NULL);

    SDL_RenderPresent(renderer);
}
```

## Notes

- Virtual resolution is independent of window size
- Game logic always uses virtual coordinates for consistency
- Letterbox areas receive no input events
- Pixel-perfect mode only uses integer scales (1x, 2x, 3x, etc.)
- DPI scale affects coordinate conversion automatically
- Call `agentite_vres_update()` on any window resize
