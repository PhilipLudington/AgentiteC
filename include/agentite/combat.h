/**
 * Agentite Tactical Combat System
 *
 * Turn-based tactical combat with initiative ordering, telegraphing,
 * reaction mechanics, status effects, and grid-based positioning.
 *
 * Ported from AgentiteZ (Zig) combat system.
 *
 * Usage:
 *   // Create combat system
 *   Agentite_CombatSystem *combat = agentite_combat_create(16, 16);
 *
 *   // Add combatants
 *   Agentite_Combatant player = { .name = "Hero", .hp = 100, .initiative = 10 };
 *   int player_id = agentite_combat_add_combatant(combat, &player, true);
 *
 *   Agentite_Combatant enemy = { .name = "Goblin", .hp = 30, .initiative = 5 };
 *   int enemy_id = agentite_combat_add_combatant(combat, &enemy, false);
 *
 *   // Start combat
 *   agentite_combat_start(combat);
 *
 *   // Game loop
 *   while (!agentite_combat_is_over(combat)) {
 *       int current = agentite_combat_get_current_combatant(combat);
 *       // Get telegraphs, make decisions, queue actions
 *       agentite_combat_queue_action(combat, &action);
 *       agentite_combat_execute_turn(combat);
 *   }
 *
 *   agentite_combat_destroy(combat);
 */

#ifndef AGENTITE_COMBAT_H
#define AGENTITE_COMBAT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_COMBAT_MAX_COMBATANTS   32
#define AGENTITE_COMBAT_MAX_STATUS       8
#define AGENTITE_COMBAT_MAX_ABILITIES    8
#define AGENTITE_COMBAT_MAX_ACTIONS      64
#define AGENTITE_COMBAT_INVALID_ID       (-1)

/*============================================================================
 * Status Effect Types
 *============================================================================*/

typedef enum Agentite_StatusType {
    AGENTITE_STATUS_NONE = 0,
    AGENTITE_STATUS_STUNNED,      /* Cannot act */
    AGENTITE_STATUS_BURNING,      /* Damage over time (fire) */
    AGENTITE_STATUS_POISONED,     /* Damage over time (poison) */
    AGENTITE_STATUS_BLEEDING,     /* Damage over time (physical) */
    AGENTITE_STATUS_ROOTED,       /* Cannot move */
    AGENTITE_STATUS_BLINDED,      /* Reduced hit chance */
    AGENTITE_STATUS_VULNERABLE,   /* +50% damage taken */
    AGENTITE_STATUS_FORTIFIED,    /* -25% damage taken */
    AGENTITE_STATUS_HASTED,       /* Extra action */
    AGENTITE_STATUS_SLOWED,       /* Reduced initiative */
    AGENTITE_STATUS_INVULNERABLE, /* No damage */
    AGENTITE_STATUS_CONCEALED,    /* Harder to hit */
    AGENTITE_STATUS_INJURED,      /* Reduced max HP */
    AGENTITE_STATUS_COUNT
} Agentite_StatusType;

/*============================================================================
 * Action Types
 *============================================================================*/

typedef enum Agentite_ActionType {
    AGENTITE_ACTION_NONE = 0,
    AGENTITE_ACTION_MOVE,         /* Move to position */
    AGENTITE_ACTION_ATTACK,       /* Basic attack */
    AGENTITE_ACTION_DEFEND,       /* Defensive stance */
    AGENTITE_ACTION_USE_ITEM,     /* Use consumable */
    AGENTITE_ACTION_ABILITY,      /* Use special ability */
    AGENTITE_ACTION_WAIT,         /* Skip turn (keep reaction) */
    AGENTITE_ACTION_FLEE,         /* Attempt to flee combat */
} Agentite_ActionType;

/*============================================================================
 * Combat Result
 *============================================================================*/

typedef enum Agentite_CombatResult {
    AGENTITE_COMBAT_ONGOING = 0,
    AGENTITE_COMBAT_VICTORY,      /* Player team won */
    AGENTITE_COMBAT_DEFEAT,       /* Enemy team won */
    AGENTITE_COMBAT_FLED,         /* Player fled */
    AGENTITE_COMBAT_DRAW,         /* Both sides eliminated */
} Agentite_CombatResult;

