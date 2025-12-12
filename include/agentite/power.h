/**
 * Agentite Power Network System
 *
 * Grid-based power distribution for factory and strategy games.
 * Buildings connect to power networks through poles/substations,
 * with automatic network merging and splitting.
 *
 * Ported from AgentiteZ (Zig) power system.
 *
 * Usage:
 *   // Create power system
 *   Agentite_PowerSystem *ps = agentite_power_create(100, 100);
 *
 *   // Add power poles
 *   int pole1 = agentite_power_add_pole(ps, 10, 10, 5);  // radius 5
 *   int pole2 = agentite_power_add_pole(ps, 14, 10, 5);  // connects to pole1
 *
 *   // Add generator
 *   agentite_power_add_producer(ps, 10, 10, 100);  // 100 units production
 *
 *   // Add consumer (building)
 *   agentite_power_add_consumer(ps, 12, 10, 50);  // 50 units consumption
 *
 *   // Check power status
 *   Agentite_PowerStatus status = agentite_power_get_status_at(ps, 12, 10);
 *   if (status == AGENTITE_POWER_POWERED) { ... }
 *
 *   agentite_power_destroy(ps);
 */

#ifndef AGENTITE_POWER_H
#define AGENTITE_POWER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_POWER_MAX_POLES      256
#define AGENTITE_POWER_MAX_NETWORKS   64
#define AGENTITE_POWER_MAX_NODES      512
#define AGENTITE_POWER_INVALID_ID     (-1)

/* Default connection multiplier (pole connects to other poles within radius * this) */
#define AGENTITE_POWER_CONNECTION_MULT  2.0f

/* Brownout threshold (percentage of demand met) */
#define AGENTITE_POWER_BROWNOUT_THRESHOLD  0.5f

/*============================================================================
 * Power Status
 *============================================================================*/

typedef enum Agentite_PowerStatus {
    AGENTITE_POWER_UNPOWERED = 0,   /* No power available */
    AGENTITE_POWER_BROWNOUT,        /* Partial power (below threshold) */
    AGENTITE_POWER_POWERED,         /* Full power available */
} Agentite_PowerStatus;

/*============================================================================
 * Node Types
 *============================================================================*/

typedef enum Agentite_PowerNodeType {
    AGENTITE_POWER_POLE = 0,        /* Power pole/pylon */
    AGENTITE_POWER_SUBSTATION,      /* Larger coverage area */
    AGENTITE_POWER_PRODUCER,        /* Generator */
    AGENTITE_POWER_CONSUMER,        /* Building using power */
} Agentite_PowerNodeType;

/*============================================================================
 * Structures
 *============================================================================*/

/**
 * Power pole/substation.
 */
typedef struct Agentite_PowerPole {
    int32_t id;
    int32_t x, y;                   /* Grid position */
    int32_t radius;                 /* Coverage radius */
    int32_t network_id;             /* Which network this belongs to */
    bool is_substation;             /* Substations have larger connection range */
} Agentite_PowerPole;

/**
 * Power producer (generator).
 */
typedef struct Agentite_PowerProducer {
    int32_t id;
    int32_t x, y;                   /* Grid position */
    int32_t production;             /* Power units produced */
    int32_t network_id;             /* Which network this belongs to */
    int32_t entity_id;              /* Link to game entity */
    bool active;                    /* Is generator running */
} Agentite_PowerProducer;

/**
 * Power consumer (building).
 */
typedef struct Agentite_PowerConsumer {
    int32_t id;
    int32_t x, y;                   /* Grid position */
    int32_t consumption;            /* Power units required */
    int32_t network_id;             /* Which network this belongs to */
    int32_t entity_id;              /* Link to game entity */
    bool active;                    /* Is consumer active */
    bool satisfied;                 /* Is power requirement met */
} Agentite_PowerConsumer;

/**
 * Power network statistics.
 */
typedef struct Agentite_NetworkStats {
    int32_t network_id;
    int32_t total_production;
    int32_t total_consumption;
    int32_t pole_count;
    int32_t producer_count;
    int32_t consumer_count;
    Agentite_PowerStatus status;
    float satisfaction_ratio;       /* production / consumption */
} Agentite_NetworkStats;

/**
 * Forward declaration.
 */
typedef struct Agentite_PowerSystem Agentite_PowerSystem;

/**
 * Network change callback.
 */
typedef void (*Agentite_PowerCallback)(
    Agentite_PowerSystem *ps,
    int network_id,
    Agentite_PowerStatus old_status,
    Agentite_PowerStatus new_status,
    void *userdata
);

/*============================================================================
 * Power System Creation
 *============================================================================*/

/**
 * Create a new power system.
 *
 * @param grid_width   Width of power grid
 * @param grid_height  Height of power grid
 * @return New power system or NULL on failure
 */
Agentite_PowerSystem *agentite_power_create(int grid_width, int grid_height);

/**
 * Destroy power system and free resources.
 *
 * @param ps Power system to destroy
 */
void agentite_power_destroy(Agentite_PowerSystem *ps);

