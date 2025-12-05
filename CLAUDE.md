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
│   ├── camera3d.c      # 3D orbital camera with animations
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
│   ├── unlock.c        # Unlock/achievement system
│   ├── spatial.c       # Spatial hash index for entity lookup
│   ├── fog.c           # Fog of war / exploration system
│   ├── rate.c          # Rate tracking / rolling metrics
│   ├── network.c       # Network/graph system (power grid)
│   ├── blueprint.c     # Blueprint system for building templates
│   ├── construction.c  # Ghost building / construction queue
│   ├── dialog.c        # Dialog / narrative system
│   ├── game_speed.c    # Variable simulation speed control
│   ├── crafting.c      # Crafting state machine
│   └── biome.c         # Biome/terrain system
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
├── camera3d.h          # 3D orbital camera API
├── tilemap.h           # Tilemap system API
├── text.h              # Text rendering API
├── input.h             # Input system with action mapping
├── audio.h             # Audio system API
├── pathfinding.h       # A* pathfinding API
├── ai.h                # AI personality system API
├── task.h              # Task queue system API
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
├── viewmodel.h         # View model/data binding API
├── spatial.h           # Spatial hash index API
├── fog.h               # Fog of war / exploration API
├── rate.h              # Rate tracking / rolling metrics API
├── network.h           # Network/graph system API
├── blueprint.h         # Blueprint system API
├── construction.h      # Ghost building / construction queue API
├── dialog.h            # Dialog / narrative system API
├── game_speed.h        # Variable simulation speed API
├── crafting.h          # Crafting state machine API
└── biome.h             # Biome/terrain system API

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

## Spatial Hash Index

O(1) entity lookup by grid cell for efficient spatial queries. Useful for item pickup, collision detection, and proximity queries:

```c
#include "carbon/spatial.h"

// Create spatial index (capacity = expected occupied cells)
Carbon_SpatialIndex *spatial = carbon_spatial_create(256);

// Add entities at grid positions
carbon_spatial_add(spatial, 10, 20, player_entity);
carbon_spatial_add(spatial, 10, 20, item_entity);  // Multiple entities per cell

// Check for entities
if (carbon_spatial_has(spatial, 10, 20)) {
    uint32_t first = carbon_spatial_query(spatial, 10, 20);  // Get first entity
}

// Get all entities at a position
uint32_t entities[16];
int count = carbon_spatial_query_all(spatial, 10, 20, entities, 16);

// Check for specific entity
if (carbon_spatial_has_entity(spatial, 10, 20, player_entity)) {
    // Player is at this cell
}

// Move entity between cells
carbon_spatial_move(spatial, old_x, old_y, new_x, new_y, entity_id);

// Remove entity
carbon_spatial_remove(spatial, 10, 20, entity_id);

// Region queries (rectangular area)
Carbon_SpatialQueryResult results[64];
int found = carbon_spatial_query_rect(spatial, x1, y1, x2, y2, results, 64);
for (int i = 0; i < found; i++) {
    uint32_t entity = results[i].entity_id;
    int ex = results[i].x;
    int ey = results[i].y;
    // Process entity...
}

// Radius query (Chebyshev distance - square area)
found = carbon_spatial_query_radius(spatial, center_x, center_y, 5, results, 64);

// Circle query (Euclidean distance - true circle)
found = carbon_spatial_query_circle(spatial, center_x, center_y, 5, results, 64);

// Iterate entities at a cell
Carbon_SpatialIterator iter = carbon_spatial_iter_begin(spatial, x, y);
while (carbon_spatial_iter_valid(&iter)) {
    uint32_t entity = carbon_spatial_iter_get(&iter);
    // Process entity...
    carbon_spatial_iter_next(&iter);
}

// Statistics
int total = carbon_spatial_total_count(spatial);      // All entities
int cells = carbon_spatial_occupied_cells(spatial);   // Cells with entities
float load = carbon_spatial_load_factor(spatial);     // Hash table load

// Clear all entities
carbon_spatial_clear(spatial);

// Cleanup
carbon_spatial_destroy(spatial);
```

**Key features:**
- O(1) add, remove, query, move operations
- Multiple entities per cell (up to `CARBON_SPATIAL_MAX_PER_CELL`, default 16)
- Rectangular and circular region queries
- Automatic hash table growth when load factor exceeds 0.7
- Iterator for processing all entities at a cell

**Common patterns:**

```c
// Item pickup - find items at player position
if (carbon_spatial_has(items_index, player_x, player_y)) {
    uint32_t item = carbon_spatial_query(items_index, player_x, player_y);
    pickup_item(player, item);
    carbon_spatial_remove(items_index, player_x, player_y, item);
}

// Collision detection - check if destination is occupied
if (!carbon_spatial_has(units_index, dest_x, dest_y)) {
    carbon_spatial_move(units_index, unit_x, unit_y, dest_x, dest_y, unit_id);
}

// Find nearby enemies
Carbon_SpatialQueryResult nearby[32];
int count = carbon_spatial_query_circle(enemies_index, unit_x, unit_y,
                                         attack_range, nearby, 32);
for (int i = 0; i < count; i++) {
    attack(unit_id, nearby[i].entity_id);
}
```

**Performance notes:**
- Initial capacity should be ~1.5-2x expected occupied cells
- Hash table grows automatically, but pre-sizing avoids rehashing
- Region queries iterate all cells in range (O(area))
- Use separate indices for different entity types if counts are large

## Fog of War / Exploration System

Per-cell exploration tracking with visibility radius. Supports three visibility states: unexplored, explored (shroud), and visible:

```c
#include "carbon/fog.h"

// Create fog of war system (matching map dimensions)
Carbon_FogOfWar *fog = carbon_fog_create(100, 100);

// Set shroud alpha (explored but not visible cells)
carbon_fog_set_shroud_alpha(fog, 0.5f);  // 50% visible

// Add vision sources (units, buildings, scouts)
Carbon_VisionSource unit_vision = carbon_fog_add_source(fog, 50, 50, 8);  // Radius 8
Carbon_VisionSource base_vision = carbon_fog_add_source(fog, 10, 10, 12); // Larger radius

// Move vision source when unit moves
carbon_fog_move_source(fog, unit_vision, new_x, new_y);

// Update radius (e.g., upgrades)
carbon_fog_set_source_radius(fog, unit_vision, 10);

// Remove vision source when unit dies
carbon_fog_remove_source(fog, unit_vision);

// Update visibility (call each frame or after moving sources)
carbon_fog_update(fog);

// Query visibility for rendering
Carbon_VisibilityState state = carbon_fog_get_state(fog, tile_x, tile_y);
switch (state) {
    case CARBON_VIS_UNEXPLORED:
        // Don't render tile at all
        break;
    case CARBON_VIS_EXPLORED:
        // Render with shroud/fog overlay
        break;
    case CARBON_VIS_VISIBLE:
        // Render fully
        break;
}

// Convenience checks
if (carbon_fog_is_visible(fog, x, y)) { }     // Currently visible
if (carbon_fog_is_explored(fog, x, y)) { }    // Explored or visible
if (carbon_fog_is_unexplored(fog, x, y)) { }  // Never seen

// Get alpha for sprite tinting
float alpha = carbon_fog_get_alpha(fog, x, y);  // 0.0, shroud_alpha, or 1.0
carbon_sprite_draw_tinted(sr, &sprite, x * 32, y * 32, alpha, alpha, alpha, 1.0f);

// Region queries
if (carbon_fog_any_visible_in_rect(fog, x1, y1, x2, y2)) {
    // At least one cell is visible (for enemy building detection)
}
int visible_count = carbon_fog_count_visible_in_rect(fog, x1, y1, x2, y2);

// Manual exploration (reveal map areas)
carbon_fog_explore_cell(fog, x, y);              // Single cell
carbon_fog_explore_rect(fog, 0, 0, 10, 10);      // Rectangle
carbon_fog_explore_circle(fog, 50, 50, 5);       // Circle

// Cheat/debug commands
carbon_fog_reveal_all(fog);   // Reveal entire map
carbon_fog_explore_all(fog);  // Mark all as explored (but not visible)
carbon_fog_reset(fog);        // Reset to unexplored

// Statistics
float explored_pct = carbon_fog_get_exploration_percent(fog);  // 0.0 to 1.0
int unexplored, explored, visible;
carbon_fog_get_stats(fog, &unexplored, &explored, &visible);

// Exploration callback (for achievements, events)
void on_cell_explored(Carbon_FogOfWar *fog, int x, int y, void *userdata) {
    printf("Explored cell (%d, %d)!\n", x, y);
}
carbon_fog_set_exploration_callback(fog, on_cell_explored, NULL);

// Cleanup
carbon_fog_destroy(fog);
```

**Line of Sight (Optional):**

Enable LOS checking so vision is blocked by walls/obstacles:

```c
// Callback to check if a cell blocks vision
bool is_wall(int x, int y, void *userdata) {
    Tilemap *map = userdata;
    return tilemap_get_tile(map, x, y) == TILE_WALL;
}

// Enable LOS checking
carbon_fog_set_los_callback(fog, is_wall, tilemap);

// Now visibility is blocked by walls
carbon_fog_update(fog);

// Check LOS between two points
if (carbon_fog_has_los(fog, unit_x, unit_y, target_x, target_y)) {
    // Can see target
}
```

**Key features:**
- Three visibility states: unexplored, explored (shroud), visible
- Multiple vision sources with independent radii
- Automatic dirty tracking for efficient updates
- Optional line-of-sight checking with blocker callback
- Exploration callbacks for achievements/events
- Statistics for exploration progress

**Integration with rendering:**

```c
// In render loop - cull tiles outside camera, then check visibility
for (int y = min_y; y <= max_y; y++) {
    for (int x = min_x; x <= max_x; x++) {
        Carbon_VisibilityState vis = carbon_fog_get_state(fog, x, y);
        if (vis == CARBON_VIS_UNEXPLORED) continue;  // Skip unexplored

        // Draw tile with fog alpha
        float alpha = (vis == CARBON_VIS_VISIBLE) ? 1.0f : fog_shroud_alpha;
        carbon_sprite_draw_tinted(sr, &tile_sprite, x * 32, y * 32,
                                   alpha, alpha, alpha, 1.0f);
    }
}
```

## Rate Tracking System

Rolling window metrics for production and consumption rates. Useful for economy statistics, performance monitoring, and analytics displays:

```c
#include "carbon/rate.h"

// Create rate tracker
// 8 metrics, sample every 0.5 seconds, keep 120 samples = 60 seconds of history
Carbon_RateTracker *rates = carbon_rate_create(8, 0.5f, 120);

// Name metrics for debugging
carbon_rate_set_name(rates, 0, "Gold");
carbon_rate_set_name(rates, 1, "Iron");
carbon_rate_set_name(rates, 2, "Food");
carbon_rate_set_name(rates, 3, "Power");

// In game loop: update each frame
carbon_rate_update(rates, delta_time);

// Record production/consumption as they happen
carbon_rate_record_production(rates, 0, 100);  // Produced 100 gold
carbon_rate_record_consumption(rates, 0, 50);  // Consumed 50 gold
carbon_rate_record(rates, 1, 20, 15);          // Iron: produced 20, consumed 15

// Query rates over different time windows
float gold_per_sec = carbon_rate_get_production_rate(rates, 0, 10.0f);  // Last 10s
float gold_drain = carbon_rate_get_consumption_rate(rates, 0, 10.0f);
float gold_net = carbon_rate_get_net_rate(rates, 0, 10.0f);  // Production - consumption

// Get comprehensive stats
Carbon_RateStats stats = carbon_rate_get_stats(rates, 0, 30.0f);  // Last 30 seconds
printf("Gold: +%.1f/s (min %d, max %d)\n",
       stats.production_rate, stats.min_production, stats.max_production);
printf("Total: produced %d, consumed %d, net %d\n",
       stats.total_produced, stats.total_consumed, stats.total_net);

// Aggregate queries
int32_t total_prod = carbon_rate_get_total_production(rates, 0, 60.0f);  // Last minute
int32_t min_prod = carbon_rate_get_min_production(rates, 0, 60.0f);
int32_t max_prod = carbon_rate_get_max_production(rates, 0, 60.0f);
float avg_prod = carbon_rate_get_avg_production(rates, 0, 60.0f);

// Get sample history for graphing
Carbon_RateSample samples[120];
int count = carbon_rate_get_history(rates, 0, 30.0f, samples, 120);
for (int i = 0; i < count; i++) {
    // Draw point at (samples[i].timestamp, samples[i].produced)
}

// Get latest sample
Carbon_RateSample latest;
if (carbon_rate_get_latest_sample(rates, 0, &latest)) {
    printf("Latest: +%d/-%d at %.1fs\n",
           latest.produced, latest.consumed, latest.timestamp);
}

// For turn-based games: force sample at end of turn
carbon_rate_force_sample(rates);

// Configuration queries
float interval = carbon_rate_get_interval(rates);      // 0.5
int history = carbon_rate_get_history_size(rates);     // 120
float max_window = carbon_rate_get_max_time_window(rates);  // 60.0

// Reset or cleanup
carbon_rate_reset(rates);
carbon_rate_destroy(rates);
```

**Key features:**
- Multiple metrics tracked independently
- Automatic periodic sampling at configurable intervals
- Time window queries (last 10s, 30s, 60s, etc.)
- Production/consumption tracking with net calculation
- Min/max/mean/sum statistics
- Sample history for UI graphs
- Circular buffer for constant memory usage

**Common patterns:**

```c
// Economy overview UI
void draw_economy_panel(Carbon_RateTracker *rates) {
    for (int i = 0; i < carbon_rate_get_metric_count(rates); i++) {
        const char *name = carbon_rate_get_name(rates, i);
        float rate = carbon_rate_get_net_rate(rates, i, 10.0f);

        // Color based on positive/negative
        uint32_t color = rate >= 0 ? COLOR_GREEN : COLOR_RED;
        cui_label_colored(ui, name, color);
        cui_label_printf(ui, "%+.1f/s", rate);
    }
}

// Power grid status
float power_production = carbon_rate_get_production_rate(rates, POWER, 5.0f);
float power_consumption = carbon_rate_get_consumption_rate(rates, POWER, 5.0f);
if (power_consumption > power_production) {
    show_warning("Power deficit!");
}
```

## Network/Graph System (Power Grid)

Union-find based network system for connected component grouping with resource distribution. Useful for power grids, water pipes, or any system where nodes form connected groups and share resources:

```c
#include "carbon/network.h"

// Create network system
Carbon_NetworkSystem *network = carbon_network_create();

// Add nodes (e.g., power poles, substations)
// Each node has a position and coverage radius
uint32_t generator = carbon_network_add_node(network, 10, 10, 5);   // Radius 5
uint32_t pole1 = carbon_network_add_node(network, 15, 10, 3);       // Radius 3
uint32_t pole2 = carbon_network_add_node(network, 20, 10, 3);
uint32_t consumer = carbon_network_add_node(network, 25, 10, 2);

// Set production/consumption values
carbon_network_set_production(network, generator, 100);    // Generates 100 power
carbon_network_set_consumption(network, consumer, 30);     // Consumes 30 power

// Nodes connect when their coverage overlaps (distance <= radius1 + radius2)
// After adding/moving nodes, update to recalculate connectivity
carbon_network_update(network);

// Query network state
uint32_t group = carbon_network_get_group(network, consumer);  // Get group ID

Carbon_NetworkGroup group_info;
if (carbon_network_get_group_info(network, group, &group_info)) {
    printf("Group %u: %d nodes, balance %d (%s)\n",
           group_info.id,
           group_info.node_count,
           group_info.balance,
           group_info.powered ? "powered" : "unpowered");
}

// Check if specific node is powered
if (carbon_network_node_is_powered(network, consumer)) {
    printf("Consumer has power!\n");
}

// Check if a cell has power coverage
if (carbon_network_cell_is_powered(network, building_x, building_y)) {
    printf("Building location has power!\n");
}

// Move a node (may change connectivity)
carbon_network_move_node(network, pole1, 18, 12);
carbon_network_update(network);

// Deactivate a node (e.g., damaged)
carbon_network_set_active(network, pole1, false);
carbon_network_update(network);

// Get all nodes covering a cell
Carbon_NetworkCoverage coverage[8];
int count = carbon_network_get_coverage(network, 12, 10, coverage, 8);
for (int i = 0; i < count; i++) {
    printf("Covered by node %u at (%d, %d), distance %d\n",
           coverage[i].node_id, coverage[i].x, coverage[i].y, coverage[i].distance);
}

// Find nearest node to position
uint32_t nearest = carbon_network_get_nearest_node(network, 50, 50, 20);  // Max distance 20
if (nearest != CARBON_NETWORK_INVALID) {
    printf("Nearest node: %u\n", nearest);
}

// Get all nodes in a group
uint32_t nodes[64];
int node_count = carbon_network_get_group_nodes(network, group, nodes, 64);

// Get all groups
uint32_t groups[32];
int group_count = carbon_network_get_all_groups(network, groups, 32);

// Statistics
int total_nodes, active_nodes, num_groups, powered_groups;
carbon_network_get_stats(network, &total_nodes, &active_nodes, &num_groups, &powered_groups);

int32_t total_prod = carbon_network_total_production(network);
int32_t total_cons = carbon_network_total_consumption(network);
int32_t balance = carbon_network_total_balance(network);

// Callback for group changes
void on_network_change(Carbon_NetworkSystem *net, uint32_t node_id,
                       uint32_t old_group, uint32_t new_group, void *userdata) {
    printf("Node %u moved from group %u to group %u\n", node_id, old_group, new_group);
}
carbon_network_set_callback(network, on_network_change, NULL);

// Remove node
carbon_network_remove_node(network, pole1);
carbon_network_update(network);

// Cleanup
carbon_network_destroy(network);
```

**Key features:**
- Union-find algorithm with path compression (O(α(n)) ≈ O(1) connectivity)
- Nodes connect when coverage areas overlap (Chebyshev distance)
- Per-node production/consumption tracking
- Group balance calculation (production - consumption)
- Coverage queries for cells
- Lazy recalculation with dirty tracking
- Callback for group membership changes

**Common patterns:**

```c
// Power grid for buildings
void place_building(Game *game, int x, int y, BuildingType type) {
    // Check if location has power
    if (!carbon_network_cell_is_powered(game->power_grid, x, y)) {
        show_warning("No power at this location!");
        return;
    }

    // Create building
    Building *b = create_building(game, x, y, type);

    // If building produces/consumes power, add as network node
    if (type == BUILDING_GENERATOR) {
        b->power_node = carbon_network_add_node(game->power_grid, x, y, 8);
        carbon_network_set_production(game->power_grid, b->power_node, 100);
    } else if (type == BUILDING_FACTORY) {
        b->power_node = carbon_network_add_node(game->power_grid, x, y, 2);
        carbon_network_set_consumption(game->power_grid, b->power_node, 25);
    }

    carbon_network_update(game->power_grid);
}

// Check grid status each frame
void update_power_status(Game *game) {
    carbon_network_update(game->power_grid);

    // Check for power deficit
    if (carbon_network_total_balance(game->power_grid) < 0) {
        show_power_warning();
    }
}
```

## Blueprint System

Save and place building templates with relative positioning. Supports capturing selections, rotation, mirroring, and placement validation:

```c
#include "carbon/blueprint.h"

// Create a blueprint
Carbon_Blueprint *bp = carbon_blueprint_create("Solar Farm");

// Add entries manually (relative positions from origin)
carbon_blueprint_add_entry(bp, 0, 0, BUILDING_SOLAR_PANEL, 0);  // Origin
carbon_blueprint_add_entry(bp, 1, 0, BUILDING_SOLAR_PANEL, 0);  // Right
carbon_blueprint_add_entry(bp, 0, 1, BUILDING_SOLAR_PANEL, 0);  // Below
carbon_blueprint_add_entry(bp, 1, 1, BUILDING_SOLAR_PANEL, 0);  // Diagonal

// Or capture from existing buildings in the world
bool capture_building(int x, int y, uint16_t *type, uint8_t *dir,
                      uint32_t *metadata, void *userdata) {
    Building *b = get_building_at(x, y);
    if (!b) return false;
    *type = b->type;
    *dir = b->direction;
    *metadata = b->extra_data;
    return true;
}
int captured = carbon_blueprint_capture(bp, 10, 10, 15, 15,
                                         capture_building, game);

// Transform the blueprint
carbon_blueprint_rotate_cw(bp);       // 90 degrees clockwise
carbon_blueprint_rotate_ccw(bp);      // 90 degrees counter-clockwise
carbon_blueprint_rotate(bp, CARBON_BLUEPRINT_ROT_180);
carbon_blueprint_mirror_x(bp);        // Flip horizontally
carbon_blueprint_mirror_y(bp);        // Flip vertically
carbon_blueprint_normalize(bp);       // Move origin to top-left

// Query blueprint info
const char *name = carbon_blueprint_get_name(bp);
int count = carbon_blueprint_get_entry_count(bp);
int width, height;
carbon_blueprint_get_bounds(bp, &width, &height);

// Get individual entries
const Carbon_BlueprintEntry *entry = carbon_blueprint_get_entry(bp, 0);
printf("Entry at (%d, %d): type %d, dir %d\n",
       entry->rel_x, entry->rel_y, entry->building_type, entry->direction);

// Validate placement
bool can_place(int x, int y, uint16_t type, uint8_t dir, void *userdata) {
    return is_cell_empty(x, y) && is_terrain_buildable(x, y);
}
Carbon_BlueprintPlacement result = carbon_blueprint_can_place(bp, 50, 50,
                                                               can_place, game);
if (result.valid) {
    printf("Can place all %d entries\n", result.valid_count);
} else {
    printf("Cannot place: %d invalid entries, first at index %d\n",
           result.invalid_count, result.first_invalid_index);
}

// Place the blueprint
void place_building_at(int x, int y, uint16_t type, uint8_t dir,
                       uint32_t metadata, void *userdata) {
    create_building(x, y, type, dir);
}
int placed = carbon_blueprint_place(bp, 50, 50, place_building_at, game);

// Clone and modify
Carbon_Blueprint *copy = carbon_blueprint_clone(bp);
carbon_blueprint_set_name(copy, "Solar Farm (Rotated)");
carbon_blueprint_rotate_cw(copy);

// Cleanup
carbon_blueprint_destroy(bp);
carbon_blueprint_destroy(copy);
```

**Blueprint Library (managing multiple blueprints):**