/*============================================================================
 * Distance Type
 *============================================================================*/

typedef enum Agentite_DistanceType {
    AGENTITE_DISTANCE_CHEBYSHEV,  /* Max of dx, dy (8-directional) */
    AGENTITE_DISTANCE_MANHATTAN,  /* dx + dy (4-directional) */
    AGENTITE_DISTANCE_EUCLIDEAN,  /* sqrt(dx^2 + dy^2) */
} Agentite_DistanceType;

/*============================================================================
 * Structures
 *============================================================================*/

/**
 * Grid position.
 */
typedef struct Agentite_GridPos {
    int x;
    int y;
} Agentite_GridPos;

/**
 * Status effect instance.
 */
typedef struct Agentite_StatusEffect {
    Agentite_StatusType type;
    int duration;                 /* Turns remaining (-1 = permanent) */
    int stacks;                   /* Stack count (for stackable effects) */
    float damage_per_tick;        /* For DoT effects */
    int source_id;                /* Who applied this effect */
} Agentite_StatusEffect;

/**
 * Attack definition.
 */
typedef struct Agentite_Attack {
    char name[32];
    int base_damage;
    int range;                    /* Grid units */
    float hit_chance;             /* 0.0 to 1.0 */
    bool piercing;                /* Ignores armor */
    int aoe_radius;               /* 0 = single target */
    Agentite_StatusType applies_status;
    int status_duration;
    float status_chance;          /* Chance to apply status */
} Agentite_Attack;

/**
 * Ability definition.
 */
typedef struct Agentite_Ability {
    char name[32];
    char description[128];
    int cooldown_max;             /* Turns between uses */
    int cooldown_current;         /* Turns until available */
    int resource_cost;            /* Mana/energy cost */
    Agentite_Attack attack;       /* Attack data if offensive */
    bool is_offensive;
    bool targets_self;
    bool targets_allies;
    int heal_amount;              /* If healing ability */
} Agentite_Ability;

/**
 * Combatant data.
 */
typedef struct Agentite_Combatant {
    /* Identity */
    char name[64];
    int32_t entity_id;            /* Link to game entity */

    /* Health */
    int hp;
    int hp_max;
    int temp_hp;                  /* Absorbs damage first */

    /* Combat stats */
    int initiative;               /* Turn order (higher = earlier) */
    int armor;                    /* Reduces non-piercing damage */
    float dodge_chance;           /* Base dodge chance */
    int attack_bonus;             /* Added to attack damage */
    int defense_bonus;            /* Added to armor when defending */

    /* Position */
    Agentite_GridPos position;
    int movement_range;           /* Tiles per move action */

    /* Abilities */
    Agentite_Ability abilities[AGENTITE_COMBAT_MAX_ABILITIES];
    int ability_count;

    /* Status effects */
    Agentite_StatusEffect status[AGENTITE_COMBAT_MAX_STATUS];
    int status_count;

    /* Combat state */
    bool has_acted;               /* Used action this turn */
    bool has_moved;               /* Used movement this turn */
    bool is_defending;            /* In defensive stance */
    bool is_alive;
    bool is_player_team;

    /* Resources */
    int resource;                 /* Mana/energy/etc */
    int resource_max;
} Agentite_Combatant;

/**
 * Telegraph - shows enemy intent before player commits.
 */
typedef struct Agentite_Telegraph {
    int attacker_id;
    int target_id;
    Agentite_ActionType action_type;
    int ability_index;            /* If using ability */
    int predicted_damage;
    float hit_chance;
    Agentite_StatusType status_applied;
    Agentite_GridPos target_pos;  /* For movement */
} Agentite_Telegraph;

/**
 * Combat action.
 */
typedef struct Agentite_CombatAction {
    Agentite_ActionType type;
    int actor_id;
    int target_id;                /* Target combatant (-1 if position) */
    Agentite_GridPos target_pos;  /* For move/AoE */
    int ability_index;            /* For ability use */
    int item_id;                  /* For item use */
} Agentite_CombatAction;

