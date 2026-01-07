/*
 * Carbon Game Engine - Siege/Bombardment System Implementation
 *
 * Sustained attack mechanics over multiple rounds for location assault.
 */

#include "agentite/agentite.h"
#include "agentite/siege.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Agentite_SiegeManager {
    /* Siege instances */
    Agentite_Siege sieges[AGENTITE_SIEGE_MAX_INSTANCES];
    uint32_t next_id;

    /* Configuration */
    Agentite_SiegeConfig config;

    /* Callbacks */
    Agentite_SiegeDefenseFunc defense_callback;
    void *defense_userdata;

    Agentite_SiegeDefenderFunc defender_callback;
    void *defender_userdata;

    Agentite_SiegeDamageFunc damage_callback;
    void *damage_userdata;

    Agentite_SiegeEventFunc event_callback;
    void *event_userdata;

    Agentite_SiegeCanBeginFunc can_begin_callback;
    void *can_begin_userdata;

    Agentite_SiegeBuildingsFunc buildings_callback;
    void *buildings_userdata;

    /* Event dispatcher (optional) */
    Agentite_EventDispatcher *events;

    /* Turn tracking */
    int32_t current_turn;

    /* Statistics */
    Agentite_SiegeStats stats;
};

/*============================================================================
 * Static Helper Functions
 *============================================================================*/

static Agentite_Siege *find_siege_by_id(Agentite_SiegeManager *mgr, uint32_t id) {
    if (!mgr || id == AGENTITE_SIEGE_INVALID) return NULL;

    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES; i++) {
        if (mgr->sieges[i].active && mgr->sieges[i].id == id) {
            return &mgr->sieges[i];
        }
    }
    return NULL;
}

static const Agentite_Siege *find_siege_by_id_const(const Agentite_SiegeManager *mgr, uint32_t id) {
    if (!mgr || id == AGENTITE_SIEGE_INVALID) return NULL;

    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES; i++) {
        if (mgr->sieges[i].active && mgr->sieges[i].id == id) {
            return &mgr->sieges[i];
        }
    }
    return NULL;
}

static Agentite_Siege *find_free_slot(Agentite_SiegeManager *mgr) {
    if (!mgr) return NULL;

    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES; i++) {
        if (!mgr->sieges[i].active) {
            return &mgr->sieges[i];
        }
    }
    return NULL;
}

static void emit_event(Agentite_SiegeManager *mgr, uint32_t siege_id,
                       Agentite_SiegeEvent event, const Agentite_SiegeRoundResult *result) {
    if (!mgr || !mgr->event_callback) return;
    mgr->event_callback(mgr, siege_id, event, result, mgr->event_userdata);
}

static int32_t calculate_base_damage(const Agentite_SiegeManager *mgr, const Agentite_Siege *siege) {
    if (!mgr || !siege) return 0;

    /* Use custom damage callback if provided */
    if (mgr->damage_callback) {
        return mgr->damage_callback(siege, mgr->damage_userdata);
    }

    /* Default damage calculation based on force ratio and config */
    float force_ratio = 1.0f;
    if (siege->current_defense_force > 0) {
        force_ratio = (float)siege->current_attack_force / (float)siege->current_defense_force;
    } else {
        force_ratio = 10.0f; /* Overwhelming if no defenders */
    }

    /* Base damage scaled by force ratio and modifiers */
    float damage = (float)mgr->config.base_damage_per_round * force_ratio;
    damage *= siege->attack_modifier;
    damage *= siege->damage_modifier;

    /* Reduce by defense modifier */
    damage /= siege->defense_modifier;

    return (int32_t)fmaxf(1.0f, damage);
}

static void apply_attrition(Agentite_SiegeManager *mgr, Agentite_Siege *siege,
                            Agentite_SiegeRoundResult *result) {
    if (!mgr || !siege || !result) return;

    /* Calculate attacker attrition */
    float attacker_loss_rate = mgr->config.attacker_attrition_rate;
    if (siege->current_defense_force > 0) {
        /* More losses if defenders are stronger */
        float ratio = (float)siege->current_defense_force / (float)siege->current_attack_force;
        attacker_loss_rate *= (1.0f + ratio * 0.5f);
    }
    int32_t attacker_losses = (int32_t)(siege->current_attack_force * attacker_loss_rate);
    attacker_losses = attacker_losses > 0 ? attacker_losses : 0;

    /* Calculate defender attrition */
    float defender_loss_rate = mgr->config.defender_attrition_rate;
    if (siege->current_attack_force > 0) {
        float ratio = (float)siege->current_attack_force / (float)siege->current_defense_force;
        defender_loss_rate *= (1.0f + ratio * 0.5f);
    }
    int32_t defender_losses = (int32_t)(siege->current_defense_force * defender_loss_rate);
    defender_losses = defender_losses > 0 ? defender_losses : 0;

    /* Apply losses */
    siege->current_attack_force -= attacker_losses;
    siege->current_defense_force -= defender_losses;

    if (siege->current_attack_force < 0) siege->current_attack_force = 0;
    if (siege->current_defense_force < 0) siege->current_defense_force = 0;

    /* Update result */
    result->attacker_casualties = attacker_losses;
    result->defender_casualties = defender_losses;

    /* Update siege totals */
    siege->total_attacker_casualties += attacker_losses;
    siege->total_defender_casualties += defender_losses;
}

