# Strategy Game Systems Implementation Plan

This plan adds 9 engine-agnostic systems extracted from the Ecosocialism project to the Carbon engine. These systems are designed to support strategy games and simulations but are generic enough for any game type.

## Overview

| System | Priority | New Files | Description |
|--------|----------|-----------|-------------|
| Data Configuration | High | config.h/c | TOML-based data loading with hash lookup |
| Event System | High | event.h/c | Trigger-based events with player choices |
| Save/Load | High | save.h/c | Game state serialization with versioning |
| Turn Manager | Medium | turn.h/c | Multi-phase turn orchestration |
| History Tracking | Medium | history.h/c | Metrics snapshots and event log |
| Resource System | Medium | resource.h/c | Per-turn generation and spending |
| Modifier Stack | Medium | modifier.h/c | Composable effect multipliers |
| Unlock Tree | Medium | unlock.h/c | Prerequisites and tech trees |
| Threshold Tracker | Low | threshold.h/c | Value boundary callbacks |

---

## Phase 1: Core Infrastructure (High Priority)

### 1.1 Data Configuration System

**Files:**
- `include/carbon/config.h`
- `src/data/config.c`

**Dependencies:** None (standalone). Requires adding tomlc99 library.

**API Design:**

```c
#include "carbon/config.h"

// Generic definition with string ID and name
typedef struct Carbon_DataEntry {
    char id[64];
    char name[128];
    void *data;           // Points to type-specific struct
} Carbon_DataEntry;

// Loader manages multiple data types
typedef struct Carbon_DataLoader Carbon_DataLoader;

// Create/destroy
Carbon_DataLoader *carbon_data_create(void);
void carbon_data_destroy(Carbon_DataLoader *loader);

// Load data from TOML file with custom parser callback
typedef bool (*Carbon_DataParseFunc)(const char *key, void *toml_table, void *out_entry, void *userdata);
bool carbon_data_load(Carbon_DataLoader *loader, const char *path,
                      const char *array_key,  // e.g. "policy", "event"
                      size_t entry_size,      // sizeof(MyDataDef)
                      Carbon_DataParseFunc parse_func,
                      void *userdata);

// Access data
size_t carbon_data_count(const Carbon_DataLoader *loader);
void *carbon_data_get_by_index(const Carbon_DataLoader *loader, size_t index);
void *carbon_data_find(const Carbon_DataLoader *loader, const char *id);  // O(1) hash lookup
const char *carbon_data_get_last_error(const Carbon_DataLoader *loader);
```

**Implementation Notes:**
- Uses tomlc99 (single-header TOML parser, MIT license)
- Stores entries in contiguous array for cache-friendly iteration
- Builds hash map after load for O(1) ID lookup
- Parse callback allows game to define struct layout

**Example Usage:**
```c
typedef struct PolicyDef {
    char id[64];
    char name[128];
    int cost;
    float effect;
} PolicyDef;

bool parse_policy(const char *key, void *tbl, void *out, void *ud) {
    PolicyDef *p = out;
    strncpy(p->id, toml_string_at(tbl, "id"), 64);
    strncpy(p->name, toml_string_at(tbl, "name"), 128);
    p->cost = toml_int_at(tbl, "cost");
    p->effect = toml_double_at(tbl, "effect");
    return true;
}

Carbon_DataLoader *loader = carbon_data_create();
carbon_data_load(loader, "data/policies.toml", "policy", sizeof(PolicyDef), parse_policy, NULL);
PolicyDef *p = carbon_data_find(loader, "renewable_energy");
```

---

### 1.2 Event System

**Files:**
- `include/carbon/event.h` (game events, not SDL events)
- `src/data/event.c`

**Dependencies:** Data Configuration System (for loading event definitions)

**API Design:**

