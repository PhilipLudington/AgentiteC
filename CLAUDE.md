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
│   ├── engine.c        # Engine initialization, SDL3/GPU setup, frame management
│   ├── error.c         # Error handling system
│   ├── event.c         # Pub-sub event dispatcher
│   ├── containers.c    # Container utilities (random, shuffle)
│   └── game_context.c  # Unified system container
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
│   ├── pathfinding.c   # A* pathfinding for tile-based maps
│   └── personality.c   # AI personality and decision making
├── strategy/           # Turn-based strategy game systems
│   ├── turn.c          # Turn/phase management
│   ├── resource.c      # Resource economy
│   ├── tech.c          # Technology tree with prerequisites
│   ├── victory.c       # Victory condition tracking
│   ├── modifier.c      # Value modifier stacks
│   ├── threshold.c     # Value threshold callbacks
│   ├── history.c       # Metrics history tracking
│   ├── save.c          # TOML-based save/load
│   ├── game_event.c    # Scripted game events
│   └── unlock.c        # Unlock/achievement system
├── ui/                 # Immediate-mode GUI system
│   └── viewmodel.c     # Observable values for UI data binding
└── game/               # Game-specific logic

include/carbon/
├── carbon.h            # Public API header
├── error.h             # Error handling API
├── event.h             # Event dispatcher API
├── validate.h          # Validation macros (header-only)
├── containers.h        # Dynamic arrays, random utils
├── ecs.h               # ECS API and component definitions
├── ui.h                # UI system header
├── sprite.h            # Sprite/texture API
├── animation.h         # Sprite animation API
├── camera.h            # 2D camera API
├── tilemap.h           # Tilemap system API
├── text.h              # Text rendering API
├── input.h             # Input system with action mapping
├── audio.h             # Audio system API
├── pathfinding.h       # A* pathfinding API
├── ai.h                # AI personality system API
├── turn.h              # Turn/phase system API
├── resource.h          # Resource economy API
├── tech.h              # Technology tree API
├── victory.h           # Victory condition API
├── modifier.h          # Modifier stack API
├── threshold.h         # Threshold tracker API
├── history.h           # History tracking API
├── save.h              # Save/load system API
├── game_event.h        # Game event system API
├── unlock.h            # Unlock system API
└── viewmodel.h         # View model/data binding API

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

## Event Dispatcher System

Publish-subscribe event system for decoupled communication between game systems:

```c
#include "carbon/event.h"

// Create dispatcher
Carbon_EventDispatcher *events = carbon_event_dispatcher_create();

// Define callback
void on_turn_started(const Carbon_Event *event, void *userdata) {
    printf("Turn %u started!\n", event->turn.turn);
}

// Subscribe to specific event type
Carbon_ListenerID id = carbon_event_subscribe(events, CARBON_EVENT_TURN_STARTED,
                                               on_turn_started, NULL);

// Subscribe to ALL events (for logging, debugging)
carbon_event_subscribe_all(events, log_all_events, NULL);

// Emit events using convenience functions
carbon_event_emit_turn_started(events, 1);
carbon_event_emit_resource_changed(events, RESOURCE_GOLD, 100, 150);
carbon_event_emit_entity_created(events, player_entity);

// Or emit custom events
Carbon_Event e = { .type = CARBON_EVENT_CUSTOM };
e.custom.id = MY_CUSTOM_EVENT;
e.custom.data = my_data;
carbon_event_emit(events, &e);

// Deferred emission (safe during callbacks)
carbon_event_emit_deferred(events, &e);
carbon_event_flush_deferred(events);  // Call at end of frame

// Unsubscribe when done
carbon_event_unsubscribe(events, id);

// Cleanup
carbon_event_dispatcher_destroy(events);
```

**Built-in event types:**
- Engine: `WINDOW_RESIZE`, `WINDOW_FOCUS`, `ENGINE_SHUTDOWN`
- Game lifecycle: `GAME_STARTED`, `GAME_PAUSED`, `GAME_RESUMED`, `STATE_CHANGED`
- Turn-based: `TURN_STARTED`, `TURN_ENDED`, `PHASE_STARTED`, `PHASE_ENDED`
- Entity: `ENTITY_CREATED`, `ENTITY_DESTROYED`, `ENTITY_MODIFIED`
- Selection: `SELECTION_CHANGED`, `SELECTION_CLEARED`
- Resource: `RESOURCE_CHANGED`, `RESOURCE_DEPLETED`, `RESOURCE_THRESHOLD`
- Tech: `TECH_RESEARCHED`, `TECH_STARTED`, `UNLOCK_ACHIEVED`
- Victory: `VICTORY_ACHIEVED`, `DEFEAT`, `VICTORY_PROGRESS`
- UI: `UI_BUTTON_CLICKED`, `UI_VALUE_CHANGED`, `UI_PANEL_OPENED`
- Custom: `CARBON_EVENT_CUSTOM` + user-defined IDs (1000+)

