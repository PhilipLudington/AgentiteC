# Entity Component System (ECS)

Wrapper around Flecs v4.0.0 for game entity management.

## Quick Start

```c
#include "carbon/ecs.h"

// Initialize
Carbon_World *ecs_world = carbon_ecs_init();
ecs_world_t *w = carbon_ecs_get_world(ecs_world);

// Create entities with components
ecs_entity_t player = carbon_ecs_entity_new_named(ecs_world, "Player");
ecs_set(w, player, C_Position, { .x = 100.0f, .y = 100.0f });
ecs_set(w, player, C_Velocity, { .vx = 0.0f, .vy = 0.0f });
ecs_set(w, player, C_Health, { .health = 100, .max_health = 100 });

// In game loop
carbon_ecs_progress(ecs_world, delta_time);

// Query components
const C_Position *pos = ecs_get(w, player, C_Position);

// Cleanup
carbon_ecs_shutdown(ecs_world);
```

## Built-in Components

| Component | Fields |
|-----------|--------|
| `C_Position` | `x`, `y` |
| `C_Velocity` | `vx`, `vy` |
| `C_Size` | `width`, `height` |
| `C_Color` | `r`, `g`, `b`, `a` |
| `C_Name` | `name[64]` |
| `C_Active` | `active` |
| `C_Health` | `health`, `max_health` |
| `C_RenderLayer` | `layer` |

## Adding Custom Components

1. Define in `src/game/components.h`:
```c
typedef struct C_Projectile {
    ecs_entity_t owner;
    float lifetime;
} C_Projectile;
ECS_COMPONENT_DECLARE(C_Projectile);
```

2. Register in `src/game/components.c`:
```c
ECS_COMPONENT_DEFINE(world, C_Projectile);
```

## Creating Systems

```c
void MovementSystem(ecs_iter_t *it) {
    C_Position *pos = ecs_field(it, C_Position, 0);
    C_Velocity *vel = ecs_field(it, C_Velocity, 1);

    for (int i = 0; i < it->count; i++) {
        pos[i].x += vel[i].vx * it->delta_time;
        pos[i].y += vel[i].vy * it->delta_time;
    }
}

// Register system
ECS_SYSTEM(world, MovementSystem, EcsOnUpdate, C_Position, C_Velocity);
```

## Queries

```c
// Create query
ecs_query_t *q = ecs_query(world, {
    .terms = {
        { .id = ecs_id(C_Position) },
        { .id = ecs_id(C_Health) }
    }
});

// Iterate
ecs_iter_t it = ecs_query_iter(world, q);
while (ecs_query_next(&it)) {
    C_Position *p = ecs_field(&it, C_Position, 0);
    C_Health *h = ecs_field(&it, C_Health, 1);
    for (int i = 0; i < it.count; i++) {
        // Process entities...
    }
}
```

## Entity Operations

```c
// Create/destroy
ecs_entity_t e = ecs_new(world);
ecs_delete(world, e);

// Add/remove components
ecs_add(world, e, C_Active);
ecs_remove(world, e, C_Active);

// Check component
if (ecs_has(world, e, C_Health)) { }

// Get/set
const C_Health *h = ecs_get(world, e, C_Health);
ecs_set(world, e, C_Health, { .health = 50 });
```

## Notes

- See [Flecs documentation](https://www.flecs.dev/flecs/) for advanced features
- Systems are processed each frame via `carbon_ecs_progress()`
