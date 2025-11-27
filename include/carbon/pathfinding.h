/*
 * Carbon Pathfinding System
 *
 * A* pathfinding for tile-based maps with support for weighted costs,
 * diagonal movement, and dynamic obstacle updates.
 *
 * Usage:
 *   // Create pathfinder for a tilemap
 *   Carbon_Pathfinder *pf = carbon_pathfinder_create(100, 100);
 *
 *   // Mark obstacles (walls, water, etc.)
 *   carbon_pathfinder_set_walkable(pf, 10, 10, false);
 *   carbon_pathfinder_set_cost(pf, 5, 5, 2.0f);  // Rough terrain
 *
 *   // Or sync with a tilemap layer
 *   carbon_pathfinder_sync_tilemap(pf, tilemap, collision_layer, blocked_tiles, count);
 *
 *   // Find path
 *   Carbon_Path *path = carbon_pathfinder_find(pf, start_x, start_y, end_x, end_y);
 *   if (path) {
 *       for (int i = 0; i < path->length; i++) {
 *           int x = path->points[i].x;
 *           int y = path->points[i].y;
 *           // Move unit along path...
 *       }
 *       carbon_path_destroy(path);
 *   }
 *
 *   // Cleanup
 *   carbon_pathfinder_destroy(pf);
 */

#ifndef CARBON_PATHFINDING_H
#define CARBON_PATHFINDING_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Carbon_Tilemap Carbon_Tilemap;

/* ============================================================================
 * Types
 * ============================================================================ */

/* Point in a path */
typedef struct Carbon_PathPoint {
    int x;
    int y;
} Carbon_PathPoint;

/* Result path from A* search */
typedef struct Carbon_Path {
    Carbon_PathPoint *points;   /* Array of points from start to end */
    int length;                 /* Number of points in path */
    float total_cost;           /* Total movement cost of path */
} Carbon_Path;

/* Pathfinding options */
typedef struct Carbon_PathOptions {
    bool allow_diagonal;        /* Allow diagonal movement (default: true) */
    float diagonal_cost;        /* Cost multiplier for diagonal (default: 1.414) */
    int max_iterations;         /* Max nodes to explore (0 = unlimited) */
    bool cut_corners;           /* Allow diagonal past corners (default: false) */
} Carbon_PathOptions;

/* Default pathfinding options */
#define CARBON_PATH_OPTIONS_DEFAULT { \
    .allow_diagonal = true, \
    .diagonal_cost = 1.41421356f, \
    .max_iterations = 0, \
    .cut_corners = false \
}

/* Opaque pathfinder type */
typedef struct Carbon_Pathfinder Carbon_Pathfinder;

/* ============================================================================
 * Pathfinder Lifecycle
 * ============================================================================ */

/* Create pathfinder for grid of given dimensions */
Carbon_Pathfinder *carbon_pathfinder_create(int width, int height);

/* Destroy pathfinder and free resources */
void carbon_pathfinder_destroy(Carbon_Pathfinder *pf);

/* Get pathfinder grid dimensions */
void carbon_pathfinder_get_size(Carbon_Pathfinder *pf, int *width, int *height);

/* ============================================================================
 * Grid Configuration
 * ============================================================================ */

/* Set whether a tile is walkable (default: true) */
void carbon_pathfinder_set_walkable(Carbon_Pathfinder *pf, int x, int y, bool walkable);

/* Get whether a tile is walkable */
bool carbon_pathfinder_is_walkable(Carbon_Pathfinder *pf, int x, int y);

/* Set movement cost for a tile (default: 1.0, higher = slower) */
void carbon_pathfinder_set_cost(Carbon_Pathfinder *pf, int x, int y, float cost);

/* Get movement cost for a tile */
float carbon_pathfinder_get_cost(Carbon_Pathfinder *pf, int x, int y);

/* Set a rectangular region as walkable/blocked */
void carbon_pathfinder_fill_walkable(Carbon_Pathfinder *pf,
                                      int x, int y, int width, int height,
                                      bool walkable);

/* Set a rectangular region to a specific cost */
void carbon_pathfinder_fill_cost(Carbon_Pathfinder *pf,
                                  int x, int y, int width, int height,
                                  float cost);

/* Reset all tiles to walkable with cost 1.0 */
void carbon_pathfinder_clear(Carbon_Pathfinder *pf);

/* ============================================================================
 * Tilemap Integration
 * ============================================================================ */

/* Sync pathfinder with tilemap - marks tiles as blocked based on tile IDs
 * blocked_tiles: array of tile IDs that should be non-walkable
 * count: number of tile IDs in the array */
void carbon_pathfinder_sync_tilemap(Carbon_Pathfinder *pf,
                                     Carbon_Tilemap *tilemap,
                                     int layer,
                                     const uint16_t *blocked_tiles,
                                     int count);

/* Sync with tilemap using a cost callback
 * cost_func: returns movement cost for tile ID (0 = blocked, >0 = walkable) */
typedef float (*Carbon_TileCostFunc)(uint16_t tile_id, void *userdata);
void carbon_pathfinder_sync_tilemap_ex(Carbon_Pathfinder *pf,
                                        Carbon_Tilemap *tilemap,
                                        int layer,
                                        Carbon_TileCostFunc cost_func,
                                        void *userdata);

/* ============================================================================
 * Pathfinding
 * ============================================================================ */

/* Find path from start to end using default options
 * Returns NULL if no path exists */
Carbon_Path *carbon_pathfinder_find(Carbon_Pathfinder *pf,
                                     int start_x, int start_y,
                                     int end_x, int end_y);

/* Find path with custom options */
Carbon_Path *carbon_pathfinder_find_ex(Carbon_Pathfinder *pf,
                                        int start_x, int start_y,
                                        int end_x, int end_y,
                                        const Carbon_PathOptions *options);

/* Check if a path exists (faster than full pathfinding) */
bool carbon_pathfinder_has_path(Carbon_Pathfinder *pf,
                                 int start_x, int start_y,
                                 int end_x, int end_y);

/* Check if a straight line is clear (no obstacles) */
bool carbon_pathfinder_line_clear(Carbon_Pathfinder *pf,
                                   int x1, int y1,
                                   int x2, int y2);

/* ============================================================================
 * Path Operations
 * ============================================================================ */

/* Destroy a path and free its memory */
void carbon_path_destroy(Carbon_Path *path);

/* Get point at index (returns NULL if out of bounds) */
const Carbon_PathPoint *carbon_path_get_point(Carbon_Path *path, int index);

/* Simplify path by removing redundant points on straight lines */
Carbon_Path *carbon_path_simplify(Carbon_Path *path);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/* Calculate Manhattan distance between two points */
int carbon_pathfinder_distance_manhattan(int x1, int y1, int x2, int y2);

/* Calculate Euclidean distance between two points */
float carbon_pathfinder_distance_euclidean(int x1, int y1, int x2, int y2);

/* Calculate Chebyshev distance (diagonal distance) */
int carbon_pathfinder_distance_chebyshev(int x1, int y1, int x2, int y2);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_PATHFINDING_H */
