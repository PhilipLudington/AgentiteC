/**
 * @file spatial.h
 * @brief Spatial Hash Index for O(1) entity lookup by grid cell
 *
 * Provides efficient spatial queries for tile-based games. Entities are indexed
 * by their grid position, enabling fast lookup, collision detection, and
 * proximity queries without iterating all entities.
 *
 * Features:
 * - O(1) add, remove, query, move operations
 * - Multiple entities per cell support
 * - Rectangular region queries
 * - Radius queries (circular area)
 * - Iterator for cell contents
 */

#ifndef AGENTITE_SPATIAL_H
#define AGENTITE_SPATIAL_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum entities per cell (can be overridden at compile time) */
#ifndef AGENTITE_SPATIAL_MAX_PER_CELL
#define AGENTITE_SPATIAL_MAX_PER_CELL 16
#endif

/* Maximum entities returned by region/radius queries */
#ifndef AGENTITE_SPATIAL_MAX_QUERY_RESULTS
#define AGENTITE_SPATIAL_MAX_QUERY_RESULTS 256
#endif

/* Invalid entity ID */
#define AGENTITE_SPATIAL_INVALID 0

/**
 * @brief Opaque spatial index handle
 */
typedef struct Agentite_SpatialIndex Agentite_SpatialIndex;

/**
 * @brief Query result for region/radius queries
 */
typedef struct Agentite_SpatialQueryResult {
    uint32_t entity_id;     /**< Entity ID */
    int32_t x;              /**< Grid X position */
    int32_t y;              /**< Grid Y position */
} Agentite_SpatialQueryResult;

/**
 * @brief Iterator for iterating cell contents
 */
typedef struct Agentite_SpatialIterator {
    Agentite_SpatialIndex *index;
    int32_t x;
    int32_t y;
    int current;            /**< Current index within cell */
    int count;              /**< Total entities in cell */
} Agentite_SpatialIterator;

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

/**
 * @brief Create a spatial index
 *
 * @param capacity Initial hash table capacity (will grow as needed)
 * @return New spatial index or NULL on failure
 *
 * @note capacity should be roughly 1.5-2x expected number of occupied cells
 */
Agentite_SpatialIndex *agentite_spatial_create(int capacity);

/**
 * @brief Destroy a spatial index and free all memory
 *
 * @param index Spatial index to destroy
 */
void agentite_spatial_destroy(Agentite_SpatialIndex *index);

/**
 * @brief Clear all entities from the spatial index
 *
 * @param index Spatial index to clear
 */
void agentite_spatial_clear(Agentite_SpatialIndex *index);

/* ============================================================================
 * Basic Operations
 * ========================================================================= */

/**
 * @brief Add an entity at a grid position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param entity_id Entity ID (must be non-zero)
 * @return true if added successfully, false if cell is full or invalid params
 *
 * @note Entity IDs should be unique per cell. Adding the same entity twice
 *       to the same cell will store it twice.
 */
bool agentite_spatial_add(Agentite_SpatialIndex *index, int x, int y, uint32_t entity_id);

/**
 * @brief Remove an entity from a grid position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param entity_id Entity ID to remove
 * @return true if removed, false if not found
 */
bool agentite_spatial_remove(Agentite_SpatialIndex *index, int x, int y, uint32_t entity_id);

/**
 * @brief Move an entity from one cell to another
 *
 * @param index Spatial index
 * @param old_x Previous X coordinate
 * @param old_y Previous Y coordinate
 * @param new_x New X coordinate
 * @param new_y New Y coordinate
 * @param entity_id Entity ID to move
 * @return true if moved successfully
 *
 * @note If old position not found, entity is still added to new position
 */
bool agentite_spatial_move(Agentite_SpatialIndex *index,
                         int old_x, int old_y,
                         int new_x, int new_y,
                         uint32_t entity_id);

/* ============================================================================
 * Query Operations
 * ========================================================================= */

/**
 * @brief Check if any entity exists at a position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if at least one entity is at this position
 */
bool agentite_spatial_has(Agentite_SpatialIndex *index, int x, int y);

/**
 * @brief Get the first entity at a position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return Entity ID or AGENTITE_SPATIAL_INVALID (0) if none
 */
uint32_t agentite_spatial_query(Agentite_SpatialIndex *index, int x, int y);

/**
 * @brief Get all entities at a position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param out_entities Array to fill with entity IDs
 * @param max_entities Size of output array
 * @return Number of entities found
 */
