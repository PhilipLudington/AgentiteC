#ifndef AGENTITE_AI_H
#define AGENTITE_AI_H

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
 *   Agentite_AISystem *ai = agentite_ai_create();
 *
 *   // Initialize per-faction AI state
 *   Agentite_AIState state;
 *   agentite_ai_state_init(&state, AGENTITE_AI_AGGRESSIVE);
 *
 *   // Register custom evaluators for game-specific actions
 *   agentite_ai_register_evaluator(ai, AGENTITE_AI_ACTION_ATTACK, evaluate_attacks);
 *   agentite_ai_register_evaluator(ai, AGENTITE_AI_ACTION_BUILD, evaluate_builds);
 *
 *   // Each turn, process AI decisions
 *   Agentite_AIDecision decision;
 *   agentite_ai_process_turn(ai, &state, game_context, &decision);
 *
 *   // Execute top actions
 *   for (int i = 0; i < decision.action_count; i++) {
 *       execute_action(&decision.actions[i]);
 *   }
 *
 *   // Cleanup
 *   agentite_ai_destroy(ai);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_AI_MAX_ACTIONS      16   /* Maximum actions per decision */
#define AGENTITE_AI_MAX_EVALUATORS   16   /* Maximum registered evaluators */
#define AGENTITE_AI_MAX_COOLDOWNS    8    /* Maximum cooldown trackers */
#define AGENTITE_AI_MAX_GOALS        8    /* Maximum concurrent goals */
#define AGENTITE_AI_MAX_THREATS      8    /* Maximum tracked threats */

/*============================================================================
 * AI Personality Types
 *============================================================================*/

/**
 * Built-in AI personality archetypes
 */
typedef enum Agentite_AIPersonality {
    AGENTITE_AI_BALANCED = 0,     /* Equal weights across all behaviors */
    AGENTITE_AI_AGGRESSIVE,       /* Prioritizes combat and conquest */
    AGENTITE_AI_DEFENSIVE,        /* Prioritizes protection and fortification */
    AGENTITE_AI_ECONOMIC,         /* Prioritizes resource generation */
    AGENTITE_AI_EXPANSIONIST,     /* Prioritizes territory acquisition */
    AGENTITE_AI_TECHNOLOGIST,     /* Prioritizes research and upgrades */
    AGENTITE_AI_DIPLOMATIC,       /* Prioritizes alliances and negotiation */
    AGENTITE_AI_OPPORTUNIST,      /* Adapts based on situation */
    AGENTITE_AI_PERSONALITY_COUNT,

    /* User-defined personalities start here */
    AGENTITE_AI_PERSONALITY_USER = 100,
} Agentite_AIPersonality;

/*============================================================================
 * AI Action Types
 *============================================================================*/

/**
 * Types of actions the AI can take
 */
typedef enum Agentite_AIActionType {
    AGENTITE_AI_ACTION_NONE = 0,
    AGENTITE_AI_ACTION_BUILD,         /* Construct buildings/units */
    AGENTITE_AI_ACTION_ATTACK,        /* Attack enemy targets */
    AGENTITE_AI_ACTION_DEFEND,        /* Defend owned territory */
    AGENTITE_AI_ACTION_EXPAND,        /* Claim new territory */
    AGENTITE_AI_ACTION_RESEARCH,      /* Research technologies */
    AGENTITE_AI_ACTION_DIPLOMACY,     /* Diplomatic actions */
    AGENTITE_AI_ACTION_RECRUIT,       /* Hire/train units */
    AGENTITE_AI_ACTION_RETREAT,       /* Withdraw from danger */
    AGENTITE_AI_ACTION_SCOUT,         /* Explore/gather intel */
    AGENTITE_AI_ACTION_TRADE,         /* Economic transactions */
    AGENTITE_AI_ACTION_UPGRADE,       /* Improve existing assets */
    AGENTITE_AI_ACTION_SPECIAL,       /* Game-specific special action */
    AGENTITE_AI_ACTION_COUNT,

    /* User-defined action types start here */
    AGENTITE_AI_ACTION_USER = 100,
} Agentite_AIActionType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Behavior weights that drive AI decisions.
 * Values are typically 0.0-1.0 but can exceed 1.0 for emphasis.
 * Higher values make the AI more likely to choose related actions.
 */
