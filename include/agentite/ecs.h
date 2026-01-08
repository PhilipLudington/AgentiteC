/**
 * @file ecs.h
 * @brief Entity Component System wrapper around Flecs.
 *
 * This module provides a simplified wrapper around Flecs (https://www.flecs.dev/)
 * for game development. It includes:
 * - Simplified initialization and cleanup
 * - Pre-defined common game components (position, velocity, health, etc.)
 * - Convenience macros for component operations
 * - Direct access to Flecs for advanced features
 *
 * @section ecs_usage Basic Usage
 * @code
 * Agentite_World *world = agentite_ecs_init();
 * agentite_ecs_register_components(world);
 *
 * ecs_entity_t player = agentite_ecs_entity_new_named(world, "Player");
 * AGENTITE_ECS_SET(world, player, C_Position, {100, 200});
 * AGENTITE_ECS_SET(world, player, C_Velocity, {1, 0});
 *
 * // Game loop:
 * while (running) {
 *     agentite_ecs_progress(world, delta_time);
 * }
 *
 * agentite_ecs_shutdown(world);
 * @endcode
 *
 * @section ecs_thread_safety Thread Safety
 * - World creation/destruction: NOT thread-safe (main thread only)
 * - System execution: Managed by Flecs (see Flecs docs for multi-threading)
 * - Entity creation during iteration: Deferred automatically
 * - Component pointers may invalidate after world modifications
 *
 * @section ecs_advanced Advanced Features
 * For advanced Flecs features (observers, relationships, prefabs, etc.),
 * use agentite_ecs_get_world() to access the underlying ecs_world_t pointer.
 *
 * @see https://www.flecs.dev/flecs/ for full Flecs documentation
 */
#ifndef AGENTITE_ECS_H
#define AGENTITE_ECS_H

#include "flecs.h"
#include <stdbool.h>

/** @brief Forward declaration for engine type (see engine.h) */
typedef struct Agentite_Engine Agentite_Engine;

/**
 * @brief Opaque ECS world wrapper.
 *
 * Wraps the Flecs ecs_world_t with Agentite-specific initialization.
 * Created via agentite_ecs_init(), destroyed via agentite_ecs_shutdown().
 *
 * Use agentite_ecs_get_world() to access the underlying Flecs world
 * for advanced operations.
 */
typedef struct Agentite_World Agentite_World;

/** @defgroup ecs_components Common Game Components
 *
 * Pre-defined components for common game object properties.
 * Register these with agentite_ecs_register_components() before use.
 *
 * @{
 */

/**
 * @brief World position component.
 *
 * Stores an entity's position in world coordinates (not screen coordinates).
 * Typically used with C_Velocity for movement systems.
 */
typedef struct {
    float x;    /**< X position in world units */
    float y;    /**< Y position in world units */
} C_Position;

/**
 * @brief Velocity component for physics/movement.
 *
 * Stores velocity for entities that move. Units are world units per second.
 * Commonly paired with C_Position in movement systems.
 */
typedef struct {
    float vx;   /**< X velocity in world units/second */
    float vy;   /**< Y velocity in world units/second */
} C_Velocity;

/**
 * @brief Size component for collision/rendering bounds.
 *
 * Defines the dimensions of an entity in world units.
 * Used for collision detection and sprite scaling.
 */
typedef struct {
    float width;    /**< Width in world units */
    float height;   /**< Height in world units */
} C_Size;

/**
 * @brief Color component for tinting/rendering.
 *
 * RGBA color values normalized to 0.0-1.0 range.
 * Used for sprite tinting and effects.
 */
typedef struct {
    float r;    /**< Red component (0.0-1.0) */
    float g;    /**< Green component (0.0-1.0) */
    float b;    /**< Blue component (0.0-1.0) */
    float a;    /**< Alpha component (0.0-1.0, 1.0 = opaque) */
} C_Color;

/**
 * @brief Name component for debugging/lookup.
 *
 * Associates a human-readable name with an entity.
 *
 * @warning The string pointer must remain valid for the entity's lifetime.
 *          The component does not own or copy the string.
 */
typedef struct {
    const char *name;   /**< Entity name (borrowed pointer, not owned) */
} C_Name;

/**
 * @brief Active flag component.
 *
 * Used to enable/disable entity processing without deleting the entity.
 * Useful for object pooling or temporarily disabling entities.
 */
