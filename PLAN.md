# Agentite Engine Feature Roadmap

## Phase 1: Core Visual & Gameplay Systems

### Particle System
- [x] Design particle emitter API (`agentite_particle_*`)
- [x] Implement particle pool with configurable max particles (default 10,000)
- [x] Add emitter types: point, line, circle, rectangle
- [x] Implement particle properties: lifetime, velocity, acceleration, gravity
- [x] Add color interpolation (start/end color with easing)
- [x] Add size interpolation (start/end size)
- [x] Implement rotation and angular velocity
- [x] Add texture/sprite support with UV animation
- [x] Implement blend modes: additive, alpha, multiply
- [x] Add emission patterns: burst, continuous, timed
- [x] Integrate with sprite renderer batch system
- [x] Add world-space vs local-space modes
- [x] Create preset particles: explosion, smoke, fire, sparks, rain, snow

### Collision Detection System
- [x] Design collision API (`agentite_collision_*`)
- [x] Implement shape primitives: AABB, Circle, OBB, Capsule
- [x] Add shape-vs-shape tests for all primitive combinations
- [x] Implement collision result struct (normal, depth, contact points)
- [x] Add broad-phase using existing spatial hash
- [x] Implement collision layers and masks
- [x] Add raycast queries against shapes
- [x] Add point-in-shape queries
- [x] Add shape sweep/cast tests
- [x] Integrate with ECS as optional C_Collider component
- [x] Add debug visualization via gizmos

### Simple Kinematic Physics
- [x] Design physics API (`agentite_physics_*`)
- [x] Implement kinematic body (position, velocity, acceleration)
- [x] Add gravity support (global and per-body)
- [x] Implement drag/friction coefficients
- [x] Add collision response (bounce, slide, stop)
- [x] Implement trigger volumes (non-solid collision detection)
- [x] Add physics step with fixed timestep accumulator
- [x] Integrate with ECS as C_PhysicsBody component

