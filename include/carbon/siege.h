/*
 * Carbon Game Engine - Siege/Bombardment System
 *
 * Sustained attack mechanics over multiple rounds for location assault.
 * Supports progressive damage application, building destruction, population
 * effects, and configurable siege requirements.
 *
 * Usage:
 * ```c
 * #include "carbon/siege.h"
 *
 * // Create siege manager
 * Carbon_SiegeManager *siege = carbon_siege_create();
 *
 * // Configure callbacks for game integration
 * int get_defense(uint32_t location, void *ud) {
 *     return get_location_defense_value(location);
 * }
 * carbon_siege_set_defense_callback(siege, get_defense, game);
 *
 * // Start a siege
 * if (carbon_siege_can_begin(siege, attacker_id, target_location, attacking_force)) {
 *     uint32_t siege_id = carbon_siege_begin(siege, attacker_id, target_location,
 *                                             attacking_force);
 *
 *     // Each turn, process the siege
 *     Carbon_SiegeRoundResult result;
 *     carbon_siege_process_round(siege, siege_id, &result);
 *
 *     if (result.target_captured) {
 *         printf("Location captured!\n");
 *     } else if (result.siege_broken) {
 *         printf("Siege failed!\n");
 *     }
 * }
 *
 * // Cleanup
 * carbon_siege_destroy(siege);
 * ```
 */

#ifndef CARBON_SIEGE_H
#define CARBON_SIEGE_H

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
#define CARBON_SIEGE_MAX_INSTANCES      64

/* Maximum buildings that can be tracked per siege */
#define CARBON_SIEGE_MAX_BUILDINGS      32

/* Invalid siege handle */
#define CARBON_SIEGE_INVALID            0

/* Default configuration values */
#define CARBON_SIEGE_DEFAULT_MAX_ROUNDS         20
#define CARBON_SIEGE_DEFAULT_MIN_FORCE_RATIO    0.5f
#define CARBON_SIEGE_DEFAULT_DAMAGE_PER_ROUND   10
#define CARBON_SIEGE_DEFAULT_CAPTURE_THRESHOLD  0.0f

/*============================================================================
 * Enums
 *============================================================================*/

/**
 * Siege status enumeration
 */
typedef enum Carbon_SiegeStatus {
    CARBON_SIEGE_INACTIVE = 0,      /* Siege slot not in use */
    CARBON_SIEGE_PREPARING,         /* Siege being set up */
    CARBON_SIEGE_ACTIVE,            /* Siege in progress */
    CARBON_SIEGE_CAPTURED,          /* Target captured by attacker */
    CARBON_SIEGE_BROKEN,            /* Siege broken by defenders */
    CARBON_SIEGE_RETREATED,         /* Attacker retreated */
    CARBON_SIEGE_TIMEOUT            /* Max rounds exceeded */
} Carbon_SiegeStatus;

/**
 * Siege event types for callbacks
 */
typedef enum Carbon_SiegeEvent {
    CARBON_SIEGE_EVENT_STARTED = 0,
    CARBON_SIEGE_EVENT_ROUND_PROCESSED,
    CARBON_SIEGE_EVENT_BUILDING_DAMAGED,
    CARBON_SIEGE_EVENT_BUILDING_DESTROYED,
    CARBON_SIEGE_EVENT_DEFENSE_REDUCED,
    CARBON_SIEGE_EVENT_CAPTURED,
    CARBON_SIEGE_EVENT_BROKEN,
    CARBON_SIEGE_EVENT_RETREATED,
    CARBON_SIEGE_EVENT_TIMEOUT
} Carbon_SiegeEvent;

/**
 * Building damage level
 */
typedef enum Carbon_BuildingDamageLevel {
    CARBON_BUILDING_INTACT = 0,
    CARBON_BUILDING_LIGHT_DAMAGE,
    CARBON_BUILDING_MODERATE_DAMAGE,
    CARBON_BUILDING_HEAVY_DAMAGE,
    CARBON_BUILDING_DESTROYED
} Carbon_BuildingDamageLevel;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Building state during siege
 */
