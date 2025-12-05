# Carbon Engine Feature Plan

Features identified from StellarThrone codebase analysis for potential addition to the Carbon game engine.

## Features Already in Carbon (No Action Needed)

- Turn/phase management (`turn.h`)
- Resource economy system (`resource.h`)
- Tech tree with prerequisites (`tech.h`)
- Victory conditions (`victory.h`)
- Save/load system (`save.h`)
- Event dispatcher (`event.h`)
- AI personality system (`ai.h`)
- Task queue system (`task.h`)
- Fog of war/visibility (`fog.h`)
- Network/graph system (`network.h`)
- Rate tracking (`rate.h`)
- Modifier stacks (`modifier.h`)
- Threshold callbacks (`threshold.h`)

---

## Implemented Features (Phase 1)

### 1. Hierarchical Task Network (HTN) AI Planner ✓

**Status:** COMPLETED
**Files:** `include/carbon/htn.h`, `src/ai/htn.c`

A sophisticated AI planning system that decomposes high-level goals into executable primitive tasks. Significantly more powerful than the current task queue for autonomous AI agents.

**Core Components:**
- World state management (key-value pairs with typed values)
- Task registry (compound tasks, primitive tasks, methods)
- Condition evaluation with operators (==, !=, >, >=, <, <=, has, not_has)
- Method decomposition (compound task → subtasks based on preconditions)
- Plan generation with cycle detection and timeout
- Plan execution with task lifecycle management

**API Sketch:**
```c
// World state
Carbon_HTNWorldState *carbon_htn_world_state_create(void);
void carbon_htn_world_state_set_int(Carbon_HTNWorldState *ws, const char *key, int32_t value);
void carbon_htn_world_state_set_float(Carbon_HTNWorldState *ws, const char *key, float value);
void carbon_htn_world_state_set_bool(Carbon_HTNWorldState *ws, const char *key, bool value);
int32_t carbon_htn_world_state_get_int(Carbon_HTNWorldState *ws, const char *key);

// Task registration
Carbon_HTNDomain *carbon_htn_domain_create(void);
void carbon_htn_register_primitive(Carbon_HTNDomain *domain, const char *name,
                                    Carbon_HTNPrimitiveFunc execute,
                                    Carbon_HTNConditionFunc precondition,
                                    Carbon_HTNEffectFunc effects);
void carbon_htn_register_compound(Carbon_HTNDomain *domain, const char *name);
void carbon_htn_add_method(Carbon_HTNDomain *domain, const char *compound_task,
                           Carbon_HTNConditionFunc precondition,
                           const char **subtasks, int subtask_count);

// Planning
Carbon_HTNPlan *carbon_htn_plan(Carbon_HTNDomain *domain, Carbon_HTNWorldState *ws,
                                 const char *root_task, int max_iterations);
bool carbon_htn_plan_valid(Carbon_HTNPlan *plan);
int carbon_htn_plan_length(Carbon_HTNPlan *plan);

// Execution
Carbon_HTNExecutor *carbon_htn_executor_create(Carbon_HTNDomain *domain);
void carbon_htn_executor_set_plan(Carbon_HTNExecutor *exec, Carbon_HTNPlan *plan);
Carbon_HTNStatus carbon_htn_executor_update(Carbon_HTNExecutor *exec,
                                             Carbon_HTNWorldState *ws, void *userdata);
```

**Use Cases:**
- Complex AI behavior (strategy game opponents)
- NPC daily routines with goal-driven behavior
- Automated base building and resource management

---

### 2. Multi-Track AI Decision System ✓

**Status:** COMPLETED
**Files:** `include/carbon/ai_tracks.h`, `src/ai/ai_tracks.c`

Parallel decision-making tracks that prevent resource competition between different AI concerns.

**Core Components:**
- Track registry with independent execution
- Per-track decision structures with priority scoring
- Budget allocation per track (with budget provider callbacks)
- Decision audit trail with reason strings
- Blackboard integration for resource reservations
- Decision filtering and validation
- Statistics tracking for AI tuning

