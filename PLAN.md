# Carbon Engine Feature Roadmap

Features extracted from StellarThroneCPP and Ecosocialism projects that would enhance the Carbon game engine. All features are engine-agnostic and designed for C11.

---

## Overview

| Phase | System | Priority | New Files | Status | Description |
|-------|--------|----------|-----------|--------|-------------|
| 1 | Event Dispatcher | High | event.h/c | **DONE** | Pub-sub messaging for decoupled systems |
| 1 | Save/Load | High | save.h/c | **DONE** (TOML) | TOML-based serialization (already existed) |
| 1 | Validation | Medium | validate.h | **DONE** | Macro-based validation utilities |
| 1 | Container Utils | Medium | containers.h/c | **DONE** | Generic algorithms and random |
| 2 | Turn/Phase System | High | turn.h/c | **DONE** | Multi-phase turn orchestration (already existed) |
| 2 | Resource Economy | Medium | resource.h/c | **DONE** | Multi-resource pools (already existed) |
| 2 | Technology Tree | Medium | tech.h/c | **DONE** | Research with prerequisites |
| 2 | Victory System | Low | victory.h/c | **DONE** | Multi-win-condition tracking |
| 3 | AI Personality | Medium | ai.h/c | **DONE** | Weighted decision making |
| 4 | View Model | Medium | viewmodel.h/c | **DONE** | UI data binding |
| 4 | Theme Enhancement | Low | (existing) | **DONE** | Semantic color system |
| 5 | 3D Camera | Low | camera3d.h/c | **DONE** | Orbital camera for 3D views |
| - | History Tracking | Medium | history.h/c | **DONE** | Metrics snapshots (already existed) |
| - | Modifier Stack | Medium | modifier.h/c | **DONE** | Composable effect multipliers (already existed) |
| - | Threshold Tracker | Low | threshold.h/c | **DONE** | Value boundary callbacks (already existed) |

---

## Phase 1: Core Infrastructure ✅ COMPLETE

### 1.1 Event/Messaging System
**Priority:** High
**Location:** `src/core/event.c`, `include/carbon/event.h`

A publish-subscribe event dispatcher for decoupled system communication.

**Features:**
- Type-safe event data using tagged unions
- Per-event-type subscription with listener IDs
- `subscribe()`, `unsubscribe()`, `emit()` API
- Factory functions for common events
- Support for 30+ event types
- Deferred emission queue for events during callbacks

**API Design:**
```c
// Event types
typedef enum {
    CARBON_EVENT_NONE = 0,
    // Engine events
    CARBON_EVENT_WINDOW_RESIZE,
    CARBON_EVENT_WINDOW_FOCUS,
    // Game events
    CARBON_EVENT_TURN_STARTED,
    CARBON_EVENT_TURN_ENDED,
    CARBON_EVENT_PHASE_STARTED,
    CARBON_EVENT_PHASE_ENDED,
    CARBON_EVENT_ENTITY_CREATED,
    CARBON_EVENT_ENTITY_DESTROYED,
    CARBON_EVENT_SELECTION_CHANGED,
    CARBON_EVENT_RESOURCE_CHANGED,
    CARBON_EVENT_TECH_RESEARCHED,
    CARBON_EVENT_VICTORY_ACHIEVED,
    // ... extensible
    CARBON_EVENT_CUSTOM = 1000,  // User-defined events start here
} Carbon_EventType;

// Event data (tagged union)
typedef struct Carbon_Event {
    Carbon_EventType type;
    uint32_t timestamp;
    union {
        struct { int width, height; } window_resize;
        struct { uint32_t turn; } turn;
        struct { Carbon_TurnPhase phase; } phase;
        struct { ecs_entity_t entity; } entity;
        struct { int32_t id; float x, y; } selection;
        struct { int resource_type; int old_value, new_value; } resource;
        struct { uint32_t tech_id; } tech;
        struct { int victory_type; int winner_id; } victory;
        struct { void *data; size_t size; } custom;
    };
} Carbon_Event;

// Dispatcher
typedef struct Carbon_EventDispatcher Carbon_EventDispatcher;
typedef void (*Carbon_EventCallback)(const Carbon_Event *event, void *userdata);
typedef uint32_t Carbon_ListenerID;

Carbon_EventDispatcher *carbon_event_dispatcher_create(void);
void carbon_event_dispatcher_destroy(Carbon_EventDispatcher *d);

Carbon_ListenerID carbon_event_subscribe(Carbon_EventDispatcher *d,
                                          Carbon_EventType type,
                                          Carbon_EventCallback callback,
                                          void *userdata);
Carbon_ListenerID carbon_event_subscribe_all(Carbon_EventDispatcher *d,
                                              Carbon_EventCallback callback,
                                              void *userdata);
void carbon_event_unsubscribe(Carbon_EventDispatcher *d, Carbon_ListenerID id);
void carbon_event_emit(Carbon_EventDispatcher *d, const Carbon_Event *event);

// Deferred emission (for events during callbacks)
void carbon_event_emit_deferred(Carbon_EventDispatcher *d, const Carbon_Event *event);
void carbon_event_flush_deferred(Carbon_EventDispatcher *d);

// Convenience emitters
void carbon_event_emit_turn_started(Carbon_EventDispatcher *d, uint32_t turn);
void carbon_event_emit_turn_ended(Carbon_EventDispatcher *d, uint32_t turn);
void carbon_event_emit_entity_created(Carbon_EventDispatcher *d, ecs_entity_t entity);
void carbon_event_emit_resource_changed(Carbon_EventDispatcher *d, int type, int old_val, int new_val);
```

**Implementation Notes:**
- Use dynamic array for listeners per event type
- Listener IDs are monotonically increasing (never reused)
- Thread-safe option via mutex (optional compile flag)
- Deferred emission queue prevents issues during callback iteration

---

### 1.2 Save/Load System
**Priority:** High
**Location:** `src/core/save.c`, `include/carbon/save.h`

