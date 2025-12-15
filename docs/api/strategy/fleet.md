# Fleet/Army Management System

Strategic unit management for 4X and strategy games with automated battle resolution, unit counters, morale, commanders, and experience.

## Quick Start

```c
#include "agentite/fleet.h"

// Create fleet manager
Agentite_FleetManager *fm = agentite_fleet_create();

// Create a fleet
Agentite_Fleet fleet = {
    .name = "First Fleet",
    .owner_id = player_id,
    .morale = 100,
    .supply = 100, .supply_max = 100,
    .is_space_fleet = true
};
int fleet_id = agentite_fleet_add(fm, &fleet);

// Add units
agentite_fleet_add_units(fm, fleet_id, AGENTITE_UNIT_DESTROYER, 5);
agentite_fleet_add_units(fm, fleet_id, AGENTITE_UNIT_CRUISER, 2);
agentite_fleet_add_units(fm, fleet_id, AGENTITE_UNIT_FIGHTER, 20);

// Assign commander
Agentite_Commander cmd = {
    .name = "Admiral Smith",
    .attack_bonus = 10,
    .defense_bonus = 5,
    .level = 3,
    .ability = AGENTITE_ABILITY_FIRST_STRIKE
};
agentite_fleet_set_commander(fm, fleet_id, &cmd);

// Preview battle before committing
Agentite_BattlePreview preview;
agentite_fleet_preview_battle(fm, my_fleet_id, enemy_fleet_id, &preview);

if (preview.attacker_win_chance > 0.6f) {
    // Execute battle
    Agentite_BattleResult result;
    agentite_fleet_battle(fm, my_fleet_id, enemy_fleet_id, &result);
    show_battle_report(&result);
}

agentite_fleet_destroy(fm);
```

## Key Concepts

### Unit Classes

Space units:
- `AGENTITE_UNIT_FIGHTER` - Fast, anti-bomber
- `AGENTITE_UNIT_BOMBER` - Anti-capital ship
- `AGENTITE_UNIT_CORVETTE` - Light multi-role
- `AGENTITE_UNIT_FRIGATE` - Anti-fighter screen
- `AGENTITE_UNIT_DESTROYER` - Balanced combat
- `AGENTITE_UNIT_CRUISER` - Heavy combat
- `AGENTITE_UNIT_BATTLESHIP` - Capital ship
- `AGENTITE_UNIT_CARRIER` - Fighter/bomber platform
- `AGENTITE_UNIT_DREADNOUGHT` - Super-capital

Ground units:
- `AGENTITE_UNIT_INFANTRY`, `AGENTITE_UNIT_ARMOR`, `AGENTITE_UNIT_ARTILLERY`
- `AGENTITE_UNIT_MECH`, `AGENTITE_UNIT_SPECIAL_OPS`, `AGENTITE_UNIT_ANTI_AIR`
- `AGENTITE_UNIT_ENGINEER`, `AGENTITE_UNIT_TRANSPORT`, `AGENTITE_UNIT_DROPSHIP`

### Counter System

Units have effectiveness modifiers against other types:

```c
Agentite_Effectiveness eff = agentite_unit_get_effectiveness(
    AGENTITE_UNIT_FIGHTER, AGENTITE_UNIT_BOMBER
);
// Returns AGENTITE_EFFECT_COUNTER (1.5x damage)

float mult = agentite_effectiveness_multiplier(eff);
// Returns 1.5
```

Effectiveness levels:
- `HARD_COUNTER` - 0.5x (very weak against)
- `WEAK` - 0.75x
- `NEUTRAL` - 1.0x
- `STRONG` - 1.25x
- `COUNTER` - 1.5x (very effective against)

### Commanders

Commanders provide bonuses and special abilities:

```c
// Get bonus percentage
int atk_bonus = agentite_commander_get_bonus(commander, 0);  // Attack
int def_bonus = agentite_commander_get_bonus(commander, 1);  // Defense
int mor_bonus = agentite_commander_get_bonus(commander, 2);  // Morale
int spd_bonus = agentite_commander_get_bonus(commander, 3);  // Speed

// Add experience (may level up)
if (agentite_fleet_commander_add_xp(fm, fleet_id, 500)) {
    show_notification("Admiral leveled up!");
}
```