typedef struct Carbon_SiegeBuilding {
    uint32_t building_id;           /* External building identifier */
    int32_t max_health;             /* Maximum health */
    int32_t current_health;         /* Current health */
    int32_t defense_contribution;   /* Defense points this building provides */
    bool destroyed;                 /* Whether building is destroyed */
} Carbon_SiegeBuilding;

/**
 * Result of a siege round
 */
typedef struct Carbon_SiegeRoundResult {
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
    Carbon_SiegeStatus end_status;  /* Final status if siege ended */
} Carbon_SiegeRoundResult;

/**
 * Siege instance data
 */
typedef struct Carbon_Siege {
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
    Carbon_SiegeStatus status;      /* Current status */
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
    Carbon_SiegeBuilding buildings[CARBON_SIEGE_MAX_BUILDINGS];
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
} Carbon_Siege;

/**
 * Siege configuration
 */
typedef struct Carbon_SiegeConfig {
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
} Carbon_SiegeConfig;

/**
 * Siege statistics
 */
typedef struct Carbon_SiegeStats {
    int32_t total_sieges;           /* Total sieges ever started */
    int32_t active_sieges;          /* Currently active sieges */
    int32_t captured_count;         /* Sieges ending in capture */
    int32_t broken_count;           /* Sieges broken by defenders */
    int32_t retreated_count;        /* Sieges where attacker retreated */
    int32_t timeout_count;          /* Sieges that timed out */
    int32_t total_rounds_processed; /* Total rounds across all sieges */
    int32_t total_buildings_destroyed;
    int32_t total_casualties;       /* All casualties combined */
} Carbon_SiegeStats;

/*============================================================================
 * Forward Declarations
 *============================================================================*/

typedef struct Carbon_SiegeManager Carbon_SiegeManager;
typedef struct Carbon_EventDispatcher Carbon_EventDispatcher;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Callback to get defense value of a location
 * Returns: Defense strength value
 */
typedef int32_t (*Carbon_SiegeDefenseFunc)(
    uint32_t location,
    void *userdata
);

/**
 * Callback to get defender faction of a location
 * Returns: Faction ID of the defender
 */
typedef uint32_t (*Carbon_SiegeDefenderFunc)(
    uint32_t location,
    void *userdata
);

/**
 * Callback to calculate damage for a round
 * Returns: Damage to deal this round
 */
typedef int32_t (*Carbon_SiegeDamageFunc)(
    const Carbon_Siege *siege,
    void *userdata
);

/**
 * Callback when siege events occur
 */
typedef void (*Carbon_SiegeEventFunc)(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    Carbon_SiegeEvent event,
    const Carbon_SiegeRoundResult *result,
    void *userdata
);

/**
 * Callback to check if siege can begin (custom validation)
 * Returns: true if siege is allowed
 */
typedef bool (*Carbon_SiegeCanBeginFunc)(
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force,
    void *userdata
);

/**
 * Callback to populate buildings for a location
 * Returns: Number of buildings added
 */
