#ifndef AGENTITE_AI_TRACKS_H
#define AGENTITE_AI_TRACKS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Multi-Track AI Decision System
 *
 * Parallel decision-making tracks that prevent resource competition between
 * different AI concerns. Each track operates independently with its own budget,
 * evaluator, and decision set.
 *
 * Usage:
 *   // Create track system with blackboard for coordination
 *   Agentite_AITrackSystem *tracks = agentite_ai_tracks_create();
 *   agentite_ai_tracks_set_blackboard(tracks, blackboard);
 *
 *   // Register tracks
 *   int econ_track = agentite_ai_tracks_register(tracks, "economy", evaluate_economy);
 *   int mil_track = agentite_ai_tracks_register(tracks, "military", evaluate_military);
 *
 *   // Set budgets per resource type
 *   agentite_ai_tracks_set_budget(tracks, econ_track, RESOURCE_GOLD, 1000);
 *   agentite_ai_tracks_set_budget(tracks, mil_track, RESOURCE_GOLD, 500);
 *
 *   // Each turn, evaluate all tracks
 *   Agentite_AITrackResult results;
 *   agentite_ai_tracks_evaluate_all(tracks, game_state, &results);
 *
 *   // Execute decisions from each track
 *   for (int i = 0; i < results.track_count; i++) {
 *       for (int j = 0; j < results.decisions[i].count; j++) {
 *           execute_decision(&results.decisions[i].items[j]);
 *       }
 *   }
 *
 *   // Cleanup
 *   agentite_ai_tracks_destroy(tracks);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_AI_TRACKS_MAX          8    /* Maximum registered tracks */
#define AGENTITE_AI_TRACKS_MAX_BUDGETS  8    /* Maximum budget types per track */
#define AGENTITE_AI_TRACKS_MAX_DECISIONS 16  /* Maximum decisions per track */
#define AGENTITE_AI_TRACKS_NAME_LEN     32   /* Maximum track name length */
#define AGENTITE_AI_TRACKS_REASON_LEN   128  /* Maximum reason string length */

/*============================================================================
 * Predefined Track Types
 *============================================================================*/

/**
 * Built-in track type identifiers for common AI concerns.
 * Games can use these or define their own starting at AGENTITE_AI_TRACK_USER.
 */
typedef enum Agentite_AITrackType {
    AGENTITE_AI_TRACK_ECONOMY = 0,    /* Resource production, expansion */
    AGENTITE_AI_TRACK_MILITARY,       /* Unit production, defense */
    AGENTITE_AI_TRACK_RESEARCH,       /* Technology priorities */
    AGENTITE_AI_TRACK_DIPLOMACY,      /* Relations, treaties */
    AGENTITE_AI_TRACK_EXPANSION,      /* Territory growth */
    AGENTITE_AI_TRACK_INFRASTRUCTURE, /* Building, improvements */
    AGENTITE_AI_TRACK_ESPIONAGE,      /* Intelligence, sabotage */
    AGENTITE_AI_TRACK_CUSTOM,         /* Game-specific track */
    AGENTITE_AI_TRACK_COUNT,

    /* User-defined track types start here */
    AGENTITE_AI_TRACK_USER = 100,
} Agentite_AITrackType;

/*============================================================================
 * Decision Priority Levels
 *============================================================================*/

typedef enum Agentite_AIDecisionPriority {
    AGENTITE_AI_PRIORITY_LOW = 0,
    AGENTITE_AI_PRIORITY_NORMAL,
    AGENTITE_AI_PRIORITY_HIGH,
    AGENTITE_AI_PRIORITY_CRITICAL,
    AGENTITE_AI_PRIORITY_COUNT,
} Agentite_AIDecisionPriority;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * A single decision from a track
 */
typedef struct Agentite_AITrackDecision {
    int32_t action_type;            /* Game-defined action type */
    int32_t target_id;              /* Target entity/location/faction */
    int32_t secondary_id;           /* Secondary parameter */
    float score;                    /* Decision score (higher = better) */
    Agentite_AIDecisionPriority priority;
    int32_t resource_type;          /* Resource to spend (-1 = none) */
    int32_t resource_cost;          /* Cost of this decision */
    void *userdata;                 /* Game-specific data (not owned) */
} Agentite_AITrackDecision;

/**
 * Budget allocation for a single resource type
 */
typedef struct Agentite_AITrackBudget {
    int32_t resource_type;          /* Resource identifier */
    int32_t allocated;              /* Total allocated to this track */
    int32_t spent;                  /* Amount spent this turn */
    int32_t reserved;               /* Amount reserved on blackboard */
    bool active;                    /* Whether this budget slot is used */
} Agentite_AITrackBudget;

