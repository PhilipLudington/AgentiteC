/**
 * @file fog.h
 * @brief Fog of War / Exploration System
 *
 * Per-cell exploration tracking with visibility radius. Supports three visibility
 * states: unexplored, explored (shroud), and visible. Vision sources (units, buildings)
 * reveal areas within their radius.
 *
 * Features:
 * - Three visibility states per cell
 * - Multiple vision sources with different radii
 * - Efficient visibility recalculation
 * - Integration with sprite renderer for alpha-based shroud
 * - Event callbacks for exploration (optional)
 */

#ifndef AGENTITE_FOG_H
#define AGENTITE_FOG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of vision sources */
#ifndef AGENTITE_FOG_MAX_SOURCES
#define AGENTITE_FOG_MAX_SOURCES 256
#endif

/**
 * @brief Visibility state for a cell
 */
typedef enum Agentite_VisibilityState {
    AGENTITE_VIS_UNEXPLORED = 0,  /**< Never seen - completely black */
    AGENTITE_VIS_EXPLORED   = 1,  /**< Previously seen - shroud/fog */
    AGENTITE_VIS_VISIBLE    = 2   /**< Currently visible - full clarity */
} Agentite_VisibilityState;

/**
 * @brief Opaque fog of war handle
 */
typedef struct Agentite_FogOfWar Agentite_FogOfWar;

/**
 * @brief Vision source handle
 */
typedef uint32_t Agentite_VisionSource;

/**
 * @brief Invalid vision source constant
 */
#define AGENTITE_VISION_SOURCE_INVALID 0

/**
 * @brief Callback for when a cell is explored for the first time
 *
 * @param fog Fog of war system
 * @param x Cell X coordinate
 * @param y Cell Y coordinate
 * @param userdata User data pointer
 */
typedef void (*Agentite_ExplorationCallback)(Agentite_FogOfWar *fog, int x, int y, void *userdata);

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

/**
 * @brief Create a fog of war system
 *
 * @param width Map width in cells
 * @param height Map height in cells
 * @return New fog of war system or NULL on failure
 */
Agentite_FogOfWar *agentite_fog_create(int width, int height);

/**
 * @brief Destroy a fog of war system
 *
 * @param fog Fog of war system to destroy
 */
void agentite_fog_destroy(Agentite_FogOfWar *fog);

/**
 * @brief Reset fog to unexplored state
 *
 * @param fog Fog of war system
 */
void agentite_fog_reset(Agentite_FogOfWar *fog);

/**
 * @brief Reveal entire map (cheat/debug)
 *
 * @param fog Fog of war system
 */
void agentite_fog_reveal_all(Agentite_FogOfWar *fog);

/**
 * @brief Mark entire map as explored (but not visible)
 *
 * @param fog Fog of war system
 */
void agentite_fog_explore_all(Agentite_FogOfWar *fog);

/* ============================================================================
 * Vision Sources
 * ========================================================================= */

/**
 * @brief Add a vision source at a position
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param radius Vision radius in cells
 * @return Vision source handle or AGENTITE_VISION_SOURCE_INVALID on failure
 */
Agentite_VisionSource agentite_fog_add_source(Agentite_FogOfWar *fog, int x, int y, int radius);

/**
 * @brief Remove a vision source
 *
 * @param fog Fog of war system
 * @param source Vision source handle
 */
void agentite_fog_remove_source(Agentite_FogOfWar *fog, Agentite_VisionSource source);

/**
 * @brief Move a vision source to a new position
 *
 * @param fog Fog of war system
 * @param source Vision source handle
 * @param new_x New X coordinate
 * @param new_y New Y coordinate
 */
void agentite_fog_move_source(Agentite_FogOfWar *fog, Agentite_VisionSource source, int new_x, int new_y);

/**
 * @brief Update a vision source's radius
 *
 * @param fog Fog of war system
 * @param source Vision source handle
 * @param new_radius New vision radius
 */
void agentite_fog_set_source_radius(Agentite_FogOfWar *fog, Agentite_VisionSource source, int new_radius);

