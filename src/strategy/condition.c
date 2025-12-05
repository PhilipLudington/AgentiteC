#include "carbon/condition.h"
#include "carbon/validate.h"
#include <math.h>

/* Default max condition */
#define DEFAULT_MAX_CONDITION 100.0f

/* Decay multipliers lookup */
static const float decay_multipliers[] = {
    CARBON_DECAY_MULT_LOW,      /* CARBON_QUALITY_LOW */
    CARBON_DECAY_MULT_STANDARD, /* CARBON_QUALITY_STANDARD */
    CARBON_DECAY_MULT_HIGH      /* CARBON_QUALITY_HIGH */
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

void carbon_condition_init(Carbon_Condition *cond, Carbon_QualityTier quality) {
    carbon_condition_init_ex(cond, quality, DEFAULT_MAX_CONDITION);
}

void carbon_condition_init_ex(Carbon_Condition *cond, Carbon_QualityTier quality, float max_condition) {
    CARBON_VALIDATE_PTR(cond);

    cond->condition = max_condition;
    cond->max_condition = max_condition;
    cond->quality = quality;
    cond->is_damaged = false;
    cond->usage_count = 0;
    cond->repair_count = 0;
}

void carbon_condition_decay_time(Carbon_Condition *cond, float amount) {
    CARBON_VALIDATE_PTR(cond);

    float multiplier = carbon_condition_get_decay_multiplier(cond->quality);
    float decay = amount * multiplier;

    cond->condition -= decay;
    if (cond->condition < 0.0f) {
        cond->condition = 0.0f;
    }
}

void carbon_condition_decay_usage(Carbon_Condition *cond, float amount) {
    CARBON_VALIDATE_PTR(cond);

    cond->usage_count++;

    float multiplier = carbon_condition_get_decay_multiplier(cond->quality);
    float decay = amount * multiplier;

    cond->condition -= decay;
    if (cond->condition < 0.0f) {
        cond->condition = 0.0f;
    }
}

void carbon_condition_decay_raw(Carbon_Condition *cond, float amount) {
    CARBON_VALIDATE_PTR(cond);

    cond->condition -= amount;
    if (cond->condition < 0.0f) {
        cond->condition = 0.0f;
    }
}

void carbon_condition_repair(Carbon_Condition *cond, float amount) {
    CARBON_VALIDATE_PTR(cond);

    cond->condition += amount;
    if (cond->condition > cond->max_condition) {
        cond->condition = cond->max_condition;
    }

    /* Clear damaged flag on any repair */
    cond->is_damaged = false;
    cond->repair_count++;
}

void carbon_condition_repair_full(Carbon_Condition *cond) {
    CARBON_VALIDATE_PTR(cond);

    cond->condition = cond->max_condition;
    cond->is_damaged = false;
    cond->repair_count++;
}

void carbon_condition_damage(Carbon_Condition *cond) {
    CARBON_VALIDATE_PTR(cond);
    cond->is_damaged = true;
}

void carbon_condition_undamage(Carbon_Condition *cond) {
    CARBON_VALIDATE_PTR(cond);
    cond->is_damaged = false;
}

Carbon_ConditionStatus carbon_condition_get_status(const Carbon_Condition *cond) {
    if (!cond) return CARBON_CONDITION_CRITICAL;

    float percent = carbon_condition_get_percent(cond);

    if (percent >= CARBON_CONDITION_THRESHOLD_GOOD) {
        return CARBON_CONDITION_GOOD;
    } else if (percent >= CARBON_CONDITION_THRESHOLD_FAIR) {
        return CARBON_CONDITION_FAIR;
    } else if (percent >= CARBON_CONDITION_THRESHOLD_POOR) {
        return CARBON_CONDITION_POOR;
    } else {
        return CARBON_CONDITION_CRITICAL;
    }
}

float carbon_condition_get_percent(const Carbon_Condition *cond) {
    if (!cond || cond->max_condition <= 0.0f) return 0.0f;
    return (cond->condition / cond->max_condition) * 100.0f;
}

float carbon_condition_get_normalized(const Carbon_Condition *cond) {
    if (!cond || cond->max_condition <= 0.0f) return 0.0f;
    return cond->condition / cond->max_condition;
}

bool carbon_condition_is_usable(const Carbon_Condition *cond) {
    if (!cond) return false;
    return !cond->is_damaged && cond->condition > 0.0f;
}

float carbon_condition_get_failure_probability(const Carbon_Condition *cond, float base_rate) {
    if (!cond) return base_rate;

    /* Higher condition = lower failure chance */
    /* Formula: base_rate * (1.0 - normalized_condition)^2 */
    float normalized = carbon_condition_get_normalized(cond);
    float damage_factor = 1.0f - normalized;
    return base_rate * damage_factor * damage_factor;
}

float carbon_condition_get_efficiency(const Carbon_Condition *cond, float min_efficiency) {
    if (!cond) return min_efficiency;

    /* Linear interpolation from min_efficiency to 1.0 based on condition */
    float normalized = carbon_condition_get_normalized(cond);
    return min_efficiency + (1.0f - min_efficiency) * normalized;
}

int32_t carbon_condition_get_repair_cost(const Carbon_Condition *cond, int32_t base_cost) {
    if (!cond) return base_cost;

    /* Cost proportional to damage */
    float damage_percent = 1.0f - carbon_condition_get_normalized(cond);
    return (int32_t)(base_cost * damage_percent + 0.5f);  /* Round to nearest */
}

float carbon_condition_get_decay_multiplier(Carbon_QualityTier quality) {
    if (quality < 0 || quality > CARBON_QUALITY_HIGH) {
        return CARBON_DECAY_MULT_STANDARD;
    }
    return decay_multipliers[quality];
}

const char *carbon_condition_status_string(Carbon_ConditionStatus status) {
    if (status < 0 || status > CARBON_CONDITION_CRITICAL) {
        return "Unknown";
    }
    return status_strings[status];
}

const char *carbon_condition_quality_string(Carbon_QualityTier quality) {
    if (quality < 0 || quality > CARBON_QUALITY_HIGH) {
        return "Unknown";
    }
    return quality_strings[quality];
}