Binary serialization with version checking and data integrity validation.

**Features:**
- Magic number + version header
- CRC32 checksum validation
- Binary reader/writer with type-safe methods
- Directory management for save files
- Human-readable error codes

**API Design:**
```c
// Save header (written automatically)
typedef struct Carbon_SaveHeader {
    uint32_t magic;           // 'CARB' = 0x42524143
    uint16_t version_major;
    uint16_t version_minor;
    uint64_t timestamp;
    uint32_t checksum;        // CRC32 of data after header
    uint32_t data_size;
} Carbon_SaveHeader;

// Result codes
typedef enum {
    CARBON_SAVE_OK = 0,
    CARBON_SAVE_ERROR_FILE,
    CARBON_SAVE_ERROR_WRITE,
    CARBON_SAVE_ERROR_CHECKSUM,
    CARBON_SAVE_ERROR_VERSION,
    CARBON_SAVE_ERROR_CORRUPT,
} Carbon_SaveResult;

// Binary writer
typedef struct Carbon_BinaryWriter Carbon_BinaryWriter;

Carbon_BinaryWriter *carbon_binary_writer_create(const char *path);
void carbon_binary_writer_destroy(Carbon_BinaryWriter *w);

void carbon_write_u8(Carbon_BinaryWriter *w, uint8_t value);
void carbon_write_u16(Carbon_BinaryWriter *w, uint16_t value);
void carbon_write_u32(Carbon_BinaryWriter *w, uint32_t value);
void carbon_write_u64(Carbon_BinaryWriter *w, uint64_t value);
void carbon_write_i32(Carbon_BinaryWriter *w, int32_t value);
void carbon_write_f32(Carbon_BinaryWriter *w, float value);
void carbon_write_f64(Carbon_BinaryWriter *w, double value);
void carbon_write_bool(Carbon_BinaryWriter *w, bool value);
void carbon_write_string(Carbon_BinaryWriter *w, const char *str);
void carbon_write_bytes(Carbon_BinaryWriter *w, const void *data, size_t size);

// Write array with count prefix
void carbon_write_array_u32(Carbon_BinaryWriter *w, const uint32_t *arr, size_t count);
void carbon_write_array_f32(Carbon_BinaryWriter *w, const float *arr, size_t count);

Carbon_SaveResult carbon_binary_writer_finalize(Carbon_BinaryWriter *w,
                                                 uint16_t version_major,
                                                 uint16_t version_minor);

// Binary reader
typedef struct Carbon_BinaryReader Carbon_BinaryReader;

Carbon_BinaryReader *carbon_binary_reader_create(const char *path);
void carbon_binary_reader_destroy(Carbon_BinaryReader *r);

Carbon_SaveResult carbon_binary_reader_validate(Carbon_BinaryReader *r,
                                                 uint16_t expected_major,
                                                 uint16_t expected_minor);
const Carbon_SaveHeader *carbon_binary_reader_get_header(Carbon_BinaryReader *r);

uint8_t carbon_read_u8(Carbon_BinaryReader *r);
uint16_t carbon_read_u16(Carbon_BinaryReader *r);
uint32_t carbon_read_u32(Carbon_BinaryReader *r);
uint64_t carbon_read_u64(Carbon_BinaryReader *r);
int32_t carbon_read_i32(Carbon_BinaryReader *r);
float carbon_read_f32(Carbon_BinaryReader *r);
double carbon_read_f64(Carbon_BinaryReader *r);
bool carbon_read_bool(Carbon_BinaryReader *r);
char *carbon_read_string(Carbon_BinaryReader *r);  // Caller must free
size_t carbon_read_bytes(Carbon_BinaryReader *r, void *buffer, size_t max_size);

// Array reading
size_t carbon_read_array_u32(Carbon_BinaryReader *r, uint32_t *out, size_t max_count);
size_t carbon_read_array_f32(Carbon_BinaryReader *r, float *out, size_t max_count);

bool carbon_binary_reader_has_error(Carbon_BinaryReader *r);
const char *carbon_binary_reader_error(Carbon_BinaryReader *r);
bool carbon_binary_reader_eof(Carbon_BinaryReader *r);

// Save directory management
const char *carbon_save_get_directory(void);  // Platform-specific
bool carbon_save_set_directory(const char *path);
bool carbon_save_list(const char *extension, char ***out_files, int *out_count);
void carbon_save_free_list(char **files, int count);
bool carbon_save_delete(const char *filename);
bool carbon_save_exists(const char *filename);
```

**Implementation Notes:**
- Little-endian format (convert on big-endian platforms)
- String format: uint32_t length + chars (no null terminator in file)
- CRC32 uses standard polynomial 0xEDB88320
- Save directory: `~/Library/Application Support/<app>/` (macOS), `~/.local/share/<app>/` (Linux), `%APPDATA%/<app>/` (Windows)

---

### 1.3 Validation Framework
**Priority:** Medium
**Location:** `include/carbon/validate.h` (header-only)

Macro-based validation utilities with logging integration.

**Features:**
- Pointer and ID validation with early return
- Range checking
- Automatic context from `__func__`
- Integration with Carbon's error system