Commander abilities:
- `FIRST_STRIKE` - Bonus damage in round 1
- `TACTICAL_RETREAT` - Reduced losses on retreat
- `INSPIRATION` - Morale bonus
- `FLANKING` - Bonus vs damaged enemies
- `FORTIFY` - Defensive bonus
- `BLITZ` - Extra attack speed
- `LOGISTICS` - Reduced supply cost
- `VETERAN_TRAINING` - Faster XP gain

### Battle System

Battles are resolved in rounds using Lanchester's laws:

```c
// Preview shows estimated outcome
Agentite_BattlePreview preview;
agentite_fleet_preview_battle(fm, attacker_id, defender_id, &preview);

printf("Win chance: %.0f%%\n", preview.attacker_win_chance * 100);
printf("Est. losses: %d vs %d\n",
       preview.estimated_attacker_losses,
       preview.estimated_defender_losses);

// Execute battle
Agentite_BattleResult result;
agentite_fleet_battle(fm, attacker_id, defender_id, &result);

// Access round-by-round details
for (int r = 0; r < result.rounds_fought; r++) {
    printf("Round %d: %d vs %d damage\n",
           result.rounds[r].round_number,
           result.rounds[r].attacker_damage,
           result.rounds[r].defender_damage);
}
```

### Fleet Operations

```c
// Merge fleets
agentite_fleet_merge(fm, dst_fleet_id, src_fleet_id);  // src is removed

// Split units into new fleet
int new_fleet = agentite_fleet_split(fm, src_fleet_id,
    AGENTITE_UNIT_DESTROYER, 3, "Task Force Alpha");

// Repair damaged units
agentite_fleet_repair(fm, fleet_id, 50);  // +50 HP per unit

// Update morale
agentite_fleet_update_morale(fm, fleet_id, -10);  // After defeat

// Retreat from combat
agentite_fleet_retreat(fm, fleet_id);
```

## Key Functions

| Function | Description |
|----------|-------------|
| `agentite_fleet_create()` | Create fleet manager |
| `agentite_fleet_destroy(fm)` | Free fleet manager |
| `agentite_fleet_add(fm, fleet)` | Add fleet, returns ID |
| `agentite_fleet_remove(fm, id)` | Remove fleet |
| `agentite_fleet_get(fm, id)` | Get fleet by ID (mutable) |
| `agentite_fleet_add_units(fm, id, class, count)` | Add units to fleet |
| `agentite_fleet_remove_units(fm, id, class, count)` | Remove units |
| `agentite_fleet_get_unit_count(fm, id, class)` | Count units (-1 for total) |
| `agentite_fleet_get_strength(fm, id)` | Get combat power |
| `agentite_fleet_set_commander(fm, id, cmd)` | Assign commander |
| `agentite_fleet_preview_battle(fm, atk, def, out)` | Preview battle outcome |
| `agentite_fleet_battle(fm, atk, def, out)` | Execute battle |
| `agentite_fleet_merge(fm, dst, src)` | Combine fleets |
| `agentite_fleet_split(fm, src, class, count, name)` | Split into new fleet |
| `agentite_fleet_get_upkeep(fm, id)` | Per-turn maintenance cost |
| `agentite_fleet_get_cost(fm, id)` | Total build cost |

## Experience System

Units gain experience from combat:

```c
// Add XP to specific unit type
agentite_fleet_add_unit_xp(fm, fleet_id, AGENTITE_UNIT_FIGHTER, 100);

// Add XP to all units
agentite_fleet_add_unit_xp(fm, fleet_id, -1, 50);

// Veterans deal more damage
const Agentite_Fleet *fleet = agentite_fleet_get_const(fm, fleet_id);
for (int i = 0; i < fleet->unit_count; i++) {
    float bonus = agentite_unit_xp_bonus(&fleet->units[i]);
    // Returns 1.0 to 1.5 based on experience
}
```

## Battle Callback

Hook into battles for animation/UI:

```c
void on_battle_round(Agentite_FleetManager *fm,
                     const Agentite_BattleRound *round, void *userdata) {
    animate_battle_round(round->round_number,
                         round->attacker_damage,
                         round->defender_damage);
}

agentite_fleet_set_battle_callback(fm, on_battle_round, NULL);
```

## Notes

- Fleets are copied when added; modifying the original has no effect
- Morale affects combat performance and retreat chance
- Supply affects movement range in strategic layer
- Experience persists until units are destroyed
- Battles automatically update unit counts and XP
