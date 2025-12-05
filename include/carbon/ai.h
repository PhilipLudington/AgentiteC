#ifndef CARBON_AI_H
#define CARBON_AI_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon AI Personality System
 *
 * Personality-driven AI decision making with weighted behaviors, threat
 * assessment, goal management, and extensible action evaluation.
 *
 * Usage:
 *   // Create AI system
 *   Carbon_AISystem *ai = carbon_ai_create();
 *
 *   // Initialize per-faction AI state
 *   Carbon_AIState state;
 *   carbon_ai_state_init(&state, CARBON_AI_AGGRESSIVE);
 *
 *   // Register custom evaluators for game-specific actions
 *   carbon_ai_register_evaluator(ai, CARBON_AI_ACTION_ATTACK, evaluate_attacks);
 *   carbon_ai_register_evaluator(ai, CARBON_AI_ACTION_BUILD, evaluate_builds);
 *
 *   // Each turn, process AI decisions
 *   Carbon_AIDecision decision;
 *   carbon_ai_process_turn(ai, &state, game_context, &decision);
 *
 *   // Execute top actions
 *   for (int i = 0; i < decision.action_count; i++) {
 *       execute_action(&decision.actions[i]);
 *   }
 *
 *   // Cleanup
 *   carbon_ai_destroy(ai);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_AI_MAX_ACTIONS      16   /* Maximum actions per decision */
#define CARBON_AI_MAX_EVALUATORS   16   /* Maximum registered evaluators */
#define CARBON_AI_MAX_COOLDOWNS    8    /* Maximum cooldown trackers */
#define CARBON_AI_MAX_GOALS        8    /* Maximum concurrent goals */
#define CARBON_AI_MAX_THREATS      8    /* Maximum tracked threats */

/*============================================================================
 * AI Personality Types
 *============================================================================*/

/**
 * Built-in AI personality archetypes
 */
typedef enum Carbon_AIPersonality {
    CARBON_AI_BALANCED = 0,     /* Equal weights across all behaviors */
    CARBON_AI_AGGRESSIVE,       /* Prioritizes combat and conquest */
    CARBON_AI_DEFENSIVE,        /* Prioritizes protection and fortification */
    CARBON_AI_ECONOMIC,         /* Prioritizes resource generation */
    CARBON_AI_EXPANSIONIST,     /* Prioritizes territory acquisition */
    CARBON_AI_TECHNOLOGIST,     /* Prioritizes research and upgrades */
    CARBON_AI_DIPLOMATIC,       /* Prioritizes alliances and negotiation */
    CARBON_AI_OPPORTUNIST,      /* Adapts based on situation */
    CARBON_AI_PERSONALITY_COUNT,

    /* User-defined personalities start here */
    CARBON_AI_PERSONALITY_USER = 100,
} Carbon_AIPersonality;

/*============================================================================
 * AI Action Types
 *============================================================================*/

/**
 * Types of actions the AI can take
 */
typedef enum Carbon_AIActionType {
    CARBON_AI_ACTION_NONE = 0,
    CARBON_AI_ACTION_BUILD,         /* Construct buildings/units */
    CARBON_AI_ACTION_ATTACK,        /* Attack enemy targets */
    CARBON_AI_ACTION_DEFEND,        /* Defend owned territory */
    CARBON_AI_ACTION_EXPAND,        /* Claim new territory */
    CARBON_AI_ACTION_RESEARCH,      /* Research technologies */
    CARBON_AI_ACTION_DIPLOMACY,     /* Diplomatic actions */
    CARBON_AI_ACTION_RECRUIT,       /* Hire/train units */
    CARBON_AI_ACTION_RETREAT,       /* Withdraw from danger */
    CARBON_AI_ACTION_SCOUT,         /* Explore/gather intel */
    CARBON_AI_ACTION_TRADE,         /* Economic transactions */
    CARBON_AI_ACTION_UPGRADE,       /* Improve existing assets */
    CARBON_AI_ACTION_SPECIAL,       /* Game-specific special action */
    CARBON_AI_ACTION_COUNT,

    /* User-defined action types start here */
    CARBON_AI_ACTION_USER = 100,
} Carbon_AIActionType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Behavior weights that drive AI decisions.
 * Values are typically 0.0-1.0 but can exceed 1.0 for emphasis.
 * Higher values make the AI more likely to choose related actions.
 */
