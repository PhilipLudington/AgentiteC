#ifndef CARBON_TECH_H
#define CARBON_TECH_H

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
 *   Carbon_TechTree *tree = carbon_tech_create();
 *
 *   // Define a technology
 *   Carbon_TechDef tech = {
 *       .id = "improved_farming",
 *       .name = "Improved Farming",
 *       .description = "Increases food production by 20%",
 *       .branch = TECH_BRANCH_ECONOMY,
 *       .tier = 1,
 *       .research_cost = 100,
 *       .prereq_count = 0,
 *       .effect_count = 1,
 *   };
 *   tech.effects[0] = (Carbon_TechEffect){
 *       .type = TECH_EFFECT_RESOURCE_BONUS,
 *       .target = RESOURCE_FOOD,
 *       .value = 0.20f,
 *   };
 *   carbon_tech_register(tree, &tech);
 *
 *   // Start research (per-faction state)
 *   Carbon_TechState state;
 *   carbon_tech_state_init(&state);
 *   carbon_tech_start_research(tree, &state, "improved_farming");
 *
 *   // Each turn, add research points
 *   if (carbon_tech_add_points(tree, &state, research_per_turn)) {
 *       // Tech completed!
 *   }
 *
 *   // Cleanup
 *   carbon_tech_destroy(tree);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_TECH_MAX              256   /* Maximum technologies */
#define CARBON_TECH_MAX_PREREQS      4     /* Prerequisites per tech */
#define CARBON_TECH_MAX_EFFECTS      4     /* Effects per tech */
#define CARBON_TECH_MAX_RESOURCE_COSTS 4   /* Different resource costs per tech */
#define CARBON_TECH_MAX_ACTIVE       4     /* Concurrent research slots */

/*============================================================================
 * Effect Types
 *============================================================================*/

/**
 * Technology effect types (game can extend with custom values >= 100)
 */
typedef enum Carbon_TechEffectType {
    CARBON_TECH_EFFECT_NONE = 0,

    /* Resource effects */
    CARBON_TECH_EFFECT_RESOURCE_BONUS,      /* Increase resource generation */
    CARBON_TECH_EFFECT_RESOURCE_CAP,        /* Increase resource maximum */
    CARBON_TECH_EFFECT_COST_REDUCTION,      /* Reduce costs by percentage */

    /* Production effects */
    CARBON_TECH_EFFECT_PRODUCTION_SPEED,    /* Faster building/unit production */
    CARBON_TECH_EFFECT_UNLOCK_UNIT,         /* Enable a unit type */
    CARBON_TECH_EFFECT_UNLOCK_BUILDING,     /* Enable a building type */
    CARBON_TECH_EFFECT_UNLOCK_ABILITY,      /* Enable an ability */

    /* Combat effects */
    CARBON_TECH_EFFECT_ATTACK_BONUS,        /* Increase attack stat */
    CARBON_TECH_EFFECT_DEFENSE_BONUS,       /* Increase defense stat */
    CARBON_TECH_EFFECT_HEALTH_BONUS,        /* Increase health */
    CARBON_TECH_EFFECT_RANGE_BONUS,         /* Increase range */
    CARBON_TECH_EFFECT_SPEED_BONUS,         /* Increase movement speed */

    /* Miscellaneous */
    CARBON_TECH_EFFECT_VISION_BONUS,        /* Increase sight range */
    CARBON_TECH_EFFECT_EXPERIENCE_BONUS,    /* Increase XP gain */
    CARBON_TECH_EFFECT_CUSTOM,              /* Game-defined effect */

    /* User-defined effects start here */
    CARBON_TECH_EFFECT_USER = 100,
} Carbon_TechEffectType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Single technology effect
 */
typedef struct Carbon_TechEffect {
    Carbon_TechEffectType type;
    int32_t target;             /* Resource type, unit ID, etc. (game-defined) */
    float value;                /* Effect magnitude (0.2 = +20%, etc.) */
    char modifier_source[32];   /* Name for modifier stack (optional) */
} Carbon_TechEffect;

/**
 * Resource cost for researching a technology
 */
