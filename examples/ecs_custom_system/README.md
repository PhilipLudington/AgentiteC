# ECS Custom System Example

Deep dive into the Entity Component System (Flecs) patterns used in Agentite.

## Features Shown

- Defining custom components with `ECS_COMPONENT_DECLARE` / `ECS_COMPONENT_DEFINE`
- Creating systems with different phases (`EcsOnUpdate`, `EcsPostUpdate`)
- Querying entities with `ecs_query()` and iterating results
- Dynamic entity creation and destruction
- Component data access via `ecs_field()`

## What It Does

Creates a particle simulation with:
- **Emitters**: Spawn particles at regular intervals
- **Particles**: Move with velocity, affected by gravity, fade out over time
- **Automatic cleanup**: Particles deleted when lifetime expires

## Key Concepts

### Defining Components

```c
/* Component is just a plain struct */
typedef struct {
    float remaining;
    float initial;
} C_Lifetime;

/* Declare the component ID (in header or at file scope) */
ECS_COMPONENT_DECLARE(C_Lifetime);

/* Define the component (once, after ECS init) */
ECS_COMPONENT_DEFINE(world, C_Lifetime);
```

### Creating Systems

```c
/* System function signature */
static void PhysicsSystem(ecs_iter_t *it)
{
    /* Get component arrays for matched entities */
    C_Position *pos = ecs_field(it, C_Position, 0);  /* First query term */
    C_Velocity *vel = ecs_field(it, C_Velocity, 1);  /* Second query term */

    /* Process all matched entities */
    for (int i = 0; i < it->count; i++) {
        pos[i].x += vel[i].vx * it->delta_time;
        pos[i].y += vel[i].vy * it->delta_time;
    }
}

/* Register system to run during EcsOnUpdate phase */
ECS_SYSTEM(world, PhysicsSystem, EcsOnUpdate, C_Position, C_Velocity);
```

### System Phases

Systems run in phase order during `ecs_progress()`:

| Phase | Purpose | Example |
|-------|---------|---------|
| `EcsPreUpdate` | Input processing, preparation | InputSystem |
| `EcsOnUpdate` | Main game logic | PhysicsSystem, AISystem |
| `EcsPostUpdate` | Cleanup, collision response | LifetimeSystem |

### Creating Entities with Components

```c
ecs_entity_t particle = ecs_new(world);

ecs_set(world, particle, C_Position, {100, 200});
ecs_set(world, particle, C_Velocity, {50, -100});
ecs_set(world, particle, C_Lifetime, {3.0f, 3.0f});
```

### Querying Entities Manually

For rendering or other non-system operations:

```c
ecs_query_t *q = ecs_query(world, {
    .terms = {
        { .id = ecs_id(C_Position) },
        { .id = ecs_id(C_Color) }
    }
});

ecs_iter_t it = ecs_query_iter(world, q);
while (ecs_query_next(&it)) {
    C_Position *pos = ecs_field(&it, C_Position, 0);
    C_Color *col = ecs_field(&it, C_Color, 1);

    for (int i = 0; i < it.count; i++) {
        // Render entity at pos[i] with col[i]
    }
}

ecs_query_fini(q);  /* Clean up query */
```

### Deleting Entities

```c
/* Safe to call during system iteration (deferred until end of progress) */
ecs_delete(it->world, it->entities[i]);
```

## Common Pitfalls

1. **Field indices start at 0**, not 1
2. **Deletions are deferred** during system iteration
3. **Component pointers invalidate** when world is modified
4. **Systems run in registration order** within a phase

## Build

From the project root:

```bash
make examples
```
