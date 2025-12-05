/**
 * Carbon Multi-Track AI Decision System
 *
 * Parallel decision-making tracks that prevent resource competition between
 * different AI concerns. Each track operates independently with its own budget,
 * evaluator, and decision set.
 */

#include "carbon/carbon.h"
#include "carbon/ai_tracks.h"
#include "carbon/blackboard.h"
#include "carbon/error.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Individual track data
 */
typedef struct {
    char name[CARBON_AI_TRACKS_NAME_LEN];
    Carbon_AITrackType type;
    Carbon_AITrackEvaluator evaluator;
    void *userdata;
    bool used;
    bool enabled;

    /* Budgets */
    Carbon_AITrackBudget budgets[CARBON_AI_TRACKS_MAX_BUDGETS];
    int budget_count;

    /* Audit trail */
    char reason[CARBON_AI_TRACKS_REASON_LEN];

    /* Statistics */
    Carbon_AITrackStats stats;
} AITrack;

/**
 * Track system internal structure
 */
struct Carbon_AITrackSystem {
    AITrack tracks[CARBON_AI_TRACKS_MAX];
    int track_count;

    /* Blackboard for coordination */
    Carbon_Blackboard *blackboard;

    /* Filter callback */
    Carbon_AITrackFilter filter;
    void *filter_userdata;

    /* Budget provider callback */
    Carbon_AITrackBudgetProvider budget_provider;
    void *budget_provider_userdata;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find track by ID
 */
static AITrack *get_track(Carbon_AITrackSystem *tracks, int track_id) {
    if (!tracks || track_id < 0 || track_id >= CARBON_AI_TRACKS_MAX) {
        return NULL;
    }
    if (!tracks->tracks[track_id].used) {
        return NULL;
    }
    return &tracks->tracks[track_id];
}

/**
 * Find track by ID (const version)
 */
static const AITrack *get_track_const(const Carbon_AITrackSystem *tracks, int track_id) {
    if (!tracks || track_id < 0 || track_id >= CARBON_AI_TRACKS_MAX) {
        return NULL;
    }
    if (!tracks->tracks[track_id].used) {
        return NULL;
    }
    return &tracks->tracks[track_id];
}

/**
 * Find or create budget entry for a track
 */
static Carbon_AITrackBudget *get_or_create_budget(AITrack *track, int32_t resource_type) {
    /* Find existing */
    for (int i = 0; i < track->budget_count; i++) {
        if (track->budgets[i].active && track->budgets[i].resource_type == resource_type) {
            return &track->budgets[i];
        }
    }

    /* Find free slot */
    if (track->budget_count >= CARBON_AI_TRACKS_MAX_BUDGETS) {
        carbon_set_error("carbon_ai_tracks: max budgets per track reached");
        return NULL;
    }

    Carbon_AITrackBudget *budget = &track->budgets[track->budget_count++];
    budget->resource_type = resource_type;
    budget->allocated = 0;
    budget->spent = 0;
    budget->reserved = 0;
    budget->active = true;
    return budget;
}

/**
 * Find budget entry (const version)
 */
static const Carbon_AITrackBudget *find_budget(const AITrack *track, int32_t resource_type) {
    for (int i = 0; i < track->budget_count; i++) {
        if (track->budgets[i].active && track->budgets[i].resource_type == resource_type) {
            return &track->budgets[i];
        }
    }
    return NULL;
}

/**
 * Compare decisions by score (for qsort)
 */
static int compare_by_score(const void *a, const void *b) {
    const Carbon_AITrackDecision *da = (const Carbon_AITrackDecision *)a;
    const Carbon_AITrackDecision *db = (const Carbon_AITrackDecision *)b;
    if (db->score > da->score) return 1;
    if (db->score < da->score) return -1;
    return 0;
}

/**
 * Compare decisions by priority then score (for qsort)
 */
static int compare_by_priority(const void *a, const void *b) {
    const Carbon_AITrackDecision *da = (const Carbon_AITrackDecision *)a;
    const Carbon_AITrackDecision *db = (const Carbon_AITrackDecision *)b;

    /* Higher priority first */
    if (db->priority != da->priority) {
        return (int)db->priority - (int)da->priority;
    }
    /* Then by score */
    if (db->score > da->score) return 1;
    if (db->score < da->score) return -1;
    return 0;
}

/*============================================================================
 * System Lifecycle
 *============================================================================*/

Carbon_AITrackSystem *carbon_ai_tracks_create(void) {
    Carbon_AITrackSystem *tracks = CARBON_ALLOC(Carbon_AITrackSystem);
    if (!tracks) {
        carbon_set_error("carbon_ai_tracks_create: allocation failed");
        return NULL;
    }
    return tracks;
}

void carbon_ai_tracks_destroy(Carbon_AITrackSystem *tracks) {
    if (tracks) {
        free(tracks);
    }
}

void carbon_ai_tracks_reset(Carbon_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        AITrack *track = &tracks->tracks[i];
        if (!track->used) continue;

        /* Reset budgets spent */
        for (int j = 0; j < track->budget_count; j++) {
            track->budgets[j].spent = 0;
            track->budgets[j].reserved = 0;
        }

        /* Clear reason */
        track->reason[0] = '\0';
    }
}

