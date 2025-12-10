# Strategic Coordinator

Game phase detection and utility-based strategic decision making. Automatically allocates budgets based on option utilities and game phase.

## Quick Start

```c
#include "agentite/strategy.h"

Agentite_StrategyCoordinator *coord = agentite_strategy_create();

// Set phase analyzer
int analyze_phase(void *game_state, float *out_metrics, int max, void *userdata) {
    out_metrics[0] = (float)game->turn / (float)game->max_turns;
    out_metrics[1] = (float)game->territories / (float)game->total_territories;
    return 2;  // Number of metrics
}
agentite_strategy_set_phase_analyzer(coord, analyze_phase, NULL);

// Configure thresholds
agentite_strategy_set_phase_thresholds(coord, 0.25f, 0.60f, 0.85f);
```

## Adding Strategic Options

```c
// Option with utility curve
agentite_strategy_add_option(coord, "economy",
    &agentite_curve_sqrt(0.0f, 1.0f),  // Diminishing returns
    1.0f);                            // Base weight

agentite_strategy_add_option(coord, "military",
    &agentite_curve_sigmoid(8.0f, 0.5f),  // Ramps up mid-game
    1.0f);

// Phase-specific modifiers
agentite_strategy_set_phase_modifier(coord, "economy",
    AGENTITE_GAME_PHASE_EARLY_EXPANSION, 1.5f);  // +50% early
agentite_strategy_set_phase_modifier(coord, "military",
    AGENTITE_GAME_PHASE_LATE_COMPETITION, 1.8f); // +80% late
```

## Utility Curves

| Curve | Description |
|-------|-------------|
| `agentite_curve_linear(min, max)` | y = x |
| `agentite_curve_quadratic(min, max)` | y = x² (slow start) |
| `agentite_curve_sqrt(min, max)` | y = √x (fast start, diminishing) |
| `agentite_curve_sigmoid(steepness, midpoint)` | S-curve |
| `agentite_curve_inverse(min, max)` | y = 1 - x |
| `agentite_curve_step(threshold, low, high)` | Step at threshold |
| `agentite_curve_exponential(rate, min, max)` | Exponential growth |
| `agentite_curve_logarithmic(scale, min, max)` | Logarithmic |

## Input Provider

```c
float provide_input(void *game_state, const char *option, void *userdata) {
    if (strcmp(option, "economy") == 0) {
        return 1.0f - (float)our_income / avg_income;  // Need more if behind
    }
    return 0.5f;
}
agentite_strategy_set_input_provider(coord, provide_input, NULL);
```

## Evaluation & Budget Allocation

```c
// Detect phase and evaluate
Agentite_GamePhase phase = agentite_strategy_detect_phase(coord, game_state);
agentite_strategy_evaluate_options(coord, game_state);

// Get best option
const char *best = agentite_strategy_get_best_option(coord);

// Allocate budget proportionally
Agentite_BudgetAllocation allocs[4];
int count = agentite_strategy_allocate_budget(coord, total_resources, allocs, 4);
for (int i = 0; i < count; i++) {
    printf("%s: %d (%.0f%%)\n", allocs[i].option_name,
           allocs[i].allocated, allocs[i].proportion * 100);
}

// Set constraints
agentite_strategy_set_min_allocation(coord, "military", 0.15f);
agentite_strategy_set_max_allocation(coord, "research", 0.40f);
```

## Game Phases

| Phase | Description |
|-------|-------------|
| `AGENTITE_GAME_PHASE_EARLY_EXPANSION` | Early game, expansion focus |
| `AGENTITE_GAME_PHASE_MID_CONSOLIDATION` | Mid game, consolidation |
| `AGENTITE_GAME_PHASE_LATE_COMPETITION` | Late game, competition |
| `AGENTITE_GAME_PHASE_ENDGAME` | Final push |
