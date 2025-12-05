#ifndef CARBON_ANOMALY_H
#define CARBON_ANOMALY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Anomaly / Discovery System
 *
 * Discoverable points of interest with research/investigation mechanics.
 * Supports anomaly type registry with rarity tiers, discovery and research
 * status tracking, research progress over time, and reward distribution.
 *
 * Usage:
 *   // Create registry and define anomaly types
 *   Carbon_AnomalyRegistry *registry = carbon_anomaly_registry_create();
 *   Carbon_AnomalyTypeDef def = {
 *       .id = "ancient_ruins",
 *       .name = "Ancient Ruins",
 *       .description = "Mysterious structures from a forgotten era",
 *       .rarity = CARBON_ANOMALY_UNCOMMON,
 *       .research_time = 10.0f,
 *   };
 *   int type_id = carbon_anomaly_register_type(registry, &def);
 *
 *   // Create manager and spawn anomalies
 *   Carbon_AnomalyManager *mgr = carbon_anomaly_manager_create(registry);
 *   uint32_t anomaly = carbon_anomaly_spawn(mgr, type_id, 50, 50, 0);
 *
 *   // Discover and research
 *   carbon_anomaly_discover(mgr, anomaly, player_faction);
 *   carbon_anomaly_start_research(mgr, anomaly, player_faction, scientist_entity);
 *
 *   // In game loop
 *   carbon_anomaly_add_progress(mgr, anomaly, delta_time);
 *   if (carbon_anomaly_is_complete(mgr, anomaly)) {
 *       // Collect rewards
 *   }
 *
 *   // Cleanup
 *   carbon_anomaly_manager_destroy(mgr);
 *   carbon_anomaly_registry_destroy(registry);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_ANOMALY_MAX_TYPES       64    /* Maximum anomaly types */
#define CARBON_ANOMALY_MAX_INSTANCES   256   /* Maximum active anomalies */
#define CARBON_ANOMALY_INVALID         0     /* Invalid anomaly handle */
#define CARBON_ANOMALY_NAME_MAX        32    /* Max name length */
#define CARBON_ANOMALY_DESC_MAX        128   /* Max description length */
#define CARBON_ANOMALY_ID_MAX          32    /* Max string ID length */

/*============================================================================
 * Rarity Tiers
 *============================================================================*/

/**
 * Anomaly rarity tiers
 * Affects spawn probability and typically reward quality
 */
typedef enum Carbon_AnomalyRarity {
    CARBON_ANOMALY_COMMON = 0,      /* ~60% of spawns */
    CARBON_ANOMALY_UNCOMMON,        /* ~25% of spawns */
    CARBON_ANOMALY_RARE,            /* ~12% of spawns */
    CARBON_ANOMALY_LEGENDARY,       /* ~3% of spawns */
    CARBON_ANOMALY_RARITY_COUNT,
} Carbon_AnomalyRarity;

/*============================================================================
 * Status
 *============================================================================*/

/**
 * Anomaly discovery and research status
 */
typedef enum Carbon_AnomalyStatus {
    CARBON_ANOMALY_UNDISCOVERED = 0,  /* Not yet found */
    CARBON_ANOMALY_DISCOVERED,        /* Found but not researched */
    CARBON_ANOMALY_RESEARCHING,       /* Currently being researched */
    CARBON_ANOMALY_COMPLETED,         /* Research complete */
    CARBON_ANOMALY_DEPLETED,          /* Rewards collected, no further use */
} Carbon_AnomalyStatus;

/*============================================================================
 * Reward Types
 *============================================================================*/

/**
 * Types of rewards from completing anomaly research
 */
typedef enum Carbon_AnomalyRewardType {
    CARBON_ANOMALY_REWARD_NONE = 0,
    CARBON_ANOMALY_REWARD_RESOURCES,   /* Resource bonus */
    CARBON_ANOMALY_REWARD_TECH,        /* Technology unlock/progress */
    CARBON_ANOMALY_REWARD_UNIT,        /* Free unit(s) */
    CARBON_ANOMALY_REWARD_MODIFIER,    /* Temporary/permanent modifier */
    CARBON_ANOMALY_REWARD_ARTIFACT,    /* Special item */
    CARBON_ANOMALY_REWARD_MAP,         /* Reveal map area */
    CARBON_ANOMALY_REWARD_CUSTOM,      /* Game-defined reward */
    CARBON_ANOMALY_REWARD_TYPE_COUNT,
} Carbon_AnomalyRewardType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/* Forward declarations */
typedef struct Carbon_AnomalyRegistry Carbon_AnomalyRegistry;
typedef struct Carbon_AnomalyManager Carbon_AnomalyManager;
typedef struct Carbon_Anomaly Carbon_Anomaly;

