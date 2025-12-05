# Carbon Engine Reorganization Plan

This plan reorganizes the Carbon game engine to maximize its usefulness for Claude Code-assisted game development.

## Status: COMPLETED

All phases have been implemented successfully. The engine now has:
- A unified `Carbon_GameContext` for easy system management
- A complete game template with state machine and ECS integration
- 7 example projects demonstrating different features
- Comprehensive documentation in CLAUDE.md and SYSTEMS.md
- JSON schemas for data-driven development

---

## Implementation Summary

### Phase 1: Game Template Structure - COMPLETED

#### 1.1 Game Context System - DONE
- `include/carbon/game_context.h` - Unified context struct with all engine systems
- `src/core/game_context.c` - Proper initialization/cleanup order with rollback on failure

#### 1.2 Convenience Helpers - DONE
- `include/carbon/helpers.h` - Macros for sprite/text batching, math utilities, timers

#### 1.3 Game Template Files - DONE
- `src/game/game.h/c` - Game lifecycle management
- `src/game/components.h/c` - Game-specific ECS components (Player, Enemy, Speed, Collider, etc.)
- `src/game/systems/` - Movement, collision, and AI systems
- `src/game/states/` - State machine with menu, playing, paused states
- `src/game/data/loader.h/c` - JSON parsing and level loading

---

### Phase 2: Reorganize Examples - COMPLETED

#### 2.1 Move Current Demo - DONE
- `examples/demo/main.c` - Original comprehensive demo
- `examples/demo/README.md` - Documentation

#### 2.2 Minimal Example - DONE
- `examples/minimal/main.c` - ~40 lines, window + clear color

#### 2.3 Sprites Example - DONE
- `examples/sprites/main.c` - Sprite transforms, batching, camera

#### 2.4 Animation Example - DONE
- `examples/animation/main.c` - Playback modes, callbacks, speed control

#### 2.5 Tilemap Example - DONE
- `examples/tilemap/main.c` - Multi-layer tilemap with camera scrolling

#### 2.6 UI Example - DONE
- `examples/ui/main.c` - All widget types, panels, event handling

#### 2.7 Strategy Example - DONE
- `examples/strategy/main.c` - RTS selection, A* pathfinding, unit movement

#### 2.8 New Main Entry Point - DONE
- `src/main.c` - Bootstrap using game template (~100 lines)

---

### Phase 3: Documentation - COMPLETED

#### 3.1 Systems Dependency Document - DONE
- `SYSTEMS.md` - Init order, frame structure, system diagram, common patterns

#### 3.2 Data Schemas - DONE
- `assets/schemas/entity.schema.json`
- `assets/schemas/level.schema.json`
- `assets/schemas/animation.schema.json`

#### 3.3 Update CLAUDE.md - DONE
Added sections:
- Quick Start (New Game)
- Build Commands (updated with all targets)
- Project Structure (updated)
- Common Patterns (entity types, game states, level loading)
- Troubleshooting

#### 3.4 Example READMEs - DONE
Each example has a README.md with usage and patterns.

---

### Phase 4: Error Context System - COMPLETED

#### 4.1 Error Handling - DONE
- `include/carbon/error.h` - Thread-local error API
- `src/core/error.c` - Implementation with SDL integration

#### 4.2 Update Existing Systems - DEFERRED
The error system is available for use. Updating existing systems can be done incrementally as needed.

---

### Phase 5: Build System - COMPLETED

#### 5.1 Makefile Updates - DONE
- Added `src/game/` source files
- Added `src/core/game_context.c` and `src/core/error.c`
- Added example targets: `make example-minimal`, `make example-sprites`, etc.
- `make run` runs new main.c with game template
- `make run-demo` runs the comprehensive demo
- `make help` shows all available targets

---

## Verification

All success criteria met:

1. ✅ `make` builds the new game template successfully
2. ✅ `make run` runs the new bootstrap with game template
3. ✅ `make run-demo` runs the full feature demo
4. ✅ `make example-minimal` builds and runs minimal example
5. ✅ All examples compile and run correctly
6. ✅ CLAUDE.md contains clear instructions for common tasks
7. ✅ SYSTEMS.md documents all dependencies
8. ✅ New developers can start a game by copying the template
9. ✅ Claude Code can reliably add features to predictable locations

---

## New File Structure

```
include/carbon/
├── carbon.h            # Core engine API
├── game_context.h      # NEW: Unified context
├── helpers.h           # NEW: Convenience macros
├── error.h             # NEW: Error handling
└── [existing headers]

src/
├── main.c              # REPLACED: Game template bootstrap
├── core/
│   ├── engine.c
│   ├── game_context.c  # NEW
│   └── error.c         # NEW
├── game/               # NEW: Game template
│   ├── game.h/c
│   ├── components.h/c
│   ├── systems/
│   │   ├── systems.h/c
│   │   ├── movement.c
│   │   ├── collision.c
│   │   └── ai.c
│   ├── states/
│   │   ├── state.h/c
│   │   ├── menu.c
│   │   ├── playing.c
│   │   └── paused.c
│   └── data/
│       ├── loader.h
│       └── loader.c
└── [existing directories]

examples/               # NEW
├── demo/
├── minimal/
├── sprites/
├── animation/
├── tilemap/
├── ui/
└── strategy/

assets/schemas/         # NEW
├── entity.schema.json
├── level.schema.json
└── animation.schema.json

SYSTEMS.md              # NEW
PLAN.md                 # This file (updated)
CLAUDE.md               # Updated with new sections
Makefile                # Updated with new targets
```
