#ifndef CARBON_VICTORY_H
#define CARBON_VICTORY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Victory Condition System
 *
 * Tracks multiple victory conditions with progress monitoring and event
 * integration. Supports configurable thresholds, per-faction progress,
 * and custom victory checkers.
 *
 * Usage:
 *   // Create victory manager
 *   Carbon_VictoryManager *victory = carbon_victory_create_with_events(events);
 *
 *   // Register victory conditions
 *   Carbon_VictoryCondition domination = {
 *       .id = "domination",
 *       .name = "World Domination",
 *       .description = "Control 75% of the map",
 *       .type = CARBON_VICTORY_DOMINATION,
 *       .threshold = 0.75f,
 *       .enabled = true,
 *   };
 *   carbon_victory_register(victory, &domination);
 *
 *   // Update progress each turn
 *   float territory_percent = calculate_territory_control(faction_id);
 *   carbon_victory_update_progress(victory, faction_id,
 *                                   CARBON_VICTORY_DOMINATION, territory_percent);
 *
 *   // Check for victory
 *   if (carbon_victory_check(victory)) {
 *       int winner = carbon_victory_get_winner(victory);
 *       int type = carbon_victory_get_winning_type(victory);
 *   }
 *
 *   // Cleanup
 *   carbon_victory_destroy(victory);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_VICTORY_MAX_CONDITIONS  16   /* Maximum victory types */
#define CARBON_VICTORY_MAX_FACTIONS    16   /* Maximum factions to track */

/*============================================================================
 * Victory Types
 *============================================================================*/

/**
 * Built-in victory condition types (game can extend with values >= 100)
 */
typedef enum Carbon_VictoryType {
    CARBON_VICTORY_NONE = 0,

    /* Standard victory types */
    CARBON_VICTORY_DOMINATION,      /* Control percentage of territory */
    CARBON_VICTORY_ELIMINATION,     /* Defeat all opponents */
    CARBON_VICTORY_TECHNOLOGY,      /* Research all/specific techs */
    CARBON_VICTORY_ECONOMIC,        /* Accumulate resources */
    CARBON_VICTORY_SCORE,           /* Highest score after N turns */
    CARBON_VICTORY_TIME,            /* Survive for N turns */
    CARBON_VICTORY_OBJECTIVE,       /* Complete specific objectives */
    CARBON_VICTORY_WONDER,          /* Build a wonder structure */
    CARBON_VICTORY_DIPLOMATIC,      /* Achieve diplomatic status */
    CARBON_VICTORY_CULTURAL,        /* Achieve cultural dominance */

    /* User-defined victory types start here */
    CARBON_VICTORY_USER = 100,
} Carbon_VictoryType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Victory condition definition
 */
typedef struct Carbon_VictoryCondition {
    /* Identity */
    char id[64];                    /* Unique identifier */
    char name[128];                 /* Display name */
    char description[256];          /* Description text */

    /* Type and threshold */
    int type;                       /* Victory type (game-defined or built-in) */
    float threshold;                /* Progress threshold (0.0-1.0), default 1.0 */

    /* Configuration */
    bool enabled;                   /* Whether this condition is active */
    int32_t target_value;           /* For numeric goals (e.g., 100000 gold) */
    int32_t target_turn;            /* For time-based (e.g., survive 100 turns) */

    /* Scoring (for SCORE victory type) */
    int32_t score_weight;           /* Weight in final score calculation */
} Carbon_VictoryCondition;

/**
 * Per-faction victory progress
 */
typedef struct Carbon_VictoryProgress {
    float progress[CARBON_VICTORY_MAX_CONDITIONS];  /* 0.0 to 1.0 per condition */
    int32_t score[CARBON_VICTORY_MAX_CONDITIONS];   /* Raw scores per condition */
    bool eliminated;                                /* Whether faction is eliminated */
} Carbon_VictoryProgress;

/**
 * Victory state (result when victory achieved)
 */
typedef struct Carbon_VictoryState {
    bool achieved;                  /* Whether victory has been achieved */
    int victory_type;               /* Which victory condition was met */
    int winner_id;                  /* Faction/player that won */
    int32_t winning_score;          /* Final score (if applicable) */
    uint32_t winning_turn;          /* Turn when victory occurred */
    char message[256];              /* Victory message */
} Carbon_VictoryState;

/**
 * Callback for victory achievement
 */
typedef void (*Carbon_VictoryCallback)(int faction_id,
                                        int victory_type,
                                        const Carbon_VictoryCondition *condition,
                                        void *userdata);

/**
 * Custom victory checker function.
 * Returns true if this faction meets the victory condition.
 *
 * @param faction_id  Faction to check
 * @param type        Victory type being checked
 * @param out_progress Output: current progress (0.0-1.0)
 * @param userdata    User data passed during registration
 * @return true if victory condition is met
 */
