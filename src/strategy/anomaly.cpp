/**
 * Carbon Anomaly / Discovery System
 *
 * Discoverable points of interest with research/investigation mechanics.
 */

#include "agentite/agentite.h"
#include "agentite/anomaly.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <time.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Anomaly type registry internal structure
 */
struct Agentite_AnomalyRegistry {
    Agentite_AnomalyTypeDef types[AGENTITE_ANOMALY_MAX_TYPES];
    int type_count;
};

/**
 * Anomaly manager internal structure
 */
struct Agentite_AnomalyManager {
    Agentite_AnomalyRegistry *registry;

    /* Anomaly instances */
    Agentite_Anomaly anomalies[AGENTITE_ANOMALY_MAX_INSTANCES];
    uint32_t next_id;

    /* Turn tracking */
    int32_t current_turn;

    /* Callbacks */
    Agentite_AnomalyRewardFunc reward_callback;
    void *reward_userdata;
    Agentite_AnomalyDiscoveryFunc discovery_callback;
    void *discovery_userdata;
    Agentite_AnomalySpawnFunc spawn_callback;
    void *spawn_userdata;
    Agentite_AnomalyCanResearchFunc can_research_callback;
    void *can_research_userdata;

    /* Random state */
    uint32_t random_state;
    float rarity_weights[AGENTITE_ANOMALY_RARITY_COUNT];
};

/*============================================================================
 * Random Number Generation
 *============================================================================*/

/**
 * Simple xorshift32 PRNG
 */
static uint32_t random_next(Agentite_AnomalyManager *mgr) {
    uint32_t x = mgr->random_state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    mgr->random_state = x;
    return x;
}

/**
 * Random float in [0, 1)
 */
static float random_float(Agentite_AnomalyManager *mgr) {
    return (float)(random_next(mgr) & 0x7FFFFFFF) / (float)0x80000000;
}

/**
 * Random int in [0, max)
 */
static int random_int(Agentite_AnomalyManager *mgr, int max) {
    if (max <= 0) return 0;
    return (int)(random_next(mgr) % (uint32_t)max);
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find anomaly by ID
 */
static Agentite_Anomaly *find_anomaly(Agentite_AnomalyManager *mgr, uint32_t id) {
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        if (mgr->anomalies[i].active && mgr->anomalies[i].id == id) {
            return &mgr->anomalies[i];
        }
    }
    return NULL;
}

/**
 * Find free anomaly slot
 */
static Agentite_Anomaly *alloc_anomaly(Agentite_AnomalyManager *mgr) {
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        if (!mgr->anomalies[i].active) {
            return &mgr->anomalies[i];
        }
    }
    return NULL;
}

/**
 * Calculate distance squared between two points
 */
static int32_t distance_squared(int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    int32_t dx = x2 - x1;
    int32_t dy = y2 - y1;
    return dx * dx + dy * dy;
}

/**
 * Select a random rarity based on weights
 */
static Agentite_AnomalyRarity select_rarity(Agentite_AnomalyManager *mgr,
                                           Agentite_AnomalyRarity max_rarity) {
    /* Calculate total weight up to max_rarity */
    float total = 0.0f;
    for (int i = 0; i <= (int)max_rarity; i++) {
        total += mgr->rarity_weights[i];
    }

    if (total <= 0.0f) return AGENTITE_ANOMALY_COMMON;

    /* Roll and select */
    float roll = random_float(mgr) * total;
    float cumulative = 0.0f;

    for (int i = 0; i <= (int)max_rarity; i++) {
        cumulative += mgr->rarity_weights[i];
        if (roll < cumulative) {
            return (Agentite_AnomalyRarity)i;
        }
    }

    return max_rarity;
}

/**
 * Select a random type from registry with given rarity
 */
static int select_type_by_rarity(Agentite_AnomalyManager *mgr, Agentite_AnomalyRarity rarity) {
    /* Count types with this rarity */
    int count = 0;
    int candidates[AGENTITE_ANOMALY_MAX_TYPES];

    for (int i = 0; i < mgr->registry->type_count; i++) {
        if (mgr->registry->types[i].rarity == rarity) {
            candidates[count++] = i;
        }
    }

    if (count == 0) return -1;

    /* Select random candidate */
    return candidates[random_int(mgr, count)];
}