typedef struct Carbon_AIWeights {
    float aggression;       /* Weight for attack/combat actions */
    float defense;          /* Weight for defensive actions */
    float expansion;        /* Weight for territory expansion */
    float economy;          /* Weight for economic development */
    float technology;       /* Weight for research/upgrades */
    float diplomacy;        /* Weight for diplomatic actions */
    float caution;          /* Risk aversion (higher = more careful) */
    float opportunism;      /* React to immediate opportunities */
} Carbon_AIWeights;

/**
 * A single AI action with target and priority
 */
typedef struct Carbon_AIAction {
    Carbon_AIActionType type;   /* Action type */
    int32_t target_id;          /* Target entity/location/faction ID */
    int32_t secondary_id;       /* Secondary target (e.g., unit type to build) */
    float priority;             /* Priority score (higher = more important) */
    float urgency;              /* Time sensitivity (higher = do sooner) */
    void *data;                 /* Action-specific data (game-defined) */
    size_t data_size;           /* Size of data for serialization */
} Carbon_AIAction;

/**
 * Collection of AI actions representing a turn's decisions
 */
typedef struct Carbon_AIDecision {
    Carbon_AIAction actions[CARBON_AI_MAX_ACTIONS];
    int action_count;
    float total_score;          /* Combined score of all actions */
} Carbon_AIDecision;

/**
 * Tracked threat information
 */
typedef struct Carbon_AIThreat {
    int32_t source_id;          /* Threatening faction/entity ID */
    float level;                /* Threat level (0.0-1.0) */
    float distance;             /* Proximity (lower = closer/more urgent) */
    int32_t target_id;          /* What is being threatened */
    int turns_since_update;     /* Staleness counter */
} Carbon_AIThreat;

/**
 * AI goal tracking
 */
typedef struct Carbon_AIGoal {
    int32_t type;               /* Game-defined goal type */
    int32_t target_id;          /* Goal target */
    float priority;             /* Goal priority */
    float progress;             /* 0.0 to 1.0 completion */
    int turns_active;           /* How long pursuing this goal */
    bool completed;             /* Whether goal is achieved */
} Carbon_AIGoal;

/**
 * Per-faction AI state
 */
typedef struct Carbon_AIState {
    /* Personality and weights */
    Carbon_AIPersonality personality;
    Carbon_AIWeights weights;
    Carbon_AIWeights base_weights;  /* Original weights for reset */

    /* Strategic targets */
    int32_t primary_target;         /* Main enemy to focus on (-1 = none) */
    int32_t ally_target;            /* Faction to ally with (-1 = none) */

    /* Threat assessment */
    float overall_threat;           /* Global threat level (0.0-1.0) */
    Carbon_AIThreat threats[CARBON_AI_MAX_THREATS];
    int threat_count;

    /* Goals */
    Carbon_AIGoal goals[CARBON_AI_MAX_GOALS];
    int goal_count;

    /* Action cooldowns (prevent repetitive actions) */
    int cooldowns[CARBON_AI_MAX_COOLDOWNS];

    /* Situational modifiers */
    float morale;                   /* AI confidence (affects risk-taking) */
    float resources_ratio;          /* Our resources vs average */
    float military_ratio;           /* Our military vs average */
    float tech_ratio;               /* Our tech level vs average */

    /* Memory/Learning */
    int32_t last_action_type;       /* Last action taken */
    int32_t last_target;            /* Last target */
    int turns_since_combat;         /* Turns since last combat */
    int turns_since_expansion;      /* Turns since last expansion */

    /* Random seed for deterministic behavior */
    uint32_t random_state;
} Carbon_AIState;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Evaluator function for generating scored actions.
 * Called for each registered action type during AI processing.
 *
 * @param state       AI state for this faction
 * @param game_ctx    Game-specific context pointer
 * @param out_actions Output array for generated actions
 * @param out_count   Output: number of actions generated
 * @param max_actions Maximum actions to generate
 */