static void apply_building_damage(Agentite_SiegeManager *mgr, Agentite_Siege *siege,
                                  int32_t damage, Agentite_SiegeRoundResult *result) {
    if (!mgr || !siege || !result || siege->building_count == 0) return;

    result->buildings_damaged = 0;
    result->buildings_destroyed = 0;

    /* Distribute damage across buildings randomly */
    int remaining_damage = damage;
    int attempts = 0;
    int max_attempts = siege->building_count * 3;

    while (remaining_damage > 0 && attempts < max_attempts) {
        attempts++;

        /* Pick a random non-destroyed building */
        int start = rand() % siege->building_count;
        int found = -1;

        for (int i = 0; i < siege->building_count; i++) {
            int idx = (start + i) % siege->building_count;
            if (!siege->buildings[idx].destroyed) {
                found = idx;
                break;
            }
        }

        if (found < 0) break; /* All buildings destroyed */

        /* Check if building takes damage this round */
        float chance = mgr->config.building_damage_chance;
        if ((float)rand() / (float)RAND_MAX > chance) {
            remaining_damage -= 5; /* Move on anyway */
            continue;
        }

        Agentite_SiegeBuilding *bldg = &siege->buildings[found];

        /* Apply damage */
        int bldg_damage = (remaining_damage > 10) ? 10 + rand() % 10 : remaining_damage;
        bldg->current_health -= bldg_damage;
        remaining_damage -= bldg_damage;
        result->buildings_damaged++;

        if (bldg->current_health <= 0) {
            bldg->current_health = 0;
            bldg->destroyed = true;
            result->buildings_destroyed++;
            siege->total_buildings_destroyed++;
            result->defense_reduced += bldg->defense_contribution;

            emit_event(mgr, siege->id, AGENTITE_SIEGE_EVENT_BUILDING_DESTROYED, result);
        } else {
            emit_event(mgr, siege->id, AGENTITE_SIEGE_EVENT_BUILDING_DAMAGED, result);
        }
    }
}

static void apply_population_casualties(Agentite_SiegeManager *mgr, Agentite_Siege *siege,
                                        Agentite_SiegeRoundResult *result) {
    if (!mgr || !siege || !result) return;

    /* Calculate population casualties based on damage and rate */
    float casualty_rate = mgr->config.population_casualty_rate;
    int32_t casualties = (int32_t)(result->damage_dealt * casualty_rate);

    result->population_casualties = casualties;
    siege->total_population_casualties += casualties;
}

static void check_siege_end_conditions(Agentite_SiegeManager *mgr, Agentite_Siege *siege,
                                       Agentite_SiegeRoundResult *result) {
    if (!mgr || !siege || !result) return;

    result->siege_ended = false;
    result->siege_broken = false;
    result->target_captured = false;

    /* Check for capture */
    if (siege->capture_progress >= 1.0f) {
        siege->status = AGENTITE_SIEGE_CAPTURED;
        siege->ended_turn = mgr->current_turn;
        result->siege_ended = true;
        result->target_captured = true;
        result->end_status = AGENTITE_SIEGE_CAPTURED;
        mgr->stats.captured_count++;
        emit_event(mgr, siege->id, AGENTITE_SIEGE_EVENT_CAPTURED, result);
        return;
    }

    /* Check for attacker defeat */
    if (siege->current_attack_force <= 0) {
        siege->status = AGENTITE_SIEGE_BROKEN;
        siege->ended_turn = mgr->current_turn;
        result->siege_ended = true;
        result->siege_broken = true;
        result->end_status = AGENTITE_SIEGE_BROKEN;
        mgr->stats.broken_count++;
        emit_event(mgr, siege->id, AGENTITE_SIEGE_EVENT_BROKEN, result);
        return;
    }

    /* Check for timeout */
    if (siege->current_round >= siege->max_rounds) {
        siege->status = AGENTITE_SIEGE_TIMEOUT;
        siege->ended_turn = mgr->current_turn;
        result->siege_ended = true;
        result->end_status = AGENTITE_SIEGE_TIMEOUT;
        mgr->stats.timeout_count++;
        emit_event(mgr, siege->id, AGENTITE_SIEGE_EVENT_TIMEOUT, result);
        return;
    }
}

