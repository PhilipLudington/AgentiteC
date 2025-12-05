# Carbon Engine Reorganization Plan

This plan reorganizes the Carbon game engine to maximize its usefulness for Claude Code-assisted game development.

## Overview

The goal is to transform Carbon from a demo-focused engine into a game-template-focused engine with:
- Clear, predictable file locations
- Unified context management
- Multiple reference examples
- Comprehensive documentation of patterns and dependencies

---

## Phase 1: Create Game Template Structure

### 1.1 Create Game Context System

**File: `include/carbon/game_context.h`**
- Define `Carbon_GameContext` struct containing all engine systems
- Add `carbon_game_context_create()` and `carbon_game_context_destroy()`
- Provides single point of access to all systems

**File: `src/core/game_context.c`**
- Implement context creation with proper initialization order
- Implement destruction with proper cleanup order
- Handle errors gracefully with rollback on failure

### 1.2 Create Convenience Helpers

**File: `include/carbon/helpers.h`**
- Add macros for common patterns (sprite batching, text batching)
- Add utility functions for coordinate conversion
- Add frame timing helpers

### 1.3 Create Game Template Files

**File: `src/game/game.h`**
- Define `Game` struct for game-specific state
- Declare `game_init()`, `game_shutdown()`, `game_update()`, `game_render()`
- Include game subsystem headers

**File: `src/game/game.c`**
- Implement game lifecycle functions
- Initialize game state machine
- Set up game-specific ECS systems

**File: `src/game/components.h`**
- Define game-specific ECS components
- Provide registration macro/function
- Include examples: `C_Player`, `C_Enemy`, `C_Projectile`

**File: `src/game/systems/systems.h`**
- Declare all game ECS systems
- Provide `game_systems_register()` function

**File: `src/game/systems/movement.c`**
- Example movement system using C_Position and C_Velocity
- Demonstrates ECS query patterns

**File: `src/game/systems/collision.c`**
- Example collision detection system
- Demonstrates entity iteration patterns

**File: `src/game/states/state.h`**
- Define game state interface (enter, exit, update, render)
- Define state machine struct and functions
- Declare built-in states

**File: `src/game/states/menu.c`**
- Menu state implementation
- Demonstrates UI integration

**File: `src/game/states/playing.c`**
- Main gameplay state
- Demonstrates full game loop integration

**File: `src/game/states/paused.c`**
- Pause state with overlay
- Demonstrates state transitions

**File: `src/game/data/loader.h`**
- Level/entity data loading interface
- JSON parsing utilities (using a simple parser)

**File: `src/game/data/loader.c`**
- Implementation of data loading
- Error handling for missing/malformed data

---

## Phase 2: Reorganize Examples

### 2.1 Move Current Demo

**Action:** Move `src/main.c` to `examples/demo/main.c`
- Keep all current functionality
- Update any relative paths
- Add README.md explaining what demo shows

### 2.2 Create Minimal Example

**File: `examples/minimal/main.c`**
- Absolute minimum (~50 lines)
- Window, clear screen, quit on escape
- Template for new projects

**File: `examples/minimal/README.md`**
- Explains minimal setup
- Shows how to extend

### 2.3 Create Sprites Example

**File: `examples/sprites/main.c`**
- Sprite loading and rendering
- Basic transforms (scale, rotate)
- Camera integration

### 2.4 Create Animation Example

**File: `examples/animation/main.c`**
- Sprite sheet loading
- Animation playback modes
- State-based animation switching

### 2.5 Create Tilemap Example

**File: `examples/tilemap/main.c`**
- Tileset and tilemap creation
- Multiple layers
- Camera scrolling with bounds

### 2.6 Create UI Example

**File: `examples/ui/main.c`**
- UI panel creation
- All widget types demonstrated
- Event handling

### 2.7 Create Strategy Game Example

**File: `examples/strategy/main.c`**
- RTS-style unit selection
- Pathfinding integration
- Tilemap with fog of war concept

**File: `examples/strategy/README.md`**
- Explains strategy game patterns
- Shows how to extend for full game

### 2.8 Create New Main Entry Point

**File: `src/main.c` (new)**
- Simple bootstrap (~50 lines)
- Uses game template
- Shows recommended game structure

---

## Phase 3: Add Documentation

### 3.1 Create Systems Dependency Document

**File: `SYSTEMS.md`**
- Initialization order with dependencies
- Render order documentation
- System interaction diagram (ASCII)
- Common integration patterns

### 3.2 Create Data Schemas

**File: `assets/schemas/entity.schema.json`**
- JSON schema for entity definitions
- Component data structure
- Validation rules