**API Sketch:**
```c
// Track system
Carbon_AITrackSystem *carbon_ai_tracks_create(void);
int carbon_ai_tracks_register(Carbon_AITrackSystem *tracks, const char *name,
                               Carbon_AITrackEvaluator evaluator);
carbon_ai_tracks_set_blackboard(tracks, blackboard);

// Budget allocation
void carbon_ai_tracks_set_budget(Carbon_AITrackSystem *tracks, int track_id,
                                  int resource_type, int32_t amount);
int32_t carbon_ai_tracks_get_budget(Carbon_AITrackSystem *tracks, int track_id,
                                     int resource_type);
carbon_ai_tracks_set_budget_provider(tracks, provider_fn, userdata);

// Decision making
void carbon_ai_tracks_evaluate_all(Carbon_AITrackSystem *tracks, void *game_state,
                                    Carbon_AITrackResult *out_result);
const Carbon_AITrackDecision *carbon_ai_tracks_get_best(tracks, track_id, &result);

// Audit trail
void carbon_ai_tracks_set_reason(Carbon_AITrackSystem *tracks, int track_id,
                                  const char *reason_fmt, ...);
const char *carbon_ai_tracks_get_reason(Carbon_AITrackSystem *tracks, int track_id);
```

**Predefined Tracks:**
- `CARBON_AI_TRACK_ECONOMY` - Resource production, expansion
- `CARBON_AI_TRACK_MILITARY` - Unit production, defense
- `CARBON_AI_TRACK_RESEARCH` - Technology priorities
- `CARBON_AI_TRACK_DIPLOMACY` - Relations, treaties
- `CARBON_AI_TRACK_EXPANSION` - Territory growth
- `CARBON_AI_TRACK_INFRASTRUCTURE` - Building, improvements
- `CARBON_AI_TRACK_ESPIONAGE` - Intelligence, sabotage

---

### 3. Shared Blackboard System ✓

**Status:** COMPLETED
**Files:** `include/carbon/blackboard.h`, `src/ai/blackboard.c`

Cross-system communication and data sharing without direct coupling.

**Core Components:**
- Key-value storage with typed values
- Plan publication for conflict avoidance
- Resource reservation tracking
- Circular buffer for decision history
- Subscription for value changes

**API Sketch:**
```c
// Blackboard
Carbon_Blackboard *carbon_blackboard_create(void);

// Values
void carbon_blackboard_set_int(Carbon_Blackboard *bb, const char *key, int32_t value);
void carbon_blackboard_set_float(Carbon_Blackboard *bb, const char *key, float value);
void carbon_blackboard_set_ptr(Carbon_Blackboard *bb, const char *key, void *ptr);
int32_t carbon_blackboard_get_int(Carbon_Blackboard *bb, const char *key);
bool carbon_blackboard_has(Carbon_Blackboard *bb, const char *key);

// Reservations (prevent double-spending)
bool carbon_blackboard_reserve(Carbon_Blackboard *bb, const char *resource,
                                int32_t amount, const char *owner);
void carbon_blackboard_release(Carbon_Blackboard *bb, const char *resource,
                                const char *owner);
int32_t carbon_blackboard_get_reserved(Carbon_Blackboard *bb, const char *resource);

// Plan publication
void carbon_blackboard_publish_plan(Carbon_Blackboard *bb, const char *owner,
                                     const char *plan_description);
bool carbon_blackboard_has_conflicting_plan(Carbon_Blackboard *bb,
                                             const char *resource_or_target);

// History (circular buffer)
void carbon_blackboard_log(Carbon_Blackboard *bb, const char *entry_fmt, ...);
int carbon_blackboard_get_history(Carbon_Blackboard *bb, const char **out_entries,
                                   int max_entries);
```

---

### 4. Strategic Coordinator ✓

**Status:** COMPLETED
**Files:** `include/carbon/strategy.h`, `src/ai/strategy.c`

Game phase detection and utility-based strategic decision making.