**API Design:**
```c
// Validation macros with early return
#define CARBON_VALIDATE_PTR(ptr) \
    do { \
        if (!(ptr)) { \
            carbon_set_error("%s: null pointer: %s", __func__, #ptr); \
            return; \
        } \
    } while(0)

#define CARBON_VALIDATE_PTR_RET(ptr, ret) \
    do { \
        if (!(ptr)) { \
            carbon_set_error("%s: null pointer: %s", __func__, #ptr); \
            return (ret); \
        } \
    } while(0)

#define CARBON_VALIDATE_ID(id, invalid_value) \
    do { \
        if ((id) == (invalid_value)) { \
            carbon_set_error("%s: invalid ID: %s", __func__, #id); \
            return; \
        } \
    } while(0)

#define CARBON_VALIDATE_ID_RET(id, invalid_value, ret) \
    do { \
        if ((id) == (invalid_value)) { \
            carbon_set_error("%s: invalid ID: %s", __func__, #id); \
            return (ret); \
        } \
    } while(0)

#define CARBON_VALIDATE_RANGE(val, min, max) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            carbon_set_error("%s: %s out of range [%d, %d]: %d", \
                            __func__, #val, (int)(min), (int)(max), (int)(val)); \
            return; \
        } \
    } while(0)

#define CARBON_VALIDATE_RANGE_RET(val, min, max, ret) \
    do { \
        if ((val) < (min) || (val) > (max)) { \
            carbon_set_error("%s: %s out of range [%d, %d]: %d", \
                            __func__, #val, (int)(min), (int)(max), (int)(val)); \
            return (ret); \
        } \
    } while(0)

// Entity validation (for ECS)
#define CARBON_VALIDATE_ENTITY(world, entity) \
    do { \
        if ((entity) == 0 || !ecs_is_alive(world, entity)) { \
            carbon_set_error("%s: invalid entity: %s", __func__, #entity); \
            return false; \
        } \
    } while(0)

// Debug-only assertions (compiled out in release)
#ifdef CARBON_DEBUG
#define CARBON_ASSERT(cond) \
    do { \
        if (!(cond)) { \
            SDL_Log("ASSERT FAILED: %s at %s:%d", #cond, __FILE__, __LINE__); \
            abort(); \
        } \
    } while(0)
#define CARBON_ASSERT_MSG(cond, msg) \
    do { \
        if (!(cond)) { \
            SDL_Log("ASSERT FAILED: %s - %s at %s:%d", #cond, msg, __FILE__, __LINE__); \
            abort(); \
        } \
    } while(0)
#else
#define CARBON_ASSERT(cond) ((void)0)
#define CARBON_ASSERT_MSG(cond, msg) ((void)0)
#endif
```

---

### 1.4 Container Utilities
**Priority:** Medium
**Location:** `include/carbon/containers.h` (mostly header-only), `src/core/containers.c`

Generic container algorithms to reduce code duplication.

**Features:**
- Dynamic arrays with type safety via macros
- Random selection utilities
- Weighted random choice
- Shuffle algorithm

**API Design:**
```c
// Dynamic array (type-generic via macros)
#define Carbon_Array(T) struct { T *data; size_t count; size_t capacity; }

#define carbon_array_init(arr) \
    do { (arr)->data = NULL; (arr)->count = 0; (arr)->capacity = 0; } while(0)

#define carbon_array_free(arr) \
    do { free((arr)->data); carbon_array_init(arr); } while(0)

#define carbon_array_push(arr, item) \
    do { \
        if ((arr)->count >= (arr)->capacity) { \
            (arr)->capacity = (arr)->capacity ? (arr)->capacity * 2 : 8; \
            (arr)->data = realloc((arr)->data, (arr)->capacity * sizeof(*(arr)->data)); \
        } \
        (arr)->data[(arr)->count++] = (item); \
    } while(0)

#define carbon_array_pop(arr) \
    ((arr)->count > 0 ? (arr)->data[--(arr)->count] : (arr)->data[0])

#define carbon_array_clear(arr) \
    do { (arr)->count = 0; } while(0)

#define carbon_array_remove(arr, index) \
    do { \
        if ((size_t)(index) < (arr)->count - 1) { \
            memmove(&(arr)->data[index], &(arr)->data[(index) + 1], \
                    ((arr)->count - (index) - 1) * sizeof(*(arr)->data)); \
        } \
        (arr)->count--; \
    } while(0)

#define carbon_array_remove_swap(arr, index) \
    do { \
        if ((size_t)(index) < (arr)->count - 1) { \
            (arr)->data[index] = (arr)->data[(arr)->count - 1]; \
        } \
        (arr)->count--; \
    } while(0)

#define carbon_array_reserve(arr, cap) \
    do { \
        if ((cap) > (arr)->capacity) { \
            (arr)->capacity = (cap); \
            (arr)->data = realloc((arr)->data, (arr)->capacity * sizeof(*(arr)->data)); \
        } \
    } while(0)

// Random utilities (uses SDL random internally)
int carbon_random_int(int min, int max);           // [min, max] inclusive
float carbon_random_float(float min, float max);   // [min, max)
bool carbon_random_bool(void);
size_t carbon_random_index(size_t count);          // [0, count)
void carbon_random_seed(uint64_t seed);

// Random choice from array
#define carbon_random_choice(arr, count) \
    ((count) > 0 ? (arr)[carbon_random_index(count)] : (arr)[0])

// Weighted random choice
typedef struct {
    size_t index;
    float weight;
} Carbon_WeightedItem;

size_t carbon_weighted_random(const Carbon_WeightedItem *items, size_t count);

// Shuffle array in place (Fisher-Yates)
void carbon_shuffle(void *array, size_t count, size_t element_size);

#define carbon_shuffle_array(arr, count) \
    carbon_shuffle((arr), (count), sizeof(*(arr)))
```

---

## Phase 2: Strategy Game Systems

### 2.1 Turn/Phase System
**Priority:** High (for strategy games)
**Location:** `src/game/turn.c`, `include/carbon/turn.h`

Manages turn advancement through ordered phases with callbacks.

**Features:**
- Configurable phase list
- Per-phase callbacks
- Turn counter
- Integration with event system

