#ifndef AGENTITE_LINE_H
#define AGENTITE_LINE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Bresenham Line Cell Iterator
 *
 * Iterate over grid cells along a line using Bresenham's algorithm.
 * Useful for pathfinding, line-of-sight, construction cost calculation,
 * and drawing lines on tilemaps.
 *
 * Usage:
 *   // Callback example: check if all cells are walkable
 *   bool check_walkable(int32_t x, int32_t y, void *userdata) {
 *       Tilemap *map = userdata;
 *       return tilemap_is_walkable(map, x, y);  // Return false to stop early
 *   }
 *
 *   // Returns true if all cells are walkable (callback never returned false)
 *   bool clear = agentite_iterate_line_cells(x1, y1, x2, y2, check_walkable, map);
 *
 *   // Count cells along a line (excluding endpoints)
 *   int cells = agentite_count_line_cells(0, 0, 10, 5);
 */

/**
 * Callback function for line iteration.
 * Return false to stop iteration early (e.g., hit an obstacle).
 *
 * @param x Current cell X coordinate
 * @param y Current cell Y coordinate
 * @param userdata User-provided data pointer
 * @return true to continue iteration, false to stop
 */
typedef bool (*Agentite_LineCellCallback)(int32_t x, int32_t y, void *userdata);

/**
 * Iterate over all cells along a line from (from_x, from_y) to (to_x, to_y).
 * Uses Bresenham's line algorithm for accurate rasterization.
 *
 * @param from_x Starting X coordinate
 * @param from_y Starting Y coordinate
 * @param to_x Ending X coordinate
 * @param to_y Ending Y coordinate
 * @param callback Function called for each cell
 * @param userdata User data passed to callback
 * @return true if iteration completed, false if callback stopped it early
 */
bool agentite_iterate_line_cells(int32_t from_x, int32_t from_y,
                                int32_t to_x, int32_t to_y,
                                Agentite_LineCellCallback callback,
                                void *userdata);

/**
 * Iterate over cells along a line with options to skip endpoints.
 * Useful when endpoints are special (e.g., cities in a railroad game).
 *
 * @param from_x Starting X coordinate
 * @param from_y Starting Y coordinate
 * @param to_x Ending X coordinate
 * @param to_y Ending Y coordinate
 * @param callback Function called for each cell
 * @param userdata User data passed to callback
 * @param skip_start If true, don't call callback for starting cell
 * @param skip_end If true, don't call callback for ending cell
 * @return true if iteration completed, false if callback stopped it early
 */
bool agentite_iterate_line_cells_ex(int32_t from_x, int32_t from_y,
                                   int32_t to_x, int32_t to_y,
                                   Agentite_LineCellCallback callback,
                                   void *userdata,
                                   bool skip_start,
                                   bool skip_end);

/**
 * Count the number of cells along a line (including both endpoints).
 *
 * @param from_x Starting X coordinate
 * @param from_y Starting Y coordinate
 * @param to_x Ending X coordinate
 * @param to_y Ending Y coordinate
 * @return Number of cells along the line
 */
int agentite_count_line_cells(int32_t from_x, int32_t from_y,
                            int32_t to_x, int32_t to_y);

/**
 * Count the number of cells along a line excluding endpoints.
 * Useful for calculating intermediate cells (e.g., track segments between cities).
 *
 * @param from_x Starting X coordinate
 * @param from_y Starting Y coordinate
 * @param to_x Ending X coordinate
 * @param to_y Ending Y coordinate
 * @return Number of cells along the line excluding endpoints
 */
int agentite_count_line_cells_between(int32_t from_x, int32_t from_y,
                                     int32_t to_x, int32_t to_y);

/**
 * Get all cells along a line into a buffer.
 *
 * @param from_x Starting X coordinate
 * @param from_y Starting Y coordinate
 * @param to_x Ending X coordinate
 * @param to_y Ending Y coordinate
 * @param out_x Output buffer for X coordinates (must be at least max_cells)
 * @param out_y Output buffer for Y coordinates (must be at least max_cells)
 * @param max_cells Maximum number of cells to store
 * @return Number of cells written to buffers
 */
int agentite_get_line_cells(int32_t from_x, int32_t from_y,
                          int32_t to_x, int32_t to_y,
                          int32_t *out_x, int32_t *out_y,
                          int max_cells);

/**
 * Check if a line between two points would pass through a specific cell.
 *
 * @param from_x Line starting X
 * @param from_y Line starting Y
 * @param to_x Line ending X
 * @param to_y Line ending Y
 * @param cell_x Cell X to check
 * @param cell_y Cell Y to check
 * @return true if the line passes through the cell
 */
bool agentite_line_passes_through(int32_t from_x, int32_t from_y,
                                 int32_t to_x, int32_t to_y,
                                 int32_t cell_x, int32_t cell_y);

#endif /* AGENTITE_LINE_H */
