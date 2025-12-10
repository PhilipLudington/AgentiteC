#ifndef AGENTITE_DEMAND_H
#define AGENTITE_DEMAND_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Dynamic Demand System
 *
 * Demand values that respond to service levels for economy/logistics games.
 * Demand increases with service and decays over time without service.
 *
 * Usage:
 *   Agentite_Demand demand;
 *   agentite_demand_init(&demand, 50, 50);  // Initial 50, equilibrium 50
 *
 *   // When player delivers goods/services:
 *   agentite_demand_record_service(&demand);
 *
 *   // Each update interval (e.g., each turn or time period):
 *   agentite_demand_update(&demand, delta_time);
 *
 *   // Query demand
 *   uint8_t level = agentite_demand_get(&demand);
 *   float mult = agentite_demand_get_multiplier(&demand);  // 0.5 - 2.0
 *   int32_t payment = base_price * mult;
 */

/** Demand value range */
#define AGENTITE_DEMAND_MIN 0
#define AGENTITE_DEMAND_MAX 100

/** Default values */
#define AGENTITE_DEMAND_DEFAULT_UPDATE_INTERVAL 1.0f
#define AGENTITE_DEMAND_DEFAULT_GROWTH_PER_SERVICE 5.0f
#define AGENTITE_DEMAND_DEFAULT_DECAY_RATE 2.0f

/**
 * Demand tracking structure
 */
typedef struct {
    uint8_t demand;            /**< Current demand (0-100) */
    uint8_t equilibrium;       /**< Natural resting point */
    uint8_t min_demand;        /**< Floor value */
    uint8_t max_demand;        /**< Ceiling value */

    float update_interval;     /**< Seconds between decay updates */
    float time_since_update;   /**< Time accumulator */
    uint32_t service_count;    /**< Services since last update */
    uint32_t total_services;   /**< Lifetime service count */

    float growth_per_service;  /**< Demand increase per service */
    float decay_rate;          /**< Demand decrease per update without service */
} Agentite_Demand;

/**
 * Initialize a demand tracker with default parameters.
 *
 * @param demand Demand structure to initialize
 * @param initial Initial demand value (0-100)
 * @param equilibrium Natural resting point (0-100)
 */
void agentite_demand_init(Agentite_Demand *demand, uint8_t initial, uint8_t equilibrium);

/**
 * Initialize a demand tracker with custom parameters.
 *
 * @param demand Demand structure to initialize
 * @param initial Initial demand value
 * @param equilibrium Natural resting point
 * @param min_demand Minimum demand floor
 * @param max_demand Maximum demand ceiling
 * @param growth_per_service Demand increase per service
 * @param decay_rate Demand decrease per update without service
 * @param update_interval Seconds between updates
 */
void agentite_demand_init_ex(Agentite_Demand *demand,
                           uint8_t initial,
                           uint8_t equilibrium,
                           uint8_t min_demand,
                           uint8_t max_demand,
                           float growth_per_service,
                           float decay_rate,
                           float update_interval);

/**
 * Record a service (delivery, visit, etc.).
 * Increases demand based on growth_per_service.
 *
 * @param demand Demand tracker
 */
void agentite_demand_record_service(Agentite_Demand *demand);

/**
 * Record multiple services at once.
 *
 * @param demand Demand tracker
 * @param count Number of services
 */
void agentite_demand_record_services(Agentite_Demand *demand, uint32_t count);

/**
 * Update demand over time (call each frame).
 * Handles decay toward equilibrium.
 *
 * @param demand Demand tracker
 * @param dt Delta time in seconds
 */
void agentite_demand_update(Agentite_Demand *demand, float dt);

/**
 * Force an update tick (for turn-based games).
 *
 * @param demand Demand tracker
 */
void agentite_demand_tick(Agentite_Demand *demand);

/**
 * Get current demand value (0-100).
 *
 * @param demand Demand tracker
 * @return Current demand
 */
uint8_t agentite_demand_get(const Agentite_Demand *demand);

/**
 * Get demand as a normalized value (0.0 - 1.0).
 *
 * @param demand Demand tracker
 * @return Normalized demand
 */
float agentite_demand_get_normalized(const Agentite_Demand *demand);

/**
 * Get demand as a price multiplier.
 * Returns a value from 0.5 (at demand 0) to 2.0 (at demand 100).
 * At demand 50, returns 1.0 (baseline).
 *
 * @param demand Demand tracker
 * @return Price multiplier (0.5 - 2.0)
 */
float agentite_demand_get_multiplier(const Agentite_Demand *demand);

/**
 * Get demand as a custom range multiplier.
 *
 * @param demand Demand tracker
 * @param min_mult Multiplier at demand 0
 * @param max_mult Multiplier at demand 100
 * @return Interpolated multiplier
 */
float agentite_demand_get_multiplier_range(const Agentite_Demand *demand, float min_mult, float max_mult);

/**
 * Set demand directly (bypassing normal rules).
 *
 * @param demand Demand tracker
 * @param value New demand value (clamped to min/max)
 */
void agentite_demand_set(Agentite_Demand *demand, uint8_t value);

/**
 * Adjust demand by a delta amount.
 *
 * @param demand Demand tracker
 * @param delta Amount to add (can be negative)
 */
void agentite_demand_adjust(Agentite_Demand *demand, int delta);

/**
 * Reset demand to equilibrium.
 *
 * @param demand Demand tracker
 */
void agentite_demand_reset(Agentite_Demand *demand);

/**
 * Get the equilibrium point.
 *
 * @param demand Demand tracker
 * @return Equilibrium value
 */
uint8_t agentite_demand_get_equilibrium(const Agentite_Demand *demand);

/**
 * Set a new equilibrium point.
 *
 * @param demand Demand tracker
 * @param equilibrium New equilibrium (clamped to min/max)
 */
void agentite_demand_set_equilibrium(Agentite_Demand *demand, uint8_t equilibrium);

/**
 * Get total lifetime services recorded.
 *
 * @param demand Demand tracker
 * @return Total services
 */
uint32_t agentite_demand_get_total_services(const Agentite_Demand *demand);

/**
 * Check if demand is at maximum.
 *
 * @param demand Demand tracker
 * @return true if at max demand
 */
bool agentite_demand_is_at_max(const Agentite_Demand *demand);

/**
 * Check if demand is at minimum.
 *
 * @param demand Demand tracker
 * @return true if at min demand
 */
bool agentite_demand_is_at_min(const Agentite_Demand *demand);

/**
 * Get a descriptive string for current demand level.
 *
 * @param demand Demand tracker
 * @return "Very Low", "Low", "Medium", "High", or "Very High"
 */
const char *agentite_demand_get_level_string(const Agentite_Demand *demand);

#endif /* AGENTITE_DEMAND_H */