## Validation Framework

Macro-based validation utilities for consistent error handling:

```c
#include "carbon/validate.h"

// Pointer validation (void return)
void process_data(Data *data) {
    CARBON_VALIDATE_PTR(data);  // Returns if NULL, sets error
    // ... use data safely ...
}

// Pointer validation (with return value)
bool load_file(const char *path) {
    CARBON_VALIDATE_PTR_RET(path, false);  // Returns false if NULL
    CARBON_VALIDATE_STRING_RET(path, false);  // Also checks empty string
    // ...
    return true;
}

// Multiple pointer validation
void draw_sprite(Renderer *r, Sprite *s, Texture *t) {
    CARBON_VALIDATE_PTRS3(r, s, t);  // Checks all three
    // ...
}

// Range validation
void set_volume(float vol) {
    CARBON_VALIDATE_RANGE_F(vol, 0.0f, 1.0f);  // Must be 0.0-1.0
    // ...
}

// Index validation
int get_item(int *array, size_t count, size_t index) {
    CARBON_VALIDATE_INDEX_RET(index, count, -1);  // Returns -1 if out of bounds
    return array[index];
}

// Entity validation
void damage_entity(ecs_world_t *world, ecs_entity_t entity, int damage) {
    CARBON_VALIDATE_ENTITY_ALIVE(world, entity);  // Checks ecs_is_alive()
    // ...
}

// Condition validation with message
bool connect(const char *host, int port) {
    CARBON_VALIDATE_COND_RET(port > 0 && port < 65536, "invalid port", false);
    // ...
    return true;
}

// Debug-only assertions (compiled out in release)
CARBON_ASSERT(count > 0);
CARBON_ASSERT_MSG(ptr != NULL, "pointer should never be null here");
CARBON_UNREACHABLE();  // Marks code that should never execute

// Soft warnings (continue execution)
CARBON_WARN_IF_NULL(optional_param);
CARBON_WARN_IF(count == 0, "processing empty array");
```

## Container Utilities

Dynamic arrays, random selection, and shuffle algorithms:

```c
#include "carbon/containers.h"

// Dynamic array declaration and usage
Carbon_Array(int) numbers;
carbon_array_init(&numbers);

carbon_array_push(&numbers, 10);
carbon_array_push(&numbers, 20);
carbon_array_push(&numbers, 30);

printf("Count: %zu\n", numbers.count);  // 3
printf("First: %d\n", numbers.data[0]); // 10

int last = carbon_array_pop(&numbers);  // 30, count now 2

carbon_array_remove(&numbers, 0);       // Remove first, shift remaining
carbon_array_remove_swap(&numbers, 0);  // Remove by swapping with last (O(1))

carbon_array_reserve(&numbers, 100);    // Pre-allocate capacity
carbon_array_resize(&numbers, 50);      // Resize to exactly 50 elements
carbon_array_clear(&numbers);           // Set count to 0, keep capacity

carbon_array_free(&numbers);            // Free memory

// Array iteration
Carbon_Array(float) values;
carbon_array_init(&values);
// ... push values ...

float v;
carbon_array_foreach(&values, float, v) {
    printf("%f\n", v);
}

// With index
size_t i;
carbon_array_foreach_i(&values, i, float, v) {
    printf("[%zu] = %f\n", i, v);
}

// Array utilities
int sum = carbon_array_sum(&numbers);
double avg = carbon_array_avg(&numbers);
int min_val = carbon_array_min(&numbers);
int max_val = carbon_array_max(&numbers);
size_t idx = carbon_array_find(&numbers, 42);  // SIZE_MAX if not found

// Random utilities
carbon_random_seed(0);  // 0 = time-based seed

int roll = carbon_rand_int(1, 6);           // [1, 6] inclusive
float pct = carbon_rand_float(0.0f, 1.0f);  // [0.0, 1.0)
bool coin = carbon_rand_bool();
size_t idx = carbon_rand_index(array_count); // [0, count)

// Random choice from array
int items[] = {10, 20, 30, 40, 50};
int chosen = carbon_random_choice(items, 5);  // Random element

// Weighted random selection
float weights[] = {0.5f, 0.3f, 0.2f};  // 50%, 30%, 20% probabilities
size_t selected = carbon_weighted_random_simple(weights, 3);

// Shuffle array in place (Fisher-Yates)
int deck[52];
// ... fill deck ...
carbon_shuffle_array(deck, 52);

// Shuffle Carbon_Array
carbon_array_shuffle(&numbers);

// Stack operations (same as array, different semantics)
Carbon_Stack(int) stack;
carbon_stack_init(&stack);
carbon_stack_push(&stack, 1);
carbon_stack_push(&stack, 2);
int top = carbon_stack_peek(&stack);  // 2
int popped = carbon_stack_pop(&stack); // 2
bool empty = carbon_stack_empty(&stack);
carbon_stack_free(&stack);

// Fixed-size ring buffer
Carbon_RingBuffer(int, 16) buffer;  // 16 elements max
carbon_ring_init(&buffer);
carbon_ring_push(&buffer, 42);
int val = carbon_ring_pop(&buffer);
bool full = carbon_ring_full(&buffer);
```