typedef struct Agentite_AIWeights {
    float aggression;       /* Weight for attack/combat actions */
    float defense;          /* Weight for defensive actions */
    float expansion;        /* Weight for territory expansion */
    float economy;          /* Weight for economic development */
    float technology;       /* Weight for research/upgrades */
    float diplomacy;        /* Weight for diplomatic actions */
    float caution;          /* Risk aversion (higher = more careful) */
    float opportunism;      /* React to immediate opportunities */
} Agentite_AIWeights;

/**
 * A single AI action with target and priority
 */
typedef struct Agentite_AIAction {
    Agentite_AIActionType type;   /* Action type */
    int32_t target_id;          /* Target entity/location/faction ID */
    int32_t secondary_id;       /* Secondary target (e.g., unit type to build) */
    float priority;             /* Priority score (higher = more important) */
    float urgency;              /* Time sensitivity (higher = do sooner) */
    void *data;                 /* Action-specific data (game-defined) */
    size_t data_size;           /* Size of data for serialization */
} Agentite_AIAction;

/**
 * Collection of AI actions representing a turn's decisions
 */
typedef struct Agentite_AIDecision {
    Agentite_AIAction actions[AGENTITE_AI_MAX_ACTIONS];
    int action_count;
    float total_score;          /* Combined score of all actions */
} Agentite_AIDecision;

/**
 * Tracked threat information
 */
typedef struct Agentite_AIThreat {
    int32_t source_id;          /* Threatening faction/entity ID */
    float level;                /* Threat level (0.0-1.0) */
    float distance;             /* Proximity (lower = closer/more urgent) */
    int32_t target_id;          /* What is being threatened */
    int turns_since_update;     /* Staleness counter */
} Agentite_AIThreat;

/**
 * AI goal tracking
 */
typedef struct Agentite_AIGoal {
    int32_t type;               /* Game-defined goal type */
    int32_t target_id;          /* Goal target */
    float priority;             /* Goal priority */
    float progress;             /* 0.0 to 1.0 completion */
    int turns_active;           /* How long pursuing this goal */
    bool completed;             /* Whether goal is achieved */
} Agentite_AIGoal;

/**
 * Per-faction AI state
 */
