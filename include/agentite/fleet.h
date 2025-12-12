/**
 * Agentite Fleet/Army Management System
 *
 * Strategic unit management for 4X and strategy games with automated
 * battle resolution, unit counters, morale, commanders, and experience.
 *
 * Ported from AgentiteZ (Zig) fleet system.
 *
 * Usage:
 *   // Create fleet manager
 *   Agentite_FleetManager *fm = agentite_fleet_create();
 *
 *   // Create a fleet
 *   Agentite_Fleet fleet = { .name = "First Fleet", .owner_id = player_id };
 *   int fleet_id = agentite_fleet_add(fm, &fleet);
 *
 *   // Add units to fleet
 *   agentite_fleet_add_units(fm, fleet_id, AGENTITE_UNIT_FIGHTER, 10);
 *
 *   // Assign commander
 *   Agentite_Commander cmd = { .name = "Admiral Smith", .attack_bonus = 10 };
 *   agentite_fleet_set_commander(fm, fleet_id, &cmd);
 *
 *   // Preview battle outcome
 *   Agentite_BattlePreview preview;
 *   agentite_fleet_preview_battle(fm, attacker_id, defender_id, &preview);
 *
 *   // Execute battle
 *   Agentite_BattleResult result;
 *   agentite_fleet_battle(fm, attacker_id, defender_id, &result);
 *
 *   agentite_fleet_destroy(fm);
 */

#ifndef AGENTITE_FLEET_H
#define AGENTITE_FLEET_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_FLEET_MAX              64
#define AGENTITE_FLEET_MAX_UNIT_GROUPS  16
#define AGENTITE_FLEET_INVALID_ID       (-1)

/*============================================================================
 * Unit Classes
 *============================================================================*/

typedef enum Agentite_UnitClass {
    /* Space units */
    AGENTITE_UNIT_FIGHTER = 0,    /* Fast, anti-bomber */
    AGENTITE_UNIT_BOMBER,         /* Anti-capital */
    AGENTITE_UNIT_CORVETTE,       /* Light multi-role */
    AGENTITE_UNIT_FRIGATE,        /* Anti-fighter screen */
    AGENTITE_UNIT_DESTROYER,      /* Balanced combat */
    AGENTITE_UNIT_CRUISER,        /* Heavy combat */
    AGENTITE_UNIT_BATTLESHIP,     /* Capital ship */
    AGENTITE_UNIT_CARRIER,        /* Fighter/bomber platform */
    AGENTITE_UNIT_DREADNOUGHT,    /* Super-capital */

    /* Ground units */
    AGENTITE_UNIT_INFANTRY,       /* Basic ground */
    AGENTITE_UNIT_ARMOR,          /* Tanks/vehicles */
    AGENTITE_UNIT_ARTILLERY,      /* Ranged support */
    AGENTITE_UNIT_MECH,           /* Heavy assault */
    AGENTITE_UNIT_SPECIAL_OPS,    /* Elite infantry */
    AGENTITE_UNIT_ANTI_AIR,       /* Air defense */
    AGENTITE_UNIT_ENGINEER,       /* Construction/repair */
    AGENTITE_UNIT_TRANSPORT,      /* Troop carrier */
    AGENTITE_UNIT_DROPSHIP,       /* Orbital insertion */

    AGENTITE_UNIT_CLASS_COUNT
} Agentite_UnitClass;

/*============================================================================
 * Unit Domain
 *============================================================================*/

typedef enum Agentite_UnitDomain {
    AGENTITE_DOMAIN_SPACE,
    AGENTITE_DOMAIN_GROUND,
    AGENTITE_DOMAIN_AIR,          /* For future use */
    AGENTITE_DOMAIN_NAVAL,        /* For future use */
} Agentite_UnitDomain;

/*============================================================================
 * Counter Effectiveness
 *============================================================================*/

