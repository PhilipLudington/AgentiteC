/**
 * Carbon Multi-Track AI Decision System
 *
 * Parallel decision-making tracks that prevent resource competition between
 * different AI concerns. Each track operates independently with its own budget,
 * evaluator, and decision set.
 */

#include "agentite/agentite.h"
#include "agentite/ai_tracks.h"
#include "agentite/blackboard.h"
#include "agentite/error.h"

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
    char name[AGENTITE_AI_TRACKS_NAME_LEN];
    Agentite_AITrackType type;
    Agentite_AITrackEvaluator evaluator;
    void *userdata;
    bool used;
    bool enabled;

    /* Budgets */
    Agentite_AITrackBudget budgets[AGENTITE_AI_TRACKS_MAX_BUDGETS];
    int budget_count;

    /* Audit trail */
    char reason[AGENTITE_AI_TRACKS_REASON_LEN];

    /* Statistics */
    Agentite_AITrackStats stats;
} AITrack;

/**
 * Track system internal structure
 */
struct Agentite_AITrackSystem {
    AITrack tracks[AGENTITE_AI_TRACKS_MAX];
    int track_count;

    /* Blackboard for coordination */
    Agentite_Blackboard *blackboard;

    /* Filter callback */
    Agentite_AITrackFilter filter;
    void *filter_userdata;