typedef bool (*Carbon_VictoryChecker)(int faction_id,
                                       int type,
                                       float *out_progress,
                                       void *userdata);

/*============================================================================
 * Victory Manager
 *============================================================================*/

typedef struct Carbon_VictoryManager Carbon_VictoryManager;

/**
 * Forward declaration for event dispatcher integration
 */
typedef struct Carbon_EventDispatcher Carbon_EventDispatcher;

/**
 * Create a new victory manager.
 *
 * @return New victory manager or NULL on failure
 */
Carbon_VictoryManager *carbon_victory_create(void);

/**
 * Create a victory manager with event dispatcher integration.
 * Events will be emitted when progress updates or victory is achieved.
 *
 * @param events Event dispatcher (can be NULL)
 * @return New victory manager or NULL on failure
 */
Carbon_VictoryManager *carbon_victory_create_with_events(Carbon_EventDispatcher *events);

/**
 * Destroy a victory manager and free resources.
 *
 * @param vm Victory manager to destroy
 */
void carbon_victory_destroy(Carbon_VictoryManager *vm);

/*============================================================================
 * Condition Registration
 *============================================================================*/

/**
 * Register a victory condition.
 *
 * @param vm   Victory manager
 * @param cond Victory condition definition (copied)
 * @return Condition index (0+) or -1 on failure
 */
int carbon_victory_register(Carbon_VictoryManager *vm,
                             const Carbon_VictoryCondition *cond);

/**
 * Get a victory condition by index.
 *
 * @param vm    Victory manager
 * @param index Condition index
 * @return Condition definition or NULL
 */
const Carbon_VictoryCondition *carbon_victory_get_condition(
    const Carbon_VictoryManager *vm, int index);

/**
 * Get a victory condition by type.
 *
 * @param vm   Victory manager
 * @param type Victory type
 * @return Condition definition or NULL
 */
const Carbon_VictoryCondition *carbon_victory_get_by_type(
    const Carbon_VictoryManager *vm, int type);

/**
 * Find a victory condition by ID.
 *
 * @param vm Victory manager
 * @param id Condition ID
 * @return Condition definition or NULL
 */
const Carbon_VictoryCondition *carbon_victory_find(
    const Carbon_VictoryManager *vm, const char *id);

/**
 * Get the number of registered conditions.
 *
 * @param vm Victory manager
 * @return Number of conditions
 */
int carbon_victory_condition_count(const Carbon_VictoryManager *vm);

/**
 * Enable or disable a victory condition.
 *
 * @param vm      Victory manager
 * @param type    Victory type
 * @param enabled Whether to enable
 */
void carbon_victory_set_enabled(Carbon_VictoryManager *vm, int type, bool enabled);

/**
 * Check if a victory condition is enabled.
 *
 * @param vm   Victory manager
 * @param type Victory type
 * @return true if enabled
 */
bool carbon_victory_is_enabled(const Carbon_VictoryManager *vm, int type);

/*============================================================================
 * Progress Tracking
 *============================================================================*/

/**
 * Initialize progress tracking for a faction.
 * Call this when a faction joins the game.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID (0 to MAX_FACTIONS-1)
 */
void carbon_victory_init_faction(Carbon_VictoryManager *vm, int faction_id);

/**
 * Update progress for a faction on a specific victory condition.
 * Emits CARBON_EVENT_VICTORY_PROGRESS if events are enabled.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @param progress   Progress value (0.0-1.0)
 */
void carbon_victory_update_progress(Carbon_VictoryManager *vm,
                                     int faction_id,
                                     int type,
                                     float progress);

/**
 * Update score for a faction on a specific victory condition.
 * Progress is calculated as score / target_value.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @param score      Raw score value
 */
void carbon_victory_update_score(Carbon_VictoryManager *vm,
                                  int faction_id,
                                  int type,
                                  int32_t score);

/**
 * Add to score for a faction (incremental update).
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @param delta      Score delta to add
 */
void carbon_victory_add_score(Carbon_VictoryManager *vm,
                               int faction_id,
                               int type,
                               int32_t delta);

/**
 * Get current progress for a faction on a victory condition.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @return Progress (0.0-1.0)
 */
float carbon_victory_get_progress(const Carbon_VictoryManager *vm,
                                   int faction_id,
                                   int type);

/**
 * Get current score for a faction on a victory condition.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @return Raw score
 */
int32_t carbon_victory_get_score(const Carbon_VictoryManager *vm,
                                  int faction_id,
                                  int type);

/**
 * Get the full progress struct for a faction.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @return Progress struct or NULL if invalid
 */