/**
 * Reset power system.
 *
 * @param ps Power system
 */
void agentite_power_reset(Agentite_PowerSystem *ps);

/*============================================================================
 * Pole Management
 *============================================================================*/

/**
 * Add a power pole.
 *
 * @param ps     Power system
 * @param x      Grid X position
 * @param y      Grid Y position
 * @param radius Coverage radius
 * @return Pole ID or AGENTITE_POWER_INVALID_ID on failure
 */
int agentite_power_add_pole(Agentite_PowerSystem *ps, int x, int y, int radius);

/**
 * Add a substation (larger coverage and connection range).
 *
 * @param ps     Power system
 * @param x      Grid X position
 * @param y      Grid Y position
 * @param radius Coverage radius
 * @return Pole ID or AGENTITE_POWER_INVALID_ID on failure
 */
int agentite_power_add_substation(Agentite_PowerSystem *ps, int x, int y, int radius);

/**
 * Remove a power pole.
 * May split network if pole was bridge between sections.
 *
 * @param ps      Power system
 * @param pole_id Pole ID
 * @return true if removed
 */
bool agentite_power_remove_pole(Agentite_PowerSystem *ps, int pole_id);

/**
 * Get pole by ID.
 *
 * @param ps      Power system
 * @param pole_id Pole ID
 * @return Pole data or NULL
 */
const Agentite_PowerPole *agentite_power_get_pole(
    const Agentite_PowerSystem *ps,
    int pole_id
);

/**
 * Get pole at position.
 *
 * @param ps Power system
 * @param x  Grid X position
 * @param y  Grid Y position
 * @return Pole ID or AGENTITE_POWER_INVALID_ID
 */
int agentite_power_get_pole_at(const Agentite_PowerSystem *ps, int x, int y);

/**
 * Get all poles in a network.
 *
 * @param ps         Power system
 * @param network_id Network ID
 * @param out_ids    Output array for pole IDs
 * @param max_count  Maximum IDs to return
 * @return Number of poles
 */
int agentite_power_get_network_poles(
    const Agentite_PowerSystem *ps,
    int network_id,
    int *out_ids,
    int max_count
);

/*============================================================================
 * Producer Management
 *============================================================================*/

/**
 * Add a power producer (generator).
 *
 * @param ps         Power system
 * @param x          Grid X position
 * @param y          Grid Y position
 * @param production Power units produced
 * @return Producer ID or AGENTITE_POWER_INVALID_ID on failure
 */
int agentite_power_add_producer(
    Agentite_PowerSystem *ps,
    int x,
    int y,
    int production
);

/**
 * Remove a producer.
 *
 * @param ps          Power system
 * @param producer_id Producer ID
 * @return true if removed
 */
bool agentite_power_remove_producer(Agentite_PowerSystem *ps, int producer_id);

/**
 * Set producer active state.
 *
 * @param ps          Power system
 * @param producer_id Producer ID
 * @param active      Active state
 */
void agentite_power_set_producer_active(
    Agentite_PowerSystem *ps,
    int producer_id,
    bool active
);

/**
 * Update producer output.
 *
 * @param ps          Power system
 * @param producer_id Producer ID
 * @param production  New production value
 */
void agentite_power_set_production(
    Agentite_PowerSystem *ps,
    int producer_id,
    int production
);

/**
 * Get producer by ID.
 *
 * @param ps          Power system
 * @param producer_id Producer ID
 * @return Producer data or NULL
 */
const Agentite_PowerProducer *agentite_power_get_producer(
    const Agentite_PowerSystem *ps,
    int producer_id
);

/*============================================================================
 * Consumer Management
 *============================================================================*/

/**
 * Add a power consumer (building).
 *
 * @param ps          Power system
 * @param x           Grid X position
 * @param y           Grid Y position
 * @param consumption Power units required
 * @return Consumer ID or AGENTITE_POWER_INVALID_ID on failure
 */
int agentite_power_add_consumer(
    Agentite_PowerSystem *ps,
    int x,
    int y,
    int consumption
);

/**
 * Remove a consumer.
 *
 * @param ps          Power system
 * @param consumer_id Consumer ID
 * @return true if removed
 */
bool agentite_power_remove_consumer(Agentite_PowerSystem *ps, int consumer_id);

/**
 * Set consumer active state.
 *
 * @param ps          Power system
 * @param consumer_id Consumer ID
 * @param active      Active state
 */
void agentite_power_set_consumer_active(
    Agentite_PowerSystem *ps,
    int consumer_id,
    bool active
);

/**
 * Update consumer demand.
 *
 * @param ps          Power system
 * @param consumer_id Consumer ID
 * @param consumption New consumption value
 */
void agentite_power_set_consumption(
    Agentite_PowerSystem *ps,
    int consumer_id,
    int consumption
);

/**
 * Get consumer by ID.
 *
 * @param ps          Power system
 * @param consumer_id Consumer ID
 * @return Consumer data or NULL
 */
