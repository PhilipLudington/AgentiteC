/*
 * Agentite Tactical Combat System
 *
 * Turn-based tactical combat with initiative ordering, telegraphing,
 * reaction mechanics, status effects, and grid-based positioning.
 *
 * Ported from AgentiteZ (Zig) combat system.
 */

#include "agentite/agentite.h"
#include "agentite/combat.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Agentite_CombatSystem {
    /* Combatants */
    Agentite_Combatant combatants[AGENTITE_COMBAT_MAX_COMBATANTS];
    int combatant_count;

    /* Turn order (sorted by initiative) */
    int turn_order[AGENTITE_COMBAT_MAX_COMBATANTS];
    int turn_order_count;
    int current_turn_index;

    /* Action queue */
    Agentite_CombatAction action_queue[AGENTITE_COMBAT_MAX_ACTIONS];
    int action_queue_count;

    /* Grid */
    int grid_width;
    int grid_height;
    Agentite_DistanceType distance_type;

    /* State */
    int turn_number;
    Agentite_CombatResult result;
    bool combat_started;

    /* Callbacks */
    Agentite_CombatEventCallback event_callback;
    void *event_userdata;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static void emit_event(Agentite_CombatSystem *combat, const Agentite_CombatEvent *event) {
    if (combat->event_callback) {
        combat->event_callback(combat, event, combat->event_userdata);
    }
}

static void sort_turn_order(Agentite_CombatSystem *combat) {
    /* Build list of alive combatants */
    combat->turn_order_count = 0;
    for (int i = 0; i < combat->combatant_count; i++) {
        if (combat->combatants[i].is_alive) {
            combat->turn_order[combat->turn_order_count++] = i;
        }
    }

    /* Sort by initiative (descending), player team wins ties */
    for (int i = 0; i < combat->turn_order_count - 1; i++) {
        for (int j = i + 1; j < combat->turn_order_count; j++) {
            Agentite_Combatant *a = &combat->combatants[combat->turn_order[i]];
            Agentite_Combatant *b = &combat->combatants[combat->turn_order[j]];

            bool swap = false;
            if (b->initiative > a->initiative) {
                swap = true;
            } else if (b->initiative == a->initiative && b->is_player_team && !a->is_player_team) {
                swap = true;
            }

            if (swap) {
                int temp = combat->turn_order[i];
                combat->turn_order[i] = combat->turn_order[j];
                combat->turn_order[j] = temp;
            }
        }
    }
}

static void check_combat_end(Agentite_CombatSystem *combat) {
    int player_alive = 0;
    int enemy_alive = 0;

    for (int i = 0; i < combat->combatant_count; i++) {
        if (combat->combatants[i].is_alive) {
            if (combat->combatants[i].is_player_team) {
                player_alive++;
            } else {
                enemy_alive++;
            }
        }
    }

    if (player_alive == 0 && enemy_alive == 0) {
        combat->result = AGENTITE_COMBAT_DRAW;
    } else if (player_alive == 0) {
        combat->result = AGENTITE_COMBAT_DEFEAT;
    } else if (enemy_alive == 0) {
        combat->result = AGENTITE_COMBAT_VICTORY;
    }
}

static float get_damage_multiplier(const Agentite_Combatant *target) {
    float mult = 1.0f;

    for (int i = 0; i < target->status_count; i++) {
        switch (target->status[i].type) {
            case AGENTITE_STATUS_VULNERABLE:
                mult *= 1.5f;
                break;
            case AGENTITE_STATUS_FORTIFIED:
                mult *= 0.75f;
                break;
            case AGENTITE_STATUS_INVULNERABLE:
                mult = 0.0f;
                break;
            default:
                break;
        }
    }

    return mult;
}

static bool has_status(const Agentite_Combatant *c, Agentite_StatusType type) {
    for (int i = 0; i < c->status_count; i++) {
        if (c->status[i].type == type) return true;
    }
    return false;
}

/*============================================================================
 * Combat System Creation
 *============================================================================*/

Agentite_CombatSystem *agentite_combat_create(int grid_width, int grid_height) {
    Agentite_CombatSystem *combat = AGENTITE_ALLOC(Agentite_CombatSystem);
    if (!combat) {
        agentite_set_error("Failed to allocate combat system");
        return NULL;
    }

    memset(combat, 0, sizeof(Agentite_CombatSystem));
    combat->grid_width = grid_width > 0 ? grid_width : 16;
    combat->grid_height = grid_height > 0 ? grid_height : 16;
    combat->distance_type = AGENTITE_DISTANCE_CHEBYSHEV;
    combat->result = AGENTITE_COMBAT_ONGOING;

    return combat;
}

void agentite_combat_destroy(Agentite_CombatSystem *combat) {
    if (combat) {
        free(combat);
    }
}

