# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

## Coding Standards

This project follows **Carbide** standards for safe, maintainable C/C++ code:

- **[CARBIDE.md](CARBIDE.md)** - AI development guide with patterns and examples
- **[STANDARDS.md](STANDARDS.md)** - Detailed coding rules and conventions

**Quick Reference:**
- Types: `PascalCase` with `Agentite_` prefix
- Functions: `snake_case` with `agentite_` prefix
- Check all allocations for NULL
- Use `_create`/`_destroy` pairs
- Validate external input at boundaries

**Available Commands:**
- `/carbide-review <file>` - Check code against standards
- `/carbide-safety <file>` - Security-focused review
- `make check` - Run clang-tidy static analysis
- `make safety` - Run security checks
- `make format` - Auto-format code

## Project Overview

Agentite is a C-based game engine targeting strategy games and AI agent development. Uses SDL3 with SDL_GPU for cross-platform rendering (Metal on macOS, Vulkan on Linux, D3D12/Vulkan on Windows).

**Status:** Experimental - all systems are under active development.

## Build Commands

```bash
make              # Build
make DEBUG=1      # Build with debug symbols
make run          # Build and run
make clean        # Clean build
make info         # Show build configuration
```

## Architecture

```
src/
├── main.cpp            # Entry point, game loop
├── core/
│   ├── engine.cpp      # SDL3/GPU setup, frame management
│   ├── error.cpp       # Error handling
│   ├── event.cpp       # Pub-sub event dispatcher
│   ├── command.cpp     # Command queue for validated actions
│   ├── formula.cpp     # Expression evaluation engine
│   └── game_context.cpp # Unified system container
├── ecs/ecs.cpp         # ECS wrapper around Flecs
├── graphics/
│   ├── sprite.cpp      # Batched sprite rendering
│   ├── animation.cpp   # Sprite animation
│   ├── camera.cpp      # 2D camera
│   ├── camera3d.cpp    # 3D orbital camera
│   ├── tilemap.cpp     # Chunk-based tilemap
│   └── text.cpp        # TrueType + SDF fonts
├── audio/audio.cpp     # Sound/music playback
├── input/input.cpp     # Action-based input
├── ai/
│   ├── pathfinding.cpp # A* pathfinding
│   ├── personality.cpp # AI personality system
│   ├── ai_tracks.cpp   # Multi-track AI decisions
│   ├── blackboard.cpp  # Shared AI coordination
│   ├── htn.cpp         # HTN AI planner
│   └── task.cpp        # Task queue system
├── strategy/           # Turn-based strategy systems
│   ├── turn.cpp, resource.cpp, tech.cpp, victory.cpp
│   ├── fog.cpp, spatial.cpp, modifier.cpp
│   ├── construction.cpp, blueprint.cpp
│   ├── anomaly.cpp, siege.cpp, biome.cpp
│   └── ... (see docs/README.md for full list)
├── ui/                 # Immediate-mode GUI (AUI)
└── game/               # Game template

include/agentite/       # Public API headers
lib/                    # Dependencies (flecs, stb, cglm)
```

**Core engine flow:**
1. `agentite_init()` - Creates window, initializes SDL3, creates GPU device
2. Main loop: `agentite_begin_frame()` → `agentite_poll_events()` → `agentite_begin_render_pass()` → render → `agentite_end_render_pass()` → `agentite_end_frame()`
3. `agentite_shutdown()` - Cleanup

## System Dependencies