typedef enum Agentite_Effectiveness {
    AGENTITE_EFFECT_HARD_COUNTER = 0,  /* 0.5x damage dealt */
    AGENTITE_EFFECT_WEAK,              /* 0.75x damage dealt */
    AGENTITE_EFFECT_NEUTRAL,           /* 1.0x damage dealt */
    AGENTITE_EFFECT_STRONG,            /* 1.25x damage dealt */
    AGENTITE_EFFECT_COUNTER,           /* 1.5x damage dealt */
} Agentite_Effectiveness;

/*============================================================================
 * Commander Abilities
 *============================================================================*/

typedef enum Agentite_CommanderAbility {
    AGENTITE_ABILITY_NONE = 0,
    AGENTITE_ABILITY_FIRST_STRIKE,     /* Bonus damage in round 1 */
    AGENTITE_ABILITY_TACTICAL_RETREAT, /* Reduced losses on retreat */
    AGENTITE_ABILITY_INSPIRATION,      /* Morale bonus */
    AGENTITE_ABILITY_FLANKING,         /* Bonus vs damaged enemies */
    AGENTITE_ABILITY_FORTIFY,          /* Defensive bonus */
    AGENTITE_ABILITY_BLITZ,            /* Extra attack speed */
    AGENTITE_ABILITY_LOGISTICS,        /* Reduced supply cost */
    AGENTITE_ABILITY_VETERAN_TRAINING, /* Faster experience gain */
} Agentite_CommanderAbility;

/*============================================================================
 * Battle Result
 *============================================================================*/

typedef enum Agentite_BattleOutcome {
    AGENTITE_BATTLE_ATTACKER_WIN,
    AGENTITE_BATTLE_DEFENDER_WIN,
    AGENTITE_BATTLE_DRAW,
    AGENTITE_BATTLE_ATTACKER_RETREAT,
    AGENTITE_BATTLE_DEFENDER_RETREAT,
} Agentite_BattleOutcome;

/*============================================================================
 * Structures
 *============================================================================*/

/**
 * Base stats for a unit class.
 */
typedef struct Agentite_UnitStats {
    Agentite_UnitClass unit_class;
    Agentite_UnitDomain domain;
    char name[32];

    int attack;
    int defense;
    int hp;
    int speed;
    int range;
    int cost;
    int upkeep;

    /* Counters */
    Agentite_UnitClass strong_against[4];
    int strong_count;
    Agentite_UnitClass weak_against[4];
    int weak_count;
} Agentite_UnitStats;

/**
 * A group of units of the same type within a fleet.
 */
typedef struct Agentite_UnitGroup {
    Agentite_UnitClass unit_class;
    int count;                    /* Number of units */
    int health;                   /* Current HP (per unit average) */
    int experience;               /* XP gained (0-1000) */
    int kills;                    /* Enemy units destroyed */
} Agentite_UnitGroup;

/**
 * Commander leading a fleet.
 */
typedef struct Agentite_Commander {
    char name[64];
    int32_t entity_id;            /* Link to game entity */

    /* Stat bonuses (percentage) */
    int attack_bonus;             /* +attack_bonus% to fleet attack */
    int defense_bonus;            /* +defense_bonus% to fleet defense */
    int morale_bonus;             /* +morale_bonus to morale */
    int speed_bonus;              /* +speed_bonus% to fleet speed */

    /* Experience */
    int level;                    /* Commander level (1-10) */
    int experience;               /* XP toward next level */

    /* Special ability */
    Agentite_CommanderAbility ability;
    int ability_cooldown;         /* Turns until ability available */
} Agentite_Commander;

/**
 * A fleet/army.
 */
typedef struct Agentite_Fleet {
    char name[64];
    int32_t owner_id;             /* Owner player/faction */
    int32_t entity_id;            /* Link to game entity */

    /* Units */
    Agentite_UnitGroup units[AGENTITE_FLEET_MAX_UNIT_GROUPS];
    int unit_count;

    /* Commander */
    Agentite_Commander commander;
    bool has_commander;

    /* State */
    int morale;                   /* 0-100 */
    int supply;                   /* Supply/fuel remaining */
    int supply_max;

    /* Position (game-defined) */
    int32_t position_x;
    int32_t position_y;
    int32_t sector_id;

    /* Flags */
    bool is_space_fleet;          /* true = space, false = ground army */
    bool in_combat;
    bool is_retreating;
} Agentite_Fleet;