typedef void (*Carbon_AIEvaluator)(Carbon_AIState *state,
                                    void *game_ctx,
                                    Carbon_AIAction *out_actions,
                                    int *out_count,
                                    int max_actions);

/**
 * Threat assessment callback.
 * Called to update threat information for a faction.
 *
 * @param state    AI state for this faction
 * @param game_ctx Game-specific context pointer
 * @param threats  Output array for threats
 * @param count    Output: number of threats
 * @param max      Maximum threats to report
 */
typedef void (*Carbon_AIThreatAssessor)(Carbon_AIState *state,
                                         void *game_ctx,
                                         Carbon_AIThreat *threats,
                                         int *count,
                                         int max);

/**
 * Situation analyzer callback.
 * Called to update situational modifiers (ratios, morale, etc).
 *
 * @param state    AI state to update
 * @param game_ctx Game-specific context pointer
 */
typedef void (*Carbon_AISituationAnalyzer)(Carbon_AIState *state, void *game_ctx);

/*============================================================================
 * AI System
 *============================================================================*/

typedef struct Carbon_AISystem Carbon_AISystem;

/**
 * Create a new AI system.
 *
 * @return New AI system or NULL on failure
 */
Carbon_AISystem *carbon_ai_create(void);

/**
 * Destroy an AI system and free resources.
 *
 * @param ai AI system to destroy
 */
void carbon_ai_destroy(Carbon_AISystem *ai);

/*============================================================================
 * State Management
 *============================================================================*/

/**
 * Initialize an AI state with a personality.
 *
 * @param state       State to initialize
 * @param personality Personality type
 */
void carbon_ai_state_init(Carbon_AIState *state, Carbon_AIPersonality personality);

/**
 * Reset an AI state while preserving personality.
 *
 * @param state State to reset
 */
void carbon_ai_state_reset(Carbon_AIState *state);

/**
 * Get default weights for a personality type.
 *
 * @param personality Personality type
 * @param out        Output weights
 */
void carbon_ai_get_default_weights(Carbon_AIPersonality personality,
                                    Carbon_AIWeights *out);

/**
 * Set custom weights for an AI state.
 *
 * @param state   AI state
 * @param weights New weights to apply
 */
void carbon_ai_set_weights(Carbon_AIState *state, const Carbon_AIWeights *weights);

/**
 * Modify weights temporarily (e.g., in response to events).
 * Changes are applied as multipliers to current weights.
 *
 * @param state   AI state
 * @param weights Weight modifiers (1.0 = no change)
 */
void carbon_ai_modify_weights(Carbon_AIState *state, const Carbon_AIWeights *modifiers);

/**
 * Reset weights to base personality defaults.
 *
 * @param state AI state
 */
void carbon_ai_reset_weights(Carbon_AIState *state);

/*============================================================================
 * Evaluator Registration
 *============================================================================*/

/**
 * Register an action evaluator for a specific action type.
 * Evaluators generate and score potential actions.
 *
 * @param ai        AI system
 * @param type      Action type this evaluator handles
 * @param evaluator Evaluator function
 */
void carbon_ai_register_evaluator(Carbon_AISystem *ai,
                                   Carbon_AIActionType type,
                                   Carbon_AIEvaluator evaluator);

/**
 * Set the threat assessment callback.
 *
 * @param ai       AI system
 * @param assessor Threat assessment function
 */
void carbon_ai_set_threat_assessor(Carbon_AISystem *ai,
                                    Carbon_AIThreatAssessor assessor);

/**
 * Set the situation analysis callback.
 *
 * @param ai       AI system
 * @param analyzer Situation analyzer function
 */