/*============================================================================
 * Manager Lifecycle
 *============================================================================*/

Agentite_SiegeManager *agentite_siege_create(void) {
    Agentite_SiegeManager *mgr = AGENTITE_ALLOC(Agentite_SiegeManager);
    if (!mgr) {
        agentite_set_error("Failed to allocate siege manager");
        return NULL;
    }

    mgr->next_id = 1;
    mgr->config = agentite_siege_default_config();
    mgr->current_turn = 0;

    return mgr;
}

Agentite_SiegeManager *agentite_siege_create_with_events(Agentite_EventDispatcher *events) {
    Agentite_SiegeManager *mgr = agentite_siege_create();
    if (mgr) {
        mgr->events = events;
    }
    return mgr;
}

void agentite_siege_destroy(Agentite_SiegeManager *mgr) {
    if (!mgr) return;
    free(mgr);
}

/*============================================================================
 * Configuration
 *============================================================================*/

Agentite_SiegeConfig agentite_siege_default_config(void) {
    Agentite_SiegeConfig config = {
        .default_max_rounds = AGENTITE_SIEGE_DEFAULT_MAX_ROUNDS,
        .min_force_ratio = AGENTITE_SIEGE_DEFAULT_MIN_FORCE_RATIO,
        .base_damage_per_round = AGENTITE_SIEGE_DEFAULT_DAMAGE_PER_ROUND,
        .capture_threshold = AGENTITE_SIEGE_DEFAULT_CAPTURE_THRESHOLD,
        .building_damage_chance = 0.3f,
        .population_casualty_rate = 0.01f,
        .attacker_attrition_rate = 0.02f,
        .defender_attrition_rate = 0.01f,
        .allow_retreat = true,
        .destroy_on_capture = false
    };
    return config;
}

void agentite_siege_set_config(Agentite_SiegeManager *mgr, const Agentite_SiegeConfig *config) {
    if (!mgr || !config) return;
    mgr->config = *config;
}

const Agentite_SiegeConfig *agentite_siege_get_config(const Agentite_SiegeManager *mgr) {
    if (!mgr) return NULL;
    return &mgr->config;
}

void agentite_siege_set_max_rounds(Agentite_SiegeManager *mgr, int32_t max_rounds) {
    if (!mgr || max_rounds < 1) return;
    mgr->config.default_max_rounds = max_rounds;
}

