# Shared Blackboard System

Cross-system communication and data sharing without direct coupling. Supports resource reservations, plan publication, and decision history.

## Quick Start

```c
#include "carbon/blackboard.h"

Carbon_Blackboard *bb = carbon_blackboard_create();

// Store typed values
carbon_blackboard_set_int(bb, "threat_level", 75);
carbon_blackboard_set_float(bb, "resources_ratio", 1.2f);
carbon_blackboard_set_bool(bb, "under_attack", false);
carbon_blackboard_set_string(bb, "target_name", "Enemy Base");
carbon_blackboard_set_ptr(bb, "primary_target", enemy_entity);
carbon_blackboard_set_vec2(bb, "rally_point", 100.0f, 200.0f);

// Read values
int threat = carbon_blackboard_get_int(bb, "threat_level");
float ratio = carbon_blackboard_get_float_or(bb, "ratio", 1.0f);  // With default
```

## Resource Reservations

Prevent double-spending by AI tracks:

```c
// Reserve resource for a track
if (carbon_blackboard_reserve(bb, "gold", 500, "military_track")) {
    // Resource reserved - other tracks see reduced available
}

int32_t available = carbon_blackboard_get_available(bb, "gold");
int32_t reserved = carbon_blackboard_get_reserved(bb, "gold");

// Release when done
carbon_blackboard_release(bb, "gold", "military_track");

// Reservations with expiration
carbon_blackboard_reserve_ex(bb, "iron", 100, "construction", 5);  // 5 turns
```

## Plan Publication

Avoid conflicting AI decisions:

```c
carbon_blackboard_publish_plan(bb, "military", "Attack sector 7");
carbon_blackboard_publish_plan_ex(bb, "expansion", "Colonize region",
                                   "sector_7", 3);  // Target + duration

if (carbon_blackboard_has_conflicting_plan(bb, "sector_7")) {
    // Another system has plans for this target
}

carbon_blackboard_cancel_plan(bb, "military");
```

## Decision History

```c
carbon_blackboard_set_turn(bb, current_turn);
carbon_blackboard_log(bb, "Decided to attack sector %d", sector);

const char *entries[10];
int count = carbon_blackboard_get_history_strings(bb, entries, 10);
```

## Change Subscriptions

```c
void on_change(Carbon_Blackboard *bb, const char *key,
               const Carbon_BBValue *old, const Carbon_BBValue *new,
               void *userdata) {
    printf("Key %s changed\n", key);
}

uint32_t sub = carbon_blackboard_subscribe(bb, "threat_level", on_change, NULL);
carbon_blackboard_subscribe(bb, NULL, on_change, NULL);  // All keys
carbon_blackboard_unsubscribe(bb, sub);
```

## Update Loop

```c
// Each turn, update expirations
carbon_blackboard_update(bb);
```

## Value Types

`INT`, `INT64`, `FLOAT`, `DOUBLE`, `BOOL`, `STRING`, `PTR`, `VEC2`, `VEC3`