```c
// Create library
Carbon_BlueprintLibrary *library = carbon_blueprint_library_create(0);

// Add blueprints (library takes ownership)
Carbon_Blueprint *bp1 = carbon_blueprint_create("Factory Setup");
// ... add entries ...
uint32_t handle1 = carbon_blueprint_library_add(library, bp1);

Carbon_Blueprint *bp2 = carbon_blueprint_create("Defense Tower");
// ... add entries ...
uint32_t handle2 = carbon_blueprint_library_add(library, bp2);

// Find by name
uint32_t found = carbon_blueprint_library_find(library, "Factory Setup");
if (found != CARBON_BLUEPRINT_INVALID) {
    const Carbon_Blueprint *bp = carbon_blueprint_library_get_const(library, found);
    // Use blueprint...
}

// Get all handles
uint32_t handles[64];
int count = carbon_blueprint_library_get_all(library, handles, 64);
for (int i = 0; i < count; i++) {
    Carbon_Blueprint *bp = carbon_blueprint_library_get(library, handles[i]);
    printf("Blueprint: %s\n", carbon_blueprint_get_name(bp));
}

// Remove blueprint
carbon_blueprint_library_remove(library, handle1);

// Clear all
carbon_blueprint_library_clear(library);

// Cleanup (destroys all contained blueprints)
carbon_blueprint_library_destroy(library);
```

**Key features:**
- Capture existing building layouts into reusable templates
- Relative positioning with automatic normalization
- Rotation (90°/180°/270°) and mirroring transformations
- Entry direction automatically rotated with blueprint
- Placement validation with detailed feedback
- Blueprint library for managing collections
- Callbacks for flexible integration with game systems

**Common patterns:**

```c
// Ghost preview - render blueprint at cursor before placement
void render_blueprint_preview(Game *game, Carbon_Blueprint *bp, int cursor_x, int cursor_y) {
    Carbon_BlueprintPlacement validity = carbon_blueprint_can_place(bp, cursor_x, cursor_y,
                                                                     can_place_validator, game);

    int count = carbon_blueprint_get_entry_count(bp);
    for (int i = 0; i < count; i++) {
        const Carbon_BlueprintEntry *entry = carbon_blueprint_get_entry(bp, i);
        int world_x, world_y;
        carbon_blueprint_entry_to_world(entry, cursor_x, cursor_y, &world_x, &world_y);

        // Draw ghost sprite (green if valid, red if invalid)
        bool entry_valid = can_place_validator(world_x, world_y,
                                                entry->building_type,
                                                entry->direction, game);
        uint32_t tint = entry_valid ? 0x8000FF00 : 0x80FF0000;
        draw_building_ghost(world_x, world_y, entry->building_type, tint);
    }
}

// Save/load blueprints (serialize entries)
void save_blueprint(Carbon_Blueprint *bp, FILE *f) {
    fprintf(f, "name=%s\n", carbon_blueprint_get_name(bp));
    int count = carbon_blueprint_get_entry_count(bp);
    fprintf(f, "count=%d\n", count);
    for (int i = 0; i < count; i++) {
        const Carbon_BlueprintEntry *e = carbon_blueprint_get_entry(bp, i);
        fprintf(f, "%d,%d,%d,%d,%u\n", e->rel_x, e->rel_y,
                e->building_type, e->direction, e->metadata);
    }
}
```

## Construction Queue / Ghost Building System

Planned buildings with progress tracking before actual construction. Supports ghost/preview buildings, construction progress, speed modifiers, and completion callbacks:

```c
#include "carbon/construction.h"

// Create construction queue
Carbon_ConstructionQueue *queue = carbon_construction_create(32);

// Add ghost building (planned, not yet constructed)
uint32_t ghost = carbon_construction_add_ghost(queue, 10, 20, BUILDING_FACTORY, 0);

// Or with extended options (duration, faction)
uint32_t ghost2 = carbon_construction_add_ghost_ex(queue, 15, 20,
    BUILDING_POWERPLANT, 0,   // type, direction
    30.0f,                     // base duration in seconds
    player_faction);           // owning faction

// Start construction (changes status from PENDING to CONSTRUCTING)
carbon_construction_start(queue, ghost);

// Set construction speed (based on workers assigned, upgrades, etc.)
carbon_construction_set_speed(queue, ghost, 1.5f);  // 1.5x normal speed

// Assign a builder entity
carbon_construction_set_builder(queue, ghost, worker_entity);

// In game loop - update construction progress:
carbon_construction_update(queue, delta_time);

// Query progress
float progress = carbon_construction_get_progress(queue, ghost);  // 0.0 to 1.0
float remaining = carbon_construction_get_remaining_time(queue, ghost);  // seconds

// Check for completion
if (carbon_construction_is_complete(queue, ghost)) {
    Carbon_Ghost *g = carbon_construction_get_ghost(queue, ghost);
    create_actual_building(g->x, g->y, g->building_type, g->direction);
    carbon_construction_remove_ghost(queue, ghost);
}

// Pause/resume construction
carbon_construction_pause(queue, ghost);
carbon_construction_resume(queue, ghost);

// Cancel construction (triggers callback with CANCELLED status)
carbon_construction_cancel_ghost(queue, ghost);

// Find ghost at position
uint32_t found = carbon_construction_find_at(queue, 10, 20);
if (found != CARBON_GHOST_INVALID) {
    // Ghost exists at this position
}

// Check if position has a ghost
if (carbon_construction_has_ghost_at(queue, 10, 20)) {
    // Cannot place another building here
}

// Worker-based construction (add progress directly)
carbon_construction_add_progress(queue, ghost, 0.01f);  // Add 1% progress

// Instant completion (cheat/ability)
carbon_construction_complete_instant(queue, ghost);

// Query by faction
uint32_t handles[64];
int count = carbon_construction_get_by_faction(queue, player_faction, handles, 64);
int active = carbon_construction_count_active_by_faction(queue, player_faction);

// Query by builder
int builder_jobs = carbon_construction_find_by_builder(queue, worker_entity, handles, 64);

// Queue statistics
int total = carbon_construction_count(queue);
int constructing = carbon_construction_count_active(queue);
int complete = carbon_construction_count_complete(queue);
bool full = carbon_construction_is_full(queue);
int capacity = carbon_construction_capacity(queue);

// Cleanup
carbon_construction_destroy(queue);
```

**Completion callbacks:**

```c
// Callback when ghost completes or is cancelled
void on_construction_done(Carbon_ConstructionQueue *queue,
                          const Carbon_Ghost *ghost, void *userdata) {
    Game *game = userdata;

    if (ghost->status == CARBON_GHOST_COMPLETE) {
        printf("Completed %s at (%d, %d)\n",
               building_name(ghost->building_type),
               ghost->x, ghost->y);
        // Create actual building, play sound, etc.
        create_building(game, ghost->x, ghost->y,
                        ghost->building_type, ghost->direction);
    } else if (ghost->status == CARBON_GHOST_CANCELLED) {
        printf("Construction cancelled\n");
        // Refund resources, etc.
        refund_building_cost(game, ghost->building_type);
    }
}
carbon_construction_set_callback(queue, on_construction_done, game);
```

**Condition callbacks (resource checking):**

```c
// Only allow construction when resources are available
bool can_continue_building(Carbon_ConstructionQueue *queue,
                            const Carbon_Ghost *ghost, void *userdata) {
    Game *game = userdata;
    // Check if we have resources to continue
    // (e.g., power, workers, materials)
    return has_construction_resources(game, ghost->faction_id);
}
carbon_construction_set_condition_callback(queue, can_continue_building, game);
```

**Ghost statuses:**
- `CARBON_GHOST_PENDING` - Waiting to start construction
- `CARBON_GHOST_CONSTRUCTING` - Construction in progress
- `CARBON_GHOST_COMPLETE` - Construction finished
- `CARBON_GHOST_CANCELLED` - Construction cancelled
- `CARBON_GHOST_PAUSED` - Construction paused

**Key features:**
- Progress-based construction with time duration
- Speed multipliers for upgrades and worker bonuses
- Builder assignment for worker-based construction
- Faction tracking for multiplayer/AI
- Pause/resume/cancel support
- Condition callbacks for resource checking
- Completion callbacks for game integration
- Metadata and userdata for game-specific extensions

**Common patterns:**

```c
// Ghost preview rendering
void render_ghosts(Game *game, Carbon_ConstructionQueue *queue) {
    uint32_t handles[64];
    int count = carbon_construction_get_all(queue, handles, 64);

    for (int i = 0; i < count; i++) {
        const Carbon_Ghost *g = carbon_construction_get_ghost_const(queue, handles[i]);

        // Draw ghost sprite with transparency
        float alpha = 0.5f + 0.3f * g->progress;  // More opaque as construction progresses
        draw_building_ghost(g->x, g->y, g->building_type, g->direction, alpha);

        // Draw progress bar
        if (g->status == CARBON_GHOST_CONSTRUCTING) {
            draw_progress_bar(g->x, g->y, g->progress);
        }
    }
}

// Assign workers to construction
void assign_worker_to_building(Game *game, uint32_t worker, uint32_t ghost) {
    carbon_construction_set_builder(game->construction, ghost, worker);
    // Set worker task to build
    carbon_task_queue_add_build(get_worker_tasks(game, worker),
                                 ghost_x, ghost_y, 0);
}

// Calculate speed based on workers
void update_construction_speed(Game *game) {
    uint32_t handles[64];
    int count = carbon_construction_get_all(game->construction, handles, 64);

    for (int i = 0; i < count; i++) {
        uint32_t ghost = handles[i];
        int workers = count_workers_at_site(game, ghost);
        float speed = 1.0f + (workers - 1) * 0.5f;  // +50% per extra worker
        carbon_construction_set_speed(game->construction, ghost, speed);
    }
}
```

## Dialog / Narrative System

Event-driven dialog queue with speaker attribution for narrative integration. Supports message queuing, event-triggered dialogs, and speaker types for contextual storytelling:

```c
#include "carbon/dialog.h"

// Create dialog system (capacity = max queued messages)
Carbon_DialogSystem *dialog = carbon_dialog_create(32);

// Queue messages from built-in speaker types
carbon_dialog_queue_message(dialog, CARBON_SPEAKER_SYSTEM, "Welcome to the game!");
carbon_dialog_queue_message(dialog, CARBON_SPEAKER_PLAYER, "Hello, world!");
carbon_dialog_queue_message(dialog, CARBON_SPEAKER_AI, "Initializing systems...");

// Printf-style formatting
carbon_dialog_queue_printf(dialog, CARBON_SPEAKER_TUTORIAL,
                           "You have collected %d gold.", gold_count);

// Message with options (priority, duration)
carbon_dialog_queue_message_ex(dialog, CARBON_SPEAKER_SYSTEM, 0,
                                "Critical warning!",
                                CARBON_DIALOG_PRIORITY_CRITICAL,
                                10.0f);  // 10 second duration

// Insert urgent message at front of queue
carbon_dialog_insert_front(dialog, CARBON_SPEAKER_ENEMY, "Intruder alert!");

// In game loop - update and display:
carbon_dialog_update(dialog, delta_time);

if (carbon_dialog_has_message(dialog)) {
    const Carbon_DialogMessage *msg = carbon_dialog_current(dialog);

    // Get speaker info
    const char *speaker = carbon_dialog_get_speaker_name(dialog,
                                                          msg->speaker_type,
                                                          msg->speaker_id);
    uint32_t color = carbon_dialog_get_speaker_color(dialog,
                                                      msg->speaker_type,
                                                      msg->speaker_id);

    // Display message (typewriter effect support)
    int visible_chars = carbon_dialog_get_visible_chars(dialog);
    if (visible_chars >= 0) {
        // Show partial text for typewriter effect
        char display_text[512];
        strncpy(display_text, msg->text, visible_chars);
        display_text[visible_chars] = '\0';
        draw_dialog_box(speaker, display_text, color);
    } else {
        // Show complete text
        draw_dialog_box(speaker, msg->text, color);
    }
}

// Manual advance (on player input)
if (player_pressed_continue) {
    if (carbon_dialog_animation_complete(dialog)) {
        carbon_dialog_advance(dialog);  // Next message
    } else {
        carbon_dialog_skip_animation(dialog);  // Show all text
    }
}

// Cleanup
carbon_dialog_destroy(dialog);
```

