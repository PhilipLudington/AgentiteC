/**
 * @file network.c
 * @brief Network/Graph System implementation
 *
 * Uses union-find (disjoint set) algorithm with path compression and
 * union by rank for efficient connected component grouping.
 *
 * Connectivity: Nodes connect when their coverage areas overlap,
 * i.e., when distance <= radius1 + radius2 (Chebyshev distance).
 */

#include "agentite/agentite.h"
#include "agentite/network.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Internal node structure with union-find data
 */
typedef struct NetworkNodeInternal {
    Agentite_NetworkNode node;    /**< Public node data */
    uint32_t parent;            /**< Union-find parent (self = root) */
    int rank;                   /**< Union-find rank (for union by rank) */
    bool in_use;                /**< Whether this slot is in use */
} NetworkNodeInternal;

/**
 * @brief Network system structure
 */
struct Agentite_NetworkSystem {
    NetworkNodeInternal *nodes;     /**< Node array */
    int capacity;                   /**< Maximum nodes */
    int count;                      /**< Current node count */
    uint32_t next_id;               /**< Next node ID to assign */

    bool dirty;                     /**< True if needs recalculation */

    /* Group data (calculated during update) */
    Agentite_NetworkGroup *groups;    /**< Group array */
    int group_count;                /**< Number of groups */
    int group_capacity;             /**< Group array capacity */

    /* Callback */
    Agentite_NetworkCallback callback;
    void *callback_userdata;
};

/* ============================================================================
 * Helper Functions
 * ========================================================================= */

/**
 * @brief Calculate Chebyshev distance between two points
 */
static inline int chebyshev_distance(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);
    return (dx > dy) ? dx : dy;
}

/**
 * @brief Find node slot by ID
 */
static NetworkNodeInternal *find_node(Agentite_NetworkSystem *network, uint32_t node_id) {
    if (node_id == AGENTITE_NETWORK_INVALID || node_id >= network->next_id) {
        return NULL;
    }

    /* Linear search - could optimize with hash table for large networks */
    for (int i = 0; i < network->capacity; i++) {
        if (network->nodes[i].in_use && network->nodes[i].node.id == node_id) {
            return &network->nodes[i];
        }
    }
    return NULL;
}

/**
 * @brief Find first empty slot
 */
static int find_empty_slot(Agentite_NetworkSystem *network) {
    for (int i = 0; i < network->capacity; i++) {
        if (!network->nodes[i].in_use) {
            return i;
        }
    }
    return -1;
}

/* ============================================================================
 * Union-Find Operations
 * ========================================================================= */

/**
 * @brief Find root of node with path compression
 */
static uint32_t uf_find(Agentite_NetworkSystem *network, uint32_t node_id) {
    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return AGENTITE_NETWORK_INVALID;

    /* Path compression: make every node on path point to root */
    if (node->parent != node_id) {
        node->parent = uf_find(network, node->parent);
    }
    return node->parent;
}

/**
 * @brief Union two nodes into same group (union by rank)
 */
static void uf_union(Agentite_NetworkSystem *network, uint32_t id1, uint32_t id2) {
    uint32_t root1 = uf_find(network, id1);
    uint32_t root2 = uf_find(network, id2);

    if (root1 == AGENTITE_NETWORK_INVALID || root2 == AGENTITE_NETWORK_INVALID) return;
    if (root1 == root2) return;  /* Already in same group */

    NetworkNodeInternal *r1 = find_node(network, root1);
    NetworkNodeInternal *r2 = find_node(network, root2);
    if (!r1 || !r2) return;

    /* Union by rank: attach smaller tree under larger */
    if (r1->rank < r2->rank) {
        r1->parent = root2;
    } else if (r1->rank > r2->rank) {
        r2->parent = root1;
    } else {
        r2->parent = root1;
        r1->rank++;
    }
}

/**
 * @brief Reset union-find structure for all nodes
 */
static void uf_reset(Agentite_NetworkSystem *network) {
    for (int i = 0; i < network->capacity; i++) {
        if (network->nodes[i].in_use) {
            network->nodes[i].parent = network->nodes[i].node.id;
            network->nodes[i].rank = 0;
        }
    }
}

/* ============================================================================
 * Connectivity Calculation
 * ========================================================================= */

/**
 * @brief Check if two nodes can connect (coverage overlap)
 */