**API Design:**
```c
// Phase definition
typedef enum {
    CARBON_PHASE_NONE = 0,
    CARBON_PHASE_PRE_TURN,
    CARBON_PHASE_ECONOMY,
    CARBON_PHASE_PRODUCTION,
    CARBON_PHASE_AI,
    CARBON_PHASE_MOVEMENT,
    CARBON_PHASE_COMBAT,
    CARBON_PHASE_POST_TURN,
    CARBON_PHASE_COUNT
} Carbon_TurnPhase;

typedef void (*Carbon_PhaseCallback)(Carbon_TurnPhase phase, uint32_t turn, void *userdata);

typedef struct Carbon_TurnSystem Carbon_TurnSystem;

Carbon_TurnSystem *carbon_turn_system_create(Carbon_EventDispatcher *events);
void carbon_turn_system_destroy(Carbon_TurnSystem *ts);

// Configure phases (order matters)
void carbon_turn_add_phase(Carbon_TurnSystem *ts, Carbon_TurnPhase phase,
                           Carbon_PhaseCallback callback, void *userdata);
void carbon_turn_remove_phase(Carbon_TurnSystem *ts, Carbon_TurnPhase phase);
void carbon_turn_set_phase_order(Carbon_TurnSystem *ts, const Carbon_TurnPhase *phases, int count);

// Turn control
void carbon_turn_advance(Carbon_TurnSystem *ts);  // Process all phases
void carbon_turn_process_phase(Carbon_TurnSystem *ts, Carbon_TurnPhase phase);  // Single phase
void carbon_turn_skip_to_phase(Carbon_TurnSystem *ts, Carbon_TurnPhase phase);

// Queries
uint32_t carbon_turn_get_current(Carbon_TurnSystem *ts);
Carbon_TurnPhase carbon_turn_get_phase(Carbon_TurnSystem *ts);
bool carbon_turn_is_processing(Carbon_TurnSystem *ts);
const char *carbon_turn_phase_name(Carbon_TurnPhase phase);

// Events emitted:
// - CARBON_EVENT_TURN_STARTED (at start of advance)
// - CARBON_EVENT_PHASE_STARTED (before each phase callback)
// - CARBON_EVENT_PHASE_ENDED (after each phase callback)
// - CARBON_EVENT_TURN_ENDED (after all phases complete)
```

**Implementation Notes:**
- Phases are processed in registration order
- Callbacks can be async (return immediately, signal completion later)
- For real-time hybrid games, phases can be time-limited

---

### 2.2 Resource Economy System
**Priority:** Medium
**Location:** `src/game/economy.c`, `include/carbon/economy.h`

Multi-resource economy with income tracking.

**Features:**
- Configurable resource types
- Per-entity resource pools
- Income/expense calculation
- Resource events

**API Design:**
```c
// Resource types (customizable)
#define CARBON_MAX_RESOURCE_TYPES 8

typedef struct Carbon_ResourceDef {
    const char *name;
    int32_t starting_value;
    int32_t min_value;
    int32_t max_value;  // 0 = unlimited
} Carbon_ResourceDef;

typedef struct Carbon_Resources {
    int32_t values[CARBON_MAX_RESOURCE_TYPES];
    int32_t income[CARBON_MAX_RESOURCE_TYPES];   // Per-turn income
    int32_t expense[CARBON_MAX_RESOURCE_TYPES];  // Per-turn expenses
} Carbon_Resources;

typedef struct Carbon_EconomySystem Carbon_EconomySystem;

Carbon_EconomySystem *carbon_economy_create(Carbon_EventDispatcher *events);
void carbon_economy_destroy(Carbon_EconomySystem *es);

// Define resource types
int carbon_economy_define_resource(Carbon_EconomySystem *es, const Carbon_ResourceDef *def);
const char *carbon_economy_get_resource_name(Carbon_EconomySystem *es, int type);
int carbon_economy_get_resource_count(Carbon_EconomySystem *es);

// Resource pool management
Carbon_Resources *carbon_economy_create_pool(Carbon_EconomySystem *es);
void carbon_economy_destroy_pool(Carbon_EconomySystem *es, Carbon_Resources *pool);
void carbon_economy_init_pool(Carbon_EconomySystem *es, Carbon_Resources *pool);

// Resource operations
void carbon_resource_add(Carbon_Resources *r, int type, int32_t amount);
void carbon_resource_subtract(Carbon_Resources *r, int type, int32_t amount);
bool carbon_resource_can_afford(const Carbon_Resources *r, int type, int32_t amount);
bool carbon_resource_spend(Carbon_Resources *r, int type, int32_t amount);  // Returns false if insufficient
int32_t carbon_resource_get(const Carbon_Resources *r, int type);
void carbon_resource_set(Carbon_Resources *r, int type, int32_t value);

// Income/expense
void carbon_resource_set_income(Carbon_Resources *r, int type, int32_t amount);
void carbon_resource_add_income(Carbon_Resources *r, int type, int32_t delta);
void carbon_resource_set_expense(Carbon_Resources *r, int type, int32_t amount);
int32_t carbon_resource_get_net_income(const Carbon_Resources *r, int type);

// Apply income (call during economy phase)
void carbon_economy_process_income(Carbon_EconomySystem *es, Carbon_Resources *pool);

// Bulk operations
typedef struct {
    int type;
    int32_t amount;
} Carbon_ResourceCost;

bool carbon_resource_can_afford_multi(const Carbon_Resources *r,
                                       const Carbon_ResourceCost *costs, int count);
bool carbon_resource_spend_multi(Carbon_Resources *r,
                                  const Carbon_ResourceCost *costs, int count);
void carbon_resource_add_multi(Carbon_Resources *r,
                                const Carbon_ResourceCost *amounts, int count);
```

---

### 2.3 Technology Tree System ✅ COMPLETE
**Priority:** Medium
**Location:** `src/strategy/tech.c`, `include/carbon/tech.h`

Research system with prerequisites and effects.

**Features:**
- Branch-based tech tree
- Prerequisite chains
- Effect application
- Per-faction progress tracking