void agentite_siege_set_min_force_ratio(Agentite_SiegeManager *mgr, float ratio) {
    if (!mgr || ratio < 0.0f) return;
    mgr->config.min_force_ratio = ratio;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_siege_set_defense_callback(Agentite_SiegeManager *mgr,
                                       Agentite_SiegeDefenseFunc callback,
                                       void *userdata) {
    if (!mgr) return;
    mgr->defense_callback = callback;
    mgr->defense_userdata = userdata;
}

void agentite_siege_set_defender_callback(Agentite_SiegeManager *mgr,
                                        Agentite_SiegeDefenderFunc callback,
                                        void *userdata) {
    if (!mgr) return;
    mgr->defender_callback = callback;
    mgr->defender_userdata = userdata;
}

void agentite_siege_set_damage_callback(Agentite_SiegeManager *mgr,
                                      Agentite_SiegeDamageFunc callback,
                                      void *userdata) {
    if (!mgr) return;
    mgr->damage_callback = callback;
    mgr->damage_userdata = userdata;
}

void agentite_siege_set_event_callback(Agentite_SiegeManager *mgr,
                                     Agentite_SiegeEventFunc callback,
                                     void *userdata) {
    if (!mgr) return;
    mgr->event_callback = callback;
    mgr->event_userdata = userdata;
}

void agentite_siege_set_can_begin_callback(Agentite_SiegeManager *mgr,
                                         Agentite_SiegeCanBeginFunc callback,
                                         void *userdata) {
    if (!mgr) return;
    mgr->can_begin_callback = callback;
    mgr->can_begin_userdata = userdata;
}

void agentite_siege_set_buildings_callback(Agentite_SiegeManager *mgr,
                                         Agentite_SiegeBuildingsFunc callback,
                                         void *userdata) {
    if (!mgr) return;
    mgr->buildings_callback = callback;
    mgr->buildings_userdata = userdata;
}

/*============================================================================
 * Siege Lifecycle
 *============================================================================*/

bool agentite_siege_can_begin(Agentite_SiegeManager *mgr,
                            uint32_t attacker_faction,
                            uint32_t target_location,
                            int32_t attacking_force) {
    if (!mgr) return false;
    if (attacking_force <= 0) return false;

    /* Check if location is already under siege */
    if (agentite_siege_has_siege_at(mgr, target_location)) {
        return false;
    }

    /* Get defense value */
    int32_t defense = 0;
    if (mgr->defense_callback) {
        defense = mgr->defense_callback(target_location, mgr->defense_userdata);
    }

    /* Check force ratio */
    if (defense > 0) {
        float ratio = (float)attacking_force / (float)defense;
        if (ratio < mgr->config.min_force_ratio) {
            return false;
        }
    }

    /* Custom validation callback */
    if (mgr->can_begin_callback) {
        return mgr->can_begin_callback(attacker_faction, target_location,
                                       attacking_force, mgr->can_begin_userdata);
    }

    return true;
}

uint32_t agentite_siege_begin(Agentite_SiegeManager *mgr,
                            uint32_t attacker_faction,
                            uint32_t target_location,
                            int32_t attacking_force) {
    return agentite_siege_begin_ex(mgr, attacker_faction, target_location,
                                 attacking_force,
                                 mgr ? mgr->config.default_max_rounds : AGENTITE_SIEGE_DEFAULT_MAX_ROUNDS,
                                 0);
}

uint32_t agentite_siege_begin_ex(Agentite_SiegeManager *mgr,
                               uint32_t attacker_faction,
                               uint32_t target_location,
                               int32_t attacking_force,
                               int32_t max_rounds,
                               uint32_t metadata) {
    if (!mgr) {
        agentite_set_error("Siege manager is NULL");
        return AGENTITE_SIEGE_INVALID;
    }

    if (!agentite_siege_can_begin(mgr, attacker_faction, target_location, attacking_force)) {
        agentite_set_error("Siege: Cannot begin at location %d (faction %d, force %d)", target_location, attacker_faction, attacking_force);
        return AGENTITE_SIEGE_INVALID;
    }

    Agentite_Siege *siege = find_free_slot(mgr);
    if (!siege) {
        agentite_set_error("Siege: Maximum active sieges reached (limit: %d)", AGENTITE_SIEGE_MAX_INSTANCES);
        return AGENTITE_SIEGE_INVALID;
    }

    /* Initialize siege */
    memset(siege, 0, sizeof(Agentite_Siege));
    siege->id = mgr->next_id++;
    siege->active = true;

    siege->attacker_faction = attacker_faction;
    siege->target_location = target_location;

    /* Get defender faction */
    if (mgr->defender_callback) {
        siege->defender_faction = mgr->defender_callback(target_location, mgr->defender_userdata);
    }

    /* Set forces */
    siege->initial_attack_force = attacking_force;
    siege->current_attack_force = attacking_force;

    if (mgr->defense_callback) {
        siege->initial_defense_force = mgr->defense_callback(target_location, mgr->defense_userdata);
    }
    siege->current_defense_force = siege->initial_defense_force;

    /* Initialize progress */
    siege->status = AGENTITE_SIEGE_ACTIVE;
    siege->current_round = 0;
    siege->max_rounds = max_rounds;
    siege->capture_progress = 0.0f;

    /* Default modifiers */
    siege->attack_modifier = 1.0f;
    siege->defense_modifier = 1.0f;
    siege->damage_modifier = 1.0f;

    /* Timing */
    siege->started_turn = mgr->current_turn;
    siege->ended_turn = -1;

    siege->metadata = metadata;

    /* Populate buildings if callback provided */
    if (mgr->buildings_callback) {
        siege->building_count = mgr->buildings_callback(target_location,
                                                        siege->buildings,
                                                        AGENTITE_SIEGE_MAX_BUILDINGS,
                                                        mgr->buildings_userdata);
    }

    /* Update statistics */
    mgr->stats.total_sieges++;
    mgr->stats.active_sieges++;

    /* Emit start event */
    emit_event(mgr, siege->id, AGENTITE_SIEGE_EVENT_STARTED, NULL);

    return siege->id;
}

bool agentite_siege_process_round(Agentite_SiegeManager *mgr,
                                uint32_t siege_id,
                                Agentite_SiegeRoundResult *out_result) {
    if (!mgr || !out_result) return false;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) {
        return false;
    }

    /* Initialize result */
    memset(out_result, 0, sizeof(Agentite_SiegeRoundResult));

    /* Advance round */
    siege->current_round++;
    out_result->round_number = siege->current_round;

    /* Calculate and apply damage */
    int32_t damage = calculate_base_damage(mgr, siege);
    out_result->damage_dealt = damage;
    siege->total_damage_dealt += damage;

    /* Apply damage to buildings */
    apply_building_damage(mgr, siege, damage, out_result);

    /* Apply attrition to forces */
    apply_attrition(mgr, siege, out_result);

    /* Apply population casualties */
    apply_population_casualties(mgr, siege, out_result);

    /* Calculate capture progress */
    if (siege->initial_defense_force > 0) {
        float defense_remaining = (float)siege->current_defense_force / (float)siege->initial_defense_force;
        siege->capture_progress = 1.0f - defense_remaining;

        /* Account for building destruction */
        int total_buildings = siege->building_count;
        if (total_buildings > 0) {
            float buildings_remaining = (float)(total_buildings - siege->total_buildings_destroyed) / (float)total_buildings;
            siege->capture_progress = (siege->capture_progress + (1.0f - buildings_remaining)) / 2.0f;
        }

        /* Check capture threshold */
        if (defense_remaining <= mgr->config.capture_threshold) {
            siege->capture_progress = 1.0f;
        }
    } else {
        siege->capture_progress = 1.0f; /* No defenders = instant capture */
    }

    out_result->capture_progress = siege->capture_progress;

    /* Check end conditions */
    check_siege_end_conditions(mgr, siege, out_result);

    if (out_result->siege_ended) {
        siege->active = false;
        mgr->stats.active_sieges--;
    }

    /* Update statistics */
    mgr->stats.total_rounds_processed++;
    mgr->stats.total_buildings_destroyed += out_result->buildings_destroyed;
    mgr->stats.total_casualties += out_result->attacker_casualties +
                                   out_result->defender_casualties +
                                   out_result->population_casualties;

    /* Emit round processed event */
    emit_event(mgr, siege_id, AGENTITE_SIEGE_EVENT_ROUND_PROCESSED, out_result);

    return true;
}

