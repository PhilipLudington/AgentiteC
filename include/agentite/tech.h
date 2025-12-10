#ifndef AGENTITE_TECH_H
#define AGENTITE_TECH_H

#include <stdbool.h>
#include <stdint.h>

/**
 * Carbon Technology Tree System
 *
 * A research system with prerequisites, multiple resource costs, branching,
 * and effect application. Integrates with the event dispatcher for notifications.
 *
 * Usage:
 *   // Create tech tree
 *   Agentite_TechTree *tree = agentite_tech_create();
 *
 *   // Define a technology
 *   Agentite_TechDef tech = {
 *       .id = "improved_farming",
 *       .name = "Improved Farming",
 *       .description = "Increases food production by 20%",
 *       .branch = TECH_BRANCH_ECONOMY,
 *       .tier = 1,
 *       .research_cost = 100,
 *       .prereq_count = 0,
 *       .effect_count = 1,
 *   };
 *   tech.effects[0] = (Agentite_TechEffect){
 *       .type = TECH_EFFECT_RESOURCE_BONUS,
 *       .target = RESOURCE_FOOD,
 *       .value = 0.20f,
 *   };
 *   agentite_tech_register(tree, &tech);
 *
 *   // Start research (per-faction state)
 *   Agentite_TechState state;
 *   agentite_tech_state_init(&state);
 *   agentite_tech_start_research(tree, &state, "improved_farming");
 *
 *   // Each turn, add research points
 *   if (agentite_tech_add_points(tree, &state, research_per_turn)) {
 *       // Tech completed!
 *   }
 *
 *   // Cleanup
 *   agentite_tech_destroy(tree);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_TECH_MAX              256   /* Maximum technologies */
#define AGENTITE_TECH_MAX_PREREQS      4     /* Prerequisites per tech */
#define AGENTITE_TECH_MAX_EFFECTS      4     /* Effects per tech */
#define AGENTITE_TECH_MAX_RESOURCE_COSTS 4   /* Different resource costs per tech */
#define AGENTITE_TECH_MAX_ACTIVE       4     /* Concurrent research slots */

/*============================================================================
 * Effect Types
 *============================================================================*/

/**
 * Technology effect types (game can extend with custom values >= 100)
 */
typedef enum Agentite_TechEffectType {
    AGENTITE_TECH_EFFECT_NONE = 0,

    /* Resource effects */
    AGENTITE_TECH_EFFECT_RESOURCE_BONUS,      /* Increase resource generation */
    AGENTITE_TECH_EFFECT_RESOURCE_CAP,        /* Increase resource maximum */
    AGENTITE_TECH_EFFECT_COST_REDUCTION,      /* Reduce costs by percentage */

    /* Production effects */
    AGENTITE_TECH_EFFECT_PRODUCTION_SPEED,    /* Faster building/unit production */
    AGENTITE_TECH_EFFECT_UNLOCK_UNIT,         /* Enable a unit type */
    AGENTITE_TECH_EFFECT_UNLOCK_BUILDING,     /* Enable a building type */
    AGENTITE_TECH_EFFECT_UNLOCK_ABILITY,      /* Enable an ability */

    /* Combat effects */
    AGENTITE_TECH_EFFECT_ATTACK_BONUS,        /* Increase attack stat */
    AGENTITE_TECH_EFFECT_DEFENSE_BONUS,       /* Increase defense stat */
    AGENTITE_TECH_EFFECT_HEALTH_BONUS,        /* Increase health */
    AGENTITE_TECH_EFFECT_RANGE_BONUS,         /* Increase range */
    AGENTITE_TECH_EFFECT_SPEED_BONUS,         /* Increase movement speed */

    /* Miscellaneous */
    AGENTITE_TECH_EFFECT_VISION_BONUS,        /* Increase sight range */
    AGENTITE_TECH_EFFECT_EXPERIENCE_BONUS,    /* Increase XP gain */
    AGENTITE_TECH_EFFECT_CUSTOM,              /* Game-defined effect */

    /* User-defined effects start here */
    AGENTITE_TECH_EFFECT_USER = 100,
} Agentite_TechEffectType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Single technology effect
 */