/**
 * Reward data for an anomaly
 */
typedef struct Carbon_AnomalyReward {
    Carbon_AnomalyRewardType type;    /* Type of reward */
    int32_t resource_type;            /* Resource type ID (for RESOURCES) */
    int32_t amount;                   /* Amount of reward */
    int32_t secondary;                /* Secondary value (e.g., tech ID) */
    uint32_t metadata;                /* Game-specific data */
} Carbon_AnomalyReward;

/**
 * Anomaly type definition
 */
typedef struct Carbon_AnomalyTypeDef {
    char id[CARBON_ANOMALY_ID_MAX];           /* Unique string identifier */
    char name[CARBON_ANOMALY_NAME_MAX];       /* Display name */
    char description[CARBON_ANOMALY_DESC_MAX]; /* Description text */

    Carbon_AnomalyRarity rarity;              /* Rarity tier */
    float research_time;                       /* Base research time (in game units) */
    float research_multiplier;                 /* Research speed multiplier */

    /* Rewards */
    Carbon_AnomalyReward rewards[4];          /* Up to 4 rewards */
    int reward_count;                          /* Number of rewards */

    /* Requirements */
    int32_t required_tech;                    /* Tech required to research (-1 = none) */
    int32_t min_researchers;                  /* Minimum researchers needed */

    /* Behavior flags */
    bool repeatable;                           /* Can be researched multiple times */
    bool visible_undiscovered;                 /* Show on map before discovery */
    bool dangerous;                            /* Can have negative outcomes */

    /* Game-specific */
    uint32_t metadata;                         /* Custom metadata */
    int32_t category;                          /* Game-defined category */
} Carbon_AnomalyTypeDef;

/**
 * Anomaly instance
 */
struct Carbon_Anomaly {
    uint32_t id;                      /* Unique instance ID */
    int type_id;                      /* Type from registry */

    /* Position */
    int32_t x;                        /* X coordinate */
    int32_t y;                        /* Y coordinate */

    /* Status */
    Carbon_AnomalyStatus status;      /* Current status */
    float progress;                   /* Research progress (0.0 - 1.0) */
    float research_speed;             /* Current research speed multiplier */

    /* Ownership */
    int32_t discovered_by;            /* Faction that discovered (-1 = none) */
    int32_t researching_faction;      /* Faction currently researching */
    uint32_t researcher_entity;       /* Entity doing research (0 = none) */

    /* Timing */
    int32_t discovered_turn;          /* Turn when discovered */
    int32_t research_started_turn;    /* Turn when research started */
    int32_t completed_turn;           /* Turn when completed */

    /* State */
    int32_t times_completed;          /* Times researched (for repeatable) */
    uint32_t metadata;                /* Game-specific data */

    bool active;                      /* Is this slot in use */
};

/**
 * Anomaly completion result
 */
typedef struct Carbon_AnomalyResult {
    bool success;                     /* Research succeeded */
    int reward_count;                 /* Number of rewards */
    Carbon_AnomalyReward rewards[4];  /* Actual rewards (may be modified) */
    char message[128];                /* Result message */
} Carbon_AnomalyResult;

/**
 * Anomaly spawn parameters
 */
typedef struct Carbon_AnomalySpawnParams {
    int type_id;                      /* Type to spawn (-1 = random) */
    int32_t x;                        /* X coordinate */
    int32_t y;                        /* Y coordinate */
    Carbon_AnomalyRarity max_rarity;  /* Maximum rarity for random spawn */
    uint32_t metadata;                /* Game-specific data */
    bool pre_discovered;              /* Already discovered */
    int32_t discovered_by;            /* Discovering faction if pre_discovered */
} Carbon_AnomalySpawnParams;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Reward callback - called when anomaly research completes
 *
 * @param mgr        Anomaly manager
 * @param anomaly    Anomaly that was completed
 * @param result     Result with rewards
 * @param userdata   User context
 */