void agentite_siege_retreat(Agentite_SiegeManager *mgr, uint32_t siege_id) {
    agentite_siege_end(mgr, siege_id, AGENTITE_SIEGE_RETREATED);
}

void agentite_siege_end(Agentite_SiegeManager *mgr, uint32_t siege_id,
                      Agentite_SiegeStatus end_status) {
    if (!mgr) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) return;

    siege->status = end_status;
    siege->ended_turn = mgr->current_turn;
    siege->active = false;
    mgr->stats.active_sieges--;

    /* Update stats based on end status */
    switch (end_status) {
        case AGENTITE_SIEGE_CAPTURED:
            mgr->stats.captured_count++;
            break;
        case AGENTITE_SIEGE_BROKEN:
            mgr->stats.broken_count++;
            break;
        case AGENTITE_SIEGE_RETREATED:
            mgr->stats.retreated_count++;
            break;
        case AGENTITE_SIEGE_TIMEOUT:
            mgr->stats.timeout_count++;
            break;
        default:
            break;
    }

    /* Emit appropriate event */
    Agentite_SiegeEvent event = AGENTITE_SIEGE_EVENT_RETREATED;
    switch (end_status) {
        case AGENTITE_SIEGE_CAPTURED:
            event = AGENTITE_SIEGE_EVENT_CAPTURED;
            break;
        case AGENTITE_SIEGE_BROKEN:
            event = AGENTITE_SIEGE_EVENT_BROKEN;
            break;
        case AGENTITE_SIEGE_TIMEOUT:
            event = AGENTITE_SIEGE_EVENT_TIMEOUT;
            break;
        default:
            break;
    }

    Agentite_SiegeRoundResult result = {0};
    result.siege_ended = true;
    result.end_status = end_status;
    emit_event(mgr, siege_id, event, &result);
}

/*============================================================================
 * Force Modification
 *============================================================================*/

void agentite_siege_reinforce_attacker(Agentite_SiegeManager *mgr,
                                     uint32_t siege_id,
                                     int32_t additional_force) {
    if (!mgr || additional_force <= 0) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) return;

    siege->current_attack_force += additional_force;
}

void agentite_siege_reinforce_defender(Agentite_SiegeManager *mgr,
                                     uint32_t siege_id,
                                     int32_t additional_force) {
    if (!mgr || additional_force <= 0) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) return;

    siege->current_defense_force += additional_force;
}

void agentite_siege_attacker_casualties(Agentite_SiegeManager *mgr,
                                      uint32_t siege_id,
                                      int32_t casualties) {
    if (!mgr || casualties <= 0) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) return;

    siege->current_attack_force -= casualties;
    if (siege->current_attack_force < 0) siege->current_attack_force = 0;
    siege->total_attacker_casualties += casualties;
}