typedef struct {
    bool active;    /**< true = entity should be processed, false = skip */
} C_Active;

/**
 * @brief Health component for damageable entities.
 *
 * Tracks current and maximum health for entities that can take damage.
 */
typedef struct {
    int health;         /**< Current health points */
    int max_health;     /**< Maximum health points */
} C_Health;

/**
 * @brief Render layer component for draw ordering.
 *
 * Controls the z-order for rendering. Lower values are drawn first
 * (appear behind), higher values are drawn last (appear in front).
 */
typedef struct {
    int layer;  /**< Render layer (0 = background, higher = foreground) */
} C_RenderLayer;

/** @name Component ID Declarations
 *  External declarations for component IDs (use across translation units).
 *  @{
 */
extern ECS_COMPONENT_DECLARE(C_Position);
extern ECS_COMPONENT_DECLARE(C_Velocity);
extern ECS_COMPONENT_DECLARE(C_Size);
extern ECS_COMPONENT_DECLARE(C_Color);
extern ECS_COMPONENT_DECLARE(C_Name);
extern ECS_COMPONENT_DECLARE(C_Active);
extern ECS_COMPONENT_DECLARE(C_Health);
extern ECS_COMPONENT_DECLARE(C_RenderLayer);
/** @} */

/** @} */ /* end of ecs_components */

/** @defgroup ecs_lifecycle Lifecycle Functions
 *  @{ */

/**
 * @brief Initialize the ECS world.
 *
 * Creates a new ECS world with default configuration. Call
 * agentite_ecs_register_components() after this to register
 * the common game components.
 *
 * @return New ECS world on success, NULL on failure
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_ecs_shutdown().
 *
 * @note NOT thread-safe. Must be called from main thread.
 *
 * @code
 * Agentite_World *world = agentite_ecs_init();
 * if (!world) {
 *     // Handle error
 * }
 * agentite_ecs_register_components(world);
 * @endcode
 */
Agentite_World *agentite_ecs_init(void);

/**
 * @brief Shutdown the ECS world and free all resources.
 *
 * Destroys all entities, components, systems, and queries. The world
 * pointer becomes invalid after this call.
 *
 * @param world ECS world to destroy (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_ecs_shutdown(Agentite_World *world);

/**
 * @brief Get the underlying Flecs world pointer.
 *
 * Provides access to the raw Flecs ecs_world_t for advanced features
 * not exposed by the Agentite wrapper (observers, relationships, prefabs, etc.).
 *
 * @param world ECS world wrapper (must not be NULL)
 *
 * @return Flecs world pointer (borrowed reference, do not free)
 *
 * @note The returned pointer is valid until agentite_ecs_shutdown() is called.
 *
 * @code
 * ecs_world_t *flecs = agentite_ecs_get_world(world);
 * // Use any Flecs API directly
 * ecs_observer(flecs, { ... });
 * @endcode
 */
ecs_world_t *agentite_ecs_get_world(Agentite_World *world);

/**
 * @brief Progress the ECS world (run all systems).
 *
 * Runs all registered systems for one frame. Call this once per
 * frame in your game loop. Systems receive the delta_time via
 * `it->delta_time` in their callbacks.
 *
 * @param world      ECS world (must not be NULL)
 * @param delta_time Time since last frame in seconds
 *
 * @return true if the world should continue running, false to quit
 *
 * @note Deletion and modification operations are deferred during system execution.
 */
bool agentite_ecs_progress(Agentite_World *world, float delta_time);

/* Forward declaration */
struct Agentite_Profiler;

/**
 * @brief Set profiler for ECS performance tracking.
 *
 * When a profiler is set, the ECS world will report:
 * - "ecs_progress" scope: Time spent in system iteration
 * - Entity count per frame
 *
 * @param world    ECS world (must not be NULL)
 * @param profiler Profiler instance, or NULL to disable profiling
 */
void agentite_ecs_set_profiler(Agentite_World *world, struct Agentite_Profiler *profiler);

/** @} */ /* end of ecs_lifecycle */

/** @defgroup ecs_entity Entity Functions
 *  @{ */

/**
 * @brief Create a new anonymous entity.
 *
 * Creates an entity with no name and no components. Add components
 * using AGENTITE_ECS_SET() or AGENTITE_ECS_ADD().
 *
 * @param world ECS world (must not be NULL)
 *
 * @return New entity ID on success, 0 on failure
 *
 * @code
 * ecs_entity_t bullet = agentite_ecs_entity_new(world);
 * AGENTITE_ECS_SET(world, bullet, C_Position, {x, y});
 * @endcode
 */