/**
 * Combat event (for logging/animation).
 */
typedef struct Agentite_CombatEvent {
    Agentite_ActionType action;
    int actor_id;
    int target_id;
    int damage_dealt;
    int damage_blocked;
    bool was_critical;
    bool was_dodged;
    bool was_countered;
    Agentite_StatusType status_applied;
    char description[128];
} Agentite_CombatEvent;

/**
 * Forward declaration.
 */
typedef struct Agentite_CombatSystem Agentite_CombatSystem;

/**
 * Combat event callback.
 */
typedef void (*Agentite_CombatEventCallback)(
    Agentite_CombatSystem *combat,
    const Agentite_CombatEvent *event,
    void *userdata
);

/*============================================================================
 * Combat System Creation
 *============================================================================*/

/**
 * Create a new combat system.
 *
 * @param grid_width   Width of combat grid
 * @param grid_height  Height of combat grid
 * @return New combat system or NULL on failure
 */
Agentite_CombatSystem *agentite_combat_create(int grid_width, int grid_height);

/**
 * Destroy combat system and free resources.
 *
 * @param combat Combat system to destroy
 */
void agentite_combat_destroy(Agentite_CombatSystem *combat);

/**
 * Reset combat system for new battle.
 *
 * @param combat Combat system
 */
void agentite_combat_reset(Agentite_CombatSystem *combat);

/*============================================================================
 * Combatant Management
 *============================================================================*/

/**
 * Add a combatant to the battle.
 *
 * @param combat       Combat system
 * @param combatant    Combatant data (copied)
 * @param is_player    true if on player's team
 * @return Combatant ID or AGENTITE_COMBAT_INVALID_ID on failure
 */
int agentite_combat_add_combatant(
    Agentite_CombatSystem *combat,
    const Agentite_Combatant *combatant,
    bool is_player
);

/**
 * Get a combatant by ID.
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @return Combatant data or NULL
 */
Agentite_Combatant *agentite_combat_get_combatant(Agentite_CombatSystem *combat, int id);

/**
 * Get combatant by const reference.
 */
const Agentite_Combatant *agentite_combat_get_combatant_const(
    const Agentite_CombatSystem *combat,
    int id
);

/**
 * Get number of combatants.
 *
 * @param combat Combat system
 * @return Total combatant count
 */
int agentite_combat_get_combatant_count(const Agentite_CombatSystem *combat);

/**
 * Get all combatants on a team.
 *
 * @param combat     Combat system
 * @param is_player  true for player team, false for enemies
 * @param out_ids    Output array for IDs
 * @param max_count  Maximum IDs to return
 * @return Number of combatants on team
 */
int agentite_combat_get_team(
    const Agentite_CombatSystem *combat,
    bool is_player,
    int *out_ids,
    int max_count
);

/*============================================================================
 * Combat Flow
 *============================================================================*/

/**
 * Start combat (calculates turn order, generates initial telegraphs).
 *
 * @param combat Combat system
 */
void agentite_combat_start(Agentite_CombatSystem *combat);

/**
 * Check if combat is over.
 *
 * @param combat Combat system
 * @return true if combat has ended
 */
bool agentite_combat_is_over(const Agentite_CombatSystem *combat);

/**
 * Get combat result.
 *
 * @param combat Combat system
 * @return Combat result
 */
Agentite_CombatResult agentite_combat_get_result(const Agentite_CombatSystem *combat);

/**
 * Get current turn number.
 *
 * @param combat Combat system
 * @return Turn number (starts at 1)
 */
int agentite_combat_get_turn(const Agentite_CombatSystem *combat);

/**
 * Get ID of combatant whose turn it is.
 *
 * @param combat Combat system
 * @return Combatant ID
 */
int agentite_combat_get_current_combatant(const Agentite_CombatSystem *combat);

