/**
 * Carbon Victory Condition System
 *
 * Multi-condition victory tracking with progress monitoring,
 * score calculation, and event integration.
 */

#include "carbon/carbon.h"
#include "carbon/victory.h"
#include "carbon/event.h"
#include "carbon/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Custom checker registration
 */
typedef struct VictoryChecker {
    Carbon_VictoryChecker checker;
    void *userdata;
} VictoryChecker;

/**
 * Victory manager internal structure
 */
struct Carbon_VictoryManager {
    /* Registered conditions */
    Carbon_VictoryCondition conditions[CARBON_VICTORY_MAX_CONDITIONS];
    int condition_count;

    /* Type to index mapping (-1 = not registered) */
    int type_to_index[CARBON_VICTORY_MAX_CONDITIONS + CARBON_VICTORY_USER];

    /* Per-faction progress */
    Carbon_VictoryProgress factions[CARBON_VICTORY_MAX_FACTIONS];
    bool faction_active[CARBON_VICTORY_MAX_FACTIONS];
    int faction_count;

    /* Current game state */
    Carbon_VictoryState state;
    uint32_t current_turn;

    /* Custom checkers */
    VictoryChecker checkers[CARBON_VICTORY_MAX_CONDITIONS];

    /* Callback */
    Carbon_VictoryCallback on_victory;
    void *callback_userdata;

