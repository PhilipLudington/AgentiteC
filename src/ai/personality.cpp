/**
 * Carbon AI Personality System
 *
 * Personality-driven AI decision making with weighted behaviors,
 * threat assessment, goal management, and extensible action evaluation.
 */

#include "carbon/carbon.h"
#include "carbon/ai.h"
#include "carbon/error.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Registered evaluator entry
 */
typedef struct {
    Carbon_AIActionType type;
    Carbon_AIEvaluator evaluator;
} EvaluatorEntry;

/**
 * AI system internal structure
 */
struct Carbon_AISystem {
    /* Registered evaluators */
    EvaluatorEntry evaluators[CARBON_AI_MAX_EVALUATORS];
    int evaluator_count;

    /* Callbacks */
    Carbon_AIThreatAssessor threat_assessor;
    Carbon_AISituationAnalyzer situation_analyzer;
};

/*============================================================================
 * Default Personality Weights
 *============================================================================*/

/**
 * Predefined weight configurations for each personality type
 */
static const Carbon_AIWeights DEFAULT_WEIGHTS[] = {
    /* BALANCED */
    { .aggression = 0.5f, .defense = 0.5f, .expansion = 0.5f,
      .economy = 0.5f, .technology = 0.5f, .diplomacy = 0.5f,
      .caution = 0.5f, .opportunism = 0.5f },

    /* AGGRESSIVE */
    { .aggression = 0.9f, .defense = 0.3f, .expansion = 0.7f,
      .economy = 0.4f, .technology = 0.3f, .diplomacy = 0.2f,
      .caution = 0.2f, .opportunism = 0.8f },

    /* DEFENSIVE */
    { .aggression = 0.2f, .defense = 0.9f, .expansion = 0.3f,
      .economy = 0.6f, .technology = 0.5f, .diplomacy = 0.6f,
      .caution = 0.8f, .opportunism = 0.3f },

    /* ECONOMIC */
    { .aggression = 0.3f, .defense = 0.5f, .expansion = 0.6f,
      .economy = 0.9f, .technology = 0.6f, .diplomacy = 0.7f,
      .caution = 0.6f, .opportunism = 0.5f },

    /* EXPANSIONIST */
    { .aggression = 0.6f, .defense = 0.4f, .expansion = 0.9f,
      .economy = 0.5f, .technology = 0.4f, .diplomacy = 0.4f,
      .caution = 0.3f, .opportunism = 0.7f },

    /* TECHNOLOGIST */
    { .aggression = 0.3f, .defense = 0.5f, .expansion = 0.4f,
      .economy = 0.7f, .technology = 0.9f, .diplomacy = 0.5f,
      .caution = 0.6f, .opportunism = 0.4f },

    /* DIPLOMATIC */
    { .aggression = 0.2f, .defense = 0.6f, .expansion = 0.3f,
      .economy = 0.6f, .technology = 0.5f, .diplomacy = 0.9f,
      .caution = 0.7f, .opportunism = 0.4f },

    /* OPPORTUNIST */
    { .aggression = 0.5f, .defense = 0.5f, .expansion = 0.6f,
      .economy = 0.5f, .technology = 0.5f, .diplomacy = 0.5f,
      .caution = 0.4f, .opportunism = 0.9f },
};

/*============================================================================
 * Random Number Generator (xorshift32)
 *============================================================================*/