```c
#include "carbon/event.h"

// Maximum values
#define CARBON_EVENT_MAX_CHOICES 4
#define CARBON_EVENT_MAX_EFFECTS 16
#define CARBON_EVENT_MAX_VARS 16

// Effect types are game-defined via indices
typedef struct Carbon_EventEffect {
    int type;             // Game-defined effect type enum
    float value;          // Effect magnitude
} Carbon_EventEffect;

// Player choice
typedef struct Carbon_EventChoice {
    char label[64];
    char description[256];
    Carbon_EventEffect effects[CARBON_EVENT_MAX_EFFECTS];
    int effect_count;
} Carbon_EventChoice;

// Event definition (loadable from config)
typedef struct Carbon_EventDef {
    char id[64];
    char name[128];
    char description[512];
    char trigger[256];    // Expression: "health < 0.2" or "turn > 10"
    Carbon_EventChoice choices[CARBON_EVENT_MAX_CHOICES];
    int choice_count;
    bool one_shot;        // Only trigger once per game
} Carbon_EventDef;

// Trigger context - game fills with current values
typedef struct Carbon_TriggerContext {
    const char *var_names[CARBON_EVENT_MAX_VARS];
    float var_values[CARBON_EVENT_MAX_VARS];
    int var_count;
} Carbon_TriggerContext;

// Active event awaiting player choice
typedef struct Carbon_ActiveEvent {
    const Carbon_EventDef *def;
    bool resolved;
    int choice_made;      // -1 if not yet chosen
} Carbon_ActiveEvent;

// Event manager
typedef struct Carbon_EventManager Carbon_EventManager;

Carbon_EventManager *carbon_event_create(void);
void carbon_event_destroy(Carbon_EventManager *em);

// Register event definitions (typically from data loader)
void carbon_event_register(Carbon_EventManager *em, const Carbon_EventDef *def);

// Check triggers and potentially activate an event
// Returns true if a new event was triggered
bool carbon_event_check_triggers(Carbon_EventManager *em, const Carbon_TriggerContext *ctx);

// Query active event
bool carbon_event_has_pending(const Carbon_EventManager *em);
const Carbon_ActiveEvent *carbon_event_get_pending(const Carbon_EventManager *em);

// Make a choice (returns effects to apply)
bool carbon_event_choose(Carbon_EventManager *em, int choice_index);
const Carbon_EventChoice *carbon_event_get_chosen(const Carbon_EventManager *em);

// Clear resolved event
void carbon_event_clear_pending(Carbon_EventManager *em);

// Expression evaluation (public for game use)
bool carbon_event_evaluate(const char *expr, const Carbon_TriggerContext *ctx);
```

**Trigger Expression Format:**
- Simple: `"variable > 0.5"`, `"health < 20"`, `"turn >= 100"`
- Variables are looked up in TriggerContext by name
- Operators: `>`, `<`, `>=`, `<=`, `==`, `!=`

**Implementation Notes:**
- Simple tokenizer for trigger expressions
- Tracks triggered event IDs to prevent duplicates for one_shot events
- Cooldown counter between events (configurable)
- Game applies effects from chosen choice

---

### 1.3 Save/Load System

**Files:**
- `include/carbon/save.h`
- `src/data/save.c`

**Dependencies:** Data Configuration System (uses TOML for save format)

**API Design:**

```c
#include "carbon/save.h"

#define CARBON_SAVE_MAX_PATH 512
#define CARBON_SAVE_VERSION 1

// Save file info (for save list UI)
typedef struct Carbon_SaveInfo {
    char filename[256];
    char display_name[128];
    char timestamp[32];     // ISO 8601
    int version;
    bool is_compatible;

    // Game can add preview data via metadata
    int preview_turn;
    float preview_values[4]; // Game-defined preview metrics
} Carbon_SaveInfo;

// Result of save/load operation
typedef struct Carbon_SaveResult {
    bool success;
    char filepath[CARBON_SAVE_MAX_PATH];
    char error_message[256];
    int save_version;
    bool was_migrated;
} Carbon_SaveResult;

// Callbacks for game-specific serialization
typedef bool (*Carbon_SerializeFunc)(void *game_state, void *toml_table);
typedef bool (*Carbon_DeserializeFunc)(void *game_state, const void *toml_table);

// Save manager
typedef struct Carbon_SaveManager Carbon_SaveManager;

Carbon_SaveManager *carbon_save_create(const char *saves_dir);
void carbon_save_destroy(Carbon_SaveManager *sm);

// Set game version for compatibility checking
void carbon_save_set_version(Carbon_SaveManager *sm, int version, int min_compatible);

// Save/load with callbacks
Carbon_SaveResult carbon_save_game(Carbon_SaveManager *sm,
                                    const char *save_name,
                                    Carbon_SerializeFunc serialize,
                                    void *game_state);

Carbon_SaveResult carbon_load_game(Carbon_SaveManager *sm,
                                    const char *save_name,
                                    Carbon_DeserializeFunc deserialize,
                                    void *game_state);

// Quick save/load and autosave
Carbon_SaveResult carbon_save_quick(Carbon_SaveManager *sm, Carbon_SerializeFunc s, void *gs);
Carbon_SaveResult carbon_load_quick(Carbon_SaveManager *sm, Carbon_DeserializeFunc d, void *gs);
Carbon_SaveResult carbon_save_auto(Carbon_SaveManager *sm, Carbon_SerializeFunc s, void *gs);

// List saves for load screen
Carbon_SaveInfo *carbon_save_list(const Carbon_SaveManager *sm, int *out_count);
void carbon_save_list_free(Carbon_SaveInfo *list);

// Delete a save
bool carbon_save_delete(Carbon_SaveManager *sm, const char *save_name);

// Check if save exists
bool carbon_save_exists(const Carbon_SaveManager *sm, const char *save_name);
```