void agentite_combat_reset(Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR(combat);

    combat->combatant_count = 0;
    combat->turn_order_count = 0;
    combat->current_turn_index = 0;
    combat->action_queue_count = 0;
    combat->turn_number = 0;
    combat->result = AGENTITE_COMBAT_ONGOING;
    combat->combat_started = false;
}

/*============================================================================
 * Combatant Management
 *============================================================================*/

int agentite_combat_add_combatant(
    Agentite_CombatSystem *combat,
    const Agentite_Combatant *combatant,
    bool is_player)
{
    AGENTITE_VALIDATE_PTR_RET(combat, AGENTITE_COMBAT_INVALID_ID);
    AGENTITE_VALIDATE_PTR_RET(combatant, AGENTITE_COMBAT_INVALID_ID);

    if (combat->combatant_count >= AGENTITE_COMBAT_MAX_COMBATANTS) {
        agentite_set_error("Maximum combatants reached");
        return AGENTITE_COMBAT_INVALID_ID;
    }

    int id = combat->combatant_count;
    memcpy(&combat->combatants[id], combatant, sizeof(Agentite_Combatant));

    /* Ensure valid state */
    combat->combatants[id].is_alive = true;
    combat->combatants[id].is_player_team = is_player;
    combat->combatants[id].has_acted = false;
    combat->combatants[id].has_moved = false;
    combat->combatants[id].is_defending = false;

    if (combat->combatants[id].hp_max <= 0) {
        combat->combatants[id].hp_max = combat->combatants[id].hp;
    }
    if (combat->combatants[id].resource_max <= 0) {
        combat->combatants[id].resource_max = combat->combatants[id].resource;
    }

    combat->combatant_count++;
    return id;
}

Agentite_Combatant *agentite_combat_get_combatant(Agentite_CombatSystem *combat, int id) {
    AGENTITE_VALIDATE_PTR_RET(combat, NULL);

    if (id < 0 || id >= combat->combatant_count) {
        return NULL;
    }
    return &combat->combatants[id];
}

const Agentite_Combatant *agentite_combat_get_combatant_const(
    const Agentite_CombatSystem *combat,
    int id)
{
    AGENTITE_VALIDATE_PTR_RET(combat, NULL);

    if (id < 0 || id >= combat->combatant_count) {
        return NULL;
    }
    return &combat->combatants[id];
}

int agentite_combat_get_combatant_count(const Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    return combat->combatant_count;
}

int agentite_combat_get_team(
    const Agentite_CombatSystem *combat,
    bool is_player,
    int *out_ids,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    AGENTITE_VALIDATE_PTR_RET(out_ids, 0);

    int count = 0;
    for (int i = 0; i < combat->combatant_count && count < max_count; i++) {
        if (combat->combatants[i].is_player_team == is_player) {
            out_ids[count++] = i;
        }
    }
    return count;
}

/*============================================================================
 * Combat Flow
 *============================================================================*/

void agentite_combat_start(Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR(combat);

    combat->combat_started = true;
    combat->turn_number = 1;
    combat->current_turn_index = 0;
    combat->result = AGENTITE_COMBAT_ONGOING;

    /* Reset all combatants */
    for (int i = 0; i < combat->combatant_count; i++) {
        combat->combatants[i].has_acted = false;
        combat->combatants[i].has_moved = false;
        combat->combatants[i].is_defending = false;
    }

    sort_turn_order(combat);
}

bool agentite_combat_is_over(const Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR_RET(combat, true);
    return combat->result != AGENTITE_COMBAT_ONGOING;
}

Agentite_CombatResult agentite_combat_get_result(const Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR_RET(combat, AGENTITE_COMBAT_ONGOING);
    return combat->result;
}

int agentite_combat_get_turn(const Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    return combat->turn_number;
}

int agentite_combat_get_current_combatant(const Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR_RET(combat, AGENTITE_COMBAT_INVALID_ID);

    if (combat->turn_order_count == 0) {
        return AGENTITE_COMBAT_INVALID_ID;
    }

    return combat->turn_order[combat->current_turn_index];
}

int agentite_combat_get_turn_order(
    const Agentite_CombatSystem *combat,
    int *out_ids,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    AGENTITE_VALIDATE_PTR_RET(out_ids, 0);

    int count = combat->turn_order_count < max_count ? combat->turn_order_count : max_count;
    memcpy(out_ids, combat->turn_order, sizeof(int) * count);
    return count;
}

/*============================================================================
 * Actions
 *============================================================================*/

bool agentite_combat_queue_action(
    Agentite_CombatSystem *combat,
    const Agentite_CombatAction *action)
{
    AGENTITE_VALIDATE_PTR_RET(combat, false);
    AGENTITE_VALIDATE_PTR_RET(action, false);

    if (combat->action_queue_count >= AGENTITE_COMBAT_MAX_ACTIONS) {
        agentite_set_error("Action queue full");
        return false;
    }

    if (!agentite_combat_is_action_valid(combat, action)) {
        return false;
    }

    combat->action_queue[combat->action_queue_count++] = *action;
    return true;
}