    /* Event dispatcher (optional) */
    Carbon_EventDispatcher *events;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find condition index by type
 */
static int find_condition_by_type(const Carbon_VictoryManager *vm, int type) {
    if (!vm) return -1;

    /* Check type_to_index mapping for fast lookup */
    if (type >= 0 && type < CARBON_VICTORY_MAX_CONDITIONS + CARBON_VICTORY_USER) {
        int index = vm->type_to_index[type];
        if (index >= 0 && index < vm->condition_count) {
            return index;
        }
    }

    /* Fallback to linear search */
    for (int i = 0; i < vm->condition_count; i++) {
        if (vm->conditions[i].type == type) {
            return i;
        }
    }
    return -1;
}

/**
 * Emit victory progress event
 */
static void emit_progress_event(Carbon_VictoryManager *vm, int type, float progress) {
    if (!vm || !vm->events) return;
    carbon_event_emit_victory_progress(vm->events, type, progress);
}

/**
 * Emit victory achieved event
 */
static void emit_victory_event(Carbon_VictoryManager *vm, int type, int winner_id) {
    if (!vm || !vm->events) return;
    carbon_event_emit_victory(vm->events, type, winner_id);
}

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

Carbon_VictoryManager *carbon_victory_create(void) {
    return carbon_victory_create_with_events(NULL);
}

Carbon_VictoryManager *carbon_victory_create_with_events(Carbon_EventDispatcher *events) {
    Carbon_VictoryManager *vm = CARBON_ALLOC(Carbon_VictoryManager);
    if (!vm) {
        carbon_set_error("carbon_victory_create: allocation failed");
        return NULL;
    }

    vm->events = events;
    vm->state.winner_id = -1;
    vm->state.victory_type = CARBON_VICTORY_NONE;

    /* Initialize type mapping to -1 (not registered) */
    for (int i = 0; i < CARBON_VICTORY_MAX_CONDITIONS + CARBON_VICTORY_USER; i++) {
        vm->type_to_index[i] = -1;
    }

    return vm;
}

void carbon_victory_destroy(Carbon_VictoryManager *vm) {
    if (!vm) return;
    free(vm);
}

/*============================================================================
 * Condition Registration
 *============================================================================*/

int carbon_victory_register(Carbon_VictoryManager *vm,
                             const Carbon_VictoryCondition *cond) {
    if (!vm || !cond) {
        carbon_set_error("carbon_victory_register: null parameter");
        return -1;
    }

    if (vm->condition_count >= CARBON_VICTORY_MAX_CONDITIONS) {
        carbon_set_error("carbon_victory_register: max conditions reached");
        return -1;
    }

    /* Check for duplicate type */
    if (find_condition_by_type(vm, cond->type) >= 0) {
        carbon_set_error("carbon_victory_register: type %d already registered", cond->type);
        return -1;
    }

    int index = vm->condition_count++;
    vm->conditions[index] = *cond;

    /* Default threshold to 1.0 if not set */
    if (vm->conditions[index].threshold <= 0.0f) {
        vm->conditions[index].threshold = 1.0f;
    }

    /* Update type mapping */
    if (cond->type >= 0 && cond->type < CARBON_VICTORY_MAX_CONDITIONS + CARBON_VICTORY_USER) {
        vm->type_to_index[cond->type] = index;
    }

    return index;
}

const Carbon_VictoryCondition *carbon_victory_get_condition(
    const Carbon_VictoryManager *vm, int index) {
    if (!vm || index < 0 || index >= vm->condition_count) {
        return NULL;
    }
    return &vm->conditions[index];
}

const Carbon_VictoryCondition *carbon_victory_get_by_type(
    const Carbon_VictoryManager *vm, int type) {
    int index = find_condition_by_type(vm, type);
    if (index < 0) return NULL;
    return &vm->conditions[index];
}

const Carbon_VictoryCondition *carbon_victory_find(
    const Carbon_VictoryManager *vm, const char *id) {
    if (!vm || !id) return NULL;

    for (int i = 0; i < vm->condition_count; i++) {
        if (strcmp(vm->conditions[i].id, id) == 0) {
            return &vm->conditions[i];
        }
    }
    return NULL;
}

int carbon_victory_condition_count(const Carbon_VictoryManager *vm) {
    return vm ? vm->condition_count : 0;
}

void carbon_victory_set_enabled(Carbon_VictoryManager *vm, int type, bool enabled) {
    int index = find_condition_by_type(vm, type);
    if (index >= 0) {
        vm->conditions[index].enabled = enabled;
    }
}

bool carbon_victory_is_enabled(const Carbon_VictoryManager *vm, int type) {
    int index = find_condition_by_type(vm, type);
    if (index < 0) return false;
    return vm->conditions[index].enabled;
}

/*============================================================================
 * Progress Tracking
 *============================================================================*/

void carbon_victory_init_faction(Carbon_VictoryManager *vm, int faction_id) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return;
    }

    memset(&vm->factions[faction_id], 0, sizeof(Carbon_VictoryProgress));
    vm->faction_active[faction_id] = true;
    vm->faction_count++;
}

void carbon_victory_update_progress(Carbon_VictoryManager *vm,
                                     int faction_id,
                                     int type,
                                     float progress) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return;
    }

    int index = find_condition_by_type(vm, type);
    if (index < 0) return;

    /* Clamp progress to 0-1 range */
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    float old_progress = vm->factions[faction_id].progress[index];
    vm->factions[faction_id].progress[index] = progress;

    /* Emit event if progress changed significantly (>1%) */
    if (vm->events && (progress - old_progress > 0.01f || progress - old_progress < -0.01f)) {
        emit_progress_event(vm, type, progress);
    }
}

void carbon_victory_update_score(Carbon_VictoryManager *vm,
                                  int faction_id,
                                  int type,
                                  int32_t score) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return;
    }

    int index = find_condition_by_type(vm, type);
    if (index < 0) return;

    vm->factions[faction_id].score[index] = score;

    /* Calculate progress from score and target */
    const Carbon_VictoryCondition *cond = &vm->conditions[index];
    if (cond->target_value > 0) {
        float progress = (float)score / (float)cond->target_value;
        if (progress > 1.0f) progress = 1.0f;
        vm->factions[faction_id].progress[index] = progress;

        emit_progress_event(vm, type, progress);
    }
}

