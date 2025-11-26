# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Carbon is a C-based game engine targeting strategy games. Uses SDL3 with SDL_GPU for cross-platform rendering (Metal on macOS, Vulkan on Linux, D3D12/Vulkan on Windows).

## Build Commands

```bash
# Install SDL3 (macOS)
brew install sdl3

# Build
make

# Build with debug symbols
make DEBUG=1

# Run
make run

# Clean
make clean

# Show build configuration
make info
```

## Architecture

```
src/
├── main.c              # Entry point, game loop
├── core/
│   └── engine.c        # Engine initialization, SDL3/GPU setup, frame management
├── ecs/
│   └── ecs.c           # ECS wrapper around Flecs
├── platform/           # Platform-specific code (future)
├── graphics/
│   ├── sprite.c        # Sprite/texture rendering with batching
│   └── camera.c        # 2D camera with pan, zoom, rotation
├── audio/              # Sound system (future)
├── input/
│   └── input.c         # Input abstraction with action mapping
├── ui/                 # Immediate-mode GUI system
└── game/               # Game-specific logic (future)

include/carbon/
├── carbon.h            # Public API header
├── ecs.h               # ECS API and component definitions
├── ui.h                # UI system header
├── sprite.h            # Sprite/texture API
├── camera.h            # 2D camera API
└── input.h             # Input system with action mapping

lib/
├── flecs.h/.c          # Flecs ECS library (v4.0.0)
├── stb_image.h         # Image loading
├── stb_truetype.h      # Font rendering
├── stb_rect_pack.h     # Rectangle packing
└── cglm/               # Math library (vec, mat, etc.)
```

**Core engine flow:**
1. `carbon_init()` - Creates window, initializes SDL3, creates GPU device
2. Main loop: `carbon_begin_frame()` → `carbon_poll_events()` → `carbon_begin_render_pass()` → render → `carbon_end_render_pass()` → `carbon_end_frame()`
3. `carbon_shutdown()` - Cleanup

## ECS (Entity Component System)

Uses Flecs v4.0.0 for game entity management:

```c
#include "carbon/ecs.h"

// Initialize
Carbon_World *ecs_world = carbon_ecs_init();
ecs_world_t *w = carbon_ecs_get_world(ecs_world);

// Create entities with components
ecs_entity_t player = carbon_ecs_entity_new_named(ecs_world, "Player");
ecs_set(w, player, C_Position, { .x = 100.0f, .y = 100.0f });
ecs_set(w, player, C_Velocity, { .vx = 0.0f, .vy = 0.0f });
ecs_set(w, player, C_Health, { .health = 100, .max_health = 100 });

// In game loop
carbon_ecs_progress(ecs_world, delta_time);

// Query components
const C_Position *pos = ecs_get(w, player, C_Position);

// Cleanup
carbon_ecs_shutdown(ecs_world);
```

**Built-in components:** `C_Position`, `C_Velocity`, `C_Size`, `C_Color`, `C_Name`, `C_Active`, `C_Health`, `C_RenderLayer`

## Sprite/Texture System

Batched sprite rendering with transform support:

```c
#include "carbon/sprite.h"

// Initialize
Carbon_SpriteRenderer *sr = carbon_sprite_init(gpu, window);

// Load texture
Carbon_Texture *tex = carbon_texture_load(sr, "assets/player.png");
Carbon_Sprite sprite = carbon_sprite_from_texture(tex);

// In render loop (before render pass):
carbon_sprite_begin(sr, NULL);
carbon_sprite_draw(sr, &sprite, 100.0f, 200.0f);                    // Simple
carbon_sprite_draw_scaled(sr, &sprite, x, y, 2.0f, 2.0f);           // Scaled
carbon_sprite_draw_ex(sr, &sprite, x, y, sx, sy, rotation, ox, oy); // Full transform
carbon_sprite_draw_tinted(sr, &sprite, x, y, 1.0f, 0.5f, 0.5f, 1.0f); // Color tint

// Upload before render pass
carbon_sprite_upload(sr, cmd);

// During render pass
carbon_sprite_render(sr, cmd, pass);

// Cleanup
carbon_texture_destroy(sr, tex);
carbon_sprite_shutdown(sr);
```

**Note:** Current limitation - all sprites in a batch must use the same texture. Multiple textures require separate batches or texture atlas.

