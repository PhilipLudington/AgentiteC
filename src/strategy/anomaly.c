/**
 * Carbon Anomaly / Discovery System
 *
 * Discoverable points of interest with research/investigation mechanics.
 */

#include "carbon/anomaly.h"
#include "carbon/error.h"

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
struct Carbon_AnomalyRegistry {
    Carbon_AnomalyTypeDef types[CARBON_ANOMALY_MAX_TYPES];
    int type_count;
};

/**
 * Anomaly manager internal structure
 */
struct Carbon_AnomalyManager {
    Carbon_AnomalyRegistry *registry;

    /* Anomaly instances */
    Carbon_Anomaly anomalies[CARBON_ANOMALY_MAX_INSTANCES];
    uint32_t next_id;

    /* Turn tracking */
    int32_t current_turn;

    /* Callbacks */
    Carbon_AnomalyRewardFunc reward_callback;
    void *reward_userdata;
    Carbon_AnomalyDiscoveryFunc discovery_callback;
    void *discovery_userdata;
    Carbon_AnomalySpawnFunc spawn_callback;
    void *spawn_userdata;
    Carbon_AnomalyCanResearchFunc can_research_callback;
    void *can_research_userdata;

    /* Random state */
    uint32_t random_state;
    float rarity_weights[CARBON_ANOMALY_RARITY_COUNT];
};

/*============================================================================
 * Random Number Generation
 *============================================================================*/

/**
 * Simple xorshift32 PRNG
 */