void carbon_victory_add_score(Carbon_VictoryManager *vm,
                               int faction_id,
                               int type,
                               int32_t delta) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return;
    }

    int index = find_condition_by_type(vm, type);
    if (index < 0) return;

    int32_t new_score = vm->factions[faction_id].score[index] + delta;
    carbon_victory_update_score(vm, faction_id, type, new_score);
}

float carbon_victory_get_progress(const Carbon_VictoryManager *vm,
                                   int faction_id,
                                   int type) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return 0.0f;
    }

    int index = find_condition_by_type(vm, type);
    if (index < 0) return 0.0f;

    return vm->factions[faction_id].progress[index];
}

int32_t carbon_victory_get_score(const Carbon_VictoryManager *vm,
                                  int faction_id,
                                  int type) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return 0;
    }

    int index = find_condition_by_type(vm, type);
    if (index < 0) return 0;

    return vm->factions[faction_id].score[index];
}

const Carbon_VictoryProgress *carbon_victory_get_faction_progress(
    const Carbon_VictoryManager *vm, int faction_id) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return NULL;
    }
    return &vm->factions[faction_id];
}

void carbon_victory_eliminate_faction(Carbon_VictoryManager *vm, int faction_id) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return;
    }

    if (vm->faction_active[faction_id] && !vm->factions[faction_id].eliminated) {
        vm->factions[faction_id].eliminated = true;
        vm->faction_count--;
    }
}

bool carbon_victory_is_eliminated(const Carbon_VictoryManager *vm, int faction_id) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return true;
    }
    return vm->factions[faction_id].eliminated;
}

int carbon_victory_active_faction_count(const Carbon_VictoryManager *vm) {
    if (!vm) return 0;

    int count = 0;
    for (int i = 0; i < CARBON_VICTORY_MAX_FACTIONS; i++) {
        if (vm->faction_active[i] && !vm->factions[i].eliminated) {
            count++;
        }
    }
    return count;
}

/*============================================================================
 * Victory Checking
 *============================================================================*/

bool carbon_victory_check_condition(const Carbon_VictoryManager *vm,
                                     int faction_id,
                                     int type) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return false;
    }

    /* Eliminated factions can't win */
    if (vm->factions[faction_id].eliminated) {
        return false;
    }

    int index = find_condition_by_type(vm, type);
    if (index < 0) return false;

    const Carbon_VictoryCondition *cond = &vm->conditions[index];
    if (!cond->enabled) return false;

    float progress = vm->factions[faction_id].progress[index];
    return progress >= cond->threshold;
}