## Technology Tree System

Research system with prerequisites, branching, multiple resource costs, and effect application:

```c
#include "carbon/tech.h"

// Create tech tree (optionally with event dispatcher)
Carbon_TechTree *tree = carbon_tech_create_with_events(events);

// Define a technology
Carbon_TechDef tech = {
    .id = "improved_farming",
    .name = "Improved Farming",
    .description = "Increases food production by 20%",
    .branch = TECH_BRANCH_ECONOMY,  // Game-defined constant
    .tier = 1,
    .research_cost = 100,
    .prereq_count = 0,
    .effect_count = 1,
};
tech.effects[0] = (Carbon_TechEffect){
    .type = CARBON_TECH_EFFECT_RESOURCE_BONUS,
    .target = RESOURCE_FOOD,
    .value = 0.20f,
    .modifier_source = "tech_farming",
};
carbon_tech_register(tree, &tech);

// Tech with prerequisites
Carbon_TechDef advanced_tech = {
    .id = "advanced_farming",
    .name = "Advanced Farming",
    .description = "Further increases food production",
    .branch = TECH_BRANCH_ECONOMY,
    .tier = 2,
    .research_cost = 250,
    .prereq_count = 1,
};
strcpy(advanced_tech.prerequisites[0], "improved_farming");
carbon_tech_register(tree, &advanced_tech);

// Per-faction technology state
Carbon_TechState state;
carbon_tech_state_init(&state);

// Check if can research (prerequisites met, not already researched)
if (carbon_tech_can_research(tree, &state, "improved_farming")) {
    carbon_tech_start_research(tree, &state, "improved_farming");
}

// Each turn, add research points
int research_per_turn = 25;
if (carbon_tech_add_points(tree, &state, research_per_turn)) {
    // Tech completed! CARBON_EVENT_TECH_RESEARCHED emitted
}

// Query progress
float progress = carbon_tech_get_progress(&state, 0);  // 0.0 to 1.0
int32_t remaining = carbon_tech_get_remaining(&state, 0);

// Check if researched
if (carbon_tech_is_researched(tree, &state, "improved_farming")) {
    // Apply tech effects...
}

// Get available techs (prerequisites met, not researched)
const Carbon_TechDef *available[32];
int count = carbon_tech_get_available(tree, &state, available, 32);

// Get techs by branch or tier
const Carbon_TechDef *economy_techs[32];
int econ_count = carbon_tech_get_by_branch(tree, TECH_BRANCH_ECONOMY, economy_techs, 32);

// Completion callback (alternative to events)
void on_tech_complete(const Carbon_TechDef *tech, Carbon_TechState *state, void *userdata) {
    printf("Researched: %s\n", tech->name);
    // Apply effects to game state
}
carbon_tech_set_completion_callback(tree, on_tech_complete, game);

// Repeatable technologies (cost increases each time)
Carbon_TechDef repeatable_tech = {
    .id = "weapon_upgrade",
    .name = "Weapon Upgrade",
    .research_cost = 50,
    .repeatable = true,
};
carbon_tech_register(tree, &repeatable_tech);
int times_researched = carbon_tech_get_repeat_count(tree, &state, "weapon_upgrade");

// Concurrent research (up to CARBON_TECH_MAX_ACTIVE slots)
carbon_tech_start_research(tree, &state, "improved_farming");
carbon_tech_start_research(tree, &state, "weapon_upgrade");
carbon_tech_add_points_to_slot(tree, &state, 0, 25);  // To first
carbon_tech_add_points_to_slot(tree, &state, 1, 10);  // To second

// Cancel research
carbon_tech_cancel_research(&state, 0);
carbon_tech_cancel_all_research(&state);

// Debug/cheat: instantly complete
carbon_tech_complete(tree, &state, "improved_farming");

// Cleanup
carbon_tech_destroy(tree);
```

**Built-in effect types:**
- `CARBON_TECH_EFFECT_RESOURCE_BONUS` - Increase resource generation
- `CARBON_TECH_EFFECT_RESOURCE_CAP` - Increase resource maximum
- `CARBON_TECH_EFFECT_COST_REDUCTION` - Reduce costs by percentage
- `CARBON_TECH_EFFECT_PRODUCTION_SPEED` - Faster production
- `CARBON_TECH_EFFECT_UNLOCK_UNIT` - Enable a unit type
- `CARBON_TECH_EFFECT_UNLOCK_BUILDING` - Enable a building type
- `CARBON_TECH_EFFECT_ATTACK_BONUS`, `DEFENSE_BONUS`, `HEALTH_BONUS` - Combat stats
- `CARBON_TECH_EFFECT_CUSTOM` - Game-defined effects

