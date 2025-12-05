# HTN AI Planner

Hierarchical Task Network planner for sophisticated AI behavior. Decomposes high-level goals into executable primitive tasks.

## Quick Start

```c
#include "carbon/htn.h"

// Create domain and world state
Carbon_HTNDomain *domain = carbon_htn_domain_create();
Carbon_HTNWorldState *ws = carbon_htn_world_state_create();

// Set world state
carbon_htn_ws_set_int(ws, "health", 100);
carbon_htn_ws_set_bool(ws, "has_weapon", true);
carbon_htn_ws_set_int(ws, "ammo", 10);
```

## Primitive Tasks

Primitive tasks execute actual game actions:

```c
Carbon_HTNStatus execute_shoot(Carbon_HTNWorldState *ws, void *userdata) {
    carbon_htn_ws_inc_int(ws, "ammo", -1);
    return CARBON_HTN_SUCCESS;
}

bool precond_has_ammo(const Carbon_HTNWorldState *ws, void *userdata) {
    return carbon_htn_ws_get_int(ws, "ammo") > 0;
}

carbon_htn_register_primitive(domain, "shoot", execute_shoot, precond_has_ammo, NULL);
```

### Declarative Primitives

```c
Carbon_HTNCondition conds[] = {
    carbon_htn_cond_int("ammo", CARBON_HTN_OP_GT, 0)
};
Carbon_HTNEffect effects[] = {
    carbon_htn_effect_inc_int("ammo", -1)
};
carbon_htn_register_primitive_ex(domain, "shoot", execute_shoot, conds, 1, effects, 1);
```

## Compound Tasks

Compound tasks decompose into subtasks via methods:

```c
// Register compound task
carbon_htn_register_compound(domain, "engage_enemy");

// Add methods (tried in order)
const char *ranged_attack[] = {"aim", "shoot"};
carbon_htn_add_method(domain, "engage_enemy", precond_has_ammo, ranged_attack, 2);

const char *melee_attack[] = {"approach", "melee"};
carbon_htn_add_method(domain, "engage_enemy", NULL, melee_attack, 2);  // Fallback
```

## Planning & Execution

```c
// Generate a plan
Carbon_HTNPlan *plan = carbon_htn_plan(domain, ws, "engage_enemy", 1000);
if (carbon_htn_plan_valid(plan)) {
    for (int i = 0; i < carbon_htn_plan_length(plan); i++) {
        printf("  [%d] %s\n", i, carbon_htn_plan_get_task_name(plan, i));
    }
}

// Execute plan
Carbon_HTNExecutor *exec = carbon_htn_executor_create(domain);
carbon_htn_executor_set_plan(exec, plan);

while (carbon_htn_executor_is_running(exec)) {
    Carbon_HTNStatus status = carbon_htn_executor_update(exec, ws, game_context);
    if (status == CARBON_HTN_FAILED) {
        printf("Failed at: %s\n", carbon_htn_executor_get_current_task(exec));
        break;
    }
}
```

## Condition Operators

| Operator | Description |
|----------|-------------|
| `CARBON_HTN_OP_EQ`, `NE`, `GT`, `GE`, `LT`, `LE` | Numeric comparison |
| `CARBON_HTN_OP_HAS`, `NOT_HAS` | Key existence |
| `CARBON_HTN_OP_TRUE`, `FALSE` | Boolean check |

## Key Features

- Compound tasks decompose into subtasks via methods
- Methods tried in order; first with satisfied preconditions wins
- Primitive tasks execute actual game logic
- Declarative or callback-based conditions/effects
- Plan simulation on cloned world state
- Cycle detection and iteration limits