static uint32_t random_next(Carbon_AnomalyManager *mgr) {
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
static float random_float(Carbon_AnomalyManager *mgr) {
    return (float)(random_next(mgr) & 0x7FFFFFFF) / (float)0x80000000;
}

/**
 * Random int in [0, max)
 */
static int random_int(Carbon_AnomalyManager *mgr, int max) {
    if (max <= 0) return 0;
    return (int)(random_next(mgr) % (uint32_t)max);
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find anomaly by ID
 */
static Carbon_Anomaly *find_anomaly(Carbon_AnomalyManager *mgr, uint32_t id) {
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        if (mgr->anomalies[i].active && mgr->anomalies[i].id == id) {
            return &mgr->anomalies[i];
        }
    }
    return NULL;
}

/**
 * Find free anomaly slot
 */
static Carbon_Anomaly *alloc_anomaly(Carbon_AnomalyManager *mgr) {
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
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
static Carbon_AnomalyRarity select_rarity(Carbon_AnomalyManager *mgr,
                                           Carbon_AnomalyRarity max_rarity) {
    /* Calculate total weight up to max_rarity */
    float total = 0.0f;
    for (int i = 0; i <= (int)max_rarity; i++) {
        total += mgr->rarity_weights[i];
    }

    if (total <= 0.0f) return CARBON_ANOMALY_COMMON;

    /* Roll and select */
    float roll = random_float(mgr) * total;
    float cumulative = 0.0f;

    for (int i = 0; i <= (int)max_rarity; i++) {
        cumulative += mgr->rarity_weights[i];
        if (roll < cumulative) {
            return (Carbon_AnomalyRarity)i;
        }
    }

    return max_rarity;
}

/**
 * Select a random type from registry with given rarity
 */
static int select_type_by_rarity(Carbon_AnomalyManager *mgr, Carbon_AnomalyRarity rarity) {
    /* Count types with this rarity */
    int count = 0;
    int candidates[CARBON_ANOMALY_MAX_TYPES];

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
static Carbon_AnomalyResult build_result(Carbon_AnomalyManager *mgr,
                                          const Carbon_Anomaly *anomaly,
                                          bool success) {
    Carbon_AnomalyResult result;
    memset(&result, 0, sizeof(result));
    result.success = success;

    if (!success) {
        strcpy(result.message, "Research failed");
        return result;
    }

    const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type) {
        strcpy(result.message, "Unknown anomaly type");
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

Carbon_AnomalyRegistry *carbon_anomaly_registry_create(void) {
    Carbon_AnomalyRegistry *registry = calloc(1, sizeof(Carbon_AnomalyRegistry));
    if (!registry) {
        carbon_set_error("carbon_anomaly_registry_create: allocation failed");
        return NULL;
    }
    return registry;
}

void carbon_anomaly_registry_destroy(Carbon_AnomalyRegistry *registry) {
    if (registry) {
        free(registry);
    }
}

int carbon_anomaly_register_type(Carbon_AnomalyRegistry *registry,
                                  const Carbon_AnomalyTypeDef *def) {
    if (!registry || !def) return -1;

    if (registry->type_count >= CARBON_ANOMALY_MAX_TYPES) {
        carbon_set_error("carbon_anomaly_register_type: max types reached");
        return -1;
    }

    /* Check for duplicate ID */
    for (int i = 0; i < registry->type_count; i++) {
        if (strcmp(registry->types[i].id, def->id) == 0) {
            carbon_set_error("carbon_anomaly_register_type: duplicate ID '%s'", def->id);
            return -1;
        }
    }

    int type_id = registry->type_count;
    registry->types[type_id] = *def;
    registry->type_count++;

    return type_id;
}

const Carbon_AnomalyTypeDef *carbon_anomaly_get_type(const Carbon_AnomalyRegistry *registry,
                                                      int type_id) {
    if (!registry || type_id < 0 || type_id >= registry->type_count) {
        return NULL;
    }
    return &registry->types[type_id];
}

int carbon_anomaly_find_type(const Carbon_AnomalyRegistry *registry, const char *id) {
    if (!registry || !id) return -1;

    for (int i = 0; i < registry->type_count; i++) {
        if (strcmp(registry->types[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

int carbon_anomaly_type_count(const Carbon_AnomalyRegistry *registry) {
    return registry ? registry->type_count : 0;
}

int carbon_anomaly_get_types_by_rarity(const Carbon_AnomalyRegistry *registry,
                                        Carbon_AnomalyRarity rarity,
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

int carbon_anomaly_get_types_by_category(const Carbon_AnomalyRegistry *registry,
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

Carbon_AnomalyTypeDef carbon_anomaly_type_default(void) {
    Carbon_AnomalyTypeDef def;
    memset(&def, 0, sizeof(def));

    strcpy(def.id, "unknown");
    strcpy(def.name, "Unknown Anomaly");
    strcpy(def.description, "An unidentified anomaly");

    def.rarity = CARBON_ANOMALY_COMMON;
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

Carbon_AnomalyManager *carbon_anomaly_manager_create(Carbon_AnomalyRegistry *registry) {
    if (!registry) {
        carbon_set_error("carbon_anomaly_manager_create: registry is NULL");
        return NULL;
    }

    Carbon_AnomalyManager *mgr = calloc(1, sizeof(Carbon_AnomalyManager));
    if (!mgr) {
        carbon_set_error("carbon_anomaly_manager_create: allocation failed");
        return NULL;
    }

    mgr->registry = registry;
    mgr->next_id = 1;
    mgr->current_turn = 0;

    /* Initialize random state */
    mgr->random_state = (uint32_t)time(NULL);

    /* Set default rarity weights */
    carbon_anomaly_get_default_weights(mgr->rarity_weights);

    return mgr;
}

void carbon_anomaly_manager_destroy(Carbon_AnomalyManager *mgr) {
    if (mgr) {
        free(mgr);
    }
}

Carbon_AnomalyRegistry *carbon_anomaly_manager_get_registry(Carbon_AnomalyManager *mgr) {
    return mgr ? mgr->registry : NULL;
}

/*============================================================================
 * Spawning
 *============================================================================*/

uint32_t carbon_anomaly_spawn(Carbon_AnomalyManager *mgr, int type_id,
                               int32_t x, int32_t y, uint32_t metadata) {
    Carbon_AnomalySpawnParams params;
    memset(&params, 0, sizeof(params));
    params.type_id = type_id;
    params.x = x;
    params.y = y;
    params.max_rarity = CARBON_ANOMALY_LEGENDARY;
    params.metadata = metadata;
    params.pre_discovered = false;
    params.discovered_by = -1;

    return carbon_anomaly_spawn_ex(mgr, &params);
}

uint32_t carbon_anomaly_spawn_ex(Carbon_AnomalyManager *mgr,
                                  const Carbon_AnomalySpawnParams *params) {
    if (!mgr || !params) return CARBON_ANOMALY_INVALID;

    /* Validate or select type */
    int type_id = params->type_id;
    if (type_id < 0) {
        /* Random type based on rarity */
        Carbon_AnomalyRarity rarity = select_rarity(mgr, params->max_rarity);
        type_id = select_type_by_rarity(mgr, rarity);

        /* If no type found for this rarity, try lower rarities */
        while (type_id < 0 && rarity > CARBON_ANOMALY_COMMON) {
            rarity = (Carbon_AnomalyRarity)((int)rarity - 1);
            type_id = select_type_by_rarity(mgr, rarity);
        }

        if (type_id < 0) {
            carbon_set_error("carbon_anomaly_spawn_ex: no types available");
            return CARBON_ANOMALY_INVALID;
        }
    } else if (type_id >= mgr->registry->type_count) {
        carbon_set_error("carbon_anomaly_spawn_ex: invalid type_id %d", type_id);
        return CARBON_ANOMALY_INVALID;
    }

    /* Allocate slot */
    Carbon_Anomaly *anomaly = alloc_anomaly(mgr);
    if (!anomaly) {
        carbon_set_error("carbon_anomaly_spawn_ex: max anomalies reached");
        return CARBON_ANOMALY_INVALID;
    }

    /* Initialize anomaly */
    memset(anomaly, 0, sizeof(Carbon_Anomaly));
    anomaly->id = mgr->next_id++;
    anomaly->type_id = type_id;
    anomaly->x = params->x;
    anomaly->y = params->y;
    anomaly->metadata = params->metadata;
    anomaly->active = true;

    /* Set status */
    if (params->pre_discovered) {
        anomaly->status = CARBON_ANOMALY_DISCOVERED;
        anomaly->discovered_by = params->discovered_by;
        anomaly->discovered_turn = mgr->current_turn;
    } else {
        anomaly->status = CARBON_ANOMALY_UNDISCOVERED;
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

uint32_t carbon_anomaly_spawn_random(Carbon_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      Carbon_AnomalyRarity max_rarity) {
    Carbon_AnomalySpawnParams params;
    memset(&params, 0, sizeof(params));
    params.type_id = -1;  /* Random */
    params.x = x;
    params.y = y;
    params.max_rarity = max_rarity;
    params.metadata = 0;
    params.pre_discovered = false;
    params.discovered_by = -1;

    return carbon_anomaly_spawn_ex(mgr, &params);
}

void carbon_anomaly_remove(Carbon_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (anomaly) {
        anomaly->active = false;
    }
}

/*============================================================================
 * Status and Progress
 *============================================================================*/

const Carbon_Anomaly *carbon_anomaly_get(const Carbon_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return NULL;
    return find_anomaly((Carbon_AnomalyManager *)mgr, id);
}

Carbon_Anomaly *carbon_anomaly_get_mut(Carbon_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return NULL;
    return find_anomaly(mgr, id);
}

Carbon_AnomalyStatus carbon_anomaly_get_status(const Carbon_AnomalyManager *mgr, uint32_t id) {
    const Carbon_Anomaly *anomaly = carbon_anomaly_get(mgr, id);
    return anomaly ? anomaly->status : CARBON_ANOMALY_UNDISCOVERED;
}

bool carbon_anomaly_discover(Carbon_AnomalyManager *mgr, uint32_t id, int32_t faction_id) {
    if (!mgr) return false;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) return false;

    if (anomaly->status != CARBON_ANOMALY_UNDISCOVERED) {
        return false;  /* Already discovered */
    }

    anomaly->status = CARBON_ANOMALY_DISCOVERED;
    anomaly->discovered_by = faction_id;
    anomaly->discovered_turn = mgr->current_turn;

    /* Notify callback */
    if (mgr->discovery_callback) {
        mgr->discovery_callback(mgr, anomaly, faction_id, mgr->discovery_userdata);
    }

    return true;
}

bool carbon_anomaly_start_research(Carbon_AnomalyManager *mgr, uint32_t id,
                                    int32_t faction_id, uint32_t researcher) {
    if (!mgr) return false;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) return false;

    /* Must be discovered first */
    if (anomaly->status == CARBON_ANOMALY_UNDISCOVERED) {
        return false;
    }

    /* Can't research if already researching or completed (unless repeatable) */
    if (anomaly->status == CARBON_ANOMALY_RESEARCHING) {
        return false;
    }

    if (anomaly->status == CARBON_ANOMALY_COMPLETED ||
        anomaly->status == CARBON_ANOMALY_DEPLETED) {
        /* Check if repeatable */
        const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
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
    const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (type && type->min_researchers > 0 && researcher == 0) {
        return false;  /* Needs a researcher entity */
    }

    /* Start research */
    anomaly->status = CARBON_ANOMALY_RESEARCHING;
    anomaly->researching_faction = faction_id;
    anomaly->researcher_entity = researcher;
    anomaly->research_started_turn = mgr->current_turn;

    /* Reset progress if repeatable */
    if (anomaly->times_completed > 0) {
        anomaly->progress = 0.0f;
    }

    return true;
}

void carbon_anomaly_stop_research(Carbon_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly || anomaly->status != CARBON_ANOMALY_RESEARCHING) {
        return;
    }

    /* Revert to discovered state (keep progress) */
    anomaly->status = CARBON_ANOMALY_DISCOVERED;
    anomaly->researching_faction = -1;
    anomaly->researcher_entity = 0;
}

bool carbon_anomaly_add_progress(Carbon_AnomalyManager *mgr, uint32_t id, float amount) {
    if (!mgr) return false;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly || anomaly->status != CARBON_ANOMALY_RESEARCHING) {
        return false;
    }

    const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
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
        anomaly->status = CARBON_ANOMALY_COMPLETED;
        anomaly->completed_turn = mgr->current_turn;
        anomaly->times_completed++;

        /* Generate result and notify */
        Carbon_AnomalyResult result = build_result(mgr, anomaly, true);

        if (mgr->reward_callback) {
            mgr->reward_callback(mgr, anomaly, &result, mgr->reward_userdata);
        }

        return true;
    }

    return false;
}

void carbon_anomaly_set_progress(Carbon_AnomalyManager *mgr, uint32_t id, float progress) {
    if (!mgr) return;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) return;

    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    anomaly->progress = progress;
}

float carbon_anomaly_get_progress(const Carbon_AnomalyManager *mgr, uint32_t id) {
    const Carbon_Anomaly *anomaly = carbon_anomaly_get(mgr, id);
    return anomaly ? anomaly->progress : 0.0f;
}

bool carbon_anomaly_is_complete(const Carbon_AnomalyManager *mgr, uint32_t id) {
    const Carbon_Anomaly *anomaly = carbon_anomaly_get(mgr, id);
    return anomaly && (anomaly->status == CARBON_ANOMALY_COMPLETED ||
                       anomaly->status == CARBON_ANOMALY_DEPLETED);
}

Carbon_AnomalyResult carbon_anomaly_complete_instant(Carbon_AnomalyManager *mgr, uint32_t id) {
    Carbon_AnomalyResult result;
    memset(&result, 0, sizeof(result));

    if (!mgr) {
        strcpy(result.message, "Invalid manager");
        return result;
    }

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) {
        strcpy(result.message, "Anomaly not found");
        return result;
    }

    /* Must be at least discovered */
    if (anomaly->status == CARBON_ANOMALY_UNDISCOVERED) {
        strcpy(result.message, "Anomaly not discovered");
        return result;
    }

    /* Complete */
    anomaly->progress = 1.0f;
    anomaly->status = CARBON_ANOMALY_COMPLETED;
    anomaly->completed_turn = mgr->current_turn;
    anomaly->times_completed++;

    result = build_result(mgr, anomaly, true);

    if (mgr->reward_callback) {
        mgr->reward_callback(mgr, anomaly, &result, mgr->reward_userdata);
    }

    return result;
}

Carbon_AnomalyResult carbon_anomaly_collect_rewards(Carbon_AnomalyManager *mgr, uint32_t id) {
    Carbon_AnomalyResult result;
    memset(&result, 0, sizeof(result));

    if (!mgr) {
        strcpy(result.message, "Invalid manager");
        return result;
    }

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (!anomaly) {
        strcpy(result.message, "Anomaly not found");
        return result;
    }

    if (anomaly->status != CARBON_ANOMALY_COMPLETED) {
        strcpy(result.message, "Anomaly not completed");
        return result;
    }

    result = build_result(mgr, anomaly, true);

    /* Check if repeatable */
    const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (type && type->repeatable) {
        /* Reset to discovered for re-research */
        anomaly->status = CARBON_ANOMALY_DISCOVERED;
        anomaly->progress = 0.0f;
        anomaly->researching_faction = -1;
        anomaly->researcher_entity = 0;
    } else {
        /* Mark as depleted */
        anomaly->status = CARBON_ANOMALY_DEPLETED;
    }

    return result;
}

void carbon_anomaly_deplete(Carbon_AnomalyManager *mgr, uint32_t id) {
    if (!mgr) return;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (anomaly) {
        anomaly->status = CARBON_ANOMALY_DEPLETED;
    }
}

/*============================================================================
 * Research Speed
 *============================================================================*/

void carbon_anomaly_set_research_speed(Carbon_AnomalyManager *mgr, uint32_t id, float speed) {
    if (!mgr) return;

    Carbon_Anomaly *anomaly = find_anomaly(mgr, id);
    if (anomaly) {
        if (speed < 0.0f) speed = 0.0f;
        anomaly->research_speed = speed;
    }
}

float carbon_anomaly_get_remaining_time(const Carbon_AnomalyManager *mgr, uint32_t id) {
    const Carbon_Anomaly *anomaly = carbon_anomaly_get(mgr, id);
    if (!anomaly) return 0.0f;

    const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type || type->research_time <= 0) return 0.0f;

    float remaining_progress = 1.0f - anomaly->progress;
    float effective_speed = anomaly->research_speed * type->research_multiplier;
    if (effective_speed <= 0.0f) return INFINITY;

    return remaining_progress * type->research_time / effective_speed;
}

float carbon_anomaly_get_total_time(const Carbon_AnomalyManager *mgr, uint32_t id) {
    const Carbon_Anomaly *anomaly = carbon_anomaly_get(mgr, id);
    if (!anomaly) return 0.0f;

    const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                  anomaly->type_id);
    if (!type) return 0.0f;

    float effective_speed = anomaly->research_speed * type->research_multiplier;
    if (effective_speed <= 0.0f) return INFINITY;

    return type->research_time / effective_speed;
}

/*============================================================================
 * Queries
 *============================================================================*/

int carbon_anomaly_get_at(const Carbon_AnomalyManager *mgr, int32_t x, int32_t y,
                           uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->x == x && a->y == y) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int carbon_anomaly_get_by_status(const Carbon_AnomalyManager *mgr, Carbon_AnomalyStatus status,
                                  uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->status == status) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int carbon_anomaly_get_by_type(const Carbon_AnomalyManager *mgr, int type_id,
                                uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->type_id == type_id) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int carbon_anomaly_get_by_faction(const Carbon_AnomalyManager *mgr, int32_t faction_id,
                                   uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->discovered_by == faction_id) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int carbon_anomaly_get_in_rect(const Carbon_AnomalyManager *mgr,
                                int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    /* Normalize rectangle */
    if (x1 > x2) { int32_t t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int32_t t = y1; y1 = y2; y2 = t; }

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->x >= x1 && a->x <= x2 && a->y >= y1 && a->y <= y2) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int carbon_anomaly_get_in_radius(const Carbon_AnomalyManager *mgr,
                                  int32_t center_x, int32_t center_y, int32_t radius,
                                  uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int32_t radius_sq = radius * radius;
    int count = 0;

    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (!a->active) continue;

        int32_t dist_sq = distance_squared(a->x, a->y, center_x, center_y);
        if (dist_sq <= radius_sq) {
            out_ids[count++] = a->id;
        }
    }
    return count;
}

int carbon_anomaly_get_all(const Carbon_AnomalyManager *mgr, uint32_t *out_ids, int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES && count < max; i++) {
        if (mgr->anomalies[i].active) {
            out_ids[count++] = mgr->anomalies[i].id;
        }
    }
    return count;
}

bool carbon_anomaly_has_at(const Carbon_AnomalyManager *mgr, int32_t x, int32_t y) {
    if (!mgr) return false;

    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->x == x && a->y == y) {
            return true;
        }
    }
    return false;
}

uint32_t carbon_anomaly_find_nearest(const Carbon_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      int32_t max_distance,
                                      int status) {
    if (!mgr) return CARBON_ANOMALY_INVALID;

    int32_t max_dist_sq = (max_distance < 0) ? INT32_MAX : (max_distance * max_distance);
    int32_t best_dist_sq = INT32_MAX;
    uint32_t best_id = CARBON_ANOMALY_INVALID;

    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (!a->active) continue;

        /* Status filter */
        if (status >= 0 && a->status != (Carbon_AnomalyStatus)status) {
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

bool carbon_anomaly_can_research(const Carbon_AnomalyManager *mgr, uint32_t id,
                                  int32_t faction_id) {
    if (!mgr) return false;

    const Carbon_Anomaly *anomaly = carbon_anomaly_get(mgr, id);
    if (!anomaly) return false;

    /* Must be discovered */
    if (anomaly->status == CARBON_ANOMALY_UNDISCOVERED) {
        return false;
    }

    /* Can't research if already in progress by another faction */
    if (anomaly->status == CARBON_ANOMALY_RESEARCHING &&
        anomaly->researching_faction != faction_id) {
        return false;
    }

    /* Check if completed and not repeatable */
    if (anomaly->status == CARBON_ANOMALY_COMPLETED ||
        anomaly->status == CARBON_ANOMALY_DEPLETED) {
        const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                      anomaly->type_id);
        if (!type || !type->repeatable) {
            return false;
        }
    }

    /* Custom validator */
    if (mgr->can_research_callback) {
        return mgr->can_research_callback(mgr, anomaly, faction_id,
                                           ((Carbon_AnomalyManager *)mgr)->can_research_userdata);
    }

    return true;
}

bool carbon_anomaly_can_spawn_at(const Carbon_AnomalyManager *mgr, int32_t x, int32_t y) {
    /* By default, can spawn anywhere that doesn't already have an anomaly */
    return !carbon_anomaly_has_at(mgr, x, y);
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_anomaly_set_reward_callback(Carbon_AnomalyManager *mgr,
                                         Carbon_AnomalyRewardFunc callback,
                                         void *userdata) {
    if (mgr) {
        mgr->reward_callback = callback;
        mgr->reward_userdata = userdata;
    }
}

void carbon_anomaly_set_discovery_callback(Carbon_AnomalyManager *mgr,
                                            Carbon_AnomalyDiscoveryFunc callback,
                                            void *userdata) {
    if (mgr) {
        mgr->discovery_callback = callback;
        mgr->discovery_userdata = userdata;
    }
}

void carbon_anomaly_set_spawn_callback(Carbon_AnomalyManager *mgr,
                                        Carbon_AnomalySpawnFunc callback,
                                        void *userdata) {
    if (mgr) {
        mgr->spawn_callback = callback;
        mgr->spawn_userdata = userdata;
    }
}

void carbon_anomaly_set_can_research_callback(Carbon_AnomalyManager *mgr,
                                               Carbon_AnomalyCanResearchFunc callback,
                                               void *userdata) {
    if (mgr) {
        mgr->can_research_callback = callback;
        mgr->can_research_userdata = userdata;
    }
}

/*============================================================================
 * Statistics
 *============================================================================*/

void carbon_anomaly_get_stats(const Carbon_AnomalyManager *mgr, Carbon_AnomalyStats *out_stats) {
    if (!out_stats) return;

    memset(out_stats, 0, sizeof(Carbon_AnomalyStats));

    if (!mgr) return;

    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        const Carbon_Anomaly *a = &mgr->anomalies[i];
        if (!a->active) continue;

        out_stats->total_count++;

        switch (a->status) {
            case CARBON_ANOMALY_UNDISCOVERED:
                out_stats->undiscovered_count++;
                break;
            case CARBON_ANOMALY_DISCOVERED:
                out_stats->discovered_count++;
                break;
            case CARBON_ANOMALY_RESEARCHING:
                out_stats->researching_count++;
                break;
            case CARBON_ANOMALY_COMPLETED:
                out_stats->completed_count++;
                break;
            case CARBON_ANOMALY_DEPLETED:
                out_stats->depleted_count++;
                break;
        }

        /* Count by rarity */
        const Carbon_AnomalyTypeDef *type = carbon_anomaly_get_type(mgr->registry,
                                                                      a->type_id);
        if (type && type->rarity < CARBON_ANOMALY_RARITY_COUNT) {
            out_stats->by_rarity[type->rarity]++;
        }
    }
}

int carbon_anomaly_count(const Carbon_AnomalyManager *mgr) {
    if (!mgr) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        if (mgr->anomalies[i].active) count++;
    }
    return count;
}

/*============================================================================
 * Turn Management
 *============================================================================*/

void carbon_anomaly_set_turn(Carbon_AnomalyManager *mgr, int32_t turn) {
    if (mgr) {
        mgr->current_turn = turn;
    }
}

void carbon_anomaly_update(Carbon_AnomalyManager *mgr, float delta_time) {
    if (!mgr || delta_time <= 0.0f) return;

    /* Update all researching anomalies */
    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        Carbon_Anomaly *a = &mgr->anomalies[i];
        if (a->active && a->status == CARBON_ANOMALY_RESEARCHING) {
            carbon_anomaly_add_progress(mgr, a->id, delta_time);
        }
    }
}

void carbon_anomaly_clear(Carbon_AnomalyManager *mgr) {
    if (!mgr) return;

    for (int i = 0; i < CARBON_ANOMALY_MAX_INSTANCES; i++) {
        mgr->anomalies[i].active = false;
    }
}

/*============================================================================
 * Random Generation
 *============================================================================*/

void carbon_anomaly_set_seed(Carbon_AnomalyManager *mgr, uint32_t seed) {
    if (mgr) {
        mgr->random_state = seed ? seed : (uint32_t)time(NULL);
    }
}

void carbon_anomaly_set_rarity_weights(Carbon_AnomalyManager *mgr, const float *weights) {
    if (mgr && weights) {
        for (int i = 0; i < CARBON_ANOMALY_RARITY_COUNT; i++) {
            mgr->rarity_weights[i] = weights[i];
        }
    }
}

void carbon_anomaly_get_default_weights(float *out_weights) {
    if (!out_weights) return;

    /* Default distribution: 60%, 25%, 12%, 3% */
    out_weights[CARBON_ANOMALY_COMMON] = 0.60f;
    out_weights[CARBON_ANOMALY_UNCOMMON] = 0.25f;
    out_weights[CARBON_ANOMALY_RARE] = 0.12f;
    out_weights[CARBON_ANOMALY_LEGENDARY] = 0.03f;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_anomaly_rarity_name(Carbon_AnomalyRarity rarity) {
    switch (rarity) {
        case CARBON_ANOMALY_COMMON:    return "Common";
        case CARBON_ANOMALY_UNCOMMON:  return "Uncommon";
        case CARBON_ANOMALY_RARE:      return "Rare";
        case CARBON_ANOMALY_LEGENDARY: return "Legendary";
        default:                       return "Unknown";
    }
}

const char *carbon_anomaly_status_name(Carbon_AnomalyStatus status) {
    switch (status) {
        case CARBON_ANOMALY_UNDISCOVERED: return "Undiscovered";
        case CARBON_ANOMALY_DISCOVERED:   return "Discovered";
        case CARBON_ANOMALY_RESEARCHING:  return "Researching";
        case CARBON_ANOMALY_COMPLETED:    return "Completed";
        case CARBON_ANOMALY_DEPLETED:     return "Depleted";
        default:                          return "Unknown";
    }
}

const char *carbon_anomaly_reward_type_name(Carbon_AnomalyRewardType type) {
    switch (type) {
        case CARBON_ANOMALY_REWARD_NONE:      return "None";
        case CARBON_ANOMALY_REWARD_RESOURCES: return "Resources";
        case CARBON_ANOMALY_REWARD_TECH:      return "Technology";
        case CARBON_ANOMALY_REWARD_UNIT:      return "Unit";
        case CARBON_ANOMALY_REWARD_MODIFIER:  return "Modifier";
        case CARBON_ANOMALY_REWARD_ARTIFACT:  return "Artifact";
        case CARBON_ANOMALY_REWARD_MAP:       return "Map";
        case CARBON_ANOMALY_REWARD_CUSTOM:    return "Custom";
        default:                              return "Unknown";
    }
}