**Events emitted:**
- `CARBON_EVENT_TECH_STARTED` - When research begins
- `CARBON_EVENT_TECH_RESEARCHED` - When research completes

## Victory Condition System

Multi-condition victory tracking with progress monitoring, score calculation, and event integration:

```c
#include "carbon/victory.h"

// Create victory manager (optionally with event dispatcher)
Carbon_VictoryManager *victory = carbon_victory_create_with_events(events);

// Register victory conditions
Carbon_VictoryCondition domination = {
    .id = "domination",
    .name = "World Domination",
    .description = "Control 75% of the map",
    .type = CARBON_VICTORY_DOMINATION,
    .threshold = 0.75f,
    .enabled = true,
};
carbon_victory_register(victory, &domination);

Carbon_VictoryCondition elimination = {
    .id = "elimination",
    .name = "Last Standing",
    .description = "Eliminate all opponents",
    .type = CARBON_VICTORY_ELIMINATION,
    .threshold = 1.0f,
    .enabled = true,
};
carbon_victory_register(victory, &elimination);

Carbon_VictoryCondition score_victory = {
    .id = "score",
    .name = "High Score",
    .description = "Highest score after 100 turns",
    .type = CARBON_VICTORY_SCORE,
    .target_turn = 100,
    .enabled = true,
};
carbon_victory_register(victory, &score_victory);

// Initialize faction tracking
carbon_victory_init_faction(victory, 0);  // Player
carbon_victory_init_faction(victory, 1);  // AI 1
carbon_victory_init_faction(victory, 2);  // AI 2

// Each turn, update progress
float territory_control = calculate_territory_percent(faction_id);
carbon_victory_update_progress(victory, faction_id, CARBON_VICTORY_DOMINATION, territory_control);

// For score-based conditions
carbon_victory_add_score(victory, faction_id, CARBON_VICTORY_SCORE, points_earned);

// Check for victory (call at end of turn)
carbon_victory_set_turn(victory, current_turn);
if (carbon_victory_check(victory)) {
    int winner = carbon_victory_get_winner(victory);
    int type = carbon_victory_get_winning_type(victory);
    const Carbon_VictoryState *state = carbon_victory_get_state(victory);
    printf("Victory! %s won via %s: %s\n",
           faction_names[winner],
           carbon_victory_type_name(type),
           state->message);
}

// Eliminate a faction
carbon_victory_eliminate_faction(victory, defeated_faction);

// Query progress for UI
float progress = carbon_victory_get_progress(victory, faction_id, CARBON_VICTORY_DOMINATION);
char progress_str[32];
carbon_victory_format_progress(victory, faction_id, CARBON_VICTORY_DOMINATION,
                                progress_str, sizeof(progress_str));

// Get score leader
int leader = carbon_victory_get_score_leader(victory);
int32_t total_score = carbon_victory_calculate_score(victory, faction_id);

// Custom victory checker for complex conditions
bool check_tech_victory(int faction_id, int type, float *out_progress, void *userdata) {
    GameState *game = userdata;
    int researched = count_researched_techs(game, faction_id);
    int total = total_tech_count(game);
    *out_progress = (float)researched / (float)total;
    return researched >= total;  // Win when all techs researched
}
carbon_victory_set_checker(victory, CARBON_VICTORY_TECHNOLOGY, check_tech_victory, game);

// Victory callback (alternative to events)
void on_victory(int faction_id, int type, const Carbon_VictoryCondition *cond, void *userdata) {
    printf("Faction %d won via %s!\n", faction_id, cond->name);
}
carbon_victory_set_callback(victory, on_victory, NULL);

// Enable/disable conditions mid-game
carbon_victory_set_enabled(victory, CARBON_VICTORY_SCORE, false);

// Reset for new game
carbon_victory_reset(victory);

// Cleanup
carbon_victory_destroy(victory);
```

**Built-in victory types:**
- `CARBON_VICTORY_DOMINATION` - Control percentage of territory
- `CARBON_VICTORY_ELIMINATION` - Last faction standing
- `CARBON_VICTORY_TECHNOLOGY` - Research all/specific techs
- `CARBON_VICTORY_ECONOMIC` - Accumulate resources
- `CARBON_VICTORY_SCORE` - Highest score after N turns
- `CARBON_VICTORY_TIME` - Survive for N turns
- `CARBON_VICTORY_OBJECTIVE` - Complete specific objectives
- `CARBON_VICTORY_WONDER` - Build a wonder structure
- `CARBON_VICTORY_DIPLOMATIC` - Achieve diplomatic status
- `CARBON_VICTORY_CULTURAL` - Achieve cultural dominance
- `CARBON_VICTORY_USER` (100+) - Game-defined victory types

**Events emitted:**
- `CARBON_EVENT_VICTORY_PROGRESS` - When progress changes significantly
- `CARBON_EVENT_VICTORY_ACHIEVED` - When a faction wins