typedef void (*Carbon_AnomalyRewardFunc)(Carbon_AnomalyManager *mgr,
                                          const Carbon_Anomaly *anomaly,
                                          Carbon_AnomalyResult *result,
                                          void *userdata);

/**
 * Discovery callback - called when anomaly is discovered
 *
 * @param mgr        Anomaly manager
 * @param anomaly    Anomaly that was discovered
 * @param faction_id Faction that discovered it
 * @param userdata   User context
 */
typedef void (*Carbon_AnomalyDiscoveryFunc)(Carbon_AnomalyManager *mgr,
                                             const Carbon_Anomaly *anomaly,
                                             int32_t faction_id,
                                             void *userdata);

/**
 * Can research callback - custom validation
 *
 * @param mgr        Anomaly manager
 * @param anomaly    Anomaly to research
 * @param faction_id Faction attempting research
 * @param userdata   User context
 * @return true if faction can research this anomaly
 */
typedef bool (*Carbon_AnomalyCanResearchFunc)(const Carbon_AnomalyManager *mgr,
                                               const Carbon_Anomaly *anomaly,
                                               int32_t faction_id,
                                               void *userdata);

/**
 * Spawn callback - called when anomaly is spawned
 *
 * @param mgr        Anomaly manager
 * @param anomaly    Anomaly that was spawned
 * @param userdata   User context
 */
typedef void (*Carbon_AnomalySpawnFunc)(Carbon_AnomalyManager *mgr,
                                         const Carbon_Anomaly *anomaly,
                                         void *userdata);

/*============================================================================
 * Registry Functions
 *============================================================================*/

/**
 * Create an anomaly type registry.
 *
 * @return New registry or NULL on failure
 */
Carbon_AnomalyRegistry *carbon_anomaly_registry_create(void);

/**
 * Destroy an anomaly registry.
 *
 * @param registry Registry to destroy
 */
void carbon_anomaly_registry_destroy(Carbon_AnomalyRegistry *registry);

/**
 * Register an anomaly type.
 *
 * @param registry Registry
 * @param def      Type definition
 * @return Type ID (>= 0) or -1 on failure
 */
int carbon_anomaly_register_type(Carbon_AnomalyRegistry *registry,
                                  const Carbon_AnomalyTypeDef *def);

/**
 * Get an anomaly type definition.
 *
 * @param registry Registry
 * @param type_id  Type ID
 * @return Type definition or NULL if not found
 */
const Carbon_AnomalyTypeDef *carbon_anomaly_get_type(const Carbon_AnomalyRegistry *registry,
                                                      int type_id);

/**
 * Find type by string ID.
 *
 * @param registry Registry
 * @param id       String identifier
 * @return Type ID or -1 if not found
 */
int carbon_anomaly_find_type(const Carbon_AnomalyRegistry *registry, const char *id);

/**
 * Get type count.
 *
 * @param registry Registry
 * @return Number of registered types
 */
int carbon_anomaly_type_count(const Carbon_AnomalyRegistry *registry);

/**
 * Get types by rarity.
 *
 * @param registry  Registry
 * @param rarity    Rarity tier
 * @param out_types Output array of type IDs
 * @param max       Maximum to return
 * @return Number of types found
 */
int carbon_anomaly_get_types_by_rarity(const Carbon_AnomalyRegistry *registry,
                                        Carbon_AnomalyRarity rarity,
                                        int *out_types, int max);

/**
 * Get types by category.
 *
 * @param registry  Registry
 * @param category  Category ID
 * @param out_types Output array of type IDs
 * @param max       Maximum to return
 * @return Number of types found
 */
int carbon_anomaly_get_types_by_category(const Carbon_AnomalyRegistry *registry,
                                          int32_t category,
                                          int *out_types, int max);

/**
 * Create a default type definition.
 *
 * @return Initialized type definition with defaults
 */
Carbon_AnomalyTypeDef carbon_anomaly_type_default(void);

/*============================================================================
 * Manager Functions
 *============================================================================*/

/**
 * Create an anomaly manager.
 *
 * @param registry Registry for anomaly types
 * @return New manager or NULL on failure
 */
Carbon_AnomalyManager *carbon_anomaly_manager_create(Carbon_AnomalyRegistry *registry);

