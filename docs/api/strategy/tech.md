# Technology Tree System

Research system with prerequisites, branching, and effect application.

## Quick Start

```c
#include "carbon/tech.h"

Carbon_TechTree *tree = carbon_tech_create_with_events(events);

// Define technology
Carbon_TechDef tech = {
    .id = "improved_farming",
    .name = "Improved Farming",
    .description = "Increases food production by 20%",
    .branch = TECH_BRANCH_ECONOMY,
    .tier = 1,
    .research_cost = 100,
    .prereq_count = 0,
    .effect_count = 1,
};
tech.effects[0] = (Carbon_TechEffect){
    .type = CARBON_TECH_EFFECT_RESOURCE_BONUS,
    .target = RESOURCE_FOOD,
    .value = 0.20f,
};
carbon_tech_register(tree, &tech);
```

## Prerequisites

```c
Carbon_TechDef advanced = {
    .id = "advanced_farming",
    .prereq_count = 1,
    // ...
};
strcpy(advanced.prerequisites[0], "improved_farming");
```

## Per-Faction State

```c
Carbon_TechState state;
carbon_tech_state_init(&state);

// Check and start research
if (carbon_tech_can_research(tree, &state, "improved_farming")) {
    carbon_tech_start_research(tree, &state, "improved_farming");
}

// Add points each turn
if (carbon_tech_add_points(tree, &state, research_per_turn)) {
    // Tech completed!
}
```

## Query State

```c
float progress = carbon_tech_get_progress(&state, 0);
bool researched = carbon_tech_is_researched(tree, &state, "improved_farming");

// Get available techs
const Carbon_TechDef *available[32];
int count = carbon_tech_get_available(tree, &state, available, 32);
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
// Research multiple techs (up to CARBON_TECH_MAX_ACTIVE slots)
carbon_tech_start_research(tree, &state, "tech_a");
carbon_tech_start_research(tree, &state, "tech_b");
carbon_tech_add_points_to_slot(tree, &state, 0, 25);
carbon_tech_add_points_to_slot(tree, &state, 1, 10);
```

## Repeatable Technologies

```c
Carbon_TechDef repeatable = {
    .id = "weapon_upgrade",
    .repeatable = true,
    // Cost increases each time
};
int times = carbon_tech_get_repeat_count(tree, &state, "weapon_upgrade");
```
