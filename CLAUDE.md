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
│   ├── animation.c     # Sprite-based animation system
│   ├── camera.c        # 2D camera with pan, zoom, rotation
│   ├── tilemap.c       # Chunk-based tilemap rendering
│   └── text.c          # TrueType font rendering
├── audio/
│   └── audio.c         # Audio system with mixing
├── input/
│   └── input.c         # Input abstraction with action mapping
├── ai/
│   └── pathfinding.c   # A* pathfinding for tile-based maps
├── ui/                 # Immediate-mode GUI system
└── game/               # Game-specific logic (future)

include/carbon/
├── carbon.h            # Public API header
├── ecs.h               # ECS API and component definitions
├── ui.h                # UI system header
├── sprite.h            # Sprite/texture API
├── animation.h         # Sprite animation API
├── camera.h            # 2D camera API
├── tilemap.h           # Tilemap system API
├── text.h              # Text rendering API
├── input.h             # Input system with action mapping
├── audio.h             # Audio system API
└── pathfinding.h       # A* pathfinding API

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

## Animation System

Sprite-based animation with support for sprite sheets, multiple playback modes, and variable frame timing:

```c
#include "carbon/animation.h"

// Load sprite sheet texture
Carbon_Texture *sheet = carbon_texture_load(sr, "assets/player_walk.png");

// Create animation from sprite sheet grid (8 frames in a row, 64x64 each)
Carbon_Animation *walk = carbon_animation_from_strip(sheet, 0, 0, 64, 64, 8);
carbon_animation_set_fps(walk, 12.0f);  // 12 frames per second

// Or from a grid (4 columns x 2 rows)
Carbon_Animation *idle = carbon_animation_from_grid(sheet, 0, 64, 64, 64, 4, 2);

// Create player to track playback state
Carbon_AnimationPlayer player;
carbon_animation_player_init(&player, walk);
carbon_animation_player_play(&player);

// Set playback mode
carbon_animation_player_set_mode(&player, CARBON_ANIM_LOOP);       // Loop (default)
carbon_animation_player_set_mode(&player, CARBON_ANIM_ONCE);       // Play once, stop on last frame
carbon_animation_player_set_mode(&player, CARBON_ANIM_PING_PONG);  // Reverse at ends
carbon_animation_player_set_mode(&player, CARBON_ANIM_ONCE_RESET); // Play once, reset to first frame

// In game loop:
carbon_animation_player_update(&player, delta_time);

// Draw current frame (during sprite batch)
carbon_sprite_begin(sr, NULL);
carbon_animation_draw(sr, &player, x, y);                          // Simple
carbon_animation_draw_scaled(sr, &player, x, y, 2.0f, 2.0f);       // Scaled
carbon_animation_draw_ex(sr, &player, x, y, sx, sy, rotation, ox, oy); // Full transform
carbon_animation_draw_tinted(sr, &player, x, y, 1.0f, 0.5f, 0.5f, 1.0f); // Tinted
carbon_sprite_upload(sr, cmd);
// ... render pass ...
carbon_sprite_render(sr, cmd, pass);

// Playback control
carbon_animation_player_pause(&player);
carbon_animation_player_play(&player);
carbon_animation_player_stop(&player);      // Stop and reset to frame 0
carbon_animation_player_restart(&player);   // Restart from beginning
carbon_animation_player_set_speed(&player, 2.0f);  // Double speed
carbon_animation_player_set_frame(&player, 3);     // Jump to frame 3

// Switch animations (e.g., walk -> idle)
carbon_animation_player_set_animation(&player, idle);

// Query state
bool playing = carbon_animation_player_is_playing(&player);
bool finished = carbon_animation_player_is_finished(&player);  // For ONCE mode
uint32_t frame = carbon_animation_player_get_current_frame(&player);
float progress = carbon_animation_player_get_progress(&player);  // 0.0 to 1.0

// Completion callback (for one-shot animations or each loop)
void on_attack_done(void *userdata) {
    // Switch back to idle animation
}
carbon_animation_player_set_callback(&player, on_attack_done, NULL);

// Variable frame timing (e.g., hold last frame longer)
carbon_animation_set_frame_duration(walk, 7, 0.2f);  // Frame 7 = 0.2 seconds

// Animation info
uint32_t frame_count = carbon_animation_get_frame_count(walk);
float duration = carbon_animation_get_duration(walk);  // Total seconds
Carbon_Sprite *frame = carbon_animation_get_frame(walk, 0);  // Get specific frame

// Set origin for all frames (for rotation)
carbon_animation_set_origin(walk, 0.5f, 1.0f);  // Bottom-center

// Cleanup
carbon_animation_destroy(walk);
carbon_animation_destroy(idle);
```