ecs_entity_t agentite_ecs_entity_new(Agentite_World *world);

/**
 * @brief Create a new named entity.
 *
 * Creates an entity with a name that can be looked up later using
 * ecs_lookup() on the Flecs world.
 *
 * @param world ECS world (must not be NULL)
 * @param name  Entity name (copied internally, original can be freed)
 *
 * @return New entity ID on success, 0 on failure
 *
 * @code
 * ecs_entity_t player = agentite_ecs_entity_new_named(world, "Player");
 *
 * // Later lookup by name:
 * ecs_entity_t found = ecs_lookup(agentite_ecs_get_world(world), "Player");
 * @endcode
 */
ecs_entity_t agentite_ecs_entity_new_named(Agentite_World *world, const char *name);

/**
 * @brief Delete an entity and all its components.
 *
 * Removes the entity and all associated components from the world.
 * Safe to call during system iteration (deletion is deferred until
 * after the current system completes).
 *
 * @param world  ECS world (must not be NULL)
 * @param entity Entity to delete
 *
 * @warning Entity IDs may be recycled. Check with agentite_ecs_entity_is_alive()
 *          before using stored entity IDs.
 */
void agentite_ecs_entity_delete(Agentite_World *world, ecs_entity_t entity);

/**
 * @brief Check if an entity is alive (not deleted).
 *
 * @param world  ECS world (must not be NULL)
 * @param entity Entity to check
 *
 * @return true if entity exists and is alive, false otherwise
 */
bool agentite_ecs_entity_is_alive(Agentite_World *world, ecs_entity_t entity);

/**
 * @brief Register the common game components.
 *
 * Registers C_Position, C_Velocity, C_Size, C_Color, C_Name, C_Active,
 * C_Health, and C_RenderLayer components. Call this once after
 * agentite_ecs_init() before using any components.
 *
 * @param world ECS world (must not be NULL)
 *
 * @note Custom components can still be registered directly using Flecs APIs.
 */
void agentite_ecs_register_components(Agentite_World *world);

/** @} */ /* end of ecs_entity */

/** @defgroup ecs_macros Convenience Macros
 *
 * These macros wrap common Flecs operations with the Agentite_World type,
 * providing a simpler interface for basic ECS operations.
 *
 * @{
 */

/**
 * @def AGENTITE_ECS_SET
 * @brief Set a component value on an entity.
 *
 * Creates the component if it doesn't exist, or updates it if it does.
 * The component value is provided as a compound literal.
 *
 * @param world  Agentite_World pointer
 * @param entity Target entity
 * @param T      Component type (e.g., C_Position)
 * @param ...    Component value as compound literal (e.g., {100.0f, 200.0f})
 *
 * @code
 * AGENTITE_ECS_SET(world, player, C_Position, {100.0f, 200.0f});
 * AGENTITE_ECS_SET(world, player, C_Health, {.health = 100, .max_health = 100});
 * @endcode
 */
#define AGENTITE_ECS_SET(world, entity, T, ...) \
    ecs_set(agentite_ecs_get_world(world), entity, T, __VA_ARGS__)

/**
 * @def AGENTITE_ECS_GET
 * @brief Get a const pointer to an entity's component.
 *
 * Returns a read-only pointer to the component data.
 *
 * @param world  Agentite_World pointer
 * @param entity Entity to query
 * @param T      Component type (e.g., C_Position)
 *
 * @return Const pointer to component, or NULL if entity doesn't have it
 *
 * @code
 * const C_Position *pos = AGENTITE_ECS_GET(world, player, C_Position);
 * if (pos) {
 *     printf("Position: %.1f, %.1f\n", pos->x, pos->y);
 * }
 * @endcode
 *
 * @warning The returned pointer may become invalid after world modifications.
 */
#define AGENTITE_ECS_GET(world, entity, T) \
    ecs_get(agentite_ecs_get_world(world), entity, T)

/**
 * @def AGENTITE_ECS_ADD
 * @brief Add a component to an entity (zero-initialized).
 *
 * Adds the component with default (zero) initialization.
 * No-op if entity already has the component.
 *
 * @param world  Agentite_World pointer
 * @param entity Target entity
 * @param T      Component type (e.g., C_Active)
 *
 * @code
 * AGENTITE_ECS_ADD(world, entity, C_Active);
 * @endcode
 */