/**
 * Build default anomaly result from type rewards
 */
static Agentite_AnomalyResult build_result(Agentite_AnomalyManager *mgr,
                                          const Agentite_Anomaly *anomaly,
                                          bool success) {
    Agentite_AnomalyResult result;
    memset(&result, 0, sizeof(result));
    result.success = success;

    if (!success) {
        strncpy(result.message, "Research failed", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type) {
        strncpy(result.message, "Unknown anomaly type", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    /* Copy rewards from type */
    result.reward_count = type->reward_count;
    for (int i = 0; i < type->reward_count && i < 4; i++) {
        result.rewards[i] = type->rewards[i];
    }

    snprintf(result.message, sizeof(result.message), "Completed research on %s", type->name);

    return result;
}

/*============================================================================
 * Registry Functions
 *============================================================================*/

Agentite_AnomalyRegistry *agentite_anomaly_registry_create(void) {
    Agentite_AnomalyRegistry *registry = AGENTITE_ALLOC(Agentite_AnomalyRegistry);
    if (!registry) {
        agentite_set_error("agentite_anomaly_registry_create: allocation failed");
        return NULL;
    }
    return registry;
}

void agentite_anomaly_registry_destroy(Agentite_AnomalyRegistry *registry) {
    if (registry) {
        free(registry);
    }
}

int agentite_anomaly_register_type(Agentite_AnomalyRegistry *registry,
                                  const Agentite_AnomalyTypeDef *def) {
    if (!registry || !def) return -1;

    if (registry->type_count >= AGENTITE_ANOMALY_MAX_TYPES) {
        agentite_set_error("Anomaly: Maximum types reached (%d/%d)", registry->type_count, AGENTITE_ANOMALY_MAX_TYPES);
        return -1;
    }

    /* Check for duplicate ID */
    for (int i = 0; i < registry->type_count; i++) {
        if (strcmp(registry->types[i].id, def->id) == 0) {
            agentite_set_error("agentite_anomaly_register_type: duplicate ID '%s'", def->id);
            return -1;
        }
    }

    int type_id = registry->type_count;
    registry->types[type_id] = *def;
    registry->type_count++;

    return type_id;
}

const Agentite_AnomalyTypeDef *agentite_anomaly_get_type(const Agentite_AnomalyRegistry *registry,
                                                      int type_id) {
    if (!registry || type_id < 0 || type_id >= registry->type_count) {
        return NULL;
    }
    return &registry->types[type_id];
}

int agentite_anomaly_find_type(const Agentite_AnomalyRegistry *registry, const char *id) {
    if (!registry || !id) return -1;

    for (int i = 0; i < registry->type_count; i++) {
        if (strcmp(registry->types[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

int agentite_anomaly_type_count(const Agentite_AnomalyRegistry *registry) {
    return registry ? registry->type_count : 0;
}

int agentite_anomaly_get_types_by_rarity(const Agentite_AnomalyRegistry *registry,
                                        Agentite_AnomalyRarity rarity,
                                        int *out_types, int max) {
    if (!registry || !out_types || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < registry->type_count && count < max; i++) {
        if (registry->types[i].rarity == rarity) {
            out_types[count++] = i;
        }
    }
    return count;
}

int agentite_anomaly_get_types_by_category(const Agentite_AnomalyRegistry *registry,
                                          int32_t category,
                                          int *out_types, int max) {
    if (!registry || !out_types || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < registry->type_count && count < max; i++) {
        if (registry->types[i].category == category) {
            out_types[count++] = i;
        }
    }
    return count;
}

Agentite_AnomalyTypeDef agentite_anomaly_type_default(void) {
    Agentite_AnomalyTypeDef def;
    memset(&def, 0, sizeof(def));

    strncpy(def.id, "unknown", sizeof(def.id) - 1);
    def.id[sizeof(def.id) - 1] = '\0';
    strncpy(def.name, "Unknown Anomaly", sizeof(def.name) - 1);
    def.name[sizeof(def.name) - 1] = '\0';
    strncpy(def.description, "An unidentified anomaly", sizeof(def.description) - 1);
    def.description[sizeof(def.description) - 1] = '\0';

    def.rarity = AGENTITE_ANOMALY_COMMON;
    def.research_time = 10.0f;
    def.research_multiplier = 1.0f;
    def.required_tech = -1;
    def.min_researchers = 0;
    def.repeatable = false;
    def.visible_undiscovered = false;
    def.dangerous = false;

    return def;
}

/*============================================================================
 * Manager Functions
 *============================================================================*/

Agentite_AnomalyManager *agentite_anomaly_manager_create(Agentite_AnomalyRegistry *registry) {
    if (!registry) {
        agentite_set_error("agentite_anomaly_manager_create: registry is NULL");
        return NULL;
    }

    Agentite_AnomalyManager *mgr = AGENTITE_ALLOC(Agentite_AnomalyManager);
    if (!mgr) {
        agentite_set_error("agentite_anomaly_manager_create: allocation failed");
        return NULL;
    }

    mgr->registry = registry;
    mgr->next_id = 1;
    mgr->current_turn = 0;

    /* Initialize random state */
    mgr->random_state = (uint32_t)time(NULL);

    /* Set default rarity weights */
    agentite_anomaly_get_default_weights(mgr->rarity_weights);

    return mgr;
}

void agentite_anomaly_manager_destroy(Agentite_AnomalyManager *mgr) {
    if (mgr) {
        free(mgr);
    }
}

Agentite_AnomalyRegistry *agentite_anomaly_manager_get_registry(Agentite_AnomalyManager *mgr) {
    return mgr ? mgr->registry : NULL;
}

/*============================================================================
 * Spawning
 *============================================================================*/

uint32_t agentite_anomaly_spawn(Agentite_AnomalyManager *mgr, int type_id,
                               int32_t x, int32_t y, uint32_t metadata) {
    Agentite_AnomalySpawnParams params;
    memset(&params, 0, sizeof(params));
    params.type_id = type_id;
    params.x = x;
    params.y = y;
    params.max_rarity = AGENTITE_ANOMALY_LEGENDARY;
    params.metadata = metadata;
    params.pre_discovered = false;
    params.discovered_by = -1;

    return agentite_anomaly_spawn_ex(mgr, &params);
}

uint32_t agentite_anomaly_spawn_ex(Agentite_AnomalyManager *mgr,
                                  const Agentite_AnomalySpawnParams *params) {
    if (!mgr || !params) return AGENTITE_ANOMALY_INVALID;

    /* Validate or select type */
    int type_id = params->type_id;
    if (type_id < 0) {
        /* Random type based on rarity */
        Agentite_AnomalyRarity rarity = select_rarity(mgr, params->max_rarity);
        type_id = select_type_by_rarity(mgr, rarity);

        /* If no type found for this rarity, try lower rarities */
        while (type_id < 0 && rarity > AGENTITE_ANOMALY_COMMON) {
            rarity = (Agentite_AnomalyRarity)((int)rarity - 1);
            type_id = select_type_by_rarity(mgr, rarity);
        }

        if (type_id < 0) {
            agentite_set_error("agentite_anomaly_spawn_ex: no types available");
            return AGENTITE_ANOMALY_INVALID;
        }
    } else if (type_id >= mgr->registry->type_count) {
        agentite_set_error("agentite_anomaly_spawn_ex: invalid type_id %d", type_id);
        return AGENTITE_ANOMALY_INVALID;
    }

    /* Allocate slot */
    Agentite_Anomaly *anomaly = alloc_anomaly(mgr);
    if (!anomaly) {
        agentite_set_error("Anomaly: Maximum anomalies reached (limit: %d)", AGENTITE_ANOMALY_MAX_INSTANCES);
        return AGENTITE_ANOMALY_INVALID;
    }

    /* Initialize anomaly */
    memset(anomaly, 0, sizeof(Agentite_Anomaly));
    anomaly->id = mgr->next_id++;
    anomaly->type_id = type_id;
    anomaly->x = params->x;
    anomaly->y = params->y;
    anomaly->metadata = params->metadata;
    anomaly->active = true;

    /* Set status */
    if (params->pre_discovered) {
        anomaly->status = AGENTITE_ANOMALY_DISCOVERED;
        anomaly->discovered_by = params->discovered_by;
        anomaly->discovered_turn = mgr->current_turn;
    } else {
        anomaly->status = AGENTITE_ANOMALY_UNDISCOVERED;
        anomaly->discovered_by = -1;
    }

    anomaly->researching_faction = -1;
    anomaly->researcher_entity = 0;
    anomaly->progress = 0.0f;
    anomaly->research_speed = 1.0f;
    anomaly->research_started_turn = -1;
    anomaly->completed_turn = -1;
    anomaly->times_completed = 0;

    /* Notify callback */
    if (mgr->spawn_callback) {
        mgr->spawn_callback(mgr, anomaly, mgr->spawn_userdata);
    }

    return anomaly->id;
}

uint32_t agentite_anomaly_spawn_random(Agentite_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      Agentite_AnomalyRarity max_rarity) {
    Agentite_AnomalySpawnParams params;
    memset(&params, 0, sizeof(params));
    params.type_id = -1;  /* Random */
    params.x = x;
    params.y = y;
    params.max_rarity = max_rarity;
    params.metadata = 0;
    params.pre_discovered = false;
    params.discovered_by = -1;

    return agentite_anomaly_spawn_ex(mgr, &params);
}

void agentite_anomaly_remove(Agentite_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (anomaly) {
        anomaly->active = false;
    }
}

/*============================================================================
 * Status and Progress
 *============================================================================*/

const Agentite_Anomaly *agentite_anomaly_get(const Agentite_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return NULL;
    return find_anomaly((Agentite_AnomalyManager *)mgr, id);
}

Agentite_Anomaly *agentite_anomaly_get_mut(Agentite_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return NULL;
    return find_anomaly(mgr, id);
}

Agentite_AnomalyStatus agentite_anomaly_get_status(const Agentite_AnomalyManager *mgr, uint32_t id) {
    const Agentite_Anomaly *anomaly = agentite_anomaly_get(mgr, id);
    return anomaly ? anomaly->status : AGENTITE_ANOMALY_UNDISCOVERED;
}

bool agentite_anomaly_discover(Agentite_AnomalyManager *mgr, uint32_t id, int32_t faction_id) {
    if (!mgr) return false;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) return false;

    if (anomaly->status != AGENTITE_ANOMALY_UNDISCOVERED) {
        return false;  /* Already discovered */
    }

    anomaly->status = AGENTITE_ANOMALY_DISCOVERED;
    anomaly->discovered_by = faction_id;
    anomaly->discovered_turn = mgr->current_turn;

    /* Notify callback */
    if (mgr->discovery_callback) {
        mgr->discovery_callback(mgr, anomaly, faction_id, mgr->discovery_userdata);
    }

    return true;
}

bool agentite_anomaly_start_research(Agentite_AnomalyManager *mgr, uint32_t id,
                                    int32_t faction_id, uint32_t researcher) {
    if (!mgr) return false;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) return false;

    /* Must be discovered first */
    if (anomaly->status == AGENTITE_ANOMALY_UNDISCOVERED) {
        return false;
    }

    /* Can't research if already researching or completed (unless repeatable) */
    if (anomaly->status == AGENTITE_ANOMALY_RESEARCHING) {
        return false;
    }

    if (anomaly->status == AGENTITE_ANOMALY_COMPLETED ||
        anomaly->status == AGENTITE_ANOMALY_DEPLETED) {
        /* Check if repeatable */
        const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                      anomaly->type_id);
        if (!type || !type->repeatable) {
            return false;
        }
    }

    /* Check custom validator */
    if (mgr->can_research_callback) {
        if (!mgr->can_research_callback(mgr, anomaly, faction_id, mgr->can_research_userdata)) {
            return false;
        }
    }

    /* Check type requirements */
    const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (type && type->min_researchers > 0 && researcher == 0) {
        return false;  /* Needs a researcher entity */
    }

    /* Start research */
    anomaly->status = AGENTITE_ANOMALY_RESEARCHING;
    anomaly->researching_faction = faction_id;
    anomaly->researcher_entity = researcher;
    anomaly->research_started_turn = mgr->current_turn;

    /* Reset progress if repeatable */
    if (anomaly->times_completed > 0) {
        anomaly->progress = 0.0f;
    }

    return true;
}

void agentite_anomaly_stop_research(Agentite_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly || anomaly->status != AGENTITE_ANOMALY_RESEARCHING) {
        return;
    }

    /* Revert to discovered state (keep progress) */
    anomaly->status = AGENTITE_ANOMALY_DISCOVERED;
    anomaly->researching_faction = -1;
    anomaly->researcher_entity = 0;
}

bool agentite_anomaly_add_progress(Agentite_AnomalyManager *mgr, uint32_t id, float amount) {
    if (!mgr) return false;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly || anomaly->status != AGENTITE_ANOMALY_RESEARCHING) {
        return false;
    }

    const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type || type->research_time <= 0) {
        return false;
    }

    /* Calculate progress based on research time and speed */
    float progress_per_unit = 1.0f / type->research_time;
    float effective_speed = anomaly->research_speed * type->research_multiplier;
    float progress_delta = amount * progress_per_unit * effective_speed;

    anomaly->progress += progress_delta;

    /* Check for completion */
    if (anomaly->progress >= 1.0f) {
        anomaly->progress = 1.0f;
        anomaly->status = AGENTITE_ANOMALY_COMPLETED;
        anomaly->completed_turn = mgr->current_turn;
        anomaly->times_completed++;

        /* Generate result and notify */
        Agentite_AnomalyResult result = build_result(mgr, anomaly, true);

        if (mgr->reward_callback) {
            mgr->reward_callback(mgr, anomaly, &result, mgr->reward_userdata);
        }

        return true;
    }

    return false;
}

void agentite_anomaly_set_progress(Agentite_AnomalyManager *mgr, uint32_t id, float progress) {
    if (!mgr) return;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) return;

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    anomaly->progress = progress;
}

float agentite_anomaly_get_progress(const Agentite_AnomalyManager *mgr, uint32_t id) {
    const Agentite_Anomaly *anomaly = agentite_anomaly_get(mgr, id);
    return anomaly ? anomaly->progress : 0.0f;
}

bool agentite_anomaly_is_complete(const Agentite_AnomalyManager *mgr, uint32_t id) {
    const Agentite_Anomaly *anomaly = agentite_anomaly_get(mgr, id);
    return anomaly && (anomaly->status == AGENTITE_ANOMALY_COMPLETED ||
                       anomaly->status == AGENTITE_ANOMALY_DEPLETED);
}

Agentite_AnomalyResult agentite_anomaly_complete_instant(Agentite_AnomalyManager *mgr, uint32_t id) {
    Agentite_AnomalyResult result;
    memset(&result, 0, sizeof(result));

    if (!mgr) {
        strncpy(result.message, "Invalid manager", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) {
        strncpy(result.message, "Anomaly not found", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    /* Must be at least discovered */
    if (anomaly->status == AGENTITE_ANOMALY_UNDISCOVERED) {
        strncpy(result.message, "Anomaly not discovered", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    /* Complete */
    anomaly->progress = 1.0f;
    anomaly->status = AGENTITE_ANOMALY_COMPLETED;
    anomaly->completed_turn = mgr->current_turn;
    anomaly->times_completed++;

    result = build_result(mgr, anomaly, true);

    if (mgr->reward_callback) {
        mgr->reward_callback(mgr, anomaly, &result, mgr->reward_userdata);
    }

    return result;
}

Agentite_AnomalyResult agentite_anomaly_collect_rewards(Agentite_AnomalyManager *mgr, uint32_t id) {
    Agentite_AnomalyResult result;
    memset(&result, 0, sizeof(result));

    if (!mgr) {
        strncpy(result.message, "Invalid manager", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) {
        strncpy(result.message, "Anomaly not found", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    if (anomaly->status != AGENTITE_ANOMALY_COMPLETED) {
        strncpy(result.message, "Anomaly not completed", sizeof(result.message) - 1);
        result.message[sizeof(result.message) - 1] = '\0';
        return result;
    }

    result = build_result(mgr, anomaly, true);

    /* Check if repeatable */
    const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (type && type->repeatable) {
        /* Reset to discovered for re-research */
        anomaly->status = AGENTITE_ANOMALY_DISCOVERED;
        anomaly->progress = 0.0f;
        anomaly->researching_faction = -1;
        anomaly->researcher_entity = 0;
    } else {
        /* Mark as depleted */
        anomaly->status = AGENTITE_ANOMALY_DEPLETED;
    }

    return result;
}

void agentite_anomaly_deplete(Agentite_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (anomaly) {
        anomaly->status = AGENTITE_ANOMALY_DEPLETED;
    }
}

/*============================================================================
 * Research Speed
 *============================================================================*/

void agentite_anomaly_set_research_speed(Agentite_AnomalyManager *mgr, uint32_t id, float speed) {
    if (!mgr) return;

    Agentite_Anomaly *anomaly = find_anomaly(mgr, id);
    if (anomaly) {
        if (speed < 0.0f) speed = 0.0f;
        anomaly->research_speed = speed;
    }
}

float agentite_anomaly_get_remaining_time(const Agentite_AnomalyManager *mgr, uint32_t id) {
    const Agentite_Anomaly *anomaly = agentite_anomaly_get(mgr, id);
    if (!anomaly) return 0.0f;

    const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type || type->research_time <= 0) return 0.0f;

    float remaining_progress = 1.0f - anomaly->progress;
    float effective_speed = anomaly->research_speed * type->research_multiplier;
    if (effective_speed <= 0.0f) return INFINITY;

    return remaining_progress * type->research_time / effective_speed;
}

float agentite_anomaly_get_total_time(const Agentite_AnomalyManager *mgr, uint32_t id) {
    const Agentite_Anomaly *anomaly = agentite_anomaly_get(mgr, id);
    if (!anomaly) return 0.0f;

    const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type) return 0.0f;

    float effective_speed = anomaly->research_speed * type->research_multiplier;
    if (effective_speed <= 0.0f) return INFINITY;

    return type->research_time / effective_speed;
}

/*============================================================================
 * Queries
 *============================================================================*/

int agentite_anomaly_get_at(const Agentite_AnomalyManager *mgr, int32_t x, int32_t y,
                           uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->x == x && a->y == y) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int agentite_anomaly_get_by_status(const Agentite_AnomalyManager *mgr, Agentite_AnomalyStatus status,
                                  uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->status == status) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int agentite_anomaly_get_by_type(const Agentite_AnomalyManager *mgr, int type_id,
                                uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->type_id == type_id) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int agentite_anomaly_get_by_faction(const Agentite_AnomalyManager *mgr, int32_t faction_id,
                                   uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->discovered_by == faction_id) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int agentite_anomaly_get_in_rect(const Agentite_AnomalyManager *mgr,
                                int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    /* Normalize rectangle */
    if (x1 > x2) { int32_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int32_t t = y1; y1 = y2; y2 = t; }

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->x >= x1 && a->x <= x2 && a->y >= y1 && a->y <= y2) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int agentite_anomaly_get_in_radius(const Agentite_AnomalyManager *mgr,
                                  int32_t center_x, int32_t center_y, int32_t radius,
                                  uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int32_t radius_sq = radius * radius;
    int count = 0;

    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (!a->active) continue;

        int32_t dist_sq = distance_squared(a->x, a->y, center_x, center_y);
        if (dist_sq <= radius_sq) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int agentite_anomaly_get_all(const Agentite_AnomalyManager *mgr, uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES && count < max; i++) {
        if (mgr->anomalies[i].active) {
            out_ids[count++] = mgr->anomalies[i].id;
        }
    }
    return count;
}

bool agentite_anomaly_has_at(const Agentite_AnomalyManager *mgr, int32_t x, int32_t y) {
    if (!mgr) return false;

    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->x == x && a->y == y) {
            return true;
        }
    }
    return false;
}

uint32_t agentite_anomaly_find_nearest(const Agentite_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      int32_t max_distance,
                                      int status) {
    if (!mgr) return AGENTITE_ANOMALY_INVALID;

    int32_t max_dist_sq = (max_distance < 0) ? INT32_MAX : (max_distance * max_distance);
    int32_t best_dist_sq = INT32_MAX;
    uint32_t best_id = AGENTITE_ANOMALY_INVALID;

    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (!a->active) continue;

        /* Status filter */
        if (status >= 0 && a->status != (Agentite_AnomalyStatus)status) {
            continue;
        }

        int32_t dist_sq = distance_squared(a->x, a->y, x, y);
        if (dist_sq <= max_dist_sq && dist_sq < best_dist_sq) {
            best_dist_sq = dist_sq;
            best_id = a->id;
        }
    }

    return best_id;
}

/*============================================================================
 * Validation
 *============================================================================*/

bool agentite_anomaly_can_research(const Agentite_AnomalyManager *mgr, uint32_t id,
                                  int32_t faction_id) {
    if (!mgr) return false;

    const Agentite_Anomaly *anomaly = agentite_anomaly_get(mgr, id);
    if (!anomaly) return false;

    /* Must be discovered */
    if (anomaly->status == AGENTITE_ANOMALY_UNDISCOVERED) {
        return false;
    }

    /* Can't research if already in progress by another faction */
    if (anomaly->status == AGENTITE_ANOMALY_RESEARCHING &&
        anomaly->researching_faction != faction_id) {
        return false;
    }

    /* Check if completed and not repeatable */
    if (anomaly->status == AGENTITE_ANOMALY_COMPLETED ||
        anomaly->status == AGENTITE_ANOMALY_DEPLETED) {
        const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                      anomaly->type_id);
        if (!type || !type->repeatable) {
            return false;
        }
    }

    /* Custom validator */
    if (mgr->can_research_callback) {
        return mgr->can_research_callback(mgr, anomaly, faction_id,
                                           ((Agentite_AnomalyManager *)mgr)->can_research_userdata);
    }

    return true;
}

bool agentite_anomaly_can_spawn_at(const Agentite_AnomalyManager *mgr, int32_t x, int32_t y) {
    /* By default, can spawn anywhere that doesn't already have an anomaly */
    return !agentite_anomaly_has_at(mgr, x, y);
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_anomaly_set_reward_callback(Agentite_AnomalyManager *mgr,
                                         Agentite_AnomalyRewardFunc callback,
                                         void *userdata) {
    if (mgr) {
        mgr->reward_callback = callback;
        mgr->reward_userdata = userdata;
    }
}

void agentite_anomaly_set_discovery_callback(Agentite_AnomalyManager *mgr,
                                            Agentite_AnomalyDiscoveryFunc callback,
                                            void *userdata) {
    if (mgr) {
        mgr->discovery_callback = callback;
        mgr->discovery_userdata = userdata;
    }
}

void agentite_anomaly_set_spawn_callback(Agentite_AnomalyManager *mgr,
                                        Agentite_AnomalySpawnFunc callback,
                                        void *userdata) {
    if (mgr) {
        mgr->spawn_callback = callback;
        mgr->spawn_userdata = userdata;
    }
}

void agentite_anomaly_set_can_research_callback(Agentite_AnomalyManager *mgr,
                                               Agentite_AnomalyCanResearchFunc callback,
                                               void *userdata) {
    if (mgr) {
        mgr->can_research_callback = callback;
        mgr->can_research_userdata = userdata;
    }
}

/*============================================================================
 * Statistics
 *============================================================================*/

void agentite_anomaly_get_stats(const Agentite_AnomalyManager *mgr, Agentite_AnomalyStats *out_stats) {
    if (!out_stats) return;

    memset(out_stats, 0, sizeof(Agentite_AnomalyStats));

    if (!mgr) return;

    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        const Agentite_Anomaly *a = &mgr->anomalies[i];
        if (!a->active) continue;

        out_stats->total_count++;

        switch (a->status) {
            case AGENTITE_ANOMALY_UNDISCOVERED:
                out_stats->undiscovered_count++;
                break;
            case AGENTITE_ANOMALY_DISCOVERED:
                out_stats->discovered_count++;
                break;
            case AGENTITE_ANOMALY_RESEARCHING:
                out_stats->researching_count++;
                break;
            case AGENTITE_ANOMALY_COMPLETED:
                out_stats->completed_count++;
                break;
            case AGENTITE_ANOMALY_DEPLETED:
                out_stats->depleted_count++;
                break;
        }

        /* Count by rarity */
        const Agentite_AnomalyTypeDef *type = agentite_anomaly_get_type(mgr->registry,
                                                                      a->type_id);
        if (type && type->rarity < AGENTITE_ANOMALY_RARITY_COUNT) {
            out_stats->by_rarity[type->rarity]++;
        }
    }
}

int agentite_anomaly_count(const Agentite_AnomalyManager *mgr) {
    if (!mgr) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        if (mgr->anomalies[i].active) count++;
    }
    return count;
}

/*============================================================================
 * Turn Management
 *============================================================================*/

void agentite_anomaly_set_turn(Agentite_AnomalyManager *mgr, int32_t turn) {
    if (mgr) {
        mgr->current_turn = turn;
    }
}

void agentite_anomaly_update(Agentite_AnomalyManager *mgr, float delta_time) {
    if (!mgr || delta_time <= 0.0f) return;

    /* Update all researching anomalies */
    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        Agentite_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->status == AGENTITE_ANOMALY_RESEARCHING) {
            agentite_anomaly_add_progress(mgr, a->id, delta_time);
        }
    }
}