void agentite_siege_defender_casualties(Agentite_SiegeManager *mgr,
                                      uint32_t siege_id,
                                      int32_t casualties) {
    if (!mgr || casualties <= 0) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) return;

    siege->current_defense_force -= casualties;
    if (siege->current_defense_force < 0) siege->current_defense_force = 0;
    siege->total_defender_casualties += casualties;
}

/*============================================================================
 * Modifier Control
 *============================================================================*/

void agentite_siege_set_attack_modifier(Agentite_SiegeManager *mgr,
                                      uint32_t siege_id,
                                      float modifier) {
    if (!mgr || modifier < 0.0f) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege) return;

    siege->attack_modifier = modifier;
}

void agentite_siege_set_defense_modifier(Agentite_SiegeManager *mgr,
                                       uint32_t siege_id,
                                       float modifier) {
    if (!mgr || modifier < 0.0f) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege) return;

    siege->defense_modifier = modifier;
}

void agentite_siege_set_damage_modifier(Agentite_SiegeManager *mgr,
                                      uint32_t siege_id,
                                      float modifier) {
    if (!mgr || modifier < 0.0f) return;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege) return;

    siege->damage_modifier = modifier;
}

/*============================================================================
 * Building Management
 *============================================================================*/

int agentite_siege_add_building(Agentite_SiegeManager *mgr,
                              uint32_t siege_id,
                              uint32_t building_id,
                              int32_t max_health,
                              int32_t defense_contribution) {
    if (!mgr) return -1;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || siege->building_count >= AGENTITE_SIEGE_MAX_BUILDINGS) {
        return -1;
    }

    int idx = siege->building_count;
    siege->buildings[idx].building_id = building_id;
    siege->buildings[idx].max_health = max_health;
    siege->buildings[idx].current_health = max_health;
    siege->buildings[idx].defense_contribution = defense_contribution;
    siege->buildings[idx].destroyed = false;
    siege->building_count++;

    return idx;
}

bool agentite_siege_damage_building(Agentite_SiegeManager *mgr,
                                  uint32_t siege_id,
                                  int building_index,
                                  int32_t damage) {
    if (!mgr || damage <= 0) return false;

    Agentite_Siege *siege = find_siege_by_id(mgr, siege_id);
    if (!siege || building_index < 0 || building_index >= siege->building_count) {
        return false;
    }

    Agentite_SiegeBuilding *bldg = &siege->buildings[building_index];
    if (bldg->destroyed) return false;

    bldg->current_health -= damage;

    if (bldg->current_health <= 0) {
        bldg->current_health = 0;
        bldg->destroyed = true;
        siege->total_buildings_destroyed++;

        Agentite_SiegeRoundResult result = {0};
        result.buildings_destroyed = 1;
        result.defense_reduced = bldg->defense_contribution;
        emit_event(mgr, siege_id, AGENTITE_SIEGE_EVENT_BUILDING_DESTROYED, &result);
    } else {
        Agentite_SiegeRoundResult result = {0};
        result.buildings_damaged = 1;
        emit_event(mgr, siege_id, AGENTITE_SIEGE_EVENT_BUILDING_DAMAGED, &result);
    }

    return true;
}

const Agentite_SiegeBuilding *agentite_siege_get_building(const Agentite_SiegeManager *mgr,
                                                      uint32_t siege_id,
                                                      int building_index) {
    if (!mgr) return NULL;

    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    if (!siege || building_index < 0 || building_index >= siege->building_count) {
        return NULL;
    }

    return &siege->buildings[building_index];
}

Agentite_BuildingDamageLevel agentite_siege_get_building_damage_level(const Agentite_SiegeBuilding *building) {
    if (!building || building->max_health <= 0) {
        return AGENTITE_BUILDING_DESTROYED;
    }

    if (building->destroyed) {
        return AGENTITE_BUILDING_DESTROYED;
    }

    float health_pct = (float)building->current_health / (float)building->max_health;

    if (health_pct >= 0.75f) return AGENTITE_BUILDING_INTACT;
    if (health_pct >= 0.50f) return AGENTITE_BUILDING_LIGHT_DAMAGE;
    if (health_pct >= 0.25f) return AGENTITE_BUILDING_MODERATE_DAMAGE;
    return AGENTITE_BUILDING_HEAVY_DAMAGE;
}

int agentite_siege_get_building_count(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    if (!mgr) return 0;

    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege ? siege->building_count : 0;
}

int agentite_siege_get_destroyed_building_count(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    if (!mgr) return 0;

    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    if (!siege) return 0;

    int count = 0;
    for (int i = 0; i < siege->building_count; i++) {
        if (siege->buildings[i].destroyed) count++;
    }
    return count;
}