typedef struct Agentite_TechEffect {
    Agentite_TechEffectType type;
    int32_t target;             /* Resource type, unit ID, etc. (game-defined) */
    float value;                /* Effect magnitude (0.2 = +20%, etc.) */
    char modifier_source[32];   /* Name for modifier stack (optional) */
} Agentite_TechEffect;

/**
 * Resource cost for researching a technology
 */
typedef struct Agentite_TechCost {
    int32_t resource_type;      /* Game-defined resource index */
    int32_t amount;             /* Cost amount */
} Agentite_TechCost;

/**
 * Technology definition (static data)
 */
typedef struct Agentite_TechDef {
    /* Identity */
    char id[64];                /* Unique identifier */
    char name[128];             /* Display name */
    char description[256];      /* Description text */

    /* Organization */
    int32_t branch;             /* Tech branch/category (game-defined) */
    int32_t tier;               /* Tech tier (0 = base, 1+, higher = later) */

    /* Research cost */
    int32_t research_cost;      /* Research points required */
    Agentite_TechCost resource_costs[AGENTITE_TECH_MAX_RESOURCE_COSTS];
    int resource_cost_count;

    /* Prerequisites */
    char prerequisites[AGENTITE_TECH_MAX_PREREQS][64];  /* Tech IDs required */
    int prereq_count;

    /* Effects when completed */
    Agentite_TechEffect effects[AGENTITE_TECH_MAX_EFFECTS];
    int effect_count;

    /* Flags */
    bool repeatable;            /* Can be researched multiple times */
    bool hidden;                /* Hidden until prerequisites met */
} Agentite_TechDef;

/**
 * Active research slot (for concurrent research)
 */
typedef struct Agentite_ActiveResearch {
    char tech_id[64];           /* Technology being researched */
    int32_t points_invested;    /* Points spent so far */
    int32_t points_required;    /* Total points needed */
} Agentite_ActiveResearch;

/**
 * Per-faction technology state
 */
typedef struct Agentite_TechState {
    /* Completion tracking (bitmask for up to 64 techs, or use for speed) */
    uint64_t completed_mask;    /* Fast lookup for first 64 techs */
    bool completed[AGENTITE_TECH_MAX]; /* Full completion array */
    int completed_count;

    /* Repeat counts (for repeatable techs) */
    int8_t repeat_count[AGENTITE_TECH_MAX];

    /* Active research */
    Agentite_ActiveResearch active[AGENTITE_TECH_MAX_ACTIVE];
    int active_count;
} Agentite_TechState;

/**
 * Callback for tech completion (optional)
 */
typedef void (*Agentite_TechCallback)(const Agentite_TechDef *tech,
                                     Agentite_TechState *state,
                                     void *userdata);

/*============================================================================
 * Tech Tree Manager
 *============================================================================*/

typedef struct Agentite_TechTree Agentite_TechTree;

/**
 * Forward declaration for event dispatcher integration
 */
typedef struct Agentite_EventDispatcher Agentite_EventDispatcher;

/**
 * Create a new technology tree.
 *
 * @return New tech tree or NULL on failure
 */
Agentite_TechTree *agentite_tech_create(void);

/**
 * Create a tech tree with event dispatcher integration.
 * Events will be emitted when techs are started/completed.
 *
 * @param events Event dispatcher (can be NULL)
 * @return New tech tree or NULL on failure
 */
Agentite_TechTree *agentite_tech_create_with_events(Agentite_EventDispatcher *events);

/**
 * Destroy a tech tree and free resources.
 *
 * @param tree Tech tree to destroy
 */
void agentite_tech_destroy(Agentite_TechTree *tree);