**Custom speakers:**

```c
// Register custom speakers (NPCs, named characters)
uint32_t merchant = carbon_dialog_register_speaker(dialog, "Merchant Bob",
                                                    0xFF00FFFF,  // Yellow (ABGR)
                                                    0);           // Portrait ID

uint32_t villain = carbon_dialog_register_speaker(dialog, "Dark Lord",
                                                   0xFF0000FF,  // Red
                                                   1);

// Queue message from custom speaker
carbon_dialog_queue_message_custom(dialog, merchant, "Would you like to trade?");
carbon_dialog_queue_message_custom(dialog, villain, "You cannot escape!");

// Get custom speaker info
const Carbon_Speaker *speaker = carbon_dialog_get_speaker(dialog, merchant);
printf("Speaker: %s\n", speaker->name);

// Customize built-in speaker names/colors
carbon_dialog_set_speaker_name(dialog, CARBON_SPEAKER_PLAYER, "Hero");
carbon_dialog_set_speaker_color(dialog, CARBON_SPEAKER_PLAYER, 0xFF00FF00);  // Green
```

**Event-triggered dialogs:**

```c
// Register event dialogs (triggered once, queues message)
carbon_dialog_register_event(dialog, EVENT_FIRST_KILL, CARBON_SPEAKER_AI,
                              "Combat systems online. Target eliminated.");
carbon_dialog_register_event(dialog, EVENT_LOW_HEALTH, CARBON_SPEAKER_AI,
                              "Warning: Critical damage detected!");

// Register with full options
carbon_dialog_register_event_ex(dialog, EVENT_VICTORY, CARBON_SPEAKER_SYSTEM, 0,
                                 "Congratulations! You have won!",
                                 CARBON_DIALOG_PRIORITY_CRITICAL,
                                 0.0f,   // Use default duration
                                 false); // Not repeatable

// Repeatable events
carbon_dialog_register_event_ex(dialog, EVENT_ENEMY_SPOTTED, CARBON_SPEAKER_ALLY, 0,
                                 "Enemy unit detected!",
                                 CARBON_DIALOG_PRIORITY_HIGH,
                                 3.0f,   // 3 second duration
                                 true);  // Can trigger multiple times

// Trigger events based on game state
if (player_killed_first_enemy && !carbon_dialog_event_triggered(dialog, EVENT_FIRST_KILL)) {
    carbon_dialog_trigger_event(dialog, EVENT_FIRST_KILL);
}

if (player_health < 20) {
    carbon_dialog_trigger_event(dialog, EVENT_LOW_HEALTH);  // Only triggers once
}

// Reset events for new game
carbon_dialog_reset_events(dialog);

// Reset specific event
carbon_dialog_reset_event(dialog, EVENT_LOW_HEALTH);
```

**Configuration:**

```c
// Default message duration
carbon_dialog_set_default_duration(dialog, 5.0f);  // 5 seconds

// Typewriter effect speed (characters per second)
carbon_dialog_set_text_speed(dialog, 30.0f);  // 30 chars/sec
carbon_dialog_set_text_speed(dialog, 0.0f);   // Instant (default)

// Auto-advance after duration
carbon_dialog_set_auto_advance(dialog, true);   // Default
carbon_dialog_set_auto_advance(dialog, false);  // Wait for manual advance
```

**Callbacks:**

```c
// Called when a new message is displayed
void on_message_display(Carbon_DialogSystem *dialog,
                        const Carbon_DialogMessage *msg, void *userdata) {
    play_sound(SOUND_DIALOG_OPEN);
}
carbon_dialog_set_display_callback(dialog, on_message_display, NULL);

// Called when a message is dismissed
void on_message_dismiss(Carbon_DialogSystem *dialog,
                        const Carbon_DialogMessage *msg, void *userdata) {
    play_sound(SOUND_DIALOG_CLOSE);
}
carbon_dialog_set_dismiss_callback(dialog, on_message_dismiss, NULL);

// Called when an event is triggered
void on_event_triggered(Carbon_DialogSystem *dialog, int event_id, void *userdata) {
    log_event("Dialog event triggered: %d", event_id);
}
carbon_dialog_set_event_callback(dialog, on_event_triggered, NULL);
```

**Built-in speaker types:**
- `CARBON_SPEAKER_SYSTEM` - System/narrator messages (light gray)
- `CARBON_SPEAKER_PLAYER` - Player character (green)
- `CARBON_SPEAKER_AI` - AI/computer voice (cyan)
- `CARBON_SPEAKER_NPC` - Generic NPC (white)
- `CARBON_SPEAKER_ENEMY` - Enemy/antagonist (red)
- `CARBON_SPEAKER_ALLY` - Allied character (light green)
- `CARBON_SPEAKER_TUTORIAL` - Tutorial hints (yellow)
- `CARBON_SPEAKER_CUSTOM` (100+) - User-defined speakers

**Priority levels:**
- `CARBON_DIALOG_PRIORITY_LOW` - Background chatter
- `CARBON_DIALOG_PRIORITY_NORMAL` - Default messages
- `CARBON_DIALOG_PRIORITY_HIGH` - Important messages
- `CARBON_DIALOG_PRIORITY_CRITICAL` - Must-see messages

**Key features:**
- Message queue with configurable capacity
- Built-in and custom speaker support
- Event-triggered dialogs with one-shot or repeatable modes
- Typewriter text animation effect
- Auto-advance or manual advance modes
- Speaker color and portrait customization
- Display/dismiss/event callbacks
- Printf-style message formatting

## Game Speed System

Variable simulation speed with pause support. Allows games to run at different speeds (pause, normal, fast forward) while keeping UI responsive:

```c
#include "carbon/game_speed.h"

// Create game speed controller
Carbon_GameSpeed *speed = carbon_game_speed_create();

// Set speed multiplier (1.0 = normal, 2.0 = double, etc.)
carbon_game_speed_set(speed, 2.0f);  // 2x speed

// Pause/resume
carbon_game_speed_pause(speed);
carbon_game_speed_resume(speed);
carbon_game_speed_toggle_pause(speed);

// Check state
if (carbon_game_speed_is_paused(speed)) {
    // Game is paused
}

// In game loop - scale delta time
float raw_delta = carbon_get_delta_time(engine);
float scaled_delta = carbon_game_speed_scale_delta(speed, raw_delta);

// Game logic uses scaled delta (affected by speed)
update_game_logic(scaled_delta);

// UI uses raw delta (always normal speed)
update_ui(raw_delta);

// Speed presets with cycling
carbon_game_speed_set_default_presets(speed);  // 1x, 2x, 4x
carbon_game_speed_cycle(speed);                 // Cycle through presets
carbon_game_speed_cycle_reverse(speed);         // Cycle backward

// Custom presets
float presets[] = { 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
carbon_game_speed_set_presets(speed, presets, 5);

// Set speed to specific preset by index
carbon_game_speed_set_preset(speed, 2);  // Set to third preset (2.0f)

// Query preset state
int preset_idx = carbon_game_speed_get_preset_index(speed);  // -1 if not on preset
float preset_val = carbon_game_speed_get_preset(speed, 0);   // Get first preset value

// Speed limits
carbon_game_speed_set_min(speed, 0.25f);  // Minimum speed
carbon_game_speed_set_max(speed, 8.0f);   // Maximum speed

// Convenience functions
carbon_game_speed_multiply(speed, 2.0f);  // Double current speed
carbon_game_speed_divide(speed, 2.0f);    // Halve current speed
carbon_game_speed_reset(speed);           // Reset to 1.0x

// Query state
float current = carbon_game_speed_get(speed);       // 0 if paused
float base = carbon_game_speed_get_base(speed);     // Ignores pause
bool at_min = carbon_game_speed_is_at_min(speed);
bool at_max = carbon_game_speed_is_at_max(speed);
bool normal = carbon_game_speed_is_normal(speed);   // Is 1.0x

// Format for display
char buf[32];
carbon_game_speed_to_string(speed, buf, sizeof(buf));  // "Paused", "1x", "2x"
int percent = carbon_game_speed_get_percent(speed);    // 100, 200, etc.

// Cleanup
carbon_game_speed_destroy(speed);
```

**Smooth transitions (optional):**

```c
// Enable smooth speed changes
carbon_game_speed_set_smooth_transitions(speed, true);
carbon_game_speed_set_transition_rate(speed, 5.0f);  // How fast to interpolate

// In game loop, update transitions
carbon_game_speed_update(speed, raw_delta);

// Check transition state
if (carbon_game_speed_is_transitioning(speed)) {
    // Speed is changing smoothly
}

// Skip directly to target speed
carbon_game_speed_complete_transition(speed);
```

**Callbacks:**

```c
// Called when speed changes
void on_speed_change(Carbon_GameSpeed *speed, float old_speed,
                     float new_speed, void *userdata) {
    printf("Speed: %.1fx -> %.1fx\n", old_speed, new_speed);
}
carbon_game_speed_set_callback(speed, on_speed_change, NULL);

// Called when pause state changes
void on_pause_change(Carbon_GameSpeed *speed, bool paused, void *userdata) {
    if (paused) {
        show_pause_menu();
    } else {
        hide_pause_menu();
    }
}
carbon_game_speed_set_pause_callback(speed, on_pause_change, NULL);
```

**Statistics:**

```c
// Track time spent at various speeds
float game_time = carbon_game_speed_get_total_scaled_time(speed);   // Scaled time
float real_time = carbon_game_speed_get_total_real_time(speed);     // Real time
float paused_time = carbon_game_speed_get_total_paused_time(speed); // Time paused

// Reset statistics
carbon_game_speed_reset_stats(speed);
```

**Key features:**
- Speed multipliers from 0.1x to 16x (configurable)
- Pause/resume separate from speed setting
- Speed presets with cycling (keyboard shortcuts)
- Smooth transitions between speeds (optional)
- Callbacks for speed and pause changes
- Statistics tracking for game/real time
- Thread-safe delta time scaling

**Common patterns:**

```c
// Handle keyboard shortcuts
if (carbon_input_action_just_pressed(input, ACTION_PAUSE)) {
    carbon_game_speed_toggle_pause(speed);
}
if (carbon_input_action_just_pressed(input, ACTION_SPEED_UP)) {
    carbon_game_speed_cycle(speed);
}
if (carbon_input_action_just_pressed(input, ACTION_SPEED_DOWN)) {
    carbon_game_speed_cycle_reverse(speed);
}
if (carbon_input_action_just_pressed(input, ACTION_NORMAL_SPEED)) {
    carbon_game_speed_reset(speed);
}

// Show current speed in UI
char speed_text[32];
carbon_game_speed_to_string(game_speed, speed_text, sizeof(speed_text));
cui_label(ui, speed_text);
```

