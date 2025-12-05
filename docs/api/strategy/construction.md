# Construction Queue / Ghost Building System

Planned buildings with progress tracking before actual construction.

## Quick Start

```c
#include "carbon/construction.h"

Carbon_ConstructionQueue *queue = carbon_construction_create(32);

// Add ghost building
uint32_t ghost = carbon_construction_add_ghost(queue, 10, 20, BUILDING_FACTORY, 0);

// Or with options
uint32_t ghost2 = carbon_construction_add_ghost_ex(queue, 15, 20,
    BUILDING_POWERPLANT, 0,
    30.0f,           // Duration in seconds
    player_faction);

// Start construction
carbon_construction_start(queue, ghost);
```

## Progress

```c
// Update each frame
carbon_construction_update(queue, delta_time);

// Or add progress manually (worker-based)
carbon_construction_add_progress(queue, ghost, 0.01f);

// Query
float progress = carbon_construction_get_progress(queue, ghost);
float remaining = carbon_construction_get_remaining_time(queue, ghost);

// Speed modifier
carbon_construction_set_speed(queue, ghost, 1.5f);
```

## Completion

```c
if (carbon_construction_is_complete(queue, ghost)) {
    Carbon_Ghost *g = carbon_construction_get_ghost(queue, ghost);
    create_building(g->x, g->y, g->building_type);
    carbon_construction_remove_ghost(queue, ghost);
}
```

## Callbacks

```c
void on_done(Carbon_ConstructionQueue *q, const Carbon_Ghost *g, void *ud) {
    if (g->status == CARBON_GHOST_COMPLETE) {
        create_building(g->x, g->y, g->building_type);
    } else if (g->status == CARBON_GHOST_CANCELLED) {
        refund_cost(g->building_type);
    }
}
carbon_construction_set_callback(queue, on_done, game);
```

## Control

```c
carbon_construction_pause(queue, ghost);
carbon_construction_resume(queue, ghost);
carbon_construction_cancel_ghost(queue, ghost);
carbon_construction_complete_instant(queue, ghost);  // Cheat
```

## Ghost Statuses

`PENDING`, `CONSTRUCTING`, `COMPLETE`, `CANCELLED`, `PAUSED`