**TOML Save Format:**
```toml
[metadata]
version = 1
timestamp = "2025-01-15T10:30:00Z"
save_name = "quicksave"
game_version = "1.0.0"

[game_state]
# Game-specific data written by serialize callback
turn = 42
score = 1000

[player]
# More game data...
```

**Implementation Notes:**
- Creates saves directory if it doesn't exist
- Writes TOML format for human-readable saves
- Metadata section for quick preview without full parse
- Version checking with clear error messages
- Serialize/deserialize callbacks give game full control

---

## Phase 2: Turn-Based Game Support (Medium Priority)

### 2.1 Turn Manager

**Files:**
- `include/carbon/turn.h`
- `src/game/turn.c`

**Dependencies:** None (standalone)

**API Design:**

```c
#include "carbon/turn.h"

// Turn phases (game can define meaning)
typedef enum {
    CARBON_PHASE_WORLD_UPDATE = 0,  // AI/simulation runs
    CARBON_PHASE_EVENTS,            // Events trigger
    CARBON_PHASE_PLAYER_INPUT,      // Player makes decisions
    CARBON_PHASE_RESOLUTION,        // Apply player actions
    CARBON_PHASE_END_CHECK,         // Victory/defeat check
    CARBON_PHASE_COUNT
} Carbon_TurnPhase;

// Phase callback
typedef void (*Carbon_PhaseCallback)(void *userdata, int turn_number);

// Turn manager
typedef struct Carbon_TurnManager {
    int turn_number;
    Carbon_TurnPhase current_phase;
    Carbon_PhaseCallback phase_callbacks[CARBON_PHASE_COUNT];
    void *phase_userdata[CARBON_PHASE_COUNT];
    bool turn_in_progress;
} Carbon_TurnManager;

// Create with default phases
void carbon_turn_init(Carbon_TurnManager *tm);

// Set phase callback
void carbon_turn_set_callback(Carbon_TurnManager *tm, Carbon_TurnPhase phase,
                               Carbon_PhaseCallback callback, void *userdata);

// Advance to next phase (calls callback)
// Returns true if turn completed (wrapped back to first phase)
bool carbon_turn_advance(Carbon_TurnManager *tm);

// Skip to specific phase
void carbon_turn_skip_to(Carbon_TurnManager *tm, Carbon_TurnPhase phase);

// Query
Carbon_TurnPhase carbon_turn_current_phase(const Carbon_TurnManager *tm);
int carbon_turn_number(const Carbon_TurnManager *tm);
const char *carbon_turn_phase_name(Carbon_TurnPhase phase);
```

**Implementation Notes:**
- Lightweight struct (can be stack allocated)
- Callbacks are optional - can manually advance phases
- Phase names for debug UI
- No allocations needed

---

### 2.2 History Tracking

**Files:**
- `include/carbon/history.h`
- `src/game/history.c`

**Dependencies:** None (standalone)

**API Design:**