    /* Budget provider callback */
    Agentite_AITrackBudgetProvider budget_provider;
    void *budget_provider_userdata;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find track by ID
 */
static AITrack *get_track(Agentite_AITrackSystem *tracks, int track_id) {
    if (!tracks || track_id < 0 || track_id >= AGENTITE_AI_TRACKS_MAX) {
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
static const AITrack *get_track_const(const Agentite_AITrackSystem *tracks, int track_id) {
    if (!tracks || track_id < 0 || track_id >= AGENTITE_AI_TRACKS_MAX) {
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
static Agentite_AITrackBudget *get_or_create_budget(AITrack *track, int32_t resource_type) {
    /* Find existing */
    for (int i = 0; i < track->budget_count; i++) {
        if (track->budgets[i].active && track->budgets[i].resource_type == resource_type) {
            return &track->budgets[i];
        }
    }

    /* Find free slot */
    if (track->budget_count >= AGENTITE_AI_TRACKS_MAX_BUDGETS) {
        agentite_set_error("AI Tracks: Maximum budgets per track reached (%d/%d)", track->budget_count, AGENTITE_AI_TRACKS_MAX_BUDGETS);
        return NULL;
    }

    Agentite_AITrackBudget *budget = &track->budgets[track->budget_count++];
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
static const Agentite_AITrackBudget *find_budget(const AITrack *track, int32_t resource_type) {
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
    const Agentite_AITrackDecision *da = (const Agentite_AITrackDecision *)a;
    const Agentite_AITrackDecision *db = (const Agentite_AITrackDecision *)b;
    if (db->score > da->score) return 1;
    if (db->score < da->score) return -1;
    return 0;
}

/**
 * Compare decisions by priority then score (for qsort)
 */
static int compare_by_priority(const void *a, const void *b) {
    const Agentite_AITrackDecision *da = (const Agentite_AITrackDecision *)a;
    const Agentite_AITrackDecision *db = (const Agentite_AITrackDecision *)b;

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

Agentite_AITrackSystem *agentite_ai_tracks_create(void) {
    Agentite_AITrackSystem *tracks = AGENTITE_ALLOC(Agentite_AITrackSystem);
    if (!tracks) {
        agentite_set_error("agentite_ai_tracks_create: allocation failed");
        return NULL;
    }
    return tracks;
}

void agentite_ai_tracks_destroy(Agentite_AITrackSystem *tracks) {
    if (tracks) {
        free(tracks);
    }
}

void agentite_ai_tracks_reset(Agentite_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
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

void agentite_ai_tracks_set_blackboard(Agentite_AITrackSystem *tracks,
                                      Agentite_Blackboard *bb) {
    if (tracks) {
        tracks->blackboard = bb;
    }
}

Agentite_Blackboard *agentite_ai_tracks_get_blackboard(Agentite_AITrackSystem *tracks) {
    return tracks ? tracks->blackboard : NULL;
}

/*============================================================================
 * Track Registration
 *============================================================================*/

int agentite_ai_tracks_register(Agentite_AITrackSystem *tracks,
                               const char *name,
                               Agentite_AITrackEvaluator evaluator) {
    return agentite_ai_tracks_register_ex(tracks, name, AGENTITE_AI_TRACK_CUSTOM,
                                         evaluator, NULL);
}

int agentite_ai_tracks_register_ex(Agentite_AITrackSystem *tracks,
                                  const char *name,
                                  Agentite_AITrackType type,
                                  Agentite_AITrackEvaluator evaluator,
                                  void *userdata) {
    if (!tracks || !name || !evaluator) {
        agentite_set_error("agentite_ai_tracks_register: invalid parameters");
        return -1;
    }

    /* Find free slot */
    int slot = -1;
    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
        if (!tracks->tracks[i].used) {
            slot = i;
            break;
        }
    }

    if (slot < 0) {
        agentite_set_error("AI Tracks: Maximum tracks reached (%d/%d) when registering '%s'", tracks->track_count, AGENTITE_AI_TRACKS_MAX, name);
        return -1;
    }

    AITrack *track = &tracks->tracks[slot];
    memset(track, 0, sizeof(AITrack));

    strncpy(track->name, name, AGENTITE_AI_TRACKS_NAME_LEN - 1);
    track->name[AGENTITE_AI_TRACKS_NAME_LEN - 1] = '\0';
    track->type = type;
    track->evaluator = evaluator;
    track->userdata = userdata;
    track->used = true;
    track->enabled = true;

    tracks->track_count++;
    return slot;
}

void agentite_ai_tracks_unregister(Agentite_AITrackSystem *tracks, int track_id) {
    AITrack *track = get_track(tracks, track_id);
    if (!track) return;

    /* Release any blackboard reservations */
    if (tracks->blackboard) {
        agentite_blackboard_release_all(tracks->blackboard, track->name);
    }

    track->used = false;
    tracks->track_count--;
}

int agentite_ai_tracks_get_id(const Agentite_AITrackSystem *tracks, const char *name) {
    if (!tracks || !name) return -1;

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
        if (tracks->tracks[i].used &&
            strcmp(tracks->tracks[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

const char *agentite_ai_tracks_get_name(const Agentite_AITrackSystem *tracks, int track_id) {
    const AITrack *track = get_track_const(tracks, track_id);
    return track ? track->name : NULL;
}

int agentite_ai_tracks_count(const Agentite_AITrackSystem *tracks) {
    return tracks ? tracks->track_count : 0;
}

bool agentite_ai_tracks_is_enabled(const Agentite_AITrackSystem *tracks, int track_id) {
    const AITrack *track = get_track_const(tracks, track_id);
    return track ? track->enabled : false;
}

void agentite_ai_tracks_set_enabled(Agentite_AITrackSystem *tracks, int track_id, bool enabled) {
    AITrack *track = get_track(tracks, track_id);
    if (track) {
        track->enabled = enabled;
    }
}

/*============================================================================
 * Budget Management
 *============================================================================*/

void agentite_ai_tracks_set_budget(Agentite_AITrackSystem *tracks,
                                  int track_id,
                                  int32_t resource_type,
                                  int32_t amount) {
    AITrack *track = get_track(tracks, track_id);
    if (!track) return;

    Agentite_AITrackBudget *budget = get_or_create_budget(track, resource_type);
    if (budget) {
        budget->allocated = amount;
    }

    /* Reserve on blackboard if available */
    if (tracks->blackboard && amount > 0) {
        char resource_key[64];
        snprintf(resource_key, sizeof(resource_key), "resource_%d", resource_type);
        agentite_blackboard_reserve(tracks->blackboard, resource_key, amount, track->name);
    }
}

int32_t agentite_ai_tracks_get_budget(const Agentite_AITrackSystem *tracks,
                                     int track_id,
                                     int32_t resource_type) {
    const AITrack *track = get_track_const(tracks, track_id);
    if (!track) return 0;

    const Agentite_AITrackBudget *budget = find_budget(track, resource_type);
    return budget ? budget->allocated : 0;
}

int32_t agentite_ai_tracks_get_remaining(const Agentite_AITrackSystem *tracks,
                                        int track_id,
                                        int32_t resource_type) {
    const AITrack *track = get_track_const(tracks, track_id);
    if (!track) return 0;

    const Agentite_AITrackBudget *budget = find_budget(track, resource_type);
    if (!budget) return 0;

    return budget->allocated - budget->spent;
}

bool agentite_ai_tracks_spend_budget(Agentite_AITrackSystem *tracks,
                                    int track_id,
                                    int32_t resource_type,
                                    int32_t amount) {
    AITrack *track = get_track(tracks, track_id);
    if (!track || amount <= 0) return false;

    Agentite_AITrackBudget *budget = NULL;
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

void agentite_ai_tracks_reset_spent(Agentite_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
        AITrack *track = &tracks->tracks[i];
        if (!track->used) continue;

        for (int j = 0; j < track->budget_count; j++) {
            track->budgets[j].spent = 0;
        }
    }
}

void agentite_ai_tracks_set_budget_provider(Agentite_AITrackSystem *tracks,
                                           Agentite_AITrackBudgetProvider provider,
                                           void *userdata) {
    if (!tracks) return;

    tracks->budget_provider = provider;
    tracks->budget_provider_userdata = userdata;
}

void agentite_ai_tracks_allocate_budgets(Agentite_AITrackSystem *tracks,
                                        void *game_state) {
    if (!tracks || !tracks->budget_provider) return;

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
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
                agentite_blackboard_reserve(tracks->blackboard, resource_key,
                                          amount, track->name);
            }
        }
    }
}

/*============================================================================
 * Evaluation
 *============================================================================*/

void agentite_ai_tracks_evaluate_all(Agentite_AITrackSystem *tracks,
                                    void *game_state,
                                    Agentite_AITrackResult *out_result) {
    if (!tracks || !out_result) return;

    memset(out_result, 0, sizeof(Agentite_AITrackResult));

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
        AITrack *track = &tracks->tracks[i];
        if (!track->used || !track->enabled) continue;

        Agentite_AITrackDecisionSet *set = &out_result->decisions[out_result->track_count];
        agentite_ai_tracks_evaluate(tracks, i, game_state, set);

        if (set->count > 0) {
            out_result->total_decisions += set->count;
            out_result->total_score += set->total_score;
        }

        out_result->track_count++;
    }
}

void agentite_ai_tracks_evaluate(Agentite_AITrackSystem *tracks,
                                int track_id,
                                void *game_state,
                                Agentite_AITrackDecisionSet *out_set) {
    if (!tracks || !out_set) return;

    memset(out_set, 0, sizeof(Agentite_AITrackDecisionSet));

    AITrack *track = get_track(tracks, track_id);
    if (!track || !track->enabled || !track->evaluator) return;

    out_set->track_id = track_id;
    strncpy(out_set->track_name, track->name, AGENTITE_AI_TRACKS_NAME_LEN - 1);
    strncpy(out_set->reason, track->reason, AGENTITE_AI_TRACKS_REASON_LEN - 1);

    /* Call evaluator */
    track->evaluator(
        track_id,
        game_state,
        track->budgets,
        track->budget_count,
        out_set->items,
        &out_set->count,
        AGENTITE_AI_TRACKS_MAX_DECISIONS,
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

void agentite_ai_tracks_set_filter(Agentite_AITrackSystem *tracks,
                                  Agentite_AITrackFilter filter,
                                  void *userdata) {
    if (!tracks) return;

    tracks->filter = filter;
    tracks->filter_userdata = userdata;
}

void agentite_ai_tracks_sort_decisions(Agentite_AITrackDecisionSet *set) {
    if (!set || set->count <= 1) return;

    qsort(set->items, set->count, sizeof(Agentite_AITrackDecision), compare_by_score);
}

void agentite_ai_tracks_sort_by_priority(Agentite_AITrackDecisionSet *set) {
    if (!set || set->count <= 1) return;

    qsort(set->items, set->count, sizeof(Agentite_AITrackDecision), compare_by_priority);
}

/*============================================================================
 * Decision Queries
 *============================================================================*/

const Agentite_AITrackDecision *agentite_ai_tracks_get_best(const Agentite_AITrackSystem *tracks,
                                                         int track_id,
                                                         const Agentite_AITrackResult *result) {
    if (!tracks || !result) return NULL;

    /* Find the track in results */
    for (int i = 0; i < result->track_count; i++) {
        if (result->decisions[i].track_id == track_id) {
            const Agentite_AITrackDecisionSet *set = &result->decisions[i];
            if (set->count == 0) return NULL;

            /* Find highest scored decision */
            const Agentite_AITrackDecision *best = &set->items[0];
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

int agentite_ai_tracks_get_by_type(const Agentite_AITrackResult *result,
                                  int32_t action_type,
                                  const Agentite_AITrackDecision **out,
                                  int max) {
    if (!result || !out || max <= 0) return 0;

    int count = 0;
    for (int t = 0; t < result->track_count && count < max; t++) {
        const Agentite_AITrackDecisionSet *set = &result->decisions[t];
        for (int i = 0; i < set->count && count < max; i++) {
            if (set->items[i].action_type == action_type) {
                out[count++] = &set->items[i];
            }
        }
    }
    return count;
}

int agentite_ai_tracks_get_above_score(const Agentite_AITrackResult *result,
                                      float min_score,
                                      const Agentite_AITrackDecision **out,
                                      int max) {
    if (!result || !out || max <= 0) return 0;

    int count = 0;
    for (int t = 0; t < result->track_count && count < max; t++) {
        const Agentite_AITrackDecisionSet *set = &result->decisions[t];
        for (int i = 0; i < set->count && count < max; i++) {
            if (set->items[i].score >= min_score) {
                out[count++] = &set->items[i];
            }
        }
    }
    return count;
}

int agentite_ai_tracks_get_all_sorted(const Agentite_AITrackResult *result,
                                     const Agentite_AITrackDecision **out,
                                     int max) {
    if (!result || !out || max <= 0) return 0;

    /* Collect all decisions */
    int count = 0;
    for (int t = 0; t < result->track_count && count < max; t++) {
        const Agentite_AITrackDecisionSet *set = &result->decisions[t];
        for (int i = 0; i < set->count && count < max; i++) {
            out[count++] = &set->items[i];
        }
    }

    /* Sort by score (simple bubble sort for small arrays) */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out[j]->score > out[i]->score) {
                const Agentite_AITrackDecision *tmp = out[i];
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

void agentite_ai_tracks_set_reason(Agentite_AITrackSystem *tracks,
                                  int track_id,
                                  const char *fmt, ...) {
    AITrack *track = get_track(tracks, track_id);
    if (!track || !fmt) return;

    va_list args;
    va_start(args, fmt);
    vsnprintf(track->reason, AGENTITE_AI_TRACKS_REASON_LEN, fmt, args);
    va_end(args);

    /* Also log to blackboard if available */
    if (tracks->blackboard) {
        agentite_blackboard_log(tracks->blackboard, "[%s] %s", track->name, track->reason);
    }
}

const char *agentite_ai_tracks_get_reason(const Agentite_AITrackSystem *tracks,
                                         int track_id) {
    const AITrack *track = get_track_const(tracks, track_id);
    return track ? track->reason : "";
}

void agentite_ai_tracks_clear_reasons(Agentite_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
        tracks->tracks[i].reason[0] = '\0';
    }
}

/*============================================================================
 * Statistics
 *============================================================================*/

void agentite_ai_tracks_get_stats(const Agentite_AITrackSystem *tracks,
                                 int track_id,
                                 Agentite_AITrackStats *out) {
    if (!out) return;

    memset(out, 0, sizeof(Agentite_AITrackStats));

    const AITrack *track = get_track_const(tracks, track_id);
    if (!track) return;

    *out = track->stats;

    /* Calculate success rate */
    if (out->decisions_made > 0) {
        out->success_rate = (float)out->decisions_executed / (float)out->decisions_made;
    }
}

void agentite_ai_tracks_record_execution(Agentite_AITrackSystem *tracks, int track_id) {
    AITrack *track = get_track(tracks, track_id);
    if (track) {
        track->stats.decisions_executed++;
    }
}

void agentite_ai_tracks_reset_stats(Agentite_AITrackSystem *tracks) {
    if (!tracks) return;

    for (int i = 0; i < AGENTITE_AI_TRACKS_MAX; i++) {
        memset(&tracks->tracks[i].stats, 0, sizeof(Agentite_AITrackStats));
    }
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_ai_track_type_name(Agentite_AITrackType type) {
    switch (type) {
        case AGENTITE_AI_TRACK_ECONOMY:        return "Economy";
        case AGENTITE_AI_TRACK_MILITARY:       return "Military";
        case AGENTITE_AI_TRACK_RESEARCH:       return "Research";
        case AGENTITE_AI_TRACK_DIPLOMACY:      return "Diplomacy";
        case AGENTITE_AI_TRACK_EXPANSION:      return "Expansion";
        case AGENTITE_AI_TRACK_INFRASTRUCTURE: return "Infrastructure";
        case AGENTITE_AI_TRACK_ESPIONAGE:      return "Espionage";
        case AGENTITE_AI_TRACK_CUSTOM:         return "Custom";
        default:
            if (type >= AGENTITE_AI_TRACK_USER) return "User";
            return "Unknown";
    }
}

const char *agentite_ai_priority_name(Agentite_AIDecisionPriority priority) {
    switch (priority) {
        case AGENTITE_AI_PRIORITY_LOW:      return "Low";
        case AGENTITE_AI_PRIORITY_NORMAL:   return "Normal";
        case AGENTITE_AI_PRIORITY_HIGH:     return "High";
        case AGENTITE_AI_PRIORITY_CRITICAL: return "Critical";
        default:                          return "Unknown";
    }
}

void agentite_ai_track_decision_init(Agentite_AITrackDecision *decision) {
    if (!decision) return;

    memset(decision, 0, sizeof(Agentite_AITrackDecision));
    decision->action_type = -1;
    decision->target_id = -1;
    decision->secondary_id = -1;
    decision->resource_type = -1;
    decision->priority = AGENTITE_AI_PRIORITY_NORMAL;
}

void agentite_ai_track_decision_copy(Agentite_AITrackDecision *dest,
                                    const Agentite_AITrackDecision *src) {
    if (!dest || !src) return;

    *dest = *src;
    /* Note: userdata pointer is copied, not the data it points to */
}
