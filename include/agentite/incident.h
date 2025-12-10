#ifndef AGENTITE_INCIDENT_H
#define AGENTITE_INCIDENT_H

#include <stdbool.h>
#include <stdint.h>
#include "agentite/condition.h"

/**
 * Carbon Incident/Random Event System
 *
 * Probabilistic event system for random failures and events.
 * Useful for equipment breakdowns, random events, and risk management.
 *
 * Usage:
 *   // Configure incident thresholds
 *   Agentite_IncidentConfig config = {
 *       .base_probability = 0.1f,    // 10% base chance
 *       .minor_threshold = 0.70f,    // 70% of incidents are minor
 *       .major_threshold = 0.90f     // 20% major, 10% critical
 *   };
 *
 *   // Roll for incident (based on object condition)
 *   float prob = agentite_incident_calc_probability(condition_percent, quality_mult);
 *   Agentite_IncidentType result = agentite_incident_check(prob, &config);
 *
 *   if (result != AGENTITE_INCIDENT_NONE) {
 *       switch (result) {
 *           case AGENTITE_INCIDENT_MINOR:
 *               // Handle minor incident
 *               break;
 *           case AGENTITE_INCIDENT_MAJOR:
 *               // Handle major incident
 *               break;
 *           case AGENTITE_INCIDENT_CRITICAL:
 *               // Handle critical incident
 *               break;
 *       }
 *   }
 */

/**
 * Incident severity types (can be ORed for flags)
 */
typedef enum {
    AGENTITE_INCIDENT_NONE = 0,      /**< No incident occurred */
    AGENTITE_INCIDENT_MINOR = 1,     /**< Minor incident - temporary effect */
    AGENTITE_INCIDENT_MAJOR = 2,     /**< Major incident - lasting effect */
    AGENTITE_INCIDENT_CRITICAL = 4   /**< Critical incident - severe consequence */
} Agentite_IncidentType;

/**
 * Incident configuration for probability distribution
 */
typedef struct {
    float base_probability;    /**< Base chance of incident (0.0 - 1.0) */
    float minor_threshold;     /**< Roll below this = minor (e.g., 0.70 = 70%) */
    float major_threshold;     /**< Roll below this = major (e.g., 0.90 = 90%) */
    /* Roll at or above major_threshold = critical */
} Agentite_IncidentConfig;

/**
 * Default incident configuration
 */
#define AGENTITE_INCIDENT_CONFIG_DEFAULT { \
    .base_probability = 0.1f, \
    .minor_threshold = 0.70f, \
    .major_threshold = 0.90f \
}

/**
 * Calculate incident probability based on condition.
 * Lower condition = higher probability.
 *
 * Formula: base_rate * (1.0 - condition/100)^2 * quality_mult
 *
 * @param condition_percent Condition percentage (0-100)
 * @param quality_mult Quality multiplier (e.g., 1.5 for low quality, 0.5 for high)
 * @return Incident probability (0.0 - 1.0)
 */
float agentite_incident_calc_probability(float condition_percent, float quality_mult);

/**
 * Calculate incident probability using a Condition structure.
 *
 * @param cond Condition tracker (from condition.h)
 * @param base_rate Base failure rate at 0% condition
 * @return Incident probability
 */
float agentite_incident_calc_probability_from_condition(const Agentite_Condition *cond, float base_rate);

/**
 * Check if an incident occurs and determine its severity.
 *
 * @param probability Probability of an incident occurring (0.0 - 1.0)
 * @param config Incident configuration (severity distribution)
 * @return Incident type (NONE, MINOR, MAJOR, or CRITICAL)
 */
Agentite_IncidentType agentite_incident_check(float probability, const Agentite_IncidentConfig *config);

/**
 * Check for incident using a Condition structure.
 * Convenience function that calculates probability and checks in one call.
 *
 * @param cond Condition tracker
 * @param config Incident configuration
 * @return Incident type
 */
Agentite_IncidentType agentite_incident_check_condition(const Agentite_Condition *cond,
                                                     const Agentite_IncidentConfig *config);

/**
 * Roll a random value and check if it's below a threshold.
 * Simple yes/no probability check.
 *
 * @param probability Threshold probability (0.0 - 1.0)
 * @return true if roll is below probability (event occurs)
 */
bool agentite_incident_roll(float probability);

/**
 * Roll for severity given an incident has occurred.
 * Does not check if incident occurs, only determines severity.
 *
 * @param config Incident configuration
 * @return MINOR, MAJOR, or CRITICAL
 */
Agentite_IncidentType agentite_incident_roll_severity(const Agentite_IncidentConfig *config);

/**
 * Get a descriptive string for incident type.
 *
 * @param type Incident type
 * @return "None", "Minor", "Major", or "Critical"
 */
const char *agentite_incident_type_string(Agentite_IncidentType type);

/**
 * Seed the incident random number generator.
 * If seed is 0, uses time-based seeding.
 *
 * @param seed Random seed (0 for time-based)
 */
void agentite_incident_seed(uint32_t seed);

/**
 * Get a random float between 0.0 and 1.0.
 * Uses the incident system's RNG for reproducibility.
 *
 * @return Random value [0.0, 1.0)
 */
float agentite_incident_random(void);

/**
 * Get a random integer in range [min, max] inclusive.
 *
 * @param min Minimum value
 * @param max Maximum value
 * @return Random integer in range
 */
int agentite_incident_random_range(int min, int max);

#endif /* AGENTITE_INCIDENT_H */