#define AGENTITE_ECS_ADD(world, entity, T) \
    ecs_add(agentite_ecs_get_world(world), entity, T)

/**
 * @def AGENTITE_ECS_REMOVE
 * @brief Remove a component from an entity.
 *
 * No-op if entity doesn't have the component.
 *
 * @param world  Agentite_World pointer
 * @param entity Target entity
 * @param T      Component type to remove
 *
 * @code
 * AGENTITE_ECS_REMOVE(world, entity, C_Velocity);  // Stop moving
 * @endcode
 */
#define AGENTITE_ECS_REMOVE(world, entity, T) \
    ecs_remove(agentite_ecs_get_world(world), entity, T)

/**
 * @def AGENTITE_ECS_HAS
 * @brief Check if an entity has a component.
 *
 * @param world  Agentite_World pointer
 * @param entity Entity to check
 * @param T      Component type to check for
 *
 * @return true if entity has the component, false otherwise
 *
 * @code
 * if (AGENTITE_ECS_HAS(world, entity, C_Health)) {
 *     // Entity is damageable
 * }
 * @endcode
 */
#define AGENTITE_ECS_HAS(world, entity, T) \
    ecs_has(agentite_ecs_get_world(world), entity, T)

/**
 * @def AGENTITE_ECS_SYSTEM
 * @brief Register a system to run during a specific phase.
 *
 * Registers a function as a system that runs each frame during the
 * specified phase. Systems query for entities with specific components
 * and process them in batches.
 *
 * @param world Agentite_World pointer
 * @param name  System function name
 * @param phase Execution phase (EcsOnUpdate, EcsPreUpdate, EcsPostUpdate)
 * @param ...   Additional system configuration (query terms, etc.)
 *
 * Common phases:
 * - EcsPreUpdate: Input processing, before main logic
 * - EcsOnUpdate: Main update phase (most game logic)
 * - EcsPostUpdate: Cleanup, collision response, after main logic
 *
 * @code
 * void MovementSystem(ecs_iter_t *it) {
 *     C_Position *pos = ecs_field(it, C_Position, 0);  // Field 0
 *     C_Velocity *vel = ecs_field(it, C_Velocity, 1);  // Field 1
 *     for (int i = 0; i < it->count; i++) {
 *         pos[i].x += vel[i].vx * it->delta_time;
 *         pos[i].y += vel[i].vy * it->delta_time;
 *     }
 * }
 *
 * AGENTITE_ECS_SYSTEM(world, MovementSystem, EcsOnUpdate,
 *     .query.terms = {
 *         { .id = ecs_id(C_Position) },
 *         { .id = ecs_id(C_Velocity) }
 *     });
 * @endcode
 */
#define AGENTITE_ECS_SYSTEM(world, name, phase, ...) \
    ecs_system(agentite_ecs_get_world(world), { \
        .entity = ecs_entity(agentite_ecs_get_world(world), { .name = #name, .add = ecs_ids(ecs_dependson(phase)) }), \
        .callback = name, \
        __VA_ARGS__ \
    })

/**
 * @def AGENTITE_ECS_QUERY
 * @brief Create a query for iterating over entities.
 *
 * Creates a query that can be used to iterate over entities matching
 * specific component requirements. Queries are cached for performance.
 *
 * @param world Agentite_World pointer
 * @param ...   Query configuration (terms, filters, etc.)
 *
 * @return ecs_query_t* pointer (must be freed with ecs_query_fini())
 *
 * @code
 * ecs_query_t *q = AGENTITE_ECS_QUERY(world, {
 *     .terms = {
 *         { .id = ecs_id(C_Position) },
 *         { .id = ecs_id(C_Velocity) }
 *     }
 * });
 *
 * ecs_iter_t it = ecs_query_iter(agentite_ecs_get_world(world), q);
 * while (ecs_query_next(&it)) {
 *     // Process matching entities
 * }
 *
 * ecs_query_fini(q);  // Cleanup when done
 * @endcode
 */
#define AGENTITE_ECS_QUERY(world, ...) \
    ecs_query(agentite_ecs_get_world(world), __VA_ARGS__)

/** @} */ /* end of ecs_macros */

#endif // AGENTITE_ECS_H