/*============================================================================
 * Queries - Single Siege
 *============================================================================*/

const Agentite_Siege *agentite_siege_get(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    return find_siege_by_id_const(mgr, siege_id);
}

Agentite_Siege *agentite_siege_get_mut(Agentite_SiegeManager *mgr, uint32_t siege_id) {
    return find_siege_by_id(mgr, siege_id);
}

bool agentite_siege_is_active(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege && siege->status == AGENTITE_SIEGE_ACTIVE;
}

Agentite_SiegeStatus agentite_siege_get_status(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege ? siege->status : AGENTITE_SIEGE_INACTIVE;
}

int32_t agentite_siege_get_round(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege ? siege->current_round : 0;
}

float agentite_siege_get_progress(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege ? siege->capture_progress : 0.0f;
}

int32_t agentite_siege_get_remaining_rounds(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    if (!siege) return 0;
    return siege->max_rounds - siege->current_round;
}

int32_t agentite_siege_get_attack_force(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege ? siege->current_attack_force : 0;
}

int32_t agentite_siege_get_defense_force(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    return siege ? siege->current_defense_force : 0;
}

float agentite_siege_get_force_ratio(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    if (!siege || siege->current_defense_force <= 0) return 0.0f;
    return (float)siege->current_attack_force / (float)siege->current_defense_force;
}

/*============================================================================
 * Queries - Batch
 *============================================================================*/

int agentite_siege_get_all_active(const Agentite_SiegeManager *mgr,
                                uint32_t *out_ids,
                                int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES && count < max; i++) {
        if (mgr->sieges[i].active && mgr->sieges[i].status == AGENTITE_SIEGE_ACTIVE) {
            out_ids[count++] = mgr->sieges[i].id;
        }
    }
    return count;
}

int agentite_siege_get_by_attacker(const Agentite_SiegeManager *mgr,
                                 uint32_t attacker_faction,
                                 uint32_t *out_ids,
                                 int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES && count < max; i++) {
        if (mgr->sieges[i].active &&
            mgr->sieges[i].attacker_faction == attacker_faction) {
            out_ids[count++] = mgr->sieges[i].id;
        }
    }
    return count;
}

int agentite_siege_get_by_defender(const Agentite_SiegeManager *mgr,
                                 uint32_t defender_faction,
                                 uint32_t *out_ids,
                                 int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES && count < max; i++) {
        if (mgr->sieges[i].active &&
            mgr->sieges[i].defender_faction == defender_faction) {
            out_ids[count++] = mgr->sieges[i].id;
        }
    }
    return count;
}

uint32_t agentite_siege_get_at_location(const Agentite_SiegeManager *mgr,
                                      uint32_t location) {
    if (!mgr) return AGENTITE_SIEGE_INVALID;

    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES; i++) {
        if (mgr->sieges[i].active &&
            mgr->sieges[i].status == AGENTITE_SIEGE_ACTIVE &&
            mgr->sieges[i].target_location == location) {
            return mgr->sieges[i].id;
        }
    }
    return AGENTITE_SIEGE_INVALID;
}

bool agentite_siege_has_siege_at(const Agentite_SiegeManager *mgr, uint32_t location) {
    return agentite_siege_get_at_location(mgr, location) != AGENTITE_SIEGE_INVALID;
}

int agentite_siege_get_by_status(const Agentite_SiegeManager *mgr,
                               Agentite_SiegeStatus status,
                               uint32_t *out_ids,
                               int max) {
    if (!mgr || !out_ids || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_SIEGE_MAX_INSTANCES && count < max; i++) {
        if (mgr->sieges[i].active && mgr->sieges[i].status == status) {
            out_ids[count++] = mgr->sieges[i].id;
        }
    }
    return count;
}

/*============================================================================
 * Statistics
 *============================================================================*/

void agentite_siege_get_stats(const Agentite_SiegeManager *mgr, Agentite_SiegeStats *out_stats) {
    if (!mgr || !out_stats) return;
    *out_stats = mgr->stats;
}

int agentite_siege_count_active(const Agentite_SiegeManager *mgr) {
    return mgr ? mgr->stats.active_sieges : 0;
}

void agentite_siege_reset_stats(Agentite_SiegeManager *mgr) {
    if (!mgr) return;

    /* Preserve active count */
    int active = mgr->stats.active_sieges;
    memset(&mgr->stats, 0, sizeof(Agentite_SiegeStats));
    mgr->stats.active_sieges = active;
}

/*============================================================================
 * Turn Integration
 *============================================================================*/

void agentite_siege_set_turn(Agentite_SiegeManager *mgr, int32_t turn) {
    if (!mgr) return;
    mgr->current_turn = turn;
}

