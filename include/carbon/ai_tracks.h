#ifndef CARBON_AI_TRACKS_H
#define CARBON_AI_TRACKS_H

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
 *   Carbon_AITrackSystem *tracks = carbon_ai_tracks_create();
 *   carbon_ai_tracks_set_blackboard(tracks, blackboard);
 *
 *   // Register tracks
 *   int econ_track = carbon_ai_tracks_register(tracks, "economy", evaluate_economy);
 *   int mil_track = carbon_ai_tracks_register(tracks, "military", evaluate_military);
 *
 *   // Set budgets per resource type
 *   carbon_ai_tracks_set_budget(tracks, econ_track, RESOURCE_GOLD, 1000);
 *   carbon_ai_tracks_set_budget(tracks, mil_track, RESOURCE_GOLD, 500);
 *
 *   // Each turn, evaluate all tracks
 *   Carbon_AITrackResult results;
 *   carbon_ai_tracks_evaluate_all(tracks, game_state, &results);
 *
 *   // Execute decisions from each track
 *   for (int i = 0; i < results.track_count; i++) {
 *       for (int j = 0; j < results.decisions[i].count; j++) {
 *           execute_decision(&results.decisions[i].items[j]);
 *       }
 *   }
 *
 *   // Cleanup
 *   carbon_ai_tracks_destroy(tracks);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_AI_TRACKS_MAX          8    /* Maximum registered tracks */
#define CARBON_AI_TRACKS_MAX_BUDGETS  8    /* Maximum budget types per track */
#define CARBON_AI_TRACKS_MAX_DECISIONS 16  /* Maximum decisions per track */
#define CARBON_AI_TRACKS_NAME_LEN     32   /* Maximum track name length */
#define CARBON_AI_TRACKS_REASON_LEN   128  /* Maximum reason string length */

/*============================================================================
 * Predefined Track Types
 *============================================================================*/

/**
 * Built-in track type identifiers for common AI concerns.
 * Games can use these or define their own starting at CARBON_AI_TRACK_USER.
 */
typedef enum Carbon_AITrackType {
    CARBON_AI_TRACK_ECONOMY = 0,    /* Resource production, expansion */
    CARBON_AI_TRACK_MILITARY,       /* Unit production, defense */
    CARBON_AI_TRACK_RESEARCH,       /* Technology priorities */
    CARBON_AI_TRACK_DIPLOMACY,      /* Relations, treaties */
    CARBON_AI_TRACK_EXPANSION,      /* Territory growth */
    CARBON_AI_TRACK_INFRASTRUCTURE, /* Building, improvements */
    CARBON_AI_TRACK_ESPIONAGE,      /* Intelligence, sabotage */
    CARBON_AI_TRACK_CUSTOM,         /* Game-specific track */
    CARBON_AI_TRACK_COUNT,

    /* User-defined track types start here */
    CARBON_AI_TRACK_USER = 100,
} Carbon_AITrackType;

/*============================================================================
 * Decision Priority Levels
 *============================================================================*/

typedef enum Carbon_AIDecisionPriority {
    CARBON_AI_PRIORITY_LOW = 0,
    CARBON_AI_PRIORITY_NORMAL,
    CARBON_AI_PRIORITY_HIGH,
    CARBON_AI_PRIORITY_CRITICAL,
    CARBON_AI_PRIORITY_COUNT,
} Carbon_AIDecisionPriority;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * A single decision from a track
 */
typedef struct Carbon_AITrackDecision {
    int32_t action_type;            /* Game-defined action type */
    int32_t target_id;              /* Target entity/location/faction */
    int32_t secondary_id;           /* Secondary parameter */
    float score;                    /* Decision score (higher = better) */
    Carbon_AIDecisionPriority priority;
    int32_t resource_type;          /* Resource to spend (-1 = none) */
    int32_t resource_cost;          /* Cost of this decision */
    void *userdata;                 /* Game-specific data (not owned) */
} Carbon_AITrackDecision;

/**
 * Budget allocation for a single resource type
 */
typedef struct Carbon_AITrackBudget {
    int32_t resource_type;          /* Resource identifier */
    int32_t allocated;              /* Total allocated to this track */
    int32_t spent;                  /* Amount spent this turn */
    int32_t reserved;               /* Amount reserved on blackboard */
    bool active;                    /* Whether this budget slot is used */
} Carbon_AITrackBudget;

/**
 * Collection of decisions from a single track
 */
typedef struct Carbon_AITrackDecisionSet {
    Carbon_AITrackDecision items[CARBON_AI_TRACKS_MAX_DECISIONS];
    int count;
    int track_id;
    char track_name[CARBON_AI_TRACKS_NAME_LEN];
    char reason[CARBON_AI_TRACKS_REASON_LEN];  /* Audit trail */
    float total_score;              /* Sum of all decision scores */
} Carbon_AITrackDecisionSet;

/**
 * Results from evaluating all tracks
 */