/**
 * @brief Get a vision source's position and radius
 *
 * @param fog Fog of war system
 * @param source Vision source handle
 * @param out_x Output X coordinate (can be NULL)
 * @param out_y Output Y coordinate (can be NULL)
 * @param out_radius Output radius (can be NULL)
 * @return true if source exists
 */
bool agentite_fog_get_source(const Agentite_FogOfWar *fog, Agentite_VisionSource source,
                           int *out_x, int *out_y, int *out_radius);

/**
 * @brief Clear all vision sources
 *
 * @param fog Fog of war system
 *
 * @note Also recalculates visibility
 */
void agentite_fog_clear_sources(Agentite_FogOfWar *fog);

/**
 * @brief Get number of active vision sources
 *
 * @param fog Fog of war system
 * @return Number of sources
 */
int agentite_fog_source_count(const Agentite_FogOfWar *fog);

/* ============================================================================
 * Visibility Updates
 * ========================================================================= */

/**
 * @brief Recalculate visibility from all sources
 *
 * Call this after adding/removing/moving sources. The system tracks a dirty
 * flag and only recalculates when necessary.
 *
 * @param fog Fog of war system
 */
void agentite_fog_update(Agentite_FogOfWar *fog);

/**
 * @brief Force visibility recalculation
 *
 * @param fog Fog of war system
 */
void agentite_fog_force_update(Agentite_FogOfWar *fog);

/* ============================================================================
 * Visibility Queries
 * ========================================================================= */

/**
 * @brief Get visibility state of a cell
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return Visibility state
 */
Agentite_VisibilityState agentite_fog_get_state(const Agentite_FogOfWar *fog, int x, int y);

/**
 * @brief Check if a cell is currently visible
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if visible
 */
bool agentite_fog_is_visible(const Agentite_FogOfWar *fog, int x, int y);

/**
 * @brief Check if a cell has been explored (visible or previously seen)
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if explored or visible
 */
bool agentite_fog_is_explored(const Agentite_FogOfWar *fog, int x, int y);

/**
 * @brief Check if a cell is unexplored
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if unexplored
 */
bool agentite_fog_is_unexplored(const Agentite_FogOfWar *fog, int x, int y);

/**
 * @brief Get alpha value for rendering (0.0 = black, 1.0 = visible)
 *
 * Returns render alpha based on visibility:
 * - Unexplored: 0.0 (fully hidden)
 * - Explored: shroud_alpha (customizable, default 0.5)
 * - Visible: 1.0 (fully visible)
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return Alpha value 0.0 to 1.0
 */
float agentite_fog_get_alpha(const Agentite_FogOfWar *fog, int x, int y);

/**
 * @brief Set shroud (explored but not visible) alpha
 *
 * @param fog Fog of war system
 * @param alpha Shroud alpha (0.0 to 1.0, default 0.5)
 */
void agentite_fog_set_shroud_alpha(Agentite_FogOfWar *fog, float alpha);

/**
 * @brief Get shroud alpha
 *
 * @param fog Fog of war system
 * @return Current shroud alpha
 */
float agentite_fog_get_shroud_alpha(const Agentite_FogOfWar *fog);

/* ============================================================================
 * Region Queries
 * ========================================================================= */

/**
 * @brief Check if any cell in a rectangle is visible
 *
 * @param fog Fog of war system
 * @param x1 Left X (inclusive)
 * @param y1 Top Y (inclusive)
 * @param x2 Right X (inclusive)
 * @param y2 Bottom Y (inclusive)
 * @return true if any cell is visible
 */
bool agentite_fog_any_visible_in_rect(const Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2);

/**
 * @brief Check if all cells in a rectangle are visible
 *
 * @param fog Fog of war system
 * @param x1 Left X (inclusive)
 * @param y1 Top Y (inclusive)
 * @param x2 Right X (inclusive)
 * @param y2 Bottom Y (inclusive)
 * @return true if all cells are visible
 */
bool agentite_fog_all_visible_in_rect(const Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2);