```c
#include "carbon/history.h"

#define CARBON_HISTORY_MAX_SNAPSHOTS 100
#define CARBON_HISTORY_MAX_EVENTS 50
#define CARBON_HISTORY_MAX_METRICS 16

// Metric snapshot (one per turn)
typedef struct Carbon_MetricSnapshot {
    int turn;
    float values[CARBON_HISTORY_MAX_METRICS];
} Carbon_MetricSnapshot;

// Significant event (game-defined types)
typedef struct Carbon_HistoryEvent {
    int turn;
    int type;               // Game-defined enum
    char title[64];
    char description[256];
    float value_before;
    float value_after;
} Carbon_HistoryEvent;

// Graph data for rendering
typedef struct Carbon_GraphData {
    float *values;
    int count;
    float min_value;
    float max_value;
} Carbon_GraphData;

// History tracker
typedef struct Carbon_History Carbon_History;

Carbon_History *carbon_history_create(void);
void carbon_history_destroy(Carbon_History *h);

// Register metric names (for debugging)
void carbon_history_set_metric_name(Carbon_History *h, int index, const char *name);

// Record a snapshot (circular buffer, keeps last N)
void carbon_history_add_snapshot(Carbon_History *h, const Carbon_MetricSnapshot *snap);

// Record a significant event
void carbon_history_add_event(Carbon_History *h, const Carbon_HistoryEvent *event);

// Query snapshots
int carbon_history_snapshot_count(const Carbon_History *h);
const Carbon_MetricSnapshot *carbon_history_get_snapshot(const Carbon_History *h, int index);

// Query events
int carbon_history_event_count(const Carbon_History *h);
const Carbon_HistoryEvent *carbon_history_get_event(const Carbon_History *h, int index);

// Get graph data for a metric (for UI graphing)
// Caller must free returned values array
Carbon_GraphData carbon_history_get_graph(const Carbon_History *h, int metric_index);
void carbon_graph_data_free(Carbon_GraphData *data);

// Clear all history
void carbon_history_clear(Carbon_History *h);
```

**Implementation Notes:**
- Circular buffer for snapshots (deque-style)
- Separate circular buffer for events
- Graph data normalized with min/max for easy rendering
- Metric names optional (for debug display)

---

### 2.3 Resource System

**Files:**
- `include/carbon/resource.h`
- `src/game/resource.c`

**Dependencies:** None (standalone)

**API Design:**

```c
#include "carbon/resource.h"

// Single resource type
typedef struct Carbon_Resource {
    int current;
    int maximum;            // 0 = unlimited
    int per_turn_base;
    float per_turn_modifier; // Multiplier (1.0 = normal)
} Carbon_Resource;

// Initialize with defaults
void carbon_resource_init(Carbon_Resource *r, int initial, int maximum, int per_turn);

// Per-turn tick (adds per_turn_base * per_turn_modifier, clamped to max)
void carbon_resource_tick(Carbon_Resource *r);

// Spending
bool carbon_resource_can_afford(const Carbon_Resource *r, int amount);
bool carbon_resource_spend(Carbon_Resource *r, int amount);

// Adding (respects maximum)
void carbon_resource_add(Carbon_Resource *r, int amount);

// Set values
void carbon_resource_set(Carbon_Resource *r, int value);
void carbon_resource_set_modifier(Carbon_Resource *r, float modifier);
void carbon_resource_set_per_turn(Carbon_Resource *r, int per_turn);
void carbon_resource_set_max(Carbon_Resource *r, int maximum);

// Calculate how much would be gained next tick
int carbon_resource_preview_tick(const Carbon_Resource *r);
```

**Implementation Notes:**
- Inline-able simple struct
- No allocations
- Can be used as component in ECS

---

### 2.4 Modifier Stack

**Files:**
- `include/carbon/modifier.h`
- `src/game/modifier.c`

**Dependencies:** None (standalone)

**API Design:**

```c
#include "carbon/modifier.h"

#define CARBON_MODIFIER_MAX 32

// Named modifier source (for UI display/debugging)
typedef struct Carbon_Modifier {
    char source[32];        // E.g., "policy_renewable", "tech_efficiency"
    float value;            // Multiplier delta: 0.1 = +10%, -0.05 = -5%
} Carbon_Modifier;

// Stack of modifiers
typedef struct Carbon_ModifierStack {
    Carbon_Modifier modifiers[CARBON_MODIFIER_MAX];
    int count;
} Carbon_ModifierStack;

// Initialize empty stack
void carbon_modifier_init(Carbon_ModifierStack *stack);

// Add/remove modifiers
bool carbon_modifier_add(Carbon_ModifierStack *stack, const char *source, float value);
bool carbon_modifier_remove(Carbon_ModifierStack *stack, const char *source);
bool carbon_modifier_has(const Carbon_ModifierStack *stack, const char *source);

// Update existing modifier value
bool carbon_modifier_set(Carbon_ModifierStack *stack, const char *source, float value);

// Calculate final value: base * (1 + mod1) * (1 + mod2) * ...
float carbon_modifier_apply(const Carbon_ModifierStack *stack, float base_value);

// Alternative: additive stacking: base * (1 + sum(modifiers))
float carbon_modifier_apply_additive(const Carbon_ModifierStack *stack, float base_value);

// Get total modifier for display: e.g., "+15%" or "-8%"
float carbon_modifier_total(const Carbon_ModifierStack *stack);

// Clear all modifiers
void carbon_modifier_clear(Carbon_ModifierStack *stack);

// Iterate for UI display
int carbon_modifier_count(const Carbon_ModifierStack *stack);
const Carbon_Modifier *carbon_modifier_get(const Carbon_ModifierStack *stack, int index);
```