bool carbon_victory_check(Carbon_VictoryManager *vm) {
    if (!vm || vm->state.achieved) {
        return vm ? vm->state.achieved : false;
    }

    /* Check each condition for each faction */
    for (int c = 0; c < vm->condition_count; c++) {
        const Carbon_VictoryCondition *cond = &vm->conditions[c];
        if (!cond->enabled) continue;

        /* If there's a custom checker, use it */
        if (vm->checkers[c].checker) {
            for (int f = 0; f < CARBON_VICTORY_MAX_FACTIONS; f++) {
                if (!vm->faction_active[f] || vm->factions[f].eliminated) {
                    continue;
                }

                float progress = 0.0f;
                if (vm->checkers[c].checker(f, cond->type, &progress,
                                            vm->checkers[c].userdata)) {
                    /* Update progress and declare victory */
                    vm->factions[f].progress[c] = progress;
                    carbon_victory_declare(vm, f, cond->type, NULL);
                    return true;
                }
                /* Update progress even if not winning */
                vm->factions[f].progress[c] = progress;
            }
        } else {
            /* Default check: compare progress to threshold */
            for (int f = 0; f < CARBON_VICTORY_MAX_FACTIONS; f++) {
                if (!vm->faction_active[f] || vm->factions[f].eliminated) {
                    continue;
                }

                if (vm->factions[f].progress[c] >= cond->threshold) {
                    carbon_victory_declare(vm, f, cond->type, NULL);
                    return true;
                }
            }
        }
    }

    /* Check for elimination victory (last faction standing) */
    int index = find_condition_by_type(vm, CARBON_VICTORY_ELIMINATION);
    if (index >= 0 && vm->conditions[index].enabled) {
        int survivor = -1;
        int survivor_count = 0;

        for (int f = 0; f < CARBON_VICTORY_MAX_FACTIONS; f++) {
            if (vm->faction_active[f] && !vm->factions[f].eliminated) {
                survivor = f;
                survivor_count++;
            }
        }

        if (survivor_count == 1 && survivor >= 0) {
            carbon_victory_declare(vm, survivor, CARBON_VICTORY_ELIMINATION,
                                    "Last faction standing!");
            return true;
        }
    }

    /* Check for time/score victory */
    index = find_condition_by_type(vm, CARBON_VICTORY_SCORE);
    if (index >= 0 && vm->conditions[index].enabled) {
        const Carbon_VictoryCondition *cond = &vm->conditions[index];
        if (cond->target_turn > 0 && vm->current_turn >= (uint32_t)cond->target_turn) {
            /* Time's up - highest score wins */
            int leader = carbon_victory_get_score_leader(vm);
            if (leader >= 0) {
                carbon_victory_declare(vm, leader, CARBON_VICTORY_SCORE,
                                        "Highest score at end of game!");
                return true;
            }
        }
    }

    return false;
}

void carbon_victory_declare(Carbon_VictoryManager *vm,
                             int faction_id,
                             int type,
                             const char *message) {
    if (!vm || vm->state.achieved) return;

    vm->state.achieved = true;
    vm->state.winner_id = faction_id;
    vm->state.victory_type = type;
    vm->state.winning_turn = vm->current_turn;
    vm->state.winning_score = carbon_victory_calculate_score(vm, faction_id);

    if (message) {
        strncpy(vm->state.message, message, sizeof(vm->state.message) - 1);
        vm->state.message[sizeof(vm->state.message) - 1] = '\0';
    } else {
        /* Generate default message */
        const Carbon_VictoryCondition *cond = carbon_victory_get_by_type(vm, type);
        if (cond) {
            snprintf(vm->state.message, sizeof(vm->state.message),
                     "Victory achieved: %s", cond->name);
        } else {
            snprintf(vm->state.message, sizeof(vm->state.message),
                     "Victory achieved!");
        }
    }

    /* Emit event */
    emit_victory_event(vm, type, faction_id);

    /* Call callback */
    if (vm->on_victory) {
        const Carbon_VictoryCondition *cond = carbon_victory_get_by_type(vm, type);
        vm->on_victory(faction_id, type, cond, vm->callback_userdata);
    }
}

bool carbon_victory_is_achieved(const Carbon_VictoryManager *vm) {
    return vm ? vm->state.achieved : false;
}

int carbon_victory_get_winner(const Carbon_VictoryManager *vm) {
    return vm ? vm->state.winner_id : -1;
}

int carbon_victory_get_winning_type(const Carbon_VictoryManager *vm) {
    return vm ? vm->state.victory_type : CARBON_VICTORY_NONE;
}

const Carbon_VictoryState *carbon_victory_get_state(const Carbon_VictoryManager *vm) {
    return vm ? &vm->state : NULL;
}

void carbon_victory_reset(Carbon_VictoryManager *vm) {
    if (!vm) return;

    /* Reset victory state */
    memset(&vm->state, 0, sizeof(Carbon_VictoryState));
    vm->state.winner_id = -1;
    vm->state.victory_type = CARBON_VICTORY_NONE;

    /* Reset faction progress */
    for (int i = 0; i < CARBON_VICTORY_MAX_FACTIONS; i++) {
        memset(&vm->factions[i], 0, sizeof(Carbon_VictoryProgress));
        vm->faction_active[i] = false;
    }
    vm->faction_count = 0;
    vm->current_turn = 0;
}

