# Carbon Engine Feature Plan

Features extracted from Machinae codebase for potential addition to Carbon game engine.

## Features Already in Carbon

- Grid-based pathfinding (A*)
- Sprite/animation system
- Text rendering (TrueType + SDF)
- UI system with themes
- Camera system (2D and 3D)
- Audio system
- Save/load framework
- Event dispatcher
- Resource economy basics
- Technology tree
- Victory conditions
- AI personality system
- View model / data binding
- Spatial hash index (O(1) entity lookup by grid cell)
- Fog of war / exploration system
- Rate tracking (rolling window metrics)
- Task queue system (sequential task execution for AI agents)
- Network/graph system (union-find for power grid/resource distribution)
- Blueprint system (building templates with capture/placement)
- Construction queue / ghost building (planned buildings with progress tracking)
- Dialog / narrative system (event-driven dialog queue with speaker types)
- Game speed system (variable simulation speed with pause support)

---

## New Features to Add

### Priority 1: Core Infrastructure

#### 1.1 Spatial Hash Index
O(1) entity lookup by grid cell for efficient spatial queries.

**Concept:**
- Hash table mapping packed grid coordinates to entity/item indices
- Operations: add, remove, query, move (all O(1))
- Useful for: item pickup, collision detection, entity queries at location

**API Sketch:**
```c
Carbon_SpatialIndex *carbon_spatial_create(int capacity);
void carbon_spatial_add(index, int x, int y, uint32_t entity_id);
void carbon_spatial_remove(index, int x, int y, uint32_t entity_id);
uint32_t carbon_spatial_query(index, int x, int y);  // Returns entity or 0
bool carbon_spatial_has(index, int x, int y);
void carbon_spatial_move(index, int old_x, int old_y, int new_x, int new_y, uint32_t entity_id);
void carbon_spatial_destroy(index);
```

#### 1.2 Network/Graph System (Power Grid)
Union-find algorithm for connected component grouping with resource distribution.

**Concept:**
- Nodes (e.g., power poles) define coverage areas
- Union-find groups connected nodes into networks
- Each network tracks production vs consumption
- Dirty flag for lazy recalculation

**API Sketch:**
```c
Carbon_NetworkSystem *carbon_network_create(void);
uint32_t carbon_network_add_node(network, int x, int y, int radius);
void carbon_network_remove_node(network, uint32_t node_id);
void carbon_network_set_production(network, uint32_t node_id, int32_t amount);
void carbon_network_set_consumption(network, uint32_t node_id, int32_t amount);
int carbon_network_get_group(network, uint32_t node_id);
bool carbon_network_is_powered(network, int group_id);
bool carbon_network_covers_cell(network, int x, int y);
void carbon_network_recalculate(network);
void carbon_network_destroy(network);
```

#### 1.3 Fog of War / Exploration System
Per-cell exploration tracking with visibility radius.

**Concept:**
- Grid of exploration states: unexplored, explored, visible
- Entities have vision radius
- Cells become explored when within vision radius
- Optional: shroud (explored but not currently visible)

**API Sketch:**
```c
Carbon_FogOfWar *carbon_fog_create(int width, int height);
void carbon_fog_set_vision_source(fog, int x, int y, int radius);
void carbon_fog_clear_vision_sources(fog);
void carbon_fog_update(fog);  // Recalculate visibility
Carbon_VisibilityState carbon_fog_get_state(fog, int x, int y);  // UNEXPLORED, EXPLORED, VISIBLE
bool carbon_fog_is_visible(fog, int x, int y);
bool carbon_fog_is_explored(fog, int x, int y);
void carbon_fog_reveal_all(fog);
void carbon_fog_reset(fog);
void carbon_fog_destroy(fog);
```

---

### Priority 2: Gameplay Systems

#### 2.1 Blueprint System
Save and place building templates with relative positioning.

**Concept:**
- Capture selection of buildings into template
- Store relative positions and rotations
- Preview before placement
- Validate placement (collision, resources)

**API Sketch:**
```c
Carbon_Blueprint *carbon_blueprint_create(const char *name);
void carbon_blueprint_add_entry(blueprint, int rel_x, int rel_y, int building_type, int direction);
Carbon_Blueprint *carbon_blueprint_capture(buildings, int x1, int y1, int x2, int y2);
bool carbon_blueprint_can_place(blueprint, int origin_x, int origin_y, validator_fn);
void carbon_blueprint_get_entries(blueprint, Carbon_BlueprintEntry *out, int *count);
void carbon_blueprint_rotate(blueprint);  // 90 degrees clockwise
const char *carbon_blueprint_get_name(blueprint);
void carbon_blueprint_destroy(blueprint);

// Blueprint library
Carbon_BlueprintLibrary *carbon_blueprint_library_create(int max_blueprints);
void carbon_blueprint_library_add(library, Carbon_Blueprint *blueprint);
Carbon_Blueprint *carbon_blueprint_library_get(library, int index);
Carbon_Blueprint *carbon_blueprint_library_find(library, const char *name);
void carbon_blueprint_library_destroy(library);
```