static void execute_attack(
    Agentite_CombatSystem *combat,
    int attacker_id,
    int defender_id,
    const Agentite_Attack *attack)
{
    Agentite_Combatant *attacker = &combat->combatants[attacker_id];
    Agentite_Combatant *defender = &combat->combatants[defender_id];

    Agentite_CombatEvent event;
    memset(&event, 0, sizeof(event));
    event.action = AGENTITE_ACTION_ATTACK;
    event.actor_id = attacker_id;
    event.target_id = defender_id;

    /* Check for dodge */
    if (agentite_combat_can_dodge(combat, defender_id)) {
        float dodge = agentite_combat_get_dodge_chance(combat, defender_id);
        float roll = (float)rand() / (float)RAND_MAX;
        if (roll < dodge) {
            event.was_dodged = true;
            snprintf(event.description, sizeof(event.description),
                     "%s dodged %s's attack!", defender->name, attacker->name);
            emit_event(combat, &event);
            return;
        }
    }

    /* Check hit */
    float hit_roll = (float)rand() / (float)RAND_MAX;
    float hit_chance = attack->hit_chance;

    /* Blinded reduces hit chance */
    if (has_status(attacker, AGENTITE_STATUS_BLINDED)) {
        hit_chance *= 0.5f;
    }
    /* Concealed target is harder to hit */
    if (has_status(defender, AGENTITE_STATUS_CONCEALED)) {
        hit_chance *= 0.7f;
    }

    if (hit_roll > hit_chance) {
        snprintf(event.description, sizeof(event.description),
                 "%s missed %s!", attacker->name, defender->name);
        emit_event(combat, &event);
        return;
    }

    /* Calculate damage */
    int damage = attack->base_damage + attacker->attack_bonus;

    /* Apply armor (unless piercing) */
    if (!attack->piercing) {
        int armor = defender->armor;
        if (defender->is_defending) {
            armor += defender->defense_bonus;
        }
        event.damage_blocked = armor < damage ? armor : damage;
        damage -= event.damage_blocked;
    }

    /* Apply damage modifiers from status effects */
    float mult = get_damage_multiplier(defender);
    damage = (int)(damage * mult);

    if (damage < 0) damage = 0;

    /* Apply damage */
    event.damage_dealt = agentite_combat_apply_damage(combat, defender_id, damage);

    /* Apply status effect */
    if (attack->applies_status != AGENTITE_STATUS_NONE) {
        float status_roll = (float)rand() / (float)RAND_MAX;
        if (status_roll < attack->status_chance) {
            agentite_combat_apply_status(combat, defender_id, attack->applies_status,
                                         attack->status_duration, 1, attacker_id);
            event.status_applied = attack->applies_status;
        }
    }

    snprintf(event.description, sizeof(event.description),
             "%s attacks %s for %d damage!", attacker->name, defender->name, event.damage_dealt);

    emit_event(combat, &event);

    /* Check for counter-attack */
    if (defender->is_alive && agentite_combat_can_counter(combat, defender_id)) {
        /* Simple counter: 50% chance, half damage */
        float counter_roll = (float)rand() / (float)RAND_MAX;
        if (counter_roll < 0.5f) {
            int counter_damage = (attack->base_damage / 2) + defender->attack_bonus;
            counter_damage = agentite_combat_apply_damage(combat, attacker_id, counter_damage);

            Agentite_CombatEvent counter_event;
            memset(&counter_event, 0, sizeof(counter_event));
            counter_event.action = AGENTITE_ACTION_ATTACK;
            counter_event.actor_id = defender_id;
            counter_event.target_id = attacker_id;
            counter_event.damage_dealt = counter_damage;
            counter_event.was_countered = true;
            snprintf(counter_event.description, sizeof(counter_event.description),
                     "%s counter-attacks for %d damage!", defender->name, counter_damage);
            emit_event(combat, &counter_event);
        }
    }
}

