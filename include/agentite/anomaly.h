#ifndef AGENTITE_ANOMALY_H
#define AGENTITE_ANOMALY_H

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
 *   Agentite_AnomalyRegistry *registry = agentite_anomaly_registry_create();
 *   Agentite_AnomalyTypeDef def = {
 *       .id = "ancient_ruins",
 *       .name = "Ancient Ruins",
 *       .description = "Mysterious structures from a forgotten era",
 *       .rarity = AGENTITE_ANOMALY_UNCOMMON,
 *       .research_time = 10.0f,
 *   };
 *   int type_id = agentite_anomaly_register_type(registry, &def);
 *
 *   // Create manager and spawn anomalies
 *   Agentite_AnomalyManager *mgr = agentite_anomaly_manager_create(registry);
 *   uint32_t anomaly = agentite_anomaly_spawn(mgr, type_id, 50, 50, 0);
 *
 *   // Discover and research
 *   agentite_anomaly_discover(mgr, anomaly, player_faction);
 *   agentite_anomaly_start_research(mgr, anomaly, player_faction, scientist_entity);
 *
 *   // In game loop
 *   agentite_anomaly_add_progress(mgr, anomaly, delta_time);
 *   if (agentite_anomaly_is_complete(mgr, anomaly)) {
 *       // Collect rewards
 *   }
 *
 *   // Cleanup
 *   agentite_anomaly_manager_destroy(mgr);
 *   agentite_anomaly_registry_destroy(registry);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_ANOMALY_MAX_TYPES       64    /* Maximum anomaly types */
#define AGENTITE_ANOMALY_MAX_INSTANCES   256   /* Maximum active anomalies */
#define AGENTITE_ANOMALY_INVALID         0     /* Invalid anomaly handle */
#define AGENTITE_ANOMALY_NAME_MAX        32    /* Max name length */
#define AGENTITE_ANOMALY_DESC_MAX        128   /* Max description length */
#define AGENTITE_ANOMALY_ID_MAX          32    /* Max string ID length */

/*============================================================================
 * Rarity Tiers
 *============================================================================*/

/**
 * Anomaly rarity tiers
 * Affects spawn probability and typically reward quality
 */
typedef enum Agentite_AnomalyRarity {
    AGENTITE_ANOMALY_COMMON = 0,      /* ~60% of spawns */
    AGENTITE_ANOMALY_UNCOMMON,        /* ~25% of spawns */
    AGENTITE_ANOMALY_RARE,            /* ~12% of spawns */
    AGENTITE_ANOMALY_LEGENDARY,       /* ~3% of spawns */
    AGENTITE_ANOMALY_RARITY_COUNT,
} Agentite_AnomalyRarity;

/*============================================================================
 * Status
 *============================================================================*/

/**
 * Anomaly discovery and research status
 */
typedef enum Agentite_AnomalyStatus {
    AGENTITE_ANOMALY_UNDISCOVERED = 0,  /* Not yet found */
    AGENTITE_ANOMALY_DISCOVERED,        /* Found but not researched */
    AGENTITE_ANOMALY_RESEARCHING,       /* Currently being researched */
    AGENTITE_ANOMALY_COMPLETED,         /* Research complete */
    AGENTITE_ANOMALY_DEPLETED,          /* Rewards collected, no further use */
} Agentite_AnomalyStatus;

/*============================================================================
 * Reward Types
 *============================================================================*/

/**
 * Types of rewards from completing anomaly research
 */
typedef enum Agentite_AnomalyRewardType {
    AGENTITE_ANOMALY_REWARD_NONE = 0,
    AGENTITE_ANOMALY_REWARD_RESOURCES,   /* Resource bonus */
    AGENTITE_ANOMALY_REWARD_TECH,        /* Technology unlock/progress */
    AGENTITE_ANOMALY_REWARD_UNIT,        /* Free unit(s) */
    AGENTITE_ANOMALY_REWARD_MODIFIER,    /* Temporary/permanent modifier */
    AGENTITE_ANOMALY_REWARD_ARTIFACT,    /* Special item */
    AGENTITE_ANOMALY_REWARD_MAP,         /* Reveal map area */
    AGENTITE_ANOMALY_REWARD_CUSTOM,      /* Game-defined reward */
    AGENTITE_ANOMALY_REWARD_TYPE_COUNT,
} Agentite_AnomalyRewardType;