const Agentite_PowerConsumer *agentite_power_get_consumer(
    const Agentite_PowerSystem *ps,
    int consumer_id
);

/**
 * Check if a consumer is powered.
 *
 * @param ps          Power system
 * @param consumer_id Consumer ID
 * @return true if fully powered
 */
bool agentite_power_is_consumer_powered(
    const Agentite_PowerSystem *ps,
    int consumer_id
);

/*============================================================================
 * Network Queries
 *============================================================================*/

/**
 * Get network ID at a position.
 *
 * @param ps Power system
 * @param x  Grid X position
 * @param y  Grid Y position
 * @return Network ID or AGENTITE_POWER_INVALID_ID if not covered
 */
int agentite_power_get_network_at(const Agentite_PowerSystem *ps, int x, int y);

/**
 * Get power status at a position.
 *
 * @param ps Power system
 * @param x  Grid X position
 * @param y  Grid Y position
 * @return Power status
 */
Agentite_PowerStatus agentite_power_get_status_at(
    const Agentite_PowerSystem *ps,
    int x,
    int y
);

/**
 * Check if a position is covered by a power pole.
 *
 * @param ps Power system
 * @param x  Grid X position
 * @param y  Grid Y position
 * @return true if covered
 */
bool agentite_power_is_covered(const Agentite_PowerSystem *ps, int x, int y);

/**
 * Get network statistics.
 *
 * @param ps         Power system
 * @param network_id Network ID
 * @param out_stats  Output statistics
 * @return true if network exists
 */
bool agentite_power_get_network_stats(
    const Agentite_PowerSystem *ps,
    int network_id,
    Agentite_NetworkStats *out_stats
);

/**
 * Get all network IDs.
 *
 * @param ps        Power system
 * @param out_ids   Output array for network IDs
 * @param max_count Maximum IDs to return
 * @return Number of networks
 */
int agentite_power_get_networks(
    const Agentite_PowerSystem *ps,
    int *out_ids,
    int max_count
);

/**
 * Get total system production.
 *
 * @param ps Power system
 * @return Total production across all networks
 */
int agentite_power_get_total_production(const Agentite_PowerSystem *ps);

/**
 * Get total system consumption.
 *
 * @param ps Power system
 * @return Total consumption across all networks
 */
int agentite_power_get_total_consumption(const Agentite_PowerSystem *ps);

/*============================================================================
 * Coverage Queries (for rendering)
 *============================================================================*/

/**
 * Get all cells covered by a pole.
 *
 * @param ps        Power system
 * @param pole_id   Pole ID
 * @param out_cells Output array for (x,y) pairs
 * @param max_count Maximum cells to return
 * @return Number of covered cells
 */
int agentite_power_get_pole_coverage(
    const Agentite_PowerSystem *ps,
    int pole_id,
    int *out_cells,
    int max_count
);

/**
 * Get all cells covered by a network.
 *
 * @param ps         Power system
 * @param network_id Network ID
 * @param out_cells  Output array for (x,y) pairs
 * @param max_count  Maximum cells to return
 * @return Number of covered cells
 */
int agentite_power_get_network_coverage(
    const Agentite_PowerSystem *ps,
    int network_id,
    int *out_cells,
    int max_count
);

/**
 * Find nearest powered pole to a position.
 *
 * @param ps    Power system
 * @param x     Grid X position
 * @param y     Grid Y position
 * @param out_x Output X of nearest pole
 * @param out_y Output Y of nearest pole
 * @return Distance to nearest pole, or -1 if none found
 */
int agentite_power_find_nearest_pole(
    const Agentite_PowerSystem *ps,
    int x,
    int y,
    int *out_x,
    int *out_y
);

/*============================================================================
 * Network Updates
 *============================================================================*/

/**
 * Recalculate all network connections.
 * Call after batch changes.
 *
 * @param ps Power system
 */
void agentite_power_recalculate(Agentite_PowerSystem *ps);

/**
 * Set callback for network status changes.
 *
 * @param ps       Power system
 * @param callback Callback function
 * @param userdata User data
 */
void agentite_power_set_callback(
    Agentite_PowerSystem *ps,
    Agentite_PowerCallback callback,
    void *userdata
);

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * Set connection multiplier (how far poles can connect to each other).
 *
 * @param ps         Power system
 * @param multiplier Connection range = radius * multiplier
 */
void agentite_power_set_connection_multiplier(Agentite_PowerSystem *ps, float multiplier);

/**
 * Set brownout threshold.
 *
 * @param ps        Power system
 * @param threshold Ratio below which brownout occurs (0.0 to 1.0)
 */
void agentite_power_set_brownout_threshold(Agentite_PowerSystem *ps, float threshold);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Get power status name.
 *
 * @param status Power status
 * @return Static string name
 */
const char *agentite_power_status_name(Agentite_PowerStatus status);

/**
 * Get node type name.
 *
 * @param type Node type
 * @return Static string name
 */
const char *agentite_power_node_type_name(Agentite_PowerNodeType type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_POWER_H */