**Key features:**
- Multiple simultaneous victory conditions
- Per-faction progress and score tracking
- Custom victory checkers for complex logic
- Automatic elimination victory detection
- Score-based victory with turn limits
- Event integration for UI updates

## AI Personality System

Personality-driven AI decision making with weighted behaviors, threat assessment, and goal management:

```c
#include "carbon/ai.h"

// Create AI system
Carbon_AISystem *ai = carbon_ai_create();

// Initialize per-faction AI state with personality
Carbon_AIState ai_state;
carbon_ai_state_init(&ai_state, CARBON_AI_AGGRESSIVE);

// Personalities affect behavior weights
// CARBON_AI_AGGRESSIVE: High aggression, low caution
// CARBON_AI_DEFENSIVE: High defense, high caution
// CARBON_AI_ECONOMIC: Focus on economy and trade
// CARBON_AI_EXPANSIONIST: Focus on territory expansion
// CARBON_AI_TECHNOLOGIST: Focus on research
// CARBON_AI_DIPLOMATIC: Prefers alliances
// CARBON_AI_OPPORTUNIST: Adapts to situations

// Register custom action evaluators (game-specific)
void evaluate_attacks(Carbon_AIState *state, void *game_ctx,
                      Carbon_AIAction *out, int *count, int max) {
    // Analyze game state and generate potential attack actions
    MyGame *game = game_ctx;
    *count = 0;

    for (int i = 0; i < game->enemy_count && *count < max; i++) {
        Enemy *enemy = &game->enemies[i];
        if (can_attack(game, enemy)) {
            out[*count].type = CARBON_AI_ACTION_ATTACK;
            out[*count].target_id = enemy->id;
            out[*count].priority = calculate_attack_value(game, enemy);
            out[*count].urgency = enemy->threat_level;
            (*count)++;
        }
    }
}
carbon_ai_register_evaluator(ai, CARBON_AI_ACTION_ATTACK, evaluate_attacks);
carbon_ai_register_evaluator(ai, CARBON_AI_ACTION_BUILD, evaluate_builds);
carbon_ai_register_evaluator(ai, CARBON_AI_ACTION_RESEARCH, evaluate_research);

// Register situation analyzer (updates ratios, morale)
void analyze_situation(Carbon_AIState *state, void *game_ctx) {
    MyGame *game = game_ctx;
    carbon_ai_set_ratios(state,
        our_resources / avg_resources,
        our_military / avg_military,
        our_tech / avg_tech);
    carbon_ai_set_morale(state, calculate_morale(game));
}
carbon_ai_set_situation_analyzer(ai, analyze_situation);

// Register threat assessor
void assess_threats(Carbon_AIState *state, void *game_ctx,
                    Carbon_AIThreat *threats, int *count, int max) {
    // Populate threat array based on game state
}
carbon_ai_set_threat_assessor(ai, assess_threats);

// Each turn, process AI decisions
Carbon_AIDecision decision;
carbon_ai_process_turn(ai, &ai_state, game, &decision);

// Execute returned actions
for (int i = 0; i < decision.action_count; i++) {
    Carbon_AIAction *action = &decision.actions[i];
    switch (action->type) {
        case CARBON_AI_ACTION_ATTACK:
            execute_attack(game, action->target_id);
            break;
        case CARBON_AI_ACTION_BUILD:
            execute_build(game, action->secondary_id);
            break;
        // ... handle other action types
    }
}

// Threat management
carbon_ai_add_threat(&ai_state, enemy_faction, 0.8f, our_base_id, 5.0f);
const Carbon_AIThreat *top_threat = carbon_ai_get_highest_threat(&ai_state);
float threat_level = carbon_ai_calculate_threat_level(&ai_state);

// Goal management
int goal_idx = carbon_ai_add_goal(&ai_state, GOAL_CONQUER_REGION, region_id, 0.9f);
carbon_ai_update_goal_progress(&ai_state, goal_idx, 0.5f);  // 50% complete
carbon_ai_complete_goal(&ai_state, goal_idx);
carbon_ai_cleanup_goals(&ai_state, 10);  // Remove goals older than 10 turns

// Cooldowns (prevent repetitive actions)
carbon_ai_set_cooldown(&ai_state, CARBON_AI_ACTION_DIPLOMACY, 5);  // 5 turn cooldown
if (!carbon_ai_is_on_cooldown(&ai_state, CARBON_AI_ACTION_DIPLOMACY)) {
    // Can attempt diplomacy
}

// Modify weights dynamically (e.g., in response to events)
Carbon_AIWeights boost = { .defense = 1.5f };  // 50% defense boost
carbon_ai_modify_weights(&ai_state, &boost);
carbon_ai_reset_weights(&ai_state);  // Reset to personality defaults

// Set strategic targets
carbon_ai_set_primary_target(&ai_state, enemy_faction_id);
carbon_ai_set_ally_target(&ai_state, friendly_faction_id);

// Deterministic random for reproducible AI behavior
carbon_ai_seed_random(&ai_state, game_seed);
float r = carbon_ai_random(&ai_state);  // 0.0 to 1.0
int choice = carbon_ai_random_int(&ai_state, 0, 5);  // 0 to 5

// Cleanup
carbon_ai_destroy(ai);
```

