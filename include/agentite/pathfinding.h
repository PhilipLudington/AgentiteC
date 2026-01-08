/*
 * Carbon Pathfinding System
 *
 * A* pathfinding for tile-based maps with support for weighted costs,
 * diagonal movement, and dynamic obstacle updates.
 *
 * Usage:
 *   // Create pathfinder for a tilemap
 *   Agentite_Pathfinder *pf = agentite_pathfinder_create(100, 100);
 *
 *   // Mark obstacles (walls, water, etc.)
 *   agentite_pathfinder_set_walkable(pf, 10, 10, false);
 *   agentite_pathfinder_set_cost(pf, 5, 5, 2.0f);  // Rough terrain
 *
 *   // Or sync with a tilemap layer
 *   agentite_pathfinder_sync_tilemap(pf, tilemap, collision_layer, blocked_tiles, count);
 *
 *   // Find path
 *   Agentite_Path *path = agentite_pathfinder_find(pf, start_x, start_y, end_x, end_y);
 *   if (path) {
 *       for (int i = 0; i < path->length; i++) {
 *           int x = path->points[i].x;
 *           int y = path->points[i].y;
 *           // Move unit along path...
 *       }
 *       agentite_path_destroy(path);
 *   }
 *
 *   // Cleanup
 *   agentite_pathfinder_destroy(pf);
 */

#ifndef AGENTITE_PATHFINDING_H
#define AGENTITE_PATHFINDING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_Tilemap Agentite_Tilemap;

/* ============================================================================
 * Types
 * ============================================================================ */

/* Point in a path */
typedef struct Agentite_PathPoint {
    int x;
    int y;
} Agentite_PathPoint;

/* Result path from A* search */
typedef struct Agentite_Path {
    Agentite_PathPoint *points;   /* Array of points from start to end */
    int length;                 /* Number of points in path */
    float total_cost;           /* Total movement cost of path */
} Agentite_Path;

/* Pathfinding options */
typedef struct Agentite_PathOptions {
    bool allow_diagonal;        /* Allow diagonal movement (default: true) */
    float diagonal_cost;        /* Cost multiplier for diagonal (default: 1.414) */
    int max_iterations;         /* Max nodes to explore (0 = unlimited) */
    bool cut_corners;           /* Allow diagonal past corners (default: false) */
} Agentite_PathOptions;

/* Default pathfinding options */
#define AGENTITE_PATH_OPTIONS_DEFAULT { \
    .allow_diagonal = true, \
    .diagonal_cost = 1.41421356f, \
    .max_iterations = 0, \
    .cut_corners = false \
}

/* Opaque pathfinder type */
typedef struct Agentite_Pathfinder Agentite_Pathfinder;

/* ============================================================================
 * Pathfinder Lifecycle
 * ============================================================================ */

/**
 * Create pathfinder for grid of given dimensions.
 * Caller OWNS the returned pointer and MUST call agentite_pathfinder_destroy().
 */
Agentite_Pathfinder *agentite_pathfinder_create(int width, int height);

/* Destroy pathfinder and free all resources */
void agentite_pathfinder_destroy(Agentite_Pathfinder *pf);

/* Get pathfinder grid dimensions */
void agentite_pathfinder_get_size(const Agentite_Pathfinder *pf, int *width, int *height);

/* Forward declaration */
struct Agentite_Profiler;

/**
 * Set profiler for pathfinding performance tracking.
 *
 * When a profiler is set, pathfinding operations will report:
 * - "pathfinding" scope: Time spent in A* algorithm
 *
 * @param pf       Pathfinder instance (must not be NULL)
 * @param profiler Profiler instance, or NULL to disable profiling
 */
void agentite_pathfinder_set_profiler(Agentite_Pathfinder *pf,
                                      struct Agentite_Profiler *profiler);

/* ============================================================================
 * Grid Configuration
 * ============================================================================ */

/* Set whether a tile is walkable (default: true) */
void agentite_pathfinder_set_walkable(Agentite_Pathfinder *pf, int x, int y, bool walkable);

