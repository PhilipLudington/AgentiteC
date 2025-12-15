# Combat System

Turn-based tactical combat with initiative ordering, telegraphing, reaction mechanics, status effects, and grid-based positioning.

## Quick Start

```c
#include "agentite/combat.h"

// Create combat system with 16x16 grid
Agentite_CombatSystem *combat = agentite_combat_create(16, 16);

// Add player combatant
Agentite_Combatant player = {
    .name = "Hero",
    .hp = 100, .hp_max = 100,
    .initiative = 15,
    .armor = 5,
    .position = {2, 8},
    .movement_range = 4
};
int player_id = agentite_combat_add_combatant(combat, &player, true);

// Add enemy combatant
Agentite_Combatant enemy = {
    .name = "Goblin",
    .hp = 40, .hp_max = 40,
    .initiative = 8,
    .armor = 2,
    .position = {12, 8},
    .movement_range = 3
};
int enemy_id = agentite_combat_add_combatant(combat, &enemy, false);

// Start combat
agentite_combat_start(combat);

// Game loop
while (!agentite_combat_is_over(combat)) {
    int current = agentite_combat_get_current_combatant(combat);
    const Agentite_Combatant *c = agentite_combat_get_combatant_const(combat, current);

    if (c->is_player_team) {
        // Show enemy telegraphs
        Agentite_Telegraph telegraphs[16];
        int count = agentite_combat_get_telegraphs(combat, telegraphs, 16);

        // Player selects action via UI...
        Agentite_CombatAction action = {
            .type = AGENTITE_ACTION_ATTACK,
            .actor_id = current,
            .target_id = enemy_id
        };

        if (agentite_combat_is_action_valid(combat, &action)) {
            agentite_combat_queue_action(combat, &action);
            agentite_combat_execute_turn(combat);
        }
    } else {
        // AI decides action
        agentite_combat_generate_enemy_actions(combat);
        agentite_combat_execute_turn(combat);
    }
}

Agentite_CombatResult result = agentite_combat_get_result(combat);
agentite_combat_destroy(combat);
```

## Key Concepts

### Turn Order

Combatants act in initiative order (highest first). Use `agentite_combat_get_turn_order()` to display the queue.

### Telegraphing

At the start of each player turn, enemies "telegraph" their intended actions. This allows tactical counterplay:

```c
Agentite_Telegraph telegraphs[16];
int count = agentite_combat_get_telegraphs(combat, telegraphs, 16);

for (int i = 0; i < count; i++) {
    // Show indicator on target tile
    draw_attack_indicator(telegraphs[i].target_pos, telegraphs[i].predicted_damage);
}
```

### Status Effects

Apply buffs and debuffs with duration:

```c
// Apply 3-turn burning effect
agentite_combat_apply_status(combat, target_id,
    AGENTITE_STATUS_BURNING, 3, 1, attacker_id);

// Check for status
if (agentite_combat_has_status(combat, id, AGENTITE_STATUS_STUNNED)) {
    // Cannot act
}

// Process status effects at end of turn
agentite_combat_tick_status(combat, id);
```

Available status types:
- `AGENTITE_STATUS_STUNNED` - Cannot act
- `AGENTITE_STATUS_BURNING/POISONED/BLEEDING` - Damage over time
- `AGENTITE_STATUS_ROOTED` - Cannot move
- `AGENTITE_STATUS_BLINDED` - Reduced hit chance
- `AGENTITE_STATUS_VULNERABLE` - +50% damage taken
- `AGENTITE_STATUS_FORTIFIED` - -25% damage taken
- `AGENTITE_STATUS_HASTED` - Extra action
- `AGENTITE_STATUS_INVULNERABLE` - No damage
- `AGENTITE_STATUS_CONCEALED` - Harder to hit

### Grid Movement

```c
// Get valid movement positions
Agentite_GridPos moves[64];
int count = agentite_combat_get_valid_moves(combat, combatant_id, moves, 64);

// Calculate distance
int dist = agentite_combat_distance(
    (Agentite_GridPos){2, 3},
    (Agentite_GridPos){5, 7},
    AGENTITE_DISTANCE_CHEBYSHEV  // 8-directional
);
```

## Key Functions

| Function | Description |
|----------|-------------|
| `agentite_combat_create(w, h)` | Create combat system with grid size |
| `agentite_combat_destroy(combat)` | Free combat system |
| `agentite_combat_add_combatant(combat, data, is_player)` | Add combatant, returns ID |
| `agentite_combat_start(combat)` | Begin combat, calculate turn order |
| `agentite_combat_is_over(combat)` | Check if combat ended |
| `agentite_combat_get_result(combat)` | Get victory/defeat/draw result |
| `agentite_combat_get_current_combatant(combat)` | Current turn's combatant ID |
| `agentite_combat_get_turn_order(combat, out, max)` | Get initiative queue |
| `agentite_combat_queue_action(combat, action)` | Queue action for execution |
| `agentite_combat_execute_turn(combat)` | Execute queued actions |
| `agentite_combat_is_action_valid(combat, action)` | Validate action before queue |
| `agentite_combat_get_telegraphs(combat, out, max)` | Get enemy intent previews |
| `agentite_combat_apply_damage(combat, id, dmg)` | Deal damage to combatant |
| `agentite_combat_heal(combat, id, amt)` | Heal combatant |
| `agentite_combat_apply_status(combat, id, type, dur, stacks, src)` | Apply status effect |
| `agentite_combat_get_valid_moves(combat, id, out, max)` | Get reachable positions |
| `agentite_combat_get_valid_targets(combat, id, atk, out, max)` | Get attackable targets |

## Action Types

| Type | Description |
|------|-------------|
| `AGENTITE_ACTION_MOVE` | Move to grid position |
| `AGENTITE_ACTION_ATTACK` | Basic attack on target |
| `AGENTITE_ACTION_DEFEND` | Defensive stance (+armor) |
| `AGENTITE_ACTION_USE_ITEM` | Use consumable item |
| `AGENTITE_ACTION_ABILITY` | Use special ability |
| `AGENTITE_ACTION_WAIT` | Skip turn, keep reaction |
| `AGENTITE_ACTION_FLEE` | Attempt to escape combat |

## Event Callback

Hook into combat events for animations and logging:

```c
void on_combat_event(Agentite_CombatSystem *combat,
                     const Agentite_CombatEvent *event, void *userdata) {
    if (event->was_critical) {
        play_sound("critical_hit");
    }
    show_damage_number(event->target_id, event->damage_dealt);
}

agentite_combat_set_event_callback(combat, on_combat_event, NULL);
```

## Notes

- Combatant data is copied when added; modifying the original has no effect
- Deletion during iteration uses deferred execution
- Grid uses Chebyshev distance by default (8-directional movement)
- Status effect durations decrease at end of affected combatant's turn
