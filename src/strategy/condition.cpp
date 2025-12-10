#include "agentite/condition.h"
#include "agentite/validate.h"
#include <math.h>

/* Default max condition */
#define DEFAULT_MAX_CONDITION 100.0f

/* Decay multipliers lookup */
static const float decay_multipliers[] = {
    AGENTITE_DECAY_MULT_LOW,      /* AGENTITE_QUALITY_LOW */
    AGENTITE_DECAY_MULT_STANDARD, /* AGENTITE_QUALITY_STANDARD */
    AGENTITE_DECAY_MULT_HIGH      /* AGENTITE_QUALITY_HIGH */
};

/* Status strings */
static const char *status_strings[] = {
    "Good",
    "Fair",
    "Poor",
    "Critical"
};

/* Quality strings */
static const char *quality_strings[] = {
    "Low",
    "Standard",
    "High"
};

void agentite_condition_init(Agentite_Condition *cond, Agentite_QualityTier quality) {
    agentite_condition_init_ex(cond, quality, DEFAULT_MAX_CONDITION);
}

void agentite_condition_init_ex(Agentite_Condition *cond, Agentite_QualityTier quality, float max_condition) {
    AGENTITE_VALIDATE_PTR(cond);

    cond->condition = max_condition;
    cond->max_condition = max_condition;
    cond->quality = quality;
    cond->is_damaged = false;
    cond->usage_count = 0;
    cond->repair_count = 0;
}

void agentite_condition_decay_time(Agentite_Condition *cond, float amount) {
    AGENTITE_VALIDATE_PTR(cond);

    float multiplier = agentite_condition_get_decay_multiplier(cond->quality);
    float decay = amount * multiplier;

    cond->condition -= decay;
    if (cond->condition < 0.0f) {
        cond->condition = 0.0f;
    }
}

void agentite_condition_decay_usage(Agentite_Condition *cond, float amount) {
    AGENTITE_VALIDATE_PTR(cond);

    cond->usage_count++;

    float multiplier = agentite_condition_get_decay_multiplier(cond->quality);
    float decay = amount * multiplier;

    cond->condition -= decay;
    if (cond->condition < 0.0f) {
        cond->condition = 0.0f;
    }
}

void agentite_condition_decay_raw(Agentite_Condition *cond, float amount) {
    AGENTITE_VALIDATE_PTR(cond);

    cond->condition -= amount;
    if (cond->condition < 0.0f) {
        cond->condition = 0.0f;
    }
}

void agentite_condition_repair(Agentite_Condition *cond, float amount) {
    AGENTITE_VALIDATE_PTR(cond);

    cond->condition += amount;
    if (cond->condition > cond->max_condition) {
        cond->condition = cond->max_condition;
    }

    /* Clear damaged flag on any repair */
    cond->is_damaged = false;
    cond->repair_count++;
}

void agentite_condition_repair_full(Agentite_Condition *cond) {
    AGENTITE_VALIDATE_PTR(cond);

    cond->condition = cond->max_condition;
    cond->is_damaged = false;
    cond->repair_count++;
}

void agentite_condition_damage(Agentite_Condition *cond) {
    AGENTITE_VALIDATE_PTR(cond);
    cond->is_damaged = true;
}

void agentite_condition_undamage(Agentite_Condition *cond) {
    AGENTITE_VALIDATE_PTR(cond);
    cond->is_damaged = false;
}

Agentite_ConditionStatus agentite_condition_get_status(const Agentite_Condition *cond) {
    if (!cond) return AGENTITE_CONDITION_CRITICAL;

    float percent = agentite_condition_get_percent(cond);

    if (percent >= AGENTITE_CONDITION_THRESHOLD_GOOD) {
        return AGENTITE_CONDITION_GOOD;
    } else if (percent >= AGENTITE_CONDITION_THRESHOLD_FAIR) {
        return AGENTITE_CONDITION_FAIR;
    } else if (percent >= AGENTITE_CONDITION_THRESHOLD_POOR) {
        return AGENTITE_CONDITION_POOR;
    } else {
        return AGENTITE_CONDITION_CRITICAL;
    }
}

float agentite_condition_get_percent(const Agentite_Condition *cond) {
    if (!cond || cond->max_condition <= 0.0f) return 0.0f;
    return (cond->condition / cond->max_condition) * 100.0f;
}

float agentite_condition_get_normalized(const Agentite_Condition *cond) {
    if (!cond || cond->max_condition <= 0.0f) return 0.0f;
    return cond->condition / cond->max_condition;
}

bool agentite_condition_is_usable(const Agentite_Condition *cond) {
    if (!cond) return false;
    return !cond->is_damaged && cond->condition > 0.0f;
}

float agentite_condition_get_failure_probability(const Agentite_Condition *cond, float base_rate) {
    if (!cond) return base_rate;

    /* Higher condition = lower failure chance */
    /* Formula: base_rate * (1.0 - normalized_condition)^2 */
    float normalized = agentite_condition_get_normalized(cond);
    float damage_factor = 1.0f - normalized;
    return base_rate * damage_factor * damage_factor;
}

float agentite_condition_get_efficiency(const Agentite_Condition *cond, float min_efficiency) {
    if (!cond) return min_efficiency;

    /* Linear interpolation from min_efficiency to 1.0 based on condition */
    float normalized = agentite_condition_get_normalized(cond);
    return min_efficiency + (1.0f - min_efficiency) * normalized;
}

int32_t agentite_condition_get_repair_cost(const Agentite_Condition *cond, int32_t base_cost) {
    if (!cond) return base_cost;

    /* Cost proportional to damage */
    float damage_percent = 1.0f - agentite_condition_get_normalized(cond);
    return (int32_t)(base_cost * damage_percent + 0.5f);  /* Round to nearest */
}

float agentite_condition_get_decay_multiplier(Agentite_QualityTier quality) {
    if (quality < 0 || quality > AGENTITE_QUALITY_HIGH) {
        return AGENTITE_DECAY_MULT_STANDARD;
    }
    return decay_multipliers[quality];
}

const char *agentite_condition_status_string(Agentite_ConditionStatus status) {
    if (status < 0 || status > AGENTITE_CONDITION_CRITICAL) {
        return "Unknown";
    }
    return status_strings[status];
}

const char *agentite_condition_quality_string(Agentite_QualityTier quality) {
    if (quality < 0 || quality > AGENTITE_QUALITY_HIGH) {
        return "Unknown";
    }
    return quality_strings[quality];
}