typedef int (*Carbon_SiegeBuildingsFunc)(
    uint32_t location,
    Carbon_SiegeBuilding *out_buildings,
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
Carbon_SiegeManager *carbon_siege_create(void);

/**
 * Create a siege manager with event dispatcher
 * Returns: New siege manager or NULL on failure
 */
Carbon_SiegeManager *carbon_siege_create_with_events(
    Carbon_EventDispatcher *events
);

/**
 * Destroy a siege manager and free resources
 */
void carbon_siege_destroy(Carbon_SiegeManager *mgr);

/*============================================================================
 * Configuration
 *============================================================================*/

/**
 * Get default configuration
 */
Carbon_SiegeConfig carbon_siege_default_config(void);

/**
 * Set siege configuration
 */
void carbon_siege_set_config(
    Carbon_SiegeManager *mgr,
    const Carbon_SiegeConfig *config
);

/**
 * Get current configuration
 */
const Carbon_SiegeConfig *carbon_siege_get_config(
    const Carbon_SiegeManager *mgr
);

/**
 * Set maximum rounds for new sieges
 */
void carbon_siege_set_max_rounds(Carbon_SiegeManager *mgr, int32_t max_rounds);

/**
 * Set minimum force ratio required to begin siege
 */
void carbon_siege_set_min_force_ratio(Carbon_SiegeManager *mgr, float ratio);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set callback to get defense value of a location
 */
void carbon_siege_set_defense_callback(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeDefenseFunc callback,
    void *userdata
);

/**
 * Set callback to get defender faction of a location
 */
void carbon_siege_set_defender_callback(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeDefenderFunc callback,
    void *userdata
);

/**
 * Set callback to calculate damage per round
 */
void carbon_siege_set_damage_callback(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeDamageFunc callback,
    void *userdata
);

/**
 * Set callback for siege events
 */
void carbon_siege_set_event_callback(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeEventFunc callback,
    void *userdata
);

/**
 * Set custom validation callback for beginning sieges
 */
void carbon_siege_set_can_begin_callback(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeCanBeginFunc callback,
    void *userdata
);

/**
 * Set callback to populate buildings for siege targets
 */
void carbon_siege_set_buildings_callback(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeBuildingsFunc callback,
    void *userdata
);

/*============================================================================
 * Siege Lifecycle
 *============================================================================*/

/**
 * Check if a siege can begin
 * Returns: true if siege requirements are met
 */
bool carbon_siege_can_begin(
    Carbon_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force
);

/**
 * Begin a new siege
 * Returns: Siege ID or CARBON_SIEGE_INVALID on failure
 */
uint32_t carbon_siege_begin(
    Carbon_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t target_location,
    int32_t attacking_force
);

/**
 * Begin a siege with extended options
 * Returns: Siege ID or CARBON_SIEGE_INVALID on failure
 */
uint32_t carbon_siege_begin_ex(
    Carbon_SiegeManager *mgr,
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
bool carbon_siege_process_round(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    Carbon_SiegeRoundResult *out_result
);

/**
 * End a siege (attacker retreats)
 */
void carbon_siege_retreat(Carbon_SiegeManager *mgr, uint32_t siege_id);

/**
 * Force end a siege with specific status
 */
void carbon_siege_end(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    Carbon_SiegeStatus end_status
);

/*============================================================================
 * Force Modification
 *============================================================================*/

/**
 * Add reinforcements to attacker
 */
void carbon_siege_reinforce_attacker(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t additional_force
);

/**
 * Add reinforcements to defender
 */
void carbon_siege_reinforce_defender(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t additional_force
);

/**
 * Apply casualties to attacker
 */
void carbon_siege_attacker_casualties(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t casualties
);

/**
 * Apply casualties to defender
 */
void carbon_siege_defender_casualties(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    int32_t casualties
);

/*============================================================================
 * Modifier Control
 *============================================================================*/

/**
 * Set attack power modifier
 */
void carbon_siege_set_attack_modifier(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    float modifier
);

/**
 * Set defense power modifier
 */
void carbon_siege_set_defense_modifier(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    float modifier
);

/**
 * Set damage modifier
 */
void carbon_siege_set_damage_modifier(
    Carbon_SiegeManager *mgr,
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
int carbon_siege_add_building(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    uint32_t building_id,
    int32_t max_health,
    int32_t defense_contribution
);

/**
 * Damage a specific building
 * Returns: true if building was damaged/destroyed
 */
bool carbon_siege_damage_building(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    int building_index,
    int32_t damage
);

/**
 * Get building state
 * Returns: Pointer to building or NULL if not found
 */
const Carbon_SiegeBuilding *carbon_siege_get_building(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id,
    int building_index
);

/**
 * Get building damage level
 */
Carbon_BuildingDamageLevel carbon_siege_get_building_damage_level(
    const Carbon_SiegeBuilding *building
);

/**
 * Get count of buildings in siege
 */
int carbon_siege_get_building_count(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get count of destroyed buildings
 */
int carbon_siege_get_destroyed_building_count(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/*============================================================================
 * Queries - Single Siege
 *============================================================================*/

/**
 * Get siege by ID
 * Returns: Pointer to siege or NULL if not found
 */
const Carbon_Siege *carbon_siege_get(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get mutable siege by ID
 * Returns: Pointer to siege or NULL if not found
 */
Carbon_Siege *carbon_siege_get_mut(
    Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Check if siege exists and is active
 */
bool carbon_siege_is_active(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get siege status
 */
Carbon_SiegeStatus carbon_siege_get_status(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get current round number
 */
int32_t carbon_siege_get_round(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get capture progress (0.0-1.0)
 */
float carbon_siege_get_progress(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get remaining rounds before timeout
 */
int32_t carbon_siege_get_remaining_rounds(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get current attack force
 */
int32_t carbon_siege_get_attack_force(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get current defense force
 */
int32_t carbon_siege_get_defense_force(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/**
 * Get force ratio (attack / defense)
 */
float carbon_siege_get_force_ratio(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

/*============================================================================
 * Queries - Batch
 *============================================================================*/

/**
 * Get all active siege IDs
 * Returns: Number of sieges returned
 */
int carbon_siege_get_all_active(
    const Carbon_SiegeManager *mgr,
    uint32_t *out_ids,
    int max
);

/**
 * Get sieges by attacker faction
 * Returns: Number of sieges returned
 */
int carbon_siege_get_by_attacker(
    const Carbon_SiegeManager *mgr,
    uint32_t attacker_faction,
    uint32_t *out_ids,
    int max
);

/**
 * Get sieges by defender faction
 * Returns: Number of sieges returned
 */
int carbon_siege_get_by_defender(
    const Carbon_SiegeManager *mgr,
    uint32_t defender_faction,
    uint32_t *out_ids,
    int max
);

/**
 * Get siege at location
 * Returns: Siege ID or CARBON_SIEGE_INVALID if none
 */
uint32_t carbon_siege_get_at_location(
    const Carbon_SiegeManager *mgr,
    uint32_t location
);

/**
 * Check if location is under siege
 */
bool carbon_siege_has_siege_at(
    const Carbon_SiegeManager *mgr,
    uint32_t location
);

/**
 * Get sieges by status
 * Returns: Number of sieges returned
 */
int carbon_siege_get_by_status(
    const Carbon_SiegeManager *mgr,
    Carbon_SiegeStatus status,
    uint32_t *out_ids,
    int max
);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Get overall siege statistics
 */
void carbon_siege_get_stats(
    const Carbon_SiegeManager *mgr,
    Carbon_SiegeStats *out_stats
);

/**
 * Get total active siege count
 */
int carbon_siege_count_active(const Carbon_SiegeManager *mgr);

/**
 * Reset statistics
 */
void carbon_siege_reset_stats(Carbon_SiegeManager *mgr);

/*============================================================================
 * Turn Integration
 *============================================================================*/

/**
 * Set current turn (for tracking siege timing)
 */
void carbon_siege_set_turn(Carbon_SiegeManager *mgr, int32_t turn);

/**
 * Process all active sieges for one round
 * Returns: Number of sieges processed
 */
int carbon_siege_process_all(
    Carbon_SiegeManager *mgr,
    Carbon_SiegeRoundResult *out_results,
    int max_results
);

/**
 * Update (for time-based siege progress if needed)
 */
void carbon_siege_update(Carbon_SiegeManager *mgr, float delta_time);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Get status name as string
 */
const char *carbon_siege_status_name(Carbon_SiegeStatus status);

/**
 * Get event name as string
 */
const char *carbon_siege_event_name(Carbon_SiegeEvent event);

/**
 * Get building damage level name as string
 */
const char *carbon_siege_damage_level_name(Carbon_BuildingDamageLevel level);

/**
 * Calculate estimated rounds to capture
 * Returns: Estimated rounds or -1 if unlikely to succeed
 */
int carbon_siege_estimate_rounds(
    const Carbon_SiegeManager *mgr,
    uint32_t siege_id
);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_SIEGE_H */
