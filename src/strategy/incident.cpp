#include "agentite/incident.h"
#include "agentite/condition.h"
#include <stdlib.h>
#include <time.h>

/* Simple LCG random number generator for reproducibility */
static uint32_t rng_state = 0;
static bool rng_initialized = false;

/* LCG constants (same as glibc) */
#define RNG_A 1103515245
#define RNG_C 12345
#define RNG_M 0x7FFFFFFF

/* Initialize RNG if needed */
static void ensure_rng_initialized(void) {
    if (!rng_initialized) {
        agentite_incident_seed(0);
    }
}

void agentite_incident_seed(uint32_t seed) {
    if (seed == 0) {
        rng_state = (uint32_t)time(NULL);
    } else {
        rng_state = seed;
    }
    rng_initialized = true;
}

float agentite_incident_random(void) {
    ensure_rng_initialized();
    rng_state = (RNG_A * rng_state + RNG_C) & RNG_M;
    return (float)rng_state / (float)RNG_M;
}

int agentite_incident_random_range(int min, int max) {
    if (min >= max) return min;
    float r = agentite_incident_random();
    return min + (int)(r * (max - min + 1));
}

float agentite_incident_calc_probability(float condition_percent, float quality_mult) {
    /* Clamp condition to valid range */
    if (condition_percent < 0.0f) condition_percent = 0.0f;
    if (condition_percent > 100.0f) condition_percent = 100.0f;

    /* Lower condition = higher failure probability */
    /* Formula: (1 - condition/100)^2 * quality_mult */
    float damage_factor = 1.0f - (condition_percent / 100.0f);
    float probability = damage_factor * damage_factor * quality_mult;

    /* Clamp to valid probability range */
    if (probability < 0.0f) probability = 0.0f;
    if (probability > 1.0f) probability = 1.0f;

    return probability;
}

float agentite_incident_calc_probability_from_condition(const Agentite_Condition *cond, float base_rate) {
    if (!cond) return base_rate;

    float condition_percent = agentite_condition_get_percent(cond);
    float quality_mult = agentite_condition_get_decay_multiplier(cond->quality);

    /* Scale by base_rate */
    return agentite_incident_calc_probability(condition_percent, quality_mult) * base_rate;
}

Agentite_IncidentType agentite_incident_check(float probability, const Agentite_IncidentConfig *config) {
    if (!config) {
        Agentite_IncidentConfig default_config = AGENTITE_INCIDENT_CONFIG_DEFAULT;
        config = &default_config;
    }

    /* First, check if an incident occurs at all */
    float roll = agentite_incident_random();
    if (roll >= probability) {
        return AGENTITE_INCIDENT_NONE;
    }

    /* Incident occurred, determine severity */
    return agentite_incident_roll_severity(config);
}

Agentite_IncidentType agentite_incident_check_condition(const Agentite_Condition *cond,
                                                     const Agentite_IncidentConfig *config) {
    if (!config) {
        Agentite_IncidentConfig default_config = AGENTITE_INCIDENT_CONFIG_DEFAULT;
        config = &default_config;
    }

    float probability = agentite_incident_calc_probability_from_condition(cond, config->base_probability);
    return agentite_incident_check(probability, config);
}

bool agentite_incident_roll(float probability) {
    if (probability <= 0.0f) return false;
    if (probability >= 1.0f) return true;

    return agentite_incident_random() < probability;
}

Agentite_IncidentType agentite_incident_roll_severity(const Agentite_IncidentConfig *config) {
    if (!config) {
        Agentite_IncidentConfig default_config = AGENTITE_INCIDENT_CONFIG_DEFAULT;
        config = &default_config;
    }

    float severity_roll = agentite_incident_random();

    if (severity_roll < config->minor_threshold) {
        return AGENTITE_INCIDENT_MINOR;
    } else if (severity_roll < config->major_threshold) {
        return AGENTITE_INCIDENT_MAJOR;
    } else {
        return AGENTITE_INCIDENT_CRITICAL;
    }
}

const char *agentite_incident_type_string(Agentite_IncidentType type) {
    switch (type) {
        case AGENTITE_INCIDENT_NONE:     return "None";
        case AGENTITE_INCIDENT_MINOR:    return "Minor";
        case AGENTITE_INCIDENT_MAJOR:    return "Major";
        case AGENTITE_INCIDENT_CRITICAL: return "Critical";
        default:                       return "Unknown";
    }
}
