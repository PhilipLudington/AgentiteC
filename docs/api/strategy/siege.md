# Siege / Bombardment System

Sustained attack mechanics over multiple rounds for location assault.

## Quick Start

```c
#include "agentite/siege.h"

Agentite_SiegeManager *siege = agentite_siege_create();

// Set callbacks
agentite_siege_set_defense_callback(siege, get_defense, game);
agentite_siege_set_defender_callback(siege, get_defender, game);

// Begin siege
if (agentite_siege_can_begin(siege, attacker, location, force)) {
    uint32_t id = agentite_siege_begin(siege, attacker, location, force);
}
```

## Processing Rounds

```c
Agentite_SiegeRoundResult result;
if (agentite_siege_process_round(siege, id, &result)) {
    if (result.target_captured) {
        transfer_ownership(location, attacker);
    } else if (result.siege_broken) {
        // Defenders won
    }
}
```

## Query State

```c
float progress = agentite_siege_get_progress(siege, id);
int32_t round = agentite_siege_get_round(siege, id);
float ratio = agentite_siege_get_force_ratio(siege, id);
```

## Reinforcements

```c
agentite_siege_reinforce_attacker(siege, id, 50);
agentite_siege_reinforce_defender(siege, id, 30);
agentite_siege_attacker_casualties(siege, id, 10);
```

## Modifiers

```c
agentite_siege_set_attack_modifier(siege, id, 1.2f);   // +20% attack
agentite_siege_set_defense_modifier(siege, id, 0.8f);  // -20% defense
```

## Retreat

```c
agentite_siege_retreat(siege, id);
```

## Configuration

```c
Agentite_SiegeConfig config = agentite_siege_default_config();
config.default_max_rounds = 30;
config.min_force_ratio = 0.5f;
config.base_damage_per_round = 15;
config.capture_threshold = 0.1f;
agentite_siege_set_config(siege, &config);
```

## Siege Statuses

`INACTIVE`, `PREPARING`, `ACTIVE`, `CAPTURED`, `BROKEN`, `RETREATED`, `TIMEOUT`