typedef struct Carbon_TechCost {
    int32_t resource_type;      /* Game-defined resource index */
    int32_t amount;             /* Cost amount */
} Carbon_TechCost;

/**
 * Technology definition (static data)
 */
typedef struct Carbon_TechDef {
    /* Identity */
    char id[64];                /* Unique identifier */
    char name[128];             /* Display name */
    char description[256];      /* Description text */

    /* Organization */
    int32_t branch;             /* Tech branch/category (game-defined) */
    int32_t tier;               /* Tech tier (0 = base, 1+, higher = later) */

    /* Research cost */
    int32_t research_cost;      /* Research points required */
    Carbon_TechCost resource_costs[CARBON_TECH_MAX_RESOURCE_COSTS];
    int resource_cost_count;

    /* Prerequisites */
    char prerequisites[CARBON_TECH_MAX_PREREQS][64];  /* Tech IDs required */
    int prereq_count;

    /* Effects when completed */
    Carbon_TechEffect effects[CARBON_TECH_MAX_EFFECTS];
    int effect_count;

    /* Flags */
    bool repeatable;            /* Can be researched multiple times */
    bool hidden;                /* Hidden until prerequisites met */
} Carbon_TechDef;

/**
 * Active research slot (for concurrent research)
 */
typedef struct Carbon_ActiveResearch {
    char tech_id[64];           /* Technology being researched */
    int32_t points_invested;    /* Points spent so far */
    int32_t points_required;    /* Total points needed */
} Carbon_ActiveResearch;

/**
 * Per-faction technology state
 */
typedef struct Carbon_TechState {
    /* Completion tracking (bitmask for up to 64 techs, or use for speed) */
    uint64_t completed_mask;    /* Fast lookup for first 64 techs */
    bool completed[CARBON_TECH_MAX]; /* Full completion array */
    int completed_count;

    /* Repeat counts (for repeatable techs) */
    int8_t repeat_count[CARBON_TECH_MAX];

    /* Active research */
    Carbon_ActiveResearch active[CARBON_TECH_MAX_ACTIVE];
    int active_count;
} Carbon_TechState;

/**
 * Callback for tech completion (optional)
 */
typedef void (*Carbon_TechCallback)(const Carbon_TechDef *tech,
                                     Carbon_TechState *state,
                                     void *userdata);

/*============================================================================
 * Tech Tree Manager
 *============================================================================*/

typedef struct Carbon_TechTree Carbon_TechTree;

/**
 * Forward declaration for event dispatcher integration
 */
typedef struct Carbon_EventDispatcher Carbon_EventDispatcher;

/**
 * Create a new technology tree.
 *
 * @return New tech tree or NULL on failure
 */
Carbon_TechTree *carbon_tech_create(void);

/**
 * Create a tech tree with event dispatcher integration.
 * Events will be emitted when techs are started/completed.
 *
 * @param events Event dispatcher (can be NULL)
 * @return New tech tree or NULL on failure
 */
Carbon_TechTree *carbon_tech_create_with_events(Carbon_EventDispatcher *events);

/**
 * Destroy a tech tree and free resources.
 *
 * @param tree Tech tree to destroy
 */
void carbon_tech_destroy(Carbon_TechTree *tree);

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
int carbon_tech_register(Carbon_TechTree *tree, const Carbon_TechDef *def);

/**
 * Get the number of registered technologies.
 *
 * @param tree Tech tree
 * @return Number of technologies
 */
int carbon_tech_count(const Carbon_TechTree *tree);

/**
 * Get a technology by index.
 *
 * @param tree  Tech tree
 * @param index Technology index
 * @return Technology definition or NULL
 */
const Carbon_TechDef *carbon_tech_get(const Carbon_TechTree *tree, int index);

/**
 * Find a technology by ID.
 *
 * @param tree Tech tree
 * @param id   Technology ID
 * @return Technology definition or NULL if not found
 */
const Carbon_TechDef *carbon_tech_find(const Carbon_TechTree *tree, const char *id);

/**
 * Get the index of a technology by ID.
 *
 * @param tree Tech tree
 * @param id   Technology ID
 * @return Technology index or -1 if not found
 */
