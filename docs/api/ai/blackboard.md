# Shared Blackboard System

Cross-system communication and data sharing without direct coupling. Supports resource reservations, plan publication, and decision history.

## Quick Start

```c
#include "agentite/blackboard.h"

Agentite_Blackboard *bb = agentite_blackboard_create();

// Store typed values
agentite_blackboard_set_int(bb, "threat_level", 75);
agentite_blackboard_set_float(bb, "resources_ratio", 1.2f);
agentite_blackboard_set_bool(bb, "under_attack", false);
agentite_blackboard_set_string(bb, "target_name", "Enemy Base");
agentite_blackboard_set_ptr(bb, "primary_target", enemy_entity);
agentite_blackboard_set_vec2(bb, "rally_point", 100.0f, 200.0f);

// Read values
int threat = agentite_blackboard_get_int(bb, "threat_level");
float ratio = agentite_blackboard_get_float_or(bb, "ratio", 1.0f);  // With default
```

## Resource Reservations

Prevent double-spending by AI tracks:

```c
// Reserve resource for a track
if (agentite_blackboard_reserve(bb, "gold", 500, "military_track")) {
    // Resource reserved - other tracks see reduced available
}

int32_t available = agentite_blackboard_get_available(bb, "gold");
int32_t reserved = agentite_blackboard_get_reserved(bb, "gold");

// Release when done
agentite_blackboard_release(bb, "gold", "military_track");

// Reservations with expiration
agentite_blackboard_reserve_ex(bb, "iron", 100, "construction", 5);  // 5 turns
```

## Plan Publication

Avoid conflicting AI decisions:

```c
agentite_blackboard_publish_plan(bb, "military", "Attack sector 7");
agentite_blackboard_publish_plan_ex(bb, "expansion", "Colonize region",
                                   "sector_7", 3);  // Target + duration

if (agentite_blackboard_has_conflicting_plan(bb, "sector_7")) {
    // Another system has plans for this target
}

agentite_blackboard_cancel_plan(bb, "military");
```

## Decision History

```c
agentite_blackboard_set_turn(bb, current_turn);
agentite_blackboard_log(bb, "Decided to attack sector %d", sector);

const char *entries[10];
int count = agentite_blackboard_get_history_strings(bb, entries, 10);
```

## Change Subscriptions

```c
void on_change(Agentite_Blackboard *bb, const char *key,
               const Agentite_BBValue *old, const Agentite_BBValue *new,
               void *userdata) {
    printf("Key %s changed\n", key);
}

uint32_t sub = agentite_blackboard_subscribe(bb, "threat_level", on_change, NULL);
agentite_blackboard_subscribe(bb, NULL, on_change, NULL);  // All keys
agentite_blackboard_unsubscribe(bb, sub);
```

## Update Loop

```c
// Each turn, update expirations
agentite_blackboard_update(bb);
```

## Value Types

`INT`, `INT64`, `FLOAT`, `DOUBLE`, `BOOL`, `STRING`, `PTR`, `VEC2`, `VEC3`
