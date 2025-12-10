#ifndef AGENTITE_ECS_H
#define AGENTITE_ECS_H

#include "flecs.h"
#include <stdbool.h>

// Forward declaration
typedef struct Agentite_Engine Agentite_Engine;

// ECS World wrapper
typedef struct Agentite_World Agentite_World;

// Common game components
typedef struct {
    float x, y;
} C_Position;

typedef struct {
    float vx, vy;
} C_Velocity;

typedef struct {
    float width, height;
} C_Size;

typedef struct {
    float r, g, b, a;
} C_Color;

typedef struct {
    const char *name;
} C_Name;

typedef struct {
    bool active;
} C_Active;

typedef struct {
    int health;
    int max_health;
} C_Health;

typedef struct {
    int layer;
} C_RenderLayer;

// Declare component IDs (extern for use across translation units)
extern ECS_COMPONENT_DECLARE(C_Position);
extern ECS_COMPONENT_DECLARE(C_Velocity);
extern ECS_COMPONENT_DECLARE(C_Size);
extern ECS_COMPONENT_DECLARE(C_Color);
extern ECS_COMPONENT_DECLARE(C_Name);
extern ECS_COMPONENT_DECLARE(C_Active);
extern ECS_COMPONENT_DECLARE(C_Health);
extern ECS_COMPONENT_DECLARE(C_RenderLayer);

// ECS lifecycle functions
Agentite_World *agentite_ecs_init(void);
void agentite_ecs_shutdown(Agentite_World *world);

// Get the underlying Flecs world (for advanced usage)
ecs_world_t *agentite_ecs_get_world(Agentite_World *world);

// World progression (call each frame)
bool agentite_ecs_progress(Agentite_World *world, float delta_time);

// Entity creation
ecs_entity_t agentite_ecs_entity_new(Agentite_World *world);
ecs_entity_t agentite_ecs_entity_new_named(Agentite_World *world, const char *name);
void agentite_ecs_entity_delete(Agentite_World *world, ecs_entity_t entity);
bool agentite_ecs_entity_is_alive(Agentite_World *world, ecs_entity_t entity);

// Component registration (call before using components)
void agentite_ecs_register_components(Agentite_World *world);

// Convenience macros for component operations
#define AGENTITE_ECS_SET(world, entity, T, ...) \
    ecs_set(agentite_ecs_get_world(world), entity, T, __VA_ARGS__)

#define AGENTITE_ECS_GET(world, entity, T) \
    ecs_get(agentite_ecs_get_world(world), entity, T)

#define AGENTITE_ECS_ADD(world, entity, T) \
    ecs_add(agentite_ecs_get_world(world), entity, T)

#define AGENTITE_ECS_REMOVE(world, entity, T) \
    ecs_remove(agentite_ecs_get_world(world), entity, T)

#define AGENTITE_ECS_HAS(world, entity, T) \
    ecs_has(agentite_ecs_get_world(world), entity, T)

// System registration helper
#define AGENTITE_ECS_SYSTEM(world, name, phase, ...) \
    ecs_system(agentite_ecs_get_world(world), { \
        .entity = ecs_entity(agentite_ecs_get_world(world), { .name = #name, .add = ecs_ids(ecs_dependson(phase)) }), \
        .callback = name, \
        __VA_ARGS__ \
    })

// Query helpers
#define AGENTITE_ECS_QUERY(world, ...) \
    ecs_query(agentite_ecs_get_world(world), __VA_ARGS__)

#endif // AGENTITE_ECS_H