## Crafting System

Progress-based crafting with recipe definitions, batch support, speed multipliers, and completion callbacks:

```c
#include "carbon/crafting.h"

// Create recipe registry
Carbon_RecipeRegistry *recipes = carbon_recipe_create();

// Define a recipe
Carbon_RecipeDef recipe = {
    .id = "iron_sword",
    .name = "Iron Sword",
    .description = "A sturdy sword made of iron",
    .category = CATEGORY_WEAPONS,
    .craft_time = 5.0f,
    .input_count = 2,
    .output_count = 1,
    .unlocked = true,
};
recipe.inputs[0] = (Carbon_RecipeItem){ .item_type = ITEM_IRON, .quantity = 3 };
recipe.inputs[1] = (Carbon_RecipeItem){ .item_type = ITEM_WOOD, .quantity = 1 };
recipe.outputs[0] = (Carbon_RecipeItem){ .item_type = ITEM_IRON_SWORD, .quantity = 1 };
carbon_recipe_register(recipes, &recipe);

// Recipe with station requirement
Carbon_RecipeDef potion = {
    .id = "health_potion",
    .name = "Health Potion",
    .craft_time = 3.0f,
    .required_station = STATION_ALCHEMY,  // Requires alchemy table
    .input_count = 2,
    .output_count = 1,
    .unlocked = true,
};
potion.inputs[0] = (Carbon_RecipeItem){ .item_type = ITEM_HERB, .quantity = 2 };
potion.inputs[1] = (Carbon_RecipeItem){ .item_type = ITEM_WATER, .quantity = 1 };
potion.outputs[0] = (Carbon_RecipeItem){ .item_type = ITEM_HEALTH_POTION, .quantity = 1 };
carbon_recipe_register(recipes, &potion);

// Create a crafter (per-entity or per-building)
Carbon_Crafter *crafter = carbon_crafter_create(recipes);

// Set crafting station type (determines which recipes are available)
carbon_crafter_set_station(crafter, STATION_FORGE);

// Start crafting a batch
carbon_crafter_start(crafter, "iron_sword", 5);  // Craft 5 swords

// In game loop:
carbon_crafter_update(crafter, delta_time);

// Check status
if (carbon_crafter_is_complete(crafter)) {
    int collected = carbon_crafter_collect(crafter);  // Get items
    printf("Collected %d items\n", collected);
}

// Query progress
float progress = carbon_crafter_get_progress(crafter);           // Current item (0.0-1.0)
float batch_progress = carbon_crafter_get_batch_progress(crafter); // Overall batch

// Cleanup
carbon_crafter_destroy(crafter);
carbon_recipe_destroy(recipes);
```

**Queue management:**

```c
// Queue multiple recipes
carbon_crafter_start(crafter, "iron_sword", 3);
carbon_crafter_queue(crafter, "iron_helmet", 2);
carbon_crafter_queue(crafter, "iron_shield", 1);

// Query queue
int queue_length = carbon_crafter_get_queue_length(crafter);
const Carbon_CraftJob *job = carbon_crafter_get_queued_job(crafter, 1);

// Remove from queue (cannot remove current job)
carbon_crafter_remove_queued(crafter, 2);

// Clear queue (keeps current job)
carbon_crafter_clear_queue(crafter);

// Cancel current job
carbon_crafter_cancel(crafter);

// Cancel everything
carbon_crafter_cancel_all(crafter);

// Pause/resume
carbon_crafter_pause(crafter);
carbon_crafter_resume(crafter);
```

**Speed modifiers:**

```c
// Set crafting speed (affects all recipes)
carbon_crafter_set_speed(crafter, 2.0f);  // 2x speed (faster)
carbon_crafter_set_speed(crafter, 0.5f);  // 0.5x speed (slower)

// Query speed
float speed = carbon_crafter_get_speed(crafter);

// Time queries
float remaining = carbon_crafter_get_remaining_time(crafter);       // Current item
float total_remaining = carbon_crafter_get_total_remaining_time(crafter);  // All items
```

**Resource callbacks:**

```c
// Check if resources are available
bool check_resources(Carbon_Crafter *crafter,
                     const Carbon_RecipeDef *recipe, void *userdata) {
    Inventory *inv = userdata;
    for (int i = 0; i < recipe->input_count; i++) {
        if (!inventory_has(inv, recipe->inputs[i].item_type,
                            recipe->inputs[i].quantity)) {
            return false;
        }
    }
    return true;
}

// Consume resources when crafting starts
void consume_resources(Carbon_Crafter *crafter,
                       const Carbon_RecipeDef *recipe, void *userdata) {
    Inventory *inv = userdata;
    for (int i = 0; i < recipe->input_count; i++) {
        inventory_remove(inv, recipe->inputs[i].item_type,
                          recipe->inputs[i].quantity);
    }
}

// Produce items when crafting completes
void produce_items(Carbon_Crafter *crafter,
                   const Carbon_RecipeDef *recipe,
                   int quantity, void *userdata) {
    Inventory *inv = userdata;
    for (int i = 0; i < recipe->output_count; i++) {
        inventory_add(inv, recipe->outputs[i].item_type,
                       recipe->outputs[i].quantity * quantity);
    }
}

carbon_crafter_set_resource_check(crafter, check_resources, inventory);
carbon_crafter_set_resource_consume(crafter, consume_resources, inventory);
carbon_crafter_set_resource_produce(crafter, produce_items, inventory);
```

**Completion callback:**

```c
// Called each time an item is crafted
void on_item_crafted(Carbon_Crafter *crafter,
                     const Carbon_RecipeDef *recipe,
                     int quantity, void *userdata) {
    printf("Crafted %d x %s\n", quantity, recipe->name);
    play_sound(SOUND_CRAFT_COMPLETE);
}
carbon_crafter_set_callback(crafter, on_item_crafted, NULL);
```

**Recipe queries:**

```c
// Find recipe by ID
const Carbon_RecipeDef *recipe = carbon_recipe_find(recipes, "iron_sword");

// Get recipes by category
const Carbon_RecipeDef *weapon_recipes[32];
int count = carbon_recipe_get_by_category(recipes, CATEGORY_WEAPONS, weapon_recipes, 32);

// Get recipes available at a station
const Carbon_RecipeDef *forge_recipes[32];
int forge_count = carbon_recipe_get_by_station(recipes, STATION_FORGE, forge_recipes, 32);

// Check if crafter can craft recipe (station + unlock)
if (carbon_crafter_can_craft(crafter, "iron_sword")) {
    // Recipe is available
}

// Get all available recipes for this crafter
const Carbon_RecipeDef *available[64];
int available_count = carbon_crafter_get_available_recipes(crafter, available, 64);

// Unlock/lock recipes
carbon_recipe_set_unlocked(recipes, "iron_sword", true);
if (carbon_recipe_is_unlocked(recipes, "iron_sword")) {
    // Recipe is unlocked
}
```

**Entity association:**

```c
// Associate crafter with an entity
carbon_crafter_set_entity(crafter, worker_entity);
int32_t entity = carbon_crafter_get_entity(crafter);
```

**Statistics:**

```c
int total = carbon_crafter_get_total_crafted(crafter);
float time = carbon_crafter_get_total_craft_time(crafter);
carbon_crafter_reset_stats(crafter);
```

**Key features:**
- Recipe registry with input/output items
- Station requirements for specialized crafting
- Batch crafting with queue support
- Speed multipliers for upgrades/bonuses
- Resource callbacks for inventory integration
- Completion callbacks for notifications
- Progress tracking for UI display
- Category and station filtering

**Craft statuses:**
- `CARBON_CRAFT_IDLE` - Not crafting
- `CARBON_CRAFT_IN_PROGRESS` - Actively crafting
- `CARBON_CRAFT_COMPLETE` - Batch complete, awaiting collection
- `CARBON_CRAFT_PAUSED` - Crafting paused
- `CARBON_CRAFT_FAILED` - Failed (missing resources)

## Biome System

Terrain types affecting resource distribution, movement costs, and visuals:

```c
#include "carbon/biome.h"

// Create biome system
Carbon_BiomeSystem *biomes = carbon_biome_create();

// Register biomes
Carbon_BiomeDef plains = carbon_biome_default_def();
strcpy(plains.id, "plains");
strcpy(plains.name, "Plains");
plains.color = carbon_biome_rgb(124, 252, 0);  // Lawn green
plains.movement_cost = 1.0f;                    // Normal speed
plains.flags = CARBON_BIOME_FLAG_PASSABLE | CARBON_BIOME_FLAG_BUILDABLE | CARBON_BIOME_FLAG_FARMABLE;
int plains_id = carbon_biome_register(biomes, &plains);

Carbon_BiomeDef forest = carbon_biome_default_def();
strcpy(forest.id, "forest");
strcpy(forest.name, "Dense Forest");
forest.color = carbon_biome_rgb(34, 139, 34);  // Forest green
forest.movement_cost = 1.5f;                    // 50% slower movement
forest.visibility_modifier = 0.7f;              // Reduced visibility
forest.defense_bonus = 0.2f;                    // +20% defense bonus
forest.flags = CARBON_BIOME_FLAG_PASSABLE | CARBON_BIOME_FLAG_BUILDABLE;
int forest_id = carbon_biome_register(biomes, &forest);

Carbon_BiomeDef mountain = carbon_biome_default_def();
strcpy(mountain.id, "mountain");
strcpy(mountain.name, "Mountain");
mountain.color = carbon_biome_rgb(128, 128, 128);  // Gray
mountain.movement_cost = 2.5f;                      // Very slow
mountain.defense_bonus = 0.4f;                      // +40% defense
mountain.flags = CARBON_BIOME_FLAG_PASSABLE;        // Not buildable
int mountain_id = carbon_biome_register(biomes, &mountain);

Carbon_BiomeDef water = carbon_biome_default_def();
strcpy(water.id, "water");
strcpy(water.name, "Deep Water");
water.color = carbon_biome_rgb(30, 144, 255);  // Dodger blue
water.flags = CARBON_BIOME_FLAG_WATER;          // Only naval units can pass
int water_id = carbon_biome_register(biomes, &water);

// Set resource spawn weights
carbon_biome_set_resource_weight(biomes, plains_id, RESOURCE_FOOD, 1.5f);
carbon_biome_set_resource_weight(biomes, forest_id, RESOURCE_WOOD, 2.0f);
carbon_biome_set_resource_weight(biomes, forest_id, RESOURCE_FOOD, 0.5f);  // Less food
carbon_biome_set_resource_weight(biomes, mountain_id, RESOURCE_IRON, 2.5f);
carbon_biome_set_resource_weight(biomes, mountain_id, RESOURCE_GOLD, 1.5f);

// Or use string ID
carbon_biome_set_resource_weight_by_id(biomes, "water", RESOURCE_FISH, 3.0f);

// Cleanup
carbon_biome_destroy(biomes);
```

**Biome queries:**