**Key features:**
- Create animations from sprite sheet grids or horizontal strips
- Playback modes: loop, once, ping-pong, once-reset
- Variable speed and per-frame duration
- Completion callbacks for state machine integration
- Draws through existing sprite renderer (same batching rules)

**Note:** All frames in an animation share the same texture, so they batch efficiently with the sprite renderer.

## Text Rendering System

TrueType font rendering with batched glyphs. Uses stb_truetype for font atlas generation:

```c
#include "carbon/text.h"

// Initialize text renderer
Carbon_TextRenderer *text = carbon_text_init(gpu, window);

// Load fonts (each size creates a separate atlas)
Carbon_Font *font = carbon_font_load(text, "assets/fonts/Roboto.ttf", 24.0f);

// In game loop:
carbon_text_begin(text);

// Simple text (white)
carbon_text_draw(text, font, "Hello World!", 100.0f, 200.0f);

// Colored text
carbon_text_draw_colored(text, font, "Red text", x, y, 1.0f, 0.3f, 0.3f, 1.0f);

// Scaled text
carbon_text_draw_scaled(text, font, "Big text", x, y, 2.0f);

// With alignment
carbon_text_draw_ex(text, font, "Centered", x, y, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
                    CARBON_TEXT_ALIGN_CENTER);

// Printf-style formatting
carbon_text_printf(text, font, x, y, "FPS: %.0f", fps);
carbon_text_printf_colored(text, font, x, y, 0.8f, 0.8f, 0.8f, 1.0f,
                           "Score: %d", score);

carbon_text_end(text);

// Upload before render pass
carbon_text_upload(text, cmd);

// During render pass
carbon_text_render(text, cmd, pass);

// Measure text width
float width = carbon_text_measure(font, "Hello");
float text_width, text_height;
carbon_text_measure_bounds(font, "Hello", &text_width, &text_height);

// Font metrics
float line_height = carbon_font_get_line_height(font);
float ascent = carbon_font_get_ascent(font);

// Cleanup
carbon_font_destroy(text, font);
carbon_text_shutdown(text);
```

**Key features:**
- Batched glyph rendering (up to 2048 glyphs per batch)
- Color tinting and scaling per draw call
- Printf-style formatted text
- Text alignment (left, center, right)
- Newline support (`\n`)
- Text measurement for layout

**Note:** All text in a batch must use the same font. Different font sizes require separate fonts (each with its own atlas texture).

## SDF/MSDF Text Rendering

Signed Distance Field (SDF) and Multi-channel Signed Distance Field (MSDF) text rendering for sharp text at any scale with effects like outlines, glow, and shadows:

```c
#include "carbon/text.h"

// Load SDF or MSDF font from pre-generated atlas (msdf-atlas-gen format)
Carbon_SDFFont *sdf_font = carbon_sdf_font_load(text,
    "assets/fonts/Roboto-msdf.png",   // PNG atlas
    "assets/fonts/Roboto-msdf.json"); // JSON metrics

// In game loop:
carbon_text_begin(text);

// Basic SDF text (scale parameter instead of font size)
carbon_sdf_text_draw(text, sdf_font, "Sharp at any size!", 100.0f, 200.0f, 1.0f);

// Colored SDF text
carbon_sdf_text_draw_colored(text, sdf_font, "Blue text", x, y, 2.0f,
                              0.2f, 0.5f, 1.0f, 1.0f);

// With alignment
carbon_sdf_text_draw_ex(text, sdf_font, "Centered", x, y, 1.5f,
                         1.0f, 1.0f, 1.0f, 1.0f, CARBON_TEXT_ALIGN_CENTER);

// Text effects (apply before drawing)
carbon_sdf_text_set_outline(text, 0.1f, 0.0f, 0.0f, 0.0f, 1.0f);  // Black outline
carbon_sdf_text_draw(text, sdf_font, "Outlined Text", x, y, 2.0f);

carbon_sdf_text_set_glow(text, 0.15f, 1.0f, 0.8f, 0.0f, 0.8f);    // Golden glow
carbon_sdf_text_draw(text, sdf_font, "Glowing Text", x, y, 2.0f);

carbon_sdf_text_set_weight(text, 0.1f);  // Bolder text (-0.5 to 0.5)
carbon_sdf_text_draw(text, sdf_font, "Bold Text", x, y, 1.0f);

carbon_sdf_text_clear_effects(text);  // Reset effects

// Printf-style formatting
carbon_sdf_text_printf(text, sdf_font, x, y, 1.5f, "Score: %d", score);

carbon_text_end(text);

// Upload and render (same as bitmap text)
carbon_text_upload(text, cmd);
carbon_text_render(text, cmd, pass);

// Measurement
float width = carbon_sdf_text_measure(sdf_font, "Hello", 1.5f);
float w, h;
carbon_sdf_text_measure_bounds(sdf_font, "Hello", 1.5f, &w, &h);

// Font info
float font_size = carbon_sdf_font_get_size(sdf_font);
float line_height = carbon_sdf_font_get_line_height(sdf_font);
Carbon_SDFFontType type = carbon_sdf_font_get_type(sdf_font);  // SDF or MSDF

// Cleanup
carbon_sdf_font_destroy(text, sdf_font);
```

**Generating SDF/MSDF Atlases:**