/**
 * Collection of decisions from a single track
 */
typedef struct Agentite_AITrackDecisionSet {
    Agentite_AITrackDecision items[AGENTITE_AI_TRACKS_MAX_DECISIONS];
    int count;
    int track_id;
    char track_name[AGENTITE_AI_TRACKS_NAME_LEN];
    char reason[AGENTITE_AI_TRACKS_REASON_LEN];  /* Audit trail */
    float total_score;              /* Sum of all decision scores */
} Agentite_AITrackDecisionSet;

/**
 * Results from evaluating all tracks
 */
typedef struct Agentite_AITrackResult {
    Agentite_AITrackDecisionSet decisions[AGENTITE_AI_TRACKS_MAX];
    int track_count;
    int total_decisions;            /* Sum across all tracks */
    float total_score;              /* Sum of all scores */
} Agentite_AITrackResult;

/**
 * Track statistics for debugging/UI
 */
typedef struct Agentite_AITrackStats {
    int evaluations;                /* Times evaluated */
    int decisions_made;             /* Total decisions generated */
    int decisions_executed;         /* Decisions that were executed */
    int32_t resources_spent;        /* Total resources spent */
    float avg_score;                /* Average decision score */
    float success_rate;             /* Executed / made ratio */
} Agentite_AITrackStats;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Track evaluator function.
 * Called during evaluation to generate decisions for this track.
 *
 * @param track_id    ID of this track
 * @param game_state  Game-specific context
 * @param budgets     Array of budgets for this track
 * @param budget_count Number of budget entries
 * @param out_decisions Output array for decisions
 * @param out_count   Output: number of decisions generated
 * @param max_decisions Maximum decisions to generate
 * @param userdata    Track-specific userdata
 */
typedef void (*Agentite_AITrackEvaluator)(int track_id,
                                         void *game_state,
                                         const Agentite_AITrackBudget *budgets,
                                         int budget_count,
                                         Agentite_AITrackDecision *out_decisions,
                                         int *out_count,
                                         int max_decisions,
                                         void *userdata);

/**
 * Decision filter callback.
 * Called to validate/filter decisions before they are finalized.
 *
 * @param track_id    ID of the track
 * @param decision    Decision to validate
 * @param game_state  Game-specific context
 * @param userdata    Filter userdata
 * @return true if decision should be kept
 */
typedef bool (*Agentite_AITrackFilter)(int track_id,
                                      const Agentite_AITrackDecision *decision,
                                      void *game_state,
                                      void *userdata);

/**
 * Budget provider callback.
 * Called to determine budget allocation at start of evaluation.
 *
 * @param track_id      ID of the track
 * @param resource_type Resource being budgeted
 * @param game_state    Game-specific context
 * @param userdata      Provider userdata
 * @return Budget amount for this resource
 */
typedef int32_t (*Agentite_AITrackBudgetProvider)(int track_id,
                                                  int32_t resource_type,
                                                  void *game_state,
                                                  void *userdata);

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Agentite_AITrackSystem Agentite_AITrackSystem;
typedef struct Agentite_Blackboard Agentite_Blackboard;

/*============================================================================
 * System Lifecycle
 *============================================================================*/

/**
 * Create a new track system.
 *
 * @return New track system or NULL on failure
 */
Agentite_AITrackSystem *agentite_ai_tracks_create(void);

/**
 * Destroy a track system and free resources.
 *
 * @param tracks Track system to destroy
 */
void agentite_ai_tracks_destroy(Agentite_AITrackSystem *tracks);

/**
 * Reset all tracks (clear budgets spent, statistics).
 *
 * @param tracks Track system
 */
void agentite_ai_tracks_reset(Agentite_AITrackSystem *tracks);

/*============================================================================
 * Blackboard Integration
 *============================================================================*/

/**
 * Set the blackboard for resource reservation coordination.
 *
 * @param tracks Track system
 * @param bb     Blackboard (not owned, must outlive tracks)
 */
void agentite_ai_tracks_set_blackboard(Agentite_AITrackSystem *tracks,
                                      Agentite_Blackboard *bb);

/**
 * Get the associated blackboard.
 *
 * @param tracks Track system
 * @return Blackboard or NULL
 */
Agentite_Blackboard *agentite_ai_tracks_get_blackboard(Agentite_AITrackSystem *tracks);

/*============================================================================
 * Track Registration
 *============================================================================*/

/**
 * Register a new track.
 *
 * @param tracks    Track system
 * @param name      Track name (for debugging/audit)
 * @param evaluator Evaluator function
 * @return Track ID (0+) or -1 on failure
 */