/*============================================================================
 * Technology Registration
 *============================================================================*/

/**
 * Register a technology definition.
 *
 * @param tree Tech tree
 * @param def  Technology definition (copied)
 * @return Technology index (0+) or -1 on failure
 */
int agentite_tech_register(Agentite_TechTree *tree, const Agentite_TechDef *def);

/**
 * Get the number of registered technologies.
 *
 * @param tree Tech tree
 * @return Number of technologies
 */
int agentite_tech_count(const Agentite_TechTree *tree);

/**
 * Get a technology by index.
 *
 * @param tree  Tech tree
 * @param index Technology index
 * @return Technology definition or NULL
 */
const Agentite_TechDef *agentite_tech_get(const Agentite_TechTree *tree, int index);

/**
 * Find a technology by ID.
 *
 * @param tree Tech tree
 * @param id   Technology ID
 * @return Technology definition or NULL if not found
 */
const Agentite_TechDef *agentite_tech_find(const Agentite_TechTree *tree, const char *id);

/**
 * Get the index of a technology by ID.
 *
 * @param tree Tech tree
 * @param id   Technology ID
 * @return Technology index or -1 if not found
 */
int agentite_tech_find_index(const Agentite_TechTree *tree, const char *id);

/*============================================================================
 * Technology State Management
 *============================================================================*/

/**
 * Initialize a tech state (call before use).
 *
 * @param state Tech state to initialize
 */
void agentite_tech_state_init(Agentite_TechState *state);

/**
 * Reset a tech state (clear all progress).
 *
 * @param state Tech state to reset
 */
void agentite_tech_state_reset(Agentite_TechState *state);

/*============================================================================
 * Research Operations
 *============================================================================*/

/**
 * Check if a technology has been researched.
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return true if researched
 */
bool agentite_tech_is_researched(const Agentite_TechTree *tree,
                                const Agentite_TechState *state,
                                const char *id);

/**
 * Check if a technology can be researched.
 * Returns true if prerequisites are met and not already researched
 * (unless repeatable).
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return true if can be researched
 */
bool agentite_tech_can_research(const Agentite_TechTree *tree,
                               const Agentite_TechState *state,
                               const char *id);

/**
 * Check if all prerequisites for a technology are met.
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return true if prerequisites are satisfied
 */
bool agentite_tech_has_prerequisites(const Agentite_TechTree *tree,
                                    const Agentite_TechState *state,
                                    const char *id);

/**
 * Start researching a technology.
 * Returns false if prerequisites not met or no available research slots.
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return true if research started
 */
bool agentite_tech_start_research(Agentite_TechTree *tree,
                                 Agentite_TechState *state,
                                 const char *id);

/**
 * Add research points to active research.
 * If multiple techs are being researched, distributes to the first slot.
 * Returns true if any technology was completed.
 *
 * @param tree   Tech tree
 * @param state  Tech state
 * @param points Research points to add
 * @return true if a technology completed
 */
bool agentite_tech_add_points(Agentite_TechTree *tree,
                             Agentite_TechState *state,
                             int32_t points);

/**
 * Add research points to a specific research slot.
 *
 * @param tree   Tech tree
 * @param state  Tech state
 * @param slot   Research slot index (0 to active_count-1)
 * @param points Research points to add
 * @return true if the technology completed
 */
bool agentite_tech_add_points_to_slot(Agentite_TechTree *tree,
                                     Agentite_TechState *state,
                                     int slot,
                                     int32_t points);

/**
 * Immediately complete a technology (cheat/debug).
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 */
void agentite_tech_complete(Agentite_TechTree *tree,
                           Agentite_TechState *state,
                           const char *id);

/**
 * Cancel active research.
 *
 * @param state Tech state
 * @param slot  Research slot to cancel (0 to active_count-1)
 */
void agentite_tech_cancel_research(Agentite_TechState *state, int slot);