Understanding which systems depend on others helps avoid initialization errors and guides destruction order.

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           SYSTEM DEPENDENCY GRAPH                           │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌─────────────┐                                                            │
│  │ GPU Device  │◄─────────────────────────────────────────┐                 │
│  └──────┬──────┘                                          │                 │
│         │ creates                                         │                 │
│         ▼                                                 │                 │
│  ┌─────────────────┐     ┌─────────────┐                  │                 │
│  │ Sprite Renderer │────►│  Texture    │──────────────────┤                 │
│  └────────┬────────┘     └──────┬──────┘                  │                 │
│           │                     │ referenced by           │                 │
│           │ uploads to          ▼                         │                 │
│           │              ┌─────────────┐    ┌──────────┐  │                 │
│           │              │   Sprite    │    │ Tilemap  │──┘                 │
│           │              └─────────────┘    └──────────┘                    │
│           │                                                                 │
│           ▼                                                                 │
│  ┌─────────────────┐                                                        │
│  │ Command Buffer  │◄───────────────────────────────────────────────┐       │
│  └────────┬────────┘                                                │       │
│           │                                                         │       │
│           ▼                                                         │       │
│  ┌─────────────────┐     ┌─────────────┐     ┌─────────────────┐    │       │
│  │  Render Pass    │     │ Text Render │────►│ Font / MSDF     │────┘       │
│  └─────────────────┘     └─────────────┘     └─────────────────┘            │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  GAME SYSTEMS (independent of graphics)                                     │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌────────────┐      ┌─────────────┐      ┌──────────────┐                  │
│  │ ECS World  │◄─────│ Components  │      │ Input System │                  │
│  └─────┬──────┘      └─────────────┘      └──────┬───────┘                  │
│        │                                         │                          │
│        │ queries                                 │ events                   │
│        ▼                                         ▼                          │
│  ┌────────────┐      ┌─────────────┐      ┌──────────────┐                  │
│  │  Systems   │      │ Pathfinding │      │      UI      │◄─── consumes     │
│  └────────────┘      └─────────────┘      └──────────────┘     events first │
│                                                                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  STRATEGY SYSTEMS (can be used independently)                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ┌──────────┐   ┌───────────┐   ┌─────────────┐   ┌────────────────┐        │
│  │ Turn Mgr │   │ Resources │   │  Tech Tree  │   │ Fleet Manager  │        │
│  └──────────┘   └───────────┘   └─────────────┘   └────────────────┘        │
│                                                                             │
│  ┌──────────┐   ┌───────────┐   ┌─────────────┐   ┌────────────────┐        │
│  │ Fog/War  │   │  Spatial  │   │ Combat Sys  │   │ Power Network  │        │
│  └──────────┘   └───────────┘   └─────────────┘   └────────────────┘        │
│                                                                             │
└─────────────────────────────────────────────────────────────────────────────┘

DESTRUCTION ORDER (reverse of creation):
  1. Game systems (ECS entities, strategy systems)
  2. Textures, Fonts
  3. Renderers (Sprite, Text, Tilemap)
  4. GPU Device / Engine
```

### Key Dependency Rules

| If you use... | You must first create... | And destroy in reverse |
|---------------|-------------------------|------------------------|
| `Sprite` | `Texture` | Sprite usage stops → Texture destroy |
| `Texture` | `SpriteRenderer` | Texture destroy → Renderer destroy |
| `Tilemap` | `Texture` + `Camera` | Tilemap destroy → Texture destroy |
| `Text Renderer` | `Font` | Font outlives text draws |
| `ECS System` | `ECS World` + `Components` | Delete entities → Destroy world |
| `UI` | `Input` (optional) | UI consumes input first |

### When to Use Which System

```
Need to...                              → Use
─────────────────────────────────────────────────────────────
Render sprites/images                   → sprite.h + texture
Animate sprite sequences                → animation.h
Render tile-based maps                  → tilemap.h
Display text                            → text.h (bitmap) or msdf.h (SDF)
Handle keyboard/mouse/gamepad           → input.h
Play sounds/music                       → audio.h
Manage game entities                    → ecs.h (Flecs wrapper)
Find paths on a grid                    → pathfinding.h
Show menus/buttons/dialogs              → ui.h
─────────────────────────────────────────────────────────────
Simple AI behaviors                     → ai.h (personality)
Complex goal-oriented AI                → htn.h (HTN planner)
Share data between AI agents            → blackboard.h
Parallel AI decision tracks             → ai_tracks.h
Sequential task execution               → task.h
─────────────────────────────────────────────────────────────
Turn-based game flow                    → turn.h
Tech trees and research                 → tech.h
Multiple victory conditions             → victory.h
Exploration/visibility                  → fog.h
Economy with stockpiles                 → resource.h
Fast entity spatial lookup              → spatial.h
Buff/debuff stacking                    → modifier.h
Building placement preview              → construction.h + blueprint.h
Tactical grid combat                    → combat.h
Fleet/army management                   → fleet.h
Factory power distribution              → power.h
Resolution-independent rendering        → virtual_resolution.h
```

## API Documentation

See [docs/README.md](docs/README.md) for comprehensive API documentation organized by system:

- **Core:** Sprite, Animation, Camera, Tilemap, Text, Input, Audio, ECS, Pathfinding, UI
- **AI:** Personality, HTN Planner, Blackboard, AI Tracks, Strategy Coordinator, Task Queue
- **Strategy:** Turn/Phase, Technology, Victory, Fog of War, Resources, Spatial Index, Modifiers, Construction, Anomaly, Siege, and more
- **Utilities:** Containers, Validation, Formula Engine, Events, View Model, Safe Math

## Quick Start (New Game)

```c
#include "agentite/game_context.h"

int main(int argc, char *argv[]) {
    Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
    config.window_title = "My Game";
    config.font_path = "assets/fonts/font.ttf";

    Agentite_GameContext *ctx = agentite_game_context_create(&config);
    if (!ctx) return 1;

    while (agentite_game_context_is_running(ctx)) {
        agentite_game_context_begin_frame(ctx);
        agentite_game_context_poll_events(ctx);
        // game_update()...
        // render...
        agentite_game_context_end_frame(ctx);
    }

    agentite_game_context_destroy(ctx);
    return 0;
}
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

