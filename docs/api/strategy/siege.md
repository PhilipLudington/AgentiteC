# Siege / Bombardment System

Sustained attack mechanics over multiple rounds for location assault.

## Quick Start

```c
#include "carbon/siege.h"

Carbon_SiegeManager *siege = carbon_siege_create();

// Set callbacks
carbon_siege_set_defense_callback(siege, get_defense, game);
carbon_siege_set_defender_callback(siege, get_defender, game);

// Begin siege
if (carbon_siege_can_begin(siege, attacker, location, force)) {
    uint32_t id = carbon_siege_begin(siege, attacker, location, force);
}
```

## Processing Rounds

```c
Carbon_SiegeRoundResult result;
if (carbon_siege_process_round(siege, id, &result)) {
    if (result.target_captured) {
        transfer_ownership(location, attacker);
    } else if (result.siege_broken) {
        // Defenders won
    }
}
```

## Query State

```c
float progress = carbon_siege_get_progress(siege, id);
int32_t round = carbon_siege_get_round(siege, id);
float ratio = carbon_siege_get_force_ratio(siege, id);
```

## Reinforcements

```c
carbon_siege_reinforce_attacker(siege, id, 50);
carbon_siege_reinforce_defender(siege, id, 30);
carbon_siege_attacker_casualties(siege, id, 10);
```

## Modifiers

```c
carbon_siege_set_attack_modifier(siege, id, 1.2f);   // +20% attack
carbon_siege_set_defense_modifier(siege, id, 0.8f);  // -20% defense
```

## Retreat

```c
carbon_siege_retreat(siege, id);
```

## Configuration

```c
Carbon_SiegeConfig config = carbon_siege_default_config();
config.default_max_rounds = 30;
config.min_force_ratio = 0.5f;
config.base_damage_per_round = 15;
config.capture_threshold = 0.1f;
carbon_siege_set_config(siege, &config);
```

## Siege Statuses

`INACTIVE`, `PREPARING`, `ACTIVE`, `CAPTURED`, `BROKEN`, `RETREATED`, `TIMEOUT`