```c
// Find biome by ID
const Carbon_BiomeDef *def = carbon_biome_find(biomes, "forest");
int forest_index = carbon_biome_find_index(biomes, "forest");

// Get biome by index
const Carbon_BiomeDef *biome = carbon_biome_get(biomes, 0);

// Query properties
const char *name = carbon_biome_get_name(biomes, forest_id);
uint32_t color = carbon_biome_get_color(biomes, forest_id);
float move_cost = carbon_biome_get_movement_cost(biomes, forest_id);
float resource_mult = carbon_biome_get_resource_multiplier(biomes, forest_id);
float visibility = carbon_biome_get_visibility_modifier(biomes, forest_id);
float defense = carbon_biome_get_defense_bonus(biomes, forest_id);

// Get resource weight for a biome
float wood_weight = carbon_biome_get_resource_weight(biomes, forest_id, RESOURCE_WOOD);

// Find best biome for a resource
int best_for_iron = carbon_biome_get_best_for_resource(biomes, RESOURCE_IRON);

// Get all biomes that can spawn a resource
int biome_ids[16];
int count = carbon_biome_get_all_for_resource(biomes, RESOURCE_WOOD, biome_ids, 16);
```

**Flag queries:**

```c
// Check specific flags
if (carbon_biome_has_flag(biomes, forest_id, CARBON_BIOME_FLAG_PASSABLE)) {
    // Ground units can pass
}

// Convenience functions
if (carbon_biome_is_passable(biomes, forest_id)) { }
if (carbon_biome_is_buildable(biomes, forest_id)) { }
if (carbon_biome_is_water(biomes, water_id)) { }
if (carbon_biome_is_hazardous(biomes, lava_id)) { }
```

**Biome map (per-cell biome storage):**

```c
// Create biome map (matches tilemap dimensions)
Carbon_BiomeMap *map = carbon_biome_map_create(biomes, 100, 100);

// Set biomes manually
carbon_biome_map_set(map, 10, 10, plains_id);
carbon_biome_map_fill_rect(map, 0, 0, 50, 50, plains_id);
carbon_biome_map_fill_circle(map, 75, 75, 15, forest_id);

// Query biomes
int biome_at = carbon_biome_map_get(map, 10, 10);
const Carbon_BiomeDef *def_at = carbon_biome_map_get_def(map, 10, 10);

// Query properties at position
float move_cost = carbon_biome_map_get_movement_cost(map, 10, 10);
float resource_wt = carbon_biome_map_get_resource_weight(map, 10, 10, RESOURCE_WOOD);

// Check flags at position
if (carbon_biome_map_is_passable(map, x, y)) {
    // Can move here
}
if (carbon_biome_map_is_buildable(map, x, y)) {
    // Can build here
}

// Map info
int width, height;
carbon_biome_map_get_size(map, &width, &height);

// Statistics
int plains_count = carbon_biome_map_count_biome(map, plains_id);
int stats[CARBON_BIOME_MAX];
carbon_biome_map_get_stats(map, stats);
printf("Plains: %d cells, Forest: %d cells\n", stats[plains_id], stats[forest_id]);

// Cleanup
carbon_biome_map_destroy(map);
```

**Procedural generation:**

```c
// Noise-based generation (uses multi-octave perlin noise)
int biome_ids[] = { water_id, plains_id, forest_id, mountain_id };
float thresholds[] = { 0.0f, 0.3f, 0.5f, 0.8f };  // Noise value ranges
carbon_biome_map_generate_noise(map, biome_ids, thresholds, 4, 12345);

// Smooth borders (reduces noise, larger biome regions)
carbon_biome_map_smooth(map, 2);  // 2 passes
```

**Biome flags:**
- `CARBON_BIOME_FLAG_PASSABLE` - Ground units can traverse
- `CARBON_BIOME_FLAG_BUILDABLE` - Can construct buildings
- `CARBON_BIOME_FLAG_FARMABLE` - Can grow crops
- `CARBON_BIOME_FLAG_WATER` - Naval units only
- `CARBON_BIOME_FLAG_HAZARDOUS` - Causes damage over time

**Biome definition fields:**
- `id` - Unique string identifier
- `name` - Display name
- `description` - Description text
- `color` - Primary color (ABGR format)
- `color_variant` - Secondary color for variation
- `base_tile` - Tilemap tile ID
- `movement_cost` - Movement speed multiplier (1.0 = normal)
- `resource_multiplier` - Resource yield modifier
- `visibility_modifier` - Vision range modifier
- `defense_bonus` - Defense bonus for units
- `base_temperature` - Temperature (-1.0 cold to 1.0 hot)
- `humidity` - Humidity (0.0 dry to 1.0 wet)
- `transition_priority` - Edge blending priority
- `flags` - Combination of biome flags

**Key features:**
- Biome registry with lookup by ID
- Per-biome resource spawn weights
- Movement cost modifiers
- Combat defense bonuses
- Visibility modifiers
- Biome map for per-cell tracking
- Procedural noise-based generation
- Border smoothing algorithm
- Flag-based terrain properties

**Common patterns:**

```c
// Integrate with pathfinding
float pathfinder_cost(int x, int y, void *userdata) {
    Carbon_BiomeMap *map = userdata;
    return carbon_biome_map_get_movement_cost(map, x, y);
}
carbon_pathfinder_set_cost_callback(pf, pathfinder_cost, biome_map);

// Resource spawning based on biome
void spawn_resources(Carbon_BiomeMap *map, int resource_type, int count) {
    for (int i = 0; i < count; i++) {
        int x = rand() % map_width;
        int y = rand() % map_height;
        float weight = carbon_biome_map_get_resource_weight(map, x, y, resource_type);
        if ((float)rand() / RAND_MAX < weight) {
            place_resource(x, y, resource_type);
        }
    }
}

// Render biome colors to minimap
void render_minimap(Carbon_BiomeMap *map) {
    int width, height;
    carbon_biome_map_get_size(map, &width, &height);
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            const Carbon_BiomeDef *def = carbon_biome_map_get_def(map, x, y);
            if (def) {
                draw_pixel(x, y, def->color);
            }
        }
    }
}
```

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

## Task Queue System

Sequential task execution for autonomous AI agents. Provides a queue of tasks with lifecycle management and completion callbacks:

```c
#include "carbon/task.h"

// Create task queue for an agent (capacity = max pending tasks)
Carbon_TaskQueue *queue = carbon_task_queue_create(16);

// Assign entity to execute tasks
carbon_task_queue_set_assigned_entity(queue, worker_entity);

// Queue tasks (builds a sequence of actions)
carbon_task_queue_add_move(queue, target_x, target_y);
carbon_task_queue_add_collect(queue, resource_x, resource_y, RESOURCE_WOOD);
carbon_task_queue_add_deposit(queue, storage_x, storage_y, -1);  // -1 = all types
carbon_task_queue_add_wait(queue, 1.0f);  // Wait 1 second

// Movement with run option
carbon_task_queue_add_move_ex(queue, x, y, true);  // Run to destination

// Resource operations
carbon_task_queue_add_collect_ex(queue, x, y, RESOURCE_IRON, 10);  // Specific quantity
carbon_task_queue_add_withdraw(queue, storage_x, storage_y, RESOURCE_GOLD, 50);
carbon_task_queue_add_mine(queue, node_x, node_y, 0);  // 0 = until full

// Crafting and building
carbon_task_queue_add_craft(queue, RECIPE_SWORD, 1);
carbon_task_queue_add_build(queue, x, y, BUILDING_BARRACKS);
carbon_task_queue_add_build_ex(queue, x, y, BUILDING_WALL, DIRECTION_NORTH);

// Combat tasks
carbon_task_queue_add_attack(queue, enemy_entity, true);   // true = pursue
carbon_task_queue_add_defend(queue, center_x, center_y, 5);  // 5 tile radius
carbon_task_queue_add_follow(queue, leader_entity, 2, 10);   // min/max distance

// Exploration
carbon_task_queue_add_explore(queue, area_x, area_y, 10);  // 10 tile radius

// Patrol with waypoints
int waypoints[][2] = {{10, 10}, {20, 10}, {20, 20}, {10, 20}};
carbon_task_queue_add_patrol(queue, waypoints, 4, true);  // true = loop

// Interaction
carbon_task_queue_add_interact(queue, door_x, door_y, INTERACT_OPEN);
carbon_task_queue_add_interact_entity(queue, npc_entity, INTERACT_TALK);

// In game loop - process current task:
Carbon_Task *current = carbon_task_queue_current(queue);
if (current) {
    // Start task if pending
    if (current->status == CARBON_TASK_PENDING) {
        carbon_task_queue_start(queue);
    }

    // Process based on task type
    if (current->status == CARBON_TASK_IN_PROGRESS) {
        switch (current->type) {
            case CARBON_TASK_MOVE: {
                Carbon_TaskMove *move = &current->data.move;
                if (move_agent_toward(agent, move->target_x, move->target_y)) {
                    carbon_task_queue_complete(queue);
                }
                break;
            }
            case CARBON_TASK_WAIT:
                // Built-in wait handling - auto-completes when done
                carbon_task_queue_update_wait(queue, delta_time);
                break;
            case CARBON_TASK_COLLECT: {
                Carbon_TaskCollect *collect = &current->data.collect;
                if (!resource_exists(collect->target_x, collect->target_y)) {
                    carbon_task_queue_fail(queue, "Resource depleted");
                } else if (agent_at_position(agent, collect->target_x, collect->target_y)) {
                    collect_resource(agent, collect->resource_type, collect->quantity);
                    carbon_task_queue_complete(queue);
                }
                break;
            }
            // ... handle other task types
        }
    }
}

// Update progress for UI (0.0 to 1.0)
carbon_task_queue_set_progress(queue, 0.5f);

// Task completion callback
void on_task_done(Carbon_TaskQueue *queue, const Carbon_Task *task, void *userdata) {
    Agent *agent = userdata;
    if (task->status == CARBON_TASK_COMPLETED) {
        printf("Agent completed %s\n", carbon_task_type_name(task->type));
    } else if (task->status == CARBON_TASK_FAILED) {
        printf("Task failed: %s\n", task->fail_reason);
    }
}
carbon_task_queue_set_callback(queue, on_task_done, agent);

// Insert urgent task at front (after current)
Carbon_TaskMove urgent_move = { .target_x = safe_x, .target_y = safe_y, .run = true };
carbon_task_queue_insert_front(queue, CARBON_TASK_MOVE, &urgent_move, sizeof(urgent_move));

// Cancel current and clear queue
carbon_task_queue_cancel(queue);  // Cancel current task
carbon_task_queue_clear(queue);   // Clear all tasks

// Query queue state
int count = carbon_task_queue_count(queue);
bool empty = carbon_task_queue_is_empty(queue);
bool full = carbon_task_queue_is_full(queue);
bool idle = carbon_task_queue_is_idle(queue);  // No task or current complete
int capacity = carbon_task_queue_capacity(queue);

// Get task by index
Carbon_Task *second = carbon_task_queue_get(queue, 1);

// Remove specific task
carbon_task_queue_remove(queue, 2);  // Remove task at index 2

// Custom task types
typedef struct MyCustomTask {
    int custom_data;
    float custom_value;
} MyCustomTask;

MyCustomTask custom = { .custom_data = 42, .custom_value = 1.5f };
carbon_task_queue_add_custom(queue, CARBON_TASK_USER + 1, &custom, sizeof(custom));

// Utility functions
const char *type_name = carbon_task_type_name(CARBON_TASK_MOVE);     // "Move"
const char *status_name = carbon_task_status_name(CARBON_TASK_IN_PROGRESS);  // "In Progress"

// Cleanup
carbon_task_queue_destroy(queue);
```