int agentite_ai_tracks_register(Agentite_AITrackSystem *tracks,
                               const char *name,
                               Agentite_AITrackEvaluator evaluator);

/**
 * Register a track with additional options.
 *
 * @param tracks    Track system
 * @param name      Track name
 * @param type      Track type (predefined or user)
 * @param evaluator Evaluator function
 * @param userdata  Track-specific userdata
 * @return Track ID (0+) or -1 on failure
 */
int agentite_ai_tracks_register_ex(Agentite_AITrackSystem *tracks,
                                  const char *name,
                                  Agentite_AITrackType type,
                                  Agentite_AITrackEvaluator evaluator,
                                  void *userdata);

/**
 * Unregister a track.
 *
 * @param tracks   Track system
 * @param track_id Track to unregister
 */
void agentite_ai_tracks_unregister(Agentite_AITrackSystem *tracks, int track_id);

/**
 * Get track ID by name.
 *
 * @param tracks Track system
 * @param name   Track name
 * @return Track ID or -1 if not found
 */
int agentite_ai_tracks_get_id(const Agentite_AITrackSystem *tracks, const char *name);

/**
 * Get track name by ID.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @return Track name or NULL
 */
const char *agentite_ai_tracks_get_name(const Agentite_AITrackSystem *tracks, int track_id);

/**
 * Get number of registered tracks.
 *
 * @param tracks Track system
 * @return Track count
 */
int agentite_ai_tracks_count(const Agentite_AITrackSystem *tracks);

/**
 * Check if a track is enabled.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @return true if enabled
 */
bool agentite_ai_tracks_is_enabled(const Agentite_AITrackSystem *tracks, int track_id);

/**
 * Enable or disable a track.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @param enabled  Whether track should be enabled
 */
void agentite_ai_tracks_set_enabled(Agentite_AITrackSystem *tracks, int track_id, bool enabled);

/*============================================================================
 * Budget Management
 *============================================================================*/

/**
 * Set budget for a track and resource type.
 *
 * @param tracks        Track system
 * @param track_id      Track ID
 * @param resource_type Resource identifier
 * @param amount        Budget amount
 */
void agentite_ai_tracks_set_budget(Agentite_AITrackSystem *tracks,
                                  int track_id,
                                  int32_t resource_type,
                                  int32_t amount);

/**
 * Get budget for a track and resource type.
 *
 * @param tracks        Track system
 * @param track_id      Track ID
 * @param resource_type Resource identifier
 * @return Budget amount (0 if not set)
 */
int32_t agentite_ai_tracks_get_budget(const Agentite_AITrackSystem *tracks,
                                     int track_id,
                                     int32_t resource_type);

/**
 * Get remaining budget (allocated - spent).
 *
 * @param tracks        Track system
 * @param track_id      Track ID
 * @param resource_type Resource identifier
 * @return Remaining budget
 */
int32_t agentite_ai_tracks_get_remaining(const Agentite_AITrackSystem *tracks,
                                        int track_id,
                                        int32_t resource_type);

/**
 * Mark budget as spent.
 *
 * @param tracks        Track system
 * @param track_id      Track ID
 * @param resource_type Resource identifier
 * @param amount        Amount spent
 * @return true if budget was available
 */
bool agentite_ai_tracks_spend_budget(Agentite_AITrackSystem *tracks,
                                    int track_id,
                                    int32_t resource_type,
                                    int32_t amount);

/**
 * Reset spent amounts for all tracks (call at start of turn).
 *
 * @param tracks Track system
 */
void agentite_ai_tracks_reset_spent(Agentite_AITrackSystem *tracks);

/**
 * Set budget provider callback.
 *
 * @param tracks   Track system
 * @param provider Budget provider function
 * @param userdata Provider userdata
 */
void agentite_ai_tracks_set_budget_provider(Agentite_AITrackSystem *tracks,
                                           Agentite_AITrackBudgetProvider provider,
                                           void *userdata);

/**
 * Allocate budgets using provider callback.
 *
 * @param tracks     Track system
 * @param game_state Game context
 */
void agentite_ai_tracks_allocate_budgets(Agentite_AITrackSystem *tracks,
                                        void *game_state);

/*============================================================================
 * Evaluation
 *============================================================================*/

/**
 * Evaluate all enabled tracks.
 *
 * @param tracks     Track system
 * @param game_state Game-specific context
 * @param out_result Output results struct
 */
void agentite_ai_tracks_evaluate_all(Agentite_AITrackSystem *tracks,
                                    void *game_state,
                                    Agentite_AITrackResult *out_result);

/**
 * Evaluate a single track.
 *
 * @param tracks     Track system
 * @param track_id   Track to evaluate
 * @param game_state Game-specific context
 * @param out_set    Output decision set
 */
