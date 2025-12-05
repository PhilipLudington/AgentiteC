/**
 * Carbon Construction Queue / Ghost Building System
 *
 * Planned buildings with progress tracking before actual construction.
 * Supports ghost/preview buildings, construction progress, speed modifiers,
 * and completion callbacks.
 *
 * Usage:
 *   // Create construction queue
 *   Carbon_ConstructionQueue *queue = carbon_construction_create(32);
 *
 *   // Add ghost building (planned, not yet constructed)
 *   uint32_t ghost = carbon_construction_add_ghost(queue, 10, 20, BUILDING_FACTORY, 0);
 *
 *   // Set construction speed (e.g., based on workers assigned)
 *   carbon_construction_set_speed(queue, ghost, 1.5f);
 *
 *   // In game loop:
 *   carbon_construction_update(queue, delta_time);
 *
 *   // Check for completion
 *   if (carbon_construction_is_complete(queue, ghost)) {
 *       Carbon_Ghost *g = carbon_construction_get_ghost(queue, ghost);
 *       create_actual_building(g->x, g->y, g->building_type);
 *       carbon_construction_remove_ghost(queue, ghost);
 *   }
 *
 *   // Cleanup
 *   carbon_construction_destroy(queue);
 */

#ifndef CARBON_CONSTRUCTION_H
#define CARBON_CONSTRUCTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_GHOST_INVALID 0          /* Invalid ghost handle */

/*============================================================================
 * Types
 *============================================================================*/

/**
 * Ghost building status.
 */
typedef enum Carbon_GhostStatus {
    CARBON_GHOST_PENDING = 0,       /* Waiting to start construction */
    CARBON_GHOST_CONSTRUCTING,      /* Construction in progress */
    CARBON_GHOST_COMPLETE,          /* Construction complete */
    CARBON_GHOST_CANCELLED,         /* Construction cancelled */
    CARBON_GHOST_PAUSED,            /* Construction paused */
} Carbon_GhostStatus;

/**
 * A ghost (planned) building.
 */
typedef struct Carbon_Ghost {
    uint32_t id;                    /* Unique ghost ID */
    int x;                          /* World X position */
    int y;                          /* World Y position */
    uint16_t building_type;         /* Building type ID */
    uint8_t direction;              /* Building direction (0-3) */
    Carbon_GhostStatus status;      /* Current status */

    float progress;                 /* Construction progress (0.0 to 1.0) */
    float base_duration;            /* Base construction time in seconds */
    float speed_multiplier;         /* Speed modifier (1.0 = normal) */

    int32_t faction_id;             /* Owning faction (-1 = none) */
    int32_t builder_entity;         /* Entity performing construction (-1 = none) */

    uint32_t metadata;              /* Game-defined extra data */
    void *userdata;                 /* User-defined pointer */
} Carbon_Ghost;

/**
 * Forward declaration of construction queue.
 */
typedef struct Carbon_ConstructionQueue Carbon_ConstructionQueue;

/**
 * Callback when a ghost building completes, is cancelled, or fails.
 *
 * @param queue     The construction queue
 * @param ghost     The ghost that changed status
 * @param userdata  User data passed to set_callback
 */
typedef void (*Carbon_ConstructionCallback)(
    Carbon_ConstructionQueue *queue,
    const Carbon_Ghost *ghost,
    void *userdata
);

/**
 * Callback to check if construction can proceed.
 * Return true if resources/conditions allow construction to continue.
 *
 * @param queue     The construction queue
 * @param ghost     The ghost being checked
 * @param userdata  User data passed to set_condition_callback
 * @return true if construction can continue
 */
typedef bool (*Carbon_ConstructionCondition)(
    Carbon_ConstructionQueue *queue,
    const Carbon_Ghost *ghost,
    void *userdata
);

/*============================================================================
 * Queue Creation and Destruction
 *============================================================================*/

/**
 * Create a new construction queue.
 *
 * @param max_ghosts Maximum number of ghost buildings
 * @return New queue or NULL on failure
 */
Carbon_ConstructionQueue *carbon_construction_create(int max_ghosts);

/**
 * Destroy a construction queue and free resources.
 *
 * @param queue Queue to destroy (safe if NULL)
 */
void carbon_construction_destroy(Carbon_ConstructionQueue *queue);

/*============================================================================
 * Ghost Management
 *============================================================================*/

/**
 * Add a ghost building to the queue.
 *
 * @param queue         Construction queue
 * @param x             World X position
 * @param y             World Y position
 * @param building_type Building type ID
 * @param direction     Building direction (0-3)
 * @return Ghost handle or CARBON_GHOST_INVALID on failure
 */
uint32_t carbon_construction_add_ghost(
    Carbon_ConstructionQueue *queue,
    int x, int y,
    uint16_t building_type,
    uint8_t direction
);