void carbon_ai_set_situation_analyzer(Carbon_AISystem *ai,
                                       Carbon_AISituationAnalyzer analyzer);

/*============================================================================
 * Decision Making
 *============================================================================*/

/**
 * Process a turn for an AI faction.
 * Runs all evaluators, scores actions, and returns prioritized decisions.
 *
 * @param ai       AI system
 * @param state    Faction's AI state
 * @param game_ctx Game-specific context
 * @param out      Output decision struct
 */
void carbon_ai_process_turn(Carbon_AISystem *ai,
                             Carbon_AIState *state,
                             void *game_ctx,
                             Carbon_AIDecision *out);

/**
 * Score a single action based on AI state and weights.
 *
 * @param state      AI state
 * @param type       Action type
 * @param base_score Base score before weight application
 * @return Weighted score
 */
float carbon_ai_score_action(const Carbon_AIState *state,
                              Carbon_AIActionType type,
                              float base_score);

/**
 * Sort actions in a decision by priority (highest first).
 *
 * @param decision Decision to sort
 */
void carbon_ai_sort_actions(Carbon_AIDecision *decision);

/**
 * Get the top N actions from a decision.
 *
 * @param decision Source decision
 * @param out      Output array
 * @param max      Maximum actions to return
 * @return Number of actions returned
 */
int carbon_ai_get_top_actions(const Carbon_AIDecision *decision,
                               Carbon_AIAction *out,
                               int max);

/*============================================================================
 * Threat Management
 *============================================================================*/

/**
 * Update threat assessment for an AI state.
 * Uses registered threat assessor if available.
 *
 * @param ai       AI system
 * @param state    AI state to update
 * @param game_ctx Game context
 */
void carbon_ai_update_threats(Carbon_AISystem *ai,
                               Carbon_AIState *state,
                               void *game_ctx);

/**
 * Add a threat manually.
 *
 * @param state     AI state
 * @param source_id Threatening faction/entity
 * @param level     Threat level (0.0-1.0)
 * @param target_id What is being threatened
 * @param distance  Proximity factor
 */
void carbon_ai_add_threat(Carbon_AIState *state,
                           int32_t source_id,
                           float level,
                           int32_t target_id,
                           float distance);

/**
 * Remove a threat by source ID.
 *
 * @param state     AI state
 * @param source_id Threat source to remove
 */
void carbon_ai_remove_threat(Carbon_AIState *state, int32_t source_id);

/**
 * Get the highest threat.
 *
 * @param state AI state
 * @return Highest threat or NULL if none
 */
const Carbon_AIThreat *carbon_ai_get_highest_threat(const Carbon_AIState *state);

/**
 * Calculate overall threat level from individual threats.
 *
 * @param state AI state
 * @return Combined threat level (0.0-1.0)
 */
float carbon_ai_calculate_threat_level(Carbon_AIState *state);

/*============================================================================
 * Goal Management
 *============================================================================*/

/**
 * Add a goal for the AI to pursue.
 *
 * @param state     AI state
 * @param type      Game-defined goal type
 * @param target_id Goal target
 * @param priority  Goal priority
 * @return Goal index or -1 if full
 */
int carbon_ai_add_goal(Carbon_AIState *state,
                        int32_t type,
                        int32_t target_id,
                        float priority);

/**
 * Update progress on a goal.
 *
 * @param state    AI state
 * @param index    Goal index
 * @param progress New progress (0.0-1.0)
 */
void carbon_ai_update_goal_progress(Carbon_AIState *state,
                                     int index,
                                     float progress);

/**
 * Mark a goal as completed.
 *
 * @param state AI state
 * @param index Goal index
 */
void carbon_ai_complete_goal(Carbon_AIState *state, int index);

/**
 * Remove a goal.
 *
 * @param state AI state
 * @param index Goal index to remove
 */
void carbon_ai_remove_goal(Carbon_AIState *state, int index);

/**
 * Get the highest priority incomplete goal.
 *
 * @param state AI state
 * @return Goal pointer or NULL if none
 */