**Core Components:**
- Phase detection (early/mid/late game)
- Utility curve evaluation for options
- Budget allocation proportional to utility
- Phase-based behavior modifiers

**API Sketch:**
```c
// Phase detection
typedef enum {
    CARBON_PHASE_EARLY_EXPANSION,
    CARBON_PHASE_MID_CONSOLIDATION,
    CARBON_PHASE_LATE_COMPETITION,
    CARBON_PHASE_ENDGAME
} Carbon_GamePhase;

Carbon_StrategyCoordinator *carbon_strategy_create(void);
void carbon_strategy_set_phase_thresholds(Carbon_StrategyCoordinator *coord,
                                           float early_to_mid, float mid_to_late,
                                           float late_to_end);
Carbon_GamePhase carbon_strategy_detect_phase(Carbon_StrategyCoordinator *coord,
                                               void *game_state,
                                               Carbon_PhaseAnalyzer analyzer);

// Utility evaluation
void carbon_strategy_add_option(Carbon_StrategyCoordinator *coord, const char *name,
                                 Carbon_UtilityCurve curve, float base_weight);
void carbon_strategy_evaluate_options(Carbon_StrategyCoordinator *coord,
                                       void *game_state);
float carbon_strategy_get_utility(Carbon_StrategyCoordinator *coord, const char *option);

// Budget allocation
void carbon_strategy_allocate_budget(Carbon_StrategyCoordinator *coord,
                                      int32_t total_budget,
                                      Carbon_BudgetAllocation *out_allocation);

// Phase modifiers
void carbon_strategy_set_phase_modifier(Carbon_StrategyCoordinator *coord,
                                         Carbon_GamePhase phase,
                                         const char *option, float modifier);
```

---

### 5. Trade Route / Supply Line System ✓

**Status:** COMPLETED
**Files:** `include/carbon/trade.h`, `src/strategy/trade.c`

Economic connections between locations with efficiency and protection mechanics.

**Core Components:**
- Route creation between nodes
- Distance-based efficiency calculation
- Protection bonuses from military presence
- Specialized route types (trade, military supply, research, colonial)
- Income calculation with taxes
- Supply hub mechanics

**API Sketch:**
```c
// Trade system
Carbon_TradeSystem *carbon_trade_create(void);

// Routes
uint32_t carbon_trade_create_route(Carbon_TradeSystem *trade,
                                    uint32_t source, uint32_t dest,
                                    Carbon_RouteType type);
void carbon_trade_remove_route(Carbon_TradeSystem *trade, uint32_t route_id);
void carbon_trade_set_route_protection(Carbon_TradeSystem *trade, uint32_t route_id,
                                        float protection);  // 0.0 to 1.0

// Efficiency
float carbon_trade_get_efficiency(Carbon_TradeSystem *trade, uint32_t route_id);
void carbon_trade_set_distance_callback(Carbon_TradeSystem *trade,
                                         Carbon_DistanceFunc distance_fn, void *userdata);

// Income
int32_t carbon_trade_calculate_income(Carbon_TradeSystem *trade, uint32_t faction_id);
void carbon_trade_set_tax_rate(Carbon_TradeSystem *trade, uint32_t faction_id, float rate);

// Supply bonuses
Carbon_SupplyBonus carbon_trade_get_supply_bonus(Carbon_TradeSystem *trade,
                                                  uint32_t location);

// Supply hubs
void carbon_trade_set_hub(Carbon_TradeSystem *trade, uint32_t location, bool is_hub);
int carbon_trade_get_hub_connections(Carbon_TradeSystem *trade, uint32_t hub,
                                      uint32_t *out_connections, int max);

// Queries
int carbon_trade_get_routes_from(Carbon_TradeSystem *trade, uint32_t source,
                                  uint32_t *out_routes, int max);
int carbon_trade_get_routes_to(Carbon_TradeSystem *trade, uint32_t dest,
                                uint32_t *out_routes, int max);
```

**Route Types:**
- `CARBON_ROUTE_TRADE` - Resource income
- `CARBON_ROUTE_MILITARY` - Ship repair, reinforcement
- `CARBON_ROUTE_COLONIAL` - Population growth bonus
- `CARBON_ROUTE_RESEARCH` - Research speed bonus