typedef struct Agentite_AIState {
    /* Personality and weights */
    Agentite_AIPersonality personality;
    Agentite_AIWeights weights;
    Agentite_AIWeights base_weights;  /* Original weights for reset */

    /* Strategic targets */
    int32_t primary_target;         /* Main enemy to focus on (-1 = none) */
    int32_t ally_target;            /* Faction to ally with (-1 = none) */

    /* Threat assessment */
    float overall_threat;           /* Global threat level (0.0-1.0) */
    Agentite_AIThreat threats[AGENTITE_AI_MAX_THREATS];
    int threat_count;

    /* Goals */
    Agentite_AIGoal goals[AGENTITE_AI_MAX_GOALS];
    int goal_count;

    /* Action cooldowns (prevent repetitive actions) */
    int cooldowns[AGENTITE_AI_MAX_COOLDOWNS];

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
} Agentite_AIState;

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
typedef void (*Agentite_AIEvaluator)(Agentite_AIState *state,
                                    void *game_ctx,
                                    Agentite_AIAction *out_actions,
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
typedef void (*Agentite_AIThreatAssessor)(Agentite_AIState *state,
                                         void *game_ctx,
                                         Agentite_AIThreat *threats,
                                         int *count,
                                         int max);

/**
 * Situation analyzer callback.
 * Called to update situational modifiers (ratios, morale, etc).
 *
 * @param state    AI state to update
 * @param game_ctx Game-specific context pointer
 */
typedef void (*Agentite_AISituationAnalyzer)(Agentite_AIState *state, void *game_ctx);

/*============================================================================
 * AI System
 *============================================================================*/

typedef struct Agentite_AISystem Agentite_AISystem;

/**
 * Create a new AI system.
 *
 * @return New AI system or NULL on failure
 */
Agentite_AISystem *agentite_ai_create(void);

/**
 * Destroy an AI system and free resources.
 *
 * @param ai AI system to destroy
 */
void agentite_ai_destroy(Agentite_AISystem *ai);

/*============================================================================
 * State Management
 *============================================================================*/

/**
 * Initialize an AI state with a personality.
 *
 * @param state       State to initialize
 * @param personality Personality type
 */
void agentite_ai_state_init(Agentite_AIState *state, Agentite_AIPersonality personality);

/**
 * Reset an AI state while preserving personality.
 *
 * @param state State to reset
 */
void agentite_ai_state_reset(Agentite_AIState *state);

/**
 * Get default weights for a personality type.
 *
 * @param personality Personality type
 * @param out        Output weights
 */
void agentite_ai_get_default_weights(Agentite_AIPersonality personality,
                                    Agentite_AIWeights *out);

/**
 * Set custom weights for an AI state.
 *
 * @param state   AI state
 * @param weights New weights to apply
 */
void agentite_ai_set_weights(Agentite_AIState *state, const Agentite_AIWeights *weights);

/**
 * Modify weights temporarily (e.g., in response to events).
 * Changes are applied as multipliers to current weights.
 *
 * @param state   AI state
 * @param weights Weight modifiers (1.0 = no change)
 */
void agentite_ai_modify_weights(Agentite_AIState *state, const Agentite_AIWeights *modifiers);

/**
 * Reset weights to base personality defaults.
 *
 * @param state AI state
 */
void agentite_ai_reset_weights(Agentite_AIState *state);

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
void agentite_ai_register_evaluator(Agentite_AISystem *ai,
                                   Agentite_AIActionType type,
                                   Agentite_AIEvaluator evaluator);

/**
 * Set the threat assessment callback.
 *
 * @param ai       AI system
 * @param assessor Threat assessment function
 */
void agentite_ai_set_threat_assessor(Agentite_AISystem *ai,
                                    Agentite_AIThreatAssessor assessor);

/**
 * Set the situation analysis callback.
 *
 * @param ai       AI system
 * @param analyzer Situation analyzer function
 */
void agentite_ai_set_situation_analyzer(Agentite_AISystem *ai,
                                       Agentite_AISituationAnalyzer analyzer);

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
void agentite_ai_process_turn(Agentite_AISystem *ai,
                             Agentite_AIState *state,
                             void *game_ctx,
                             Agentite_AIDecision *out);

/**
 * Score a single action based on AI state and weights.
 *
 * @param state      AI state
 * @param type       Action type
 * @param base_score Base score before weight application
 * @return Weighted score
 */
float agentite_ai_score_action(const Agentite_AIState *state,
                              Agentite_AIActionType type,
                              float base_score);

/**
 * Sort actions in a decision by priority (highest first).
 *
 * @param decision Decision to sort
 */
void agentite_ai_sort_actions(Agentite_AIDecision *decision);

/**
 * Get the top N actions from a decision.
 *
 * @param decision Source decision
 * @param out      Output array
 * @param max      Maximum actions to return
 * @return Number of actions returned
 */
int agentite_ai_get_top_actions(const Agentite_AIDecision *decision,
                               Agentite_AIAction *out,
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
void agentite_ai_update_threats(Agentite_AISystem *ai,
                               Agentite_AIState *state,
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
void agentite_ai_add_threat(Agentite_AIState *state,
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
void agentite_ai_remove_threat(Agentite_AIState *state, int32_t source_id);

/**
 * Get the highest threat.
 *
 * @param state AI state
 * @return Highest threat or NULL if none
 */
const Agentite_AIThreat *agentite_ai_get_highest_threat(const Agentite_AIState *state);

/**
 * Calculate overall threat level from individual threats.
 *
 * @param state AI state
 * @return Combined threat level (0.0-1.0)
 */
float agentite_ai_calculate_threat_level(Agentite_AIState *state);

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
int agentite_ai_add_goal(Agentite_AIState *state,
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
void agentite_ai_update_goal_progress(Agentite_AIState *state,
                                     int index,
                                     float progress);

/**
 * Mark a goal as completed.
 *
 * @param state AI state
 * @param index Goal index
 */
void agentite_ai_complete_goal(Agentite_AIState *state, int index);

/**
 * Remove a goal.
 *
 * @param state AI state
 * @param index Goal index to remove
 */
void agentite_ai_remove_goal(Agentite_AIState *state, int index);

/**
 * Get the highest priority incomplete goal.
 *
 * @param state AI state
 * @return Goal pointer or NULL if none
 */
const Agentite_AIGoal *agentite_ai_get_primary_goal(const Agentite_AIState *state);

/**
 * Clean up completed and stale goals.
 *
 * @param state           AI state
 * @param max_stale_turns Remove goals older than this
 */
void agentite_ai_cleanup_goals(Agentite_AIState *state, int max_stale_turns);

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
void agentite_ai_set_cooldown(Agentite_AIState *state,
                             Agentite_AIActionType type,
                             int turns);

/**
 * Check if an action type is on cooldown.
 *
 * @param state AI state
 * @param type  Action type
 * @return true if on cooldown
 */
bool agentite_ai_is_on_cooldown(const Agentite_AIState *state, Agentite_AIActionType type);

/**
 * Get remaining cooldown for an action type.
 *
 * @param state AI state
 * @param type  Action type
 * @return Remaining turns (0 if not on cooldown)
 */
int agentite_ai_get_cooldown(const Agentite_AIState *state, Agentite_AIActionType type);

/**
 * Decrement all cooldowns by one turn.
 *
 * @param state AI state
 */
void agentite_ai_update_cooldowns(Agentite_AIState *state);

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
void agentite_ai_update_situation(Agentite_AISystem *ai,
                                 Agentite_AIState *state,
                                 void *game_ctx);

/**
 * Set situational ratios manually.
 *
 * @param state          AI state
 * @param resources      Resource ratio (ours/average)
 * @param military       Military ratio
 * @param tech           Technology ratio
 */
void agentite_ai_set_ratios(Agentite_AIState *state,
                           float resources,
                           float military,
                           float tech);

/**
 * Set AI morale/confidence level.
 *
 * @param state  AI state
 * @param morale Morale level (0.0-1.0, 0.5 = neutral)
 */
void agentite_ai_set_morale(Agentite_AIState *state, float morale);

/*============================================================================
 * Targeting
 *============================================================================*/

/**
 * Set the primary enemy target.
 *
 * @param state     AI state
 * @param target_id Enemy faction ID (-1 to clear)
 */
void agentite_ai_set_primary_target(Agentite_AIState *state, int32_t target_id);

/**
 * Set the preferred ally target.
 *
 * @param state    AI state
 * @param ally_id  Faction ID to ally with (-1 to clear)
 */
void agentite_ai_set_ally_target(Agentite_AIState *state, int32_t ally_id);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get a human-readable name for a personality type.
 *
 * @param personality Personality type
 * @return Static string name
 */
const char *agentite_ai_personality_name(Agentite_AIPersonality personality);

/**
 * Get a human-readable name for an action type.
 *
 * @param type Action type
 * @return Static string name
 */
const char *agentite_ai_action_name(Agentite_AIActionType type);

/**
 * Generate a random float using the AI state's random generator.
 * Provides deterministic randomness for reproducible AI behavior.
 *
 * @param state AI state
 * @return Random float [0.0, 1.0)
 */
float agentite_ai_random(Agentite_AIState *state);

/**
 * Generate a random integer using the AI state's random generator.
 *
 * @param state AI state
 * @param min   Minimum value (inclusive)
 * @param max   Maximum value (inclusive)
 * @return Random integer [min, max]
 */
int agentite_ai_random_int(Agentite_AIState *state, int min, int max);

/**
 * Seed the AI random generator.
 *
 * @param state AI state
 * @param seed  Seed value (0 for time-based)
 */
void agentite_ai_seed_random(Agentite_AIState *state, uint32_t seed);

#endif /* AGENTITE_AI_H */