/**
 * Add a ghost building with extended options.
 *
 * @param queue          Construction queue
 * @param x              World X position
 * @param y              World Y position
 * @param building_type  Building type ID
 * @param direction      Building direction (0-3)
 * @param base_duration  Base construction time in seconds
 * @param faction_id     Owning faction (-1 = none)
 * @return Ghost handle or CARBON_GHOST_INVALID on failure
 */
uint32_t carbon_construction_add_ghost_ex(
    Carbon_ConstructionQueue *queue,
    int x, int y,
    uint16_t building_type,
    uint8_t direction,
    float base_duration,
    int32_t faction_id
);

/**
 * Remove a ghost building from the queue.
 * Does NOT trigger the completion callback.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if removed
 */
bool carbon_construction_remove_ghost(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Cancel a ghost building's construction.
 * Sets status to CANCELLED and triggers callback.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if cancelled
 */
bool carbon_construction_cancel_ghost(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Get a ghost building by handle.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Ghost pointer or NULL if not found
 */
Carbon_Ghost *carbon_construction_get_ghost(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Get a ghost building by handle (const version).
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Ghost pointer or NULL if not found
 */
const Carbon_Ghost *carbon_construction_get_ghost_const(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Find a ghost at a specific position.
 *
 * @param queue Queue to search
 * @param x     World X position
 * @param y     World Y position
 * @return Ghost handle or CARBON_GHOST_INVALID if none found
 */
uint32_t carbon_construction_find_at(
    const Carbon_ConstructionQueue *queue,
    int x, int y
);

/**
 * Check if there's a ghost at a specific position.
 *
 * @param queue Queue to search
 * @param x     World X position
 * @param y     World Y position
 * @return true if a ghost exists at this position
 */
bool carbon_construction_has_ghost_at(
    const Carbon_ConstructionQueue *queue,
    int x, int y
);

/*============================================================================
 * Construction Progress
 *============================================================================*/

/**
 * Update all ghost buildings.
 * Call this each frame to advance construction.
 *
 * @param queue      Construction queue
 * @param delta_time Time elapsed this frame
 */
void carbon_construction_update(
    Carbon_ConstructionQueue *queue,
    float delta_time
);

/**
 * Start construction on a ghost building.
 * Changes status from PENDING to CONSTRUCTING.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if started
 */
bool carbon_construction_start(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Pause construction on a ghost building.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if paused
 */
bool carbon_construction_pause(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Resume construction on a paused ghost building.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if resumed
 */
bool carbon_construction_resume(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Get construction progress.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Progress (0.0 to 1.0) or -1.0 if not found
 */
float carbon_construction_get_progress(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Set construction progress directly.
 * Useful for loading saved games.
 *
 * @param queue    Queue to modify
 * @param ghost    Ghost handle
 * @param progress Progress value (0.0 to 1.0)
 * @return true if set
 */
bool carbon_construction_set_progress(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float progress
);

/**
 * Add progress to a ghost building.
 * Useful for worker-based construction.
 *
 * @param queue  Queue to modify
 * @param ghost  Ghost handle
 * @param amount Progress to add (0.0 to 1.0)
 * @return true if progress was added (may complete construction)
 */
bool carbon_construction_add_progress(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float amount
);

/**
 * Check if construction is complete.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return true if complete
 */
bool carbon_construction_is_complete(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Instantly complete construction.
 * Useful for cheats or instant-build abilities.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if completed
 */
bool carbon_construction_complete_instant(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/*============================================================================
 * Speed and Modifiers
 *============================================================================*/

/**
 * Set construction speed multiplier.
 *
 * @param queue      Queue to modify
 * @param ghost      Ghost handle
 * @param multiplier Speed multiplier (1.0 = normal, 2.0 = twice as fast)
 * @return true if set
 */
bool carbon_construction_set_speed(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float multiplier
);

/**
 * Get construction speed multiplier.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Speed multiplier or 0.0 if not found
 */
float carbon_construction_get_speed(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Set base construction duration.
 *
 * @param queue    Queue to modify
 * @param ghost    Ghost handle
 * @param duration Base duration in seconds
 * @return true if set
 */
bool carbon_construction_set_duration(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    float duration
);

/**
 * Get remaining construction time.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Remaining time in seconds, or -1.0 if not found
 */
float carbon_construction_get_remaining_time(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/*============================================================================
 * Builder Assignment
 *============================================================================*/

/**
 * Assign a builder entity to a ghost.
 *
 * @param queue         Queue to modify
 * @param ghost         Ghost handle
 * @param builder_entity Entity ID of the builder (-1 to clear)
 * @return true if assigned
 */
bool carbon_construction_set_builder(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    int32_t builder_entity
);

/**
 * Get the builder entity assigned to a ghost.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Builder entity ID or -1 if none
 */
int32_t carbon_construction_get_builder(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Find ghosts assigned to a specific builder.
 *
 * @param queue          Queue to search
 * @param builder_entity Builder entity ID
 * @param out_handles    Output array for ghost handles
 * @param max_handles    Maximum handles to return
 * @return Number of ghosts found
 */
int carbon_construction_find_by_builder(
    const Carbon_ConstructionQueue *queue,
    int32_t builder_entity,
    uint32_t *out_handles,
    int max_handles
);

/*============================================================================
 * Faction Queries
 *============================================================================*/

/**
 * Get all ghosts for a faction.
 *
 * @param queue       Queue to search
 * @param faction_id  Faction ID
 * @param out_handles Output array for ghost handles
 * @param max_handles Maximum handles to return
 * @return Number of ghosts found
 */
int carbon_construction_get_by_faction(
    const Carbon_ConstructionQueue *queue,
    int32_t faction_id,
    uint32_t *out_handles,
    int max_handles
);

/**
 * Count ghosts for a faction.
 *
 * @param queue      Queue to search
 * @param faction_id Faction ID
 * @return Number of ghosts
 */
int carbon_construction_count_by_faction(
    const Carbon_ConstructionQueue *queue,
    int32_t faction_id
);

/**
 * Count active (constructing) ghosts for a faction.
 *
 * @param queue      Queue to search
 * @param faction_id Faction ID
 * @return Number of constructing ghosts
 */
int carbon_construction_count_active_by_faction(
    const Carbon_ConstructionQueue *queue,
    int32_t faction_id
);

/*============================================================================
 * Queue State
 *============================================================================*/

/**
 * Get total number of ghosts in the queue.
 *
 * @param queue Queue to query
 * @return Ghost count
 */
int carbon_construction_count(const Carbon_ConstructionQueue *queue);

/**
 * Get number of actively constructing ghosts.
 *
 * @param queue Queue to query
 * @return Active ghost count
 */
int carbon_construction_count_active(const Carbon_ConstructionQueue *queue);

/**
 * Get number of complete ghosts.
 *
 * @param queue Queue to query
 * @return Complete ghost count
 */
int carbon_construction_count_complete(const Carbon_ConstructionQueue *queue);

/**
 * Check if queue is full.
 *
 * @param queue Queue to query
 * @return true if full
 */
bool carbon_construction_is_full(const Carbon_ConstructionQueue *queue);

/**
 * Get maximum capacity.
 *
 * @param queue Queue to query
 * @return Maximum ghosts
 */
int carbon_construction_capacity(const Carbon_ConstructionQueue *queue);

/**
 * Get all ghost handles.
 *
 * @param queue       Queue to query
 * @param out_handles Output array
 * @param max_handles Maximum handles to return
 * @return Number of handles written
 */
int carbon_construction_get_all(
    const Carbon_ConstructionQueue *queue,
    uint32_t *out_handles,
    int max_handles
);

/**
 * Clear all ghosts from the queue.
 * Does NOT trigger callbacks.
 *
 * @param queue Queue to clear
 */
void carbon_construction_clear(Carbon_ConstructionQueue *queue);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set completion callback.
 * Called when a ghost completes or is cancelled.
 *
 * @param queue    Queue to modify
 * @param callback Callback function (NULL to clear)
 * @param userdata User data for callback
 */
void carbon_construction_set_callback(
    Carbon_ConstructionQueue *queue,
    Carbon_ConstructionCallback callback,
    void *userdata
);

/**
 * Set condition callback.
 * If set, construction only proceeds when this returns true.
 *
 * @param queue    Queue to modify
 * @param callback Condition function (NULL to clear)
 * @param userdata User data for callback
 */
void carbon_construction_set_condition_callback(
    Carbon_ConstructionQueue *queue,
    Carbon_ConstructionCondition callback,
    void *userdata
);

/*============================================================================
 * Metadata
 *============================================================================*/

/**
 * Set ghost metadata.
 *
 * @param queue    Queue to modify
 * @param ghost    Ghost handle
 * @param metadata Game-defined metadata
 * @return true if set
 */
bool carbon_construction_set_metadata(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    uint32_t metadata
);

/**
 * Get ghost metadata.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Metadata or 0 if not found
 */
uint32_t carbon_construction_get_metadata(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Set ghost userdata pointer.
 *
 * @param queue    Queue to modify
 * @param ghost    Ghost handle
 * @param userdata User-defined pointer
 * @return true if set
 */
bool carbon_construction_set_userdata(
    Carbon_ConstructionQueue *queue,
    uint32_t ghost,
    void *userdata
);

/**
 * Get ghost userdata pointer.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Userdata or NULL if not found
 */
void *carbon_construction_get_userdata(
    const Carbon_ConstructionQueue *queue,
    uint32_t ghost
);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get human-readable name for a ghost status.
 *
 * @param status Ghost status
 * @return Static string name
 */
const char *carbon_ghost_status_name(Carbon_GhostStatus status);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_CONSTRUCTION_H */