---

### 6. Anomaly / Discovery System ✓

**Status:** COMPLETED
**Files:** `include/carbon/anomaly.h`, `src/strategy/anomaly.c`

Discoverable points of interest with research/investigation mechanics.

**Core Components:**
- Anomaly type registry with rarity tiers
- Discovery and research status tracking
- Research progress over time
- Reward distribution on completion
- Random anomaly generation

**API Sketch:**
```c
// Anomaly registry
Carbon_AnomalyRegistry *carbon_anomaly_registry_create(void);
int carbon_anomaly_register_type(Carbon_AnomalyRegistry *reg,
                                  const Carbon_AnomalyTypeDef *def);

// Anomaly type definition
typedef struct Carbon_AnomalyTypeDef {
    const char *id;
    const char *name;
    const char *description;
    Carbon_AnomalyRarity rarity;      // COMMON, UNCOMMON, RARE, LEGENDARY
    float research_time;               // Base time to research
    Carbon_AnomalyRewardFunc reward;   // Callback when completed
} Carbon_AnomalyTypeDef;

// Anomaly instances
Carbon_AnomalyManager *carbon_anomaly_manager_create(Carbon_AnomalyRegistry *reg);
uint32_t carbon_anomaly_spawn(Carbon_AnomalyManager *mgr, int type_id,
                               int x, int y, uint32_t metadata);
uint32_t carbon_anomaly_spawn_random(Carbon_AnomalyManager *mgr, int x, int y,
                                      Carbon_AnomalyRarity max_rarity);

// Status
typedef enum {
    CARBON_ANOMALY_UNDISCOVERED,
    CARBON_ANOMALY_DISCOVERED,
    CARBON_ANOMALY_RESEARCHING,
    CARBON_ANOMALY_COMPLETED
} Carbon_AnomalyStatus;

Carbon_AnomalyStatus carbon_anomaly_get_status(Carbon_AnomalyManager *mgr, uint32_t id);
void carbon_anomaly_discover(Carbon_AnomalyManager *mgr, uint32_t id, uint32_t faction);
void carbon_anomaly_start_research(Carbon_AnomalyManager *mgr, uint32_t id,
                                    uint32_t faction, uint32_t researcher);

// Progress
void carbon_anomaly_add_progress(Carbon_AnomalyManager *mgr, uint32_t id, float amount);
float carbon_anomaly_get_progress(Carbon_AnomalyManager *mgr, uint32_t id);
bool carbon_anomaly_is_complete(Carbon_AnomalyManager *mgr, uint32_t id);

// Queries
int carbon_anomaly_get_at(Carbon_AnomalyManager *mgr, int x, int y,
                           uint32_t *out_ids, int max);
int carbon_anomaly_get_by_status(Carbon_AnomalyManager *mgr, Carbon_AnomalyStatus status,
                                  uint32_t *out_ids, int max);
```

---

### 7. Siege / Bombardment System

**Priority:** Low
**Complexity:** Medium
**Files:** `include/carbon/siege.h`, `src/strategy/siege.c`

Sustained attack mechanics over multiple rounds for location assault.

**Core Components:**
- Siege state tracking
- Progressive damage application
- Building/defense destruction
- Population effects
- Siege requirements validation
- Round limit mechanics

