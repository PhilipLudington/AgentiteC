/**
 * Agentite Engine - Transform Hierarchy System
 *
 * Provides parent-child entity relationships with automatic transform
 * propagation. Child entities inherit and combine their parent's transforms.
 *
 * Key Concepts:
 * - C_Transform: Local position, rotation, and scale
 * - C_WorldTransform: Computed world-space transform (auto-updated)
 * - Parent-child: Uses Flecs built-in EcsChildOf relationship
 *
 * Usage:
 *   // Register transform components
 *   agentite_transform_register(world);
 *
 *   // Create parent entity with transform
 *   ecs_entity_t parent = ecs_new(world);
 *   AGENTITE_ECS_SET(world, parent, C_Transform, {
 *       .local_x = 100, .local_y = 100,
 *       .rotation = 0, .scale_x = 1, .scale_y = 1
 *   });
 *
 *   // Create child and attach to parent
 *   ecs_entity_t child = ecs_new(world);
 *   agentite_transform_set_parent(world, child, parent);
 *   AGENTITE_ECS_SET(world, child, C_Transform, {
 *       .local_x = 20, .local_y = 0,  // Relative to parent
 *   });
 *
 *   // Progress world to update transforms
 *   ecs_progress(world, delta_time);
 *
 *   // Get world position of child
 *   float world_x, world_y;
 *   agentite_transform_get_world_position(world, child, &world_x, &world_y);
 *   // world_x = 120, world_y = 100
 */

#ifndef AGENTITE_TRANSFORM_H
#define AGENTITE_TRANSFORM_H

#include "flecs.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Forward declaration for Agentite wrapper */
typedef struct Agentite_World Agentite_World;

/* ============================================================================
 * Transform Components
 * ============================================================================ */

/**
 * Local transform component.
 * Position, rotation, and scale are relative to the parent entity.
 * For root entities (no parent), local == world.
 */
typedef struct C_Transform {
    float local_x;      /* Local X position */
    float local_y;      /* Local Y position */
    float rotation;     /* Rotation in radians */
    float scale_x;      /* Horizontal scale (1.0 = normal) */
    float scale_y;      /* Vertical scale (1.0 = normal) */
} C_Transform;

/**
 * World transform component (auto-computed).
 * Contains the final world-space transform after combining all parent transforms.
 * This component is automatically added and updated by the transform system.
 */
typedef struct C_WorldTransform {
    float world_x;      /* World X position */
    float world_y;      /* World Y position */
    float world_rotation;   /* Accumulated rotation in radians */
    float world_scale_x;    /* Accumulated X scale */
    float world_scale_y;    /* Accumulated Y scale */
} C_WorldTransform;

/* Default initializer for C_Transform */
#define C_TRANSFORM_DEFAULT { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f }

/* Component ID declarations (extern for use across translation units) */
extern ECS_COMPONENT_DECLARE(C_Transform);
extern ECS_COMPONENT_DECLARE(C_WorldTransform);

/* ============================================================================
 * Transform System Registration
 * ============================================================================ */

/**
 * Register transform components and systems.
 * Call once after agentite_ecs_init().
 *
 * This registers:
 * - C_Transform and C_WorldTransform components
 * - Transform propagation system (runs on EcsPostUpdate)
 *
 * @param world Flecs world pointer (use agentite_ecs_get_world())
 */
void agentite_transform_register(ecs_world_t *world);

/**
 * Register transform components with Agentite world wrapper.
 * Convenience function that calls agentite_transform_register internally.
 *
 * @param world Agentite ECS world
 */
void agentite_transform_register_world(Agentite_World *world);

/* ============================================================================
 * Parent-Child Hierarchy Functions
 * ============================================================================ */

/**
 * Set an entity's parent, creating a transform hierarchy.
 * The child's local transform becomes relative to the parent.
 *
 * If child already has a parent, it is re-parented.
 * If parent is 0, the child becomes a root entity.
 *
 * This function:
 * - Adds C_Transform to child if missing (with defaults)
 * - Adds C_WorldTransform to child if missing
 * - Establishes Flecs EcsChildOf relationship
 *
 * @param world  Flecs world
 * @param child  Entity to become a child
 * @param parent Entity to become the parent (0 to remove parent)
 */
void agentite_transform_set_parent(ecs_world_t *world,
                                    ecs_entity_t child,
                                    ecs_entity_t parent);

/**
 * Get an entity's parent.
 *
 * @param world  Flecs world
 * @param entity Entity to query
 * @return Parent entity, or 0 if no parent
 */
ecs_entity_t agentite_transform_get_parent(ecs_world_t *world,
                                            ecs_entity_t entity);

/**
 * Check if an entity has a parent.
 *
 * @param world  Flecs world
 * @param entity Entity to query
 * @return true if entity has a parent
 */
bool agentite_transform_has_parent(ecs_world_t *world, ecs_entity_t entity);

/**
 * Get all children of an entity.
 *
 * @param world       Flecs world
 * @param parent      Parent entity
 * @param out_children Output array to fill with child entity IDs
 * @param max_count   Maximum number of children to return
 * @return Number of children found (may exceed max_count)
 */
int agentite_transform_get_children(ecs_world_t *world,
                                     ecs_entity_t parent,
                                     ecs_entity_t *out_children,
                                     int max_count);

/**
 * Get the number of direct children of an entity.
 *
 * @param world  Flecs world
 * @param parent Parent entity
 * @return Number of direct children
 */
