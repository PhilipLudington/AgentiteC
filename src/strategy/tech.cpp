#include "agentite/agentite.h"
#include "agentite/tech.h"
#include "agentite/event.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

struct Agentite_TechTree {
    Agentite_TechDef techs[AGENTITE_TECH_MAX];
    int tech_count;

    /* Event integration */
    Agentite_EventDispatcher *events;

    /* Completion callback */
    Agentite_TechCallback on_complete;
    void *callback_userdata;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

static int find_tech_index(const Agentite_TechTree *tree, const char *id) {
    if (!tree || !id) return -1;

    for (int i = 0; i < tree->tech_count; i++) {
        if (strcmp(tree->techs[i].id, id) == 0) {
            return i;
        }
    }
    return -1;
}

static void emit_tech_event(Agentite_TechTree *tree, Agentite_EventType type,
                            int tech_index, const char *tech_name) {
    if (!tree || !tree->events) return;

    Agentite_Event e = { .type = type };
    e.tech.tech_id = (uint32_t)tech_index;
    e.tech.tech_name = tech_name;

    agentite_event_emit(tree->events, &e);
}

static void complete_tech_internal(Agentite_TechTree *tree,
                                    Agentite_TechState *state,
                                    int tech_index) {
    if (!tree || !state || tech_index < 0 || tech_index >= tree->tech_count) {
        return;
    }

    const Agentite_TechDef *def = &tree->techs[tech_index];

    /* Mark as completed */
    state->completed[tech_index] = true;
    state->completed_count++;

    /* Update bitmask for fast lookup (first 64 techs) */
    if (tech_index < 64) {
        state->completed_mask |= (1ULL << tech_index);
    }

    /* Increment repeat count */
    if (state->repeat_count[tech_index] < INT8_MAX) {
        state->repeat_count[tech_index]++;
    }

    /* Emit event */
    emit_tech_event(tree, AGENTITE_EVENT_TECH_RESEARCHED, tech_index, def->name);

    /* Call completion callback */
    if (tree->on_complete) {
        tree->on_complete(def, state, tree->callback_userdata);
    }
}

/*============================================================================
 * Tech Tree Creation/Destruction
 *============================================================================*/

Agentite_TechTree *agentite_tech_create(void) {
    return agentite_tech_create_with_events(NULL);
}

Agentite_TechTree *agentite_tech_create_with_events(Agentite_EventDispatcher *events) {
    Agentite_TechTree *tree = AGENTITE_ALLOC(Agentite_TechTree);
    if (!tree) {
        agentite_set_error("Failed to allocate tech tree");
        return NULL;
    }

    tree->events = events;
    return tree;
}

void agentite_tech_destroy(Agentite_TechTree *tree) {
    free(tree);
}

/*============================================================================
 * Technology Registration
 *============================================================================*/

int agentite_tech_register(Agentite_TechTree *tree, const Agentite_TechDef *def) {
    if (!tree) {
        agentite_set_error("agentite_tech_register: null tree");
        return -1;
    }
    if (!def) {
        agentite_set_error("agentite_tech_register: null definition");
        return -1;
    }
    if (tree->tech_count >= AGENTITE_TECH_MAX) {
        agentite_set_error("agentite_tech_register: maximum technologies reached");
        return -1;
    }
    if (def->id[0] == '\0') {
        agentite_set_error("agentite_tech_register: empty technology ID");
        return -1;
    }

    /* Check for duplicate ID */
    if (find_tech_index(tree, def->id) >= 0) {
        agentite_set_error("agentite_tech_register: duplicate technology ID: %s", def->id);
        return -1;
    }

    int index = tree->tech_count;
    tree->techs[index] = *def;
    tree->tech_count++;

    return index;
}

int agentite_tech_count(const Agentite_TechTree *tree) {
    return tree ? tree->tech_count : 0;
}

const Agentite_TechDef *agentite_tech_get(const Agentite_TechTree *tree, int index) {
    if (!tree || index < 0 || index >= tree->tech_count) {
        return NULL;
    }
    return &tree->techs[index];
}

const Agentite_TechDef *agentite_tech_find(const Agentite_TechTree *tree, const char *id) {
    int index = find_tech_index(tree, id);
    if (index < 0) return NULL;
    return &tree->techs[index];
}

int agentite_tech_find_index(const Agentite_TechTree *tree, const char *id) {
    return find_tech_index(tree, id);
}

/*============================================================================
 * Technology State Management
 *============================================================================*/

void agentite_tech_state_init(Agentite_TechState *state) {
    if (!state) return;
    memset(state, 0, sizeof(Agentite_TechState));
}

void agentite_tech_state_reset(Agentite_TechState *state) {
    agentite_tech_state_init(state);
}

/*============================================================================
 * Research Status Queries
 *============================================================================*/

bool agentite_tech_is_researched(const Agentite_TechTree *tree,
                                const Agentite_TechState *state,
                                const char *id) {
    if (!tree || !state || !id) return false;

    int index = find_tech_index(tree, id);
    if (index < 0) return false;

    /* Fast path for first 64 techs */
    if (index < 64) {
        return (state->completed_mask & (1ULL << index)) != 0;
    }

    return state->completed[index];
}

bool agentite_tech_has_prerequisites(const Agentite_TechTree *tree,
                                    const Agentite_TechState *state,
                                    const char *id) {
    if (!tree || !state || !id) return false;

    const Agentite_TechDef *def = agentite_tech_find(tree, id);
    if (!def) return false;

    /* No prerequisites = satisfied */
    if (def->prereq_count == 0) return true;

    /* Check each prerequisite */
    for (int i = 0; i < def->prereq_count; i++) {
        if (!agentite_tech_is_researched(tree, state, def->prerequisites[i])) {
            return false;
        }
    }

    return true;
}

bool agentite_tech_can_research(const Agentite_TechTree *tree,
                               const Agentite_TechState *state,
                               const char *id) {
    if (!tree || !state || !id) return false;

    const Agentite_TechDef *def = agentite_tech_find(tree, id);
    if (!def) return false;

    /* Already researched (and not repeatable) */
    if (agentite_tech_is_researched(tree, state, id) && !def->repeatable) {
        return false;
    }

    /* Check prerequisites */
    return agentite_tech_has_prerequisites(tree, state, id);
}

/*============================================================================
 * Research Operations
 *============================================================================*/

bool agentite_tech_start_research(Agentite_TechTree *tree,
                                 Agentite_TechState *state,
                                 const char *id) {
    if (!tree || !state || !id) {
        agentite_set_error("agentite_tech_start_research: invalid parameter");
        return false;
    }

    /* Check if we can research this tech */
    if (!agentite_tech_can_research(tree, state, id)) {
        agentite_set_error("agentite_tech_start_research: cannot research %s", id);
        return false;
    }

    /* Check for available research slot */
    if (state->active_count >= AGENTITE_TECH_MAX_ACTIVE) {
        agentite_set_error("agentite_tech_start_research: no available research slots");
        return false;
    }

    /* Check if already researching this tech */
    for (int i = 0; i < state->active_count; i++) {
        if (strcmp(state->active[i].tech_id, id) == 0) {
            agentite_set_error("agentite_tech_start_research: already researching %s", id);
            return false;
        }
    }

    const Agentite_TechDef *def = agentite_tech_find(tree, id);
    int tech_index = find_tech_index(tree, id);

    /* Calculate cost (may scale for repeatable techs) */
    int32_t cost = agentite_tech_calculate_cost(def, state->repeat_count[tech_index]);

    /* Initialize research slot */
    Agentite_ActiveResearch *slot = &state->active[state->active_count];
    strncpy(slot->tech_id, id, sizeof(slot->tech_id) - 1);
    slot->tech_id[sizeof(slot->tech_id) - 1] = '\0';
    slot->points_invested = 0;
    slot->points_required = cost;
    state->active_count++;

    /* Emit event */
    emit_tech_event(tree, AGENTITE_EVENT_TECH_STARTED, tech_index, def->name);

    return true;
}

bool agentite_tech_add_points(Agentite_TechTree *tree,
                             Agentite_TechState *state,
                             int32_t points) {
    return agentite_tech_add_points_to_slot(tree, state, 0, points);
}

bool agentite_tech_add_points_to_slot(Agentite_TechTree *tree,
                                     Agentite_TechState *state,
                                     int slot,
                                     int32_t points) {
    if (!tree || !state) return false;
    if (slot < 0 || slot >= state->active_count) return false;
    if (points <= 0) return false;

    Agentite_ActiveResearch *active = &state->active[slot];
    if (active->tech_id[0] == '\0') return false;

    active->points_invested += points;

    /* Check if completed */
    if (active->points_invested >= active->points_required) {
        int tech_index = find_tech_index(tree, active->tech_id);
        if (tech_index >= 0) {
            complete_tech_internal(tree, state, tech_index);
        }

        /* Remove from active research (shift remaining) */
        for (int i = slot; i < state->active_count - 1; i++) {
            state->active[i] = state->active[i + 1];
        }
        memset(&state->active[state->active_count - 1], 0, sizeof(Agentite_ActiveResearch));
        state->active_count--;

        return true;
    }

    return false;
}

void agentite_tech_complete(Agentite_TechTree *tree,
                           Agentite_TechState *state,
                           const char *id) {
    if (!tree || !state || !id) return;

    int index = find_tech_index(tree, id);
    if (index < 0) return;

    /* If actively researching, remove from queue */
    for (int i = 0; i < state->active_count; i++) {
        if (strcmp(state->active[i].tech_id, id) == 0) {
            /* Shift remaining */
            for (int j = i; j < state->active_count - 1; j++) {
                state->active[j] = state->active[j + 1];
            }
            memset(&state->active[state->active_count - 1], 0, sizeof(Agentite_ActiveResearch));
            state->active_count--;
            break;
        }
    }

    complete_tech_internal(tree, state, index);
}

void agentite_tech_cancel_research(Agentite_TechState *state, int slot) {
    if (!state) return;
    if (slot < 0 || slot >= state->active_count) return;

    /* Shift remaining slots */
    for (int i = slot; i < state->active_count - 1; i++) {
        state->active[i] = state->active[i + 1];
    }
    memset(&state->active[state->active_count - 1], 0, sizeof(Agentite_ActiveResearch));
    state->active_count--;
}

void agentite_tech_cancel_all_research(Agentite_TechState *state) {
    if (!state) return;
    memset(state->active, 0, sizeof(state->active));
    state->active_count = 0;
}

/*============================================================================
 * Progress Queries
 *============================================================================*/

float agentite_tech_get_progress(const Agentite_TechState *state, int slot) {
    if (!state || slot < 0 || slot >= state->active_count) return 0.0f;

    const Agentite_ActiveResearch *active = &state->active[slot];
    if (active->points_required <= 0) return 0.0f;

    float progress = (float)active->points_invested / (float)active->points_required;
    if (progress > 1.0f) progress = 1.0f;
    if (progress < 0.0f) progress = 0.0f;

    return progress;
}

int32_t agentite_tech_get_remaining(const Agentite_TechState *state, int slot) {
    if (!state || slot < 0 || slot >= state->active_count) return 0;

    const Agentite_ActiveResearch *active = &state->active[slot];
    int32_t remaining = active->points_required - active->points_invested;
    return remaining > 0 ? remaining : 0;
}

bool agentite_tech_is_researching(const Agentite_TechState *state, const char *id) {
    if (!state || !id) return false;

    for (int i = 0; i < state->active_count; i++) {
        if (strcmp(state->active[i].tech_id, id) == 0) {
            return true;
        }
    }
    return false;
}

int agentite_tech_active_count(const Agentite_TechState *state) {
    return state ? state->active_count : 0;
}

int agentite_tech_get_repeat_count(const Agentite_TechTree *tree,
                                  const Agentite_TechState *state,
                                  const char *id) {
    if (!tree || !state || !id) return 0;

    int index = find_tech_index(tree, id);
    if (index < 0) return 0;

    return state->repeat_count[index];
}

/*============================================================================
 * Filtered Queries
 *============================================================================*/

int agentite_tech_get_available(const Agentite_TechTree *tree,
                               const Agentite_TechState *state,
                               const Agentite_TechDef **out_defs,
                               int max_count) {
    if (!tree || !state || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->tech_count && count < max_count; i++) {
        const Agentite_TechDef *def = &tree->techs[i];

        /* Skip hidden techs that don't have prerequisites met */
        if (def->hidden && !agentite_tech_has_prerequisites(tree, state, def->id)) {
            continue;
        }

        if (agentite_tech_can_research(tree, state, def->id)) {
            out_defs[count++] = def;
        }
    }
    return count;
}

int agentite_tech_get_completed(const Agentite_TechTree *tree,
                               const Agentite_TechState *state,
                               const Agentite_TechDef **out_defs,
                               int max_count) {
    if (!tree || !state || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->tech_count && count < max_count; i++) {
        if (state->completed[i]) {
            out_defs[count++] = &tree->techs[i];
        }
    }
    return count;
}

int agentite_tech_get_by_branch(const Agentite_TechTree *tree,
                               int32_t branch,
                               const Agentite_TechDef **out_defs,
                               int max_count) {
    if (!tree || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->tech_count && count < max_count; i++) {
        if (tree->techs[i].branch == branch) {
            out_defs[count++] = &tree->techs[i];
        }
    }
    return count;
}

int agentite_tech_get_by_tier(const Agentite_TechTree *tree,
                             int32_t tier,
                             const Agentite_TechDef **out_defs,
                             int max_count) {
    if (!tree || !out_defs || max_count <= 0) return 0;

    int count = 0;
    for (int i = 0; i < tree->tech_count && count < max_count; i++) {
        if (tree->techs[i].tier == tier) {
            out_defs[count++] = &tree->techs[i];
        }
    }
    return count;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_tech_set_completion_callback(Agentite_TechTree *tree,
                                          Agentite_TechCallback callback,
                                          void *userdata) {
    if (!tree) return;
    tree->on_complete = callback;
    tree->callback_userdata = userdata;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_tech_effect_type_name(Agentite_TechEffectType type) {
    switch (type) {
        case AGENTITE_TECH_EFFECT_NONE:            return "None";
        case AGENTITE_TECH_EFFECT_RESOURCE_BONUS:  return "Resource Bonus";
        case AGENTITE_TECH_EFFECT_RESOURCE_CAP:    return "Resource Cap";
        case AGENTITE_TECH_EFFECT_COST_REDUCTION:  return "Cost Reduction";
        case AGENTITE_TECH_EFFECT_PRODUCTION_SPEED: return "Production Speed";
        case AGENTITE_TECH_EFFECT_UNLOCK_UNIT:     return "Unlock Unit";
        case AGENTITE_TECH_EFFECT_UNLOCK_BUILDING: return "Unlock Building";
        case AGENTITE_TECH_EFFECT_UNLOCK_ABILITY:  return "Unlock Ability";
        case AGENTITE_TECH_EFFECT_ATTACK_BONUS:    return "Attack Bonus";
        case AGENTITE_TECH_EFFECT_DEFENSE_BONUS:   return "Defense Bonus";
        case AGENTITE_TECH_EFFECT_HEALTH_BONUS:    return "Health Bonus";
        case AGENTITE_TECH_EFFECT_RANGE_BONUS:     return "Range Bonus";
        case AGENTITE_TECH_EFFECT_SPEED_BONUS:     return "Speed Bonus";
        case AGENTITE_TECH_EFFECT_VISION_BONUS:    return "Vision Bonus";
        case AGENTITE_TECH_EFFECT_EXPERIENCE_BONUS: return "Experience Bonus";
        case AGENTITE_TECH_EFFECT_CUSTOM:          return "Custom";
        default:
            if (type >= AGENTITE_TECH_EFFECT_USER) {
                return "User-Defined";
            }
            return "Unknown";
    }
}

int32_t agentite_tech_calculate_cost(const Agentite_TechDef *def, int repeat_count) {
    if (!def) return 0;

    int32_t base_cost = def->research_cost;

    /* For repeatable techs, increase cost by 50% each repetition */
    if (def->repeatable && repeat_count > 0) {
        for (int i = 0; i < repeat_count; i++) {
            base_cost = (base_cost * 3) / 2;  /* 1.5x per repeat */
        }
    }

    return base_cost;
}