Use [msdf-atlas-gen](https://github.com/Chlumsky/msdf-atlas-gen) to create atlas files:
```bash
# MSDF (best quality, sharp corners)
msdf-atlas-gen -font Roboto.ttf -type msdf -format png \
    -imageout Roboto-msdf.png -json Roboto-msdf.json \
    -size 48 -pxrange 4

# SDF (simpler, slight rounding on corners)
msdf-atlas-gen -font Roboto.ttf -type sdf -format png \
    -imageout Roboto-sdf.png -json Roboto-sdf.json \
    -size 48 -pxrange 4
```

**Key features:**
- Sharp text at any scale (no blurring when zoomed)
- Text effects: outline, glow, weight adjustment
- MSDF for sharp corners, SDF for simpler use cases
- Uses same batch system as bitmap text
- Compatible with msdf-atlas-gen JSON + PNG output

**Note:** SDF and bitmap fonts cannot be mixed in the same batch. Use separate `carbon_text_begin()`/`carbon_text_render()` calls.

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

## Tilemap System

Chunk-based tilemap for large maps with multiple layers and efficient frustum culling:

```c
#include "carbon/tilemap.h"

// Create tileset from texture (e.g., 4x4 grid of 32px tiles = 128x128 texture)
Carbon_Texture *tex = carbon_texture_load(sr, "assets/tileset.png");
Carbon_Tileset *tileset = carbon_tileset_create(tex, 32, 32);  // tile_width, tile_height

// Create tilemap (100x100 tiles = 3200x3200 world units)
Carbon_Tilemap *tilemap = carbon_tilemap_create(tileset, 100, 100);

// Add layers (rendered back to front)
int ground = carbon_tilemap_add_layer(tilemap, "ground");
int objects = carbon_tilemap_add_layer(tilemap, "objects");

// Set tiles (tile ID 0 = empty, 1+ = valid tile from tileset)
carbon_tilemap_fill(tilemap, ground, 0, 0, 100, 100, 1);      // Fill with grass
carbon_tilemap_set_tile(tilemap, objects, 50, 50, 5);          // Place tree

// Layer properties
carbon_tilemap_set_layer_visible(tilemap, objects, true);
carbon_tilemap_set_layer_opacity(tilemap, objects, 0.8f);

// In render loop (during sprite batch, before upload):
carbon_sprite_begin(sr, NULL);
carbon_tilemap_render(tilemap, sr, camera);  // Renders with frustum culling
// ... other sprites ...
carbon_sprite_upload(sr, cmd);
// ... render pass ...
carbon_sprite_render(sr, cmd, pass);

// Coordinate conversion (for mouse picking)
int tile_x, tile_y;
carbon_tilemap_world_to_tile(tilemap, world_x, world_y, &tile_x, &tile_y);
Carbon_TileID tile = carbon_tilemap_get_tile(tilemap, ground, tile_x, tile_y);

// Cleanup
carbon_tilemap_destroy(tilemap);
carbon_tileset_destroy(tileset);
```

**Key features:**
- Chunk-based storage (32x32 tiles per chunk) for efficient large maps
- Automatic frustum culling - only visible tiles are rendered
- Multiple layers with per-layer visibility and opacity
- Uses existing sprite renderer batching (single tileset = single batch)
- Coordinate conversion for world/tile position mapping

**Note:** All tiles in a tilemap must use the same tileset texture. For multiple tilesets, use a texture atlas or render separate tilemaps.

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

## Audio System

Sound effects and music playback with mixing support:

```c
#include "carbon/audio.h"

// Initialize
Carbon_Audio *audio = carbon_audio_init();

// Load sounds (WAV files, fully loaded in memory)
Carbon_Sound *sound_shoot = carbon_sound_load(audio, "assets/sounds/shoot.wav");
Carbon_Sound *sound_jump = carbon_sound_load(audio, "assets/sounds/jump.wav");

// Load music (WAV files, for background music)
Carbon_Music *music_bg = carbon_music_load(audio, "assets/music/background.wav");

// Play sounds (returns handle for control)
Carbon_SoundHandle h = carbon_sound_play(audio, sound_shoot);

// Play with options: volume (0-1), pan (-1 to +1), loop
carbon_sound_play_ex(audio, sound_shoot, 0.8f, 0.0f, false);

// Control playing sounds
carbon_sound_set_volume(audio, h, 0.5f);
carbon_sound_set_pan(audio, h, -0.5f);  // Pan left
carbon_sound_stop(audio, h);
if (carbon_sound_is_playing(audio, h)) { }

// Play music (only one track at a time)
carbon_music_play(audio, music_bg);                    // Loop by default
carbon_music_play_ex(audio, music_bg, 0.7f, true);     // Volume + loop
carbon_music_pause(audio);
carbon_music_resume(audio);
carbon_music_stop(audio);

// Volume controls
carbon_audio_set_master_volume(audio, 0.8f);  // Affects all audio
carbon_audio_set_sound_volume(audio, 1.0f);   // Affects all sounds
carbon_audio_set_music_volume(audio, 0.5f);   // Affects music

// In game loop (for streaming, currently no-op)
carbon_audio_update(audio);

// Cleanup
carbon_sound_destroy(audio, sound_shoot);
carbon_music_destroy(audio, music_bg);
carbon_audio_shutdown(audio);
```

**Key features:**
- Up to 32 simultaneous sound channels
- Automatic audio format conversion (any WAV → float32 stereo)
- Per-sound volume and stereo panning
- Separate volume controls for master, sounds, and music
- Sound handles for runtime control of playing sounds

## Pathfinding System

A* pathfinding for tile-based maps with diagonal movement, weighted costs, and tilemap integration:

```c
#include "carbon/pathfinding.h"

// Create pathfinder matching your tilemap dimensions
Carbon_Pathfinder *pf = carbon_pathfinder_create(100, 100);

// Option 1: Manual obstacle setup
carbon_pathfinder_set_walkable(pf, 10, 10, false);           // Block single tile
carbon_pathfinder_fill_walkable(pf, 20, 20, 5, 5, false);    // Block 5x5 region
carbon_pathfinder_set_cost(pf, 5, 5, 2.0f);                  // Rough terrain (slower)

// Option 2: Sync with tilemap layer (recommended)
uint16_t blocked[] = { 2, 3, 4 };  // Tile IDs that are walls/obstacles
carbon_pathfinder_sync_tilemap(pf, tilemap, collision_layer, blocked, 3);

// Option 3: Custom cost function for terrain types
float terrain_cost(uint16_t tile_id, void *userdata) {
    switch (tile_id) {
        case 1: return 1.0f;   // Grass - normal speed
        case 2: return 0.0f;   // Water - blocked
        case 3: return 2.0f;   // Mud - half speed
        case 4: return 1.5f;   // Forest - slower
        default: return 1.0f;
    }
}
carbon_pathfinder_sync_tilemap_ex(pf, tilemap, ground_layer, terrain_cost, NULL);

// Find a path
Carbon_Path *path = carbon_pathfinder_find(pf, start_x, start_y, end_x, end_y);
if (path) {
    // Follow the path
    for (int i = 0; i < path->length; i++) {
        int tx = path->points[i].x;
        int ty = path->points[i].y;
        // Move unit to tile (tx, ty)...
    }
    printf("Path cost: %.2f\n", path->total_cost);
    carbon_path_destroy(path);
} else {
    printf("No path found!\n");
}

// Pathfinding with options
Carbon_PathOptions opts = CARBON_PATH_OPTIONS_DEFAULT;
opts.allow_diagonal = false;       // 4-directional only
opts.max_iterations = 1000;        // Limit search (0 = unlimited)
opts.cut_corners = true;           // Allow diagonal past corners
Carbon_Path *path2 = carbon_pathfinder_find_ex(pf, x1, y1, x2, y2, &opts);

// Quick path checks (no path allocation)
if (carbon_pathfinder_has_path(pf, x1, y1, x2, y2)) { }     // Path exists?
if (carbon_pathfinder_line_clear(pf, x1, y1, x2, y2)) { }   // Direct line clear?

// Simplify path (remove redundant waypoints on straight lines)
path = carbon_path_simplify(path);  // Returns simplified, frees original

// Distance utilities
int manhattan = carbon_pathfinder_distance_manhattan(x1, y1, x2, y2);
float euclidean = carbon_pathfinder_distance_euclidean(x1, y1, x2, y2);
int chebyshev = carbon_pathfinder_distance_chebyshev(x1, y1, x2, y2);

// Cleanup
carbon_pathfinder_destroy(pf);
```

**Key features:**
- A* algorithm with binary heap for efficient search
- 4-directional or 8-directional (diagonal) movement
- Per-tile movement costs for terrain variation
- Tilemap integration with blocked tile lists or custom cost functions
- Corner-cutting prevention (configurable)
- Path simplification to reduce waypoints
- Line-of-sight checking with Bresenham's algorithm

**Performance notes:**
- Pathfinder reuses memory between searches (create once, use many times)
- Use `max_iterations` to limit search on very large maps
- `carbon_pathfinder_has_path()` is equivalent to full pathfinding (future: early exit)
- For dynamic obstacles, update with `set_walkable()` or re-sync tilemap

## Development Notes

- Target platforms: macOS, Linux, Windows
- Language: C11
- Graphics: SDL3 SDL_GPU (auto-selects Metal/Vulkan/D3D12)
- ECS: Flecs (high-performance Entity Component System)
- Math: cglm (SIMD-optimized)
- Repository: https://github.com/PhilipLudington/Carbon