int agentite_transform_get_child_count(ecs_world_t *world,
                                        ecs_entity_t parent);

/**
 * Remove parent from an entity (make it a root entity).
 * Equivalent to agentite_transform_set_parent(world, entity, 0).
 *
 * @param world  Flecs world
 * @param entity Entity to detach
 */
void agentite_transform_remove_parent(ecs_world_t *world, ecs_entity_t entity);

/* ============================================================================
 * World Transform Access
 * ============================================================================ */

/**
 * Get the world-space position of an entity.
 * Returns the computed position after all parent transforms are applied.
 *
 * @param world   Flecs world
 * @param entity  Entity to query
 * @param out_x   Output X position (can be NULL)
 * @param out_y   Output Y position (can be NULL)
 * @return true if entity has a world transform, false otherwise
 */
bool agentite_transform_get_world_position(ecs_world_t *world,
                                            ecs_entity_t entity,
                                            float *out_x,
                                            float *out_y);

/**
 * Get the world-space rotation of an entity.
 *
 * @param world  Flecs world
 * @param entity Entity to query
 * @return World rotation in radians, or 0 if no transform
 */
float agentite_transform_get_world_rotation(ecs_world_t *world,
                                             ecs_entity_t entity);

/**
 * Get the world-space scale of an entity.
 *
 * @param world     Flecs world
 * @param entity    Entity to query
 * @param out_sx    Output X scale (can be NULL)
 * @param out_sy    Output Y scale (can be NULL)
 * @return true if entity has a world transform, false otherwise
 */
bool agentite_transform_get_world_scale(ecs_world_t *world,
                                         ecs_entity_t entity,
                                         float *out_sx,
                                         float *out_sy);

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

/**
 * Convert a point from local space to world space.
 * Applies the entity's full transform hierarchy.
 *
 * @param world     Flecs world
 * @param entity    Entity whose transform to use
 * @param local_x   Local X coordinate
 * @param local_y   Local Y coordinate
 * @param out_world_x  Output world X (can be NULL)
 * @param out_world_y  Output world Y (can be NULL)
 * @return true on success, false if entity has no transform
 */
bool agentite_transform_local_to_world(ecs_world_t *world,
                                        ecs_entity_t entity,
                                        float local_x,
                                        float local_y,
                                        float *out_world_x,
                                        float *out_world_y);

/**
 * Convert a point from world space to local space.
 * Inverts the entity's full transform hierarchy.
 *
 * @param world     Flecs world
 * @param entity    Entity whose transform to use
 * @param world_x   World X coordinate
 * @param world_y   World Y coordinate
 * @param out_local_x  Output local X (can be NULL)
 * @param out_local_y  Output local Y (can be NULL)
 * @return true on success, false if entity has no transform
 */
bool agentite_transform_world_to_local(ecs_world_t *world,
                                        ecs_entity_t entity,
                                        float world_x,
                                        float world_y,
                                        float *out_local_x,
                                        float *out_local_y);

/* ============================================================================
 * Transform Manipulation
 * ============================================================================ */

/**
 * Set an entity's local position.
 * Adds C_Transform if missing.
 *
 * @param world  Flecs world
 * @param entity Entity to modify
 * @param x      Local X position
 * @param y      Local Y position
 */
void agentite_transform_set_local_position(ecs_world_t *world,
                                            ecs_entity_t entity,
                                            float x,
                                            float y);

/**
 * Set an entity's local rotation.
 * Adds C_Transform if missing.
 *
 * @param world    Flecs world
 * @param entity   Entity to modify
 * @param radians  Rotation in radians
 */
void agentite_transform_set_local_rotation(ecs_world_t *world,
                                            ecs_entity_t entity,
                                            float radians);

/**
 * Set an entity's local scale.
 * Adds C_Transform if missing.
 *
 * @param world   Flecs world
 * @param entity  Entity to modify
 * @param scale_x Horizontal scale
 * @param scale_y Vertical scale
 */
void agentite_transform_set_local_scale(ecs_world_t *world,
                                         ecs_entity_t entity,
                                         float scale_x,
                                         float scale_y);

/**
 * Translate an entity in local space.
 *
 * @param world  Flecs world
 * @param entity Entity to modify
 * @param dx     X translation delta
 * @param dy     Y translation delta
 */
void agentite_transform_translate(ecs_world_t *world,
                                   ecs_entity_t entity,
                                   float dx,
                                   float dy);

/**
 * Rotate an entity by a delta angle.
 *
 * @param world      Flecs world
 * @param entity     Entity to modify
 * @param delta_rad  Rotation delta in radians
 */
void agentite_transform_rotate(ecs_world_t *world,
                                ecs_entity_t entity,
                                float delta_rad);

/* ============================================================================
 * Manual Transform Update
 * ============================================================================ */

/**
 * Force update world transforms for an entity and its children.
 * Normally called automatically by the transform system during ecs_progress().
 * Use this when you need updated world transforms immediately after modifying
 * local transforms within the same frame.
 *
 * @param world  Flecs world
 * @param entity Root entity to update (children are updated recursively)
 */
void agentite_transform_update(ecs_world_t *world, ecs_entity_t entity);

/**
 * Update all world transforms.
 * Called automatically by the transform system, but can be called manually
 * if you need immediate updates.
 *
 * @param world Flecs world
 */
void agentite_transform_update_all(ecs_world_t *world);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_TRANSFORM_H */
