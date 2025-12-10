/*
 * Carbon Game Engine - Siege/Bombardment System
 *
 * Sustained attack mechanics over multiple rounds for location assault.
 * Supports progressive damage application, building destruction, population
 * effects, and configurable siege requirements.
 *
 * Usage:
 * ```c
 * #include "agentite/siege.h"
 *
 * // Create siege manager
 * Agentite_SiegeManager *siege = agentite_siege_create();
 *
 * // Configure callbacks for game integration
 * int get_defense(uint32_t location, void *ud) {
 *     return get_location_defense_value(location);
 * }
 * agentite_siege_set_defense_callback(siege, get_defense, game);
 *
 * // Start a siege
 * if (agentite_siege_can_begin(siege, attacker_id, target_location, attacking_force)) {
 *     uint32_t siege_id = agentite_siege_begin(siege, attacker_id, target_location,
 *                                             attacking_force);
 *
 *     // Each turn, process the siege
 *     Agentite_SiegeRoundResult result;
 *     agentite_siege_process_round(siege, siege_id, &result);
 *
 *     if (result.target_captured) {
 *         printf("Location captured!\n");
 *     } else if (result.siege_broken) {
 *         printf("Siege failed!\n");
 *     }
 * }
 *
 * // Cleanup
 * agentite_siege_destroy(siege);
 * ```
 */

#ifndef AGENTITE_SIEGE_H
#define AGENTITE_SIEGE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

/* Maximum concurrent sieges */
#define AGENTITE_SIEGE_MAX_INSTANCES      64

/* Maximum buildings that can be tracked per siege */
#define AGENTITE_SIEGE_MAX_BUILDINGS      32

/* Invalid siege handle */
#define AGENTITE_SIEGE_INVALID            0

/* Default configuration values */
#define AGENTITE_SIEGE_DEFAULT_MAX_ROUNDS         20
#define AGENTITE_SIEGE_DEFAULT_MIN_FORCE_RATIO    0.5f
#define AGENTITE_SIEGE_DEFAULT_DAMAGE_PER_ROUND   10
#define AGENTITE_SIEGE_DEFAULT_CAPTURE_THRESHOLD  0.0f

/*============================================================================
 * Enums
 *============================================================================*/

/**
 * Siege status enumeration
 */
typedef enum Agentite_SiegeStatus {
    AGENTITE_SIEGE_INACTIVE = 0,      /* Siege slot not in use */
    AGENTITE_SIEGE_PREPARING,         /* Siege being set up */
    AGENTITE_SIEGE_ACTIVE,            /* Siege in progress */
    AGENTITE_SIEGE_CAPTURED,          /* Target captured by attacker */
    AGENTITE_SIEGE_BROKEN,            /* Siege broken by defenders */
    AGENTITE_SIEGE_RETREATED,         /* Attacker retreated */
    AGENTITE_SIEGE_TIMEOUT            /* Max rounds exceeded */
} Agentite_SiegeStatus;

/**
 * Siege event types for callbacks
 */
typedef enum Agentite_SiegeEvent {
    AGENTITE_SIEGE_EVENT_STARTED = 0,
    AGENTITE_SIEGE_EVENT_ROUND_PROCESSED,
    AGENTITE_SIEGE_EVENT_BUILDING_DAMAGED,
    AGENTITE_SIEGE_EVENT_BUILDING_DESTROYED,
    AGENTITE_SIEGE_EVENT_DEFENSE_REDUCED,
    AGENTITE_SIEGE_EVENT_CAPTURED,
    AGENTITE_SIEGE_EVENT_BROKEN,
    AGENTITE_SIEGE_EVENT_RETREATED,
    AGENTITE_SIEGE_EVENT_TIMEOUT
} Agentite_SiegeEvent;

/**
 * Building damage level
 */
