/**
 * Carbon Construction Queue / Ghost Building System
 *
 * Planned buildings with progress tracking before actual construction.
 * Supports ghost/preview buildings, construction progress, speed modifiers,
 * and completion callbacks.
 *
 * Usage:
 *   // Create construction queue
 *   Agentite_ConstructionQueue *queue = agentite_construction_create(32);
 *
 *   // Add ghost building (planned, not yet constructed)
 *   uint32_t ghost = agentite_construction_add_ghost(queue, 10, 20, BUILDING_FACTORY, 0);
 *
 *   // Set construction speed (e.g., based on workers assigned)
 *   agentite_construction_set_speed(queue, ghost, 1.5f);
 *
 *   // In game loop:
 *   agentite_construction_update(queue, delta_time);
 *
 *   // Check for completion
 *   if (agentite_construction_is_complete(queue, ghost)) {
 *       Agentite_Ghost *g = agentite_construction_get_ghost(queue, ghost);
 *       create_actual_building(g->x, g->y, g->building_type);
 *       agentite_construction_remove_ghost(queue, ghost);
 *   }
 *
 *   // Cleanup
 *   agentite_construction_destroy(queue);
 */

#ifndef AGENTITE_CONSTRUCTION_H
#define AGENTITE_CONSTRUCTION_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_GHOST_INVALID 0          /* Invalid ghost handle */

/*============================================================================
 * Types
 *============================================================================*/

/**
 * Ghost building status.
 */
typedef enum Agentite_GhostStatus {
    AGENTITE_GHOST_PENDING = 0,       /* Waiting to start construction */
    AGENTITE_GHOST_CONSTRUCTING,      /* Construction in progress */
    AGENTITE_GHOST_COMPLETE,          /* Construction complete */
    AGENTITE_GHOST_CANCELLED,         /* Construction cancelled */
    AGENTITE_GHOST_PAUSED,            /* Construction paused */
} Agentite_GhostStatus;

/**
 * A ghost (planned) building.
 */
typedef struct Agentite_Ghost {
    uint32_t id;                    /* Unique ghost ID */
    int x;                          /* World X position */
    int y;                          /* World Y position */
    uint16_t building_type;         /* Building type ID */
    uint8_t direction;              /* Building direction (0-3) */
    Agentite_GhostStatus status;      /* Current status */

    float progress;                 /* Construction progress (0.0 to 1.0) */
    float base_duration;            /* Base construction time in seconds */
    float speed_multiplier;         /* Speed modifier (1.0 = normal) */

    int32_t faction_id;             /* Owning faction (-1 = none) */
    int32_t builder_entity;         /* Entity performing construction (-1 = none) */

    uint32_t metadata;              /* Game-defined extra data */
    void *userdata;                 /* User-defined pointer */
} Agentite_Ghost;

/**
 * Forward declaration of construction queue.
 */
typedef struct Agentite_ConstructionQueue Agentite_ConstructionQueue;

/**
 * Callback when a ghost building completes, is cancelled, or fails.
 *
 * @param queue     The construction queue
 * @param ghost     The ghost that changed status
 * @param userdata  User data passed to set_callback
 */