/*============================================================================
 * Score Victory Support
 *============================================================================*/

void carbon_victory_set_turn(Carbon_VictoryManager *vm, uint32_t turn) {
    if (vm) {
        vm->current_turn = turn;
    }
}

int32_t carbon_victory_calculate_score(const Carbon_VictoryManager *vm,
                                        int faction_id) {
    if (!vm || faction_id < 0 || faction_id >= CARBON_VICTORY_MAX_FACTIONS) {
        return 0;
    }

    int32_t total = 0;
    const Carbon_VictoryProgress *progress = &vm->factions[faction_id];

    for (int i = 0; i < vm->condition_count; i++) {
        const Carbon_VictoryCondition *cond = &vm->conditions[i];
        int32_t weight = cond->score_weight > 0 ? cond->score_weight : 1;
        total += progress->score[i] * weight;
    }

    return total;
}

int carbon_victory_get_score_leader(const Carbon_VictoryManager *vm) {
    if (!vm) return -1;

    int leader = -1;
    int32_t highest = -1;

    for (int f = 0; f < CARBON_VICTORY_MAX_FACTIONS; f++) {
        if (!vm->faction_active[f] || vm->factions[f].eliminated) {
            continue;
        }

        int32_t score = carbon_victory_calculate_score(vm, f);
        if (score > highest) {
            highest = score;
            leader = f;
        }
    }

    return leader;
}

/*============================================================================
 * Custom Victory Checkers
 *============================================================================*/

void carbon_victory_set_checker(Carbon_VictoryManager *vm,
                                 int type,
                                 Carbon_VictoryChecker checker,
                                 void *userdata) {
    if (!vm) return;

    int index = find_condition_by_type(vm, type);
    if (index < 0) return;

    vm->checkers[index].checker = checker;
    vm->checkers[index].userdata = userdata;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_victory_set_callback(Carbon_VictoryManager *vm,
                                  Carbon_VictoryCallback callback,
                                  void *userdata) {
    if (!vm) return;
    vm->on_victory = callback;
    vm->callback_userdata = userdata;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_victory_type_name(int type) {
    switch (type) {
        case CARBON_VICTORY_NONE:        return "None";
        case CARBON_VICTORY_DOMINATION:  return "Domination";
        case CARBON_VICTORY_ELIMINATION: return "Elimination";
        case CARBON_VICTORY_TECHNOLOGY:  return "Technology";
        case CARBON_VICTORY_ECONOMIC:    return "Economic";
        case CARBON_VICTORY_SCORE:       return "Score";
        case CARBON_VICTORY_TIME:        return "Time";
        case CARBON_VICTORY_OBJECTIVE:   return "Objective";
        case CARBON_VICTORY_WONDER:      return "Wonder";
        case CARBON_VICTORY_DIPLOMATIC:  return "Diplomatic";
        case CARBON_VICTORY_CULTURAL:    return "Cultural";
        default:
            if (type >= CARBON_VICTORY_USER) {
                return "Custom";
            }
            return "Unknown";
    }
}

char *carbon_victory_format_progress(const Carbon_VictoryManager *vm,
                                      int faction_id,
                                      int type,
                                      char *buffer,
                                      size_t size) {
    if (!vm || !buffer || size == 0) {
        return buffer;
    }

    float progress = carbon_victory_get_progress(vm, faction_id, type);
    int index = find_condition_by_type(vm, type);

    if (index >= 0) {
        const Carbon_VictoryCondition *cond = &vm->conditions[index];
        snprintf(buffer, size, "%.1f%% / %.1f%%",
                 progress * 100.0f,
                 cond->threshold * 100.0f);
    } else {
        snprintf(buffer, size, "%.1f%%", progress * 100.0f);
    }

    return buffer;
}