**Built-in personalities:**
- `CARBON_AI_BALANCED` - Equal weights across all behaviors
- `CARBON_AI_AGGRESSIVE` - High aggression, low caution
- `CARBON_AI_DEFENSIVE` - High defense and caution
- `CARBON_AI_ECONOMIC` - Focus on resource generation
- `CARBON_AI_EXPANSIONIST` - Prioritizes territory acquisition
- `CARBON_AI_TECHNOLOGIST` - Prioritizes research
- `CARBON_AI_DIPLOMATIC` - Prefers alliances
- `CARBON_AI_OPPORTUNIST` - Adapts based on situation

**Built-in action types:**
- `CARBON_AI_ACTION_BUILD` - Construct buildings/units
- `CARBON_AI_ACTION_ATTACK` - Attack enemies
- `CARBON_AI_ACTION_DEFEND` - Defend territory
- `CARBON_AI_ACTION_EXPAND` - Claim new territory
- `CARBON_AI_ACTION_RESEARCH` - Research technologies
- `CARBON_AI_ACTION_DIPLOMACY` - Diplomatic actions
- `CARBON_AI_ACTION_RECRUIT` - Train units
- `CARBON_AI_ACTION_RETREAT` - Withdraw from danger
- `CARBON_AI_ACTION_SCOUT` - Explore/gather intel
- `CARBON_AI_ACTION_TRADE` - Economic transactions
- `CARBON_AI_ACTION_UPGRADE` - Improve existing assets

**Key features:**
- Personality-driven behavior weights
- Custom evaluators for game-specific action scoring
- Threat assessment with distance/age decay
- Goal tracking with progress monitoring
- Situational modifiers (resource/military/tech ratios)
- Action cooldowns to prevent repetition
- Deterministic random for reproducible behavior
- Dynamic weight modification

## View Model System

Observable values with change detection for UI data binding:

```c
#include "carbon/viewmodel.h"

// Create view model
Carbon_ViewModel *vm = carbon_vm_create();

// Define observables
uint32_t health_id = carbon_vm_define_int(vm, "player_health", 100);
uint32_t gold_id = carbon_vm_define_int(vm, "gold", 0);
uint32_t name_id = carbon_vm_define_string(vm, "player_name", "Hero");
uint32_t pos_id = carbon_vm_define_vec2(vm, "position", 100.0f, 200.0f);
uint32_t color_id = carbon_vm_define_color(vm, "tint", 1.0f, 1.0f, 1.0f, 1.0f);

// Subscribe to changes
void on_health_changed(Carbon_ViewModel *vm, const Carbon_VMChangeEvent *event, void *userdata) {
    UI *ui = userdata;
    printf("Health changed: %d -> %d\n",
           event->old_value.i32, event->new_value.i32);
    ui_update_health_bar(ui, event->new_value.i32);
}
uint32_t listener = carbon_vm_subscribe(vm, health_id, on_health_changed, ui);

// Subscribe to ALL changes (for logging, debugging)
carbon_vm_subscribe_all(vm, log_all_changes, NULL);

// Update values (triggers callbacks if changed)
carbon_vm_set_int(vm, health_id, 75);   // Triggers on_health_changed
carbon_vm_set_int(vm, health_id, 75);   // No callback (same value)

// Batch updates (defer callbacks until commit)
carbon_vm_begin_batch(vm);
carbon_vm_set_int(vm, health_id, 50);
carbon_vm_set_int(vm, gold_id, 100);
carbon_vm_set_string(vm, name_id, "Champion");
carbon_vm_commit_batch(vm);  // All callbacks fire now

// Cancel batch (revert changes)
carbon_vm_begin_batch(vm);
carbon_vm_set_int(vm, health_id, 0);
carbon_vm_cancel_batch(vm);  // health_id reverts to 50

// Get values
int health = carbon_vm_get_int(vm, health_id);
const char *name = carbon_vm_get_string(vm, name_id);
Carbon_VMVec2 pos = carbon_vm_get_vec2(vm, pos_id);

// Lookup by name
uint32_t id = carbon_vm_find(vm, "player_health");

// Value validation
bool validate_health(Carbon_ViewModel *vm, uint32_t id,
                     const Carbon_VMValue *new_val, void *userdata) {
    return new_val->i32 >= 0 && new_val->i32 <= 100;  // Clamp 0-100
}
carbon_vm_set_validator(vm, health_id, validate_health, NULL);

// Custom formatting
int format_gold(Carbon_ViewModel *vm, uint32_t id, const Carbon_VMValue *val,
                char *buf, size_t size, void *userdata) {
    return snprintf(buf, size, "%d gold", val->i32);
}
carbon_vm_set_formatter(vm, gold_id, format_gold, NULL);

char buf[64];
carbon_vm_format(vm, gold_id, buf, sizeof(buf));  // "100 gold"

// Computed values (auto-update when dependencies change)
Carbon_VMValue compute_health_percent(Carbon_ViewModel *vm, uint32_t id, void *userdata) {
    int health = carbon_vm_get_int(vm, *(uint32_t*)userdata);
    Carbon_VMValue result = { .type = CARBON_VM_TYPE_FLOAT };
    result.f32 = (float)health / 100.0f;
    return result;
}
uint32_t deps[] = { health_id };
uint32_t percent_id = carbon_vm_define_computed(vm, "health_percent",
    CARBON_VM_TYPE_FLOAT, compute_health_percent, &health_id, deps, 1);

// Force notification (even if value unchanged)
carbon_vm_notify(vm, health_id);
carbon_vm_notify_all(vm);

// Unsubscribe
carbon_vm_unsubscribe(vm, listener);

// Cleanup
carbon_vm_destroy(vm);
```