/**
 * Round-by-round battle record.
 */
typedef struct Agentite_BattleRound {
    int round_number;
    int attacker_damage;
    int defender_damage;
    int attacker_losses;          /* Units lost */
    int defender_losses;
    int attacker_morale;
    int defender_morale;
} Agentite_BattleRound;

/**
 * Full battle result.
 */
typedef struct Agentite_BattleResult {
    Agentite_BattleOutcome outcome;

    int rounds_fought;
    Agentite_BattleRound rounds[20];  /* Max 20 rounds */

    /* Total casualties */
    int attacker_units_lost;
    int defender_units_lost;
    int attacker_units_remaining;
    int defender_units_remaining;

    /* Experience gained */
    int attacker_xp;
    int defender_xp;

    /* Fleet IDs */
    int attacker_id;
    int defender_id;
    int winner_id;
} Agentite_BattleResult;

/**
 * Battle preview (estimated outcome using Lanchester's laws).
 */
typedef struct Agentite_BattlePreview {
    float attacker_win_chance;
    float defender_win_chance;

    int estimated_attacker_losses;
    int estimated_defender_losses;

    int attacker_strength;
    int defender_strength;

    /* Warnings */
    bool attacker_outmatched;
    bool defender_outmatched;
} Agentite_BattlePreview;

/**
 * Forward declaration.
 */
typedef struct Agentite_FleetManager Agentite_FleetManager;

/**
 * Battle event callback.
 */
typedef void (*Agentite_BattleCallback)(
    Agentite_FleetManager *fm,
    const Agentite_BattleRound *round,
    void *userdata
);

/*============================================================================
 * Fleet Manager Creation
 *============================================================================*/

/**
 * Create a new fleet manager.
 *
 * @return New fleet manager or NULL on failure
 */
Agentite_FleetManager *agentite_fleet_create(void);

/**
 * Destroy fleet manager and free resources.
 *
 * @param fm Fleet manager to destroy
 */
void agentite_fleet_destroy(Agentite_FleetManager *fm);

/*============================================================================
 * Fleet Management
 *============================================================================*/

/**
 * Add a new fleet.
 *
 * @param fm    Fleet manager
 * @param fleet Fleet data (copied)
 * @return Fleet ID or AGENTITE_FLEET_INVALID_ID on failure
 */
int agentite_fleet_add(Agentite_FleetManager *fm, const Agentite_Fleet *fleet);

/**
 * Remove a fleet.
 *
 * @param fm Fleet manager
 * @param id Fleet ID
 * @return true if removed
 */
bool agentite_fleet_remove(Agentite_FleetManager *fm, int id);

/**
 * Get a fleet by ID.
 *
 * @param fm Fleet manager
 * @param id Fleet ID
 * @return Fleet data or NULL
 */
Agentite_Fleet *agentite_fleet_get(Agentite_FleetManager *fm, int id);

/**
 * Get fleet by const reference.
 */
const Agentite_Fleet *agentite_fleet_get_const(const Agentite_FleetManager *fm, int id);

/**
 * Get number of fleets.
 *
 * @param fm Fleet manager
 * @return Fleet count
 */
int agentite_fleet_count(const Agentite_FleetManager *fm);

/**
 * Get all fleets owned by a player.
 *
 * @param fm       Fleet manager
 * @param owner_id Owner ID
 * @param out_ids  Output array for fleet IDs
 * @param max_count Maximum IDs to return
 * @return Number of fleets owned
 */
int agentite_fleet_get_by_owner(
    const Agentite_FleetManager *fm,
    int32_t owner_id,
    int *out_ids,
    int max_count
);