**Built-in task types:**
- `CARBON_TASK_MOVE` - Move to target position
- `CARBON_TASK_EXPLORE` - Explore area around position
- `CARBON_TASK_COLLECT` - Collect resource at position
- `CARBON_TASK_DEPOSIT` - Deposit carried items
- `CARBON_TASK_CRAFT` - Craft item using recipe
- `CARBON_TASK_BUILD` - Construct building
- `CARBON_TASK_ATTACK` - Attack target entity
- `CARBON_TASK_DEFEND` - Defend position
- `CARBON_TASK_FOLLOW` - Follow target entity
- `CARBON_TASK_FLEE` - Flee from danger
- `CARBON_TASK_WAIT` - Wait for duration
- `CARBON_TASK_INTERACT` - Interact with entity/object
- `CARBON_TASK_PATROL` - Patrol between waypoints
- `CARBON_TASK_WITHDRAW` - Withdraw resources from storage
- `CARBON_TASK_MINE` - Mine resource node
- `CARBON_TASK_USER` (100+) - Game-defined task types

**Task statuses:**
- `CARBON_TASK_PENDING` - Not yet started
- `CARBON_TASK_IN_PROGRESS` - Currently executing
- `CARBON_TASK_COMPLETED` - Successfully completed
- `CARBON_TASK_FAILED` - Failed (check `fail_reason`)
- `CARBON_TASK_CANCELLED` - Cancelled before completion

**Key features:**
- FIFO task queue with configurable capacity
- Type-specific task parameters (move coordinates, craft recipes, etc.)
- Task lifecycle: pending → in_progress → completed/failed/cancelled
- Completion callbacks for state machine integration
- Built-in wait task with automatic completion
- Priority front insertion for urgent tasks
- Entity assignment for agent tracking
- Custom task types for game-specific actions

**Common patterns:**

```c
// Worker gathering loop
void setup_gather_loop(Carbon_TaskQueue *queue, int resource_x, int resource_y,
                        int storage_x, int storage_y) {
    carbon_task_queue_clear(queue);
    carbon_task_queue_add_move(queue, resource_x, resource_y);
    carbon_task_queue_add_collect(queue, resource_x, resource_y, RESOURCE_WOOD);
    carbon_task_queue_add_move(queue, storage_x, storage_y);
    carbon_task_queue_add_deposit(queue, storage_x, storage_y, -1);
    // Re-queue on completion callback to create loop
}

// Guard patrol
void setup_guard_patrol(Carbon_TaskQueue *queue) {
    int waypoints[][2] = {{10, 10}, {30, 10}, {30, 30}, {10, 30}};
    carbon_task_queue_add_patrol(queue, waypoints, 4, true);
}

// Interrupt current task for emergency
void handle_emergency(Carbon_TaskQueue *queue, int safe_x, int safe_y) {
    Carbon_TaskMove flee = { .target_x = safe_x, .target_y = safe_y, .run = true };
    carbon_task_queue_insert_front(queue, CARBON_TASK_MOVE, &flee, sizeof(flee));
    carbon_task_queue_cancel(queue);  // Cancel current, starts flee immediately
}
```

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

## UI Theme System

Semantic color system with predefined themes and easy customization:

```c
#include "carbon/ui.h"

// Get predefined theme presets
CUI_Theme dark_theme = cui_theme_dark();   // Default dark theme
CUI_Theme light_theme = cui_theme_light(); // Light theme variant

// Apply theme to UI context
cui_set_theme(ui, &dark_theme);

// Get current theme for reading
const CUI_Theme *current = cui_get_theme(ui);

// Customize accent color (updates related colors automatically)
CUI_Theme custom = cui_theme_dark();
cui_theme_set_accent(&custom, cui_rgb(100, 150, 255));  // Blue accent
cui_set_theme(ui, &custom);

// Set semantic colors (success, warning, danger, info)
cui_theme_set_semantic_colors(&custom,
    cui_rgb(80, 200, 120),   // success - green
    cui_rgb(255, 180, 50),   // warning - orange
    cui_rgb(240, 80, 80),    // danger - red
    cui_rgb(80, 150, 240));  // info - blue

// Use semantic button variants
if (cui_button_primary(ui, "Save")) {
    // Accent-colored button
}
if (cui_button_success(ui, "Confirm")) {
    // Green success button
}
if (cui_button_warning(ui, "Caution")) {
    // Orange warning button
}
if (cui_button_danger(ui, "Delete")) {
    // Red danger button for destructive actions
}
if (cui_button_info(ui, "Help")) {
    // Blue info button
}

// Colored progress bars
cui_progress_bar(ui, health, 0, 100);  // Uses theme.progress_fill
cui_progress_bar_colored(ui, health, 0, 100, current->success);  // Green health bar
cui_progress_bar_colored(ui, mana, 0, 100, current->info);       // Blue mana bar

// Color helper functions
uint32_t lighter = cui_color_brighten(color, 0.2f);  // 20% brighter
uint32_t darker = cui_color_darken(color, 0.2f);     // 20% darker
uint32_t faded = cui_color_alpha(color, 0.5f);       // 50% opacity
uint32_t blended = cui_color_lerp(color1, color2, 0.5f);  // 50% blend

// Direct theme color access for custom widgets
cui_draw_rect(ctx, x, y, w, h, current->success);  // Green rect
cui_label_colored(ui, "Error!", current->danger);   // Red text
```

**Theme structure colors:**
```c
typedef struct CUI_Theme {
    /* Background colors */
    uint32_t bg_panel;              // Panel/window background
    uint32_t bg_widget;             // Widget background (normal)
    uint32_t bg_widget_hover;       // Widget background (hovered)
    uint32_t bg_widget_active;      // Widget background (pressed/active)
    uint32_t bg_widget_disabled;    // Widget background (disabled)

    /* Border */
    uint32_t border;                // Border color

    /* Text colors */
    uint32_t text;                  // Primary text
    uint32_t text_dim;              // Secondary/dimmed text
    uint32_t text_highlight;        // Highlighted text (white in dark, black in light)
    uint32_t text_disabled;         // Disabled text

    /* Accent color */
    uint32_t accent;                // Primary interactive color
    uint32_t accent_hover;          // Accent when hovered
    uint32_t accent_active;         // Accent when pressed

    /* Semantic colors */
    uint32_t success, success_hover;  // Green - positive/confirm
    uint32_t warning, warning_hover;  // Orange - caution/attention
    uint32_t danger, danger_hover;    // Red - destructive/error
    uint32_t info, info_hover;        // Blue - informational

    /* Widget-specific */
    uint32_t checkbox_check;        // Checkmark color
    uint32_t slider_track;          // Slider track background
    uint32_t slider_grab;           // Slider handle
    uint32_t scrollbar;             // Scrollbar track
    uint32_t scrollbar_grab;        // Scrollbar thumb
    uint32_t progress_fill;         // Progress bar fill
    uint32_t selection;             // Text selection background

    /* Metrics */
    float corner_radius;            // Rounded corner radius (default: 4)
    float border_width;             // Border thickness (default: 1)
    float widget_height;            // Standard widget height (default: 28)
    float spacing;                  // Inter-widget spacing (default: 4)
    float padding;                  // Inner padding (default: 8)
    float scrollbar_width;          // Scrollbar width (default: 12)
} CUI_Theme;
```

**Semantic button variants:**
- `cui_button_primary()` - Accent-colored for primary actions
- `cui_button_success()` - Green for confirmations, positive actions
- `cui_button_warning()` - Orange for cautionary actions
- `cui_button_danger()` - Red for destructive actions (delete, cancel)
- `cui_button_info()` - Blue for informational actions (help, details)

**Key features:**
- Predefined dark and light themes
- Semantic colors for consistent UI meaning
- Easy accent color customization
- Color helper functions (brighten, darken, alpha, lerp)
- Semantic button variants for common actions
- All colors in packed ABGR format (0xAABBGGRR)

## 3D Camera System

Orbital 3D camera with spherical coordinate control and smooth animations:

```c
#include "carbon/camera3d.h"

// Create camera
Carbon_Camera3D *cam = carbon_camera3d_create();

// Set perspective projection (default)
carbon_camera3d_set_perspective(cam, 60.0f, 16.0f/9.0f, 0.1f, 1000.0f);

// Or orthographic for strategy/isometric views
carbon_camera3d_set_orthographic(cam, 20.0f, 15.0f, 0.1f, 1000.0f);

// Position using spherical coordinates (orbit around target)
carbon_camera3d_set_target(cam, 0, 0, 0);           // Look at origin
carbon_camera3d_set_spherical(cam, 45.0f,           // Yaw: 45 degrees
                                   30.0f,           // Pitch: 30 degrees up
                                   15.0f);          // Distance: 15 units

// Or set position directly
carbon_camera3d_set_position(cam, 10.0f, 5.0f, 10.0f);

// Orbital controls (for mouse/gamepad input)
carbon_camera3d_orbit(cam, delta_yaw, delta_pitch);   // Rotate around target
carbon_camera3d_zoom(cam, delta_distance);            // Zoom in/out
carbon_camera3d_pan(cam, right_amount, up_amount);    // Pan in camera space
carbon_camera3d_pan_xz(cam, dx, dz);                  // Pan in world XZ plane

// Set constraints
carbon_camera3d_set_distance_limits(cam, 5.0f, 100.0f);   // Min/max zoom
carbon_camera3d_set_pitch_limits(cam, -80.0f, 80.0f);     // Prevent gimbal lock

// Smooth animations
carbon_camera3d_animate_spherical_to(cam, 90.0f, 45.0f, 20.0f, 1.5f);  // Over 1.5 seconds
carbon_camera3d_animate_target_to(cam, 10.0f, 0.0f, 10.0f, 1.0f);       // Move target
carbon_camera3d_animate_to(cam, 20.0f, 10.0f, 20.0f, 2.0f);             // Move position

// Check animation state
if (carbon_camera3d_is_animating(cam)) {
    // Camera is transitioning
}
carbon_camera3d_stop_animation(cam);  // Cancel animation

// In game loop:
carbon_camera3d_update(cam, delta_time);  // Update animations and matrices

// Get matrices for rendering
const float *view = carbon_camera3d_get_view_matrix(cam);
const float *proj = carbon_camera3d_get_projection_matrix(cam);
const float *vp = carbon_camera3d_get_vp_matrix(cam);  // Combined

// Get camera direction vectors
float fx, fy, fz;
carbon_camera3d_get_forward(cam, &fx, &fy, &fz);
carbon_camera3d_get_right(cam, &rx, &ry, &rz);
carbon_camera3d_get_up(cam, &ux, &uy, &uz);

// 3D picking (mouse to world ray)
float ray_ox, ray_oy, ray_oz, ray_dx, ray_dy, ray_dz;
carbon_camera3d_screen_to_ray(cam, mouse_x, mouse_y, screen_w, screen_h,
                               &ray_ox, &ray_oy, &ray_oz,
                               &ray_dx, &ray_dy, &ray_dz);

// Project world point to screen
float sx, sy;
if (carbon_camera3d_world_to_screen(cam, world_x, world_y, world_z,
                                     screen_w, screen_h, &sx, &sy)) {
    // Point is visible, draw UI at (sx, sy)
}

// Window resize
carbon_camera3d_set_aspect(cam, new_width / new_height);

// Cleanup
carbon_camera3d_destroy(cam);
```

**Key features:**
- Spherical coordinate orbit (yaw, pitch, distance around target)
- Perspective and orthographic projection modes
- Distance and pitch constraints to prevent issues
- Smooth animated transitions with easing
- Screen-to-ray conversion for 3D picking
- World-to-screen projection for UI overlays
- Direction vectors (forward, right, up) for movement