**File: `assets/schemas/level.schema.json`**
- JSON schema for level files
- Tilemap references
- Entity spawn points

**File: `assets/schemas/animation.schema.json`**
- JSON schema for animation definitions
- Frame timing
- Playback modes

### 3.3 Update CLAUDE.md

Add new sections:
- Quick Start (New Game)
- Common Patterns (with code examples)
- Adding New Entity Types
- Adding New Game States
- Adding New Systems
- Data File Formats
- Troubleshooting

### 3.4 Add Example READMEs

Each example directory gets a README.md explaining:
- What the example demonstrates
- Key files and their purposes
- How to run
- How to extend

---

## Phase 4: Add Error Context System

### 4.1 Create Error Handling

**File: `include/carbon/error.h`**
- `carbon_set_error(const char *fmt, ...)`
- `const char *carbon_get_last_error(void)`
- `void carbon_clear_error(void)`

**File: `src/core/error.c`**
- Thread-local error buffer
- Printf-style formatting
- Integration with SDL_GetError()

### 4.2 Update Existing Systems

Update all systems to use new error reporting:
- `src/core/engine.c`
- `src/graphics/sprite.c`
- `src/graphics/text.c`
- `src/graphics/animation.c`
- `src/graphics/tilemap.c`
- `src/graphics/camera.c`
- `src/input/input.c`
- `src/audio/audio.c`
- `src/ai/pathfinding.c`
- `src/ecs/ecs.c`

---

## Phase 5: Update Build System

### 5.1 Update Makefile

- Add `src/game/` source files
- Add `src/core/game_context.c`
- Add `src/core/error.c`
- Add example build targets (`make example-minimal`, `make example-demo`, etc.)
- Add `make new-game NAME=mygame` target to copy template
- Update `make run` to run new main.c
- Add `make run-demo` to run the full demo

### 5.2 Create Example Makefiles

Each example gets its own simple Makefile or uses main Makefile targets.

---

## File Summary

### New Files to Create

```
include/carbon/
├── game_context.h      # Unified context struct
├── helpers.h           # Convenience macros
└── error.h             # Error handling

src/core/
├── game_context.c      # Context implementation
└── error.c             # Error implementation

src/game/
├── game.h              # Game interface
├── game.c              # Game implementation
├── components.h        # Game components
├── systems/
│   ├── systems.h       # Systems interface
│   ├── movement.c      # Movement system
│   └── collision.c     # Collision system
├── states/
│   ├── state.h         # State machine
│   ├── menu.c          # Menu state
│   ├── playing.c       # Playing state
│   └── paused.c        # Paused state
└── data/
    ├── loader.h        # Data loading interface
    └── loader.c        # Data loading implementation

examples/
├── minimal/
│   ├── main.c
│   └── README.md
├── demo/
│   ├── main.c          # (moved from src/main.c)
│   └── README.md
├── sprites/
│   ├── main.c
│   └── README.md
├── animation/
│   ├── main.c
│   └── README.md
├── tilemap/
│   ├── main.c
│   └── README.md
├── ui/
│   ├── main.c
│   └── README.md
└── strategy/
    ├── main.c
    └── README.md

assets/schemas/
├── entity.schema.json
├── level.schema.json
└── animation.schema.json

SYSTEMS.md              # Dependency documentation
```

### Files to Modify

```
src/main.c              # Replace with simple bootstrap
CLAUDE.md               # Add new sections
Makefile                # Add new targets and sources
```

### Files to Move

```
src/main.c → examples/demo/main.c
```

---

## Implementation Order

1. **Phase 1.1-1.2**: Game context and helpers (foundation)
2. **Phase 4**: Error handling (needed by other phases)
3. **Phase 2.1**: Move demo (clears path for new main.c)
4. **Phase 1.3**: Game template files
5. **Phase 2.8**: New main.c bootstrap
6. **Phase 5**: Update Makefile (make it buildable)
7. **Phase 2.2-2.7**: Create examples
8. **Phase 3**: Documentation
9. **Phase 4.2**: Update existing systems with error reporting

---

## Success Criteria

After implementation:

1. `make` builds the new game template successfully
2. `make run` runs the new bootstrap with game template
3. `make run-demo` runs the full feature demo
4. `make example-minimal` builds and runs minimal example
5. All examples compile and run correctly
6. CLAUDE.md contains clear instructions for common tasks
7. SYSTEMS.md documents all dependencies
8. New developers can start a game by copying the template
9. Claude Code can reliably add features to predictable locations
