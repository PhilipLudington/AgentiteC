# Multi-Track AI Decisions

Parallel decision-making tracks that prevent resource competition between different AI concerns. Each track operates independently with its own budget.

## Quick Start

```c
#include "agentite/ai_tracks.h"

Agentite_AITrackSystem *tracks = agentite_ai_tracks_create();
agentite_ai_tracks_set_blackboard(tracks, blackboard);

// Register tracks with evaluators
int econ_track = agentite_ai_tracks_register(tracks, "economy", evaluate_economy);
int mil_track = agentite_ai_tracks_register(tracks, "military", evaluate_military);

// Set budgets
agentite_ai_tracks_set_budget(tracks, econ_track, RESOURCE_GOLD, 1000);
agentite_ai_tracks_set_budget(tracks, mil_track, RESOURCE_GOLD, 500);
```

## Evaluator Function

```c
void evaluate_economy(int track_id, void *game_state,
                      const Agentite_AITrackBudget *budgets, int budget_count,
                      Agentite_AITrackDecision *out, int *count, int max,
                      void *userdata) {
    *count = 0;
    Agentite_AITrackDecision *d = &out[(*count)++];
    agentite_ai_track_decision_init(d);
    d->action_type = ACTION_BUILD_MINE;
    d->target_id = best_location;
    d->score = 0.8f;
    d->resource_type = RESOURCE_GOLD;
    d->resource_cost = 500;
}
```

## Evaluating & Executing

```c
// Evaluate all tracks
Agentite_AITrackResult results;
agentite_ai_tracks_evaluate_all(tracks, game_state, &results);

// Process each track's decisions
for (int t = 0; t < results.track_count; t++) {
    Agentite_AITrackDecisionSet *set = &results.decisions[t];
    agentite_ai_tracks_sort_by_priority(set);

    for (int i = 0; i < set->count; i++) {
        Agentite_AITrackDecision *d = &set->items[i];

        // Check budget
        if (d->resource_cost > 0) {
            if (!agentite_ai_tracks_spend_budget(tracks, set->track_id,
                                                d->resource_type, d->resource_cost)) {
                continue;
            }
        }
        execute_action(d->action_type, d->target_id);
    }
}
```

## Dynamic Budgets

```c
int32_t provide_budget(int track_id, int32_t resource_type,
                       void *game_state, void *userdata) {
    if (track_id == mil_track && in_war) {
        return total_resources * 0.5f;  // 50% to military in wartime
    }
    return total_resources * 0.3f;
}

agentite_ai_tracks_set_budget_provider(tracks, provide_budget, NULL);
agentite_ai_tracks_allocate_budgets(tracks, game_state);
```

## Query Helpers

```c
const Agentite_AITrackDecision *best = agentite_ai_tracks_get_best(tracks, mil_track, &results);
const Agentite_AITrackDecision *good[32];
int count = agentite_ai_tracks_get_above_score(&results, 0.7f, good, 32);
```

## Track Types

`ECONOMY`, `MILITARY`, `RESEARCH`, `DIPLOMACY`, `EXPANSION`, `INFRASTRUCTURE`, `ESPIONAGE`, `CUSTOM`, `USER` (100+)

## Priority Levels

`LOW`, `NORMAL`, `HIGH`, `CRITICAL`

## Per-Turn Reset

```c
agentite_ai_tracks_reset_spent(tracks);
agentite_ai_tracks_clear_reasons(tracks);
```
