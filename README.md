# Agentite

A C game engine designed for AI agents and agentic game development.

**Status: Experimental** - All systems are under active development. APIs may change.

## Design Philosophy

Agentite is built around these core principles:

- **AI/Agent-First**: Designed from the ground up for use by AI agents and in agentic workflows. Clear, predictable APIs that AI can reason about and use effectively.

- **AI/Agent-Developed**: This engine is being developed collaboratively with Claude Code. The codebase serves as a testbed for AI-assisted software development.

- **Simplicity Over Features**: Prefer straightforward implementations over clever abstractions. Code should be readable and debuggable.

- **Minimal Dependencies**: Only essential third-party libraries. Every dependency is carefully considered for its value vs. complexity tradeoff.

- **Data-Oriented Design**: Favor data transformations over deep object hierarchies. Cache-friendly structures where performance matters.

- **ECS-First Architecture**: Entity Component System as the primary organizational pattern, powered by Flecs.

- **C11 Simplicity**: A C API with C++ implementation where needed. No templates, no inheritance hierarchies, no hidden allocations.

## Features

### Core Engine
- SDL3 with SDL_GPU for cross-platform rendering (Metal, Vulkan, D3D12)
- Batched sprite rendering with camera support
- Chunk-based tilemap system
- TrueType font rendering with SDF/MSDF support
- Action-based input system
- Audio playback (sounds and music)

### AI Systems
- A* pathfinding with customizable heuristics
- Hierarchical Task Network (HTN) planner
- Blackboard for shared AI state
- Multi-track AI decision system
- Personality system for character behavior

### Strategy Game Systems
- Turn and phase management
- Technology trees
- Victory condition tracking
- Fog of war
- Resource management
- Spatial indexing
- Modifier system
- Construction and blueprints

### UI
- Immediate-mode UI system (AUI)
- Panels, buttons, sliders, text input
- Tables with sorting and multi-select
- Color picker
- Theming support

## Quick Start

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

        // Your game logic here

        agentite_game_context_end_frame(ctx);
    }

    agentite_game_context_destroy(ctx);
    return 0;
}
```

## Building

### Requirements
- C11/C++17 compiler (gcc, clang, or MSVC)
- SDL3
- Make

### macOS
```bash
brew install sdl3
make
make run
```

### Linux
```bash
# Ubuntu/Debian
sudo apt install libsdl3-dev

# Fedora
sudo dnf install SDL3-devel

make
make run
```

### Windows (MinGW)
```bash
# Install SDL3, then:
make
```

### Build Options
```bash
make              # Release build
make DEBUG=1      # Debug build with symbols
make run          # Build and run
make clean        # Clean build artifacts
make test         # Run tests
make info         # Show build configuration
```

## Examples

```bash
make example-minimal      # Basic window setup
make example-sprites      # Sprite rendering
make example-animation    # Animation system
make example-tilemap      # Tilemap rendering
make example-ui           # UI system demo (spinbox, tooltips, gamepad)
make example-ui-node      # Retained-mode UI with tweens
make example-charts       # Data visualization (line, bar, pie, real-time)
make example-richtext     # BBCode formatted text with animations
make example-dialogs      # Modal dialogs, file dialogs, notifications
make example-strategy     # Strategy game patterns
make example-strategy-sim # Strategy systems simulation
make example-pathfinding  # A* pathfinding demo
make example-ecs          # Custom ECS systems
make example-msdf         # Scalable text rendering
```

## Project Structure

```
include/agentite/    # Public API headers
src/
  core/              # Engine core (SDL3, GPU, events)
  graphics/          # Sprite, tilemap, text, camera
  audio/             # Sound and music
  input/             # Input handling
  ui/                # Immediate-mode UI (AUI)
  ecs/               # Flecs wrapper
  ai/                # Pathfinding, HTN, personalities
  strategy/          # Turn-based strategy systems
  game/              # Game template
lib/                 # Third-party libraries
examples/            # Example projects
docs/                # API documentation
tests/               # Unit tests
```

## Documentation

See [docs/README.md](docs/README.md) for comprehensive API documentation.

See [CLAUDE.md](CLAUDE.md) for development guidance when working with AI assistants.

## Future Direction

- **Platform Support**: Continued focus on macOS, Linux, and Windows
- **Fewer Dependencies**: Investigating alternatives to reduce third-party code
- **New Systems**: Additional AI and strategy game systems as needed
- **Performance**: Profiling and optimization for real-world game workloads
- **Examples**: More comprehensive examples and tutorials

## Third-Party Libraries

Agentite uses the following libraries (see [THIRD_PARTY_LICENSES.md](THIRD_PARTY_LICENSES.md) for full license texts):

| Library | Purpose | License |
|---------|---------|---------|
| SDL3 | Windowing, input, GPU abstraction | zlib |
| Flecs | Entity Component System | MIT |
| cglm | SIMD math operations | MIT |
| stb_image | Image loading | Public Domain/MIT |
| stb_truetype | Font rasterization | Public Domain/MIT |
| stb_rect_pack | Texture atlas packing | Public Domain/MIT |
| tomlc99 | TOML parsing | MIT |
| Catch2 | Testing (dev only) | BSL-1.0 |

## License

MIT License - see [LICENSE](LICENSE) for details.

Copyright (c) 2025 Philip Ludington

---

*Agentite is developed with assistance from [Claude Code](https://claude.ai/code).*