/**
 * @brief Count visible cells in a rectangle
 *
 * @param fog Fog of war system
 * @param x1 Left X (inclusive)
 * @param y1 Top Y (inclusive)
 * @param x2 Right X (inclusive)
 * @param y2 Bottom Y (inclusive)
 * @return Number of visible cells
 */
int agentite_fog_count_visible_in_rect(const Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2);

/* ============================================================================
 * Manual Exploration
 * ========================================================================= */

/**
 * @brief Manually explore a cell (make it explored/visible)
 *
 * @param fog Fog of war system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 */
void agentite_fog_explore_cell(Agentite_FogOfWar *fog, int x, int y);

/**
 * @brief Manually explore a rectangular region
 *
 * @param fog Fog of war system
 * @param x1 Left X (inclusive)
 * @param y1 Top Y (inclusive)
 * @param x2 Right X (inclusive)
 * @param y2 Bottom Y (inclusive)
 */
void agentite_fog_explore_rect(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2);

/**
 * @brief Manually explore a circular region
 *
 * @param fog Fog of war system
 * @param center_x Center X coordinate
 * @param center_y Center Y coordinate
 * @param radius Radius in cells
 */
void agentite_fog_explore_circle(Agentite_FogOfWar *fog, int center_x, int center_y, int radius);

/* ============================================================================
 * Callbacks
 * ========================================================================= */

/**
 * @brief Set callback for when cells are explored for the first time
 *
 * @param fog Fog of war system
 * @param callback Callback function (NULL to disable)
 * @param userdata User data passed to callback
 */
void agentite_fog_set_exploration_callback(Agentite_FogOfWar *fog,
                                          Agentite_ExplorationCallback callback,
                                          void *userdata);

/* ============================================================================
 * Statistics
 * ========================================================================= */

/**
 * @brief Get map dimensions
 *
 * @param fog Fog of war system
 * @param out_width Output width (can be NULL)
 * @param out_height Output height (can be NULL)
 */
void agentite_fog_get_size(const Agentite_FogOfWar *fog, int *out_width, int *out_height);

/**
 * @brief Get exploration statistics
 *
 * @param fog Fog of war system
 * @param out_unexplored Output unexplored count (can be NULL)
 * @param out_explored Output explored count (can be NULL)
 * @param out_visible Output visible count (can be NULL)
 */
void agentite_fog_get_stats(const Agentite_FogOfWar *fog,
                          int *out_unexplored, int *out_explored, int *out_visible);

/**
 * @brief Get exploration percentage (0.0 to 1.0)
 *
 * @param fog Fog of war system
 * @return Percentage of map that is explored or visible
 */
float agentite_fog_get_exploration_percent(const Agentite_FogOfWar *fog);

/* ============================================================================
 * Line of Sight (Optional Extension)
 * ========================================================================= */

/**
 * @brief Callback to check if a cell blocks vision
 *
 * @param x Cell X coordinate
 * @param y Cell Y coordinate
 * @param userdata User data pointer
 * @return true if cell blocks vision
 */
typedef bool (*Agentite_VisionBlockerCallback)(int x, int y, void *userdata);

/**
 * @brief Enable line-of-sight checking for vision
 *
 * When enabled, vision sources only reveal cells they have direct line of sight to,
 * checking the blocker callback for obstacles.
 *
 * @param fog Fog of war system
 * @param callback Blocker check callback (NULL to disable LOS)
 * @param userdata User data passed to callback
 */
void agentite_fog_set_los_callback(Agentite_FogOfWar *fog,
                                  Agentite_VisionBlockerCallback callback,
                                  void *userdata);

/**
 * @brief Check if there's line of sight between two cells
 *
 * Uses the blocker callback if set. Returns true if no blockers are in the way.
 *
 * @param fog Fog of war system
 * @param x1 Start X
 * @param y1 Start Y
 * @param x2 End X
 * @param y2 End Y
 * @return true if line of sight exists
 */
bool agentite_fog_has_los(Agentite_FogOfWar *fog, int x1, int y1, int x2, int y2);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_FOG_H */
