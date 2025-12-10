# Construction Queue / Ghost Building System

Planned buildings with progress tracking before actual construction.

## Quick Start

```c
#include "agentite/construction.h"

Agentite_ConstructionQueue *queue = agentite_construction_create(32);

// Add ghost building
uint32_t ghost = agentite_construction_add_ghost(queue, 10, 20, BUILDING_FACTORY, 0);

// Or with options
uint32_t ghost2 = agentite_construction_add_ghost_ex(queue, 15, 20,
    BUILDING_POWERPLANT, 0,
    30.0f,           // Duration in seconds
    player_faction);

// Start construction
agentite_construction_start(queue, ghost);
```

## Progress

```c
// Update each frame
agentite_construction_update(queue, delta_time);

// Or add progress manually (worker-based)
agentite_construction_add_progress(queue, ghost, 0.01f);

// Query
float progress = agentite_construction_get_progress(queue, ghost);
float remaining = agentite_construction_get_remaining_time(queue, ghost);

// Speed modifier
agentite_construction_set_speed(queue, ghost, 1.5f);
```

## Completion

```c
if (agentite_construction_is_complete(queue, ghost)) {
    Agentite_Ghost *g = agentite_construction_get_ghost(queue, ghost);
    create_building(g->x, g->y, g->building_type);
    agentite_construction_remove_ghost(queue, ghost);
}
```

## Callbacks

```c
void on_done(Agentite_ConstructionQueue *q, const Agentite_Ghost *g, void *ud) {
    if (g->status == AGENTITE_GHOST_COMPLETE) {
        create_building(g->x, g->y, g->building_type);
    } else if (g->status == AGENTITE_GHOST_CANCELLED) {
        refund_cost(g->building_type);
    }
}
agentite_construction_set_callback(queue, on_done, game);
```

## Control

```c
agentite_construction_pause(queue, ghost);
agentite_construction_resume(queue, ghost);
agentite_construction_cancel_ghost(queue, ghost);
agentite_construction_complete_instant(queue, ghost);  // Cheat
```

## Ghost Statuses

`PENDING`, `CONSTRUCTING`, `COMPLETE`, `CANCELLED`, `PAUSED`