bool agentite_combat_execute_turn(Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    if (combat->action_queue_count == 0) {
        return false;
    }

    /* Process all queued actions */
    for (int i = 0; i < combat->action_queue_count; i++) {
        Agentite_CombatAction *action = &combat->action_queue[i];
        Agentite_Combatant *actor = &combat->combatants[action->actor_id];

        if (!actor->is_alive) continue;

        /* Check for stun */
        if (has_status(actor, AGENTITE_STATUS_STUNNED)) {
            Agentite_CombatEvent event;
            memset(&event, 0, sizeof(event));
            event.action = action->type;
            event.actor_id = action->actor_id;
            snprintf(event.description, sizeof(event.description),
                     "%s is stunned and cannot act!", actor->name);
            emit_event(combat, &event);
            continue;
        }

        switch (action->type) {
            case AGENTITE_ACTION_MOVE: {
                /* Check for root */
                if (has_status(actor, AGENTITE_STATUS_ROOTED)) {
                    Agentite_CombatEvent event;
                    memset(&event, 0, sizeof(event));
                    event.action = AGENTITE_ACTION_MOVE;
                    event.actor_id = action->actor_id;
                    snprintf(event.description, sizeof(event.description),
                             "%s is rooted and cannot move!", actor->name);
                    emit_event(combat, &event);
                    break;
                }
                actor->position = action->target_pos;
                actor->has_moved = true;

                Agentite_CombatEvent event;
                memset(&event, 0, sizeof(event));
                event.action = AGENTITE_ACTION_MOVE;
                event.actor_id = action->actor_id;
                snprintf(event.description, sizeof(event.description),
                         "%s moves to (%d, %d)", actor->name,
                         action->target_pos.x, action->target_pos.y);
                emit_event(combat, &event);
                break;
            }

            case AGENTITE_ACTION_ATTACK: {
                if (action->target_id >= 0) {
                    /* Use basic attack or ability attack */
                    Agentite_Attack basic = agentite_attack_create("Attack",
                        10 + actor->attack_bonus, 1, 0.9f);
                    execute_attack(combat, action->actor_id, action->target_id, &basic);
                }
                actor->has_acted = true;
                break;
            }

            case AGENTITE_ACTION_DEFEND: {
                actor->is_defending = true;
                actor->has_acted = true;

                Agentite_CombatEvent event;
                memset(&event, 0, sizeof(event));
                event.action = AGENTITE_ACTION_DEFEND;
                event.actor_id = action->actor_id;
                snprintf(event.description, sizeof(event.description),
                         "%s takes a defensive stance", actor->name);
                emit_event(combat, &event);
                break;
            }

            case AGENTITE_ACTION_ABILITY: {
                if (action->ability_index >= 0 && action->ability_index < actor->ability_count) {
                    Agentite_Ability *ability = &actor->abilities[action->ability_index];

                    if (ability->cooldown_current > 0) break;
                    if (actor->resource < ability->resource_cost) break;

                    actor->resource -= ability->resource_cost;
                    ability->cooldown_current = ability->cooldown_max;

                    if (ability->is_offensive && action->target_id >= 0) {
                        execute_attack(combat, action->actor_id, action->target_id, &ability->attack);
                    } else if (ability->heal_amount > 0) {
                        int target = ability->targets_self ? action->actor_id : action->target_id;
                        if (target >= 0) {
                            int healed = agentite_combat_heal(combat, target, ability->heal_amount);
                            Agentite_CombatEvent event;
                            memset(&event, 0, sizeof(event));
                            event.action = AGENTITE_ACTION_ABILITY;
                            event.actor_id = action->actor_id;
                            event.target_id = target;
                            snprintf(event.description, sizeof(event.description),
                                     "%s uses %s, healing %d HP",
                                     actor->name, ability->name, healed);
                            emit_event(combat, &event);
                        }
                    }
                }
                actor->has_acted = true;
                break;
            }

            case AGENTITE_ACTION_WAIT: {
                /* Skip turn but keep reaction */
                Agentite_CombatEvent event;
                memset(&event, 0, sizeof(event));
                event.action = AGENTITE_ACTION_WAIT;
                event.actor_id = action->actor_id;
                snprintf(event.description, sizeof(event.description),
                         "%s waits", actor->name);
                emit_event(combat, &event);
                break;
            }

            case AGENTITE_ACTION_FLEE: {
                if (actor->is_player_team) {
                    /* 30% base flee chance */
                    float flee_roll = (float)rand() / (float)RAND_MAX;
                    if (flee_roll < 0.3f) {
                        combat->result = AGENTITE_COMBAT_FLED;
                        Agentite_CombatEvent event;
                        memset(&event, 0, sizeof(event));
                        event.action = AGENTITE_ACTION_FLEE;
                        event.actor_id = action->actor_id;
                        snprintf(event.description, sizeof(event.description),
                                 "%s fled from battle!", actor->name);
                        emit_event(combat, &event);
                    } else {
                        Agentite_CombatEvent event;
                        memset(&event, 0, sizeof(event));
                        event.action = AGENTITE_ACTION_FLEE;
                        event.actor_id = action->actor_id;
                        snprintf(event.description, sizeof(event.description),
                                 "%s failed to flee!", actor->name);
                        emit_event(combat, &event);
                    }
                }
                actor->has_acted = true;
                break;
            }

            default:
                break;
        }

        check_combat_end(combat);
        if (combat->result != AGENTITE_COMBAT_ONGOING) break;
    }

    combat->action_queue_count = 0;
    return true;
}

