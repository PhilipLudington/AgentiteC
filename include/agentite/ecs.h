/**
 * Carbon Entity Component System
 *
 * A thin wrapper around Flecs (https://www.flecs.dev/) providing:
 * - Simplified initialization and cleanup
 * - Pre-defined common game components
 * - Convenience macros for component operations
 *
 * Usage:
 *   Agentite_World *world = agentite_ecs_init();
 *   agentite_ecs_register_components(world);
 *
 *   ecs_entity_t player = agentite_ecs_entity_new_named(world, "Player");
 *   AGENTITE_ECS_SET(world, player, C_Position, {100, 200});
 *   AGENTITE_ECS_SET(world, player, C_Velocity, {1, 0});
 *
 *   // Game loop:
 *   agentite_ecs_progress(world, delta_time);
 *
 *   agentite_ecs_shutdown(world);
 *
 * For advanced Flecs features, use agentite_ecs_get_world() to get the
 * underlying ecs_world_t pointer.
 */
#ifndef AGENTITE_ECS_H
#define AGENTITE_ECS_H

#include "flecs.h"
#include <stdbool.h>

/* Forward declaration */
typedef struct Agentite_Engine Agentite_Engine;

/* Opaque ECS world wrapper */
typedef struct Agentite_World Agentite_World;

/* ============================================================================
 * Common Game Components
 *
 * Pre-defined components for common game object properties.
 * Use agentite_ecs_register_components() to register these.
 * ============================================================================ */

/**
 * World position component.
 * Uses world coordinates (not screen coordinates).
 */
typedef struct {
    float x, y;  /* World position */
} C_Position;

/**
 * Velocity component for physics/movement.
 * Units are typically world units per second.
 */
typedef struct {
    float vx, vy;  /* Velocity in world units/second */
} C_Velocity;

/**
 * Size component for collision/rendering bounds.
 */
typedef struct {
    float width, height;  /* Dimensions in world units */
} C_Size;

/**
 * Color component for tinting/rendering.
 * Values are normalized 0.0-1.0.
 */
typedef struct {
    float r, g, b, a;  /* RGBA, normalized 0.0-1.0 */
} C_Color;

/**
 * Name component for debugging/lookup.
 * Note: The string pointer must remain valid for entity lifetime.
 */
typedef struct {
    const char *name;  /* Entity name (borrowed pointer) */
} C_Name;

/**
 * Active flag component.
 * Use to enable/disable entity processing without deleting.
 */
typedef struct {
    bool active;  /* true = entity is active and should be processed */
} C_Active;

/**
 * Health component for damageable entities.
 */
typedef struct {
    int health;      /* Current health */
    int max_health;  /* Maximum health */
} C_Health;

/**
 * Render layer component for draw ordering.
 * Lower layers are drawn first (behind higher layers).
 */
typedef struct {
    int layer;  /* Render layer (0 = background, higher = foreground) */
} C_RenderLayer;

/* Component ID declarations (extern for use across translation units) */
extern ECS_COMPONENT_DECLARE(C_Position);
extern ECS_COMPONENT_DECLARE(C_Velocity);
extern ECS_COMPONENT_DECLARE(C_Size);
extern ECS_COMPONENT_DECLARE(C_Color);
extern ECS_COMPONENT_DECLARE(C_Name);
extern ECS_COMPONENT_DECLARE(C_Active);
extern ECS_COMPONENT_DECLARE(C_Health);
extern ECS_COMPONENT_DECLARE(C_RenderLayer);

/* ============================================================================
 * Lifecycle Functions
 * ============================================================================ */

/**
 * Initialize the ECS world.
 * Caller OWNS the returned pointer and MUST call agentite_ecs_shutdown().
 *
 * @return New ECS world, or NULL on failure
 */
Agentite_World *agentite_ecs_init(void);

/**
 * Shutdown the ECS world and free all resources.
 * All entities and components are destroyed.
 * Safe to call with NULL.
 *
 * @param world ECS world to destroy
 */
void agentite_ecs_shutdown(Agentite_World *world);

/**
 * Get the underlying Flecs world pointer.
 * Use for advanced Flecs features not exposed by the wrapper.
 * The returned pointer is valid until agentite_ecs_shutdown() is called.
 *
 * @param world ECS world wrapper
 * @return Flecs world pointer (borrowed, do not free)
 */
ecs_world_t *agentite_ecs_get_world(Agentite_World *world);

/**
 * Progress the ECS world (run all systems).
 * Call once per frame in your game loop.
 *
 * @param world ECS world
 * @param delta_time Time since last frame in seconds
 * @return true if the world should continue running
 */
bool agentite_ecs_progress(Agentite_World *world, float delta_time);

/* ============================================================================
 * Entity Functions
 * ============================================================================ */