#### 2.2 Ghost Building / Construction Queue
Planned buildings with progress tracking before actual construction.

**Concept:**
- Ghost = planned building, not yet constructed
- Consumes resources when placed (or when construction starts)
- Progress tracking (0% to 100%)
- Different construction speeds (player vs AI)
- Visual distinction from completed buildings

**API Sketch:**
```c
Carbon_ConstructionQueue *carbon_construction_create(int max_ghosts);
uint32_t carbon_construction_add_ghost(queue, int x, int y, int building_type, int direction);
void carbon_construction_remove_ghost(queue, uint32_t ghost_id);
void carbon_construction_update(queue, float delta_time);
float carbon_construction_get_progress(queue, uint32_t ghost_id);
void carbon_construction_set_speed(queue, uint32_t ghost_id, float speed);
bool carbon_construction_is_complete(queue, uint32_t ghost_id);
Carbon_Ghost *carbon_construction_get_ghost(queue, uint32_t ghost_id);
void carbon_construction_set_callback(queue, construction_complete_fn, void *userdata);
int carbon_construction_count(queue);
void carbon_construction_destroy(queue);
```

#### 2.3 Task Queue for AI
Sequential task execution system extending AI personality.

**Concept:**
- Queue of tasks for autonomous agents
- Task types: move, explore, collect, craft, build, withdraw, mine, wait
- Task lifecycle: pending, in_progress, completed, failed
- Integration with pathfinding and crafting systems

**API Sketch:**
```c
Carbon_TaskQueue *carbon_task_queue_create(int max_tasks);
void carbon_task_queue_add_move(queue, int target_x, int target_y);
void carbon_task_queue_add_explore(queue, int area_x, int area_y, int radius);
void carbon_task_queue_add_collect(queue, int x, int y, int resource_type);
void carbon_task_queue_add_craft(queue, int recipe_id, int quantity);
void carbon_task_queue_add_build(queue, int x, int y, int building_type);
void carbon_task_queue_add_wait(queue, float duration);
Carbon_Task *carbon_task_queue_current(queue);
void carbon_task_queue_advance(queue);  // Mark current complete, move to next
void carbon_task_queue_fail(queue, const char *reason);
void carbon_task_queue_clear(queue);
int carbon_task_queue_count(queue);
bool carbon_task_queue_is_empty(queue);
void carbon_task_queue_destroy(queue);
```

---

### Priority 3: Analytics & Feedback

#### 3.1 Rate Tracking / Metrics History
Rolling window metrics for production and consumption rates.

**Concept:**
- Periodic sampling of values (e.g., every 0.5s)
- Circular buffer of samples (e.g., 120 samples = 60 seconds)
- Multiple tracked metrics (resources, power, etc.)
- Time window queries (last 10s, 30s, 60s)
- Min/max/mean calculations

**API Sketch:**
```c
Carbon_RateTracker *carbon_rate_tracker_create(int metric_count, float sample_interval, int history_size);
void carbon_rate_tracker_update(tracker, float delta_time);
void carbon_rate_tracker_record(tracker, int metric_id, int32_t produced, int32_t consumed);
float carbon_rate_tracker_get_production_rate(tracker, int metric_id, float time_window);
float carbon_rate_tracker_get_consumption_rate(tracker, int metric_id, float time_window);
float carbon_rate_tracker_get_net_rate(tracker, int metric_id, float time_window);
void carbon_rate_tracker_get_history(tracker, int metric_id, Carbon_RateSample *out, int *count);
float carbon_rate_tracker_get_min(tracker, int metric_id, float time_window);
float carbon_rate_tracker_get_max(tracker, int metric_id, float time_window);
float carbon_rate_tracker_get_mean(tracker, int metric_id, float time_window);
void carbon_rate_tracker_destroy(tracker);
```

#### 3.2 Dialog / Narrative System
Event-driven dialog queue with speaker types.

**Concept:**
- Queue of messages with speaker attribution
- Event-triggered dialogs (milestones, discoveries)
- Event bitmask to prevent re-triggering
- Speaker types (AI, player, system, custom)