**API Design:**
```c
// Technology effect types
typedef enum {
    CARBON_TECH_EFFECT_NONE = 0,
    CARBON_TECH_EFFECT_RESOURCE_BONUS,
    CARBON_TECH_EFFECT_PRODUCTION_BONUS,
    CARBON_TECH_EFFECT_UNLOCK_UNIT,
    CARBON_TECH_EFFECT_UNLOCK_BUILDING,
    CARBON_TECH_EFFECT_STAT_MODIFIER,
    CARBON_TECH_EFFECT_CUSTOM,
} Carbon_TechEffectType;

typedef struct Carbon_TechEffect {
    Carbon_TechEffectType type;
    int target_id;      // Unit/building/resource type
    float value;        // Bonus amount or multiplier
} Carbon_TechEffect;

#define CARBON_TECH_MAX_PREREQS 4
#define CARBON_TECH_MAX_EFFECTS 4

typedef struct Carbon_Technology {
    uint32_t id;
    const char *name;
    const char *description;
    int branch;                           // Category/branch ID
    int32_t cost;                         // Research cost
    uint32_t prerequisites[CARBON_TECH_MAX_PREREQS];  // Tech IDs (0 = none)
    int prereq_count;
    Carbon_TechEffect effects[CARBON_TECH_MAX_EFFECTS];
    int effect_count;
} Carbon_Technology;

// Per-faction tech state (supports up to 64 techs via bitmask, or use hash set for more)
typedef struct Carbon_TechState {
    uint64_t researched_mask;             // Bitmask of completed techs
    uint32_t current_research;            // Tech ID being researched (0 = none)
    int32_t research_progress;            // Points toward current tech
} Carbon_TechState;

typedef struct Carbon_TechSystem Carbon_TechSystem;

Carbon_TechSystem *carbon_tech_system_create(Carbon_EventDispatcher *events);
void carbon_tech_system_destroy(Carbon_TechSystem *ts);

// Define technologies
uint32_t carbon_tech_define(Carbon_TechSystem *ts, const Carbon_Technology *tech);
const Carbon_Technology *carbon_tech_get(Carbon_TechSystem *ts, uint32_t id);
int carbon_tech_count(Carbon_TechSystem *ts);

// Tech state management
void carbon_tech_state_init(Carbon_TechState *state);

// Research operations
bool carbon_tech_is_researched(const Carbon_TechState *state, uint32_t tech_id);
bool carbon_tech_can_research(Carbon_TechSystem *ts, const Carbon_TechState *state, uint32_t tech_id);
void carbon_tech_start_research(Carbon_TechState *state, uint32_t tech_id);
bool carbon_tech_add_progress(Carbon_TechSystem *ts, Carbon_TechState *state, int32_t points);  // Returns true if completed
void carbon_tech_complete(Carbon_TechSystem *ts, Carbon_TechState *state, uint32_t tech_id);
void carbon_tech_cancel_research(Carbon_TechState *state);

// Queries
int carbon_tech_get_available(Carbon_TechSystem *ts, const Carbon_TechState *state,
                               uint32_t *out_ids, int max_count);
int carbon_tech_get_by_branch(Carbon_TechSystem *ts, int branch,
                               uint32_t *out_ids, int max_count);
float carbon_tech_get_progress_percent(Carbon_TechSystem *ts, const Carbon_TechState *state);
int32_t carbon_tech_get_remaining_cost(Carbon_TechSystem *ts, const Carbon_TechState *state);
```

---

### 2.4 Victory Condition System
**Priority:** Low
**Location:** `src/game/victory.c`, `include/carbon/victory.h`

Multi-win-condition tracking.

**Features:**
- Multiple victory types
- Progress tracking
- Configurable thresholds

**API Design:**
```c
typedef enum {
    CARBON_VICTORY_NONE = 0,
    CARBON_VICTORY_DOMINATION,    // Control X% of territory
    CARBON_VICTORY_ELIMINATION,   // Defeat all opponents
    CARBON_VICTORY_TECHNOLOGY,    // Research all techs
    CARBON_VICTORY_ECONOMIC,      // Accumulate X resources
    CARBON_VICTORY_SCORE,         // Highest score after N turns
    CARBON_VICTORY_CUSTOM,
    CARBON_VICTORY_COUNT
} Carbon_VictoryType;

typedef struct Carbon_VictoryCondition {
    Carbon_VictoryType type;
    bool enabled;
    float threshold;              // e.g., 0.75 for 75% domination
    int32_t target_value;         // e.g., 100000 for economic
    int32_t target_turn;          // For score victory
} Carbon_VictoryCondition;

typedef struct Carbon_VictoryProgress {
    float progress[CARBON_VICTORY_COUNT];  // 0.0 to 1.0 per condition type
} Carbon_VictoryProgress;

typedef struct Carbon_VictoryState {
    bool achieved;
    Carbon_VictoryType type;
    int winner_id;                // Faction/player ID
    char message[256];
} Carbon_VictoryState;

// Custom victory checker callback
typedef bool (*Carbon_VictoryChecker)(int faction_id, float *out_progress, void *userdata);

typedef struct Carbon_VictorySystem Carbon_VictorySystem;

Carbon_VictorySystem *carbon_victory_system_create(Carbon_EventDispatcher *events);
void carbon_victory_system_destroy(Carbon_VictorySystem *vs);

void carbon_victory_set_condition(Carbon_VictorySystem *vs, const Carbon_VictoryCondition *cond);
void carbon_victory_disable_condition(Carbon_VictorySystem *vs, Carbon_VictoryType type);
void carbon_victory_set_custom_checker(Carbon_VictorySystem *vs, Carbon_VictoryChecker checker, void *userdata);

void carbon_victory_check(Carbon_VictorySystem *vs);  // Checks all conditions, emits event if won
void carbon_victory_declare(Carbon_VictorySystem *vs, Carbon_VictoryType type, int winner_id, const char *message);
const Carbon_VictoryState *carbon_victory_get_state(Carbon_VictorySystem *vs);
bool carbon_victory_is_achieved(Carbon_VictorySystem *vs);

// Progress queries (for UI)
void carbon_victory_get_progress(Carbon_VictorySystem *vs, int faction_id, Carbon_VictoryProgress *out);
```