void agentite_combat_next_turn(Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR(combat);

    if (combat->result != AGENTITE_COMBAT_ONGOING) return;

    /* Process status effects for current combatant */
    int current_id = agentite_combat_get_current_combatant(combat);
    if (current_id >= 0) {
        agentite_combat_tick_status(combat, current_id);

        /* Reduce ability cooldowns */
        Agentite_Combatant *current = &combat->combatants[current_id];
        for (int i = 0; i < current->ability_count; i++) {
            if (current->abilities[i].cooldown_current > 0) {
                current->abilities[i].cooldown_current--;
            }
        }
    }

    /* Move to next combatant */
    combat->current_turn_index++;
    if (combat->current_turn_index >= combat->turn_order_count) {
        /* New round */
        combat->current_turn_index = 0;
        combat->turn_number++;

        /* Reset turn state for all combatants */
        for (int i = 0; i < combat->combatant_count; i++) {
            combat->combatants[i].has_acted = false;
            combat->combatants[i].has_moved = false;
            combat->combatants[i].is_defending = false;
        }

        /* Re-sort turn order (in case initiative changed) */
        sort_turn_order(combat);
    }

    /* Skip dead combatants */
    while (combat->current_turn_index < combat->turn_order_count) {
        int id = combat->turn_order[combat->current_turn_index];
        if (combat->combatants[id].is_alive) break;
        combat->current_turn_index++;
    }
}

bool agentite_combat_is_action_valid(
    const Agentite_CombatSystem *combat,
    const Agentite_CombatAction *action)
{
    AGENTITE_VALIDATE_PTR_RET(combat, false);
    AGENTITE_VALIDATE_PTR_RET(action, false);

    if (action->actor_id < 0 || action->actor_id >= combat->combatant_count) {
        return false;
    }

    const Agentite_Combatant *actor = &combat->combatants[action->actor_id];
    if (!actor->is_alive) return false;

    switch (action->type) {
        case AGENTITE_ACTION_MOVE:
            if (actor->has_moved) return false;
            if (!agentite_combat_is_position_valid(combat, action->target_pos)) return false;
            break;

        case AGENTITE_ACTION_ATTACK:
            if (actor->has_acted) return false;
            if (action->target_id < 0 || action->target_id >= combat->combatant_count) return false;
            if (!combat->combatants[action->target_id].is_alive) return false;
            break;

        case AGENTITE_ACTION_DEFEND:
        case AGENTITE_ACTION_WAIT:
            if (actor->has_acted) return false;
            break;

        case AGENTITE_ACTION_ABILITY:
            if (actor->has_acted) return false;
            if (action->ability_index < 0 || action->ability_index >= actor->ability_count) return false;
            break;

        case AGENTITE_ACTION_FLEE:
            if (actor->has_acted) return false;
            break;

        default:
            return false;
    }

    return true;
}

/*============================================================================
 * Telegraphing
 *============================================================================*/

int agentite_combat_get_telegraphs(
    const Agentite_CombatSystem *combat,
    Agentite_Telegraph *out_telegraphs,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    AGENTITE_VALIDATE_PTR_RET(out_telegraphs, 0);

    int count = 0;

    /* Generate telegraphs for all enemies */
    for (int i = 0; i < combat->combatant_count && count < max_count; i++) {
        const Agentite_Combatant *enemy = &combat->combatants[i];
        if (!enemy->is_alive || enemy->is_player_team) continue;

        /* Find best target (player with lowest HP) */
        int target_id = -1;
        int lowest_hp = INT32_MAX;

        for (int j = 0; j < combat->combatant_count; j++) {
            const Agentite_Combatant *target = &combat->combatants[j];
            if (!target->is_alive || !target->is_player_team) continue;
            if (target->hp < lowest_hp) {
                lowest_hp = target->hp;
                target_id = j;
            }
        }

        if (target_id < 0) continue;

        Agentite_Telegraph *t = &out_telegraphs[count++];
        t->attacker_id = i;
        t->target_id = target_id;
        t->action_type = AGENTITE_ACTION_ATTACK;
        t->ability_index = -1;
        t->predicted_damage = 10 + enemy->attack_bonus;
        t->hit_chance = 0.9f;
        t->status_applied = AGENTITE_STATUS_NONE;
    }

    return count;
}

void agentite_combat_generate_enemy_actions(Agentite_CombatSystem *combat) {
    AGENTITE_VALIDATE_PTR(combat);

    for (int i = 0; i < combat->combatant_count; i++) {
        Agentite_Combatant *enemy = &combat->combatants[i];
        if (!enemy->is_alive || enemy->is_player_team) continue;
        if (enemy->has_acted) continue;

        /* Simple AI: attack nearest player */
        int target_id = -1;
        int min_dist = INT32_MAX;

        for (int j = 0; j < combat->combatant_count; j++) {
            const Agentite_Combatant *target = &combat->combatants[j];
            if (!target->is_alive || !target->is_player_team) continue;

            int dist = agentite_combat_distance(enemy->position, target->position,
                                                combat->distance_type);
            if (dist < min_dist) {
                min_dist = dist;
                target_id = j;
            }
        }

        if (target_id < 0) continue;

        Agentite_CombatAction action;
        memset(&action, 0, sizeof(action));
        action.actor_id = i;
        action.type = AGENTITE_ACTION_ATTACK;
        action.target_id = target_id;

        agentite_combat_queue_action(combat, &action);
    }
}