typedef enum Agentite_BuildingDamageLevel {
    AGENTITE_BUILDING_INTACT = 0,
    AGENTITE_BUILDING_LIGHT_DAMAGE,
    AGENTITE_BUILDING_MODERATE_DAMAGE,
    AGENTITE_BUILDING_HEAVY_DAMAGE,
    AGENTITE_BUILDING_DESTROYED
} Agentite_BuildingDamageLevel;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Building state during siege
 */
typedef struct Agentite_SiegeBuilding {
    uint32_t building_id;           /* External building identifier */
    int32_t max_health;             /* Maximum health */
    int32_t current_health;         /* Current health */
    int32_t defense_contribution;   /* Defense points this building provides */
    bool destroyed;                 /* Whether building is destroyed */
} Agentite_SiegeBuilding;

/**
 * Result of a siege round
 */
typedef struct Agentite_SiegeRoundResult {
    int32_t round_number;           /* Current round (1-based) */
    int32_t damage_dealt;           /* Total damage dealt this round */
    int32_t buildings_damaged;      /* Number of buildings damaged */
    int32_t buildings_destroyed;    /* Number of buildings destroyed */
    int32_t defense_reduced;        /* Defense points reduced */
    int32_t population_casualties;  /* Civilian casualties */
    int32_t attacker_casualties;    /* Attacker losses */
    int32_t defender_casualties;    /* Defender military losses */
    float capture_progress;         /* Progress toward capture (0.0-1.0) */
    bool siege_broken;              /* Defenders won */
    bool target_captured;           /* Attackers won */
    bool siege_ended;               /* Siege ended for any reason */
    Agentite_SiegeStatus end_status;  /* Final status if siege ended */
} Agentite_SiegeRoundResult;

/**
 * Siege instance data
 */
typedef struct Agentite_Siege {
    uint32_t id;                    /* Unique siege identifier */
    bool active;                    /* Slot in use */

    /* Participants */
    uint32_t attacker_faction;      /* Attacking faction ID */
    uint32_t defender_faction;      /* Defending faction ID */
    uint32_t target_location;       /* Location being sieged */

    /* Force strength */
    int32_t initial_attack_force;   /* Starting attacker strength */
    int32_t current_attack_force;   /* Current attacker strength */
    int32_t initial_defense_force;  /* Starting defender strength */
    int32_t current_defense_force;  /* Current defender strength */

    /* Progress */
    Agentite_SiegeStatus status;      /* Current status */
    int32_t current_round;          /* Current round number */
    int32_t max_rounds;             /* Maximum rounds before timeout */
    float capture_progress;         /* Progress toward capture (0.0-1.0) */

    /* Damage tracking */
    int32_t total_damage_dealt;     /* Cumulative damage */
    int32_t total_buildings_destroyed;
    int32_t total_population_casualties;
    int32_t total_attacker_casualties;
    int32_t total_defender_casualties;

    /* Buildings */
    Agentite_SiegeBuilding buildings[AGENTITE_SIEGE_MAX_BUILDINGS];
    int building_count;

    /* Timing */
    int32_t started_turn;           /* Turn siege started */
    int32_t ended_turn;             /* Turn siege ended (-1 if ongoing) */

    /* Modifiers */
    float attack_modifier;          /* Multiplier for attack power */
    float defense_modifier;         /* Multiplier for defense power */
    float damage_modifier;          /* Multiplier for damage dealt */

    /* User data */
    uint32_t metadata;              /* Game-specific data */
} Agentite_Siege;

/**
 * Siege configuration
 */
typedef struct Agentite_SiegeConfig {
    int32_t default_max_rounds;     /* Default max rounds for new sieges */
    float min_force_ratio;          /* Minimum attacker/defender ratio to begin */
    int32_t base_damage_per_round;  /* Base damage dealt per round */
    float capture_threshold;        /* Defense remaining for capture (0.0-1.0) */
    float building_damage_chance;   /* Chance to damage a building per round */
    float population_casualty_rate; /* Population casualty rate per round */
    float attacker_attrition_rate;  /* Attacker losses per round */
    float defender_attrition_rate;  /* Defender losses per round */
    bool allow_retreat;             /* Whether attacker can retreat */
    bool destroy_on_capture;        /* Destroy remaining buildings on capture */
} Agentite_SiegeConfig;

