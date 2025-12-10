# Victory Condition System

Multi-condition victory tracking with progress monitoring.

## Quick Start

```c
#include "agentite/victory.h"

Agentite_VictoryManager *victory = agentite_victory_create_with_events(events);

// Register conditions
Agentite_VictoryCondition domination = {
    .id = "domination",
    .name = "World Domination",
    .description = "Control 75% of the map",
    .type = AGENTITE_VICTORY_DOMINATION,
    .threshold = 0.75f,
    .enabled = true,
};
agentite_victory_register(victory, &domination);

// Initialize factions
agentite_victory_init_faction(victory, 0);  // Player
agentite_victory_init_faction(victory, 1);  // AI
```

## Update Progress

```c
// Each turn
float territory = calculate_territory_percent(faction_id);
agentite_victory_update_progress(victory, faction_id, AGENTITE_VICTORY_DOMINATION, territory);

// For score-based
agentite_victory_add_score(victory, faction_id, AGENTITE_VICTORY_SCORE, points);

// Eliminate faction
agentite_victory_eliminate_faction(victory, defeated_faction);
```

## Check Victory

```c
agentite_victory_set_turn(victory, current_turn);
if (agentite_victory_check(victory)) {
    int winner = agentite_victory_get_winner(victory);
    int type = agentite_victory_get_winning_type(victory);
    printf("Victory! Type: %s\n", agentite_victory_type_name(type));
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
agentite_victory_set_checker(victory, AGENTITE_VICTORY_TECHNOLOGY, check_tech_victory, game);
```

## Query Progress

```c
float progress = agentite_victory_get_progress(victory, faction_id, AGENTITE_VICTORY_DOMINATION);
char buf[32];
agentite_victory_format_progress(victory, faction_id, AGENTITE_VICTORY_DOMINATION, buf, sizeof(buf));
int leader = agentite_victory_get_score_leader(victory);
```