/*============================================================================
 * Reactions
 *============================================================================*/

bool agentite_combat_can_dodge(const Agentite_CombatSystem *combat, int id) {
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    if (id < 0 || id >= combat->combatant_count) return false;
    const Agentite_Combatant *c = &combat->combatants[id];

    if (!c->is_alive) return false;
    if (c->has_acted) return false;  /* Already committed */
    if (c->dodge_chance <= 0.0f) return false;
    if (has_status(c, AGENTITE_STATUS_STUNNED)) return false;
    if (has_status(c, AGENTITE_STATUS_ROOTED)) return false;

    return true;
}

bool agentite_combat_can_counter(const Agentite_CombatSystem *combat, int id) {
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    if (id < 0 || id >= combat->combatant_count) return false;
    const Agentite_Combatant *c = &combat->combatants[id];

    if (!c->is_alive) return false;
    if (c->has_acted) return false;  /* Already committed */
    if (has_status(c, AGENTITE_STATUS_STUNNED)) return false;

    return true;
}

float agentite_combat_get_dodge_chance(const Agentite_CombatSystem *combat, int id) {
    AGENTITE_VALIDATE_PTR_RET(combat, 0.0f);

    if (id < 0 || id >= combat->combatant_count) return 0.0f;
    const Agentite_Combatant *c = &combat->combatants[id];

    float dodge = c->dodge_chance;

    /* Defending increases dodge */
    if (c->is_defending) {
        dodge += 0.2f;
    }

    /* Haste increases dodge */
    if (has_status(c, AGENTITE_STATUS_HASTED)) {
        dodge += 0.1f;
    }

    /* Slowed decreases dodge */
    if (has_status(c, AGENTITE_STATUS_SLOWED)) {
        dodge -= 0.1f;
    }

    if (dodge < 0.0f) dodge = 0.0f;
    if (dodge > 0.9f) dodge = 0.9f;  /* Cap at 90% */

    return dodge;
}

/*============================================================================
 * Damage
 *============================================================================*/

int agentite_combat_calculate_damage(
    const Agentite_CombatSystem *combat,
    int attacker,
    int defender,
    const Agentite_Attack *attack)
{
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    AGENTITE_VALIDATE_PTR_RET(attack, 0);

    if (attacker < 0 || attacker >= combat->combatant_count) return 0;
    if (defender < 0 || defender >= combat->combatant_count) return 0;

    const Agentite_Combatant *atk = &combat->combatants[attacker];
    const Agentite_Combatant *def = &combat->combatants[defender];

    int damage = attack->base_damage + atk->attack_bonus;

    /* Armor reduction */
    if (!attack->piercing) {
        int armor = def->armor;
        if (def->is_defending) {
            armor += def->defense_bonus;
        }
        damage -= armor;
    }

    /* Status modifiers */
    float mult = get_damage_multiplier(def);
    damage = (int)(damage * mult);

    return damage > 0 ? damage : 0;
}

int agentite_combat_apply_damage(Agentite_CombatSystem *combat, int id, int damage) {
    AGENTITE_VALIDATE_PTR_RET(combat, 0);

    if (id < 0 || id >= combat->combatant_count) return 0;
    Agentite_Combatant *c = &combat->combatants[id];

    if (!c->is_alive || damage <= 0) return 0;

    int actual = 0;

    /* Absorb with temp HP first */
    if (c->temp_hp > 0) {
        int absorbed = c->temp_hp < damage ? c->temp_hp : damage;
        c->temp_hp -= absorbed;
        damage -= absorbed;
    }

    /* Apply remaining to HP */
    actual = c->hp < damage ? c->hp : damage;
    c->hp -= actual;

    if (c->hp <= 0) {
        c->hp = 0;
        c->is_alive = false;
    }

    return actual;
}

int agentite_combat_heal(Agentite_CombatSystem *combat, int id, int amount) {
    AGENTITE_VALIDATE_PTR_RET(combat, 0);

    if (id < 0 || id >= combat->combatant_count) return 0;
    Agentite_Combatant *c = &combat->combatants[id];

    if (!c->is_alive || amount <= 0) return 0;

    int missing = c->hp_max - c->hp;
    int healed = missing < amount ? missing : amount;
    c->hp += healed;

    return healed;
}

/*============================================================================
 * Status Effects
 *============================================================================*/

