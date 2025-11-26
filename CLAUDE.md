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
├── platform/           # Platform-specific code (future)
├── graphics/           # Rendering, sprites, shaders (future)
├── audio/              # Sound system (future)
├── input/              # Input handling (future)
├── ui/                 # GUI system (future)
└── game/               # Game-specific logic (future)

include/carbon/
└── carbon.h            # Public API header

lib/
├── stb_image.h         # Image loading
├── stb_truetype.h      # Font rendering
├── stb_rect_pack.h     # Rectangle packing
└── cglm/               # Math library (vec, mat, etc.)
```

**Core engine flow:**
1. `carbon_init()` - Creates window, initializes SDL3, creates GPU device
2. Main loop: `carbon_begin_frame()` → `carbon_poll_events()` → `carbon_begin_render_pass()` → render → `carbon_end_render_pass()` → `carbon_end_frame()`
3. `carbon_shutdown()` - Cleanup

## Development Notes

- Target platforms: macOS, Linux, Windows
- Language: C11
- Graphics: SDL3 SDL_GPU (auto-selects Metal/Vulkan/D3D12)
- Math: cglm (SIMD-optimized)
- Repository: https://github.com/PhilipLudington/Carbon
