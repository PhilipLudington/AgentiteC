#include "agentite/demand.h"
#include "agentite/validate.h"

/* Clamp value to uint8_t range with min/max */
static inline uint8_t clamp_demand(int value, uint8_t min_val, uint8_t max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return (uint8_t)value;
}

void agentite_demand_init(Agentite_Demand *demand, uint8_t initial, uint8_t equilibrium) {
    agentite_demand_init_ex(demand, initial, equilibrium,
                          AGENTITE_DEMAND_MIN, AGENTITE_DEMAND_MAX,
                          AGENTITE_DEMAND_DEFAULT_GROWTH_PER_SERVICE,
                          AGENTITE_DEMAND_DEFAULT_DECAY_RATE,
                          AGENTITE_DEMAND_DEFAULT_UPDATE_INTERVAL);
}

void agentite_demand_init_ex(Agentite_Demand *demand,
                           uint8_t initial,
                           uint8_t equilibrium,
                           uint8_t min_demand,
                           uint8_t max_demand,
                           float growth_per_service,
                           float decay_rate,
                           float update_interval) {
    AGENTITE_VALIDATE_PTR(demand);

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

    demand->update_interval = update_interval > 0.0f ? update_interval : AGENTITE_DEMAND_DEFAULT_UPDATE_INTERVAL;
    demand->time_since_update = 0.0f;
    demand->service_count = 0;
    demand->total_services = 0;

    demand->growth_per_service = growth_per_service > 0.0f ? growth_per_service : AGENTITE_DEMAND_DEFAULT_GROWTH_PER_SERVICE;
    demand->decay_rate = decay_rate > 0.0f ? decay_rate : AGENTITE_DEMAND_DEFAULT_DECAY_RATE;
}

void agentite_demand_record_service(Agentite_Demand *demand) {
    AGENTITE_VALIDATE_PTR(demand);

    demand->service_count++;
    demand->total_services++;

    /* Immediate growth from service */
    float new_demand = (float)demand->demand + demand->growth_per_service;
    demand->demand = clamp_demand((int)new_demand, demand->min_demand, demand->max_demand);
}

void agentite_demand_record_services(Agentite_Demand *demand, uint32_t count) {
    AGENTITE_VALIDATE_PTR(demand);
    if (count == 0) return;

    demand->service_count += count;
    demand->total_services += count;

    float new_demand = (float)demand->demand + (demand->growth_per_service * count);
    demand->demand = clamp_demand((int)new_demand, demand->min_demand, demand->max_demand);
}

void agentite_demand_update(Agentite_Demand *demand, float dt) {
    AGENTITE_VALIDATE_PTR(demand);

    demand->time_since_update += dt;

    while (demand->time_since_update >= demand->update_interval) {
        demand->time_since_update -= demand->update_interval;
        agentite_demand_tick(demand);
    }
}

void agentite_demand_tick(Agentite_Demand *demand) {
    AGENTITE_VALIDATE_PTR(demand);

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

uint8_t agentite_demand_get(const Agentite_Demand *demand) {
    return demand ? demand->demand : 0;
}

float agentite_demand_get_normalized(const Agentite_Demand *demand) {
    if (!demand) return 0.0f;
    return (float)demand->demand / (float)AGENTITE_DEMAND_MAX;
}

float agentite_demand_get_multiplier(const Agentite_Demand *demand) {
    return agentite_demand_get_multiplier_range(demand, 0.5f, 2.0f);
}

float agentite_demand_get_multiplier_range(const Agentite_Demand *demand, float min_mult, float max_mult) {
    if (!demand) return 1.0f;

    float normalized = (float)demand->demand / (float)AGENTITE_DEMAND_MAX;
    return min_mult + (max_mult - min_mult) * normalized;
}

void agentite_demand_set(Agentite_Demand *demand, uint8_t value) {
    AGENTITE_VALIDATE_PTR(demand);
    demand->demand = clamp_demand(value, demand->min_demand, demand->max_demand);
}

void agentite_demand_adjust(Agentite_Demand *demand, int delta) {
    AGENTITE_VALIDATE_PTR(demand);

    int new_value = (int)demand->demand + delta;
    demand->demand = clamp_demand(new_value, demand->min_demand, demand->max_demand);
}

void agentite_demand_reset(Agentite_Demand *demand) {
    AGENTITE_VALIDATE_PTR(demand);
    demand->demand = demand->equilibrium;
    demand->service_count = 0;
    demand->time_since_update = 0.0f;
}

uint8_t agentite_demand_get_equilibrium(const Agentite_Demand *demand) {
    return demand ? demand->equilibrium : 50;
}

void agentite_demand_set_equilibrium(Agentite_Demand *demand, uint8_t equilibrium) {
    AGENTITE_VALIDATE_PTR(demand);
    demand->equilibrium = clamp_demand(equilibrium, demand->min_demand, demand->max_demand);
}

uint32_t agentite_demand_get_total_services(const Agentite_Demand *demand) {
    return demand ? demand->total_services : 0;
}

bool agentite_demand_is_at_max(const Agentite_Demand *demand) {
    return demand && demand->demand >= demand->max_demand;
}

bool agentite_demand_is_at_min(const Agentite_Demand *demand) {
    return demand && demand->demand <= demand->min_demand;
}

const char *agentite_demand_get_level_string(const Agentite_Demand *demand) {
    if (!demand) return "Unknown";

    uint8_t d = demand->demand;
    if (d < 20) return "Very Low";
    if (d < 40) return "Low";
    if (d < 60) return "Medium";
    if (d < 80) return "High";
    return "Very High";
}