/**
 * Get turn order (sorted by initiative).
 *
 * @param combat    Combat system
 * @param out_ids   Output array for combatant IDs
 * @param max_count Maximum IDs to return
 * @return Number of combatants in turn order
 */
int agentite_combat_get_turn_order(
    const Agentite_CombatSystem *combat,
    int *out_ids,
    int max_count
);

/*============================================================================
 * Actions
 *============================================================================*/

/**
 * Queue an action for the current combatant.
 *
 * @param combat Combat system
 * @param action Action to queue
 * @return true if action was valid and queued
 */
bool agentite_combat_queue_action(
    Agentite_CombatSystem *combat,
    const Agentite_CombatAction *action
);

/**
 * Execute the current turn (process queued actions).
 *
 * @param combat Combat system
 * @return true if turn was executed
 */
bool agentite_combat_execute_turn(Agentite_CombatSystem *combat);

/**
 * Skip to the next combatant's turn.
 *
 * @param combat Combat system
 */
void agentite_combat_next_turn(Agentite_CombatSystem *combat);

/**
 * Check if an action is valid.
 *
 * @param combat Combat system
 * @param action Action to validate
 * @return true if action can be performed
 */
bool agentite_combat_is_action_valid(
    const Agentite_CombatSystem *combat,
    const Agentite_CombatAction *action
);

/*============================================================================
 * Telegraphing (Enemy Intent)
 *============================================================================*/

/**
 * Get telegraphs for all enemies.
 * Called at start of player turn to show enemy intent.
 *
 * @param combat     Combat system
 * @param out_telegraphs Output array
 * @param max_count  Maximum telegraphs to return
 * @return Number of telegraphs
 */
int agentite_combat_get_telegraphs(
    const Agentite_CombatSystem *combat,
    Agentite_Telegraph *out_telegraphs,
    int max_count
);

/**
 * Generate AI actions for enemies (call after player commits).
 *
 * @param combat Combat system
 */
void agentite_combat_generate_enemy_actions(Agentite_CombatSystem *combat);

/*============================================================================
 * Reactions (Dodge/Counter)
 *============================================================================*/

/**
 * Check if combatant can dodge (hasn't acted, has dodge chance).
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @return true if can attempt dodge
 */
bool agentite_combat_can_dodge(const Agentite_CombatSystem *combat, int id);

/**
 * Check if combatant can counter-attack.
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @return true if can counter
 */
bool agentite_combat_can_counter(const Agentite_CombatSystem *combat, int id);

/**
 * Get dodge chance for a combatant (includes status effects).
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @return Dodge chance (0.0 to 1.0)
 */
float agentite_combat_get_dodge_chance(const Agentite_CombatSystem *combat, int id);

/*============================================================================
 * Damage Calculation
 *============================================================================*/

/**
 * Calculate damage from an attack.
 *
 * @param combat    Combat system
 * @param attacker  Attacker ID
 * @param defender  Defender ID
 * @param attack    Attack data
 * @return Damage after mitigation
 */
int agentite_combat_calculate_damage(
    const Agentite_CombatSystem *combat,
    int attacker,
    int defender,
    const Agentite_Attack *attack
);

/**
 * Apply damage to a combatant.
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @param damage Damage amount
 * @return Actual damage dealt (after temp HP)
 */
int agentite_combat_apply_damage(Agentite_CombatSystem *combat, int id, int damage);

/**
 * Heal a combatant.
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @param amount Heal amount
 * @return Actual amount healed
 */
int agentite_combat_heal(Agentite_CombatSystem *combat, int id, int amount);

/*============================================================================
 * Status Effects
 *============================================================================*/

/**
 * Apply a status effect to a combatant.
 *
 * @param combat   Combat system
 * @param id       Target combatant ID
 * @param type     Status type
 * @param duration Duration in turns (-1 = permanent)
 * @param stacks   Stack count
 * @param source   Source combatant ID
 * @return true if applied
 */
bool agentite_combat_apply_status(
    Agentite_CombatSystem *combat,
    int id,
    Agentite_StatusType type,
    int duration,
    int stacks,
    int source
);

