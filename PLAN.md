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
- [ ] Design collision API (`agentite_collision_*`)
- [ ] Implement shape primitives: AABB, Circle, OBB, Capsule
- [ ] Add shape-vs-shape tests for all primitive combinations
- [ ] Implement collision result struct (normal, depth, contact points)
- [ ] Add broad-phase using existing spatial hash
- [ ] Implement collision layers and masks
- [ ] Add raycast queries against shapes
- [ ] Add point-in-shape queries
- [ ] Add shape sweep/cast tests
- [ ] Integrate with ECS as optional C_Collider component
- [ ] Add debug visualization via gizmos

### Simple Kinematic Physics
- [ ] Design physics API (`agentite_physics_*`)
- [ ] Implement kinematic body (position, velocity, acceleration)
- [ ] Add gravity support (global and per-body)
- [ ] Implement drag/friction coefficients
- [ ] Add collision response (bounce, slide, stop)
- [ ] Implement trigger volumes (non-solid collision detection)
- [ ] Add physics step with fixed timestep accumulator
- [ ] Integrate with ECS as C_PhysicsBody component

### Chipmunk2D Integration (Optional Full Physics)
Research indicates [Chipmunk2D](https://github.com/slembcke/Chipmunk2D) is the best fit:
- MIT license (permissive, compatible with Agentite)
- Pure C99, no external dependencies
- Lightweight and fast, designed for 2D games
- Features: rigid bodies, joints, collision filtering, raycasting, sleeping objects
- Easier API than Box2D, well-documented

- [ ] Add Chipmunk2D to lib/ directory
- [ ] Create wrapper API (`agentite_physics2d_*`)
- [ ] Implement space management (create, step, destroy)
- [ ] Add body creation helpers (static, dynamic, kinematic)
- [ ] Add shape creation (circle, box, polygon, segment)
- [ ] Implement constraint/joint wrappers
- [ ] Add collision callbacks and filtering
- [ ] Integrate with ECS transform system
- [ ] Add debug draw integration with gizmos
- [ ] Document when to use simple physics vs Chipmunk2D

## Phase 2: Procedural & Visual Polish

### Procedural Noise System
- [ ] Design noise API (`agentite_noise_*`)
- [ ] Implement Perlin noise 2D/3D
- [ ] Implement Simplex noise 2D/3D
- [ ] Add Worley/cellular noise
- [ ] Implement fractal Brownian motion (fBm)
- [ ] Add ridged multifractal noise
- [ ] Add turbulence function
- [ ] Implement domain warping helpers
- [ ] Add noise-to-tilemap generation utility
- [ ] Create heightmap generation helpers
- [ ] Add resource/biome distribution helpers for strategy maps

### Shader System
- [ ] Design shader API (`agentite_shader_*`)
- [ ] Implement shader loading from SPIR-V files
- [ ] Add runtime shader compilation (optional, platform-dependent)
- [ ] Create shader parameter binding API
- [ ] Implement uniform buffer management
- [ ] Add built-in shader library: grayscale, blur, glow, outline
- [ ] Implement post-processing pipeline
- [ ] Add fullscreen quad rendering helper
- [ ] Document shader authoring workflow

### Screen Transitions
- [ ] Design transition API (`agentite_transition_*`)
- [ ] Implement fade (in/out, to color)
- [ ] Add wipe transitions (directional)
- [ ] Add dissolve/pixelate transitions
- [ ] Implement crossfade between render targets
- [ ] Add transition callbacks (on_start, on_complete)
- [ ] Integrate with scene system for automatic transitions

### Basic 2D Lighting
- [ ] Design lighting API (`agentite_light_*`)
- [ ] Implement point lights with radius and falloff
- [ ] Add ambient light layer
- [ ] Implement light occlusion/shadows (raycast-based)
- [ ] Add light color and intensity
- [ ] Implement light blending with sprites
- [ ] Add day/night cycle helper
- [ ] Create shadow casting for tilemaps

## Phase 3: Developer Experience

### Performance Profiler
- [ ] Design profiler API (`agentite_profiler_*`)
- [ ] Implement frame time tracking (update, render, present)
- [ ] Add draw call counter
- [ ] Track batch count and vertex count
- [ ] Add entity count monitoring
- [ ] Implement memory allocation tracking
- [ ] Create profiler overlay UI widget
- [ ] Add frame time graph (rolling history)
- [ ] Implement scope-based profiling (`AGENTITE_PROFILE_SCOPE`)
- [ ] Add CSV/JSON export for external analysis
- [ ] Track system-specific timings (AI, physics, rendering)

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