/*============================================================================
 * Unit Management
 *============================================================================*/

/**
 * Add units to a fleet.
 *
 * @param fm         Fleet manager
 * @param fleet_id   Fleet ID
 * @param unit_class Unit class to add
 * @param count      Number of units
 * @return true if added
 */
bool agentite_fleet_add_units(
    Agentite_FleetManager *fm,
    int fleet_id,
    Agentite_UnitClass unit_class,
    int count
);

/**
 * Remove units from a fleet.
 *
 * @param fm         Fleet manager
 * @param fleet_id   Fleet ID
 * @param unit_class Unit class to remove
 * @param count      Number of units
 * @return Actual units removed
 */
int agentite_fleet_remove_units(
    Agentite_FleetManager *fm,
    int fleet_id,
    Agentite_UnitClass unit_class,
    int count
);

/**
 * Get unit count in a fleet.
 *
 * @param fm         Fleet manager
 * @param fleet_id   Fleet ID
 * @param unit_class Unit class (or -1 for total)
 * @return Unit count
 */
int agentite_fleet_get_unit_count(
    const Agentite_FleetManager *fm,
    int fleet_id,
    int unit_class
);

/**
 * Get total fleet strength (combat power).
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @return Combat strength value
 */
int agentite_fleet_get_strength(const Agentite_FleetManager *fm, int fleet_id);

/**
 * Get unit stats for a class.
 *
 * @param unit_class Unit class
 * @return Static unit stats
 */
const Agentite_UnitStats *agentite_unit_get_stats(Agentite_UnitClass unit_class);

/**
 * Get effectiveness multiplier between unit classes.
 *
 * @param attacker Attacking unit class
 * @param defender Defending unit class
 * @return Effectiveness enum
 */
Agentite_Effectiveness agentite_unit_get_effectiveness(
    Agentite_UnitClass attacker,
    Agentite_UnitClass defender
);

/**
 * Get damage multiplier for effectiveness.
 *
 * @param effectiveness Effectiveness level
 * @return Damage multiplier (0.5 to 1.5)
 */
float agentite_effectiveness_multiplier(Agentite_Effectiveness effectiveness);

/*============================================================================
 * Commander Management
 *============================================================================*/

/**
 * Set fleet commander.
 *
 * @param fm        Fleet manager
 * @param fleet_id  Fleet ID
 * @param commander Commander data (copied, NULL to remove)
 * @return true if set
 */
bool agentite_fleet_set_commander(
    Agentite_FleetManager *fm,
    int fleet_id,
    const Agentite_Commander *commander
);

/**
 * Get fleet commander.
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @return Commander data or NULL if none
 */
Agentite_Commander *agentite_fleet_get_commander(Agentite_FleetManager *fm, int fleet_id);

/**
 * Add experience to commander.
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @param xp       Experience points to add
 * @return true if leveled up
 */
bool agentite_fleet_commander_add_xp(Agentite_FleetManager *fm, int fleet_id, int xp);

/**
 * Calculate commander bonus percentage.
 *
 * @param commander Commander
 * @param stat      0=attack, 1=defense, 2=morale, 3=speed
 * @return Bonus percentage
 */
int agentite_commander_get_bonus(const Agentite_Commander *commander, int stat);

/*============================================================================
 * Fleet Operations
 *============================================================================*/

/**
 * Merge two fleets together.
 *
 * @param fm     Fleet manager
 * @param dst_id Destination fleet ID
 * @param src_id Source fleet ID (will be removed)
 * @return true if merged
 */
bool agentite_fleet_merge(Agentite_FleetManager *fm, int dst_id, int src_id);

/**
 * Split units from a fleet into a new fleet.
 *
 * @param fm              Fleet manager
 * @param src_id          Source fleet ID
 * @param unit_class      Unit class to split
 * @param count           Number of units
 * @param new_fleet_name  Name for new fleet
 * @return New fleet ID or AGENTITE_FLEET_INVALID_ID
 */