const Carbon_VictoryProgress *carbon_victory_get_faction_progress(
    const Carbon_VictoryManager *vm, int faction_id);

/**
 * Mark a faction as eliminated.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 */
void carbon_victory_eliminate_faction(Carbon_VictoryManager *vm, int faction_id);

/**
 * Check if a faction is eliminated.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @return true if eliminated
 */
bool carbon_victory_is_eliminated(const Carbon_VictoryManager *vm, int faction_id);

/**
 * Get the number of surviving (non-eliminated) factions.
 *
 * @param vm Victory manager
 * @return Number of active factions
 */
int carbon_victory_active_faction_count(const Carbon_VictoryManager *vm);

/*============================================================================
 * Victory Checking
 *============================================================================*/

/**
 * Check all victory conditions for all factions.
 * Emits CARBON_EVENT_VICTORY_ACHIEVED if a winner is found.
 *
 * @param vm Victory manager
 * @return true if victory was achieved
 */
bool carbon_victory_check(Carbon_VictoryManager *vm);

/**
 * Check a specific victory condition for a specific faction.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @return true if this faction meets this condition
 */
bool carbon_victory_check_condition(const Carbon_VictoryManager *vm,
                                     int faction_id,
                                     int type);

/**
 * Declare victory manually (for scripted/custom wins).
 * Emits CARBON_EVENT_VICTORY_ACHIEVED.
 *
 * @param vm         Victory manager
 * @param faction_id Winning faction
 * @param type       Victory type
 * @param message    Victory message (can be NULL)
 */
void carbon_victory_declare(Carbon_VictoryManager *vm,
                             int faction_id,
                             int type,
                             const char *message);

/**
 * Check if victory has been achieved.
 *
 * @param vm Victory manager
 * @return true if game is over
 */
bool carbon_victory_is_achieved(const Carbon_VictoryManager *vm);

/**
 * Get the winning faction ID.
 *
 * @param vm Victory manager
 * @return Faction ID or -1 if no winner
 */
int carbon_victory_get_winner(const Carbon_VictoryManager *vm);

/**
 * Get the victory type that was achieved.
 *
 * @param vm Victory manager
 * @return Victory type or CARBON_VICTORY_NONE
 */
int carbon_victory_get_winning_type(const Carbon_VictoryManager *vm);

/**
 * Get the full victory state.
 *
 * @param vm Victory manager
 * @return Victory state struct
 */
const Carbon_VictoryState *carbon_victory_get_state(const Carbon_VictoryManager *vm);

/**
 * Reset victory state (for new game).
 *
 * @param vm Victory manager
 */
void carbon_victory_reset(Carbon_VictoryManager *vm);

/*============================================================================
 * Score Victory Support
 *============================================================================*/

/**
 * Set the current turn (for time-based and score victories).
 *
 * @param vm   Victory manager
 * @param turn Current turn number
 */
void carbon_victory_set_turn(Carbon_VictoryManager *vm, uint32_t turn);

/**
 * Calculate total score for a faction across all conditions.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @return Total weighted score
 */
int32_t carbon_victory_calculate_score(const Carbon_VictoryManager *vm,
                                        int faction_id);

/**
 * Get the faction with the highest total score.
 *
 * @param vm Victory manager
 * @return Faction ID with highest score, or -1 if no factions
 */
int carbon_victory_get_score_leader(const Carbon_VictoryManager *vm);

/*============================================================================
 * Custom Victory Checkers
 *============================================================================*/

/**
 * Register a custom victory checker function.
 * The checker is called during carbon_victory_check() for this type.
 *
 * @param vm       Victory manager
 * @param type     Victory type (should match a registered condition)
 * @param checker  Checker function
 * @param userdata User data to pass to checker
 */
void carbon_victory_set_checker(Carbon_VictoryManager *vm,
                                 int type,
                                 Carbon_VictoryChecker checker,
                                 void *userdata);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set a callback for when victory is achieved.
 *
 * @param vm       Victory manager
 * @param callback Function to call on victory
 * @param userdata User data to pass to callback
 */
void carbon_victory_set_callback(Carbon_VictoryManager *vm,
                                  Carbon_VictoryCallback callback,
                                  void *userdata);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get a human-readable name for a victory type.
 *
 * @param type Victory type
 * @return Static string name
 */
const char *carbon_victory_type_name(int type);

/**
 * Get progress as a formatted percentage string.
 *
 * @param vm         Victory manager
 * @param faction_id Faction ID
 * @param type       Victory type
 * @param buffer     Output buffer
 * @param size       Buffer size
 * @return buffer pointer
 */
char *carbon_victory_format_progress(const Carbon_VictoryManager *vm,
                                      int faction_id,
                                      int type,
                                      char *buffer,
                                      size_t size);

#endif /* CARBON_VICTORY_H */
