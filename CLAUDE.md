# CLAUDE.md

This file provides guidance to Claude Code when working with code in this repository.

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