---

## Phase 3: AI Framework

### 3.1 AI Personality System
**Priority:** Medium
**Location:** `src/ai/personality.c`, `include/carbon/ai.h`

Personality-driven AI decision making.

**Features:**
- Personality types with weighted behaviors
- Per-faction AI state
- Threat assessment
- Goal management

**API Design:**
```c
// AI personality types
typedef enum {
    CARBON_AI_BALANCED = 0,
    CARBON_AI_AGGRESSIVE,
    CARBON_AI_DEFENSIVE,
    CARBON_AI_ECONOMIC,
    CARBON_AI_EXPANSIONIST,
    CARBON_AI_TECHNOLOGIST,
    CARBON_AI_COUNT
} Carbon_AIPersonality;

// Behavior weights (normalized, sum to 1.0)
typedef struct Carbon_AIWeights {
    float aggression;      // Attack enemies
    float defense;         // Protect own territory
    float expansion;       // Claim new territory
    float economy;         // Build economy
    float technology;      // Research
    float diplomacy;       // Make alliances
} Carbon_AIWeights;

// Per-faction AI state
typedef struct Carbon_AIState {
    Carbon_AIPersonality personality;
    Carbon_AIWeights weights;
    uint32_t primary_target;      // Enemy faction to focus on
    uint32_t ally_target;         // Faction to ally with
    float threat_level;           // 0.0 to 1.0
    int cooldowns[8];             // Action cooldowns
} Carbon_AIState;

// AI decision output
typedef enum {
    CARBON_AI_ACTION_NONE = 0,
    CARBON_AI_ACTION_BUILD,
    CARBON_AI_ACTION_ATTACK,
    CARBON_AI_ACTION_DEFEND,
    CARBON_AI_ACTION_EXPAND,
    CARBON_AI_ACTION_RESEARCH,
    CARBON_AI_ACTION_DIPLOMACY,
    CARBON_AI_ACTION_COUNT
} Carbon_AIActionType;

typedef struct Carbon_AIAction {
    Carbon_AIActionType type;
    uint32_t target_id;
    int32_t priority;
    void *data;                   // Action-specific data
} Carbon_AIAction;

#define CARBON_AI_MAX_ACTIONS 16

typedef struct Carbon_AIDecision {
    Carbon_AIAction actions[CARBON_AI_MAX_ACTIONS];
    int action_count;
} Carbon_AIDecision;

typedef struct Carbon_AISystem Carbon_AISystem;

Carbon_AISystem *carbon_ai_system_create(void);
void carbon_ai_system_destroy(Carbon_AISystem *ai);

// Configure personality weights
void carbon_ai_get_default_weights(Carbon_AIPersonality personality, Carbon_AIWeights *out);
void carbon_ai_set_weights(Carbon_AIState *state, const Carbon_AIWeights *weights);
void carbon_ai_state_init(Carbon_AIState *state, Carbon_AIPersonality personality);

// Custom evaluator function receives game state and returns scored actions
typedef void (*Carbon_AIEvaluator)(Carbon_AIState *state, void *game_context,
                                    Carbon_AIAction *out_actions, int *out_count, int max_actions);

void carbon_ai_register_evaluator(Carbon_AISystem *ai, Carbon_AIActionType type,
                                   Carbon_AIEvaluator evaluator);

// Decision making (call during AI phase)
void carbon_ai_process_turn(Carbon_AISystem *ai, Carbon_AIState *state, void *game_context,
                            Carbon_AIDecision *out_decision);

// Utility functions
float carbon_ai_score_action(Carbon_AIState *state, Carbon_AIActionType type, float base_score);
void carbon_ai_update_cooldowns(Carbon_AIState *state);
```

---

## Phase 4: UI Enhancements

### 4.1 View Model / Data Binding
**Priority:** Medium
**Location:** `src/ui/viewmodel.c`, `include/carbon/viewmodel.h`

Separates game state from UI presentation.

**Features:**
- Observable values with change detection
- Event-driven UI updates
- Type-safe value access

**API Design:**
```c
// Observable value types
typedef enum {
    CARBON_VM_INT,
    CARBON_VM_FLOAT,
    CARBON_VM_BOOL,
    CARBON_VM_STRING,
    CARBON_VM_POINTER,
} Carbon_ViewModelType;

typedef struct Carbon_ViewModel Carbon_ViewModel;
typedef void (*Carbon_VMChangeCallback)(Carbon_ViewModel *vm, uint32_t observable_id, void *userdata);

Carbon_ViewModel *carbon_viewmodel_create(Carbon_EventDispatcher *events);
void carbon_viewmodel_destroy(Carbon_ViewModel *vm);

// Define observables
uint32_t carbon_vm_define_int(Carbon_ViewModel *vm, const char *name, int32_t initial);
uint32_t carbon_vm_define_float(Carbon_ViewModel *vm, const char *name, float initial);
uint32_t carbon_vm_define_bool(Carbon_ViewModel *vm, const char *name, bool initial);
uint32_t carbon_vm_define_string(Carbon_ViewModel *vm, const char *name, const char *initial);
uint32_t carbon_vm_define_ptr(Carbon_ViewModel *vm, const char *name, void *initial);

// Set values (emits change event if different)
void carbon_vm_set_int(Carbon_ViewModel *vm, uint32_t id, int32_t value);
void carbon_vm_set_float(Carbon_ViewModel *vm, uint32_t id, float value);
void carbon_vm_set_bool(Carbon_ViewModel *vm, uint32_t id, bool value);
void carbon_vm_set_string(Carbon_ViewModel *vm, uint32_t id, const char *value);
void carbon_vm_set_ptr(Carbon_ViewModel *vm, uint32_t id, void *value);

// Get values
int32_t carbon_vm_get_int(Carbon_ViewModel *vm, uint32_t id);
float carbon_vm_get_float(Carbon_ViewModel *vm, uint32_t id);
bool carbon_vm_get_bool(Carbon_ViewModel *vm, uint32_t id);
const char *carbon_vm_get_string(Carbon_ViewModel *vm, uint32_t id);
void *carbon_vm_get_ptr(Carbon_ViewModel *vm, uint32_t id);

// Lookup by name
uint32_t carbon_vm_find(Carbon_ViewModel *vm, const char *name);

// Change notification
void carbon_vm_subscribe(Carbon_ViewModel *vm, uint32_t id,
                          Carbon_VMChangeCallback callback, void *userdata);
void carbon_vm_unsubscribe(Carbon_ViewModel *vm, uint32_t id, Carbon_VMChangeCallback callback);
void carbon_vm_subscribe_all(Carbon_ViewModel *vm, Carbon_VMChangeCallback callback, void *userdata);

// Batch updates (defer change events until commit)
void carbon_vm_begin_update(Carbon_ViewModel *vm);
void carbon_vm_commit_update(Carbon_ViewModel *vm);

// Force notification even if value unchanged
void carbon_vm_notify(Carbon_ViewModel *vm, uint32_t id);
```