static uint32_t xorshift32(uint32_t *state) {
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Compare actions by priority (for qsort, descending order)
 */
static int compare_actions(const void *a, const void *b) {
    const Carbon_AIAction *action_a = (const Carbon_AIAction*)a;
    const Carbon_AIAction *action_b = (const Carbon_AIAction*)b;

    /* Sort by priority descending, then by urgency descending */
    if (action_b->priority != action_a->priority) {
        return (action_b->priority > action_a->priority) ? 1 : -1;
    }
    return (action_b->urgency > action_a->urgency) ? 1 : -1;
}

/**
 * Get weight for an action type
 */
static float get_weight_for_action(const Carbon_AIWeights *weights,
                                    Carbon_AIActionType type) {
    switch (type) {
        case CARBON_AI_ACTION_ATTACK:
            return weights->aggression;
        case CARBON_AI_ACTION_DEFEND:
            return weights->defense;
        case CARBON_AI_ACTION_EXPAND:
        case CARBON_AI_ACTION_SCOUT:
            return weights->expansion;
        case CARBON_AI_ACTION_BUILD:
        case CARBON_AI_ACTION_TRADE:
            return weights->economy;
        case CARBON_AI_ACTION_RESEARCH:
        case CARBON_AI_ACTION_UPGRADE:
            return weights->technology;
        case CARBON_AI_ACTION_DIPLOMACY:
            return weights->diplomacy;
        case CARBON_AI_ACTION_RETREAT:
            return weights->caution;
        case CARBON_AI_ACTION_RECRUIT:
            return (weights->aggression + weights->defense) * 0.5f;
        default:
            return 0.5f;  /* Neutral weight for unknown types */
    }
}

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

Carbon_AISystem *carbon_ai_create(void) {
    Carbon_AISystem *ai = CARBON_ALLOC(Carbon_AISystem);
    if (!ai) {
        carbon_set_error("carbon_ai_create: allocation failed");
        return NULL;
    }
    return ai;
}

void carbon_ai_destroy(Carbon_AISystem *ai) {
    if (ai) {
        free(ai);
    }
}

/*============================================================================
 * State Management
 *============================================================================*/

void carbon_ai_state_init(Carbon_AIState *state, Carbon_AIPersonality personality) {
    if (!state) return;

    memset(state, 0, sizeof(Carbon_AIState));

    state->personality = personality;
    state->primary_target = -1;
    state->ally_target = -1;
    state->morale = 0.5f;
    state->resources_ratio = 1.0f;
    state->military_ratio = 1.0f;
    state->tech_ratio = 1.0f;
    state->last_action_type = CARBON_AI_ACTION_NONE;
    state->last_target = -1;

    /* Set weights based on personality */
    carbon_ai_get_default_weights(personality, &state->weights);
    state->base_weights = state->weights;

    /* Initialize random state */
    carbon_ai_seed_random(state, 0);
}

void carbon_ai_state_reset(Carbon_AIState *state) {
    if (!state) return;

    Carbon_AIPersonality personality = state->personality;
    carbon_ai_state_init(state, personality);
}

void carbon_ai_get_default_weights(Carbon_AIPersonality personality,
                                    Carbon_AIWeights *out) {
    if (!out) return;

    if (personality >= 0 && personality < CARBON_AI_PERSONALITY_COUNT) {
        *out = DEFAULT_WEIGHTS[personality];
    } else {
        /* User-defined personality - use balanced as fallback */
        *out = DEFAULT_WEIGHTS[CARBON_AI_BALANCED];
    }
}

void carbon_ai_set_weights(Carbon_AIState *state, const Carbon_AIWeights *weights) {
    if (!state || !weights) return;
    state->weights = *weights;
}

void carbon_ai_modify_weights(Carbon_AIState *state, const Carbon_AIWeights *modifiers) {
    if (!state || !modifiers) return;

    state->weights.aggression *= modifiers->aggression;
    state->weights.defense *= modifiers->defense;
    state->weights.expansion *= modifiers->expansion;
    state->weights.economy *= modifiers->economy;
    state->weights.technology *= modifiers->technology;
    state->weights.diplomacy *= modifiers->diplomacy;
    state->weights.caution *= modifiers->caution;
    state->weights.opportunism *= modifiers->opportunism;
}

void carbon_ai_reset_weights(Carbon_AIState *state) {
    if (!state) return;
    state->weights = state->base_weights;
}

/*============================================================================
 * Evaluator Registration
 *============================================================================*/

void carbon_ai_register_evaluator(Carbon_AISystem *ai,
                                   Carbon_AIActionType type,
                                   Carbon_AIEvaluator evaluator) {
    if (!ai || !evaluator) return;

    if (ai->evaluator_count >= CARBON_AI_MAX_EVALUATORS) {
        carbon_set_error("carbon_ai_register_evaluator: max evaluators reached");
        return;
    }

    /* Check for duplicate */
    for (int i = 0; i < ai->evaluator_count; i++) {
        if (ai->evaluators[i].type == type) {
            ai->evaluators[i].evaluator = evaluator;
            return;
        }
    }

    ai->evaluators[ai->evaluator_count].type = type;
    ai->evaluators[ai->evaluator_count].evaluator = evaluator;
    ai->evaluator_count++;
}

void carbon_ai_set_threat_assessor(Carbon_AISystem *ai,
                                    Carbon_AIThreatAssessor assessor) {
    if (ai) {
        ai->threat_assessor = assessor;
    }
}

void carbon_ai_set_situation_analyzer(Carbon_AISystem *ai,
                                       Carbon_AISituationAnalyzer analyzer) {
    if (ai) {
        ai->situation_analyzer = analyzer;
    }
}

/*============================================================================
 * Decision Making
 *============================================================================*/

void carbon_ai_process_turn(Carbon_AISystem *ai,
                             Carbon_AIState *state,
                             void *game_ctx,
                             Carbon_AIDecision *out) {
    if (!ai || !state || !out) return;

    memset(out, 0, sizeof(Carbon_AIDecision));

    /* Update situation and threats first */
    carbon_ai_update_situation(ai, state, game_ctx);
    carbon_ai_update_threats(ai, state, game_ctx);

    /* Collect actions from all evaluators */
    Carbon_AIAction temp_actions[CARBON_AI_MAX_ACTIONS * 2];
    int temp_count = 0;

    for (int i = 0; i < ai->evaluator_count && temp_count < CARBON_AI_MAX_ACTIONS * 2; i++) {
        Carbon_AIAction eval_actions[CARBON_AI_MAX_ACTIONS];
        int eval_count = 0;

        /* Skip if action type is on cooldown */
        if (carbon_ai_is_on_cooldown(state, ai->evaluators[i].type)) {
            continue;
        }

        /* Call evaluator */
        ai->evaluators[i].evaluator(state, game_ctx, eval_actions, &eval_count,
                                     CARBON_AI_MAX_ACTIONS);

        /* Score and add actions */
        for (int j = 0; j < eval_count && temp_count < CARBON_AI_MAX_ACTIONS * 2; j++) {
            Carbon_AIAction *action = &eval_actions[j];

            /* Apply personality weights */
            action->priority = carbon_ai_score_action(state, action->type,
                                                       action->priority);

            /* Boost urgency for high-threat situations */
            if (state->overall_threat > 0.7f &&
                (action->type == CARBON_AI_ACTION_DEFEND ||
                 action->type == CARBON_AI_ACTION_RETREAT)) {
                action->urgency *= 1.5f;
            }

            /* Add small random factor for variety */
            action->priority += carbon_ai_random(state) * 0.1f - 0.05f;

            temp_actions[temp_count++] = *action;
        }
    }

    /* Sort all actions by priority */
    if (temp_count > 0) {
        qsort(temp_actions, temp_count, sizeof(Carbon_AIAction), compare_actions);
    }

    /* Copy top actions to output */
    out->action_count = (temp_count < CARBON_AI_MAX_ACTIONS) ?
                         temp_count : CARBON_AI_MAX_ACTIONS;
    out->total_score = 0.0f;

    for (int i = 0; i < out->action_count; i++) {
        out->actions[i] = temp_actions[i];
        out->total_score += temp_actions[i].priority;
    }

    /* Update cooldowns */
    carbon_ai_update_cooldowns(state);

    /* Update memory */
    if (out->action_count > 0) {
        state->last_action_type = out->actions[0].type;
        state->last_target = out->actions[0].target_id;
    }
}

float carbon_ai_score_action(const Carbon_AIState *state,
                              Carbon_AIActionType type,
                              float base_score) {
    if (!state) return base_score;

    float weight = get_weight_for_action(&state->weights, type);
    float score = base_score * weight;

    /* Apply situational modifiers */
    switch (type) {
        case CARBON_AI_ACTION_ATTACK:
            /* Boost attack if we're strong */
            if (state->military_ratio > 1.2f) {
                score *= 1.2f;
            }
            /* Reduce if under threat */
            if (state->overall_threat > 0.6f) {
                score *= 0.7f;
            }
            break;

        case CARBON_AI_ACTION_DEFEND:
            /* Boost defense under threat */
            if (state->overall_threat > 0.5f) {
                score *= 1.0f + state->overall_threat;
            }
            break;

        case CARBON_AI_ACTION_EXPAND:
            /* Reduce expansion under threat */
            if (state->overall_threat > 0.4f) {
                score *= 0.6f;
            }
            break;

        case CARBON_AI_ACTION_BUILD:
        case CARBON_AI_ACTION_TRADE:
            /* Boost economy if we're resource-poor */
            if (state->resources_ratio < 0.8f) {
                score *= 1.3f;
            }
            break;

        case CARBON_AI_ACTION_RESEARCH:
        case CARBON_AI_ACTION_UPGRADE:
            /* Boost tech if we're behind */
            if (state->tech_ratio < 0.9f) {
                score *= 1.2f;
            }
            break;

        case CARBON_AI_ACTION_RETREAT:
            /* Strongly boost retreat if low morale and under threat */
            if (state->morale < 0.3f && state->overall_threat > 0.6f) {
                score *= 2.0f;
            }
            break;

        default:
            break;
    }

    /* Apply morale modifier */
    if (state->morale > 0.7f) {
        /* High morale = more aggressive */
        if (type == CARBON_AI_ACTION_ATTACK || type == CARBON_AI_ACTION_EXPAND) {
            score *= 1.1f;
        }
    } else if (state->morale < 0.3f) {
        /* Low morale = more cautious */
        if (type == CARBON_AI_ACTION_DEFEND || type == CARBON_AI_ACTION_RETREAT) {
            score *= 1.2f;
        }
    }

    return score;
}

void carbon_ai_sort_actions(Carbon_AIDecision *decision) {
    if (!decision || decision->action_count < 2) return;

    qsort(decision->actions, decision->action_count,
          sizeof(Carbon_AIAction), compare_actions);
}

int carbon_ai_get_top_actions(const Carbon_AIDecision *decision,
                               Carbon_AIAction *out,
                               int max) {
    if (!decision || !out || max <= 0) return 0;

    int count = (decision->action_count < max) ? decision->action_count : max;
    memcpy(out, decision->actions, count * sizeof(Carbon_AIAction));
    return count;
}

/*============================================================================
 * Threat Management
 *============================================================================*/

void carbon_ai_update_threats(Carbon_AISystem *ai,
                               Carbon_AIState *state,
                               void *game_ctx) {
    if (!ai || !state) return;

    /* Age existing threats */
    for (int i = 0; i < state->threat_count; i++) {
        state->threats[i].turns_since_update++;
    }

    /* Use custom assessor if available */
    if (ai->threat_assessor) {
        Carbon_AIThreat new_threats[CARBON_AI_MAX_THREATS];
        int new_count = 0;

        ai->threat_assessor(state, game_ctx, new_threats, &new_count,
                            CARBON_AI_MAX_THREATS);

        /* Replace threat list */
        state->threat_count = new_count;
        memcpy(state->threats, new_threats, new_count * sizeof(Carbon_AIThreat));
    }

    /* Recalculate overall threat */
    state->overall_threat = carbon_ai_calculate_threat_level(state);
}

void carbon_ai_add_threat(Carbon_AIState *state,
                           int32_t source_id,
                           float level,
                           int32_t target_id,
                           float distance) {
    if (!state) return;

    /* Check for existing threat from same source */
    for (int i = 0; i < state->threat_count; i++) {
        if (state->threats[i].source_id == source_id) {
            /* Update existing */
            state->threats[i].level = level;
            state->threats[i].target_id = target_id;
            state->threats[i].distance = distance;
            state->threats[i].turns_since_update = 0;
            return;
        }
    }

    /* Add new threat */
    if (state->threat_count < CARBON_AI_MAX_THREATS) {
        Carbon_AIThreat *threat = &state->threats[state->threat_count++];
        threat->source_id = source_id;
        threat->level = level;
        threat->target_id = target_id;
        threat->distance = distance;
        threat->turns_since_update = 0;
    }
}

void carbon_ai_remove_threat(Carbon_AIState *state, int32_t source_id) {
    if (!state) return;

    for (int i = 0; i < state->threat_count; i++) {
        if (state->threats[i].source_id == source_id) {
            /* Swap with last and decrement */
            state->threats[i] = state->threats[--state->threat_count];
            return;
        }
    }
}

const Carbon_AIThreat *carbon_ai_get_highest_threat(const Carbon_AIState *state) {
    if (!state || state->threat_count == 0) return NULL;

    const Carbon_AIThreat *highest = &state->threats[0];
    for (int i = 1; i < state->threat_count; i++) {
        if (state->threats[i].level > highest->level) {
            highest = &state->threats[i];
        }
    }
    return highest;
}

float carbon_ai_calculate_threat_level(Carbon_AIState *state) {
    if (!state || state->threat_count == 0) return 0.0f;

    float total = 0.0f;
    float max = 0.0f;

    for (int i = 0; i < state->threat_count; i++) {
        /* Weight by distance (closer = more threatening) */
        float weighted = state->threats[i].level;
        if (state->threats[i].distance > 0) {
            weighted *= 1.0f / (1.0f + state->threats[i].distance * 0.1f);
        }

        /* Decay by age */
        float age_factor = 1.0f / (1.0f + state->threats[i].turns_since_update * 0.2f);
        weighted *= age_factor;

        total += weighted;
        if (weighted > max) max = weighted;
    }

    /* Combine: 70% max threat, 30% average */
    float average = total / state->threat_count;
    float combined = max * 0.7f + average * 0.3f;

    /* Clamp to 0-1 */
    if (combined > 1.0f) combined = 1.0f;
    return combined;
}

/*============================================================================
 * Goal Management
 *============================================================================*/

int carbon_ai_add_goal(Carbon_AIState *state,
                        int32_t type,
                        int32_t target_id,
                        float priority) {
    if (!state || state->goal_count >= CARBON_AI_MAX_GOALS) {
        return -1;
    }

    int index = state->goal_count++;
    Carbon_AIGoal *goal = &state->goals[index];

    goal->type = type;
    goal->target_id = target_id;
    goal->priority = priority;
    goal->progress = 0.0f;
    goal->turns_active = 0;
    goal->completed = false;

    return index;
}

void carbon_ai_update_goal_progress(Carbon_AIState *state,
                                     int index,
                                     float progress) {
    if (!state || index < 0 || index >= state->goal_count) return;

    state->goals[index].progress = progress;
    if (progress >= 1.0f) {
        state->goals[index].completed = true;
    }
}

void carbon_ai_complete_goal(Carbon_AIState *state, int index) {
    if (!state || index < 0 || index >= state->goal_count) return;

    state->goals[index].completed = true;
    state->goals[index].progress = 1.0f;
}

void carbon_ai_remove_goal(Carbon_AIState *state, int index) {
    if (!state || index < 0 || index >= state->goal_count) return;

    /* Swap with last and decrement */
    state->goals[index] = state->goals[--state->goal_count];
}

const Carbon_AIGoal *carbon_ai_get_primary_goal(const Carbon_AIState *state) {
    if (!state || state->goal_count == 0) return NULL;

    const Carbon_AIGoal *primary = NULL;
    float highest_priority = -1.0f;

    for (int i = 0; i < state->goal_count; i++) {
        if (!state->goals[i].completed &&
            state->goals[i].priority > highest_priority) {
            primary = &state->goals[i];
            highest_priority = state->goals[i].priority;
        }
    }

    return primary;
}

void carbon_ai_cleanup_goals(Carbon_AIState *state, int max_stale_turns) {
    if (!state) return;

    for (int i = state->goal_count - 1; i >= 0; i--) {
        Carbon_AIGoal *goal = &state->goals[i];

        /* Remove completed or stale goals */
        if (goal->completed || goal->turns_active > max_stale_turns) {
            carbon_ai_remove_goal(state, i);
        } else {
            goal->turns_active++;
        }
    }
}

/*============================================================================
 * Cooldowns
 *============================================================================*/

void carbon_ai_set_cooldown(Carbon_AIState *state,
                             Carbon_AIActionType type,
                             int turns) {
    if (!state || type < 0 || type >= CARBON_AI_MAX_COOLDOWNS) return;
    state->cooldowns[type] = turns;
}

bool carbon_ai_is_on_cooldown(const Carbon_AIState *state, Carbon_AIActionType type) {
    if (!state || type < 0 || type >= CARBON_AI_MAX_COOLDOWNS) return false;
    return state->cooldowns[type] > 0;
}

int carbon_ai_get_cooldown(const Carbon_AIState *state, Carbon_AIActionType type) {
    if (!state || type < 0 || type >= CARBON_AI_MAX_COOLDOWNS) return 0;
    return state->cooldowns[type];
}

void carbon_ai_update_cooldowns(Carbon_AIState *state) {
    if (!state) return;

    for (int i = 0; i < CARBON_AI_MAX_COOLDOWNS; i++) {
        if (state->cooldowns[i] > 0) {
            state->cooldowns[i]--;
        }
    }

    /* Update activity tracking */
    state->turns_since_combat++;
    state->turns_since_expansion++;

    /* Reset counters based on last action */
    if (state->last_action_type == CARBON_AI_ACTION_ATTACK) {
        state->turns_since_combat = 0;
    } else if (state->last_action_type == CARBON_AI_ACTION_EXPAND) {
        state->turns_since_expansion = 0;
    }
}

/*============================================================================
 * Situation Analysis
 *============================================================================*/

void carbon_ai_update_situation(Carbon_AISystem *ai,
                                 Carbon_AIState *state,
                                 void *game_ctx) {
    if (!ai || !state) return;

    if (ai->situation_analyzer) {
        ai->situation_analyzer(state, game_ctx);
    }
}

void carbon_ai_set_ratios(Carbon_AIState *state,
                           float resources,
                           float military,
                           float tech) {
    if (!state) return;

    state->resources_ratio = resources;
    state->military_ratio = military;
    state->tech_ratio = tech;
}

void carbon_ai_set_morale(Carbon_AIState *state, float morale) {
    if (!state) return;

    if (morale < 0.0f) morale = 0.0f;
    if (morale > 1.0f) morale = 1.0f;
    state->morale = morale;
}

/*============================================================================
 * Targeting
 *============================================================================*/

void carbon_ai_set_primary_target(Carbon_AIState *state, int32_t target_id) {
    if (state) {
        state->primary_target = target_id;
    }
}

void carbon_ai_set_ally_target(Carbon_AIState *state, int32_t ally_id) {
    if (state) {
        state->ally_target = ally_id;
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_ai_personality_name(Carbon_AIPersonality personality) {
    switch (personality) {
        case CARBON_AI_BALANCED:     return "Balanced";
        case CARBON_AI_AGGRESSIVE:   return "Aggressive";
        case CARBON_AI_DEFENSIVE:    return "Defensive";
        case CARBON_AI_ECONOMIC:     return "Economic";
        case CARBON_AI_EXPANSIONIST: return "Expansionist";
        case CARBON_AI_TECHNOLOGIST: return "Technologist";
        case CARBON_AI_DIPLOMATIC:   return "Diplomatic";
        case CARBON_AI_OPPORTUNIST:  return "Opportunist";
        default:
            if (personality >= CARBON_AI_PERSONALITY_USER) {
                return "Custom";
            }
            return "Unknown";
    }
}

const char *carbon_ai_action_name(Carbon_AIActionType type) {
    switch (type) {
        case CARBON_AI_ACTION_NONE:      return "None";
        case CARBON_AI_ACTION_BUILD:     return "Build";
        case CARBON_AI_ACTION_ATTACK:    return "Attack";
        case CARBON_AI_ACTION_DEFEND:    return "Defend";
        case CARBON_AI_ACTION_EXPAND:    return "Expand";
        case CARBON_AI_ACTION_RESEARCH:  return "Research";
        case CARBON_AI_ACTION_DIPLOMACY: return "Diplomacy";
        case CARBON_AI_ACTION_RECRUIT:   return "Recruit";
        case CARBON_AI_ACTION_RETREAT:   return "Retreat";
        case CARBON_AI_ACTION_SCOUT:     return "Scout";
        case CARBON_AI_ACTION_TRADE:     return "Trade";
        case CARBON_AI_ACTION_UPGRADE:   return "Upgrade";
        case CARBON_AI_ACTION_SPECIAL:   return "Special";
        default:
            if (type >= CARBON_AI_ACTION_USER) {
                return "Custom";
            }
            return "Unknown";
    }
}

float carbon_ai_random(Carbon_AIState *state) {
    if (!state) return 0.0f;

    uint32_t r = xorshift32(&state->random_state);
    return (float)r / (float)UINT32_MAX;
}

int carbon_ai_random_int(Carbon_AIState *state, int min, int max) {
    if (!state || min > max) return min;

    float r = carbon_ai_random(state);
    return min + (int)(r * (max - min + 1));
}

void carbon_ai_seed_random(Carbon_AIState *state, uint32_t seed) {
    if (!state) return;

    if (seed == 0) {
        seed = (uint32_t)time(NULL);
    }
    state->random_state = seed;
}