static bool nodes_can_connect(const Agentite_NetworkNode *a, const Agentite_NetworkNode *b) {
    if (!a->active || !b->active) return false;

    int dist = chebyshev_distance(a->x, a->y, b->x, b->y);
    return dist <= (a->radius + b->radius);
}

/**
 * @brief Build connected components
 */
static void build_connectivity(Agentite_NetworkSystem *network) {
    /* Reset union-find */
    uf_reset(network);

    /* Connect all overlapping nodes */
    for (int i = 0; i < network->capacity; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        for (int j = i + 1; j < network->capacity; j++) {
            if (!network->nodes[j].in_use || !network->nodes[j].node.active) continue;

            if (nodes_can_connect(&network->nodes[i].node, &network->nodes[j].node)) {
                uf_union(network, network->nodes[i].node.id, network->nodes[j].node.id);
            }
        }
    }
}

/**
 * @brief Build group information from union-find results
 */
static void build_groups(Agentite_NetworkSystem *network) {
    /* Clear existing groups */
    network->group_count = 0;

    /* Find all unique roots and build groups */
    for (int i = 0; i < network->capacity; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        uint32_t root = uf_find(network, network->nodes[i].node.id);
        network->nodes[i].node.group = root;

        /* Find or create group for this root */
        int group_idx = -1;
        for (int g = 0; g < network->group_count; g++) {
            if (network->groups[g].id == root) {
                group_idx = g;
                break;
            }
        }

        if (group_idx < 0) {
            /* Create new group */
            if (network->group_count >= network->group_capacity) {
                /* Grow groups array */
                int new_cap = network->group_capacity * 2;
                if (new_cap < 16) new_cap = 16;
                Agentite_NetworkGroup *new_groups = AGENTITE_REALLOC(network->groups,
                                                           Agentite_NetworkGroup, new_cap);
                if (!new_groups) continue;
                network->groups = new_groups;
                network->group_capacity = new_cap;
            }

            group_idx = network->group_count++;
            network->groups[group_idx].id = root;
            network->groups[group_idx].total_production = 0;
            network->groups[group_idx].total_consumption = 0;
            network->groups[group_idx].balance = 0;
            network->groups[group_idx].node_count = 0;
            network->groups[group_idx].powered = false;
        }

        /* Add node contribution to group */
        network->groups[group_idx].total_production += network->nodes[i].node.production;
        network->groups[group_idx].total_consumption += network->nodes[i].node.consumption;
        network->groups[group_idx].node_count++;
    }

    /* Calculate balance and powered state */
    for (int g = 0; g < network->group_count; g++) {
        network->groups[g].balance = network->groups[g].total_production -
                                     network->groups[g].total_consumption;
        network->groups[g].powered = network->groups[g].balance >= 0;
    }
}

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

Agentite_NetworkSystem *agentite_network_create(void) {
    Agentite_NetworkSystem *network = AGENTITE_ALLOC(Agentite_NetworkSystem);
    if (!network) {
        agentite_set_error("Network: Failed to allocate system");
        return NULL;
    }

    int initial_capacity = 64;
    network->nodes = AGENTITE_ALLOC_ARRAY(NetworkNodeInternal, initial_capacity);
    if (!network->nodes) {
        agentite_set_error("Network: Failed to allocate nodes");
        free(network);
        return NULL;
    }

    network->capacity = initial_capacity;
    network->count = 0;
    network->next_id = 1;  /* Start at 1, 0 is invalid */
    network->dirty = false;

    network->groups = NULL;
    network->group_count = 0;
    network->group_capacity = 0;

    network->callback = NULL;
    network->callback_userdata = NULL;

    return network;
}

void agentite_network_destroy(Agentite_NetworkSystem *network) {
    if (!network) return;
    free(network->nodes);
    free(network->groups);
    free(network);
}

void agentite_network_clear(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR(network);

    for (int i = 0; i < network->capacity; i++) {
        network->nodes[i].in_use = false;
    }
    network->count = 0;
    network->group_count = 0;
    network->dirty = false;
}

/* ============================================================================
 * Node Management
 * ========================================================================= */