bool agentite_combat_apply_status(
    Agentite_CombatSystem *combat,
    int id,
    Agentite_StatusType type,
    int duration,
    int stacks,
    int source)
{
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    if (id < 0 || id >= combat->combatant_count) return false;
    if (type == AGENTITE_STATUS_NONE) return false;

    Agentite_Combatant *c = &combat->combatants[id];

    /* Check if already has this status */
    for (int i = 0; i < c->status_count; i++) {
        if (c->status[i].type == type) {
            /* Refresh duration and add stacks */
            c->status[i].duration = duration;
            c->status[i].stacks += stacks;
            c->status[i].source_id = source;
            return true;
        }
    }

    /* Add new status */
    if (c->status_count >= AGENTITE_COMBAT_MAX_STATUS) {
        return false;
    }

    Agentite_StatusEffect *s = &c->status[c->status_count++];
    s->type = type;
    s->duration = duration;
    s->stacks = stacks;
    s->source_id = source;

    /* Set DoT damage for appropriate types */
    switch (type) {
        case AGENTITE_STATUS_BURNING:
            s->damage_per_tick = 5.0f * stacks;
            break;
        case AGENTITE_STATUS_POISONED:
            s->damage_per_tick = 3.0f * stacks;
            break;
        case AGENTITE_STATUS_BLEEDING:
            s->damage_per_tick = 4.0f * stacks;
            break;
        default:
            s->damage_per_tick = 0.0f;
            break;
    }

    return true;
}

bool agentite_combat_remove_status(
    Agentite_CombatSystem *combat,
    int id,
    Agentite_StatusType type)
{
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    if (id < 0 || id >= combat->combatant_count) return false;
    Agentite_Combatant *c = &combat->combatants[id];

    for (int i = 0; i < c->status_count; i++) {
        if (c->status[i].type == type) {
            /* Remove by shifting */
            for (int j = i; j < c->status_count - 1; j++) {
                c->status[j] = c->status[j + 1];
            }
            c->status_count--;
            return true;
        }
    }

    return false;
}

bool agentite_combat_has_status(
    const Agentite_CombatSystem *combat,
    int id,
    Agentite_StatusType type)
{
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    if (id < 0 || id >= combat->combatant_count) return false;
    return has_status(&combat->combatants[id], type);
}

void agentite_combat_tick_status(Agentite_CombatSystem *combat, int id) {
    AGENTITE_VALIDATE_PTR(combat);

    if (id < 0 || id >= combat->combatant_count) return;
    Agentite_Combatant *c = &combat->combatants[id];

    for (int i = c->status_count - 1; i >= 0; i--) {
        Agentite_StatusEffect *s = &c->status[i];

        /* Apply DoT damage */
        if (s->damage_per_tick > 0.0f) {
            int dot_damage = (int)s->damage_per_tick;
            agentite_combat_apply_damage(combat, id, dot_damage);

            Agentite_CombatEvent event;
            memset(&event, 0, sizeof(event));
            event.actor_id = s->source_id;
            event.target_id = id;
            event.damage_dealt = dot_damage;
            snprintf(event.description, sizeof(event.description),
                     "%s takes %d %s damage", c->name, dot_damage,
                     agentite_status_name(s->type));
            emit_event(combat, &event);
        }

        /* Reduce duration */
        if (s->duration > 0) {
            s->duration--;
            if (s->duration == 0) {
                /* Remove expired status */
                for (int j = i; j < c->status_count - 1; j++) {
                    c->status[j] = c->status[j + 1];
                }
                c->status_count--;
            }
        }
    }
}

/*============================================================================
 * Grid and Movement
 *============================================================================*/

int agentite_combat_distance(
    Agentite_GridPos from,
    Agentite_GridPos to,
    Agentite_DistanceType type)
{
    int dx = abs(to.x - from.x);
    int dy = abs(to.y - from.y);

    switch (type) {
        case AGENTITE_DISTANCE_CHEBYSHEV:
            return dx > dy ? dx : dy;
        case AGENTITE_DISTANCE_MANHATTAN:
            return dx + dy;
        case AGENTITE_DISTANCE_EUCLIDEAN:
            return (int)sqrtf((float)(dx * dx + dy * dy));
        default:
            return dx > dy ? dx : dy;
    }
}

bool agentite_combat_is_position_valid(
    const Agentite_CombatSystem *combat,
    Agentite_GridPos pos)
{
    AGENTITE_VALIDATE_PTR_RET(combat, false);

    return pos.x >= 0 && pos.x < combat->grid_width &&
           pos.y >= 0 && pos.y < combat->grid_height;
}

int agentite_combat_get_combatant_at(
    const Agentite_CombatSystem *combat,
    Agentite_GridPos pos)
{
    AGENTITE_VALIDATE_PTR_RET(combat, -1);

    for (int i = 0; i < combat->combatant_count; i++) {
        const Agentite_Combatant *c = &combat->combatants[i];
        if (c->is_alive && c->position.x == pos.x && c->position.y == pos.y) {
            return i;
        }
    }
    return -1;
}