void agentite_ai_tracks_evaluate(Agentite_AITrackSystem *tracks,
                                int track_id,
                                void *game_state,
                                Agentite_AITrackDecisionSet *out_set);

/**
 * Set decision filter callback.
 *
 * @param tracks   Track system
 * @param filter   Filter function
 * @param userdata Filter userdata
 */
void agentite_ai_tracks_set_filter(Agentite_AITrackSystem *tracks,
                                  Agentite_AITrackFilter filter,
                                  void *userdata);

/**
 * Sort decisions within a set by score (highest first).
 *
 * @param set Decision set to sort
 */
void agentite_ai_tracks_sort_decisions(Agentite_AITrackDecisionSet *set);

/**
 * Sort decisions by priority, then score.
 *
 * @param set Decision set to sort
 */
void agentite_ai_tracks_sort_by_priority(Agentite_AITrackDecisionSet *set);

/*============================================================================
 * Decision Queries
 *============================================================================*/

/**
 * Get the best decision from a track.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @param result   Evaluation result
 * @return Best decision or NULL
 */
const Agentite_AITrackDecision *agentite_ai_tracks_get_best(const Agentite_AITrackSystem *tracks,
                                                         int track_id,
                                                         const Agentite_AITrackResult *result);

/**
 * Get decisions of a specific type from results.
 *
 * @param result      Evaluation result
 * @param action_type Action type to filter by
 * @param out         Output array
 * @param max         Maximum decisions to return
 * @return Number of matching decisions
 */
int agentite_ai_tracks_get_by_type(const Agentite_AITrackResult *result,
                                  int32_t action_type,
                                  const Agentite_AITrackDecision **out,
                                  int max);

/**
 * Get decisions above a score threshold.
 *
 * @param result    Evaluation result
 * @param min_score Minimum score threshold
 * @param out       Output array
 * @param max       Maximum decisions to return
 * @return Number of matching decisions
 */
int agentite_ai_tracks_get_above_score(const Agentite_AITrackResult *result,
                                      float min_score,
                                      const Agentite_AITrackDecision **out,
                                      int max);

/**
 * Get all decisions from all tracks, merged and sorted.
 *
 * @param result Evaluation result
 * @param out    Output array
 * @param max    Maximum decisions to return
 * @return Number of decisions returned
 */
int agentite_ai_tracks_get_all_sorted(const Agentite_AITrackResult *result,
                                     const Agentite_AITrackDecision **out,
                                     int max);

/*============================================================================
 * Audit Trail
 *============================================================================*/

/**
 * Set reason string for a track (audit trail).
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @param fmt      Printf-style format string
 * @param ...      Format arguments
 */
void agentite_ai_tracks_set_reason(Agentite_AITrackSystem *tracks,
                                  int track_id,
                                  const char *fmt, ...);

/**
 * Get reason string for a track.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @return Reason string or empty string
 */
const char *agentite_ai_tracks_get_reason(const Agentite_AITrackSystem *tracks,
                                         int track_id);

/**
 * Clear all reason strings.
 *
 * @param tracks Track system
 */
void agentite_ai_tracks_clear_reasons(Agentite_AITrackSystem *tracks);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Get statistics for a track.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @param out      Output stats struct
 */
void agentite_ai_tracks_get_stats(const Agentite_AITrackSystem *tracks,
                                 int track_id,
                                 Agentite_AITrackStats *out);

/**
 * Record that a decision was executed.
 * Updates success rate statistics.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 */
void agentite_ai_tracks_record_execution(Agentite_AITrackSystem *tracks, int track_id);

/**
 * Reset statistics for all tracks.
 *
 * @param tracks Track system
 */
void agentite_ai_tracks_reset_stats(Agentite_AITrackSystem *tracks);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get human-readable name for a track type.
 *
 * @param type Track type
 * @return Static string name
 */
const char *agentite_ai_track_type_name(Agentite_AITrackType type);

/**
 * Get human-readable name for a priority level.
 *
 * @param priority Priority level
 * @return Static string name
 */
const char *agentite_ai_priority_name(Agentite_AIDecisionPriority priority);

/**
 * Initialize a decision struct to defaults.
 *
 * @param decision Decision to initialize
 */
void agentite_ai_track_decision_init(Agentite_AITrackDecision *decision);

/**
 * Copy a decision.
 *
 * @param dest Destination
 * @param src  Source
 */
void agentite_ai_track_decision_copy(Agentite_AITrackDecision *dest,
                                    const Agentite_AITrackDecision *src);

#endif /* AGENTITE_AI_TRACKS_H */