/* Get whether a tile is walkable */
bool agentite_pathfinder_is_walkable(const Agentite_Pathfinder *pf, int x, int y);

/* Set movement cost for a tile (default: 1.0, higher = slower) */
void agentite_pathfinder_set_cost(Agentite_Pathfinder *pf, int x, int y, float cost);

/* Get movement cost for a tile */
float agentite_pathfinder_get_cost(const Agentite_Pathfinder *pf, int x, int y);

/* Set a rectangular region as walkable/blocked */
void agentite_pathfinder_fill_walkable(Agentite_Pathfinder *pf,
                                      int x, int y, int width, int height,
                                      bool walkable);

/* Set a rectangular region to a specific cost */
void agentite_pathfinder_fill_cost(Agentite_Pathfinder *pf,
                                  int x, int y, int width, int height,
                                  float cost);

/* Reset all tiles to walkable with cost 1.0 */
void agentite_pathfinder_clear(Agentite_Pathfinder *pf);

/* ============================================================================
 * Tilemap Integration
 * ============================================================================ */

/* Sync pathfinder with tilemap - marks tiles as blocked based on tile IDs
 * blocked_tiles: array of tile IDs that should be non-walkable
 * count: number of tile IDs in the array */
void agentite_pathfinder_sync_tilemap(Agentite_Pathfinder *pf,
                                     Agentite_Tilemap *tilemap,
                                     int layer,
                                     const uint16_t *blocked_tiles,
                                     int count);

/* Sync with tilemap using a cost callback
 * cost_func: returns movement cost for tile ID (0 = blocked, >0 = walkable) */
typedef float (*Agentite_TileCostFunc)(uint16_t tile_id, void *userdata);
void agentite_pathfinder_sync_tilemap_ex(Agentite_Pathfinder *pf,
                                        Agentite_Tilemap *tilemap,
                                        int layer,
                                        Agentite_TileCostFunc cost_func,
                                        void *userdata);

/* ============================================================================
 * Pathfinding
 * ============================================================================ */

/**
 * Find path from start to end using default options.
 * Returns NULL if no path exists.
 * Caller OWNS the returned pointer and MUST call agentite_path_destroy().
 */
Agentite_Path *agentite_pathfinder_find(Agentite_Pathfinder *pf,
                                     int start_x, int start_y,
                                     int end_x, int end_y);

/**
 * Find path with custom options.
 * Returns NULL if no path exists.
 * Caller OWNS the returned pointer and MUST call agentite_path_destroy().
 */
Agentite_Path *agentite_pathfinder_find_ex(Agentite_Pathfinder *pf,
                                        int start_x, int start_y,
                                        int end_x, int end_y,
                                        const Agentite_PathOptions *options);

/* Check if a path exists (faster than full pathfinding) */
bool agentite_pathfinder_has_path(Agentite_Pathfinder *pf,
                                 int start_x, int start_y,
                                 int end_x, int end_y);

/* Check if a straight line is clear (no obstacles) */
bool agentite_pathfinder_line_clear(Agentite_Pathfinder *pf,
                                   int x1, int y1,
                                   int x2, int y2);

/* ============================================================================
 * Path Operations
 * ============================================================================ */

/* Destroy a path and free its memory */
void agentite_path_destroy(Agentite_Path *path);

/* Get point at index (returns NULL if out of bounds) */
const Agentite_PathPoint *agentite_path_get_point(const Agentite_Path *path, int index);

/* Simplify path by removing redundant points on straight lines */
Agentite_Path *agentite_path_simplify(Agentite_Path *path);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Calculate Manhattan distance between two points */
int agentite_pathfinder_distance_manhattan(int x1, int y1, int x2, int y2);

/* Calculate Euclidean distance between two points */
float agentite_pathfinder_distance_euclidean(int x1, int y1, int x2, int y2);

/* Calculate Chebyshev distance (diagonal distance) */
int agentite_pathfinder_distance_chebyshev(int x1, int y1, int x2, int y2);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_PATHFINDING_H */
