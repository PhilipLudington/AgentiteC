/**
 * @file network.h
 * @brief Network/Graph System for connected component grouping with resource distribution
 *
 * Provides efficient network management for systems like power grids, water pipes,
 * or any graph where nodes form connected groups and share resources. Uses union-find
 * algorithm for O(α(n)) ≈ O(1) connectivity operations.
 *
 * Features:
 * - Union-find for fast connected component grouping
 * - Per-node production/consumption tracking
 * - Coverage area per node (radius-based)
 * - Network-wide resource balance calculation
 * - Lazy recalculation with dirty tracking
 * - Event integration for network changes
 */

#ifndef AGENTITE_NETWORK_H
#define AGENTITE_NETWORK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum nodes per network system (can be overridden at compile time) */
#ifndef AGENTITE_NETWORK_MAX_NODES
#define AGENTITE_NETWORK_MAX_NODES 4096
#endif

/* Maximum connected networks (groups) */
#ifndef AGENTITE_NETWORK_MAX_GROUPS
#define AGENTITE_NETWORK_MAX_GROUPS 256
#endif

/* Invalid node/group ID */
#define AGENTITE_NETWORK_INVALID UINT32_MAX

/**
 * @brief Opaque network system handle
 */
typedef struct Agentite_NetworkSystem Agentite_NetworkSystem;

/**
 * @brief Node information structure
 */
typedef struct Agentite_NetworkNode {
    uint32_t id;            /**< Node ID */
    int32_t x;              /**< Grid X position */
    int32_t y;              /**< Grid Y position */
    int32_t radius;         /**< Coverage radius */
    int32_t production;     /**< Resource production per tick */
    int32_t consumption;    /**< Resource consumption per tick */
    uint32_t group;         /**< Current network group (after update) */
    bool active;            /**< Whether node is active */
} Agentite_NetworkNode;

/**
 * @brief Network group (connected component) information
 */
typedef struct Agentite_NetworkGroup {
    uint32_t id;                /**< Group ID */
    int32_t total_production;   /**< Sum of all node production */
    int32_t total_consumption;  /**< Sum of all node consumption */
    int32_t balance;            /**< Production - consumption */
    int node_count;             /**< Number of nodes in group */
    bool powered;               /**< Whether balance >= 0 */
} Agentite_NetworkGroup;

/**
 * @brief Query result for coverage queries
 */
typedef struct Agentite_NetworkCoverage {
    uint32_t node_id;       /**< Node providing coverage */
    int32_t x;              /**< Node X position */
    int32_t y;              /**< Node Y position */
    int32_t distance;       /**< Distance from query point */
} Agentite_NetworkCoverage;

/**
 * @brief Callback for network change events
 */
typedef void (*Agentite_NetworkCallback)(Agentite_NetworkSystem *network,
                                        uint32_t node_id,
                                        uint32_t old_group,
                                        uint32_t new_group,
                                        void *userdata);

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

/**
 * @brief Create a network system
 *
 * @return New network system or NULL on failure
 */
Agentite_NetworkSystem *agentite_network_create(void);

/**
 * @brief Destroy a network system and free all memory
 *
 * @param network Network system to destroy
 */
void agentite_network_destroy(Agentite_NetworkSystem *network);

/**
 * @brief Clear all nodes from the network
 *
 * @param network Network system to clear
 */
void agentite_network_clear(Agentite_NetworkSystem *network);

/* ============================================================================
 * Node Management
 * ========================================================================= */

/**
 * @brief Add a node to the network
 *
 * @param network Network system
 * @param x Grid X position
 * @param y Grid Y position
 * @param radius Coverage radius (nodes within radius*2 distance connect)
 * @return Node ID or AGENTITE_NETWORK_INVALID on failure
 *
 * @note Nodes connect when their coverage areas overlap (distance <= radius1 + radius2)
 */
uint32_t agentite_network_add_node(Agentite_NetworkSystem *network, int x, int y, int radius);

/**
 * @brief Remove a node from the network
 *
 * @param network Network system
 * @param node_id Node ID to remove
 * @return true if removed, false if not found
 */
bool agentite_network_remove_node(Agentite_NetworkSystem *network, uint32_t node_id);

/**
 * @brief Move a node to a new position
 *
 * @param network Network system
 * @param node_id Node ID to move
 * @param new_x New X position
 * @param new_y New Y position
 * @return true if moved, false if not found
 *
 * @note This may change network connectivity
 */
bool agentite_network_move_node(Agentite_NetworkSystem *network, uint32_t node_id, int new_x, int new_y);