/**
 * Siege statistics
 */
typedef struct Agentite_SiegeStats {
    int32_t total_sieges;           /* Total sieges ever started */
    int32_t active_sieges;          /* Currently active sieges */
    int32_t captured_count;         /* Sieges ending in capture */
    int32_t broken_count;           /* Sieges broken by defenders */
    int32_t retreated_count;        /* Sieges where attacker retreated */
    int32_t timeout_count;          /* Sieges that timed out */
    int32_t total_rounds_processed; /* Total rounds across all sieges */
    int32_t total_buildings_destroyed;
    int32_t total_casualties;       /* All casualties combined */
} Agentite_SiegeStats;

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Agentite_SiegeManager Agentite_SiegeManager;
typedef struct Agentite_EventDispatcher Agentite_EventDispatcher;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Callback to get defense value of a location
 * Returns: Defense strength value
 */
typedef int32_t (*Agentite_SiegeDefenseFunc)(
    uint32_t location,
    void *userdata
);

/**
 * Callback to get defender faction of a location
 * Returns: Faction ID of the defender
 */
typedef uint32_t (*Agentite_SiegeDefenderFunc)(
    uint32_t location,
    void *userdata
);

/**
 * Callback to calculate damage for a round
 * Returns: Damage to deal this round
 */
typedef int32_t (*Agentite_SiegeDamageFunc)(
    const Agentite_Siege *siege,
    void *userdata
);

/**
 * Callback when siege events occur
 */
typedef void (*Agentite_SiegeEventFunc)(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    Agentite_SiegeEvent event,
    const Agentite_SiegeRoundResult *result,
    void *userdata
);

/**
 * Callback to check if siege can begin (custom validation)
 * Returns: true if siege is allowed
 */
typedef bool (*Agentite_SiegeCanBeginFunc)(
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force,
    void *userdata
);

/**
 * Callback to populate buildings for a location
 * Returns: Number of buildings added
 */
typedef int (*Agentite_SiegeBuildingsFunc)(
    uint32_t location,
    Agentite_SiegeBuilding *out_buildings,
    int max_buildings,
    void *userdata
);

/*============================================================================
 * Manager Lifecycle
 *============================================================================*/

/**
 * Create a siege manager
 * Returns: New siege manager or NULL on failure
 */
Agentite_SiegeManager *agentite_siege_create(void);

/**
 * Create a siege manager with event dispatcher
 * Returns: New siege manager or NULL on failure
 */
Agentite_SiegeManager *agentite_siege_create_with_events(
    Agentite_EventDispatcher *events
);

/**
 * Destroy a siege manager and free resources
 */
void agentite_siege_destroy(Agentite_SiegeManager *mgr);

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * Get default configuration
 */
Agentite_SiegeConfig agentite_siege_default_config(void);

/**
 * Set siege configuration
 */
void agentite_siege_set_config(
    Agentite_SiegeManager *mgr,
    const Agentite_SiegeConfig *config
);

/**
 * Get current configuration
 */
const Agentite_SiegeConfig *agentite_siege_get_config(
    const Agentite_SiegeManager *mgr
);

/**
 * Set maximum rounds for new sieges
 */
void agentite_siege_set_max_rounds(Agentite_SiegeManager *mgr, int32_t max_rounds);

/**
 * Set minimum force ratio required to begin siege
 */
void agentite_siege_set_min_force_ratio(Agentite_SiegeManager *mgr, float ratio);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set callback to get defense value of a location
 */
void agentite_siege_set_defense_callback(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeDefenseFunc callback,
    void *userdata
);

