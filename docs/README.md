# AgentiteC Engine Documentation

## API Reference

### Core Systems

| System | Description | Header |
|--------|-------------|--------|
| [Sprite](api/sprite.md) | Batched sprite rendering | `agentite/sprite.h` |
| [Animation](api/animation.md) | Sprite-based animation | `agentite/animation.h` |
| [Camera](api/camera.md) | 2D/3D camera control | `agentite/camera.h`, `camera3d.h` |
| [Tilemap](api/tilemap.md) | Chunk-based tile rendering | `agentite/tilemap.h` |
| [Text](api/text.md) | Bitmap and SDF text | `agentite/text.h` |
| [Shader](api/shader.md) | Shader system & post-processing | `agentite/shader.h` |
| [Transition](api/transition.md) | Screen transitions & effects | `agentite/transition.h` |
| [Input](api/input.md) | Action-based input | `agentite/input.h` |
| [Audio](api/audio.md) | Sound and music | `agentite/audio.h` |
| [ECS](api/ecs.md) | Entity Component System | `agentite/ecs.h` |
| [Pathfinding](api/pathfinding.md) | A* pathfinding | `agentite/pathfinding.h` |
| [UI](api/ui.md) | Immediate-mode GUI | `agentite/ui.h` |

### AI Systems

| System | Description | Header |
|--------|-------------|--------|
| [AI Personality](api/ai/personality.md) | Personality-driven decisions | `agentite/ai.h` |
| [HTN Planner](api/ai/htn.md) | Hierarchical Task Network | `agentite/htn.h` |
| [Blackboard](api/ai/blackboard.md) | Shared data coordination | `agentite/blackboard.h` |
| [AI Tracks](api/ai/tracks.md) | Multi-track decisions | `agentite/ai_tracks.h` |
| [Strategy](api/ai/strategy.md) | Phase detection & utility | `agentite/strategy.h` |
| [Task Queue](api/ai/task.md) | Sequential task execution | `agentite/task.h` |

### Strategy Game Systems

| System | Description | Header |
|--------|-------------|--------|
| [Turn/Phase](api/strategy/turn.md) | Turn-based game flow | `agentite/turn.h` |
| [Technology](api/strategy/tech.md) | Research tree | `agentite/tech.h` |
| [Victory](api/strategy/victory.md) | Victory conditions | `agentite/victory.h` |
| [Fog of War](api/strategy/fog.md) | Exploration & visibility | `agentite/fog.h` |
| [Resources](api/strategy/resource.md) | Economy management | `agentite/resource.h` |
| [Spatial Index](api/strategy/spatial.md) | O(1) entity lookup | `agentite/spatial.h` |
| [Modifiers](api/strategy/modifier.md) | Value modifier stacks | `agentite/modifier.h` |
| [Construction](api/strategy/construction.md) | Ghost buildings | `agentite/construction.h` |
| [Anomaly](api/strategy/anomaly.md) | Discoveries & research | `agentite/anomaly.h` |
| [Siege](api/strategy/siege.md) | Location assault | `agentite/siege.h` |
| [Combat](api/strategy/combat.md) | Tactical grid combat | `agentite/combat.h` |
| [Fleet](api/strategy/fleet.md) | Army/fleet management | `agentite/fleet.h` |
| [Power](api/strategy/power.md) | Power grid distribution | `agentite/power.h` |
| [Virtual Resolution](api/virtual_resolution.md) | Resolution-independent rendering | `agentite/virtual_resolution.h` |
| [Other Systems](api/strategy/other.md) | Rate, Network, Dialog, etc. | Various |

### Developer Tools

| System | Description | Header |
|--------|-------------|--------|
| [Profiler](api/profiler.md) | Performance profiling | `agentite/profiler.h` |
| [Localization](api/localization.md) | Multi-language support | `agentite/localization.h` |

### Utilities

| System | Description | Header |
|--------|-------------|--------|
| [Utilities](api/utilities.md) | Containers, validation, formula, events, etc. | Various |

## Guides

| Guide | Description |
|-------|-------------|
| [Integration Recipes](integration-recipes.md) | Multi-system patterns for common game features |

## Quick Links

- [Main CLAUDE.md](../CLAUDE.md) - Build commands, architecture, common patterns
- [Examples](../examples/) - Working code examples
- [Assets Schemas](../assets/schemas/) - JSON schemas for data files