/**
 * Remove a status effect.
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @param type   Status type to remove
 * @return true if removed
 */
bool agentite_combat_remove_status(
    Agentite_CombatSystem *combat,
    int id,
    Agentite_StatusType type
);

/**
 * Check if combatant has a status effect.
 *
 * @param combat Combat system
 * @param id     Combatant ID
 * @param type   Status type
 * @return true if has status
 */
bool agentite_combat_has_status(
    const Agentite_CombatSystem *combat,
    int id,
    Agentite_StatusType type
);

/**
 * Process status effects at end of turn (DoT damage, duration reduction).
 *
 * @param combat Combat system
 * @param id     Combatant ID
 */
void agentite_combat_tick_status(Agentite_CombatSystem *combat, int id);

/*============================================================================
 * Grid and Movement
 *============================================================================*/

/**
 * Calculate distance between two positions.
 *
 * @param from      Starting position
 * @param to        Ending position
 * @param type      Distance calculation type
 * @return Distance in grid units
 */
int agentite_combat_distance(
    Agentite_GridPos from,
    Agentite_GridPos to,
    Agentite_DistanceType type
);

/**
 * Check if a position is valid (within grid bounds).
 *
 * @param combat Combat system
 * @param pos    Position to check
 * @return true if valid
 */
bool agentite_combat_is_position_valid(
    const Agentite_CombatSystem *combat,
    Agentite_GridPos pos
);

/**
 * Check if a position is occupied.
 *
 * @param combat Combat system
 * @param pos    Position to check
 * @return Combatant ID at position or -1
 */
int agentite_combat_get_combatant_at(
    const Agentite_CombatSystem *combat,
    Agentite_GridPos pos
);

/**
 * Get valid movement positions for a combatant.
 *
 * @param combat    Combat system
 * @param id        Combatant ID
 * @param out_pos   Output array for positions
 * @param max_count Maximum positions to return
 * @return Number of valid positions
 */
int agentite_combat_get_valid_moves(
    const Agentite_CombatSystem *combat,
    int id,
    Agentite_GridPos *out_pos,
    int max_count
);

/**
 * Get valid targets for an attack.
 *
 * @param combat    Combat system
 * @param attacker  Attacker ID
 * @param attack    Attack data
 * @param out_ids   Output array for target IDs
 * @param max_count Maximum targets to return
 * @return Number of valid targets
 */
int agentite_combat_get_valid_targets(
    const Agentite_CombatSystem *combat,
    int attacker,
    const Agentite_Attack *attack,
    int *out_ids,
    int max_count
);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set event callback (for animations, logging).
 *
 * @param combat   Combat system
 * @param callback Callback function
 * @param userdata User data
 */
void agentite_combat_set_event_callback(
    Agentite_CombatSystem *combat,
    Agentite_CombatEventCallback callback,
    void *userdata
);

/*============================================================================
 * Utility
 *============================================================================*/

/**
 * Get status effect name.
 *
 * @param type Status type
 * @return Static string name
 */
const char *agentite_status_name(Agentite_StatusType type);

/**
 * Get action type name.
 *
 * @param type Action type
 * @return Static string name
 */
const char *agentite_action_name(Agentite_ActionType type);

/**
 * Create a basic attack.
 *
 * @param name       Attack name
 * @param damage     Base damage
 * @param range      Range in grid units
 * @param hit_chance Hit chance (0.0 to 1.0)
 * @return Attack structure
 */
Agentite_Attack agentite_attack_create(
    const char *name,
    int damage,
    int range,
    float hit_chance
);

/**
 * Set combat grid size.
 *
 * @param combat Combat system
 * @param width  Grid width
 * @param height Grid height
 */
void agentite_combat_set_grid_size(
    Agentite_CombatSystem *combat,
    int width,
    int height
);

/**
 * Set distance calculation type.
 *
 * @param combat Combat system
 * @param type   Distance type
 */
void agentite_combat_set_distance_type(
    Agentite_CombatSystem *combat,
    Agentite_DistanceType type
);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_COMBAT_H */