**Default values:**
- Projection: Perspective, 60° FOV, 16:9 aspect
- Distance limits: 1.0 to 1000.0
- Pitch limits: -89° to 89° (prevents gimbal lock)
- Easing: Smooth (ease-in-out)

## Logging System

File-based logging with subsystem tags and log levels:

```c
#include "carbon/log.h"

// Initialize (uses default path: /tmp/carbon.log on Unix, carbon.log on Windows)
carbon_log_init();
// Or: carbon_log_init_with_path("game.log");

// Set log level (DEBUG includes all, ERROR only errors)
carbon_log_set_level(CARBON_LOG_LEVEL_DEBUG);

// Log messages with subsystem tags
carbon_log_info(CARBON_LOG_CORE, "Engine initialized");
carbon_log_warning(CARBON_LOG_GRAPHICS, "Texture not found: %s", path);
carbon_log_error(CARBON_LOG_AUDIO, "Failed to load sound");
carbon_log_debug(CARBON_LOG_AI, "Processing %d entities", count);

// Errors auto-flush for crash debugging
carbon_log_flush();  // Manual flush

// Shutdown
carbon_log_shutdown();
```

**Predefined subsystems:** `CARBON_LOG_CORE`, `CARBON_LOG_ECS`, `CARBON_LOG_GRAPHICS`, `CARBON_LOG_AUDIO`, `CARBON_LOG_INPUT`, `CARBON_LOG_AI`, `CARBON_LOG_UI`, `CARBON_LOG_GAME`, `CARBON_LOG_NET`, `CARBON_LOG_SAVE`

**Output format:** `[2024-01-15 14:30:22] [ERROR  ] [Graphics  ] Failed to load texture`

## Safe Arithmetic Library

Overflow-protected integer arithmetic:

```c
#include "carbon/math_safe.h"

// Check before operation
if (carbon_would_multiply_overflow(price, quantity)) {
    // Handle overflow case
}

// Safe operations (clamp on overflow, log warning)
int32_t total = carbon_safe_multiply(price, quantity);
int32_t balance = carbon_safe_add(balance, income);
int32_t result = carbon_safe_subtract(funds, cost);
int32_t ratio = carbon_safe_divide(a, b);  // Handles divide by zero

// 64-bit variants
int64_t big = carbon_safe_multiply_i64(a, b);

// Unsigned variants
uint32_t sum = carbon_safe_add_u32(a, b);
uint32_t diff = carbon_safe_subtract_u32(a, b);  // Clamps to 0

// Disable overflow warnings if needed
carbon_safe_math_set_warnings(false);
```

## Notification/Toast System

Timed notification messages for player feedback:

```c
#include "carbon/notification.h"

// Create manager
Carbon_NotificationManager *notify = carbon_notify_create();

// Add notifications
carbon_notify_add(notify, "Game saved!", CARBON_NOTIFY_SUCCESS);
carbon_notify_add(notify, "Low health!", CARBON_NOTIFY_WARNING);
carbon_notify_add(notify, "Connection lost", CARBON_NOTIFY_ERROR);
carbon_notify_printf(notify, CARBON_NOTIFY_INFO, "Score: %d", score);

// Custom color
carbon_notify_add_colored(notify, "Special message", 1.0f, 0.5f, 0.0f);

// Custom duration
carbon_notify_add_timed(notify, "Brief", CARBON_NOTIFY_INFO, 2.0f);

// In game loop:
carbon_notify_update(notify, delta_time);

// Render (during text batch)
carbon_text_begin(text);
carbon_notify_render(notify, text, font, 10.0f, 10.0f, 24.0f);  // x, y, spacing
carbon_text_end(text);

// Query
int count = carbon_notify_count(notify);
const Carbon_Notification *n = carbon_notify_get(notify, 0);

// Clear all
carbon_notify_clear(notify);

// Cleanup
carbon_notify_destroy(notify);
```

**Notification types:** `CARBON_NOTIFY_INFO` (white), `CARBON_NOTIFY_SUCCESS` (green), `CARBON_NOTIFY_WARNING` (yellow), `CARBON_NOTIFY_ERROR` (red)

## Line Cell Iterator

Bresenham line iteration for grid-based operations:

```c
#include "carbon/line.h"

// Callback for each cell along line
bool check_walkable(int32_t x, int32_t y, void *userdata) {
    Tilemap *map = userdata;
    return tilemap_is_walkable(map, x, y);  // false stops iteration
}

// Iterate over all cells (returns true if callback never stopped)
bool clear = carbon_iterate_line_cells(x1, y1, x2, y2, check_walkable, map);

// Skip endpoints (useful for city-to-city connections)
carbon_iterate_line_cells_ex(x1, y1, x2, y2, callback, data, true, true);

// Count cells
int total = carbon_count_line_cells(0, 0, 10, 5);       // Including endpoints
int between = carbon_count_line_cells_between(0, 0, 10, 5);  // Excluding endpoints

// Get cells into buffer
int32_t xs[100], ys[100];
int count = carbon_get_line_cells(x1, y1, x2, y2, xs, ys, 100);

// Check if line passes through a specific cell
if (carbon_line_passes_through(x1, y1, x2, y2, cell_x, cell_y)) {
    // Line intersects this cell
}
```

## Condition/Degradation System

Track object condition with decay and repair:

```c
#include "carbon/condition.h"

// Initialize
Carbon_Condition cond;
carbon_condition_init(&cond, CARBON_QUALITY_STANDARD);  // or LOW, HIGH

// Apply decay
carbon_condition_decay_time(&cond, 0.1f);    // Time-based (per tick)
carbon_condition_decay_usage(&cond, 1.0f);   // Usage-based (when used)

// Check status
Carbon_ConditionStatus status = carbon_condition_get_status(&cond);
float percent = carbon_condition_get_percent(&cond);
bool usable = carbon_condition_is_usable(&cond);

// Repair
int32_t cost = carbon_condition_get_repair_cost(&cond, 100);  // Base cost 100
carbon_condition_repair(&cond, 25.0f);   // Partial repair
carbon_condition_repair_full(&cond);      // Full repair

// Damage (requires repair before use)
carbon_condition_damage(&cond);

// Efficiency based on condition
float eff = carbon_condition_get_efficiency(&cond, 0.5f);  // 0.5 to 1.0

// Failure probability
float fail_chance = carbon_condition_get_failure_probability(&cond, 0.1f);
```

**Quality tiers:** `CARBON_QUALITY_LOW` (1.5x decay), `CARBON_QUALITY_STANDARD` (1.0x), `CARBON_QUALITY_HIGH` (0.5x)

**Status thresholds:** `CARBON_CONDITION_GOOD` (≥75%), `CARBON_CONDITION_FAIR` (≥50%), `CARBON_CONDITION_POOR` (≥25%), `CARBON_CONDITION_CRITICAL` (<25%)

## Financial Period Tracking

Track revenue/expenses over rolling time periods:

```c
#include "carbon/finances.h"

// Create tracker (30-second periods)
Carbon_FinancialTracker *finances = carbon_finances_create(30.0f);

// Record transactions
carbon_finances_record_revenue(finances, 1000);
carbon_finances_record_expense(finances, 500);

// Update each frame
carbon_finances_update(finances, delta_time);

// Or force period end (turn-based games)
carbon_finances_end_period(finances);

// Query current period
int32_t profit = carbon_finances_get_current_profit(finances);
int32_t revenue = carbon_finances_get_current_revenue(finances);

// Query historical
int32_t last = carbon_finances_get_last_profit(finances);
int32_t all_time = carbon_finances_get_all_time_profit(finances);

// Sum/average last N periods
Carbon_FinancialPeriod sum = carbon_finances_sum_periods(finances, 5);
Carbon_FinancialPeriod avg = carbon_finances_avg_periods(finances, 5);

// Period callback
void on_period_end(const Carbon_FinancialPeriod *period, void *userdata) {
    printf("Period profit: %d\n", carbon_finances_get_profit(period));
}
carbon_finances_set_period_callback(finances, on_period_end, NULL);

// Cleanup
carbon_finances_destroy(finances);
```

## Loan/Credit System

Tiered loans with interest:

```c
#include "carbon/loan.h"

// Create system with tiers
Carbon_LoanSystem *loans = carbon_loan_create();
carbon_loan_add_tier(loans, "Small Loan", 10000, 0.01f);   // 1% per period
carbon_loan_add_tier(loans, "Medium Loan", 50000, 0.015f); // 1.5%
carbon_loan_add_tier(loans, "Large Loan", 100000, 0.02f);  // 2%

// Per-player state
Carbon_LoanState state;
carbon_loan_state_init(&state);

// Take a loan
if (carbon_loan_can_take(&state)) {
    int32_t money;
    carbon_loan_take(&state, loans, 0, &money);  // Tier 0
    player_money += money;
}

// Each period, charge interest
int32_t interest = carbon_loan_charge_interest(&state, loans);

// Query
int32_t owed = carbon_loan_get_amount_owed(&state);
int32_t next_interest = carbon_loan_get_projected_interest(&state, loans);

// Repay
if (carbon_loan_can_repay(&state, player_money)) {
    int32_t cost;
    carbon_loan_repay(&state, &cost);
    player_money -= cost;
}

// Cleanup
carbon_loan_destroy(loans);
```

## Dynamic Demand System

Demand values responding to service levels:

```c
#include "carbon/demand.h"

// Initialize (initial value 50, equilibrium 50)
Carbon_Demand demand;
carbon_demand_init(&demand, 50, 50);

// When service is provided
carbon_demand_record_service(&demand);

// Update (handles decay toward equilibrium)
carbon_demand_update(&demand, delta_time);

// Or tick manually (turn-based)
carbon_demand_tick(&demand);

// Query
uint8_t level = carbon_demand_get(&demand);  // 0-100
float mult = carbon_demand_get_multiplier(&demand);  // 0.5 to 2.0

// Use as price multiplier
int32_t price = base_price * mult;

// Level descriptions
const char *desc = carbon_demand_get_level_string(&demand);  // "Very Low" to "Very High"

// Custom multiplier range
float custom_mult = carbon_demand_get_multiplier_range(&demand, 0.8f, 1.5f);
```

## Incident/Random Event System

Probabilistic events based on condition:

```c
#include "carbon/incident.h"

// Configure severity distribution
Carbon_IncidentConfig config = {
    .base_probability = 0.1f,    // 10% base chance
    .minor_threshold = 0.70f,    // 70% of incidents are minor
    .major_threshold = 0.90f     // 20% major, 10% critical
};

// Check for incident based on condition
Carbon_IncidentType result = carbon_incident_check_condition(&cond, &config);

if (result != CARBON_INCIDENT_NONE) {
    switch (result) {
        case CARBON_INCIDENT_MINOR:
            // Temporary effect
            break;
        case CARBON_INCIDENT_MAJOR:
            // Lasting effect
            break;
        case CARBON_INCIDENT_CRITICAL:
            // Severe consequence
            break;
    }
}

// Or calculate probability manually
float prob = carbon_incident_calc_probability(condition_percent, quality_mult);
Carbon_IncidentType type = carbon_incident_check(prob, &config);

// Simple probability roll
if (carbon_incident_roll(0.3f)) {
    // 30% chance triggered
}

// Deterministic random (for reproducible behavior)
carbon_incident_seed(game_seed);
float r = carbon_incident_random();  // 0.0 to 1.0
int n = carbon_incident_random_range(1, 6);  // 1 to 6
```

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