**API Sketch:**
```c
Carbon_DialogSystem *carbon_dialog_create(int max_messages);
void carbon_dialog_queue_message(dialog, Carbon_Speaker speaker, const char *text);
void carbon_dialog_queue_printf(dialog, Carbon_Speaker speaker, const char *fmt, ...);
void carbon_dialog_register_event(dialog, int event_id, Carbon_Speaker speaker, const char *text);
void carbon_dialog_trigger_event(dialog, int event_id);  // Only triggers once per event
bool carbon_dialog_has_message(dialog);
const Carbon_DialogMessage *carbon_dialog_current(dialog);
void carbon_dialog_advance(dialog);
void carbon_dialog_clear(dialog);
void carbon_dialog_reset_events(dialog);  // Allow events to trigger again
void carbon_dialog_destroy(dialog);

typedef enum {
    CARBON_SPEAKER_SYSTEM,
    CARBON_SPEAKER_PLAYER,
    CARBON_SPEAKER_AI,
    CARBON_SPEAKER_CUSTOM  // + custom ID
} Carbon_Speaker;
```

---

### Priority 4: World Generation

#### 4.1 Biome System
Terrain types affecting resource distribution and visuals.

**Concept:**
- Enum of biome types with properties
- Affects resource spawn rates
- Visual color coding
- Integration with tilemap and world generation

**API Sketch:**
```c
Carbon_BiomeSystem *carbon_biome_create(void);
int carbon_biome_register(biomes, const char *name, uint32_t color, float resource_mult);
void carbon_biome_set_resource_weight(biomes, int biome_id, int resource_type, float weight);
int carbon_biome_get_best_for_resource(biomes, int resource_type);
const char *carbon_biome_get_name(biomes, int biome_id);
uint32_t carbon_biome_get_color(biomes, int biome_id);
float carbon_biome_get_resource_weight(biomes, int biome_id, int resource_type);
void carbon_biome_destroy(biomes);
```

#### 4.2 Crafting State Machine
Progress-based crafting with batch support and speed multipliers.

**Concept:**
- Crafting progress (0.0 to 1.0)
- Speed multipliers per crafter
- Batch crafting (queue multiple)
- Completion callbacks
- Shared between player and AI

**API Sketch:**
```c
Carbon_CraftingState *carbon_crafting_create(void);
bool carbon_crafting_start(crafting, int recipe_id, int quantity);
void carbon_crafting_update(crafting, float delta_time);
void carbon_crafting_set_speed(crafting, float multiplier);
float carbon_crafting_get_progress(crafting);
int carbon_crafting_get_remaining(crafting);
bool carbon_crafting_is_active(crafting);
void carbon_crafting_cancel(crafting);
void carbon_crafting_set_callback(crafting, crafting_complete_fn, void *userdata);
void carbon_crafting_destroy(crafting);
```

---

### Priority 5: Simulation Control

#### 5.1 Game Speed System
Variable simulation speed with pause support.

**Concept:**
- Speed multipliers (0x pause, 1x, 2x, 4x, etc.)
- Scaled delta time for game logic
- UI remains at normal speed
- Pause state separate from speed

**API Sketch:**
```c
Carbon_GameSpeed *carbon_game_speed_create(void);
void carbon_game_speed_set(speed, float multiplier);  // 0.0 = pause, 1.0 = normal
float carbon_game_speed_get(speed);
void carbon_game_speed_pause(speed);
void carbon_game_speed_resume(speed);
bool carbon_game_speed_is_paused(speed);
float carbon_game_speed_scale_delta(speed, float raw_delta);  // Returns scaled delta
void carbon_game_speed_cycle(speed);  // Cycle through 1x -> 2x -> 4x -> 1x
void carbon_game_speed_destroy(speed);
```

---

## Implementation Order

1. ~~**Spatial Hash Index** - Foundation for other systems~~ ✅ DONE
2. ~~**Fog of War** - Common strategy game requirement~~ ✅ DONE
3. ~~**Rate Tracking** - Useful analytics for economy games~~ ✅ DONE
4. ~~**Task Queue** - Extends existing AI system~~ ✅ DONE
5. ~~**Network System** - Power/resource distribution~~ ✅ DONE
6. ~~**Blueprint System** - UX improvement for builders~~ ✅ DONE
7. ~~**Ghost Building** - Pairs with blueprints~~ ✅ DONE
8. ~~**Dialog System** - Narrative integration~~ ✅ DONE
9. ~~**Game Speed** - Simulation control~~ ✅ DONE
10. **Crafting State Machine** - Extends resource system
11. **Biome System** - World generation enhancement

---

## Notes

- All systems should integrate with existing Carbon patterns (error handling, validation, events)
- Each system should have corresponding unit tests in `tests/`
- Documentation should be added to `CLAUDE.md` following existing format
- Consider ECS integration where appropriate (components for fog, network nodes, etc.)