typedef struct Carbon_AITrackResult {
    Carbon_AITrackDecisionSet decisions[CARBON_AI_TRACKS_MAX];
    int track_count;
    int total_decisions;            /* Sum across all tracks */
    float total_score;              /* Sum of all scores */
} Carbon_AITrackResult;

/**
 * Track statistics for debugging/UI
 */
typedef struct Carbon_AITrackStats {
    int evaluations;                /* Times evaluated */
    int decisions_made;             /* Total decisions generated */
    int decisions_executed;         /* Decisions that were executed */
    int32_t resources_spent;        /* Total resources spent */
    float avg_score;                /* Average decision score */
    float success_rate;             /* Executed / made ratio */
} Carbon_AITrackStats;

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
typedef void (*Carbon_AITrackEvaluator)(int track_id,
                                         void *game_state,
                                         const Carbon_AITrackBudget *budgets,
                                         int budget_count,
                                         Carbon_AITrackDecision *out_decisions,
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
typedef bool (*Carbon_AITrackFilter)(int track_id,
                                      const Carbon_AITrackDecision *decision,
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
typedef int32_t (*Carbon_AITrackBudgetProvider)(int track_id,
                                                  int32_t resource_type,
                                                  void *game_state,
                                                  void *userdata);

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Carbon_AITrackSystem Carbon_AITrackSystem;
typedef struct Carbon_Blackboard Carbon_Blackboard;

/*============================================================================
 * System Lifecycle
 *============================================================================*/

/**
 * Create a new track system.
 *
 * @return New track system or NULL on failure
 */
Carbon_AITrackSystem *carbon_ai_tracks_create(void);

/**
 * Destroy a track system and free resources.
 *
 * @param tracks Track system to destroy
 */
void carbon_ai_tracks_destroy(Carbon_AITrackSystem *tracks);

/**
 * Reset all tracks (clear budgets spent, statistics).
 *
 * @param tracks Track system
 */
void carbon_ai_tracks_reset(Carbon_AITrackSystem *tracks);

/*============================================================================
 * Blackboard Integration
 *============================================================================*/

/**
 * Set the blackboard for resource reservation coordination.
 *
 * @param tracks Track system
 * @param bb     Blackboard (not owned, must outlive tracks)
 */
void carbon_ai_tracks_set_blackboard(Carbon_AITrackSystem *tracks,
                                      Carbon_Blackboard *bb);

/**
 * Get the associated blackboard.
 *
 * @param tracks Track system
 * @return Blackboard or NULL
 */
Carbon_Blackboard *carbon_ai_tracks_get_blackboard(Carbon_AITrackSystem *tracks);

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
int carbon_ai_tracks_register(Carbon_AITrackSystem *tracks,
                               const char *name,
                               Carbon_AITrackEvaluator evaluator);

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
int carbon_ai_tracks_register_ex(Carbon_AITrackSystem *tracks,
                                  const char *name,
                                  Carbon_AITrackType type,
                                  Carbon_AITrackEvaluator evaluator,
                                  void *userdata);

/**
 * Unregister a track.
 *
 * @param tracks   Track system
 * @param track_id Track to unregister
 */
void carbon_ai_tracks_unregister(Carbon_AITrackSystem *tracks, int track_id);

/**
 * Get track ID by name.
 *
 * @param tracks Track system
 * @param name   Track name
 * @return Track ID or -1 if not found
 */
int carbon_ai_tracks_get_id(const Carbon_AITrackSystem *tracks, const char *name);

/**
 * Get track name by ID.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @return Track name or NULL
 */
const char *carbon_ai_tracks_get_name(const Carbon_AITrackSystem *tracks, int track_id);

/**
 * Get number of registered tracks.
 *
 * @param tracks Track system
 * @return Track count
 */
int carbon_ai_tracks_count(const Carbon_AITrackSystem *tracks);

/**
 * Check if a track is enabled.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @return true if enabled
 */
bool carbon_ai_tracks_is_enabled(const Carbon_AITrackSystem *tracks, int track_id);

/**
 * Enable or disable a track.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @param enabled  Whether track should be enabled
 */
void carbon_ai_tracks_set_enabled(Carbon_AITrackSystem *tracks, int track_id, bool enabled);

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
void carbon_ai_tracks_set_budget(Carbon_AITrackSystem *tracks,
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
int32_t carbon_ai_tracks_get_budget(const Carbon_AITrackSystem *tracks,
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
int32_t carbon_ai_tracks_get_remaining(const Carbon_AITrackSystem *tracks,
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
bool carbon_ai_tracks_spend_budget(Carbon_AITrackSystem *tracks,
                                    int track_id,
                                    int32_t resource_type,
                                    int32_t amount);

/**
 * Reset spent amounts for all tracks (call at start of turn).
 *
 * @param tracks Track system
 */
void carbon_ai_tracks_reset_spent(Carbon_AITrackSystem *tracks);

/**
 * Set budget provider callback.
 *
 * @param tracks   Track system
 * @param provider Budget provider function
 * @param userdata Provider userdata
 */
void carbon_ai_tracks_set_budget_provider(Carbon_AITrackSystem *tracks,
                                           Carbon_AITrackBudgetProvider provider,
                                           void *userdata);

/**
 * Allocate budgets using provider callback.
 *
 * @param tracks     Track system
 * @param game_state Game context
 */
void carbon_ai_tracks_allocate_budgets(Carbon_AITrackSystem *tracks,
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
void carbon_ai_tracks_evaluate_all(Carbon_AITrackSystem *tracks,
                                    void *game_state,
                                    Carbon_AITrackResult *out_result);

/**
 * Evaluate a single track.
 *
 * @param tracks     Track system
 * @param track_id   Track to evaluate
 * @param game_state Game-specific context
 * @param out_set    Output decision set
 */
void carbon_ai_tracks_evaluate(Carbon_AITrackSystem *tracks,
                                int track_id,
                                void *game_state,
                                Carbon_AITrackDecisionSet *out_set);

/**
 * Set decision filter callback.
 *
 * @param tracks   Track system
 * @param filter   Filter function
 * @param userdata Filter userdata
 */
void carbon_ai_tracks_set_filter(Carbon_AITrackSystem *tracks,
                                  Carbon_AITrackFilter filter,
                                  void *userdata);

/**
 * Sort decisions within a set by score (highest first).
 *
 * @param set Decision set to sort
 */
void carbon_ai_tracks_sort_decisions(Carbon_AITrackDecisionSet *set);

/**
 * Sort decisions by priority, then score.
 *
 * @param set Decision set to sort
 */
void carbon_ai_tracks_sort_by_priority(Carbon_AITrackDecisionSet *set);

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
const Carbon_AITrackDecision *carbon_ai_tracks_get_best(const Carbon_AITrackSystem *tracks,
                                                         int track_id,
                                                         const Carbon_AITrackResult *result);

/**
 * Get decisions of a specific type from results.
 *
 * @param result      Evaluation result
 * @param action_type Action type to filter by
 * @param out         Output array
 * @param max         Maximum decisions to return
 * @return Number of matching decisions
 */
int carbon_ai_tracks_get_by_type(const Carbon_AITrackResult *result,
                                  int32_t action_type,
                                  const Carbon_AITrackDecision **out,
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
int carbon_ai_tracks_get_above_score(const Carbon_AITrackResult *result,
                                      float min_score,
                                      const Carbon_AITrackDecision **out,
                                      int max);

/**
 * Get all decisions from all tracks, merged and sorted.
 *
 * @param result Evaluation result
 * @param out    Output array
 * @param max    Maximum decisions to return
 * @return Number of decisions returned
 */
int carbon_ai_tracks_get_all_sorted(const Carbon_AITrackResult *result,
                                     const Carbon_AITrackDecision **out,
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
void carbon_ai_tracks_set_reason(Carbon_AITrackSystem *tracks,
                                  int track_id,
                                  const char *fmt, ...);

/**
 * Get reason string for a track.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 * @return Reason string or empty string
 */
const char *carbon_ai_tracks_get_reason(const Carbon_AITrackSystem *tracks,
                                         int track_id);

/**
 * Clear all reason strings.
 *
 * @param tracks Track system
 */
void carbon_ai_tracks_clear_reasons(Carbon_AITrackSystem *tracks);

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
void carbon_ai_tracks_get_stats(const Carbon_AITrackSystem *tracks,
                                 int track_id,
                                 Carbon_AITrackStats *out);

/**
 * Record that a decision was executed.
 * Updates success rate statistics.
 *
 * @param tracks   Track system
 * @param track_id Track ID
 */
void carbon_ai_tracks_record_execution(Carbon_AITrackSystem *tracks, int track_id);

/**
 * Reset statistics for all tracks.
 *
 * @param tracks Track system
 */
void carbon_ai_tracks_reset_stats(Carbon_AITrackSystem *tracks);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get human-readable name for a track type.
 *
 * @param type Track type
 * @return Static string name
 */
const char *carbon_ai_track_type_name(Carbon_AITrackType type);

/**
 * Get human-readable name for a priority level.
 *
 * @param priority Priority level
 * @return Static string name
 */
const char *carbon_ai_priority_name(Carbon_AIDecisionPriority priority);

/**
 * Initialize a decision struct to defaults.
 *
 * @param decision Decision to initialize
 */
void carbon_ai_track_decision_init(Carbon_AITrackDecision *decision);

/**
 * Copy a decision.
 *
 * @param dest Destination
 * @param src  Source
 */
void carbon_ai_track_decision_copy(Carbon_AITrackDecision *dest,
                                    const Carbon_AITrackDecision *src);

#endif /* CARBON_AI_TRACKS_H */