**API Sketch:**
```c
// Siege manager
Carbon_SiegeManager *carbon_siege_create(void);

// Start siege
uint32_t carbon_siege_begin(Carbon_SiegeManager *mgr, uint32_t attacker_id,
                             uint32_t target_location, uint32_t attacking_force);
bool carbon_siege_can_begin(Carbon_SiegeManager *mgr, uint32_t attacker_id,
                             uint32_t target_location, uint32_t attacking_force);

// Process siege round
Carbon_SiegeResult carbon_siege_process_round(Carbon_SiegeManager *mgr, uint32_t siege_id);

typedef struct Carbon_SiegeResult {
    int buildings_damaged;
    int buildings_destroyed;
    int defense_reduced;
    int population_casualties;
    bool siege_broken;       // Defenders won
    bool target_captured;    // Attackers won
    int round_number;
} Carbon_SiegeResult;

// Query
int carbon_siege_get_round(Carbon_SiegeManager *mgr, uint32_t siege_id);
float carbon_siege_get_progress(Carbon_SiegeManager *mgr, uint32_t siege_id);
bool carbon_siege_is_active(Carbon_SiegeManager *mgr, uint32_t siege_id);

// End siege
void carbon_siege_end(Carbon_SiegeManager *mgr, uint32_t siege_id);
void carbon_siege_retreat(Carbon_SiegeManager *mgr, uint32_t siege_id);

// Configuration
void carbon_siege_set_max_rounds(Carbon_SiegeManager *mgr, int max_rounds);
void carbon_siege_set_damage_callback(Carbon_SiegeManager *mgr,
                                       Carbon_SiegeDamageFunc damage_fn, void *userdata);
```

---

### 8. Formula Engine

**Priority:** Low
**Complexity:** Low
**Files:** `include/carbon/formula.h`, `src/core/formula.c`

Runtime-configurable game balance through expression evaluation.

**Core Components:**
- Mathematical expression parsing
- Variable substitution
- Function support (min, max, clamp, floor, ceil, sqrt, etc.)
- Formula caching for performance
- Error handling

**API Sketch:**
```c
// Formula context
Carbon_FormulaContext *carbon_formula_create(void);

// Variables
void carbon_formula_set_var(Carbon_FormulaContext *ctx, const char *name, double value);
double carbon_formula_get_var(Carbon_FormulaContext *ctx, const char *name);
void carbon_formula_clear_vars(Carbon_FormulaContext *ctx);

// Evaluation
double carbon_formula_eval(Carbon_FormulaContext *ctx, const char *expression);
bool carbon_formula_valid(Carbon_FormulaContext *ctx, const char *expression);
const char *carbon_formula_get_error(Carbon_FormulaContext *ctx);

// Compiled formulas (faster for repeated evaluation)
Carbon_Formula *carbon_formula_compile(Carbon_FormulaContext *ctx, const char *expression);
double carbon_formula_exec(Carbon_Formula *formula, Carbon_FormulaContext *ctx);
void carbon_formula_free(Carbon_Formula *formula);

// Built-in functions: min, max, clamp, floor, ceil, round, sqrt, pow, log, abs, sin, cos
```

**Example Usage:**
```c
Carbon_FormulaContext *ctx = carbon_formula_create();
carbon_formula_set_var(ctx, "base_damage", 10.0);
carbon_formula_set_var(ctx, "strength", 15.0);
carbon_formula_set_var(ctx, "level", 5.0);

double damage = carbon_formula_eval(ctx, "base_damage + strength * 0.5 + level * 2");
// damage = 10 + 7.5 + 10 = 27.5
```

---

### 9. Command Queue System ✓

**Status:** COMPLETED
**Files:** `include/carbon/command.h`, `src/core/command.c`

Validated, atomic command execution for player actions.

**Core Components:**
- Command type registry
- Pre-execution validation
- Queued execution during turn processing
- Result reporting (success/failure with reason)
- Command history for undo/replay

