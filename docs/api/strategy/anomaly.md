# Anomaly / Discovery System

Discoverable points of interest with research mechanics.

## Quick Start

```c
#include "agentite/anomaly.h"

Agentite_AnomalyRegistry *registry = agentite_anomaly_registry_create();

// Define anomaly type
Agentite_AnomalyTypeDef ruins = agentite_anomaly_type_default();
strcpy(ruins.id, "ancient_ruins");
strcpy(ruins.name, "Ancient Ruins");
ruins.rarity = AGENTITE_ANOMALY_UNCOMMON;
ruins.research_time = 10.0f;
ruins.reward_count = 1;
ruins.rewards[0] = (Agentite_AnomalyReward){
    .type = AGENTITE_ANOMALY_REWARD_TECH,
    .amount = 50,
};
int ruins_type = agentite_anomaly_register_type(registry, &ruins);

Agentite_AnomalyManager *anomalies = agentite_anomaly_manager_create(registry);
```

## Spawning

```c
uint32_t id = agentite_anomaly_spawn(anomalies, ruins_type, 50, 50, 0);
uint32_t random = agentite_anomaly_spawn_random(anomalies, x, y, AGENTITE_ANOMALY_RARE);
```

## Discovery & Research

```c
// Discovery (when unit explores)
agentite_anomaly_discover(anomalies, id, player_faction);

// Start research
agentite_anomaly_start_research(anomalies, id, player_faction, scientist);

// Update (auto-progress)
agentite_anomaly_update(anomalies, delta_time);

// Or manual progress
if (agentite_anomaly_add_progress(anomalies, id, 1.0f)) {
    printf("Complete!\n");
}
```

## Collect Rewards

```c
if (agentite_anomaly_is_complete(anomalies, id)) {
    Agentite_AnomalyResult result = agentite_anomaly_collect_rewards(anomalies, id);
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