/*============================================================================
 * Data Structures
 *============================================================================*/

/* Forward declarations */
typedef struct Agentite_AnomalyRegistry Agentite_AnomalyRegistry;
typedef struct Agentite_AnomalyManager Agentite_AnomalyManager;
typedef struct Agentite_Anomaly Agentite_Anomaly;

/**
 * Reward data for an anomaly
 */
typedef struct Agentite_AnomalyReward {
    Agentite_AnomalyRewardType type;    /* Type of reward */
    int32_t resource_type;            /* Resource type ID (for RESOURCES) */
    int32_t amount;                   /* Amount of reward */
    int32_t secondary;                /* Secondary value (e.g., tech ID) */
    uint32_t metadata;                /* Game-specific data */
} Agentite_AnomalyReward;

/**
 * Anomaly type definition
 */
typedef struct Agentite_AnomalyTypeDef {
    char id[AGENTITE_ANOMALY_ID_MAX];           /* Unique string identifier */
    char name[AGENTITE_ANOMALY_NAME_MAX];       /* Display name */
    char description[AGENTITE_ANOMALY_DESC_MAX]; /* Description text */

    Agentite_AnomalyRarity rarity;              /* Rarity tier */
    float research_time;                       /* Base research time (in game units) */
    float research_multiplier;                 /* Research speed multiplier */

    /* Rewards */
    Agentite_AnomalyReward rewards[4];          /* Up to 4 rewards */
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
} Agentite_AnomalyTypeDef;

/**
 * Anomaly instance
 */
struct Agentite_Anomaly {
    uint32_t id;                      /* Unique instance ID */
    int type_id;                      /* Type from registry */

    /* Position */
    int32_t x;                        /* X coordinate */
    int32_t y;                        /* Y coordinate */

