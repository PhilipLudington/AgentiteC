# Carbon Engine Documentation

## API Reference

### Core Systems

| System | Description | Header |
|--------|-------------|--------|
| [Sprite](api/sprite.md) | Batched sprite rendering | `carbon/sprite.h` |
| [Animation](api/animation.md) | Sprite-based animation | `carbon/animation.h` |
| [Camera](api/camera.md) | 2D/3D camera control | `carbon/camera.h`, `camera3d.h` |
| [Tilemap](api/tilemap.md) | Chunk-based tile rendering | `carbon/tilemap.h` |
| [Text](api/text.md) | Bitmap and SDF text | `carbon/text.h` |
| [Input](api/input.md) | Action-based input | `carbon/input.h` |
| [Audio](api/audio.md) | Sound and music | `carbon/audio.h` |
| [ECS](api/ecs.md) | Entity Component System | `carbon/ecs.h` |
| [Pathfinding](api/pathfinding.md) | A* pathfinding | `carbon/pathfinding.h` |
| [UI](api/ui.md) | Immediate-mode GUI | `carbon/ui.h` |

### AI Systems

| System | Description | Header |
|--------|-------------|--------|
| [AI Personality](api/ai/personality.md) | Personality-driven decisions | `carbon/ai.h` |
| [HTN Planner](api/ai/htn.md) | Hierarchical Task Network | `carbon/htn.h` |
| [Blackboard](api/ai/blackboard.md) | Shared data coordination | `carbon/blackboard.h` |
| [AI Tracks](api/ai/tracks.md) | Multi-track decisions | `carbon/ai_tracks.h` |
| [Strategy](api/ai/strategy.md) | Phase detection & utility | `carbon/strategy.h` |
| [Task Queue](api/ai/task.md) | Sequential task execution | `carbon/task.h` |

### Strategy Game Systems

| System | Description | Header |
|--------|-------------|--------|
| [Turn/Phase](api/strategy/turn.md) | Turn-based game flow | `carbon/turn.h` |
| [Technology](api/strategy/tech.md) | Research tree | `carbon/tech.h` |
| [Victory](api/strategy/victory.md) | Victory conditions | `carbon/victory.h` |
| [Fog of War](api/strategy/fog.md) | Exploration & visibility | `carbon/fog.h` |
| [Resources](api/strategy/resource.md) | Economy management | `carbon/resource.h` |
| [Spatial Index](api/strategy/spatial.md) | O(1) entity lookup | `carbon/spatial.h` |
| [Modifiers](api/strategy/modifier.md) | Value modifier stacks | `carbon/modifier.h` |
| [Construction](api/strategy/construction.md) | Ghost buildings | `carbon/construction.h` |
| [Anomaly](api/strategy/anomaly.md) | Discoveries & research | `carbon/anomaly.h` |
| [Siege](api/strategy/siege.md) | Location assault | `carbon/siege.h` |
| [Other Systems](api/strategy/other.md) | Rate, Network, Dialog, etc. | Various |

### Utilities

| System | Description | Header |
|--------|-------------|--------|
| [Utilities](api/utilities.md) | Containers, validation, formula, events, etc. | Various |

## Quick Links

- [Main CLAUDE.md](../CLAUDE.md) - Build commands, architecture, common patterns
- [Examples](../examples/) - Working code examples
- [Assets Schemas](../assets/schemas/) - JSON schemas for data files
