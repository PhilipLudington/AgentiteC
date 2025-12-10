# HTN AI Planner

Hierarchical Task Network planner for sophisticated AI behavior. Decomposes high-level goals into executable primitive tasks.

## Quick Start

```c
#include "agentite/htn.h"

// Create domain and world state
Agentite_HTNDomain *domain = agentite_htn_domain_create();
Agentite_HTNWorldState *ws = agentite_htn_world_state_create();

// Set world state
agentite_htn_ws_set_int(ws, "health", 100);
agentite_htn_ws_set_bool(ws, "has_weapon", true);
agentite_htn_ws_set_int(ws, "ammo", 10);
```

## Primitive Tasks

Primitive tasks execute actual game actions:

```c
Agentite_HTNStatus execute_shoot(Agentite_HTNWorldState *ws, void *userdata) {
    agentite_htn_ws_inc_int(ws, "ammo", -1);
    return AGENTITE_HTN_SUCCESS;
}

bool precond_has_ammo(const Agentite_HTNWorldState *ws, void *userdata) {
    return agentite_htn_ws_get_int(ws, "ammo") > 0;
}

agentite_htn_register_primitive(domain, "shoot", execute_shoot, precond_has_ammo, NULL);
```

### Declarative Primitives

```c
Agentite_HTNCondition conds[] = {
    agentite_htn_cond_int("ammo", AGENTITE_HTN_OP_GT, 0)
};
Agentite_HTNEffect effects[] = {
    agentite_htn_effect_inc_int("ammo", -1)
};
agentite_htn_register_primitive_ex(domain, "shoot", execute_shoot, conds, 1, effects, 1);
```

## Compound Tasks

Compound tasks decompose into subtasks via methods:

```c
// Register compound task
agentite_htn_register_compound(domain, "engage_enemy");

// Add methods (tried in order)
const char *ranged_attack[] = {"aim", "shoot"};
agentite_htn_add_method(domain, "engage_enemy", precond_has_ammo, ranged_attack, 2);

const char *melee_attack[] = {"approach", "melee"};
agentite_htn_add_method(domain, "engage_enemy", NULL, melee_attack, 2);  // Fallback
```

## Planning & Execution

```c
// Generate a plan
Agentite_HTNPlan *plan = agentite_htn_plan(domain, ws, "engage_enemy", 1000);
if (agentite_htn_plan_valid(plan)) {
    for (int i = 0; i < agentite_htn_plan_length(plan); i++) {
        printf("  [%d] %s\n", i, agentite_htn_plan_get_task_name(plan, i));
    }
}

// Execute plan
Agentite_HTNExecutor *exec = agentite_htn_executor_create(domain);
agentite_htn_executor_set_plan(exec, plan);

while (agentite_htn_executor_is_running(exec)) {
    Agentite_HTNStatus status = agentite_htn_executor_update(exec, ws, game_context);
    if (status == AGENTITE_HTN_FAILED) {
        printf("Failed at: %s\n", agentite_htn_executor_get_current_task(exec));
        break;
    }
}
```

## Condition Operators

| Operator | Description |
|----------|-------------|
| `AGENTITE_HTN_OP_EQ`, `NE`, `GT`, `GE`, `LT`, `LE` | Numeric comparison |
| `AGENTITE_HTN_OP_HAS`, `NOT_HAS` | Key existence |
| `AGENTITE_HTN_OP_TRUE`, `FALSE` | Boolean check |

## Key Features

- Compound tasks decompose into subtasks via methods
- Methods tried in order; first with satisfied preconditions wins
- Primitive tasks execute actual game logic
- Declarative or callback-based conditions/effects
- Plan simulation on cloned world state
- Cycle detection and iteration limits