2. Register in `src/game/components.cpp`:
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

4. Register system: `ECS_SYSTEM(world, ProjectileSystem, EcsOnUpdate, C_Projectile);`

### Adding a New Game State

1. Create state file `src/game/states/newstate.cpp` with `enter`, `exit`, `update`, `render` functions
2. Add to state enum in `src/game/states/state.h`
3. Register in `src/game/game.cpp` with `game_state_machine_register()`

### Render Loop Pattern

```c
// Before render pass - upload batched data
agentite_sprite_begin(sr, NULL);
agentite_sprite_draw(sr, &sprite, x, y);
agentite_sprite_upload(sr, cmd);

// During render pass
agentite_sprite_render(sr, cmd, pass);
```

## Troubleshooting

### Build Issues

**SDL3 not found:**
```bash
# macOS
brew install sdl3
```

### Runtime Issues

**Black screen:**
- Check `agentite_begin_render_pass()` return value
- Ensure upload calls happen before render pass

**Sprites not showing:**
- Check texture loaded successfully
- Verify batch order: `begin` → `draw` → `upload` → render pass → `render`

**Input not working:**
- Call `agentite_input_begin_frame()` at start of frame
- Call `agentite_input_update()` after polling events
- Check UI isn't consuming events

### Error Handling

```c
Agentite_Texture *tex = agentite_texture_load(sprites, "missing.png");
if (!tex) {
    SDL_Log("Failed: %s", agentite_get_last_error());
}
```

## Development Notes

- Target platforms: macOS, Linux, Windows
- Language: C11 headers, C++17 implementation
- Graphics: SDL3 SDL_GPU (auto-selects Metal/Vulkan/D3D12)
- ECS: Flecs v4.0.0
- Math: cglm (SIMD-optimized)

## Conventions

For general C/C++ coding standards (memory management, error handling, security), see [CARBIDE.md](CARBIDE.md) and [STANDARDS.md](STANDARDS.md).

### Agentite-Specific Naming

| Prefix/Suffix | Usage | Example |
|---------------|-------|---------|
| `Agentite_` | Public types (structs, enums) | `Agentite_Camera` |
| `agentite_` | Public functions | `agentite_sprite_draw()` |
| `C_` | ECS components | `C_Position`, `C_Velocity` |
| `S_` | ECS singleton components | `S_GameState` |

**Internal Naming (in .cpp files):**
- Static functions: lowercase with underscores, no prefix
- Static variables: `s_` prefix
- Constants: `k_` prefix or `SCREAMING_CASE`

### Agentite Lifetime Rules

- Textures must outlive sprites/tilemaps using them
- Renderers must outlive their resources (textures, fonts)
- GPU device must outlive all renderers
- Destroying a renderer invalidates all resources created from it
- Iterator/query results in ECS are borrowed; invalid after world modifications

### Thread Safety

**Main Thread Only (NOT thread-safe):**
- All SDL functions
- All rendering functions (`_render()`, `_draw()`, `_upload()`)
- Window/input handling
- ECS world modifications

**Thread-Safe:**
- `agentite_get_last_error()` (thread-local storage)
- Read-only ECS queries (with proper synchronization)
- Formula evaluation (after parsing)
- Pure math functions (cglm)

**Safe Patterns for Background Loading:**
```c
// WRONG - GPU operations on background thread
void* load_thread(void* arg) {
    tex = agentite_texture_load(sr, path);  // CRASH!
}

// RIGHT - Load data on background, create GPU resource on main
void* load_thread(void* arg) {
    // Load raw image data (CPU only)
    image_data = stbi_load(path, &w, &h, &c, 4);
}
// Then on main thread:
tex = agentite_texture_create_from_data(sr, image_data, w, h);
```

## Common Pitfalls

### Rendering Order

**Upload Before Render Pass:**
```c
// WRONG - upload during render pass
agentite_begin_render_pass(engine, cmd, &pass);
agentite_sprite_upload(sr, cmd);  // TOO LATE!
agentite_sprite_render(sr, cmd, pass);

// RIGHT - upload before render pass
agentite_sprite_upload(sr, cmd);
agentite_begin_render_pass(engine, cmd, &pass);
agentite_sprite_render(sr, cmd, pass);
```

**Correct Render Layer Order:**
1. Tilemap (background)
2. Sprites (game objects)
3. Text (labels, HUD)
4. UI (menus, dialogs - rendered last, on top)

**Batch Operations Must Be Paired:**
```c
agentite_sprite_begin(sr, camera);  // MUST call before draw
agentite_sprite_draw(sr, ...);
agentite_sprite_draw(sr, ...);
agentite_sprite_upload(sr, cmd);    // MUST call after all draws
// ... begin render pass ...
agentite_sprite_render(sr, cmd, pass);
```