    /* Status */
    Agentite_AnomalyStatus status;      /* Current status */
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
typedef struct Agentite_AnomalyResult {
    bool success;                     /* Research succeeded */
    int reward_count;                 /* Number of rewards */
    Agentite_AnomalyReward rewards[4];  /* Actual rewards (may be modified) */
    char message[128];                /* Result message */
} Agentite_AnomalyResult;

/**
 * Anomaly spawn parameters
 */
typedef struct Agentite_AnomalySpawnParams {
    int type_id;                      /* Type to spawn (-1 = random) */
    int32_t x;                        /* X coordinate */
    int32_t y;                        /* Y coordinate */
    Agentite_AnomalyRarity max_rarity;  /* Maximum rarity for random spawn */
    uint32_t metadata;                /* Game-specific data */
    bool pre_discovered;              /* Already discovered */
    int32_t discovered_by;            /* Discovering faction if pre_discovered */
} Agentite_AnomalySpawnParams;

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
typedef void (*Agentite_AnomalyRewardFunc)(Agentite_AnomalyManager *mgr,
                                          const Agentite_Anomaly *anomaly,
                                          Agentite_AnomalyResult *result,
                                          void *userdata);

/**
 * Discovery callback - called when anomaly is discovered
 *
 * @param mgr        Anomaly manager
 * @param anomaly    Anomaly that was discovered
 * @param faction_id Faction that discovered it
 * @param userdata   User context
 */
typedef void (*Agentite_AnomalyDiscoveryFunc)(Agentite_AnomalyManager *mgr,
                                             const Agentite_Anomaly *anomaly,
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
typedef bool (*Agentite_AnomalyCanResearchFunc)(const Agentite_AnomalyManager *mgr,
                                               const Agentite_Anomaly *anomaly,
                                               int32_t faction_id,
                                               void *userdata);

/**
 * Spawn callback - called when anomaly is spawned
 *
 * @param mgr        Anomaly manager
 * @param anomaly    Anomaly that was spawned
 * @param userdata   User context
 */
typedef void (*Agentite_AnomalySpawnFunc)(Agentite_AnomalyManager *mgr,
                                         const Agentite_Anomaly *anomaly,
                                         void *userdata);

/*============================================================================
 * Registry Functions
 *============================================================================*/

/**
 * Create an anomaly type registry.
 *
 * @return New registry or NULL on failure
 */
Agentite_AnomalyRegistry *agentite_anomaly_registry_create(void);

/**
 * Destroy an anomaly registry.
 *
 * @param registry Registry to destroy
 */
void agentite_anomaly_registry_destroy(Agentite_AnomalyRegistry *registry);

/**
 * Register an anomaly type.
 *
 * @param registry Registry
 * @param def      Type definition
 * @return Type ID (>= 0) or -1 on failure
 */
int agentite_anomaly_register_type(Agentite_AnomalyRegistry *registry,
                                  const Agentite_AnomalyTypeDef *def);

/**
 * Get an anomaly type definition.
 *
 * @param registry Registry
 * @param type_id  Type ID
 * @return Type definition or NULL if not found
 */
const Agentite_AnomalyTypeDef *agentite_anomaly_get_type(const Agentite_AnomalyRegistry *registry,
                                                      int type_id);

/**
 * Find type by string ID.
 *
 * @param registry Registry
 * @param id       String identifier
 * @return Type ID or -1 if not found
 */
int agentite_anomaly_find_type(const Agentite_AnomalyRegistry *registry, const char *id);

/**
 * Get type count.
 *
 * @param registry Registry
 * @return Number of registered types
 */
int agentite_anomaly_type_count(const Agentite_AnomalyRegistry *registry);

/**
 * Get types by rarity.
 *
 * @param registry  Registry
 * @param rarity    Rarity tier
 * @param out_types Output array of type IDs
 * @param max       Maximum to return
 * @return Number of types found
 */
int agentite_anomaly_get_types_by_rarity(const Agentite_AnomalyRegistry *registry,
                                        Agentite_AnomalyRarity rarity,
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
int agentite_anomaly_get_types_by_category(const Agentite_AnomalyRegistry *registry,
                                          int32_t category,
                                          int *out_types, int max);

/**
 * Create a default type definition.
 *
 * @return Initialized type definition with defaults
 */
Agentite_AnomalyTypeDef agentite_anomaly_type_default(void);

/*============================================================================
 * Manager Functions
 *============================================================================*/

/**
 * Create an anomaly manager.
 *
 * @param registry Registry for anomaly types
 * @return New manager or NULL on failure
 */
Agentite_AnomalyManager *agentite_anomaly_manager_create(Agentite_AnomalyRegistry *registry);

/**
 * Destroy an anomaly manager.
 *
 * @param mgr Manager to destroy
 */
void agentite_anomaly_manager_destroy(Agentite_AnomalyManager *mgr);

/**
 * Get the registry associated with a manager.
 *
 * @param mgr Manager
 * @return Associated registry
 */
Agentite_AnomalyRegistry *agentite_anomaly_manager_get_registry(Agentite_AnomalyManager *mgr);

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
 * @return Anomaly ID or AGENTITE_ANOMALY_INVALID on failure
 */
uint32_t agentite_anomaly_spawn(Agentite_AnomalyManager *mgr, int type_id,
                               int32_t x, int32_t y, uint32_t metadata);

/**
 * Spawn anomaly with extended options.
 *
 * @param mgr    Manager
 * @param params Spawn parameters
 * @return Anomaly ID or AGENTITE_ANOMALY_INVALID on failure
 */
uint32_t agentite_anomaly_spawn_ex(Agentite_AnomalyManager *mgr,
                                  const Agentite_AnomalySpawnParams *params);

/**
 * Spawn a random anomaly based on rarity weights.
 *
 * @param mgr        Manager
 * @param x          X coordinate
 * @param y          Y coordinate
 * @param max_rarity Maximum rarity that can spawn
 * @return Anomaly ID or AGENTITE_ANOMALY_INVALID on failure
 */
uint32_t agentite_anomaly_spawn_random(Agentite_AnomalyManager *mgr,
                                      int32_t x, int32_t y,
                                      Agentite_AnomalyRarity max_rarity);

/**
 * Remove an anomaly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID to remove
 */
void agentite_anomaly_remove(Agentite_AnomalyManager *mgr, uint32_t id);

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
const Agentite_Anomaly *agentite_anomaly_get(const Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Get mutable anomaly for modification.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Anomaly pointer or NULL if not found
 */
Agentite_Anomaly *agentite_anomaly_get_mut(Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Get anomaly status.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Status
 */
Agentite_AnomalyStatus agentite_anomaly_get_status(const Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Discover an anomaly.
 *
 * @param mgr        Manager
 * @param id         Anomaly ID
 * @param faction_id Discovering faction
 * @return true if discovery succeeded
 */
bool agentite_anomaly_discover(Agentite_AnomalyManager *mgr, uint32_t id, int32_t faction_id);

/**
 * Start researching an anomaly.
 *
 * @param mgr        Manager
 * @param id         Anomaly ID
 * @param faction_id Faction starting research
 * @param researcher Entity performing research (0 = none)
 * @return true if research started
 */
bool agentite_anomaly_start_research(Agentite_AnomalyManager *mgr, uint32_t id,
                                    int32_t faction_id, uint32_t researcher);

/**
 * Stop researching an anomaly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 */
void agentite_anomaly_stop_research(Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Add research progress to an anomaly.
 *
 * @param mgr    Manager
 * @param id     Anomaly ID
 * @param amount Progress amount (scaled by research_time)
 * @return true if research completed
 */
bool agentite_anomaly_add_progress(Agentite_AnomalyManager *mgr, uint32_t id, float amount);

/**
 * Set research progress directly.
 *
 * @param mgr      Manager
 * @param id       Anomaly ID
 * @param progress Progress (0.0 - 1.0)
 */
void agentite_anomaly_set_progress(Agentite_AnomalyManager *mgr, uint32_t id, float progress);

/**
 * Get research progress.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Progress (0.0 - 1.0)
 */
float agentite_anomaly_get_progress(const Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Check if research is complete.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return true if research is complete
 */
bool agentite_anomaly_is_complete(const Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Complete research instantly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Result with rewards
 */
Agentite_AnomalyResult agentite_anomaly_complete_instant(Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Collect rewards from completed anomaly.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Result with rewards
 */
Agentite_AnomalyResult agentite_anomaly_collect_rewards(Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Mark anomaly as depleted (no further use).
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 */
void agentite_anomaly_deplete(Agentite_AnomalyManager *mgr, uint32_t id);

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
void agentite_anomaly_set_research_speed(Agentite_AnomalyManager *mgr, uint32_t id, float speed);

/**
 * Get remaining research time.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Remaining time (considering speed)
 */
float agentite_anomaly_get_remaining_time(const Agentite_AnomalyManager *mgr, uint32_t id);

/**
 * Get total research time.
 *
 * @param mgr Manager
 * @param id  Anomaly ID
 * @return Total time (considering type and modifiers)
 */
float agentite_anomaly_get_total_time(const Agentite_AnomalyManager *mgr, uint32_t id);

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
int agentite_anomaly_get_at(const Agentite_AnomalyManager *mgr, int32_t x, int32_t y,
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
int agentite_anomaly_get_by_status(const Agentite_AnomalyManager *mgr, Agentite_AnomalyStatus status,
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
int agentite_anomaly_get_by_type(const Agentite_AnomalyManager *mgr, int type_id,
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
int agentite_anomaly_get_by_faction(const Agentite_AnomalyManager *mgr, int32_t faction_id,
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
int agentite_anomaly_get_in_rect(const Agentite_AnomalyManager *mgr,
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
int agentite_anomaly_get_in_radius(const Agentite_AnomalyManager *mgr,
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
int agentite_anomaly_get_all(const Agentite_AnomalyManager *mgr, uint32_t *out_ids, int max);

/**
 * Check if an anomaly exists at a position.
 *
 * @param mgr Manager
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return true if anomaly exists at position
 */
bool agentite_anomaly_has_at(const Agentite_AnomalyManager *mgr, int32_t x, int32_t y);

/**
 * Find nearest anomaly to a position.
 *
 * @param mgr          Manager
 * @param x            X coordinate
 * @param y            Y coordinate
 * @param max_distance Maximum search distance (-1 = unlimited)
 * @param status       Status filter (-1 = any)
 * @return Anomaly ID or AGENTITE_ANOMALY_INVALID if none found
 */
uint32_t agentite_anomaly_find_nearest(const Agentite_AnomalyManager *mgr,
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
bool agentite_anomaly_can_research(const Agentite_AnomalyManager *mgr, uint32_t id,
                                  int32_t faction_id);

/**
 * Check if position is valid for spawning.
 *
 * @param mgr Manager
 * @param x   X coordinate
 * @param y   Y coordinate
 * @return true if spawn is valid
 */
bool agentite_anomaly_can_spawn_at(const Agentite_AnomalyManager *mgr, int32_t x, int32_t y);

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
void agentite_anomaly_set_reward_callback(Agentite_AnomalyManager *mgr,
                                         Agentite_AnomalyRewardFunc callback,
                                         void *userdata);

/**
 * Set discovery callback.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void agentite_anomaly_set_discovery_callback(Agentite_AnomalyManager *mgr,
                                            Agentite_AnomalyDiscoveryFunc callback,
                                            void *userdata);

/**
 * Set spawn callback.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void agentite_anomaly_set_spawn_callback(Agentite_AnomalyManager *mgr,
                                        Agentite_AnomalySpawnFunc callback,
                                        void *userdata);

/**
 * Set custom can-research validator.
 *
 * @param mgr      Manager
 * @param callback Callback function
 * @param userdata User context
 */
void agentite_anomaly_set_can_research_callback(Agentite_AnomalyManager *mgr,
                                               Agentite_AnomalyCanResearchFunc callback,
                                               void *userdata);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Anomaly statistics
 */
typedef struct Agentite_AnomalyStats {
    int total_count;                  /* Total active anomalies */
    int undiscovered_count;           /* Undiscovered count */
    int discovered_count;             /* Discovered but not researched */
    int researching_count;            /* Currently being researched */
    int completed_count;              /* Completed count */
    int depleted_count;               /* Depleted count */
    int by_rarity[AGENTITE_ANOMALY_RARITY_COUNT];  /* Count by rarity */
} Agentite_AnomalyStats;

/**
 * Get statistics.
 *
 * @param mgr       Manager
 * @param out_stats Output statistics
 */
void agentite_anomaly_get_stats(const Agentite_AnomalyManager *mgr, Agentite_AnomalyStats *out_stats);

/**
 * Get total anomaly count.
 *
 * @param mgr Manager
 * @return Total active anomalies
 */
int agentite_anomaly_count(const Agentite_AnomalyManager *mgr);

/*============================================================================
 * Turn Management
 *============================================================================*/

/**
 * Set current turn (for tracking).
 *
 * @param mgr  Manager
 * @param turn Current turn number
 */
void agentite_anomaly_set_turn(Agentite_AnomalyManager *mgr, int32_t turn);

/**
 * Update all anomalies (call each frame/turn).
 * Processes research progress for all active anomalies.
 *
 * @param mgr        Manager
 * @param delta_time Time delta (for progress)
 */
void agentite_anomaly_update(Agentite_AnomalyManager *mgr, float delta_time);

/**
 * Clear all anomalies.
 *
 * @param mgr Manager
 */
void agentite_anomaly_clear(Agentite_AnomalyManager *mgr);

/*============================================================================
 * Random Generation
 *============================================================================*/

/**
 * Set random seed for spawning.
 *
 * @param mgr  Manager
 * @param seed Random seed (0 = use time)
 */
void agentite_anomaly_set_seed(Agentite_AnomalyManager *mgr, uint32_t seed);

/**
 * Set rarity weights for random spawning.
 *
 * @param mgr     Manager
 * @param weights Array of weights for each rarity tier
 */
void agentite_anomaly_set_rarity_weights(Agentite_AnomalyManager *mgr, const float *weights);

/**
 * Get default rarity weights.
 *
 * @param out_weights Output array (must be AGENTITE_ANOMALY_RARITY_COUNT)
 */
void agentite_anomaly_get_default_weights(float *out_weights);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get rarity name.
 *
 * @param rarity Rarity tier
 * @return Static string name
 */
const char *agentite_anomaly_rarity_name(Agentite_AnomalyRarity rarity);

/**
 * Get status name.
 *
 * @param status Status
 * @return Static string name
 */
const char *agentite_anomaly_status_name(Agentite_AnomalyStatus status);

/**
 * Get reward type name.
 *
 * @param type Reward type
 * @return Static string name
 */
const char *agentite_anomaly_reward_type_name(Agentite_AnomalyRewardType type);

#endif /* AGENTITE_ANOMALY_H */