uint32_t agentite_network_add_node(Agentite_NetworkSystem *network, int x, int y, int radius) {
    AGENTITE_VALIDATE_PTR_RET(network, AGENTITE_NETWORK_INVALID);

    /* Check if we need to grow */
    if (network->count >= network->capacity) {
        int new_cap = network->capacity * 2;
        NetworkNodeInternal *new_nodes = (NetworkNodeInternal*)realloc(network->nodes,
                                                  new_cap * sizeof(NetworkNodeInternal));
        if (!new_nodes) {
            agentite_set_error("Network: Failed to grow node array (%d to %d nodes)", network->capacity, new_cap);
            return AGENTITE_NETWORK_INVALID;
        }

        /* Initialize new slots */
        memset(&new_nodes[network->capacity], 0,
               (new_cap - network->capacity) * sizeof(NetworkNodeInternal));

        network->nodes = new_nodes;
        network->capacity = new_cap;
    }

    int slot = find_empty_slot(network);
    if (slot < 0) {
        agentite_set_error("Network: No empty slot (%d/%d nodes)", network->count, network->capacity);
        return AGENTITE_NETWORK_INVALID;
    }

    uint32_t id = network->next_id++;
    NetworkNodeInternal *node = &network->nodes[slot];

    node->node.id = id;
    node->node.x = x;
    node->node.y = y;
    node->node.radius = radius;
    node->node.production = 0;
    node->node.consumption = 0;
    node->node.group = id;  /* Initially own group */
    node->node.active = true;
    node->parent = id;
    node->rank = 0;
    node->in_use = true;

    network->count++;
    network->dirty = true;

    return id;
}

bool agentite_network_remove_node(Agentite_NetworkSystem *network, uint32_t node_id) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return false;

    node->in_use = false;
    network->count--;
    network->dirty = true;

    return true;
}

bool agentite_network_move_node(Agentite_NetworkSystem *network, uint32_t node_id, int new_x, int new_y) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return false;

    node->node.x = new_x;
    node->node.y = new_y;
    network->dirty = true;

    return true;
}

bool agentite_network_set_radius(Agentite_NetworkSystem *network, uint32_t node_id, int radius) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return false;

    node->node.radius = radius;
    network->dirty = true;

    return true;
}

bool agentite_network_set_active(Agentite_NetworkSystem *network, uint32_t node_id, bool active) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return false;

    if (node->node.active != active) {
        node->node.active = active;
        network->dirty = true;
    }

    return true;
}

const Agentite_NetworkNode *agentite_network_get_node(Agentite_NetworkSystem *network, uint32_t node_id) {
    AGENTITE_VALIDATE_PTR_RET(network, NULL);

    NetworkNodeInternal *node = find_node(network, node_id);
    return node ? &node->node : NULL;
}

/* ============================================================================
 * Resource Management
 * ========================================================================= */

bool agentite_network_set_production(Agentite_NetworkSystem *network, uint32_t node_id, int32_t production) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return false;

    node->node.production = production;
    network->dirty = true;
    return true;
}

bool agentite_network_set_consumption(Agentite_NetworkSystem *network, uint32_t node_id, int32_t consumption) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return false;

    node->node.consumption = consumption;
    network->dirty = true;
    return true;
}

int32_t agentite_network_add_production(Agentite_NetworkSystem *network, uint32_t node_id, int32_t amount) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return 0;

    node->node.production += amount;
    network->dirty = true;
    return node->node.production;
}

int32_t agentite_network_add_consumption(Agentite_NetworkSystem *network, uint32_t node_id, int32_t amount) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return 0;

    node->node.consumption += amount;
    network->dirty = true;
    return node->node.consumption;
}

/* ============================================================================
 * Network Update and Queries
 * ========================================================================= */

void agentite_network_update(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR(network);

    if (!network->dirty) return;
    agentite_network_recalculate(network);
}

void agentite_network_recalculate(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR(network);

    /* Store old groups for callback */
    uint32_t *old_groups = NULL;
    if (network->callback) {
        old_groups = (uint32_t*)malloc(network->capacity * sizeof(uint32_t));
        if (old_groups) {
            for (int i = 0; i < network->capacity; i++) {
                if (network->nodes[i].in_use) {
                    old_groups[i] = network->nodes[i].node.group;
                }
            }
        }
    }

    build_connectivity(network);
    build_groups(network);
    network->dirty = false;

    /* Fire callbacks for group changes */
    if (network->callback && old_groups) {
        for (int i = 0; i < network->capacity; i++) {
            if (network->nodes[i].in_use) {
                uint32_t new_group = network->nodes[i].node.group;
                if (old_groups[i] != new_group) {
                    network->callback(network, network->nodes[i].node.id,
                                       old_groups[i], new_group,
                                       network->callback_userdata);
                }
            }
        }
        free(old_groups);
    }
}

