#ifndef AGENTITE_CONDITION_H
#define AGENTITE_CONDITION_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Condition/Degradation System
 *
 * Track object condition with time-based and usage-based decay for
 * equipment, buildings, vehicles, and other degradable game objects.
 *
 * Usage:
 *   // Create condition tracker
 *   Agentite_Condition cond;
 *   agentite_condition_init(&cond, AGENTITE_QUALITY_STANDARD);
 *
 *   // Each game tick/turn:
 *   agentite_condition_decay_time(&cond, 0.1f);  // Time-based decay
 *
 *   // When object is used:
 *   agentite_condition_decay_usage(&cond, 1.0f);
 *
 *   // Check status
 *   Agentite_ConditionStatus status = agentite_condition_get_status(&cond);
 *   if (status == AGENTITE_CONDITION_CRITICAL) {
 *       // Needs repair!
 *   }
 *
 *   // Repair
 *   int32_t cost = agentite_condition_get_repair_cost(&cond, 100);
 *   agentite_condition_repair(&cond, 25.0f);  // Partial repair
 *   agentite_condition_repair_full(&cond);     // Full repair
 */

/**
 * Condition status thresholds
 */
typedef enum {
    AGENTITE_CONDITION_GOOD,      /**< >= 75% condition */
    AGENTITE_CONDITION_FAIR,      /**< >= 50% condition */
    AGENTITE_CONDITION_POOR,      /**< >= 25% condition */
    AGENTITE_CONDITION_CRITICAL   /**< < 25% condition */
} Agentite_ConditionStatus;

/**
 * Quality tiers affect decay rates
 */
typedef enum {
    AGENTITE_QUALITY_LOW,         /**< Decays 1.5x faster */
    AGENTITE_QUALITY_STANDARD,    /**< Normal decay rate */
    AGENTITE_QUALITY_HIGH         /**< Decays 0.5x slower */
} Agentite_QualityTier;

/** Decay rate multipliers by quality */
#define AGENTITE_DECAY_MULT_LOW      1.5f
#define AGENTITE_DECAY_MULT_STANDARD 1.0f
#define AGENTITE_DECAY_MULT_HIGH     0.5f

/** Status thresholds (percentage) */
#define AGENTITE_CONDITION_THRESHOLD_GOOD     75.0f
#define AGENTITE_CONDITION_THRESHOLD_FAIR     50.0f
#define AGENTITE_CONDITION_THRESHOLD_POOR     25.0f

/**
 * Condition tracking structure
 */
typedef struct {
    float condition;           /**< Current condition (0.0 - max_condition) */
    float max_condition;       /**< Maximum condition (usually 100.0) */
    Agentite_QualityTier quality; /**< Quality tier affecting decay rate */
    bool is_damaged;           /**< If true, requires repair before use */
    uint32_t usage_count;      /**< Total usage count (for statistics) */
    uint32_t repair_count;     /**< Number of times repaired */
} Agentite_Condition;

/**
 * Initialize a condition tracker.
 *
 * @param cond Condition structure to initialize
 * @param quality Quality tier (affects decay rate)
 */
void agentite_condition_init(Agentite_Condition *cond, Agentite_QualityTier quality);

/**
 * Initialize with custom max condition.
 *
 * @param cond Condition structure to initialize
 * @param quality Quality tier
 * @param max_condition Maximum condition value
 */
void agentite_condition_init_ex(Agentite_Condition *cond, Agentite_QualityTier quality, float max_condition);

/**
 * Apply time-based decay.
 * Amount is multiplied by quality decay multiplier.
 *
 * @param cond Condition tracker
 * @param amount Base decay amount (will be modified by quality)
 */
void agentite_condition_decay_time(Agentite_Condition *cond, float amount);

/**
 * Apply usage-based decay.
 * Call when the object is used. Increments usage counter.
 *
 * @param cond Condition tracker
 * @param amount Decay per use (will be modified by quality)
 */
void agentite_condition_decay_usage(Agentite_Condition *cond, float amount);

/**
 * Apply raw decay without quality modifier.
 *
 * @param cond Condition tracker
 * @param amount Exact decay amount
 */
void agentite_condition_decay_raw(Agentite_Condition *cond, float amount);

/**
 * Repair condition by a specified amount.
 *
 * @param cond Condition tracker
 * @param amount Amount to repair (capped at max)
 */
void agentite_condition_repair(Agentite_Condition *cond, float amount);

/**
 * Fully repair to max condition.
 *
 * @param cond Condition tracker
 */
void agentite_condition_repair_full(Agentite_Condition *cond);

/**
 * Mark object as damaged (requires repair before use).
 *
 * @param cond Condition tracker
 */
void agentite_condition_damage(Agentite_Condition *cond);

/**
 * Clear damaged flag (called automatically by repair functions).
 *
 * @param cond Condition tracker
 */
void agentite_condition_undamage(Agentite_Condition *cond);

/**
 * Get the current condition status.
 *
 * @param cond Condition tracker
 * @return Status (GOOD/FAIR/POOR/CRITICAL)
 */
Agentite_ConditionStatus agentite_condition_get_status(const Agentite_Condition *cond);

/**
 * Get condition as a percentage (0.0 - 100.0).
 *
 * @param cond Condition tracker
 * @return Condition percentage
 */
float agentite_condition_get_percent(const Agentite_Condition *cond);

/**
 * Get condition as a normalized value (0.0 - 1.0).
 *
 * @param cond Condition tracker
 * @return Normalized condition
 */
float agentite_condition_get_normalized(const Agentite_Condition *cond);

/**
 * Check if object is usable (not damaged and condition > 0).
 *
 * @param cond Condition tracker
 * @return true if usable
 */
bool agentite_condition_is_usable(const Agentite_Condition *cond);

/**
 * Calculate probability of failure based on condition.
 * Higher condition = lower failure chance.
 *
 * Formula: base_rate * (1.0 - condition/max)^2
 *
 * @param cond Condition tracker
 * @param base_rate Base failure rate at 0% condition
 * @return Failure probability (0.0 - base_rate)
 */
float agentite_condition_get_failure_probability(const Agentite_Condition *cond, float base_rate);

/**
 * Calculate efficiency modifier based on condition.
 * Returns a multiplier that decreases as condition worsens.
 *
 * Formula: min_efficiency + (1.0 - min_efficiency) * (condition/max)
 *
 * @param cond Condition tracker
 * @param min_efficiency Minimum efficiency at 0% condition (e.g., 0.5 for 50%)
 * @return Efficiency multiplier (min_efficiency to 1.0)
 */
float agentite_condition_get_efficiency(const Agentite_Condition *cond, float min_efficiency);

/**
 * Calculate repair cost based on damage amount.
 *
 * @param cond Condition tracker
 * @param base_cost Cost to fully repair from 0%
 * @return Proportional repair cost
 */
int32_t agentite_condition_get_repair_cost(const Agentite_Condition *cond, int32_t base_cost);

/**
 * Get the decay rate multiplier for a quality tier.
 *
 * @param quality Quality tier
 * @return Decay multiplier
 */
float agentite_condition_get_decay_multiplier(Agentite_QualityTier quality);

/**
 * Get a human-readable string for condition status.
 *
 * @param status Condition status
 * @return Status string ("Good", "Fair", "Poor", "Critical")
 */
const char *agentite_condition_status_string(Agentite_ConditionStatus status);

/**
 * Get a human-readable string for quality tier.
 *
 * @param quality Quality tier
 * @return Quality string ("Low", "Standard", "High")
 */
const char *agentite_condition_quality_string(Agentite_QualityTier quality);

#endif /* AGENTITE_CONDITION_H */
