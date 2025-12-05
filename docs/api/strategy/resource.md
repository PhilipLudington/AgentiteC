# Resource Economy System

Resource tracking with production, consumption, and capacity management.

## Quick Start

```c
#include "carbon/resource.h"

Carbon_ResourceManager *resources = carbon_resource_create();

// Register resource types
carbon_resource_register(resources, RESOURCE_GOLD, "Gold", 0, 10000);
carbon_resource_register(resources, RESOURCE_FOOD, "Food", 0, 5000);

// Initialize per-faction
Carbon_ResourceState state;
carbon_resource_state_init(&state);
carbon_resource_set(&state, RESOURCE_GOLD, 1000);
```

## Operations

```c
// Add/remove
carbon_resource_add(&state, RESOURCE_GOLD, 100);
carbon_resource_remove(&state, RESOURCE_GOLD, 50);

// Check availability
if (carbon_resource_has(&state, RESOURCE_GOLD, 500)) { }

// Get current/capacity
int32_t gold = carbon_resource_get(&state, RESOURCE_GOLD);
int32_t cap = carbon_resource_get_capacity(&state, RESOURCE_GOLD);
```

## Production System

```c
// Set production/consumption rates
carbon_resource_set_production(&state, RESOURCE_GOLD, 50);   // +50/turn
carbon_resource_set_consumption(&state, RESOURCE_GOLD, 20);  // -20/turn

// Apply each turn
carbon_resource_apply_production(&state, resources);

// Query rates
int32_t net = carbon_resource_get_net(&state, RESOURCE_GOLD);  // +30
```

## Event Integration

```c
Carbon_ResourceManager *resources = carbon_resource_create_with_events(events);
// Emits CARBON_EVENT_RESOURCE_CHANGED, RESOURCE_DEPLETED, RESOURCE_THRESHOLD
```

See also: [Rate Tracking](rate.md) for rolling metrics and graphs.