## Camera System

2D camera with pan, zoom, and rotation support. Uses view-projection matrix in the GPU shader:

```c
#include "carbon/camera.h"

// Create camera
Carbon_Camera *camera = carbon_camera_create(1280.0f, 720.0f);

// Connect to sprite renderer
carbon_sprite_set_camera(sprites, camera);

// Control camera
carbon_camera_set_position(camera, 500.0f, 300.0f);  // Center on world position
carbon_camera_move(camera, dx, dy);                   // Pan by delta
carbon_camera_set_zoom(camera, 2.0f);                 // 2x magnification
carbon_camera_set_rotation(camera, 45.0f);            // Rotate 45 degrees

// Update each frame (recomputes matrices if dirty)
carbon_camera_update(camera);

// Coordinate conversion (for mouse picking)
float world_x, world_y;
carbon_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

// Get visible world bounds
float left, right, top, bottom;
carbon_camera_get_bounds(camera, &left, &right, &top, &bottom);

// Cleanup
carbon_camera_destroy(camera);
```

**Note:** When no camera is set (`carbon_sprite_set_camera(sr, NULL)`), sprites render in screen-space coordinates for UI elements.

## Input System

Action-based input abstraction supporting keyboard, mouse, and gamepad. Define named actions and bind multiple inputs to each:

```c
#include "carbon/input.h"

// Initialize
Carbon_Input *input = carbon_input_init();

// Register actions
int action_jump = carbon_input_register_action(input, "jump");
int action_fire = carbon_input_register_action(input, "fire");
int action_move_left = carbon_input_register_action(input, "move_left");

// Bind keys (up to 4 bindings per action)
carbon_input_bind_key(input, action_jump, SDL_SCANCODE_SPACE);
carbon_input_bind_key(input, action_jump, SDL_SCANCODE_W);           // Alternative key
carbon_input_bind_mouse(input, action_fire, 1);                       // Left mouse button
carbon_input_bind_gamepad_button(input, action_jump, SDL_GAMEPAD_BUTTON_SOUTH);
carbon_input_bind_gamepad_axis(input, action_move_left, SDL_GAMEPAD_AXIS_LEFTX, 0.3f, false);

// In game loop:
carbon_input_begin_frame(input);  // Reset per-frame state

while (SDL_PollEvent(&event)) {
    if (cui_process_event(ui, &event)) continue;  // UI first
    carbon_input_process_event(input, &event);     // Then input
    if (event.type == SDL_EVENT_QUIT) carbon_quit(engine);
}

carbon_input_update(input);  // Compute just_pressed/released

// Query actions (works for any bound input)
if (carbon_input_action_pressed(input, action_jump)) { /* held down */ }
if (carbon_input_action_just_pressed(input, action_jump)) { /* this frame */ }
if (carbon_input_action_just_released(input, action_jump)) { /* released */ }
float val = carbon_input_action_value(input, action_move_left);  // Analog: -1.0 to 1.0

// Convenience name-based queries (slightly slower)
if (carbon_input_pressed(input, "jump")) { }

// Direct input queries
float mx, my;
carbon_input_get_mouse_position(input, &mx, &my);
carbon_input_get_scroll(input, &scroll_x, &scroll_y);
if (carbon_input_key_just_pressed(input, SDL_SCANCODE_F1)) { }
if (carbon_input_mouse_button(input, 0)) { }  // 0=left, 1=mid, 2=right

// Gamepad support
int gamepad_count = carbon_input_get_gamepad_count(input);
const Carbon_GamepadState *pad = carbon_input_get_gamepad(input, 0);

// Cleanup
carbon_input_shutdown(input);
```

**Key features:**
- Action mapping: One action → multiple inputs (keyboard, mouse, gamepad)
- Just pressed/released detection for frame-perfect input
- Analog values from gamepad axes and triggers
- Automatic gamepad hot-plug support
- Direct input queries when needed

## Development Notes

- Target platforms: macOS, Linux, Windows
- Language: C11
- Graphics: SDL3 SDL_GPU (auto-selects Metal/Vulkan/D3D12)
- ECS: Flecs (high-performance Entity Component System)
- Math: cglm (SIMD-optimized)
- Repository: https://github.com/PhilipLudington/Carbon