**Supported value types:**
- `CARBON_VM_TYPE_INT` - 32-bit integer
- `CARBON_VM_TYPE_INT64` - 64-bit integer
- `CARBON_VM_TYPE_FLOAT` - Single precision float
- `CARBON_VM_TYPE_DOUBLE` - Double precision float
- `CARBON_VM_TYPE_BOOL` - Boolean
- `CARBON_VM_TYPE_STRING` - String (up to 256 chars)
- `CARBON_VM_TYPE_POINTER` - Void pointer (not owned)
- `CARBON_VM_TYPE_VEC2` - 2D vector (x, y)
- `CARBON_VM_TYPE_VEC3` - 3D vector (x, y, z)
- `CARBON_VM_TYPE_VEC4` - 4D vector / color (r, g, b, a)

**Key features:**
- Type-safe observable values with change detection
- Per-observable and global subscriptions
- Batch updates with commit/cancel
- Value validation before changes
- Custom formatting for display
- Computed values with automatic dependency tracking
- Event dispatcher integration

## Quick Start (New Game)

The recommended way to start a new game is using the game template:

```c
#include "carbon/game_context.h"
#include "game/game.h"

int main(int argc, char *argv[]) {
    // Configure
    Carbon_GameContextConfig config = CARBON_GAME_CONTEXT_DEFAULT;
    config.window_title = "My Game";
    config.font_path = "assets/fonts/font.ttf";

    // Create context (initializes all systems)
    Carbon_GameContext *ctx = carbon_game_context_create(&config);
    if (!ctx) return 1;

    // Create game (state machine, ECS systems)
    Game *game = game_init(ctx);

    // Main loop
    while (carbon_game_context_is_running(ctx)) {
        carbon_game_context_begin_frame(ctx);
        carbon_game_context_poll_events(ctx);
        game_update(game, ctx);
        // ... render ...
        carbon_game_context_end_frame(ctx);
    }

    game_shutdown(game);
    carbon_game_context_destroy(ctx);
    return 0;
}
```

See `src/main.c` for the complete bootstrap and `src/game/` for the template structure.

## Build Commands

```bash
# Build main game (uses game template)
make

# Build and run main game
make run

# Build and run comprehensive demo
make run-demo

# Build with debug symbols
make DEBUG=1

# Run examples
make example-minimal     # Minimal window setup
make example-sprites     # Sprite rendering
make example-animation   # Animation system
make example-tilemap     # Tilemap rendering
make example-ui          # UI widgets
make example-strategy    # RTS-style selection + pathfinding

# Clean
make clean

# Show help
make help
```

## Project Structure

```
src/
├── main.c                  # Game bootstrap (uses game template)
├── core/
│   ├── engine.c            # SDL3/GPU setup, frame management
│   ├── game_context.c      # Unified system container
│   └── error.c             # Error handling
├── game/                   # Game template
│   ├── game.h/c            # Game lifecycle
│   ├── components.h/c      # Game-specific ECS components
│   ├── systems/            # ECS systems (movement, collision, AI)
│   ├── states/             # State machine (menu, playing, paused)
│   └── data/               # JSON data loading
├── graphics/
│   ├── sprite.c            # Batched sprite rendering
│   ├── animation.c         # Sprite animation
│   ├── camera.c            # 2D camera
│   ├── tilemap.c           # Chunk-based tilemaps
│   └── text.c              # TrueType + SDF fonts
├── audio/audio.c           # Sound/music playback
├── input/input.c           # Action-based input
├── ecs/ecs.c               # Flecs wrapper
├── ai/pathfinding.c        # A* pathfinding
└── ui/                     # Immediate-mode UI

include/carbon/
├── carbon.h                # Core engine API
├── game_context.h          # Unified context
├── error.h                 # Error handling
├── helpers.h               # Convenience macros/utils
├── sprite.h, camera.h, etc.

examples/
├── minimal/                # ~40 lines window setup
├── demo/                   # Full feature demo
├── sprites/                # Sprite rendering
├── animation/              # Animation system
├── tilemap/                # Tilemap rendering
├── ui/                     # UI widgets
└── strategy/               # RTS patterns

assets/schemas/             # JSON schemas for data files
```