int agentite_siege_process_all(Agentite_SiegeManager *mgr,
                             Agentite_SiegeRoundResult *out_results,
                             int max_results) {
    if (!mgr) return 0;

    uint32_t active_ids[AGENTITE_SIEGE_MAX_INSTANCES];
    int active_count = agentite_siege_get_all_active(mgr, active_ids, AGENTITE_SIEGE_MAX_INSTANCES);

    int processed = 0;
    for (int i = 0; i < active_count; i++) {
        Agentite_SiegeRoundResult result;
        if (agentite_siege_process_round(mgr, active_ids[i], &result)) {
            if (out_results && processed < max_results) {
                out_results[processed] = result;
            }
            processed++;
        }
    }

    return processed;
}

void agentite_siege_update(Agentite_SiegeManager *mgr, float delta_time) {
    (void)delta_time; /* Currently unused - sieges are turn-based */
    if (!mgr) return;
    /* Reserved for future time-based siege mechanics */
}

/*============================================================================
 * Utility
 *============================================================================*/

const char *agentite_siege_status_name(Agentite_SiegeStatus status) {
    switch (status) {
        case AGENTITE_SIEGE_INACTIVE:  return "Inactive";
        case AGENTITE_SIEGE_PREPARING: return "Preparing";
        case AGENTITE_SIEGE_ACTIVE:    return "Active";
        case AGENTITE_SIEGE_CAPTURED:  return "Captured";
        case AGENTITE_SIEGE_BROKEN:    return "Broken";
        case AGENTITE_SIEGE_RETREATED: return "Retreated";
        case AGENTITE_SIEGE_TIMEOUT:   return "Timeout";
        default:                     return "Unknown";
    }
}

const char *agentite_siege_event_name(Agentite_SiegeEvent event) {
    switch (event) {
        case AGENTITE_SIEGE_EVENT_STARTED:           return "Started";
        case AGENTITE_SIEGE_EVENT_ROUND_PROCESSED:   return "Round Processed";
        case AGENTITE_SIEGE_EVENT_BUILDING_DAMAGED:  return "Building Damaged";
        case AGENTITE_SIEGE_EVENT_BUILDING_DESTROYED:return "Building Destroyed";
        case AGENTITE_SIEGE_EVENT_DEFENSE_REDUCED:   return "Defense Reduced";
        case AGENTITE_SIEGE_EVENT_CAPTURED:          return "Captured";
        case AGENTITE_SIEGE_EVENT_BROKEN:            return "Broken";
        case AGENTITE_SIEGE_EVENT_RETREATED:         return "Retreated";
        case AGENTITE_SIEGE_EVENT_TIMEOUT:           return "Timeout";
        default:                                   return "Unknown";
    }
}

const char *agentite_siege_damage_level_name(Agentite_BuildingDamageLevel level) {
    switch (level) {
        case AGENTITE_BUILDING_INTACT:          return "Intact";
        case AGENTITE_BUILDING_LIGHT_DAMAGE:    return "Light Damage";
        case AGENTITE_BUILDING_MODERATE_DAMAGE: return "Moderate Damage";
        case AGENTITE_BUILDING_HEAVY_DAMAGE:    return "Heavy Damage";
        case AGENTITE_BUILDING_DESTROYED:       return "Destroyed";
        default:                              return "Unknown";
    }
}

int agentite_siege_estimate_rounds(const Agentite_SiegeManager *mgr, uint32_t siege_id) {
    if (!mgr) return -1;

    const Agentite_Siege *siege = find_siege_by_id_const(mgr, siege_id);
    if (!siege || siege->status != AGENTITE_SIEGE_ACTIVE) return -1;

    /* Calculate expected rounds to capture based on current rate */
    if (siege->current_round == 0) {
        /* No rounds processed yet - rough estimate */
        float force_ratio = 1.0f;
        if (siege->current_defense_force > 0) {
            force_ratio = (float)siege->current_attack_force / (float)siege->current_defense_force;
        }
        if (force_ratio < 1.0f) return -1; /* Unlikely to succeed */
        return (int)(siege->max_rounds / force_ratio);
    }

    /* Estimate based on current progress rate */
    float progress_per_round = siege->capture_progress / (float)siege->current_round;
    if (progress_per_round <= 0.0f) return -1;

    float remaining_progress = 1.0f - siege->capture_progress;
    int estimated_rounds = (int)(remaining_progress / progress_per_round) + 1;

    if (estimated_rounds > siege->max_rounds - siege->current_round) {
        return -1; /* Won't finish before timeout */
    }

    return estimated_rounds;
}