**Implementation Notes:**
- Fixed-size array (no allocations)
- Source names for debugging/UI tooltips
- Both multiplicative and additive stacking modes

---

### 2.5 Unlock/Tech Tree

**Files:**
- `include/carbon/unlock.h`
- `src/game/unlock.c`

**Dependencies:** Data Configuration System (for loading definitions)

**API Design:**

```c
#include "carbon/unlock.h"

#define CARBON_UNLOCK_MAX_PREREQS 8

// Unlock node definition (loadable from config)
typedef struct Carbon_UnlockDef {
    char id[64];
    char name[128];
    char description[256];
    char category[32];

    char prerequisites[CARBON_UNLOCK_MAX_PREREQS][64];
    int prereq_count;

    int cost;               // Research points, gold, etc.

    // Game-specific data can follow in derived struct
} Carbon_UnlockDef;

// Unlock tree manager
typedef struct Carbon_UnlockTree Carbon_UnlockTree;

Carbon_UnlockTree *carbon_unlock_create(void);
void carbon_unlock_destroy(Carbon_UnlockTree *tree);

// Register unlock nodes
void carbon_unlock_register(Carbon_UnlockTree *tree, const Carbon_UnlockDef *def);

// Mark as completed
void carbon_unlock_complete(Carbon_UnlockTree *tree, const char *id);

// Query state
bool carbon_unlock_is_completed(const Carbon_UnlockTree *tree, const char *id);
bool carbon_unlock_has_prerequisites(const Carbon_UnlockTree *tree, const char *id);
bool carbon_unlock_can_research(const Carbon_UnlockTree *tree, const char *id);

// Get available (researchable) unlocks
// Returns array of pointers, caller provides buffer
int carbon_unlock_get_available(const Carbon_UnlockTree *tree,
                                 const Carbon_UnlockDef **out_defs,
                                 int max_count);

// Get unlocks by category
int carbon_unlock_get_by_category(const Carbon_UnlockTree *tree,
                                   const char *category,
                                   const Carbon_UnlockDef **out_defs,
                                   int max_count);

// Get all unlocks
int carbon_unlock_count(const Carbon_UnlockTree *tree);
const Carbon_UnlockDef *carbon_unlock_get_by_index(const Carbon_UnlockTree *tree, int index);
const Carbon_UnlockDef *carbon_unlock_find(const Carbon_UnlockTree *tree, const char *id);

// Reset all progress
void carbon_unlock_reset(Carbon_UnlockTree *tree);

// Progress tracking (for active research)
typedef struct Carbon_ResearchProgress {
    char current_id[64];
    int points_invested;
    int points_required;
} Carbon_ResearchProgress;

void carbon_unlock_start_research(Carbon_UnlockTree *tree, Carbon_ResearchProgress *progress, const char *id);
bool carbon_unlock_add_points(Carbon_UnlockTree *tree, Carbon_ResearchProgress *progress, int points);
float carbon_unlock_get_progress_percent(const Carbon_ResearchProgress *progress);
```

**Implementation Notes:**
- Stores completion state as bitset or hash set
- Prerequisites checked by looking up completed set
- Category for filtering in UI

---

### 2.6 Threshold Tracker

**Files:**
- `include/carbon/threshold.h`
- `src/game/threshold.c`

**Dependencies:** None (standalone)

**API Design:**

```c
#include "carbon/threshold.h"

#define CARBON_THRESHOLD_MAX 16

// Callback when threshold is crossed
typedef void (*Carbon_ThresholdCallback)(int threshold_id,
                                          float old_value,
                                          float new_value,
                                          bool crossed_above,  // true = went above, false = went below
                                          void *userdata);

// Single threshold
typedef struct Carbon_Threshold {
    float boundary;
    Carbon_ThresholdCallback callback;
    void *userdata;
    bool was_above;         // Previous state
} Carbon_Threshold;

// Tracker for multiple thresholds on one value
typedef struct Carbon_ThresholdTracker {
    Carbon_Threshold thresholds[CARBON_THRESHOLD_MAX];
    int count;
    float current_value;
} Carbon_ThresholdTracker;

// Initialize
void carbon_threshold_init(Carbon_ThresholdTracker *tracker, float initial_value);

// Add threshold
int carbon_threshold_add(Carbon_ThresholdTracker *tracker,
                          float boundary,
                          Carbon_ThresholdCallback callback,
                          void *userdata);

// Remove threshold by ID
void carbon_threshold_remove(Carbon_ThresholdTracker *tracker, int threshold_id);

// Update value and fire callbacks if thresholds crossed
void carbon_threshold_update(Carbon_ThresholdTracker *tracker, float new_value);

// Query
float carbon_threshold_get_value(const Carbon_ThresholdTracker *tracker);
int carbon_threshold_count(const Carbon_ThresholdTracker *tracker);
```