/**
 * Set callback to get defender faction of a location
 */
void agentite_siege_set_defender_callback(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeDefenderFunc callback,
    void *userdata
);

/**
 * Set callback to calculate damage per round
 */
void agentite_siege_set_damage_callback(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeDamageFunc callback,
    void *userdata
);

/**
 * Set callback for siege events
 */
void agentite_siege_set_event_callback(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeEventFunc callback,
    void *userdata
);

/**
 * Set custom validation callback for beginning sieges
 */
void agentite_siege_set_can_begin_callback(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeCanBeginFunc callback,
    void *userdata
);

/**
 * Set callback to populate buildings for siege targets
 */
void agentite_siege_set_buildings_callback(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeBuildingsFunc callback,
    void *userdata
);

/*============================================================================
 * Siege Lifecycle
 *============================================================================*/

/**
 * Check if a siege can begin
 * Returns: true if siege requirements are met
 */
bool agentite_siege_can_begin(
    Agentite_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force
);

/**
 * Begin a new siege
 * Returns: Siege ID or AGENTITE_SIEGE_INVALID on failure
 */
uint32_t agentite_siege_begin(
    Agentite_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force
);

/**
 * Begin a siege with extended options
 * Returns: Siege ID or AGENTITE_SIEGE_INVALID on failure
 */
uint32_t agentite_siege_begin_ex(
    Agentite_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force,
    int32_t max_rounds,
    uint32_t metadata
);

/**
 * Process a siege round
 * Returns: true if round was processed successfully
 */
bool agentite_siege_process_round(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    Agentite_SiegeRoundResult *out_result
);

/**
 * End a siege (attacker retreats)
 */
void agentite_siege_retreat(Agentite_SiegeManager *mgr, uint32_t siege_id);

/**
 * Force end a siege with specific status
 */
void agentite_siege_end(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    Agentite_SiegeStatus end_status
);

/*============================================================================
 * Force Modification
 *============================================================================*/

/**
 * Add reinforcements to attacker
 */
void agentite_siege_reinforce_attacker(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t additional_force
);

/**
 * Add reinforcements to defender
 */
void agentite_siege_reinforce_defender(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t additional_force
);

/**
 * Apply casualties to attacker
 */
void agentite_siege_attacker_casualties(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t casualties
);

/**
 * Apply casualties to defender
 */
void agentite_siege_defender_casualties(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t casualties
);

/*============================================================================
 * Modifier Control
 *============================================================================*/

/**
 * Set attack power modifier
 */
void agentite_siege_set_attack_modifier(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    float modifier
);

/**
 * Set defense power modifier
 */
void agentite_siege_set_defense_modifier(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    float modifier
);

/**
 * Set damage modifier
 */
void agentite_siege_set_damage_modifier(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    float modifier
);

/*============================================================================
 * Building Management
 *============================================================================*/

/**
 * Add a building to track during siege
 * Returns: Index of added building or -1 on failure
 */
int agentite_siege_add_building(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    uint32_t building_id,
    int32_t max_health,
    int32_t defense_contribution
);

/**
 * Damage a specific building
 * Returns: true if building was damaged/destroyed
 */
bool agentite_siege_damage_building(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    int building_index,
    int32_t damage
);

/**
 * Get building state
 * Returns: Pointer to building or NULL if not found
 */
const Agentite_SiegeBuilding *agentite_siege_get_building(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id,
    int building_index
);

/**
 * Get building damage level
 */
Agentite_BuildingDamageLevel agentite_siege_get_building_damage_level(
    const Agentite_SiegeBuilding *building
);

/**
 * Get count of buildings in siege
 */