## Common Patterns

### Adding a New Entity Type

1. Define component in `src/game/components.h`:
```c
typedef struct C_Projectile {
    ecs_entity_t owner;
    float lifetime;
} C_Projectile;
ECS_COMPONENT_DECLARE(C_Projectile);
```

2. Register in `src/game/components.c`:
```c
ECS_COMPONENT_DEFINE(world, C_Projectile);
```

3. Create system in `src/game/systems/`:
```c
void ProjectileSystem(ecs_iter_t *it) {
    C_Projectile *proj = ecs_field(it, C_Projectile, 0);
    for (int i = 0; i < it->count; i++) {
        proj[i].lifetime -= it->delta_time;
        if (proj[i].lifetime <= 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}
```

4. Register system in `src/game/systems/systems.c`:
```c
ECS_SYSTEM(world, ProjectileSystem, EcsOnUpdate, C_Projectile);
```

### Adding a New Game State

1. Create state file `src/game/states/gameover.c`:
```c
static void gameover_enter(Carbon_GameContext *ctx, void *userdata) { }
static void gameover_exit(Carbon_GameContext *ctx, void *userdata) { }
static void gameover_update(Carbon_GameContext *ctx, float dt, void *userdata) { }
static void gameover_render(Carbon_GameContext *ctx,
                             SDL_GPUCommandBuffer *cmd,
                             SDL_GPURenderPass *pass,
                             void *userdata) { }

GameState game_state_gameover_create(void) {
    return (GameState){
        .name = "GameOver",
        .enter = gameover_enter,
        .exit = gameover_exit,
        .update = gameover_update,
        .render = gameover_render,
        .userdata = NULL
    };
}
```

2. Add to state enum in `src/game/states/state.h`:
```c
typedef enum {
    // ...existing states...
    GAME_STATE_GAME_OVER,
} GameStateID;
```

3. Register in `src/game/game.c`:
```c
GameState gameover_state = game_state_gameover_create();
game_state_machine_register(sm, GAME_STATE_GAME_OVER, &gameover_state);
```

### Loading Level Data

```c
#include "game/data/loader.h"

LevelData level;
if (game_load_level("assets/levels/level1.json", &level)) {
    // Set up tilemap
    for (int i = 0; i < level.width * level.height; i++) {
        int x = i % level.width;
        int y = i / level.width;
        carbon_tilemap_set_tile(tilemap, ground, x, y, level.tiles[i]);
    }

    // Spawn entities
    for (int i = 0; i < level.spawn_count; i++) {
        spawn_entity(level.spawns[i].type,
                     level.spawns[i].x,
                     level.spawns[i].y);
    }

    game_free_level(&level);
}
```

## Troubleshooting

### Build Issues

**SDL3 not found:**
```bash
# macOS
brew install sdl3

# Linux - build from source
git clone https://github.com/libsdl-org/SDL
cd SDL && mkdir build && cd build
cmake .. && make && sudo make install
```

**Font not loading:**
- Check path is relative to working directory (usually project root)
- Ensure font file exists: `ls assets/fonts/`

### Runtime Issues

**Black screen / no rendering:**
- Check `carbon_begin_render_pass()` return value
- Ensure upload calls happen before render pass
- Verify command buffer is acquired

**Sprites not showing:**
1. Check texture loaded: `if (!tex) { SDL_Log("Error"); }`
2. Ensure sprite batch: `carbon_sprite_begin()` → draw → `carbon_sprite_upload()` → render pass → `carbon_sprite_render()`
3. Check camera position if sprites are in world space

**Input not working:**
- Ensure `carbon_input_begin_frame()` is called at start of frame
- Call `carbon_input_update()` after polling events
- Check UI isn't consuming events: `if (cui_process_event(ui, &event)) continue;`

### Error Handling

```c
#include "carbon/error.h"

Carbon_Texture *tex = carbon_texture_load(sprites, "missing.png");
if (!tex) {
    SDL_Log("Failed: %s", carbon_get_last_error());
}
```

## Development Notes

- Target platforms: macOS, Linux, Windows
- Language: C11
- Graphics: SDL3 SDL_GPU (auto-selects Metal/Vulkan/D3D12)
- ECS: Flecs (high-performance Entity Component System)
- Math: cglm (SIMD-optimized)
- Repository: https://github.com/PhilipLudington/Carbon

## Additional Documentation

- `SYSTEMS.md` - System dependencies and initialization order
- `examples/*/README.md` - Per-example documentation
- `assets/schemas/` - JSON schemas for data files