bool agentite_network_is_dirty(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR_RET(network, false);
    return network->dirty;
}

uint32_t agentite_network_get_group(Agentite_NetworkSystem *network, uint32_t node_id) {
    AGENTITE_VALIDATE_PTR_RET(network, AGENTITE_NETWORK_INVALID);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return AGENTITE_NETWORK_INVALID;

    return node->node.group;
}

bool agentite_network_get_group_info(Agentite_NetworkSystem *network,
                                    uint32_t group_id,
                                    Agentite_NetworkGroup *out_group) {
    AGENTITE_VALIDATE_PTR_RET(network, false);
    AGENTITE_VALIDATE_PTR_RET(out_group, false);

    for (int g = 0; g < network->group_count; g++) {
        if (network->groups[g].id == group_id) {
            *out_group = network->groups[g];
            return true;
        }
    }
    return false;
}

bool agentite_network_is_powered(Agentite_NetworkSystem *network, uint32_t group_id) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    for (int g = 0; g < network->group_count; g++) {
        if (network->groups[g].id == group_id) {
            return network->groups[g].powered;
        }
    }
    return false;
}

bool agentite_network_node_is_powered(Agentite_NetworkSystem *network, uint32_t node_id) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node || !node->node.active) return false;

    return agentite_network_is_powered(network, node->node.group);
}

/* ============================================================================
 * Coverage Queries
 * ========================================================================= */

bool agentite_network_covers_cell(Agentite_NetworkSystem *network, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    for (int i = 0; i < network->capacity; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        int dist = chebyshev_distance(x, y, network->nodes[i].node.x, network->nodes[i].node.y);
        if (dist <= network->nodes[i].node.radius) {
            return true;
        }
    }
    return false;
}

bool agentite_network_cell_is_powered(Agentite_NetworkSystem *network, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(network, false);

    for (int i = 0; i < network->capacity; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        int dist = chebyshev_distance(x, y, network->nodes[i].node.x, network->nodes[i].node.y);
        if (dist <= network->nodes[i].node.radius) {
            /* Check if this node's group is powered */
            if (agentite_network_is_powered(network, network->nodes[i].node.group)) {
                return true;
            }
        }
    }
    return false;
}

int agentite_network_get_coverage(Agentite_NetworkSystem *network, int x, int y,
                                 Agentite_NetworkCoverage *out_coverage, int max_results) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    AGENTITE_VALIDATE_PTR_RET(out_coverage, 0);
    if (max_results <= 0) return 0;

    int count = 0;
    for (int i = 0; i < network->capacity && count < max_results; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        int dist = chebyshev_distance(x, y, network->nodes[i].node.x, network->nodes[i].node.y);
        if (dist <= network->nodes[i].node.radius) {
            out_coverage[count].node_id = network->nodes[i].node.id;
            out_coverage[count].x = network->nodes[i].node.x;
            out_coverage[count].y = network->nodes[i].node.y;
            out_coverage[count].distance = dist;
            count++;
        }
    }
    return count;
}

uint32_t agentite_network_get_nearest_node(Agentite_NetworkSystem *network, int x, int y, int max_distance) {
    AGENTITE_VALIDATE_PTR_RET(network, AGENTITE_NETWORK_INVALID);

    uint32_t nearest_id = AGENTITE_NETWORK_INVALID;
    int nearest_dist = (max_distance < 0) ? INT32_MAX : max_distance + 1;

    for (int i = 0; i < network->capacity; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        int dist = chebyshev_distance(x, y, network->nodes[i].node.x, network->nodes[i].node.y);
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest_id = network->nodes[i].node.id;
        }
    }
    return nearest_id;
}