### ECS Pitfalls

**Never Delete During Iteration:**
```c
// WRONG - modifies world during iteration
void BadSystem(ecs_iter_t *it) {
    for (int i = 0; i < it->count; i++) {
        if (should_remove) {
            ecs_delete(it->world, it->entities[i]);  // CRASH or undefined!
        }
    }
}

// RIGHT - defer deletion
void GoodSystem(ecs_iter_t *it) {
    for (int i = 0; i < it->count; i++) {
        if (should_remove) {
            ecs_delete(it->world, it->entities[i]);  // Deferred by default in systems
        }
    }
}
// Or use ecs_defer_begin/end for manual control
```

**Component Pointers Invalidate:**
```c
// WRONG - pointer may be invalid after add
C_Position *pos = ecs_get_mut(world, entity, C_Position);
ecs_add(world, entity, C_Velocity);  // May relocate entity!
pos->x = 10;  // CRASH - pos may be stale

// RIGHT - get pointer after modifications
ecs_add(world, entity, C_Velocity);
C_Position *pos = ecs_get_mut(world, entity, C_Position);
pos->x = 10;  // Safe
```

**System Execution Order:**
- Systems run in registration order within a phase
- Use `EcsOnUpdate`, `EcsPreUpdate`, `EcsPostUpdate` for ordering
- For explicit ordering: `ecs_add_pair(world, sys, EcsDependsOn, other_sys)`

**Field Indices Start at 0:**
```c
// Query: C_Position, C_Velocity
C_Position *pos = ecs_field(it, C_Position, 0);  // First field = 0
C_Velocity *vel = ecs_field(it, C_Velocity, 1);  // Second field = 1
```

### Memory Pitfalls

**Texture Lifetime:**
```c
// WRONG - texture destroyed while sprite references it
Agentite_Texture *tex = agentite_texture_load(sr, "sprite.png");
Agentite_Sprite sprite = { .texture = tex, ... };
agentite_texture_destroy(tex);
agentite_sprite_draw(sr, &sprite, x, y);  // CRASH - tex is freed

// RIGHT - keep texture alive while in use
// Destroy textures only after they're no longer referenced
```

**Renderer Destruction Order:**
```c
// WRONG - destroy renderer before resources
agentite_sprite_renderer_destroy(sr);
agentite_texture_destroy(tex);  // CRASH - tex belongs to destroyed renderer

// RIGHT - destroy resources first
agentite_texture_destroy(tex);
agentite_sprite_renderer_destroy(sr);
```

**String Parameters Are Copied:**
```c
// This is safe - engine copies the string
char temp[256];
snprintf(temp, sizeof(temp), "Player %d", id);
config.window_title = temp;  // OK - copied during create
agentite_game_context_create(&config);
// temp can go out of scope
```

### Coordinate Systems

**Screen Coordinates:**
- Origin: top-left (0, 0)
- X increases rightward
- Y increases downward
- Units: pixels

**World Coordinates:**
- Can use any convention (engine/camera handles transform)
- Common: origin at center, Y-up for math, Y-down for 2D games

**Tile Coordinates:**
- Integer-based (tile_x, tile_y)
- Convert to world: `world_x = tile_x * tile_width`
- Convert from world: `tile_x = floor(world_x / tile_width)`

**Camera Transform:**
```c
// Screen → World
vec2 world_pos;
agentite_camera_screen_to_world(camera, screen_x, screen_y, world_pos);

// World → Screen
vec2 screen_pos;
agentite_camera_world_to_screen(camera, world_x, world_y, screen_pos);
```

### Input Pitfalls

**Frame Timing:**
```c
// WRONG - missing begin_frame
while (running) {
    agentite_input_update(input);  // pressed/released won't work!
}

// RIGHT
while (running) {
    agentite_input_begin_frame(input);  // Clear previous frame state
    // ... poll events ...
    agentite_input_update(input);       // Process this frame
}
```

**UI Event Consumption:**
```c
// WRONG - game processes event UI already handled
SDL_Event event;
while (SDL_PollEvent(&event)) {
    aui_process_event(ui, &event);
    agentite_input_process_event(input, &event);  // Double-handling!
}

// RIGHT - check if UI consumed event
while (SDL_PollEvent(&event)) {
    if (!aui_process_event(ui, &event)) {
        agentite_input_process_event(input, &event);
    }
}
```

**Action vs Raw Input:**
```c
// Prefer actions for game logic (rebindable)
if (agentite_input_action_pressed(input, "jump")) { ... }

// Use raw input only for UI/editor (specific key needed)
if (agentite_input_key_pressed(input, SDL_SCANCODE_ESCAPE)) { ... }
```