int carbon_tech_find_index(const Carbon_TechTree *tree, const char *id);

/*============================================================================
 * Technology State Management
 *============================================================================*/

/**
 * Initialize a tech state (call before use).
 *
 * @param state Tech state to initialize
 */
void carbon_tech_state_init(Carbon_TechState *state);

/**
 * Reset a tech state (clear all progress).
 *
 * @param state Tech state to reset
 */
void carbon_tech_state_reset(Carbon_TechState *state);

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
bool carbon_tech_is_researched(const Carbon_TechTree *tree,
                                const Carbon_TechState *state,
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
bool carbon_tech_can_research(const Carbon_TechTree *tree,
                               const Carbon_TechState *state,
                               const char *id);

/**
 * Check if all prerequisites for a technology are met.
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return true if prerequisites are satisfied
 */
bool carbon_tech_has_prerequisites(const Carbon_TechTree *tree,
                                    const Carbon_TechState *state,
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
bool carbon_tech_start_research(Carbon_TechTree *tree,
                                 Carbon_TechState *state,
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
bool carbon_tech_add_points(Carbon_TechTree *tree,
                             Carbon_TechState *state,
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
bool carbon_tech_add_points_to_slot(Carbon_TechTree *tree,
                                     Carbon_TechState *state,
                                     int slot,
                                     int32_t points);

/**
 * Immediately complete a technology (cheat/debug).
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 */
void carbon_tech_complete(Carbon_TechTree *tree,
                           Carbon_TechState *state,
                           const char *id);

/**
 * Cancel active research.
 *
 * @param state Tech state
 * @param slot  Research slot to cancel (0 to active_count-1)
 */
void carbon_tech_cancel_research(Carbon_TechState *state, int slot);

/**
 * Cancel all active research.
 *
 * @param state Tech state
 */
void carbon_tech_cancel_all_research(Carbon_TechState *state);

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
float carbon_tech_get_progress(const Carbon_TechState *state, int slot);

/**
 * Get remaining research points needed.
 *
 * @param state Tech state
 * @param slot  Research slot index
 * @return Remaining points (0 if not researching)
 */
int32_t carbon_tech_get_remaining(const Carbon_TechState *state, int slot);

/**
 * Check if currently researching a specific technology.
 *
 * @param state Tech state
 * @param id    Technology ID
 * @return true if actively researching this tech
 */
bool carbon_tech_is_researching(const Carbon_TechState *state, const char *id);

/**
 * Get the number of active research slots in use.
 *
 * @param state Tech state
 * @return Number of active research slots
 */
int carbon_tech_active_count(const Carbon_TechState *state);

/**
 * Get how many times a repeatable tech has been completed.
 *
 * @param tree  Tech tree
 * @param state Tech state
 * @param id    Technology ID
 * @return Completion count (0 if never completed)
 */
int carbon_tech_get_repeat_count(const Carbon_TechTree *tree,
                                  const Carbon_TechState *state,
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
int carbon_tech_get_available(const Carbon_TechTree *tree,
                               const Carbon_TechState *state,
                               const Carbon_TechDef **out_defs,
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
int carbon_tech_get_completed(const Carbon_TechTree *tree,
                               const Carbon_TechState *state,
                               const Carbon_TechDef **out_defs,
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
int carbon_tech_get_by_branch(const Carbon_TechTree *tree,
                               int32_t branch,
                               const Carbon_TechDef **out_defs,
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
int carbon_tech_get_by_tier(const Carbon_TechTree *tree,
                             int32_t tier,
                             const Carbon_TechDef **out_defs,
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
void carbon_tech_set_completion_callback(Carbon_TechTree *tree,
                                          Carbon_TechCallback callback,
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
const char *carbon_tech_effect_type_name(Carbon_TechEffectType type);

/**
 * Calculate total research points needed for a tech at a given repeat level.
 * For repeatable techs, cost may increase with each completion.
 *
 * @param def          Technology definition
 * @param repeat_count Current completion count
 * @return Research points required
 */
int32_t carbon_tech_calculate_cost(const Carbon_TechDef *def, int repeat_count);

#endif /* CARBON_TECH_H */
