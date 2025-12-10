# AI Personality System

Personality-driven AI decision making with weighted behaviors, threat assessment, and goal management.

## Quick Start

```c
#include "agentite/ai.h"

// Create AI system
Agentite_AISystem *ai = agentite_ai_create();

// Initialize per-faction AI state with personality
Agentite_AIState ai_state;
agentite_ai_state_init(&ai_state, AGENTITE_AI_AGGRESSIVE);
```

## Personalities

| Personality | Description |
|-------------|-------------|
| `AGENTITE_AI_BALANCED` | Equal weights across all behaviors |
| `AGENTITE_AI_AGGRESSIVE` | High aggression, low caution |
| `AGENTITE_AI_DEFENSIVE` | High defense and caution |
| `AGENTITE_AI_ECONOMIC` | Focus on resource generation |
| `AGENTITE_AI_EXPANSIONIST` | Prioritizes territory acquisition |
| `AGENTITE_AI_TECHNOLOGIST` | Prioritizes research |
| `AGENTITE_AI_DIPLOMATIC` | Prefers alliances |
| `AGENTITE_AI_OPPORTUNIST` | Adapts based on situation |

## Registering Evaluators

```c
void evaluate_attacks(Agentite_AIState *state, void *game_ctx,
                      Agentite_AIAction *out, int *count, int max) {
    // Generate potential attack actions with priorities
    out[*count].type = AGENTITE_AI_ACTION_ATTACK;
    out[*count].target_id = enemy->id;
    out[*count].priority = calculate_attack_value(game, enemy);
    (*count)++;
}

agentite_ai_register_evaluator(ai, AGENTITE_AI_ACTION_ATTACK, evaluate_attacks);
```

## Processing Turns

```c
Agentite_AIDecision decision;
agentite_ai_process_turn(ai, &ai_state, game, &decision);

// Execute returned actions
for (int i = 0; i < decision.action_count; i++) {
    switch (decision.actions[i].type) {
        case AGENTITE_AI_ACTION_ATTACK:
            execute_attack(game, decision.actions[i].target_id);
            break;
        // ...
    }
}
```

## Threat Management

```c
// Add threat (faction, level, target location, decay time)
agentite_ai_add_threat(&ai_state, enemy_faction, 0.8f, our_base_id, 5.0f);

const Agentite_AIThreat *top = agentite_ai_get_highest_threat(&ai_state);
float threat_level = agentite_ai_calculate_threat_level(&ai_state);
```

## Goal Management

```c
int goal_idx = agentite_ai_add_goal(&ai_state, GOAL_CONQUER_REGION, region_id, 0.9f);
agentite_ai_update_goal_progress(&ai_state, goal_idx, 0.5f);
agentite_ai_complete_goal(&ai_state, goal_idx);
agentite_ai_cleanup_goals(&ai_state, 10);  // Remove goals older than 10 turns
```

## Cooldowns & Weights

```c
// Prevent repetitive actions
agentite_ai_set_cooldown(&ai_state, AGENTITE_AI_ACTION_DIPLOMACY, 5);
if (!agentite_ai_is_on_cooldown(&ai_state, AGENTITE_AI_ACTION_DIPLOMACY)) { }

// Modify weights dynamically
Agentite_AIWeights boost = { .defense = 1.5f };
agentite_ai_modify_weights(&ai_state, &boost);
agentite_ai_reset_weights(&ai_state);
```

## Deterministic Random

```c
agentite_ai_seed_random(&ai_state, game_seed);
float r = agentite_ai_random(&ai_state);
int choice = agentite_ai_random_int(&ai_state, 0, 5);
```

## Action Types

`AGENTITE_AI_ACTION_BUILD`, `ATTACK`, `DEFEND`, `EXPAND`, `RESEARCH`, `DIPLOMACY`, `RECRUIT`, `RETREAT`, `SCOUT`, `TRADE`, `UPGRADE`
