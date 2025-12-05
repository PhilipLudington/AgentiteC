# Multi-Track AI Decisions

Parallel decision-making tracks that prevent resource competition between different AI concerns. Each track operates independently with its own budget.

## Quick Start

```c
#include "carbon/ai_tracks.h"

Carbon_AITrackSystem *tracks = carbon_ai_tracks_create();
carbon_ai_tracks_set_blackboard(tracks, blackboard);

// Register tracks with evaluators
int econ_track = carbon_ai_tracks_register(tracks, "economy", evaluate_economy);
int mil_track = carbon_ai_tracks_register(tracks, "military", evaluate_military);

// Set budgets
carbon_ai_tracks_set_budget(tracks, econ_track, RESOURCE_GOLD, 1000);
carbon_ai_tracks_set_budget(tracks, mil_track, RESOURCE_GOLD, 500);
```

## Evaluator Function

```c
void evaluate_economy(int track_id, void *game_state,
                      const Carbon_AITrackBudget *budgets, int budget_count,
                      Carbon_AITrackDecision *out, int *count, int max,
                      void *userdata) {
    *count = 0;
    Carbon_AITrackDecision *d = &out[(*count)++];
    carbon_ai_track_decision_init(d);
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
Carbon_AITrackResult results;
carbon_ai_tracks_evaluate_all(tracks, game_state, &results);

// Process each track's decisions
for (int t = 0; t < results.track_count; t++) {
    Carbon_AITrackDecisionSet *set = &results.decisions[t];
    carbon_ai_tracks_sort_by_priority(set);

    for (int i = 0; i < set->count; i++) {
        Carbon_AITrackDecision *d = &set->items[i];

        // Check budget
        if (d->resource_cost > 0) {
            if (!carbon_ai_tracks_spend_budget(tracks, set->track_id,
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

carbon_ai_tracks_set_budget_provider(tracks, provide_budget, NULL);
carbon_ai_tracks_allocate_budgets(tracks, game_state);
```

## Query Helpers

```c
const Carbon_AITrackDecision *best = carbon_ai_tracks_get_best(tracks, mil_track, &results);
const Carbon_AITrackDecision *good[32];
int count = carbon_ai_tracks_get_above_score(&results, 0.7f, good, 32);
```

## Track Types

`ECONOMY`, `MILITARY`, `RESEARCH`, `DIPLOMACY`, `EXPANSION`, `INFRASTRUCTURE`, `ESPIONAGE`, `CUSTOM`, `USER` (100+)

## Priority Levels

`LOW`, `NORMAL`, `HIGH`, `CRITICAL`

## Per-Turn Reset

```c
carbon_ai_tracks_reset_spent(tracks);
carbon_ai_tracks_clear_reasons(tracks);
```