/**
 * Destroy an anomaly manager.
 *
 * @param mgr Manager to destroy
 */
void carbon_anomaly_manager_destroy(Carbon_AnomalyManager *mgr);

/**
 * Get the registry associated with a manager.
 *
 * @param mgr Manager
 * @return Associated registry
 */
Carbon_AnomalyRegistry *carbon_anomaly_manager_get_registry(Carbon_AnomalyManager *mgr);

/*============================================================================
 * Spawning
 *============================================================================*/

/**
 * Spawn an anomaly at a location.
 *
 * @param mgr      Manager
 * @param type_id  Type to spawn
 * @param x        X coordinate
 * @param y        Y coordinate
 * @param metadata Game-specific data
 * @return Anomaly ID or CARBON_ANOMALY_INVALID on failure
 */
uint32_t carbon_anomaly_spawn(Carbon_AnomalyManager *mgr, int type_id,
                               int32_t x, int32_t y, uint32_t metadata);

/**
 * Spawn anomaly with extended options.
 *
 * @param mgr    Manager
 * @param params Spawn parameters
 * @return Anomaly ID or CARBON_ANOMALY_INVALID on failure
 */
uint32_t carbon_anomaly_spawn_ex(Carbon_AnomalyManager *mgr,
                                  const Carbon_AnomalySpawnParams *params);

/**
 * Spawn a random anomaly based on rarity weights.
 *
 * @param mgr        Manager
 * @param x          X coordinate
 * @param y          Y coordinate
 * @param max_rarity Maximum rarity that can spawn
 * @return Anomaly ID or CARBON_ANOMALY_INVALID on failure
 */
uint32_t carbon_anomaly_spawn_random(Carbon_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      Carbon_AnomalyRarity max_rarity);

/**
 * Remove an anomaly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID to remove
 */
void carbon_anomaly_remove(Carbon_AnomalyManager *mgr, uint32_t id);

/*============================================================================
 * Status and Progress
 *============================================================================*/

/**
 * Get an anomaly by ID.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Anomaly pointer or NULL if not found
 */