typedef void (*Agentite_ConstructionCallback)(
    Agentite_ConstructionQueue *queue,
    const Agentite_Ghost *ghost,
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
typedef bool (*Agentite_ConstructionCondition)(
    Agentite_ConstructionQueue *queue,
    const Agentite_Ghost *ghost,
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
Agentite_ConstructionQueue *agentite_construction_create(int max_ghosts);

/**
 * Destroy a construction queue and free resources.
 *
 * @param queue Queue to destroy (safe if NULL)
 */
void agentite_construction_destroy(Agentite_ConstructionQueue *queue);

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
 * @return Ghost handle or AGENTITE_GHOST_INVALID on failure
 */
uint32_t agentite_construction_add_ghost(
    Agentite_ConstructionQueue *queue,
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
 * @return Ghost handle or AGENTITE_GHOST_INVALID on failure
 */
uint32_t agentite_construction_add_ghost_ex(
    Agentite_ConstructionQueue *queue,
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
bool agentite_construction_remove_ghost(
    Agentite_ConstructionQueue *queue,
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
bool agentite_construction_cancel_ghost(
    Agentite_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Get a ghost building by handle.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Ghost pointer or NULL if not found
 */
Agentite_Ghost *agentite_construction_get_ghost(
    Agentite_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Get a ghost building by handle (const version).
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Ghost pointer or NULL if not found
 */
const Agentite_Ghost *agentite_construction_get_ghost_const(
    const Agentite_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Find a ghost at a specific position.
 *
 * @param queue Queue to search
 * @param x     World X position
 * @param y     World Y position
 * @return Ghost handle or AGENTITE_GHOST_INVALID if none found
 */
uint32_t agentite_construction_find_at(
    const Agentite_ConstructionQueue *queue,
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
bool agentite_construction_has_ghost_at(
    const Agentite_ConstructionQueue *queue,
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
void agentite_construction_update(
    Agentite_ConstructionQueue *queue,
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
bool agentite_construction_start(
    Agentite_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Pause construction on a ghost building.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if paused
 */
bool agentite_construction_pause(
    Agentite_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Resume construction on a paused ghost building.
 *
 * @param queue Queue to modify
 * @param ghost Ghost handle
 * @return true if resumed
 */
bool agentite_construction_resume(
    Agentite_ConstructionQueue *queue,
    uint32_t ghost
);

/**
 * Get construction progress.
 *
 * @param queue Queue to query
 * @param ghost Ghost handle
 * @return Progress (0.0 to 1.0) or -1.0 if not found
 */
float agentite_construction_get_progress(
    const Agentite_ConstructionQueue *queue,
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
bool agentite_construction_set_progress(
    Agentite_ConstructionQueue *queue,
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
bool agentite_construction_add_progress(
    Agentite_ConstructionQueue *queue,
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
bool agentite_construction_is_complete(
    const Agentite_ConstructionQueue *queue,
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
bool agentite_construction_complete_instant(
    Agentite_ConstructionQueue *queue,
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
bool agentite_construction_set_speed(
    Agentite_ConstructionQueue *queue,
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
float agentite_construction_get_speed(
    const Agentite_ConstructionQueue *queue,
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
bool agentite_construction_set_duration(
    Agentite_ConstructionQueue *queue,
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
float agentite_construction_get_remaining_time(
    const Agentite_ConstructionQueue *queue,
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
bool agentite_construction_set_builder(
    Agentite_ConstructionQueue *queue,
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
int32_t agentite_construction_get_builder(
    const Agentite_ConstructionQueue *queue,
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
int agentite_construction_find_by_builder(
    const Agentite_ConstructionQueue *queue,
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
int agentite_construction_get_by_faction(
    const Agentite_ConstructionQueue *queue,
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
int agentite_construction_count_by_faction(
    const Agentite_ConstructionQueue *queue,
    int32_t faction_id
);

/**
 * Count active (constructing) ghosts for a faction.
 *
 * @param queue      Queue to search
 * @param faction_id Faction ID
 * @return Number of constructing ghosts
 */
int agentite_construction_count_active_by_faction(
    const Agentite_ConstructionQueue *queue,
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
int agentite_construction_count(const Agentite_ConstructionQueue *queue);

/**
 * Get number of actively constructing ghosts.
 *
 * @param queue Queue to query
 * @return Active ghost count
 */
int agentite_construction_count_active(const Agentite_ConstructionQueue *queue);

/**
 * Get number of complete ghosts.
 *
 * @param queue Queue to query
 * @return Complete ghost count
 */
int agentite_construction_count_complete(const Agentite_ConstructionQueue *queue);

/**
 * Check if queue is full.
 *
 * @param queue Queue to query
 * @return true if full
 */
bool agentite_construction_is_full(const Agentite_ConstructionQueue *queue);

/**
 * Get maximum capacity.
 *
 * @param queue Queue to query
 * @return Maximum ghosts
 */
int agentite_construction_capacity(const Agentite_ConstructionQueue *queue);

/**
 * Get all ghost handles.
 *
 * @param queue       Queue to query
 * @param out_handles Output array
 * @param max_handles Maximum handles to return
 * @return Number of handles written
 */
int agentite_construction_get_all(
    const Agentite_ConstructionQueue *queue,
    uint32_t *out_handles,
    int max_handles
);

/**
 * Clear all ghosts from the queue.
 * Does NOT trigger callbacks.
 *
 * @param queue Queue to clear
 */
void agentite_construction_clear(Agentite_ConstructionQueue *queue);

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
void agentite_construction_set_callback(
    Agentite_ConstructionQueue *queue,
    Agentite_ConstructionCallback callback,
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
void agentite_construction_set_condition_callback(
    Agentite_ConstructionQueue *queue,
    Agentite_ConstructionCondition callback,
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
bool agentite_construction_set_metadata(
    Agentite_ConstructionQueue *queue,
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
uint32_t agentite_construction_get_metadata(
    const Agentite_ConstructionQueue *queue,
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
bool agentite_construction_set_userdata(
    Agentite_ConstructionQueue *queue,
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
void *agentite_construction_get_userdata(
    const Agentite_ConstructionQueue *queue,
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
const char *agentite_ghost_status_name(Agentite_GhostStatus status);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_CONSTRUCTION_H */