---

### 4.2 Theme System Enhancement
**Priority:** Low
**Location:** Enhance existing `src/ui/` code

Add semantic colors and consistent styling.

**Features:**
- Named color slots (background, text, accent, success, warning, danger)
- Per-widget state colors (normal, hover, pressed, disabled)
- Easy theme switching

**API Design:**
```c
// Add to existing UI system
typedef struct Carbon_UITheme {
    // Background colors
    struct { float r, g, b, a; } bg_normal;
    struct { float r, g, b, a; } bg_light;
    struct { float r, g, b, a; } bg_dark;
    struct { float r, g, b, a; } bg_panel;

    // Text colors
    struct { float r, g, b, a; } text_normal;
    struct { float r, g, b, a; } text_dimmed;
    struct { float r, g, b, a; } text_highlight;
    struct { float r, g, b, a; } text_disabled;

    // Accent colors (buttons, selections)
    struct { float r, g, b, a; } accent_normal;
    struct { float r, g, b, a; } accent_hover;
    struct { float r, g, b, a; } accent_pressed;
    struct { float r, g, b, a; } accent_disabled;

    // Semantic colors
    struct { float r, g, b, a; } success;
    struct { float r, g, b, a; } warning;
    struct { float r, g, b, a; } danger;
    struct { float r, g, b, a; } info;

    // Border styling
    struct { float r, g, b, a; } border_color;
    float border_radius;
    float border_width;

    // Spacing
    float padding;
    float margin;
    float item_spacing;
} Carbon_UITheme;

Carbon_UITheme carbon_ui_theme_default(void);
Carbon_UITheme carbon_ui_theme_dark(void);
Carbon_UITheme carbon_ui_theme_light(void);

void cui_set_theme(Carbon_UI *ui, const Carbon_UITheme *theme);
const Carbon_UITheme *cui_get_theme(Carbon_UI *ui);
```

---

## Phase 5: 3D Support (Future)

### 5.1 3D Camera System
**Priority:** Low (future expansion)
**Location:** `src/graphics/camera3d.c`, `include/carbon/camera3d.h`

Orbital camera for 3D views (galaxy maps, isometric).

**Features:**
- Spherical coordinate manipulation
- Smooth transitions
- Orbital controls (orbit, zoom, pan)

**API Design:**
```c
typedef struct Carbon_Camera3D Carbon_Camera3D;

Carbon_Camera3D *carbon_camera3d_create(void);
void carbon_camera3d_destroy(Carbon_Camera3D *cam);

// Position
void carbon_camera3d_set_position(Carbon_Camera3D *cam, float x, float y, float z);
void carbon_camera3d_set_target(Carbon_Camera3D *cam, float x, float y, float z);
void carbon_camera3d_get_position(Carbon_Camera3D *cam, float *x, float *y, float *z);
void carbon_camera3d_get_target(Carbon_Camera3D *cam, float *x, float *y, float *z);

// Spherical coordinates
void carbon_camera3d_set_spherical(Carbon_Camera3D *cam, float yaw, float pitch, float distance);
void carbon_camera3d_get_spherical(Carbon_Camera3D *cam, float *yaw, float *pitch, float *distance);

// Controls
void carbon_camera3d_orbit(Carbon_Camera3D *cam, float delta_yaw, float delta_pitch);
void carbon_camera3d_zoom(Carbon_Camera3D *cam, float delta);
void carbon_camera3d_pan(Carbon_Camera3D *cam, float dx, float dy);

// Constraints
void carbon_camera3d_set_distance_limits(Carbon_Camera3D *cam, float min, float max);
void carbon_camera3d_set_pitch_limits(Carbon_Camera3D *cam, float min, float max);

// Projection
void carbon_camera3d_set_perspective(Carbon_Camera3D *cam, float fov, float aspect,
                                      float near, float far);
void carbon_camera3d_set_orthographic(Carbon_Camera3D *cam, float width, float height,
                                       float near, float far);

// Matrices
void carbon_camera3d_get_view_matrix(Carbon_Camera3D *cam, float *out_mat4);
void carbon_camera3d_get_projection_matrix(Carbon_Camera3D *cam, float *out_mat4);
void carbon_camera3d_get_view_projection(Carbon_Camera3D *cam, float *out_mat4);

// Update (call each frame)
void carbon_camera3d_update(Carbon_Camera3D *cam);

// Smooth transitions
void carbon_camera3d_animate_to(Carbon_Camera3D *cam, float x, float y, float z, float duration);
void carbon_camera3d_animate_spherical_to(Carbon_Camera3D *cam, float yaw, float pitch, float distance, float duration);
bool carbon_camera3d_is_animating(Carbon_Camera3D *cam);
```

---

## Additional Systems (From Ecosocialism)

### History Tracking
**Priority:** Medium
**Location:** `src/game/history.c`, `include/carbon/history.h`