const Carbon_AIGoal *carbon_ai_get_primary_goal(const Carbon_AIState *state);

/**
 * Clean up completed and stale goals.
 *
 * @param state           AI state
 * @param max_stale_turns Remove goals older than this
 */
void carbon_ai_cleanup_goals(Carbon_AIState *state, int max_stale_turns);

/*============================================================================
 * Cooldowns
 *============================================================================*/

/**
 * Set a cooldown for an action type.
 *
 * @param state AI state
 * @param type  Action type
 * @param turns Turns to wait
 */
void carbon_ai_set_cooldown(Carbon_AIState *state,
                             Carbon_AIActionType type,
                             int turns);

/**
 * Check if an action type is on cooldown.
 *
 * @param state AI state
 * @param type  Action type
 * @return true if on cooldown
 */
bool carbon_ai_is_on_cooldown(const Carbon_AIState *state, Carbon_AIActionType type);

/**
 * Get remaining cooldown for an action type.
 *
 * @param state AI state
 * @param type  Action type
 * @return Remaining turns (0 if not on cooldown)
 */
int carbon_ai_get_cooldown(const Carbon_AIState *state, Carbon_AIActionType type);

/**
 * Decrement all cooldowns by one turn.
 *
 * @param state AI state
 */
void carbon_ai_update_cooldowns(Carbon_AIState *state);

/*============================================================================
 * Situation Analysis
 *============================================================================*/

/**
 * Update situational modifiers.
 * Uses registered situation analyzer if available.
 *
 * @param ai       AI system
 * @param state    AI state to update
 * @param game_ctx Game context
 */
void carbon_ai_update_situation(Carbon_AISystem *ai,
                                 Carbon_AIState *state,
                                 void *game_ctx);

/**
 * Set situational ratios manually.
 *
 * @param state          AI state
 * @param resources      Resource ratio (ours/average)
 * @param military       Military ratio
 * @param tech           Technology ratio
 */
void carbon_ai_set_ratios(Carbon_AIState *state,
                           float resources,
                           float military,
                           float tech);

/**
 * Set AI morale/confidence level.
 *
 * @param state  AI state
 * @param morale Morale level (0.0-1.0, 0.5 = neutral)
 */
void carbon_ai_set_morale(Carbon_AIState *state, float morale);

/*============================================================================
 * Targeting
 *============================================================================*/

/**
 * Set the primary enemy target.
 *
 * @param state     AI state
 * @param target_id Enemy faction ID (-1 to clear)
 */
void carbon_ai_set_primary_target(Carbon_AIState *state, int32_t target_id);

/**
 * Set the preferred ally target.
 *
 * @param state    AI state
 * @param ally_id  Faction ID to ally with (-1 to clear)
 */
void carbon_ai_set_ally_target(Carbon_AIState *state, int32_t ally_id);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get a human-readable name for a personality type.
 *
 * @param personality Personality type
 * @return Static string name
 */
const char *carbon_ai_personality_name(Carbon_AIPersonality personality);

/**
 * Get a human-readable name for an action type.
 *
 * @param type Action type
 * @return Static string name
 */
const char *carbon_ai_action_name(Carbon_AIActionType type);

/**
 * Generate a random float using the AI state's random generator.
 * Provides deterministic randomness for reproducible AI behavior.
 *
 * @param state AI state
 * @return Random float [0.0, 1.0)
 */
float carbon_ai_random(Carbon_AIState *state);

/**
 * Generate a random integer using the AI state's random generator.
 *
 * @param state AI state
 * @param min   Minimum value (inclusive)
 * @param max   Maximum value (inclusive)
 * @return Random integer [min, max]
 */
int carbon_ai_random_int(Carbon_AIState *state, int min, int max);

/**
 * Seed the AI random generator.
 *
 * @param state AI state
 * @param seed  Seed value (0 for time-based)
 */
void carbon_ai_seed_random(Carbon_AIState *state, uint32_t seed);

#endif /* CARBON_AI_H */