int agentite_network_get_node_coverage(Agentite_NetworkSystem *network, uint32_t node_id,
                                      int32_t *out_x, int32_t *out_y, int max_cells) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    AGENTITE_VALIDATE_PTR_RET(out_x, 0);
    AGENTITE_VALIDATE_PTR_RET(out_y, 0);
    if (max_cells <= 0) return 0;

    NetworkNodeInternal *node = find_node(network, node_id);
    if (!node) return 0;

    int cx = node->node.x;
    int cy = node->node.y;
    int r = node->node.radius;
    int count = 0;

    for (int dy = -r; dy <= r && count < max_cells; dy++) {
        for (int dx = -r; dx <= r && count < max_cells; dx++) {
            /* Chebyshev distance check is implicit in loop bounds */
            out_x[count] = cx + dx;
            out_y[count] = cy + dy;
            count++;
        }
    }
    return count;
}

/* ============================================================================
 * Node Iteration
 * ========================================================================= */

int agentite_network_get_group_nodes(Agentite_NetworkSystem *network, uint32_t group_id,
                                    uint32_t *out_nodes, int max_nodes) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    AGENTITE_VALIDATE_PTR_RET(out_nodes, 0);
    if (max_nodes <= 0) return 0;

    int count = 0;
    for (int i = 0; i < network->capacity && count < max_nodes; i++) {
        if (!network->nodes[i].in_use || !network->nodes[i].node.active) continue;

        if (network->nodes[i].node.group == group_id) {
            out_nodes[count++] = network->nodes[i].node.id;
        }
    }
    return count;
}

int agentite_network_get_all_groups(Agentite_NetworkSystem *network,
                                   uint32_t *out_groups, int max_groups) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    AGENTITE_VALIDATE_PTR_RET(out_groups, 0);
    if (max_groups <= 0) return 0;

    int count = 0;
    for (int g = 0; g < network->group_count && count < max_groups; g++) {
        out_groups[count++] = network->groups[g].id;
    }
    return count;
}

int agentite_network_get_all_nodes(Agentite_NetworkSystem *network,
                                  uint32_t *out_nodes, int max_nodes) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    AGENTITE_VALIDATE_PTR_RET(out_nodes, 0);
    if (max_nodes <= 0) return 0;

    int count = 0;
    for (int i = 0; i < network->capacity && count < max_nodes; i++) {
        if (network->nodes[i].in_use) {
            out_nodes[count++] = network->nodes[i].node.id;
        }
    }
    return count;
}

/* ============================================================================
 * Statistics
 * ========================================================================= */

int agentite_network_node_count(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    return network->count;
}

int agentite_network_group_count(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    return network->group_count;
}

int32_t agentite_network_total_production(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);

    int32_t total = 0;
    for (int i = 0; i < network->capacity; i++) {
        if (network->nodes[i].in_use && network->nodes[i].node.active) {
            total += network->nodes[i].node.production;
        }
    }
    return total;
}

int32_t agentite_network_total_consumption(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);

    int32_t total = 0;
    for (int i = 0; i < network->capacity; i++) {
        if (network->nodes[i].in_use && network->nodes[i].node.active) {
            total += network->nodes[i].node.consumption;
        }
    }
    return total;
}

int32_t agentite_network_total_balance(Agentite_NetworkSystem *network) {
    AGENTITE_VALIDATE_PTR_RET(network, 0);
    return agentite_network_total_production(network) - agentite_network_total_consumption(network);
}

/* ============================================================================
 * Callbacks
 * ========================================================================= */

void agentite_network_set_callback(Agentite_NetworkSystem *network,
                                  Agentite_NetworkCallback callback,
                                  void *userdata) {
    AGENTITE_VALIDATE_PTR(network);
    network->callback = callback;
    network->callback_userdata = userdata;
}

/* ============================================================================
 * Debug/Visualization
 * ========================================================================= */

void agentite_network_get_stats(Agentite_NetworkSystem *network,
                               int *out_nodes, int *out_active,
                               int *out_groups, int *out_powered) {
    AGENTITE_VALIDATE_PTR(network);

    int nodes = 0;
    int active = 0;
    int powered = 0;

    for (int i = 0; i < network->capacity; i++) {
        if (network->nodes[i].in_use) {
            nodes++;
            if (network->nodes[i].node.active) {
                active++;
            }
        }
    }

    for (int g = 0; g < network->group_count; g++) {
        if (network->groups[g].powered) {
            powered++;
        }
    }

    if (out_nodes) *out_nodes = nodes;
    if (out_active) *out_active = active;
    if (out_groups) *out_groups = network->group_count;
    if (out_powered) *out_powered = powered;
}