/**
 * Cancel all active research.
 *
 * @param state Tech state
 */
void agentite_tech_cancel_all_research(Agentite_TechState *state);

/*============================================================================
 * Query Functions
 *============================================================================*/

/**
 * Get research progress as a percentage.
 *
 * @param state Tech state
 * @param slot  Research slot index
 * @return Progress from 0.0 to 1.0
 */
float agentite_tech_get_progress(const Agentite_TechState *state, int slot);

/**
 * Get remaining research points needed.
 *
 * @param state Tech state
 * @param slot  Research slot index
 * @return Remaining points (0 if not researching)
 */
int32_t agentite_tech_get_remaining(const Agentite_TechState *state, int slot);

/**
 * Check if currently researching a specific technology.
 *
 * @param state Tech state
 * @param id    Technology ID
 * @return true if actively researching this tech
 */
bool agentite_tech_is_researching(const Agentite_TechState *state, const char *id);

/**
 * Get the number of active research slots in use.
 *
 * @param state Tech state
 * @return Number of active research slots
 */
int agentite_tech_active_count(const Agentite_TechState *state);

/**
 * Get how many times a repeatable tech has been completed.
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return Completion count (0 if never completed)
 */
int agentite_tech_get_repeat_count(const Agentite_TechTree *tree,
                                  const Agentite_TechState *state,
                                  const char *id);

/*============================================================================
 * Filtered Queries
 *============================================================================*/

/**
 * Get all available (researchable) technologies.
 *
 * @param tree      Tech tree
 * @param state     Tech state
 * @param out_defs  Output array for tech definitions
 * @param max_count Maximum number to return
 * @return Number of available techs
 */
int agentite_tech_get_available(const Agentite_TechTree *tree,
                               const Agentite_TechState *state,
                               const Agentite_TechDef **out_defs,
                               int max_count);

/**
 * Get all completed technologies.
 *
 * @param tree      Tech tree
 * @param state     Tech state
 * @param out_defs  Output array for tech definitions
 * @param max_count Maximum number to return
 * @return Number of completed techs
 */
int agentite_tech_get_completed(const Agentite_TechTree *tree,
                               const Agentite_TechState *state,
                               const Agentite_TechDef **out_defs,
                               int max_count);

/**
 * Get technologies by branch.
 *
 * @param tree      Tech tree
 * @param branch    Branch ID (game-defined)
 * @param out_defs  Output array for tech definitions
 * @param max_count Maximum number to return
 * @return Number of techs in branch
 */
int agentite_tech_get_by_branch(const Agentite_TechTree *tree,
                               int32_t branch,
                               const Agentite_TechDef **out_defs,
                               int max_count);

/**
 * Get technologies by tier.
 *
 * @param tree      Tech tree
 * @param tier      Tier number
 * @param out_defs  Output array for tech definitions
 * @param max_count Maximum number to return
 * @return Number of techs in tier
 */
int agentite_tech_get_by_tier(const Agentite_TechTree *tree,
                             int32_t tier,
                             const Agentite_TechDef **out_defs,
                             int max_count);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set a callback for when technologies are completed.
 *
 * @param tree     Tech tree
 * @param callback Function to call on completion
 * @param userdata User data to pass to callback
 */
void agentite_tech_set_completion_callback(Agentite_TechTree *tree,
                                          Agentite_TechCallback callback,
                                          void *userdata);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get a human-readable name for an effect type.
 *
 * @param type Effect type
 * @return Static string name
 */
const char *agentite_tech_effect_type_name(Agentite_TechEffectType type);

/**
 * Calculate total research points needed for a tech at a given repeat level.
 * For repeatable techs, cost may increase with each completion.
 *
 * @param def          Technology definition
 * @param repeat_count Current completion count
 * @return Research points required
 */
int32_t agentite_tech_calculate_cost(const Agentite_TechDef *def, int repeat_count);

#endif /* AGENTITE_TECH_H */