int agentite_siege_get_building_count(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get count of destroyed buildings
 */
int agentite_siege_get_destroyed_building_count(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/*============================================================================
 * Queries - Single Siege
 *============================================================================*/

/**
 * Get siege by ID
 * Returns: Pointer to siege or NULL if not found
 */
const Agentite_Siege *agentite_siege_get(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get mutable siege by ID
 * Returns: Pointer to siege or NULL if not found
 */
Agentite_Siege *agentite_siege_get_mut(
    Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Check if siege exists and is active
 */
bool agentite_siege_is_active(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get siege status
 */
Agentite_SiegeStatus agentite_siege_get_status(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get current round number
 */
int32_t agentite_siege_get_round(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get capture progress (0.0-1.0)
 */
float agentite_siege_get_progress(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get remaining rounds before timeout
 */
int32_t agentite_siege_get_remaining_rounds(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get current attack force
 */
int32_t agentite_siege_get_attack_force(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get current defense force
 */
int32_t agentite_siege_get_defense_force(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get force ratio (attack / defense)
 */
float agentite_siege_get_force_ratio(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

/*============================================================================
 * Queries - Batch
 *============================================================================*/

/**
 * Get all active siege IDs
 * Returns: Number of sieges returned
 */
int agentite_siege_get_all_active(
    const Agentite_SiegeManager *mgr,
    uint32_t *out_ids,
    int max
);

/**
 * Get sieges by attacker faction
 * Returns: Number of sieges returned
 */
int agentite_siege_get_by_attacker(
    const Agentite_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t *out_ids,
    int max
);

/**
 * Get sieges by defender faction
 * Returns: Number of sieges returned
 */
int agentite_siege_get_by_defender(
    const Agentite_SiegeManager *mgr,
    uint32_t defender_faction,
    uint32_t *out_ids,
    int max
);

/**
 * Get siege at location
 * Returns: Siege ID or AGENTITE_SIEGE_INVALID if none
 */
uint32_t agentite_siege_get_at_location(
    const Agentite_SiegeManager *mgr,
    uint32_t location
);

/**
 * Check if location is under siege
 */
bool agentite_siege_has_siege_at(
    const Agentite_SiegeManager *mgr,
    uint32_t location
);

/**
 * Get sieges by status
 * Returns: Number of sieges returned
 */
int agentite_siege_get_by_status(
    const Agentite_SiegeManager *mgr,
    Agentite_SiegeStatus status,
    uint32_t *out_ids,
    int max
);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Get overall siege statistics
 */
void agentite_siege_get_stats(
    const Agentite_SiegeManager *mgr,
    Agentite_SiegeStats *out_stats
);

/**
 * Get total active siege count
 */
int agentite_siege_count_active(const Agentite_SiegeManager *mgr);

/**
 * Reset statistics
 */
void agentite_siege_reset_stats(Agentite_SiegeManager *mgr);

/*============================================================================
 * Turn Integration
 *============================================================================*/

/**
 * Set current turn (for tracking siege timing)
 */
void agentite_siege_set_turn(Agentite_SiegeManager *mgr, int32_t turn);

/**
 * Process all active sieges for one round
 * Returns: Number of sieges processed
 */
int agentite_siege_process_all(
    Agentite_SiegeManager *mgr,
    Agentite_SiegeRoundResult *out_results,
    int max_results
);

/**
 * Update (for time-based siege progress if needed)
 */
void agentite_siege_update(Agentite_SiegeManager *mgr, float delta_time);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Get status name as string
 */
const char *agentite_siege_status_name(Agentite_SiegeStatus status);

/**
 * Get event name as string
 */
const char *agentite_siege_event_name(Agentite_SiegeEvent event);

/**
 * Get building damage level name as string
 */
const char *agentite_siege_damage_level_name(Agentite_BuildingDamageLevel level);

/**
 * Calculate estimated rounds to capture
 * Returns: Estimated rounds or -1 if unlikely to succeed
 */
int agentite_siege_estimate_rounds(
    const Agentite_SiegeManager *mgr,
    uint32_t siege_id
);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_SIEGE_H */