/*============================================================================
 * Blackboard Integration
 *============================================================================*/

void carbon_ai_tracks_set_blackboard(Carbon_AITrackSystem *tracks,
                                      Carbon_Blackboard *bb) {
    if (tracks) {
        tracks->blackboard = bb;
    }
}

Carbon_Blackboard *carbon_ai_tracks_get_blackboard(Carbon_AITrackSystem *tracks) {
    return tracks ? tracks->blackboard : NULL;
}

/*============================================================================
 * Track Registration
 *============================================================================*/

int carbon_ai_tracks_register(Carbon_AITrackSystem *tracks,
                               const char *name,
                               Carbon_AITrackEvaluator evaluator) {
    return carbon_ai_tracks_register_ex(tracks, name, CARBON_AI_TRACK_CUSTOM,
                                         evaluator, NULL);
}

int carbon_ai_tracks_register_ex(Carbon_AITrackSystem *tracks,
                                  const char *name,
                                  Carbon_AITrackType type,
                                  Carbon_AITrackEvaluator evaluator,
                                  void *userdata) {
    if (!tracks || !name || !evaluator) {
        carbon_set_error("carbon_ai_tracks_register: invalid parameters");
        return -1;
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        if (!tracks->tracks[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        carbon_set_error("carbon_ai_tracks_register: max tracks reached");
        return -1;
    }

    AITrack *track = &tracks->tracks[slot];
    memset(track, 0, sizeof(AITrack));

    strncpy(track->name, name, CARBON_AI_TRACKS_NAME_LEN - 1);
    track->name[CARBON_AI_TRACKS_NAME_LEN - 1] = '\0';
    track->type = type;
    track->evaluator = evaluator;
    track->userdata = userdata;
    track->used = true;
    track->enabled = true;

    tracks->track_count++;
    return slot;
}

void carbon_ai_tracks_unregister(Carbon_AITrackSystem *tracks, int track_id) {
    AITrack *track = get_track(tracks, track_id);
    if (!track) return;

    /* Release any blackboard reservations */
    if (tracks->blackboard) {
        carbon_blackboard_release_all(tracks->blackboard, track->name);
    }

    track->used = false;
    tracks->track_count--;
}

int carbon_ai_tracks_get_id(const Carbon_AITrackSystem *tracks, const char *name) {
    if (!tracks || !name) return -1;

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        if (tracks->tracks[i].used &&
            strcmp(tracks->tracks[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *carbon_ai_tracks_get_name(const Carbon_AITrackSystem *tracks, int track_id) {
    const AITrack *track = get_track_const(tracks, track_id);
    return track ? track->name : NULL;
}

int carbon_ai_tracks_count(const Carbon_AITrackSystem *tracks) {
    return tracks ? tracks->track_count : 0;
}

bool carbon_ai_tracks_is_enabled(const Carbon_AITrackSystem *tracks, int track_id) {
    const AITrack *track = get_track_const(tracks, track_id);
    return track ? track->enabled : false;
}

void carbon_ai_tracks_set_enabled(Carbon_AITrackSystem *tracks, int track_id, bool enabled) {
    AITrack *track = get_track(tracks, track_id);
    if (track) {
        track->enabled = enabled;
    }
}

/*============================================================================
 * Budget Management
 *============================================================================*/

void carbon_ai_tracks_set_budget(Carbon_AITrackSystem *tracks,
                                  int track_id,
                                  int32_t resource_type,
                                  int32_t amount) {
    AITrack *track = get_track(tracks, track_id);
    if (!track) return;

    Carbon_AITrackBudget *budget = get_or_create_budget(track, resource_type);
    if (budget) {
        budget->allocated = amount;
    }

    /* Reserve on blackboard if available */
    if (tracks->blackboard && amount > 0) {
        char resource_key[64];
        snprintf(resource_key, sizeof(resource_key), "resource_%d", resource_type);
        carbon_blackboard_reserve(tracks->blackboard, resource_key, amount, track->name);
    }
}

int32_t carbon_ai_tracks_get_budget(const Carbon_AITrackSystem *tracks,
                                     int track_id,
                                     int32_t resource_type) {
    const AITrack *track = get_track_const(tracks, track_id);
    if (!track) return 0;

    const Carbon_AITrackBudget *budget = find_budget(track, resource_type);
    return budget ? budget->allocated : 0;
}

int32_t carbon_ai_tracks_get_remaining(const Carbon_AITrackSystem *tracks,
                                        int track_id,
                                        int32_t resource_type) {
    const AITrack *track = get_track_const(tracks, track_id);
    if (!track) return 0;

    const Carbon_AITrackBudget *budget = find_budget(track, resource_type);
    if (!budget) return 0;

    return budget->allocated - budget->spent;
}

bool carbon_ai_tracks_spend_budget(Carbon_AITrackSystem *tracks,
                                    int track_id,
                                    int32_t resource_type,
                                    int32_t amount) {
    AITrack *track = get_track(tracks, track_id);
    if (!track || amount <= 0) return false;

    Carbon_AITrackBudget *budget = NULL;
    for (int i = 0; i < track->budget_count; i++) {
        if (track->budgets[i].active && track->budgets[i].resource_type == resource_type) {
            budget = &track->budgets[i];
            break;
        }
    }

    if (!budget) return false;

    int32_t remaining = budget->allocated - budget->spent;
    if (amount > remaining) return false;

    budget->spent += amount;
    track->stats.resources_spent += amount;
    return true;
}

void carbon_ai_tracks_reset_spent(Carbon_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        AITrack *track = &tracks->tracks[i];
        if (!track->used) continue;

        for (int j = 0; j < track->budget_count; j++) {
            track->budgets[j].spent = 0;
        }
    }
}

void carbon_ai_tracks_set_budget_provider(Carbon_AITrackSystem *tracks,
                                           Carbon_AITrackBudgetProvider provider,
                                           void *userdata) {
    if (!tracks) return;

    tracks->budget_provider = provider;
    tracks->budget_provider_userdata = userdata;
}

void carbon_ai_tracks_allocate_budgets(Carbon_AITrackSystem *tracks,
                                        void *game_state) {
    if (!tracks || !tracks->budget_provider) return;

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        AITrack *track = &tracks->tracks[i];
        if (!track->used || !track->enabled) continue;

        /* Query provider for each budget type */
        for (int j = 0; j < track->budget_count; j++) {
            if (!track->budgets[j].active) continue;

            int32_t amount = tracks->budget_provider(
                i,
                track->budgets[j].resource_type,
                game_state,
                tracks->budget_provider_userdata
            );

            track->budgets[j].allocated = amount;

            /* Update blackboard reservation */
            if (tracks->blackboard && amount > 0) {
                char resource_key[64];
                snprintf(resource_key, sizeof(resource_key), "resource_%d",
                         track->budgets[j].resource_type);
                carbon_blackboard_reserve(tracks->blackboard, resource_key,
                                          amount, track->name);
            }
        }
    }
}

/*============================================================================
 * Evaluation
 *============================================================================*/

void carbon_ai_tracks_evaluate_all(Carbon_AITrackSystem *tracks,
                                    void *game_state,
                                    Carbon_AITrackResult *out_result) {
    if (!tracks || !out_result) return;

    memset(out_result, 0, sizeof(Carbon_AITrackResult));

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        AITrack *track = &tracks->tracks[i];
        if (!track->used || !track->enabled) continue;

        Carbon_AITrackDecisionSet *set = &out_result->decisions[out_result->track_count];
        carbon_ai_tracks_evaluate(tracks, i, game_state, set);

        if (set->count > 0) {
            out_result->total_decisions += set->count;
            out_result->total_score += set->total_score;
        }

        out_result->track_count++;
    }
}

void carbon_ai_tracks_evaluate(Carbon_AITrackSystem *tracks,
                                int track_id,
                                void *game_state,
                                Carbon_AITrackDecisionSet *out_set) {
    if (!tracks || !out_set) return;

    memset(out_set, 0, sizeof(Carbon_AITrackDecisionSet));

    AITrack *track = get_track(tracks, track_id);
    if (!track || !track->enabled || !track->evaluator) return;

    out_set->track_id = track_id;
    strncpy(out_set->track_name, track->name, CARBON_AI_TRACKS_NAME_LEN - 1);
    strncpy(out_set->reason, track->reason, CARBON_AI_TRACKS_REASON_LEN - 1);

    /* Call evaluator */
    track->evaluator(
        track_id,
        game_state,
        track->budgets,
        track->budget_count,
        out_set->items,
        &out_set->count,
        CARBON_AI_TRACKS_MAX_DECISIONS,
        track->userdata
    );

    /* Apply filter if set */
    if (tracks->filter && out_set->count > 0) {
        int new_count = 0;
        for (int i = 0; i < out_set->count; i++) {
            if (tracks->filter(track_id, &out_set->items[i], game_state,
                               tracks->filter_userdata)) {
                if (i != new_count) {
                    out_set->items[new_count] = out_set->items[i];
                }
                new_count++;
            }
        }
        out_set->count = new_count;
    }

    /* Calculate total score */
    out_set->total_score = 0.0f;
    for (int i = 0; i < out_set->count; i++) {
        out_set->total_score += out_set->items[i].score;
    }

    /* Update statistics */
    track->stats.evaluations++;
    track->stats.decisions_made += out_set->count;
    if (out_set->count > 0) {
        track->stats.avg_score = out_set->total_score / (float)out_set->count;
    }
}

void carbon_ai_tracks_set_filter(Carbon_AITrackSystem *tracks,
                                  Carbon_AITrackFilter filter,
                                  void *userdata) {
    if (!tracks) return;

    tracks->filter = filter;
    tracks->filter_userdata = userdata;
}

void carbon_ai_tracks_sort_decisions(Carbon_AITrackDecisionSet *set) {
    if (!set || set->count <= 1) return;

    qsort(set->items, set->count, sizeof(Carbon_AITrackDecision), compare_by_score);
}

void carbon_ai_tracks_sort_by_priority(Carbon_AITrackDecisionSet *set) {
    if (!set || set->count <= 1) return;

    qsort(set->items, set->count, sizeof(Carbon_AITrackDecision), compare_by_priority);
}

/*============================================================================
 * Decision Queries
 *============================================================================*/

const Carbon_AITrackDecision *carbon_ai_tracks_get_best(const Carbon_AITrackSystem *tracks,
                                                         int track_id,
                                                         const Carbon_AITrackResult *result) {
    if (!tracks || !result) return NULL;

    /* Find the track in results */
    for (int i = 0; i < result->track_count; i++) {
        if (result->decisions[i].track_id == track_id) {
            const Carbon_AITrackDecisionSet *set = &result->decisions[i];
            if (set->count == 0) return NULL;

            /* Find highest scored decision */
            const Carbon_AITrackDecision *best = &set->items[0];
            for (int j = 1; j < set->count; j++) {
                if (set->items[j].score > best->score) {
                    best = &set->items[j];
                }
            }
            return best;
        }
    }
    return NULL;
}

int carbon_ai_tracks_get_by_type(const Carbon_AITrackResult *result,
                                  int32_t action_type,
                                  const Carbon_AITrackDecision **out,
                                  int max) {
    if (!result || !out || max <= 0) return 0;

    int count = 0;
    for (int t = 0; t < result->track_count && count < max; t++) {
        const Carbon_AITrackDecisionSet *set = &result->decisions[t];
        for (int i = 0; i < set->count && count < max; i++) {
            if (set->items[i].action_type == action_type) {
                out[count++] = &set->items[i];
            }
        }
    }
    return count;
}

int carbon_ai_tracks_get_above_score(const Carbon_AITrackResult *result,
                                      float min_score,
                                      const Carbon_AITrackDecision **out,
                                      int max) {
    if (!result || !out || max <= 0) return 0;

    int count = 0;
    for (int t = 0; t < result->track_count && count < max; t++) {
        const Carbon_AITrackDecisionSet *set = &result->decisions[t];
        for (int i = 0; i < set->count && count < max; i++) {
            if (set->items[i].score >= min_score) {
                out[count++] = &set->items[i];
            }
        }
    }
    return count;
}

int carbon_ai_tracks_get_all_sorted(const Carbon_AITrackResult *result,
                                     const Carbon_AITrackDecision **out,
                                     int max) {
    if (!result || !out || max <= 0) return 0;

    /* Collect all decisions */
    int count = 0;
    for (int t = 0; t < result->track_count && count < max; t++) {
        const Carbon_AITrackDecisionSet *set = &result->decisions[t];
        for (int i = 0; i < set->count && count < max; i++) {
            out[count++] = &set->items[i];
        }
    }

    /* Sort by score (simple bubble sort for small arrays) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j]->score > out[i]->score) {
                const Carbon_AITrackDecision *tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }

    return count;
}

/*============================================================================
 * Audit Trail
 *============================================================================*/

void carbon_ai_tracks_set_reason(Carbon_AITrackSystem *tracks,
                                  int track_id,
                                  const char *fmt, ...) {
    AITrack *track = get_track(tracks, track_id);
    if (!track || !fmt) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(track->reason, CARBON_AI_TRACKS_REASON_LEN, fmt, args);
    va_end(args);

    /* Also log to blackboard if available */
    if (tracks->blackboard) {
        carbon_blackboard_log(tracks->blackboard, "[%s] %s", track->name, track->reason);
    }
}

const char *carbon_ai_tracks_get_reason(const Carbon_AITrackSystem *tracks,
                                         int track_id) {
    const AITrack *track = get_track_const(tracks, track_id);
    return track ? track->reason : "";
}

void carbon_ai_tracks_clear_reasons(Carbon_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        tracks->tracks[i].reason[0] = '\0';
    }
}

/*============================================================================
 * Statistics
 *============================================================================*/

void carbon_ai_tracks_get_stats(const Carbon_AITrackSystem *tracks,
                                 int track_id,
                                 Carbon_AITrackStats *out) {
    if (!out) return;

    memset(out, 0, sizeof(Carbon_AITrackStats));

    const AITrack *track = get_track_const(tracks, track_id);
    if (!track) return;

    *out = track->stats;

    /* Calculate success rate */
    if (out->decisions_made > 0) {
        out->success_rate = (float)out->decisions_executed / (float)out->decisions_made;
    }
}

void carbon_ai_tracks_record_execution(Carbon_AITrackSystem *tracks, int track_id) {
    AITrack *track = get_track(tracks, track_id);
    if (track) {
        track->stats.decisions_executed++;
    }
}

void carbon_ai_tracks_reset_stats(Carbon_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < CARBON_AI_TRACKS_MAX; i++) {
        memset(&tracks->tracks[i].stats, 0, sizeof(Carbon_AITrackStats));
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_ai_track_type_name(Carbon_AITrackType type) {
    switch (type) {
        case CARBON_AI_TRACK_ECONOMY:        return "Economy";
        case CARBON_AI_TRACK_MILITARY:       return "Military";
        case CARBON_AI_TRACK_RESEARCH:       return "Research";
        case CARBON_AI_TRACK_DIPLOMACY:      return "Diplomacy";
        case CARBON_AI_TRACK_EXPANSION:      return "Expansion";
        case CARBON_AI_TRACK_INFRASTRUCTURE: return "Infrastructure";
        case CARBON_AI_TRACK_ESPIONAGE:      return "Espionage";
        case CARBON_AI_TRACK_CUSTOM:         return "Custom";
        default:
            if (type >= CARBON_AI_TRACK_USER) return "User";
            return "Unknown";
    }
}

const char *carbon_ai_priority_name(Carbon_AIDecisionPriority priority) {
    switch (priority) {
        case CARBON_AI_PRIORITY_LOW:      return "Low";
        case CARBON_AI_PRIORITY_NORMAL:   return "Normal";
        case CARBON_AI_PRIORITY_HIGH:     return "High";
        case CARBON_AI_PRIORITY_CRITICAL: return "Critical";
        default:                          return "Unknown";
    }
}

void carbon_ai_track_decision_init(Carbon_AITrackDecision *decision) {
    if (!decision) return;

    memset(decision, 0, sizeof(Carbon_AITrackDecision));
    decision->action_type = -1;
    decision->target_id = -1;
    decision->secondary_id = -1;
    decision->resource_type = -1;
    decision->priority = CARBON_AI_PRIORITY_NORMAL;
}

void carbon_ai_track_decision_copy(Carbon_AITrackDecision *dest,
                                    const Carbon_AITrackDecision *src) {
    if (!dest || !src) return;

    *dest = *src;
    /* Note: userdata pointer is copied, not the data it points to */
}
