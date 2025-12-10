# Technology Tree System

Research system with prerequisites, branching, and effect application.

## Quick Start

```c
#include "agentite/tech.h"

Agentite_TechTree *tree = agentite_tech_create_with_events(events);

// Define technology
Agentite_TechDef tech = {
    .id = "improved_farming",
    .name = "Improved Farming",
    .description = "Increases food production by 20%",
    .branch = TECH_BRANCH_ECONOMY,
    .tier = 1,
    .research_cost = 100,
    .prereq_count = 0,
    .effect_count = 1,
};
tech.effects[0] = (Agentite_TechEffect){
    .type = AGENTITE_TECH_EFFECT_RESOURCE_BONUS,
    .target = RESOURCE_FOOD,
    .value = 0.20f,
};
agentite_tech_register(tree, &tech);
```

## Prerequisites

```c
Agentite_TechDef advanced = {
    .id = "advanced_farming",
    .prereq_count = 1,
    // ...
};
strcpy(advanced.prerequisites[0], "improved_farming");
```

## Per-Faction State

```c
Agentite_TechState state;
agentite_tech_state_init(&state);

// Check and start research
if (agentite_tech_can_research(tree, &state, "improved_farming")) {
    agentite_tech_start_research(tree, &state, "improved_farming");
}

// Add points each turn
if (agentite_tech_add_points(tree, &state, research_per_turn)) {
    // Tech completed!
}
```

## Query State

```c
float progress = agentite_tech_get_progress(&state, 0);
bool researched = agentite_tech_is_researched(tree, &state, "improved_farming");

// Get available techs
const Agentite_TechDef *available[32];
int count = agentite_tech_get_available(tree, &state, available, 32);
```

## Effect Types

| Effect | Description |
|--------|-------------|
| `RESOURCE_BONUS` | Increase resource generation |
| `RESOURCE_CAP` | Increase resource maximum |
| `COST_REDUCTION` | Reduce costs by percentage |
| `PRODUCTION_SPEED` | Faster production |
| `UNLOCK_UNIT` | Enable a unit type |
| `UNLOCK_BUILDING` | Enable a building type |
| `ATTACK_BONUS` | Combat stat bonus |
| `DEFENSE_BONUS` | Combat stat bonus |
| `HEALTH_BONUS` | Combat stat bonus |
| `CUSTOM` | Game-defined effects |

## Concurrent Research

```c
// Research multiple techs (up to AGENTITE_TECH_MAX_ACTIVE slots)
agentite_tech_start_research(tree, &state, "tech_a");
agentite_tech_start_research(tree, &state, "tech_b");
agentite_tech_add_points_to_slot(tree, &state, 0, 25);
agentite_tech_add_points_to_slot(tree, &state, 1, 10);
```

## Repeatable Technologies

```c
Agentite_TechDef repeatable = {
    .id = "weapon_upgrade",
    .repeatable = true,
    // Cost increases each time
};
int times = agentite_tech_get_repeat_count(tree, &state, "weapon_upgrade");
```
