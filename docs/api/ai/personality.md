# AI Personality System

Personality-driven AI decision making with weighted behaviors, threat assessment, and goal management.

## Quick Start

```c
#include "carbon/ai.h"

// Create AI system
Carbon_AISystem *ai = carbon_ai_create();

// Initialize per-faction AI state with personality
Carbon_AIState ai_state;
carbon_ai_state_init(&ai_state, CARBON_AI_AGGRESSIVE);
```

## Personalities

| Personality | Description |
|-------------|-------------|
| `CARBON_AI_BALANCED` | Equal weights across all behaviors |
| `CARBON_AI_AGGRESSIVE` | High aggression, low caution |
| `CARBON_AI_DEFENSIVE` | High defense and caution |
| `CARBON_AI_ECONOMIC` | Focus on resource generation |
| `CARBON_AI_EXPANSIONIST` | Prioritizes territory acquisition |
| `CARBON_AI_TECHNOLOGIST` | Prioritizes research |
| `CARBON_AI_DIPLOMATIC` | Prefers alliances |
| `CARBON_AI_OPPORTUNIST` | Adapts based on situation |

## Registering Evaluators

```c
void evaluate_attacks(Carbon_AIState *state, void *game_ctx,
                      Carbon_AIAction *out, int *count, int max) {
    // Generate potential attack actions with priorities
    out[*count].type = CARBON_AI_ACTION_ATTACK;
    out[*count].target_id = enemy->id;
    out[*count].priority = calculate_attack_value(game, enemy);
    (*count)++;
}

carbon_ai_register_evaluator(ai, CARBON_AI_ACTION_ATTACK, evaluate_attacks);
```

## Processing Turns

```c
Carbon_AIDecision decision;
carbon_ai_process_turn(ai, &ai_state, game, &decision);

// Execute returned actions
for (int i = 0; i < decision.action_count; i++) {
    switch (decision.actions[i].type) {
        case CARBON_AI_ACTION_ATTACK:
            execute_attack(game, decision.actions[i].target_id);
            break;
        // ...
    }
}
```

## Threat Management

```c
// Add threat (faction, level, target location, decay time)
carbon_ai_add_threat(&ai_state, enemy_faction, 0.8f, our_base_id, 5.0f);

const Carbon_AIThreat *top = carbon_ai_get_highest_threat(&ai_state);
float threat_level = carbon_ai_calculate_threat_level(&ai_state);
```

## Goal Management

```c
int goal_idx = carbon_ai_add_goal(&ai_state, GOAL_CONQUER_REGION, region_id, 0.9f);
carbon_ai_update_goal_progress(&ai_state, goal_idx, 0.5f);
carbon_ai_complete_goal(&ai_state, goal_idx);
carbon_ai_cleanup_goals(&ai_state, 10);  // Remove goals older than 10 turns
```

## Cooldowns & Weights

```c
// Prevent repetitive actions
carbon_ai_set_cooldown(&ai_state, CARBON_AI_ACTION_DIPLOMACY, 5);
if (!carbon_ai_is_on_cooldown(&ai_state, CARBON_AI_ACTION_DIPLOMACY)) { }

// Modify weights dynamically
Carbon_AIWeights boost = { .defense = 1.5f };
carbon_ai_modify_weights(&ai_state, &boost);
carbon_ai_reset_weights(&ai_state);
```

## Deterministic Random

```c
carbon_ai_seed_random(&ai_state, game_seed);
float r = carbon_ai_random(&ai_state);
int choice = carbon_ai_random_int(&ai_state, 0, 5);
```

## Action Types

`CARBON_AI_ACTION_BUILD`, `ATTACK`, `DEFEND`, `EXPAND`, `RESEARCH`, `DIPLOMACY`, `RECRUIT`, `RETREAT`, `SCOUT`, `TRADE`, `UPGRADE`
