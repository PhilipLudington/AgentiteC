#ifndef CARBON_CONDITION_H
#define CARBON_CONDITION_H

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
 *   Carbon_Condition cond;
 *   carbon_condition_init(&cond, CARBON_QUALITY_STANDARD);
 *
 *   // Each game tick/turn:
 *   carbon_condition_decay_time(&cond, 0.1f);  // Time-based decay
 *
 *   // When object is used:
 *   carbon_condition_decay_usage(&cond, 1.0f);
 *
 *   // Check status
 *   Carbon_ConditionStatus status = carbon_condition_get_status(&cond);
 *   if (status == CARBON_CONDITION_CRITICAL) {
 *       // Needs repair!
 *   }
 *
 *   // Repair
 *   int32_t cost = carbon_condition_get_repair_cost(&cond, 100);
 *   carbon_condition_repair(&cond, 25.0f);  // Partial repair
 *   carbon_condition_repair_full(&cond);     // Full repair
 */

/**
 * Condition status thresholds
 */
typedef enum {
    CARBON_CONDITION_GOOD,      /**< >= 75% condition */
    CARBON_CONDITION_FAIR,      /**< >= 50% condition */
    CARBON_CONDITION_POOR,      /**< >= 25% condition */
    CARBON_CONDITION_CRITICAL   /**< < 25% condition */
} Carbon_ConditionStatus;

/**
 * Quality tiers affect decay rates
 */
typedef enum {
    CARBON_QUALITY_LOW,         /**< Decays 1.5x faster */
    CARBON_QUALITY_STANDARD,    /**< Normal decay rate */
    CARBON_QUALITY_HIGH         /**< Decays 0.5x slower */
} Carbon_QualityTier;

/** Decay rate multipliers by quality */
#define CARBON_DECAY_MULT_LOW      1.5f
#define CARBON_DECAY_MULT_STANDARD 1.0f
#define CARBON_DECAY_MULT_HIGH     0.5f

/** Status thresholds (percentage) */
#define CARBON_CONDITION_THRESHOLD_GOOD     75.0f
#define CARBON_CONDITION_THRESHOLD_FAIR     50.0f
#define CARBON_CONDITION_THRESHOLD_POOR     25.0f

/**
 * Condition tracking structure
 */
typedef struct {
    float condition;           /**< Current condition (0.0 - max_condition) */
    float max_condition;       /**< Maximum condition (usually 100.0) */
    Carbon_QualityTier quality; /**< Quality tier affecting decay rate */
    bool is_damaged;           /**< If true, requires repair before use */
    uint32_t usage_count;      /**< Total usage count (for statistics) */
    uint32_t repair_count;     /**< Number of times repaired */
} Carbon_Condition;

/**
 * Initialize a condition tracker.
 *
 * @param cond Condition structure to initialize
 * @param quality Quality tier (affects decay rate)
 */
void carbon_condition_init(Carbon_Condition *cond, Carbon_QualityTier quality);

/**
 * Initialize with custom max condition.
 *
 * @param cond Condition structure to initialize
 * @param quality Quality tier
 * @param max_condition Maximum condition value
 */
void carbon_condition_init_ex(Carbon_Condition *cond, Carbon_QualityTier quality, float max_condition);

/**
 * Apply time-based decay.
 * Amount is multiplied by quality decay multiplier.
 *
 * @param cond Condition tracker
 * @param amount Base decay amount (will be modified by quality)
 */
void carbon_condition_decay_time(Carbon_Condition *cond, float amount);

/**
 * Apply usage-based decay.
 * Call when the object is used. Increments usage counter.
 *
 * @param cond Condition tracker
 * @param amount Decay per use (will be modified by quality)
 */
void carbon_condition_decay_usage(Carbon_Condition *cond, float amount);

/**
 * Apply raw decay without quality modifier.
 *
 * @param cond Condition tracker
 * @param amount Exact decay amount
 */
void carbon_condition_decay_raw(Carbon_Condition *cond, float amount);

/**
 * Repair condition by a specified amount.
 *
 * @param cond Condition tracker
 * @param amount Amount to repair (capped at max)
 */
void carbon_condition_repair(Carbon_Condition *cond, float amount);

/**
 * Fully repair to max condition.
 *
 * @param cond Condition tracker
 */
void carbon_condition_repair_full(Carbon_Condition *cond);

/**
 * Mark object as damaged (requires repair before use).
 *
 * @param cond Condition tracker
 */
void carbon_condition_damage(Carbon_Condition *cond);

/**
 * Clear damaged flag (called automatically by repair functions).
 *
 * @param cond Condition tracker
 */
void carbon_condition_undamage(Carbon_Condition *cond);

/**
 * Get the current condition status.
 *
 * @param cond Condition tracker
 * @return Status (GOOD/FAIR/POOR/CRITICAL)
 */
Carbon_ConditionStatus carbon_condition_get_status(const Carbon_Condition *cond);

/**
 * Get condition as a percentage (0.0 - 100.0).
 *
 * @param cond Condition tracker
 * @return Condition percentage
 */
float carbon_condition_get_percent(const Carbon_Condition *cond);

/**
 * Get condition as a normalized value (0.0 - 1.0).
 *
 * @param cond Condition tracker
 * @return Normalized condition
 */
float carbon_condition_get_normalized(const Carbon_Condition *cond);

/**
 * Check if object is usable (not damaged and condition > 0).
 *
 * @param cond Condition tracker
 * @return true if usable
 */
bool carbon_condition_is_usable(const Carbon_Condition *cond);

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
float carbon_condition_get_failure_probability(const Carbon_Condition *cond, float base_rate);

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
float carbon_condition_get_efficiency(const Carbon_Condition *cond, float min_efficiency);

/**
 * Calculate repair cost based on damage amount.
 *
 * @param cond Condition tracker
 * @param base_cost Cost to fully repair from 0%
 * @return Proportional repair cost
 */
int32_t carbon_condition_get_repair_cost(const Carbon_Condition *cond, int32_t base_cost);

/**
 * Get the decay rate multiplier for a quality tier.
 *
 * @param quality Quality tier
 * @return Decay multiplier
 */
float carbon_condition_get_decay_multiplier(Carbon_QualityTier quality);

/**
 * Get a human-readable string for condition status.
 *
 * @param status Condition status
 * @return Status string ("Good", "Fair", "Poor", "Critical")
 */
const char *carbon_condition_status_string(Carbon_ConditionStatus status);

/**
 * Get a human-readable string for quality tier.
 *
 * @param quality Quality tier
 * @return Quality string ("Low", "Standard", "High")
 */
const char *carbon_condition_quality_string(Carbon_QualityTier quality);

#endif /* CARBON_CONDITION_H */
