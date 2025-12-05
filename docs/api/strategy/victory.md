# Victory Condition System

Multi-condition victory tracking with progress monitoring.

## Quick Start

```c
#include "carbon/victory.h"

Carbon_VictoryManager *victory = carbon_victory_create_with_events(events);

// Register conditions
Carbon_VictoryCondition domination = {
    .id = "domination",
    .name = "World Domination",
    .description = "Control 75% of the map",
    .type = CARBON_VICTORY_DOMINATION,
    .threshold = 0.75f,
    .enabled = true,
};
carbon_victory_register(victory, &domination);

// Initialize factions
carbon_victory_init_faction(victory, 0);  // Player
carbon_victory_init_faction(victory, 1);  // AI
```

## Update Progress

```c
// Each turn
float territory = calculate_territory_percent(faction_id);
carbon_victory_update_progress(victory, faction_id, CARBON_VICTORY_DOMINATION, territory);

// For score-based
carbon_victory_add_score(victory, faction_id, CARBON_VICTORY_SCORE, points);

// Eliminate faction
carbon_victory_eliminate_faction(victory, defeated_faction);
```

## Check Victory

```c
carbon_victory_set_turn(victory, current_turn);
if (carbon_victory_check(victory)) {
    int winner = carbon_victory_get_winner(victory);
    int type = carbon_victory_get_winning_type(victory);
    printf("Victory! Type: %s\n", carbon_victory_type_name(type));
}
```

## Victory Types

| Type | Description |
|------|-------------|
| `DOMINATION` | Control percentage of territory |
| `ELIMINATION` | Last faction standing |
| `TECHNOLOGY` | Research all/specific techs |
| `ECONOMIC` | Accumulate resources |
| `SCORE` | Highest score after N turns |
| `TIME` | Survive for N turns |
| `OBJECTIVE` | Complete specific objectives |
| `WONDER` | Build a wonder |
| `DIPLOMATIC` | Achieve diplomatic status |
| `CULTURAL` | Cultural dominance |
| `USER` (100+) | Game-defined types |

## Custom Victory Checker

```c
bool check_tech_victory(int faction, int type, float *progress, void *userdata) {
    int researched = count_researched_techs(faction);
    int total = total_tech_count();
    *progress = (float)researched / (float)total;
    return researched >= total;
}
carbon_victory_set_checker(victory, CARBON_VICTORY_TECHNOLOGY, check_tech_victory, game);
```

## Query Progress

```c
float progress = carbon_victory_get_progress(victory, faction_id, CARBON_VICTORY_DOMINATION);
char buf[32];
carbon_victory_format_progress(victory, faction_id, CARBON_VICTORY_DOMINATION, buf, sizeof(buf));
int leader = carbon_victory_get_score_leader(victory);
```