/**
 * @brief Set a node's coverage radius
 *
 * @param network Network system
 * @param node_id Node ID
 * @param radius New coverage radius
 * @return true if set, false if not found
 */
bool agentite_network_set_radius(Agentite_NetworkSystem *network, uint32_t node_id, int radius);

/**
 * @brief Set a node's active state
 *
 * Inactive nodes don't provide coverage or contribute to network balance.
 *
 * @param network Network system
 * @param node_id Node ID
 * @param active Whether node is active
 * @return true if set, false if not found
 */
bool agentite_network_set_active(Agentite_NetworkSystem *network, uint32_t node_id, bool active);

/**
 * @brief Get node information
 *
 * @param network Network system
 * @param node_id Node ID
 * @return Pointer to node info or NULL if not found
 *
 * @note Returned pointer is valid until node is removed or network is destroyed
 */
const Agentite_NetworkNode *agentite_network_get_node(Agentite_NetworkSystem *network, uint32_t node_id);

/* ============================================================================
 * Resource Management
 * ========================================================================= */

/**
 * @brief Set a node's production value
 *
 * @param network Network system
 * @param node_id Node ID
 * @param production Production amount per tick
 * @return true if set, false if not found
 */
bool agentite_network_set_production(Agentite_NetworkSystem *network, uint32_t node_id, int32_t production);

/**
 * @brief Set a node's consumption value
 *
 * @param network Network system
 * @param node_id Node ID
 * @param consumption Consumption amount per tick
 * @return true if set, false if not found
 */
bool agentite_network_set_consumption(Agentite_NetworkSystem *network, uint32_t node_id, int32_t consumption);

/**
 * @brief Add to a node's production value
 *
 * @param network Network system
 * @param node_id Node ID
 * @param amount Amount to add (can be negative)
 * @return New production value or 0 if not found
 */
int32_t agentite_network_add_production(Agentite_NetworkSystem *network, uint32_t node_id, int32_t amount);

/**
 * @brief Add to a node's consumption value
 *
 * @param network Network system
 * @param node_id Node ID
 * @param amount Amount to add (can be negative)
 * @return New consumption value or 0 if not found
 */
int32_t agentite_network_add_consumption(Agentite_NetworkSystem *network, uint32_t node_id, int32_t amount);

/* ============================================================================
 * Network Update and Queries
 * ========================================================================= */

/**
 * @brief Recalculate network connectivity
 *
 * Must be called after adding/removing/moving nodes before querying groups.
 * This is a lazy operation - only recalculates if the network is dirty.
 *
 * @param network Network system
 */
void agentite_network_update(Agentite_NetworkSystem *network);

/**
 * @brief Force recalculation of network connectivity
 *
 * @param network Network system
 */
void agentite_network_recalculate(Agentite_NetworkSystem *network);

/**
 * @brief Check if network needs recalculation
 *
 * @param network Network system
 * @return true if dirty (needs update)
 */
bool agentite_network_is_dirty(Agentite_NetworkSystem *network);

/**
 * @brief Get a node's network group
 *
 * @param network Network system
 * @param node_id Node ID
 * @return Group ID or AGENTITE_NETWORK_INVALID if not found
 *
 * @note Call agentite_network_update() first to ensure accurate results
 */
uint32_t agentite_network_get_group(Agentite_NetworkSystem *network, uint32_t node_id);

/**
 * @brief Get network group information
 *
 * @param network Network system
 * @param group_id Group ID
 * @param out_group Output structure (filled on success)
 * @return true if group exists, false otherwise
 */
bool agentite_network_get_group_info(Agentite_NetworkSystem *network,
                                    uint32_t group_id,
                                    Agentite_NetworkGroup *out_group);

/**
 * @brief Check if a network group is powered (balance >= 0)
 *
 * @param network Network system
 * @param group_id Group ID
 * @return true if powered, false if unpowered or group not found
 */
bool agentite_network_is_powered(Agentite_NetworkSystem *network, uint32_t group_id);

/**
 * @brief Check if a node is powered (its group has balance >= 0)
 *
 * @param network Network system
 * @param node_id Node ID
 * @return true if powered, false if unpowered or not found
 */
bool agentite_network_node_is_powered(Agentite_NetworkSystem *network, uint32_t node_id);

/* ============================================================================
 * Coverage Queries
 * ========================================================================= */

/**
 * @brief Check if a cell is covered by any node
 *
 * @param network Network system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if at least one active node covers this cell
 */