void agentite_anomaly_clear(Agentite_AnomalyManager *mgr) {
    if (!mgr) return;

    for (int i = 0; i < AGENTITE_ANOMALY_MAX_INSTANCES; i++) {
        mgr->anomalies[i].active = false;
    }
}

/*============================================================================
 * Random Generation
 *============================================================================*/

void agentite_anomaly_set_seed(Agentite_AnomalyManager *mgr, uint32_t seed) {
    if (mgr) {
        mgr->random_state = seed ? seed : (uint32_t)time(NULL);
    }
}

void agentite_anomaly_set_rarity_weights(Agentite_AnomalyManager *mgr, const float *weights) {
    if (mgr && weights) {
        for (int i = 0; i < AGENTITE_ANOMALY_RARITY_COUNT; i++) {
            mgr->rarity_weights[i] = weights[i];
        }
    }
}

void agentite_anomaly_get_default_weights(float *out_weights) {
    if (!out_weights) return;

    /* Default distribution: 60%, 25%, 12%, 3% */
    out_weights[AGENTITE_ANOMALY_COMMON] = 0.60f;
    out_weights[AGENTITE_ANOMALY_UNCOMMON] = 0.25f;
    out_weights[AGENTITE_ANOMALY_RARE] = 0.12f;
    out_weights[AGENTITE_ANOMALY_LEGENDARY] = 0.03f;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_anomaly_rarity_name(Agentite_AnomalyRarity rarity) {
    switch (rarity) {
        case AGENTITE_ANOMALY_COMMON:    return "Common";
        case AGENTITE_ANOMALY_UNCOMMON:  return "Uncommon";
        case AGENTITE_ANOMALY_RARE:      return "Rare";
        case AGENTITE_ANOMALY_LEGENDARY: return "Legendary";
        default:                       return "Unknown";
    }
}

const char *agentite_anomaly_status_name(Agentite_AnomalyStatus status) {
    switch (status) {
        case AGENTITE_ANOMALY_UNDISCOVERED: return "Undiscovered";
        case AGENTITE_ANOMALY_DISCOVERED:   return "Discovered";
        case AGENTITE_ANOMALY_RESEARCHING:  return "Researching";
        case AGENTITE_ANOMALY_COMPLETED:    return "Completed";
        case AGENTITE_ANOMALY_DEPLETED:     return "Depleted";
        default:                          return "Unknown";
    }
}

const char *agentite_anomaly_reward_type_name(Agentite_AnomalyRewardType type) {
    switch (type) {
        case AGENTITE_ANOMALY_REWARD_NONE:      return "None";
        case AGENTITE_ANOMALY_REWARD_RESOURCES: return "Resources";
        case AGENTITE_ANOMALY_REWARD_TECH:      return "Technology";
        case AGENTITE_ANOMALY_REWARD_UNIT:      return "Unit";
        case AGENTITE_ANOMALY_REWARD_MODIFIER:  return "Modifier";
        case AGENTITE_ANOMALY_REWARD_ARTIFACT:  return "Artifact";
        case AGENTITE_ANOMALY_REWARD_MAP:       return "Map";
        case AGENTITE_ANOMALY_REWARD_CUSTOM:    return "Custom";
        default:                              return "Unknown";
    }
}