const Carbon_Anomaly *carbon_anomaly_get(const Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Get mutable anomaly for modification.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Anomaly pointer or NULL if not found
 */
Carbon_Anomaly *carbon_anomaly_get_mut(Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Get anomaly status.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Status
 */
Carbon_AnomalyStatus carbon_anomaly_get_status(const Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Discover an anomaly.
 *
 * @param mgr        Manager
 * @param id         Anomaly ID
 * @param faction_id Discovering faction
 * @return true if discovery succeeded
 */
bool carbon_anomaly_discover(Carbon_AnomalyManager *mgr, uint32_t id, int32_t faction_id);

/**
 * Start researching an anomaly.
 *
 * @param mgr        Manager
 * @param id         Anomaly ID
 * @param faction_id Faction starting research
 * @param researcher Entity performing research (0 = none)
 * @return true if research started
 */
bool carbon_anomaly_start_research(Carbon_AnomalyManager *mgr, uint32_t id,
                                    int32_t faction_id, uint32_t researcher);

/**
 * Stop researching an anomaly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 */
void carbon_anomaly_stop_research(Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Add research progress to an anomaly.
 *
 * @param mgr    Manager
 * @param id     Anomaly ID
 * @param amount Progress amount (scaled by research_time)
 * @return true if research completed
 */
bool carbon_anomaly_add_progress(Carbon_AnomalyManager *mgr, uint32_t id, float amount);

/**
 * Set research progress directly.
 *
 * @param mgr      Manager
 * @param id       Anomaly ID
 * @param progress Progress (0.0 - 1.0)
 */
void carbon_anomaly_set_progress(Carbon_AnomalyManager *mgr, uint32_t id, float progress);

/**
 * Get research progress.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Progress (0.0 - 1.0)
 */
float carbon_anomaly_get_progress(const Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Check if research is complete.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return true if research is complete
 */
bool carbon_anomaly_is_complete(const Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Complete research instantly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Result with rewards
 */
Carbon_AnomalyResult carbon_anomaly_complete_instant(Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Collect rewards from completed anomaly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Result with rewards
 */
Carbon_AnomalyResult carbon_anomaly_collect_rewards(Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Mark anomaly as depleted (no further use).
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 */
void carbon_anomaly_deplete(Carbon_AnomalyManager *mgr, uint32_t id);

/*============================================================================
 * Research Speed
 *============================================================================*/

/**
 * Set research speed multiplier for an anomaly.
 *
 * @param mgr   Manager
 * @param id    Anomaly ID
 * @param speed Speed multiplier (1.0 = normal)
 */
void carbon_anomaly_set_research_speed(Carbon_AnomalyManager *mgr, uint32_t id, float speed);

/**
 * Get remaining research time.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Remaining time (considering speed)
 */
float carbon_anomaly_get_remaining_time(const Carbon_AnomalyManager *mgr, uint32_t id);

/**
 * Get total research time.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Total time (considering type and modifiers)
 */
float carbon_anomaly_get_total_time(const Carbon_AnomalyManager *mgr, uint32_t id);

/*============================================================================
 * Queries
 *============================================================================*/

/**
 * Get anomalies at a position.
 *
 * @param mgr     Manager
 * @param x       X coordinate
 * @param y       Y coordinate
 * @param out_ids Output array of anomaly IDs
 * @param max     Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_at(const Carbon_AnomalyManager *mgr, int32_t x, int32_t y,
                           uint32_t *out_ids, int max);

/**
 * Get anomalies by status.
 *
 * @param mgr     Manager
 * @param status  Status to filter by
 * @param out_ids Output array of anomaly IDs
 * @param max     Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_by_status(const Carbon_AnomalyManager *mgr, Carbon_AnomalyStatus status,
                                  uint32_t *out_ids, int max);

/**
 * Get anomalies by type.
 *
 * @param mgr     Manager
 * @param type_id Type ID
 * @param out_ids Output array of anomaly IDs
 * @param max     Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_by_type(const Carbon_AnomalyManager *mgr, int type_id,
                                uint32_t *out_ids, int max);

/**
 * Get anomalies discovered by a faction.
 *
 * @param mgr        Manager
 * @param faction_id Faction ID
 * @param out_ids    Output array of anomaly IDs
 * @param max        Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_by_faction(const Carbon_AnomalyManager *mgr, int32_t faction_id,
                                   uint32_t *out_ids, int max);

/**
 * Get anomalies in a rectangular region.
 *
 * @param mgr     Manager
 * @param x1      Min X
 * @param y1      Min Y
 * @param x2      Max X
 * @param y2      Max Y
 * @param out_ids Output array of anomaly IDs
 * @param max     Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_in_rect(const Carbon_AnomalyManager *mgr,
                                int32_t x1, int32_t y1, int32_t x2, int32_t y2,
                                uint32_t *out_ids, int max);

/**
 * Get anomalies within radius.
 *
 * @param mgr      Manager
 * @param center_x Center X
 * @param center_y Center Y
 * @param radius   Search radius
 * @param out_ids  Output array of anomaly IDs
 * @param max      Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_in_radius(const Carbon_AnomalyManager *mgr,
                                  int32_t center_x, int32_t center_y, int32_t radius,
                                  uint32_t *out_ids, int max);

/**
 * Get all active anomalies.
 *
 * @param mgr     Manager
 * @param out_ids Output array of anomaly IDs
 * @param max     Maximum to return
 * @return Number found
 */
int carbon_anomaly_get_all(const Carbon_AnomalyManager *mgr, uint32_t *out_ids, int max);

/**
 * Check if an anomaly exists at a position.
 *
 * @param mgr Manager
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return true if anomaly exists at position
 */
bool carbon_anomaly_has_at(const Carbon_AnomalyManager *mgr, int32_t x, int32_t y);

/**
 * Find nearest anomaly to a position.
 *
 * @param mgr          Manager
 * @param x            X coordinate
 * @param y            Y coordinate
 * @param max_distance Maximum search distance (-1 = unlimited)
 * @param status       Status filter (-1 = any)
 * @return Anomaly ID or CARBON_ANOMALY_INVALID if none found
 */
uint32_t carbon_anomaly_find_nearest(const Carbon_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      int32_t max_distance,
                                      int status);

/*============================================================================
 * Validation
 *============================================================================*/

/**
 * Check if faction can research an anomaly.
 *
 * @param mgr        Manager
 * @param id         Anomaly ID
 * @param faction_id Faction ID
 * @return true if faction can research
 */
bool carbon_anomaly_can_research(const Carbon_AnomalyManager *mgr, uint32_t id,
                                  int32_t faction_id);

/**
 * Check if position is valid for spawning.
 *
 * @param mgr Manager
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return true if spawn is valid
 */
bool carbon_anomaly_can_spawn_at(const Carbon_AnomalyManager *mgr, int32_t x, int32_t y);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set reward callback.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void carbon_anomaly_set_reward_callback(Carbon_AnomalyManager *mgr,
                                         Carbon_AnomalyRewardFunc callback,
                                         void *userdata);

/**
 * Set discovery callback.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void carbon_anomaly_set_discovery_callback(Carbon_AnomalyManager *mgr,
                                            Carbon_AnomalyDiscoveryFunc callback,
                                            void *userdata);

/**
 * Set spawn callback.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void carbon_anomaly_set_spawn_callback(Carbon_AnomalyManager *mgr,
                                        Carbon_AnomalySpawnFunc callback,
                                        void *userdata);

/**
 * Set custom can-research validator.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void carbon_anomaly_set_can_research_callback(Carbon_AnomalyManager *mgr,
                                               Carbon_AnomalyCanResearchFunc callback,
                                               void *userdata);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Anomaly statistics
 */
typedef struct Carbon_AnomalyStats {
    int total_count;                  /* Total active anomalies */
    int undiscovered_count;           /* Undiscovered count */
    int discovered_count;             /* Discovered but not researched */
    int researching_count;            /* Currently being researched */
    int completed_count;              /* Completed count */
    int depleted_count;               /* Depleted count */
    int by_rarity[CARBON_ANOMALY_RARITY_COUNT];  /* Count by rarity */
} Carbon_AnomalyStats;

/**
 * Get statistics.
 *
 * @param mgr       Manager
 * @param out_stats Output statistics
 */
void carbon_anomaly_get_stats(const Carbon_AnomalyManager *mgr, Carbon_AnomalyStats *out_stats);

/**
 * Get total anomaly count.
 *
 * @param mgr Manager
 * @return Total active anomalies
 */
int carbon_anomaly_count(const Carbon_AnomalyManager *mgr);

/*============================================================================
 * Turn Management
 *============================================================================*/

/**
 * Set current turn (for tracking).
 *
 * @param mgr  Manager
 * @param turn Current turn number
 */
void carbon_anomaly_set_turn(Carbon_AnomalyManager *mgr, int32_t turn);

/**
 * Update all anomalies (call each frame/turn).
 * Processes research progress for all active anomalies.
 *
 * @param mgr        Manager
 * @param delta_time Time delta (for progress)
 */
void carbon_anomaly_update(Carbon_AnomalyManager *mgr, float delta_time);

/**
 * Clear all anomalies.
 *
 * @param mgr Manager
 */
void carbon_anomaly_clear(Carbon_AnomalyManager *mgr);

/*============================================================================
 * Random Generation
 *============================================================================*/

/**
 * Set random seed for spawning.
 *
 * @param mgr  Manager
 * @param seed Random seed (0 = use time)
 */
void carbon_anomaly_set_seed(Carbon_AnomalyManager *mgr, uint32_t seed);

/**
 * Set rarity weights for random spawning.
 *
 * @param mgr     Manager
 * @param weights Array of weights for each rarity tier
 */
void carbon_anomaly_set_rarity_weights(Carbon_AnomalyManager *mgr, const float *weights);

/**
 * Get default rarity weights.
 *
 * @param out_weights Output array (must be CARBON_ANOMALY_RARITY_COUNT)
 */
void carbon_anomaly_get_default_weights(float *out_weights);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get rarity name.
 *
 * @param rarity Rarity tier
 * @return Static string name
 */
const char *carbon_anomaly_rarity_name(Carbon_AnomalyRarity rarity);

/**
 * Get status name.
 *
 * @param status Status
 * @return Static string name
 */
const char *carbon_anomaly_status_name(Carbon_AnomalyStatus status);

/**
 * Get reward type name.
 *
 * @param type Reward type
 * @return Static string name
 */
const char *carbon_anomaly_reward_type_name(Carbon_AnomalyRewardType type);

#endif /* CARBON_ANOMALY_H */