int agentite_spatial_query_all(Agentite_SpatialIndex *index, int x, int y,
                             uint32_t *out_entities, int max_entities);

/**
 * @brief Get count of entities at a position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return Number of entities at this cell
 */
int agentite_spatial_count_at(Agentite_SpatialIndex *index, int x, int y);

/**
 * @brief Check if a specific entity exists at a position
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param entity_id Entity ID to check for
 * @return true if entity is at this position
 */
bool agentite_spatial_has_entity(Agentite_SpatialIndex *index, int x, int y, uint32_t entity_id);

/* ============================================================================
 * Region Queries
 * ========================================================================= */

/**
 * @brief Query all entities in a rectangular region
 *
 * @param index Spatial index
 * @param x1 Left X coordinate (inclusive)
 * @param y1 Top Y coordinate (inclusive)
 * @param x2 Right X coordinate (inclusive)
 * @param y2 Bottom Y coordinate (inclusive)
 * @param out_results Array to fill with results
 * @param max_results Size of output array
 * @return Number of entities found
 */
int agentite_spatial_query_rect(Agentite_SpatialIndex *index,
                              int x1, int y1, int x2, int y2,
                              Agentite_SpatialQueryResult *out_results, int max_results);

/**
 * @brief Query all entities within a radius (Chebyshev distance)
 *
 * Uses Chebyshev distance (square area) for grid-aligned queries.
 *
 * @param index Spatial index
 * @param center_x Center X coordinate
 * @param center_y Center Y coordinate
 * @param radius Distance from center (0 = only center cell)
 * @param out_results Array to fill with results
 * @param max_results Size of output array
 * @return Number of entities found
 */
int agentite_spatial_query_radius(Agentite_SpatialIndex *index,
                                int center_x, int center_y, int radius,
                                Agentite_SpatialQueryResult *out_results, int max_results);

/**
 * @brief Query all entities within a circular radius (Euclidean distance)
 *
 * Uses Euclidean distance for true circular queries.
 *
 * @param index Spatial index
 * @param center_x Center X coordinate
 * @param center_y Center Y coordinate
 * @param radius Distance from center
 * @param out_results Array to fill with results
 * @param max_results Size of output array
 * @return Number of entities found
 */
int agentite_spatial_query_circle(Agentite_SpatialIndex *index,
                                int center_x, int center_y, int radius,
                                Agentite_SpatialQueryResult *out_results, int max_results);

/* ============================================================================
 * Iteration
 * ========================================================================= */

/**
 * @brief Begin iterating entities at a cell
 *
 * @param index Spatial index
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return Iterator (check iter.count > 0 before using)
 *
 * Example:
 * @code
 * Agentite_SpatialIterator iter = agentite_spatial_iter_begin(index, x, y);
 * while (agentite_spatial_iter_valid(&iter)) {
 *     uint32_t entity = agentite_spatial_iter_get(&iter);
 *     // process entity...
 *     agentite_spatial_iter_next(&iter);
 * }
 * @endcode
 */
Agentite_SpatialIterator agentite_spatial_iter_begin(Agentite_SpatialIndex *index, int x, int y);

/**
 * @brief Check if iterator is still valid
 *
 * @param iter Iterator
 * @return true if more entities remain
 */
bool agentite_spatial_iter_valid(const Agentite_SpatialIterator *iter);

/**
 * @brief Get current entity from iterator
 *
 * @param iter Iterator
 * @return Current entity ID
 */
uint32_t agentite_spatial_iter_get(const Agentite_SpatialIterator *iter);

/**
 * @brief Advance iterator to next entity
 *
 * @param iter Iterator
 */
void agentite_spatial_iter_next(Agentite_SpatialIterator *iter);

/* ============================================================================
 * Statistics
 * ========================================================================= */

/**
 * @brief Get total number of entities in the index
 *
 * @param index Spatial index
 * @return Total entity count (including duplicates)
 */
int agentite_spatial_total_count(Agentite_SpatialIndex *index);

/**
 * @brief Get number of occupied cells
 *
 * @param index Spatial index
 * @return Number of cells with at least one entity
 */
int agentite_spatial_occupied_cells(Agentite_SpatialIndex *index);

/**
 * @brief Get hash table load factor
 *
 * @param index Spatial index
 * @return Load factor (0.0 to 1.0, higher = more collisions)
 */
float agentite_spatial_load_factor(Agentite_SpatialIndex *index);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SPATIAL_H */