bool agentite_network_covers_cell(Agentite_NetworkSystem *network, int x, int y);

/**
 * @brief Check if a cell is covered by a powered network
 *
 * @param network Network system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if covered by an active node in a powered network
 */
bool agentite_network_cell_is_powered(Agentite_NetworkSystem *network, int x, int y);

/**
 * @brief Get all nodes covering a cell
 *
 * @param network Network system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param out_coverage Output array for coverage info
 * @param max_results Maximum results to return
 * @return Number of nodes covering this cell
 */
int agentite_network_get_coverage(Agentite_NetworkSystem *network, int x, int y,
                                 Agentite_NetworkCoverage *out_coverage, int max_results);

/**
 * @brief Get the nearest node to a position
 *
 * @param network Network system
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @param max_distance Maximum distance to search (-1 for unlimited)
 * @return Node ID or AGENTITE_NETWORK_INVALID if none found
 */
uint32_t agentite_network_get_nearest_node(Agentite_NetworkSystem *network, int x, int y, int max_distance);

/**
 * @brief Get all cells covered by a node
 *
 * @param network Network system
 * @param node_id Node ID
 * @param out_x Output array for X coordinates
 * @param out_y Output array for Y coordinates
 * @param max_cells Maximum cells to return
 * @return Number of cells covered
 */
int agentite_network_get_node_coverage(Agentite_NetworkSystem *network, uint32_t node_id,
                                      int32_t *out_x, int32_t *out_y, int max_cells);

/* ============================================================================
 * Node Iteration
 * ========================================================================= */

/**
 * @brief Get all nodes in a network group
 *
 * @param network Network system
 * @param group_id Group ID
 * @param out_nodes Output array for node IDs
 * @param max_nodes Maximum nodes to return
 * @return Number of nodes in group
 */
int agentite_network_get_group_nodes(Agentite_NetworkSystem *network, uint32_t group_id,
                                    uint32_t *out_nodes, int max_nodes);

/**
 * @brief Get all active network groups
 *
 * @param network Network system
 * @param out_groups Output array for group IDs
 * @param max_groups Maximum groups to return
 * @return Number of active groups
 */
int agentite_network_get_all_groups(Agentite_NetworkSystem *network,
                                   uint32_t *out_groups, int max_groups);

/**
 * @brief Get all nodes in the network
 *
 * @param network Network system
 * @param out_nodes Output array for node IDs
 * @param max_nodes Maximum nodes to return
 * @return Number of nodes
 */
int agentite_network_get_all_nodes(Agentite_NetworkSystem *network,
                                  uint32_t *out_nodes, int max_nodes);

/* ============================================================================
 * Statistics
 * ========================================================================= */

/**
 * @brief Get total number of nodes
 *
 * @param network Network system
 * @return Node count
 */
int agentite_network_node_count(Agentite_NetworkSystem *network);

/**
 * @brief Get number of active groups
 *
 * @param network Network system
 * @return Group count
 *
 * @note Call agentite_network_update() first
 */
int agentite_network_group_count(Agentite_NetworkSystem *network);

/**
 * @brief Get total production across all nodes
 *
 * @param network Network system
 * @return Total production
 */
int32_t agentite_network_total_production(Agentite_NetworkSystem *network);

/**
 * @brief Get total consumption across all nodes
 *
 * @param network Network system
 * @return Total consumption
 */
int32_t agentite_network_total_consumption(Agentite_NetworkSystem *network);

/**
 * @brief Get overall network balance (production - consumption)
 *
 * @param network Network system
 * @return Network balance
 */
int32_t agentite_network_total_balance(Agentite_NetworkSystem *network);

/* ============================================================================
 * Callbacks
 * ========================================================================= */

/**
 * @brief Set callback for network group changes
 *
 * Called when a node's group membership changes after update.
 *
 * @param network Network system
 * @param callback Callback function (NULL to disable)
 * @param userdata User data passed to callback
 */
void agentite_network_set_callback(Agentite_NetworkSystem *network,
                                  Agentite_NetworkCallback callback,
                                  void *userdata);

/* ============================================================================
 * Debug/Visualization
 * ========================================================================= */

/**
 * @brief Get debug information about the network
 *
 * @param network Network system
 * @param out_nodes Total nodes
 * @param out_active Active nodes
 * @param out_groups Number of groups
 * @param out_powered Number of powered groups
 */
void agentite_network_get_stats(Agentite_NetworkSystem *network,
                               int *out_nodes, int *out_active,
                               int *out_groups, int *out_powered);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_NETWORK_H */
