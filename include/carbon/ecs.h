#ifndef CARBON_ECS_H
#define CARBON_ECS_H

#include "flecs.h"
#include <stdbool.h>

// Forward declaration
typedef struct Carbon_Engine Carbon_Engine;

// ECS World wrapper
typedef struct Carbon_World Carbon_World;

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
Carbon_World *carbon_ecs_init(void);
void carbon_ecs_shutdown(Carbon_World *world);

// Get the underlying Flecs world (for advanced usage)
ecs_world_t *carbon_ecs_get_world(Carbon_World *world);

// World progression (call each frame)
bool carbon_ecs_progress(Carbon_World *world, float delta_time);

// Entity creation
ecs_entity_t carbon_ecs_entity_new(Carbon_World *world);
ecs_entity_t carbon_ecs_entity_new_named(Carbon_World *world, const char *name);
void carbon_ecs_entity_delete(Carbon_World *world, ecs_entity_t entity);
bool carbon_ecs_entity_is_alive(Carbon_World *world, ecs_entity_t entity);

// Component registration (call before using components)
void carbon_ecs_register_components(Carbon_World *world);

// Convenience macros for component operations
#define CARBON_ECS_SET(world, entity, T, ...) \
    ecs_set(carbon_ecs_get_world(world), entity, T, __VA_ARGS__)

#define CARBON_ECS_GET(world, entity, T) \
    ecs_get(carbon_ecs_get_world(world), entity, T)

#define CARBON_ECS_ADD(world, entity, T) \
    ecs_add(carbon_ecs_get_world(world), entity, T)

#define CARBON_ECS_REMOVE(world, entity, T) \
    ecs_remove(carbon_ecs_get_world(world), entity, T)

#define CARBON_ECS_HAS(world, entity, T) \
    ecs_has(carbon_ecs_get_world(world), entity, T)

// System registration helper
#define CARBON_ECS_SYSTEM(world, name, phase, ...) \
    ecs_system(carbon_ecs_get_world(world), { \
        .entity = ecs_entity(carbon_ecs_get_world(world), { .name = #name, .add = ecs_ids(ecs_dependson(phase)) }), \
        .callback = name, \
        __VA_ARGS__ \
    })

// Query helpers
#define CARBON_ECS_QUERY(world, ...) \
    ecs_query(carbon_ecs_get_world(world), __VA_ARGS__)

#endif // CARBON_ECS_H