**Implementation Notes:**
- Lightweight, no allocations
- Fires callback on both directions (above→below, below→above)
- Returns threshold ID for later removal

---

## Phase 3: Library Dependencies

### 3.1 Add tomlc99 Library

**Files:**
- `lib/toml.h`
- `lib/toml.c`

**Source:** https://github.com/cktan/tomlc99 (MIT License)

**Steps:**
1. Download toml.h and toml.c from tomlc99 repo
2. Place in `lib/` directory
3. Add to Makefile SOURCES

---

## Phase 4: Integration

### 4.1 Update Build System

Add to Makefile:
```makefile
# New source files
SOURCES += src/data/config.c
SOURCES += src/data/event.c
SOURCES += src/data/save.c
SOURCES += src/game/turn.c
SOURCES += src/game/history.c
SOURCES += src/game/resource.c
SOURCES += src/game/modifier.c
SOURCES += src/game/unlock.c
SOURCES += src/game/threshold.c
SOURCES += lib/toml.c
```

### 4.2 Update Headers

Add includes to `include/carbon/carbon.h`:
```c
// Strategy game systems
#include "carbon/config.h"
#include "carbon/event.h"
#include "carbon/save.h"
#include "carbon/turn.h"
#include "carbon/history.h"
#include "carbon/resource.h"
#include "carbon/modifier.h"
#include "carbon/unlock.h"
#include "carbon/threshold.h"
```

### 4.3 Create Example

**Files:**
- `examples/strategy-sim/main.c`
- `examples/strategy-sim/data/policies.toml`
- `examples/strategy-sim/data/events.toml`
- `examples/strategy-sim/data/techs.toml`

Demonstrates:
- Loading data from TOML
- Turn-based game loop with phases
- Event triggers and choices
- Tech tree progression
- Resource management
- History tracking for graphs
- Save/load game state

---

## Phase 5: Documentation

### 5.1 Update CLAUDE.md

Add sections documenting:
- Data Configuration System
- Event System
- Save/Load System
- Turn Manager
- History Tracking
- Resource System
- Modifier Stack
- Unlock Tree
- Threshold Tracker

### 5.2 Add TOML Data Schemas

**Files:**
- `assets/schemas/policy.schema.json`
- `assets/schemas/event.schema.json`
- `assets/schemas/tech.schema.json`

---

## Implementation Order

1. **Week 1: Core Infrastructure**
   - [ ] Add tomlc99 library
   - [ ] Implement config.c (data loading)
   - [ ] Implement save.c (serialization)
   - [ ] Basic tests

2. **Week 2: Event System**
   - [ ] Implement event.c (triggers, choices)
   - [ ] Expression parser for triggers
   - [ ] Integration with config loader

3. **Week 3: Turn-Based Support**
   - [ ] Implement turn.c (phase manager)
   - [ ] Implement resource.c
   - [ ] Implement modifier.c
   - [ ] Implement threshold.c

4. **Week 4: Progression Systems**
   - [ ] Implement unlock.c (tech trees)
   - [ ] Implement history.c (metrics tracking)
   - [ ] Integration tests

5. **Week 5: Example & Docs**
   - [ ] Create strategy-sim example
   - [ ] Sample TOML data files
   - [ ] Update CLAUDE.md
   - [ ] Final testing

---

## Testing Strategy

Each system should have:
1. **Unit tests** in `tests/` directory
2. **Integration test** in example
3. **Edge cases**: empty data, missing files, max capacity

Key test scenarios:
- Load malformed TOML → graceful error
- Trigger expression edge cases
- Save/load round-trip
- Circular dependencies in tech tree
- Modifier overflow/underflow

---

## Notes

- All systems use C11
- No external dependencies except tomlc99 (for TOML)
- Existing JSON loader in `src/game/data/loader.c` remains for JSON data
- New TOML loader is for configuration data (more human-friendly)
- All new code follows existing Carbon style
- Systems are optional - games can use only what they need