Metrics snapshots and event log for graphs and timeline.

```c
#define CARBON_HISTORY_MAX_SNAPSHOTS 100
#define CARBON_HISTORY_MAX_EVENTS 50
#define CARBON_HISTORY_MAX_METRICS 16

typedef struct Carbon_MetricSnapshot {
    int turn;
    float values[CARBON_HISTORY_MAX_METRICS];
} Carbon_MetricSnapshot;

typedef struct Carbon_HistoryEvent {
    int turn;
    int type;
    char title[64];
    char description[256];
} Carbon_HistoryEvent;

typedef struct Carbon_History Carbon_History;

Carbon_History *carbon_history_create(void);
void carbon_history_destroy(Carbon_History *h);

void carbon_history_set_metric_name(Carbon_History *h, int index, const char *name);
void carbon_history_add_snapshot(Carbon_History *h, const Carbon_MetricSnapshot *snap);
void carbon_history_add_event(Carbon_History *h, const Carbon_HistoryEvent *event);

int carbon_history_snapshot_count(const Carbon_History *h);
const Carbon_MetricSnapshot *carbon_history_get_snapshot(const Carbon_History *h, int index);
int carbon_history_event_count(const Carbon_History *h);
const Carbon_HistoryEvent *carbon_history_get_event(const Carbon_History *h, int index);
```

---

### Modifier Stack
**Priority:** Medium
**Location:** `src/game/modifier.c`, `include/carbon/modifier.h`

Composable effect multipliers with source tracking.

```c
#define CARBON_MODIFIER_MAX 32

typedef struct Carbon_Modifier {
    char source[32];
    float value;            // 0.1 = +10%, -0.05 = -5%
} Carbon_Modifier;

typedef struct Carbon_ModifierStack {
    Carbon_Modifier modifiers[CARBON_MODIFIER_MAX];
    int count;
} Carbon_ModifierStack;

void carbon_modifier_init(Carbon_ModifierStack *stack);
bool carbon_modifier_add(Carbon_ModifierStack *stack, const char *source, float value);
bool carbon_modifier_remove(Carbon_ModifierStack *stack, const char *source);
bool carbon_modifier_set(Carbon_ModifierStack *stack, const char *source, float value);

float carbon_modifier_apply(const Carbon_ModifierStack *stack, float base_value);
float carbon_modifier_apply_additive(const Carbon_ModifierStack *stack, float base_value);
float carbon_modifier_total(const Carbon_ModifierStack *stack);
```

---

### Threshold Tracker
**Priority:** Low
**Location:** `src/game/threshold.c`, `include/carbon/threshold.h`

Callbacks when values cross boundaries.

```c
typedef void (*Carbon_ThresholdCallback)(int id, float old_val, float new_val, bool crossed_above, void *userdata);

typedef struct Carbon_ThresholdTracker Carbon_ThresholdTracker;

void carbon_threshold_init(Carbon_ThresholdTracker *tracker, float initial_value);
int carbon_threshold_add(Carbon_ThresholdTracker *tracker, float boundary,
                          Carbon_ThresholdCallback callback, void *userdata);
void carbon_threshold_remove(Carbon_ThresholdTracker *tracker, int id);
void carbon_threshold_update(Carbon_ThresholdTracker *tracker, float new_value);
```

---

## Implementation Order

**Recommended sequence based on dependencies:**

1. **Event System** (1.1) - Foundation for other systems
2. **Validation Framework** (1.3) - Improves code quality immediately
3. **Container Utilities** (1.4) - Used by many systems
4. **Save/Load System** (1.2) - Critical for any real game
5. **Turn/Phase System** (2.1) - Depends on events
6. **Resource Economy** (2.2) - Depends on events
7. **Modifier Stack** - Standalone, used by economy
8. **View Model** (4.1) - Depends on events
9. **Technology Tree** (2.3) - Depends on economy
10. **History Tracking** - Standalone
11. **Victory System** (2.4) - Depends on economy, tech
12. **AI Framework** (3.1) - Depends on all game systems
13. **Threshold Tracker** - Standalone utility
14. **Theme Enhancement** (4.2) - UI polish
15. **3D Camera** (5.1) - Future expansion

---

## File Structure After Implementation

```
src/
├── core/
│   ├── engine.c
│   ├── game_context.c
│   ├── error.c
│   ├── event.c          # NEW: Event dispatcher
│   ├── save.c           # NEW: Save/load system
│   └── containers.c     # NEW: Container utilities
├── game/
│   ├── turn.c           # NEW: Turn/phase system
│   ├── economy.c        # NEW: Resource economy
│   ├── tech.c           # NEW: Technology tree
│   ├── victory.c        # NEW: Victory conditions
│   ├── history.c        # NEW: History tracking
│   ├── modifier.c       # NEW: Modifier stack
│   └── threshold.c      # NEW: Threshold tracker
├── ai/
│   ├── pathfinding.c
│   └── personality.c    # NEW: AI personality system
├── ui/
│   └── viewmodel.c      # NEW: View model system
└── graphics/
    └── camera3d.c       # NEW: 3D camera (future)

include/carbon/
├── event.h              # NEW
├── save.h               # NEW
├── validate.h           # NEW (header-only)
├── containers.h         # NEW (header-only macros + functions)
├── turn.h               # NEW
├── economy.h            # NEW
├── tech.h               # NEW
├── victory.h            # NEW
├── history.h            # NEW
├── modifier.h           # NEW
├── threshold.h          # NEW
├── ai.h                 # NEW
├── viewmodel.h          # NEW
└── camera3d.h           # NEW (future)
```

---

## Notes

- All systems follow Carbon's existing patterns (opaque pointers, `carbon_*` prefix)
- Header-only utilities where appropriate for zero runtime cost
- Event system is the backbone - most systems emit/subscribe to events
- Strategy game systems are optional modules (can be excluded from build)
- C11 standard maintained throughout
- Systems are designed to work together but can be used independently
