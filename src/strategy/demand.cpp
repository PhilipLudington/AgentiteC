#include "carbon/demand.h"
#include "carbon/validate.h"

/* Clamp value to uint8_t range with min/max */
static inline uint8_t clamp_demand(int value, uint8_t min_val, uint8_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return (uint8_t)value;
}

void carbon_demand_init(Carbon_Demand *demand, uint8_t initial, uint8_t equilibrium) {
    carbon_demand_init_ex(demand, initial, equilibrium,
                          CARBON_DEMAND_MIN, CARBON_DEMAND_MAX,
                          CARBON_DEMAND_DEFAULT_GROWTH_PER_SERVICE,
                          CARBON_DEMAND_DEFAULT_DECAY_RATE,
                          CARBON_DEMAND_DEFAULT_UPDATE_INTERVAL);
}

void carbon_demand_init_ex(Carbon_Demand *demand,
                           uint8_t initial,
                           uint8_t equilibrium,
                           uint8_t min_demand,
                           uint8_t max_demand,
                           float growth_per_service,
                           float decay_rate,
                           float update_interval) {
    CARBON_VALIDATE_PTR(demand);

    /* Ensure min <= max */
    if (min_demand > max_demand) {
        uint8_t temp = min_demand;
        min_demand = max_demand;
        max_demand = temp;
    }

    demand->min_demand = min_demand;
    demand->max_demand = max_demand;
    demand->demand = clamp_demand(initial, min_demand, max_demand);
    demand->equilibrium = clamp_demand(equilibrium, min_demand, max_demand);

    demand->update_interval = update_interval > 0.0f ? update_interval : CARBON_DEMAND_DEFAULT_UPDATE_INTERVAL;
    demand->time_since_update = 0.0f;
    demand->service_count = 0;
    demand->total_services = 0;

    demand->growth_per_service = growth_per_service > 0.0f ? growth_per_service : CARBON_DEMAND_DEFAULT_GROWTH_PER_SERVICE;
    demand->decay_rate = decay_rate > 0.0f ? decay_rate : CARBON_DEMAND_DEFAULT_DECAY_RATE;
}

void carbon_demand_record_service(Carbon_Demand *demand) {
    CARBON_VALIDATE_PTR(demand);

    demand->service_count++;
    demand->total_services++;

    /* Immediate growth from service */
    float new_demand = (float)demand->demand + demand->growth_per_service;
    demand->demand = clamp_demand((int)new_demand, demand->min_demand, demand->max_demand);
}

void carbon_demand_record_services(Carbon_Demand *demand, uint32_t count) {
    CARBON_VALIDATE_PTR(demand);
    if (count == 0) return;

    demand->service_count += count;
    demand->total_services += count;

    float new_demand = (float)demand->demand + (demand->growth_per_service * count);
    demand->demand = clamp_demand((int)new_demand, demand->min_demand, demand->max_demand);
}

void carbon_demand_update(Carbon_Demand *demand, float dt) {
    CARBON_VALIDATE_PTR(demand);

    demand->time_since_update += dt;

    while (demand->time_since_update >= demand->update_interval) {
        demand->time_since_update -= demand->update_interval;
        carbon_demand_tick(demand);
    }
}

void carbon_demand_tick(Carbon_Demand *demand) {
    CARBON_VALIDATE_PTR(demand);

    /* If no services this tick, decay toward equilibrium */
    if (demand->service_count == 0) {
        if (demand->demand > demand->equilibrium) {
            float new_demand = (float)demand->demand - demand->decay_rate;
            if (new_demand < demand->equilibrium) {
                new_demand = demand->equilibrium;
            }
            demand->demand = clamp_demand((int)new_demand, demand->min_demand, demand->max_demand);
        } else if (demand->demand < demand->equilibrium) {
            /* Slowly rise toward equilibrium if below */
            float new_demand = (float)demand->demand + (demand->decay_rate * 0.5f);
            if (new_demand > demand->equilibrium) {
                new_demand = demand->equilibrium;
            }
            demand->demand = clamp_demand((int)new_demand, demand->min_demand, demand->max_demand);
        }
    }

    /* Reset service counter for next tick */
    demand->service_count = 0;
}

uint8_t carbon_demand_get(const Carbon_Demand *demand) {
    return demand ? demand->demand : 0;
}

float carbon_demand_get_normalized(const Carbon_Demand *demand) {
    if (!demand) return 0.0f;
    return (float)demand->demand / (float)CARBON_DEMAND_MAX;
}

float carbon_demand_get_multiplier(const Carbon_Demand *demand) {
    return carbon_demand_get_multiplier_range(demand, 0.5f, 2.0f);
}

float carbon_demand_get_multiplier_range(const Carbon_Demand *demand, float min_mult, float max_mult) {
    if (!demand) return 1.0f;

    float normalized = (float)demand->demand / (float)CARBON_DEMAND_MAX;
    return min_mult + (max_mult - min_mult) * normalized;
}

void carbon_demand_set(Carbon_Demand *demand, uint8_t value) {
    CARBON_VALIDATE_PTR(demand);
    demand->demand = clamp_demand(value, demand->min_demand, demand->max_demand);
}

void carbon_demand_adjust(Carbon_Demand *demand, int delta) {
    CARBON_VALIDATE_PTR(demand);

    int new_value = (int)demand->demand + delta;
    demand->demand = clamp_demand(new_value, demand->min_demand, demand->max_demand);
}

void carbon_demand_reset(Carbon_Demand *demand) {
    CARBON_VALIDATE_PTR(demand);
    demand->demand = demand->equilibrium;
    demand->service_count = 0;
    demand->time_since_update = 0.0f;
}

uint8_t carbon_demand_get_equilibrium(const Carbon_Demand *demand) {
    return demand ? demand->equilibrium : 50;
}

void carbon_demand_set_equilibrium(Carbon_Demand *demand, uint8_t equilibrium) {
    CARBON_VALIDATE_PTR(demand);
    demand->equilibrium = clamp_demand(equilibrium, demand->min_demand, demand->max_demand);
}

uint32_t carbon_demand_get_total_services(const Carbon_Demand *demand) {
    return demand ? demand->total_services : 0;
}

bool carbon_demand_is_at_max(const Carbon_Demand *demand) {
    return demand && demand->demand >= demand->max_demand;
}

bool carbon_demand_is_at_min(const Carbon_Demand *demand) {
    return demand && demand->demand <= demand->min_demand;
}

const char *carbon_demand_get_level_string(const Carbon_Demand *demand) {
    if (!demand) return "Unknown";

    uint8_t d = demand->demand;
    if (d < 20) return "Very Low";
    if (d < 40) return "Low";
    if (d < 60) return "Medium";
    if (d < 80) return "High";
    return "Very High";
}