int agentite_fleet_split(
    Agentite_FleetManager *fm,
    int src_id,
    Agentite_UnitClass unit_class,
    int count,
    const char *new_fleet_name
);

/**
 * Update fleet morale.
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @param delta    Morale change (positive or negative)
 */
void agentite_fleet_update_morale(Agentite_FleetManager *fm, int fleet_id, int delta);

/**
 * Repair/reinforce fleet units.
 *
 * @param fm         Fleet manager
 * @param fleet_id   Fleet ID
 * @param heal_amount HP to restore per unit
 */
void agentite_fleet_repair(Agentite_FleetManager *fm, int fleet_id, int heal_amount);

/*============================================================================
 * Battle System
 *============================================================================*/

/**
 * Preview battle outcome without executing.
 * Uses Lanchester's laws for estimation.
 *
 * @param fm          Fleet manager
 * @param attacker_id Attacking fleet ID
 * @param defender_id Defending fleet ID
 * @param out_preview Output preview data
 * @return true if preview generated
 */
bool agentite_fleet_preview_battle(
    const Agentite_FleetManager *fm,
    int attacker_id,
    int defender_id,
    Agentite_BattlePreview *out_preview
);

/**
 * Execute a battle between two fleets.
 *
 * @param fm          Fleet manager
 * @param attacker_id Attacking fleet ID
 * @param defender_id Defending fleet ID
 * @param out_result  Output battle result
 * @return true if battle executed
 */
bool agentite_fleet_battle(
    Agentite_FleetManager *fm,
    int attacker_id,
    int defender_id,
    Agentite_BattleResult *out_result
);

/**
 * Set battle event callback (for animations/logging).
 *
 * @param fm       Fleet manager
 * @param callback Callback function
 * @param userdata User data
 */
void agentite_fleet_set_battle_callback(
    Agentite_FleetManager *fm,
    Agentite_BattleCallback callback,
    void *userdata
);

/**
 * Force a fleet to retreat.
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @return true if retreat started
 */
bool agentite_fleet_retreat(Agentite_FleetManager *fm, int fleet_id);

/*============================================================================
 * Experience System
 *============================================================================*/

/**
 * Add experience to fleet units.
 *
 * @param fm         Fleet manager
 * @param fleet_id   Fleet ID
 * @param unit_class Unit class (-1 for all)
 * @param xp         Experience points
 */
void agentite_fleet_add_unit_xp(
    Agentite_FleetManager *fm,
    int fleet_id,
    int unit_class,
    int xp
);

/**
 * Get experience bonus multiplier for unit group.
 * Veterans deal more damage.
 *
 * @param group Unit group
 * @return Damage multiplier (1.0 to 1.5)
 */
float agentite_unit_xp_bonus(const Agentite_UnitGroup *group);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Get unit class name.
 *
 * @param unit_class Unit class
 * @return Static string name
 */
const char *agentite_unit_class_name(Agentite_UnitClass unit_class);

/**
 * Get unit domain name.
 *
 * @param domain Unit domain
 * @return Static string name
 */
const char *agentite_unit_domain_name(Agentite_UnitDomain domain);

/**
 * Get battle outcome name.
 *
 * @param outcome Battle outcome
 * @return Static string name
 */
const char *agentite_battle_outcome_name(Agentite_BattleOutcome outcome);

/**
 * Get commander ability name.
 *
 * @param ability Commander ability
 * @return Static string name
 */
const char *agentite_commander_ability_name(Agentite_CommanderAbility ability);

/**
 * Calculate fleet upkeep cost.
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @return Upkeep cost per turn
 */
int agentite_fleet_get_upkeep(const Agentite_FleetManager *fm, int fleet_id);

/**
 * Calculate fleet build cost.
 *
 * @param fm       Fleet manager
 * @param fleet_id Fleet ID
 * @return Total build cost of all units
 */
int agentite_fleet_get_cost(const Agentite_FleetManager *fm, int fleet_id);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_FLEET_H */