**API Sketch:**
```c
// Command system
Carbon_CommandSystem *carbon_command_create(void);

// Command registration
typedef bool (*Carbon_CommandValidator)(const Carbon_Command *cmd, void *game_state,
                                         char *error_buf, size_t error_size);
typedef bool (*Carbon_CommandExecutor)(const Carbon_Command *cmd, void *game_state);

void carbon_command_register(Carbon_CommandSystem *sys, int command_type,
                              Carbon_CommandValidator validator,
                              Carbon_CommandExecutor executor);

// Command creation
Carbon_Command *carbon_command_new(int type);
void carbon_command_set_int(Carbon_Command *cmd, const char *key, int32_t value);
void carbon_command_set_float(Carbon_Command *cmd, const char *key, float value);
void carbon_command_set_entity(Carbon_Command *cmd, const char *key, uint32_t entity);

// Validation
Carbon_CommandResult carbon_command_validate(Carbon_CommandSystem *sys,
                                              const Carbon_Command *cmd,
                                              void *game_state);

// Queueing
void carbon_command_queue(Carbon_CommandSystem *sys, Carbon_Command *cmd);
int carbon_command_queue_count(Carbon_CommandSystem *sys);
void carbon_command_queue_clear(Carbon_CommandSystem *sys);

// Execution
int carbon_command_execute_all(Carbon_CommandSystem *sys, void *game_state,
                                Carbon_CommandResult *results, int max_results);

// Result
typedef struct Carbon_CommandResult {
    bool success;
    int command_type;
    char error[128];
} Carbon_CommandResult;

// History
void carbon_command_enable_history(Carbon_CommandSystem *sys, int max_commands);
int carbon_command_get_history(Carbon_CommandSystem *sys, Carbon_Command **out, int max);
```

---

### 10. Game Query API

**Priority:** Low
**Complexity:** Low
**Files:** `include/carbon/query.h`, `src/core/query.c`

Read-only state queries with structured results for clean UI integration.

**Core Components:**
- Query registration with result types
- Cached query results
- Query invalidation on state change
- Structured result formats

**API Sketch:**
```c
// Query system
Carbon_QuerySystem *carbon_query_create(void);

// Query registration
typedef void (*Carbon_QueryFunc)(void *game_state, void *params, void *result);

void carbon_query_register(Carbon_QuerySystem *sys, const char *name,
                            Carbon_QueryFunc query_fn, size_t result_size);

// Execution
void *carbon_query_exec(Carbon_QuerySystem *sys, const char *name,
                         void *game_state, void *params);

// Caching
void carbon_query_enable_cache(Carbon_QuerySystem *sys, const char *name);
void carbon_query_invalidate(Carbon_QuerySystem *sys, const char *name);
void carbon_query_invalidate_all(Carbon_QuerySystem *sys);

// Common query patterns
// These would be game-specific but show the pattern:
// carbon_query_get_faction_resources(sys, faction_id, &resources);
// carbon_query_get_entities_at(sys, x, y, entities, max);
// carbon_query_get_visible_area(sys, faction_id, &visibility);
```

---

## Implementation Priority

### Phase 1 - Core AI Enhancement ✓ COMPLETED
1. **HTN AI Planner** ✓ - `include/carbon/htn.h`, `src/ai/htn.c`
2. **Shared Blackboard** ✓ - `include/carbon/blackboard.h`, `src/ai/blackboard.c`
3. **Trade/Supply Line System** ✓ - `include/carbon/trade.h`, `src/strategy/trade.c`

### Phase 2 - Strategic Layer ✓ COMPLETED
4. **Multi-Track AI Decisions** ✓ - `include/carbon/ai_tracks.h`, `src/ai/ai_tracks.c`
5. **Strategic Coordinator** ✓ - `include/carbon/strategy.h`, `src/ai/strategy.c`
6. **Command Queue** ✓ - `include/carbon/command.h`, `src/core/command.c`

### Phase 3 - Content Systems (In Progress)
7. **Anomaly/Discovery System** ✓ - `include/carbon/anomaly.h`, `src/strategy/anomaly.c`
8. **Siege/Bombardment System** - Extended combat
9. **Formula Engine** - Data-driven balance
10. **Game Query API** - UI integration helper

---

## Design Principles

All implementations should follow Carbon's existing patterns:

1. **Static allocation where possible** - Capacity constants, no malloc in hot paths
2. **Explicit state passing** - No hidden globals
3. **Callback-based integration** - Game-specific logic via function pointers
4. **Event emission** - Integrate with existing event dispatcher
5. **Header-only validation** - Use `carbon/validate.h` macros
6. **Error handling** - Use `carbon/error.h` patterns
7. **Documentation** - Update CLAUDE.md with API examples
