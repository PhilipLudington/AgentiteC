# Anomaly / Discovery System

Discoverable points of interest with research mechanics.

## Quick Start

```c
#include "carbon/anomaly.h"

Carbon_AnomalyRegistry *registry = carbon_anomaly_registry_create();

// Define anomaly type
Carbon_AnomalyTypeDef ruins = carbon_anomaly_type_default();
strcpy(ruins.id, "ancient_ruins");
strcpy(ruins.name, "Ancient Ruins");
ruins.rarity = CARBON_ANOMALY_UNCOMMON;
ruins.research_time = 10.0f;
ruins.reward_count = 1;
ruins.rewards[0] = (Carbon_AnomalyReward){
    .type = CARBON_ANOMALY_REWARD_TECH,
    .amount = 50,
};
int ruins_type = carbon_anomaly_register_type(registry, &ruins);

Carbon_AnomalyManager *anomalies = carbon_anomaly_manager_create(registry);
```

## Spawning

```c
uint32_t id = carbon_anomaly_spawn(anomalies, ruins_type, 50, 50, 0);
uint32_t random = carbon_anomaly_spawn_random(anomalies, x, y, CARBON_ANOMALY_RARE);
```

## Discovery & Research

```c
// Discovery (when unit explores)
carbon_anomaly_discover(anomalies, id, player_faction);

// Start research
carbon_anomaly_start_research(anomalies, id, player_faction, scientist);

// Update (auto-progress)
carbon_anomaly_update(anomalies, delta_time);

// Or manual progress
if (carbon_anomaly_add_progress(anomalies, id, 1.0f)) {
    printf("Complete!\n");
}
```

## Collect Rewards

```c
if (carbon_anomaly_is_complete(anomalies, id)) {
    Carbon_AnomalyResult result = carbon_anomaly_collect_rewards(anomalies, id);
    for (int i = 0; i < result.reward_count; i++) {
        apply_reward(&result.rewards[i]);
    }
}
```

## Rarity Tiers

| Tier | Default Weight |
|------|----------------|
| `COMMON` | ~60% |
| `UNCOMMON` | ~25% |
| `RARE` | ~12% |
| `LEGENDARY` | ~3% |

## Reward Types

`RESOURCES`, `TECH`, `UNIT`, `MODIFIER`, `ARTIFACT`, `MAP`, `CUSTOM`

## Anomaly Statuses

`UNDISCOVERED`, `DISCOVERED`, `RESEARCHING`, `COMPLETED`, `DEPLETED`