/**
 * Create a new entity.
 *
 * @param world ECS world
 * @return New entity ID, or 0 on failure
 */
ecs_entity_t agentite_ecs_entity_new(Agentite_World *world);

/**
 * Create a new named entity.
 * Named entities can be looked up by name using ecs_lookup().
 *
 * @param world ECS world
 * @param name Entity name (copied internally)
 * @return New entity ID, or 0 on failure
 */
ecs_entity_t agentite_ecs_entity_new_named(Agentite_World *world, const char *name);

/**
 * Delete an entity and all its components.
 * Safe to call during system iteration (deletion is deferred).
 *
 * @param world ECS world
 * @param entity Entity to delete
 */
void agentite_ecs_entity_delete(Agentite_World *world, ecs_entity_t entity);

/**
 * Check if an entity is alive (not deleted).
 *
 * @param world ECS world
 * @param entity Entity to check
 * @return true if entity exists and is alive
 */
bool agentite_ecs_entity_is_alive(Agentite_World *world, ecs_entity_t entity);

/**
 * Register the common game components (C_Position, C_Velocity, etc.).
 * Call this once after agentite_ecs_init() before using components.
 *
 * @param world ECS world
 */
void agentite_ecs_register_components(Agentite_World *world);

/* ============================================================================
 * Convenience Macros
 *
 * These macros wrap common Flecs operations with the Agentite_World type.
 * ============================================================================ */

/**
 * Set a component value on an entity.
 * Creates the component if it doesn't exist.
 *
 * Example:
 *   AGENTITE_ECS_SET(world, player, C_Position, {100.0f, 200.0f});
 */
#define AGENTITE_ECS_SET(world, entity, T, ...) \
    ecs_set(agentite_ecs_get_world(world), entity, T, __VA_ARGS__)

/**
 * Get a const pointer to an entity's component.
 * Returns NULL if entity doesn't have the component.
 *
 * Example:
 *   const C_Position *pos = AGENTITE_ECS_GET(world, player, C_Position);
 */
#define AGENTITE_ECS_GET(world, entity, T) \
    ecs_get(agentite_ecs_get_world(world), entity, T)

/**
 * Add a component to an entity (default-initialized).
 * No-op if entity already has the component.
 *
 * Example:
 *   AGENTITE_ECS_ADD(world, entity, C_Active);
 */
#define AGENTITE_ECS_ADD(world, entity, T) \
    ecs_add(agentite_ecs_get_world(world), entity, T)

/**
 * Remove a component from an entity.
 * No-op if entity doesn't have the component.
 *
 * Example:
 *   AGENTITE_ECS_REMOVE(world, entity, C_Velocity);
 */
#define AGENTITE_ECS_REMOVE(world, entity, T) \
    ecs_remove(agentite_ecs_get_world(world), entity, T)

/**
 * Check if an entity has a component.
 *
 * Example:
 *   if (AGENTITE_ECS_HAS(world, entity, C_Health)) { ... }
 */
#define AGENTITE_ECS_HAS(world, entity, T) \
    ecs_has(agentite_ecs_get_world(world), entity, T)

/**
 * Register a system to run during a specific phase.
 *
 * Common phases:
 *   EcsOnUpdate  - Main update phase (most game logic)
 *   EcsPreUpdate - Before main update (input processing)
 *   EcsPostUpdate - After main update (cleanup, collision)
 *
 * Example:
 *   void MovementSystem(ecs_iter_t *it) {
 *       C_Position *pos = ecs_field(it, C_Position, 0);
 *       C_Velocity *vel = ecs_field(it, C_Velocity, 1);
 *       for (int i = 0; i < it->count; i++) {
 *           pos[i].x += vel[i].vx * it->delta_time;
 *           pos[i].y += vel[i].vy * it->delta_time;
 *       }
 *   }
 *   AGENTITE_ECS_SYSTEM(world, MovementSystem, EcsOnUpdate,
 *       .query.terms = {{ .id = ecs_id(C_Position) }, { .id = ecs_id(C_Velocity) }});
 */
#define AGENTITE_ECS_SYSTEM(world, name, phase, ...) \
    ecs_system(agentite_ecs_get_world(world), { \
        .entity = ecs_entity(agentite_ecs_get_world(world), { .name = #name, .add = ecs_ids(ecs_dependson(phase)) }), \
        .callback = name, \
        __VA_ARGS__ \
    })

/**
 * Create a query for iterating over entities with specific components.
 *
 * Example:
 *   ecs_query_t *q = AGENTITE_ECS_QUERY(world, {
 *       .terms = {{ .id = ecs_id(C_Position) }, { .id = ecs_id(C_Velocity) }}
 *   });
 */
#define AGENTITE_ECS_QUERY(world, ...) \
    ecs_query(agentite_ecs_get_world(world), __VA_ARGS__)

#endif // AGENTITE_ECS_H