int agentite_combat_get_valid_moves(
    const Agentite_CombatSystem *combat,
    int id,
    Agentite_GridPos *out_pos,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    AGENTITE_VALIDATE_PTR_RET(out_pos, 0);

    if (id < 0 || id >= combat->combatant_count) return 0;
    const Agentite_Combatant *c = &combat->combatants[id];

    int count = 0;
    int range = c->movement_range > 0 ? c->movement_range : 3;

    for (int dx = -range; dx <= range && count < max_count; dx++) {
        for (int dy = -range; dy <= range && count < max_count; dy++) {
            if (dx == 0 && dy == 0) continue;

            Agentite_GridPos pos = {c->position.x + dx, c->position.y + dy};
            int dist = agentite_combat_distance(c->position, pos, combat->distance_type);

            if (dist <= range &&
                agentite_combat_is_position_valid(combat, pos) &&
                agentite_combat_get_combatant_at(combat, pos) < 0) {
                out_pos[count++] = pos;
            }
        }
    }

    return count;
}

int agentite_combat_get_valid_targets(
    const Agentite_CombatSystem *combat,
    int attacker,
    const Agentite_Attack *attack,
    int *out_ids,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(combat, 0);
    AGENTITE_VALIDATE_PTR_RET(attack, 0);
    AGENTITE_VALIDATE_PTR_RET(out_ids, 0);

    if (attacker < 0 || attacker >= combat->combatant_count) return 0;
    const Agentite_Combatant *atk = &combat->combatants[attacker];

    int count = 0;

    for (int i = 0; i < combat->combatant_count && count < max_count; i++) {
        if (i == attacker) continue;

        const Agentite_Combatant *target = &combat->combatants[i];
        if (!target->is_alive) continue;
        if (target->is_player_team == atk->is_player_team) continue;  /* No friendly fire */

        int dist = agentite_combat_distance(atk->position, target->position, combat->distance_type);
        if (dist <= attack->range) {
            out_ids[count++] = i;
        }
    }

    return count;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void agentite_combat_set_event_callback(
    Agentite_CombatSystem *combat,
    Agentite_CombatEventCallback callback,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(combat);
    combat->event_callback = callback;
    combat->event_userdata = userdata;
}

/*============================================================================
 * Utility
 *============================================================================*/

const char *agentite_status_name(Agentite_StatusType type) {
    switch (type) {
        case AGENTITE_STATUS_NONE:        return "None";
        case AGENTITE_STATUS_STUNNED:     return "Stunned";
        case AGENTITE_STATUS_BURNING:     return "Burning";
        case AGENTITE_STATUS_POISONED:    return "Poisoned";
        case AGENTITE_STATUS_BLEEDING:    return "Bleeding";
        case AGENTITE_STATUS_ROOTED:      return "Rooted";
        case AGENTITE_STATUS_BLINDED:     return "Blinded";
        case AGENTITE_STATUS_VULNERABLE:  return "Vulnerable";
        case AGENTITE_STATUS_FORTIFIED:   return "Fortified";
        case AGENTITE_STATUS_HASTED:      return "Hasted";
        case AGENTITE_STATUS_SLOWED:      return "Slowed";
        case AGENTITE_STATUS_INVULNERABLE: return "Invulnerable";
        case AGENTITE_STATUS_CONCEALED:   return "Concealed";
        case AGENTITE_STATUS_INJURED:     return "Injured";
        default:                          return "Unknown";
    }
}

const char *agentite_action_name(Agentite_ActionType type) {
    switch (type) {
        case AGENTITE_ACTION_NONE:     return "None";
        case AGENTITE_ACTION_MOVE:     return "Move";
        case AGENTITE_ACTION_ATTACK:   return "Attack";
        case AGENTITE_ACTION_DEFEND:   return "Defend";
        case AGENTITE_ACTION_USE_ITEM: return "Use Item";
        case AGENTITE_ACTION_ABILITY:  return "Ability";
        case AGENTITE_ACTION_WAIT:     return "Wait";
        case AGENTITE_ACTION_FLEE:     return "Flee";
        default:                       return "Unknown";
    }
}

Agentite_Attack agentite_attack_create(
    const char *name,
    int damage,
    int range,
    float hit_chance)
{
    Agentite_Attack attack;
    memset(&attack, 0, sizeof(attack));
    if (name) {
        strncpy(attack.name, name, sizeof(attack.name) - 1);
    }
    attack.base_damage = damage;
    attack.range = range;
    attack.hit_chance = hit_chance;
    attack.piercing = false;
    attack.aoe_radius = 0;
    attack.applies_status = AGENTITE_STATUS_NONE;
    attack.status_chance = 0.0f;
    return attack;
}

void agentite_combat_set_grid_size(
    Agentite_CombatSystem *combat,
    int width,
    int height)
{
    AGENTITE_VALIDATE_PTR(combat);
    combat->grid_width = width > 0 ? width : 16;
    combat->grid_height = height > 0 ? height : 16;
}

void agentite_combat_set_distance_type(
    Agentite_CombatSystem *combat,
    Agentite_DistanceType type)
{
    AGENTITE_VALIDATE_PTR(combat);
    combat->distance_type = type;
}