### Chipmunk2D Integration (Optional Full Physics)
Research indicates [Chipmunk2D](https://github.com/slembcke/Chipmunk2D) is the best fit:
- MIT license (permissive, compatible with Agentite)
- Pure C99, no external dependencies
- Lightweight and fast, designed for 2D games
- Features: rigid bodies, joints, collision filtering, raycasting, sleeping objects
- Easier API than Box2D, well-documented

- [x] Add Chipmunk2D to lib/ directory
- [x] Create wrapper API (`agentite_physics2d_*`)
- [x] Implement space management (create, step, destroy)
- [x] Add body creation helpers (static, dynamic, kinematic)
- [x] Add shape creation (circle, box, polygon, segment)
- [x] Implement constraint/joint wrappers
- [x] Add collision callbacks and filtering
- [x] Integrate with ECS transform system
- [x] Add debug draw integration with gizmos
- [x] Document when to use simple physics vs Chipmunk2D

## Phase 2: Procedural & Visual Polish

### Procedural Noise System
- [x] Design noise API (`agentite_noise_*`)
- [x] Implement Perlin noise 2D/3D
- [x] Implement Simplex noise 2D/3D
- [x] Add Worley/cellular noise
- [x] Implement fractal Brownian motion (fBm)
- [x] Add ridged multifractal noise
- [x] Add turbulence function
- [x] Implement domain warping helpers
- [x] Add noise-to-tilemap generation utility
- [x] Create heightmap generation helpers
- [x] Add resource/biome distribution helpers for strategy maps

### Shader System
- [x] Design shader API (`agentite_shader_*`)
- [x] Implement shader loading from SPIR-V files
- [x] Add runtime shader compilation (optional, platform-dependent)
- [x] Create shader parameter binding API
- [x] Implement uniform buffer management
- [x] Add built-in shader library: grayscale, blur, glow, outline
- [x] Implement post-processing pipeline
- [x] Add fullscreen quad rendering helper
- [x] Document shader authoring workflow

### Screen Transitions
- [x] Design transition API (`agentite_transition_*`)
- [x] Implement fade (in/out, to color)
- [x] Add wipe transitions (directional)
- [x] Add dissolve/pixelate transitions
- [x] Implement crossfade between render targets
- [x] Add transition callbacks (on_start, on_complete)
- [x] Integrate with scene system for automatic transitions

### Basic 2D Lighting
- [x] Design lighting API (`agentite_light_*`)
- [x] Implement point lights with radius and falloff
- [x] Add ambient light layer
- [x] Implement light occlusion/shadows (raycast-based)
- [x] Add light color and intensity
- [x] Implement light blending with sprites
- [x] Add day/night cycle helper
- [x] Create shadow casting for tilemaps

## Phase 3: Developer Experience

### Performance Profiler
- [x] Design profiler API (`agentite_profiler_*`)
- [x] Implement frame time tracking (update, render, present)
- [x] Add draw call counter
- [x] Track batch count and vertex count
- [x] Add entity count monitoring
- [x] Implement memory allocation tracking
- [x] Create profiler overlay UI widget
- [x] Add frame time graph (rolling history)
- [x] Implement scope-based profiling (`AGENTITE_PROFILE_SCOPE`)
- [x] Add CSV/JSON export for external analysis
- [x] Track system-specific timings (AI, physics, rendering)

### Localization System
- [ ] Design localization API (`agentite_loc_*`)
- [ ] Implement string table loading (TOML or JSON format)
- [ ] Add language switching at runtime
- [ ] Implement string key lookup with fallback
- [ ] Add parameter substitution (`{0}`, `{name}`)
- [ ] Support pluralization rules
- [ ] Add font switching per language
- [ ] Integrate with UI text widgets
- [ ] Create localization validation tool

### Enhanced Debug Tools
- [ ] Add entity gizmo overlay (show positions, velocities)
- [ ] Implement collision shape visualization toggle
- [ ] Add AI path visualization
- [ ] Create spatial hash grid overlay
- [ ] Implement fog of war debug view
- [ ] Add turn/phase state inspector
- [ ] Create console command system for runtime debugging

## Phase 4: Advanced Features

### Replay System
- [ ] Design replay API (`agentite_replay_*`)
- [ ] Leverage existing command queue for recording
- [ ] Implement replay file format (versioned, compressed)
- [ ] Add replay playback with speed control
- [ ] Implement seek/scrub functionality
- [ ] Add replay metadata (timestamp, version, duration)
- [ ] Create replay UI widget

### Hot Reload / Mod Support
- [ ] Implement asset file watching
- [ ] Add texture hot reload
- [ ] Add scene/prefab hot reload
- [ ] Implement data file hot reload (TOML configs)
- [ ] Create mod loading system (load from mod directories)
- [ ] Add mod manifest format
- [ ] Implement mod load order and dependencies

### Networking (Future)
- [ ] Research: ENet vs custom UDP
- [ ] Design network API (`agentite_net_*`)
- [ ] Implement client/server architecture
- [ ] Add reliable/unreliable message channels
- [ ] Implement entity state synchronization
- [ ] Add input prediction and reconciliation
- [ ] Create lobby/matchmaking helpers

## Notes

### Physics Engine Selection

After researching permissively-licensed 2D physics engines:

| Engine | License | Language | Pros | Cons |
|--------|---------|----------|------|------|
| [Chipmunk2D](https://github.com/slembcke/Chipmunk2D) | MIT | C99 | Lightweight, game-focused, easy API, no deps | No continuous collision |
| [Box2D](https://github.com/erincatto/box2d) | MIT | C | Feature-rich, continuous collision | More complex API |
| [Ferox](https://github.com/jdeokkim/ferox) | MIT | C | Educational, very lightweight | Less mature |

**Recommendation: Chipmunk2D** for full physics needs. It matches Agentite's C-style API design, has no external dependencies, and is proven in production games. For simple cases, the built-in kinematic physics layer will suffice.

### Implementation Order Rationale

1. **Particle System** - Immediate visual impact, relatively self-contained
2. **Collision System** - Foundation for physics and gameplay
3. **Simple Physics** - Builds on collision, enables projectiles/movement
4. **Procedural Noise** - Critical for strategy game map generation
5. **Profiler** - Needed before adding more systems
6. **Everything else** - Based on project needs

### API Design Guidelines

All new systems should follow existing Agentite conventions:
- `agentite_<system>_create()` / `agentite_<system>_destroy()`
- Config structs with `_DEFAULT` macros
- Thread-local errors via `agentite_get_last_error()`
- Opaque types with forward declarations
- C11 headers, C++17 implementation
