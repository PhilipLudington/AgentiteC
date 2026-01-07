/*
 * Agentite Fleet/Army Management System
 *
 * Strategic unit management for 4X and strategy games with automated
 * battle resolution, unit counters, morale, commanders, and experience.
 *
 * Ported from AgentiteZ (Zig) fleet system.
 */

#include "agentite/agentite.h"
#include "agentite/fleet.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Unit Stats Database
 *============================================================================*/

static const Agentite_UnitStats g_unit_stats[AGENTITE_UNIT_CLASS_COUNT] = {
    /* Space units */
    [AGENTITE_UNIT_FIGHTER] = {
        .unit_class = AGENTITE_UNIT_FIGHTER,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Fighter",
        .attack = 8, .defense = 3, .hp = 20, .speed = 10, .range = 1,
        .cost = 50, .upkeep = 2,
        .strong_against = {AGENTITE_UNIT_BOMBER, AGENTITE_UNIT_CORVETTE},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_FRIGATE, AGENTITE_UNIT_CRUISER},
        .weak_count = 2
    },
    [AGENTITE_UNIT_BOMBER] = {
        .unit_class = AGENTITE_UNIT_BOMBER,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Bomber",
        .attack = 15, .defense = 2, .hp = 25, .speed = 6, .range = 2,
        .cost = 80, .upkeep = 4,
        .strong_against = {AGENTITE_UNIT_BATTLESHIP, AGENTITE_UNIT_CRUISER, AGENTITE_UNIT_DREADNOUGHT},
        .strong_count = 3,
        .weak_against = {AGENTITE_UNIT_FIGHTER, AGENTITE_UNIT_FRIGATE},
        .weak_count = 2
    },
    [AGENTITE_UNIT_CORVETTE] = {
        .unit_class = AGENTITE_UNIT_CORVETTE,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Corvette",
        .attack = 10, .defense = 5, .hp = 40, .speed = 8, .range = 2,
        .cost = 100, .upkeep = 5,
        .strong_against = {AGENTITE_UNIT_FRIGATE},
        .strong_count = 1,
        .weak_against = {AGENTITE_UNIT_DESTROYER},
        .weak_count = 1
    },
    [AGENTITE_UNIT_FRIGATE] = {
        .unit_class = AGENTITE_UNIT_FRIGATE,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Frigate",
        .attack = 12, .defense = 6, .hp = 50, .speed = 7, .range = 3,
        .cost = 150, .upkeep = 7,
        .strong_against = {AGENTITE_UNIT_FIGHTER, AGENTITE_UNIT_BOMBER},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_DESTROYER, AGENTITE_UNIT_CRUISER},
        .weak_count = 2
    },
    [AGENTITE_UNIT_DESTROYER] = {
        .unit_class = AGENTITE_UNIT_DESTROYER,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Destroyer",
        .attack = 18, .defense = 10, .hp = 80, .speed = 6, .range = 3,
        .cost = 250, .upkeep = 12,
        .strong_against = {AGENTITE_UNIT_CORVETTE, AGENTITE_UNIT_FRIGATE},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_BATTLESHIP},
        .weak_count = 1
    },
    [AGENTITE_UNIT_CRUISER] = {
        .unit_class = AGENTITE_UNIT_CRUISER,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Cruiser",
        .attack = 25, .defense = 15, .hp = 120, .speed = 5, .range = 4,
        .cost = 400, .upkeep = 20,
        .strong_against = {AGENTITE_UNIT_DESTROYER, AGENTITE_UNIT_FRIGATE},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_BOMBER, AGENTITE_UNIT_BATTLESHIP},
        .weak_count = 2
    },
    [AGENTITE_UNIT_BATTLESHIP] = {
        .unit_class = AGENTITE_UNIT_BATTLESHIP,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Battleship",
        .attack = 40, .defense = 25, .hp = 200, .speed = 3, .range = 5,
        .cost = 800, .upkeep = 40,
        .strong_against = {AGENTITE_UNIT_CRUISER, AGENTITE_UNIT_DESTROYER},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_BOMBER},
        .weak_count = 1
    },
    [AGENTITE_UNIT_CARRIER] = {
        .unit_class = AGENTITE_UNIT_CARRIER,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Carrier",
        .attack = 15, .defense = 20, .hp = 180, .speed = 4, .range = 6,
        .cost = 600, .upkeep = 30,
        .strong_against = {},
        .strong_count = 0,
        .weak_against = {AGENTITE_UNIT_BOMBER, AGENTITE_UNIT_BATTLESHIP},
        .weak_count = 2
    },
    [AGENTITE_UNIT_DREADNOUGHT] = {
        .unit_class = AGENTITE_UNIT_DREADNOUGHT,
        .domain = AGENTITE_DOMAIN_SPACE,
        .name = "Dreadnought",
        .attack = 60, .defense = 40, .hp = 350, .speed = 2, .range = 6,
        .cost = 1500, .upkeep = 75,
        .strong_against = {AGENTITE_UNIT_BATTLESHIP, AGENTITE_UNIT_CRUISER},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_BOMBER},
        .weak_count = 1
    },

    /* Ground units */
    [AGENTITE_UNIT_INFANTRY] = {
        .unit_class = AGENTITE_UNIT_INFANTRY,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Infantry",
        .attack = 5, .defense = 5, .hp = 30, .speed = 2, .range = 1,
        .cost = 30, .upkeep = 1,
        .strong_against = {AGENTITE_UNIT_SPECIAL_OPS},
        .strong_count = 1,
        .weak_against = {AGENTITE_UNIT_ARMOR, AGENTITE_UNIT_MECH},
        .weak_count = 2
    },
    [AGENTITE_UNIT_ARMOR] = {
        .unit_class = AGENTITE_UNIT_ARMOR,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Armor",
        .attack = 20, .defense = 15, .hp = 100, .speed = 4, .range = 2,
        .cost = 200, .upkeep = 10,
        .strong_against = {AGENTITE_UNIT_INFANTRY, AGENTITE_UNIT_ARTILLERY},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_MECH, AGENTITE_UNIT_ANTI_AIR},
        .weak_count = 2
    },
    [AGENTITE_UNIT_ARTILLERY] = {
        .unit_class = AGENTITE_UNIT_ARTILLERY,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Artillery",
        .attack = 25, .defense = 5, .hp = 50, .speed = 2, .range = 5,
        .cost = 180, .upkeep = 9,
        .strong_against = {AGENTITE_UNIT_INFANTRY, AGENTITE_UNIT_MECH},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_ARMOR, AGENTITE_UNIT_SPECIAL_OPS},
        .weak_count = 2
    },
    [AGENTITE_UNIT_MECH] = {
        .unit_class = AGENTITE_UNIT_MECH,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Mech",
        .attack = 35, .defense = 20, .hp = 150, .speed = 3, .range = 2,
        .cost = 350, .upkeep = 18,
        .strong_against = {AGENTITE_UNIT_ARMOR, AGENTITE_UNIT_INFANTRY},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_ARTILLERY},
        .weak_count = 1
    },
    [AGENTITE_UNIT_SPECIAL_OPS] = {
        .unit_class = AGENTITE_UNIT_SPECIAL_OPS,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Special Ops",
        .attack = 15, .defense = 8, .hp = 40, .speed = 5, .range = 2,
        .cost = 120, .upkeep = 6,
        .strong_against = {AGENTITE_UNIT_ARTILLERY, AGENTITE_UNIT_ENGINEER},
        .strong_count = 2,
        .weak_against = {AGENTITE_UNIT_INFANTRY},
        .weak_count = 1
    },
    [AGENTITE_UNIT_ANTI_AIR] = {
        .unit_class = AGENTITE_UNIT_ANTI_AIR,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Anti-Air",
        .attack = 18, .defense = 8, .hp = 60, .speed = 3, .range = 4,
        .cost = 150, .upkeep = 8,
        .strong_against = {AGENTITE_UNIT_DROPSHIP},
        .strong_count = 1,
        .weak_against = {AGENTITE_UNIT_ARMOR},
        .weak_count = 1
    },
    [AGENTITE_UNIT_ENGINEER] = {
        .unit_class = AGENTITE_UNIT_ENGINEER,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Engineer",
        .attack = 3, .defense = 3, .hp = 25, .speed = 2, .range = 1,
        .cost = 50, .upkeep = 3,
        .strong_against = {},
        .strong_count = 0,
        .weak_against = {AGENTITE_UNIT_SPECIAL_OPS},
        .weak_count = 1
    },
    [AGENTITE_UNIT_TRANSPORT] = {
        .unit_class = AGENTITE_UNIT_TRANSPORT,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Transport",
        .attack = 2, .defense = 5, .hp = 40, .speed = 5, .range = 0,
        .cost = 60, .upkeep = 3,
        .strong_against = {},
        .strong_count = 0,
        .weak_against = {AGENTITE_UNIT_ARMOR, AGENTITE_UNIT_ARTILLERY},
        .weak_count = 2
    },
    [AGENTITE_UNIT_DROPSHIP] = {
        .unit_class = AGENTITE_UNIT_DROPSHIP,
        .domain = AGENTITE_DOMAIN_GROUND,
        .name = "Dropship",
        .attack = 5, .defense = 8, .hp = 80, .speed = 8, .range = 1,
        .cost = 200, .upkeep = 10,
        .strong_against = {},
        .strong_count = 0,
        .weak_against = {AGENTITE_UNIT_ANTI_AIR},
        .weak_count = 1
    },
};

/*============================================================================
 * Internal Structures
 *============================================================================*/

struct Agentite_FleetManager {
    Agentite_Fleet fleets[AGENTITE_FLEET_MAX];
    bool fleet_active[AGENTITE_FLEET_MAX];
    int fleet_count;

    /* Callbacks */
    Agentite_BattleCallback battle_callback;
    void *battle_userdata;
};

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static int find_unit_group(Agentite_Fleet *fleet, Agentite_UnitClass unit_class) {
    for (int i = 0; i < fleet->unit_count; i++) {
        if (fleet->units[i].unit_class == unit_class) {
            return i;
        }
    }
    return -1;
}

static int calculate_fleet_attack(const Agentite_Fleet *fleet) {
    int total = 0;
    for (int i = 0; i < fleet->unit_count; i++) {
        const Agentite_UnitGroup *group = &fleet->units[i];
        const Agentite_UnitStats *stats = &g_unit_stats[group->unit_class];
        float xp_mult = agentite_unit_xp_bonus(group);
        total += (int)(stats->attack * group->count * xp_mult);
    }

    /* Apply commander bonus */
    if (fleet->has_commander) {
        total = (int)(total * (1.0f + fleet->commander.attack_bonus / 100.0f));
    }

    /* Apply morale modifier */
    float morale_mult = 0.5f + (fleet->morale / 200.0f);
    total = (int)(total * morale_mult);

    return total;
}

static int calculate_fleet_defense(const Agentite_Fleet *fleet) {
    int total = 0;
    for (int i = 0; i < fleet->unit_count; i++) {
        const Agentite_UnitGroup *group = &fleet->units[i];
        const Agentite_UnitStats *stats = &g_unit_stats[group->unit_class];
        total += stats->defense * group->count;
    }

    if (fleet->has_commander) {
        total = (int)(total * (1.0f + fleet->commander.defense_bonus / 100.0f));
    }

    return total;
}

static int get_total_units(const Agentite_Fleet *fleet) {
    int total = 0;
    for (int i = 0; i < fleet->unit_count; i++) {
        total += fleet->units[i].count;
    }
    return total;
}

static void apply_casualties(Agentite_Fleet *fleet, int casualties) {
    /* Distribute casualties proportionally among unit groups */
    int total = get_total_units(fleet);
    if (total == 0) return;

    int remaining = casualties;
    for (int i = 0; i < fleet->unit_count && remaining > 0; i++) {
        Agentite_UnitGroup *group = &fleet->units[i];
        int group_casualties = (int)(casualties * ((float)group->count / total));
        if (group_casualties > group->count) {
            group_casualties = group->count;
        }
        group->count -= group_casualties;
        remaining -= group_casualties;
    }

    /* Apply any remaining casualties to first available group */
    for (int i = 0; i < fleet->unit_count && remaining > 0; i++) {
        Agentite_UnitGroup *group = &fleet->units[i];
        int take = remaining < group->count ? remaining : group->count;
        group->count -= take;
        remaining -= take;
    }

    /* Remove empty groups */
    for (int i = fleet->unit_count - 1; i >= 0; i--) {
        if (fleet->units[i].count <= 0) {
            for (int j = i; j < fleet->unit_count - 1; j++) {
                fleet->units[j] = fleet->units[j + 1];
            }
            fleet->unit_count--;
        }
    }
}

/*============================================================================
 * Fleet Manager Creation
 *============================================================================*/

Agentite_FleetManager *agentite_fleet_create(void) {
    Agentite_FleetManager *fm = AGENTITE_ALLOC(Agentite_FleetManager);
    if (!fm) {
        agentite_set_error("Failed to allocate fleet manager");
        return NULL;
    }

    memset(fm, 0, sizeof(Agentite_FleetManager));
    return fm;
}

void agentite_fleet_destroy(Agentite_FleetManager *fm) {
    if (fm) {
        free(fm);
    }
}

/*============================================================================
 * Fleet Management
 *============================================================================*/

int agentite_fleet_add(Agentite_FleetManager *fm, const Agentite_Fleet *fleet) {
    AGENTITE_VALIDATE_PTR_RET(fm, AGENTITE_FLEET_INVALID_ID);
    AGENTITE_VALIDATE_PTR_RET(fleet, AGENTITE_FLEET_INVALID_ID);

    /* Find free slot */
    int id = -1;
    for (int i = 0; i < AGENTITE_FLEET_MAX; i++) {
        if (!fm->fleet_active[i]) {
            id = i;
            break;
        }
    }

    if (id < 0) {
        agentite_set_error("Fleet: Maximum fleets reached (%d/%d)", fm->fleet_count, AGENTITE_FLEET_MAX);
        return AGENTITE_FLEET_INVALID_ID;
    }

    memcpy(&fm->fleets[id], fleet, sizeof(Agentite_Fleet));
    fm->fleet_active[id] = true;
    fm->fleet_count++;

    /* Initialize defaults */
    if (fm->fleets[id].morale == 0) {
        fm->fleets[id].morale = 100;
    }

    return id;
}

bool agentite_fleet_remove(Agentite_FleetManager *fm, int id) {
    AGENTITE_VALIDATE_PTR_RET(fm, false);

    if (id < 0 || id >= AGENTITE_FLEET_MAX || !fm->fleet_active[id]) {
        return false;
    }

    fm->fleet_active[id] = false;
    fm->fleet_count--;
    return true;
}

Agentite_Fleet *agentite_fleet_get(Agentite_FleetManager *fm, int id) {
    AGENTITE_VALIDATE_PTR_RET(fm, NULL);

    if (id < 0 || id >= AGENTITE_FLEET_MAX || !fm->fleet_active[id]) {
        return NULL;
    }
    return &fm->fleets[id];
}

const Agentite_Fleet *agentite_fleet_get_const(const Agentite_FleetManager *fm, int id) {
    AGENTITE_VALIDATE_PTR_RET(fm, NULL);

    if (id < 0 || id >= AGENTITE_FLEET_MAX || !fm->fleet_active[id]) {
        return NULL;
    }
    return &fm->fleets[id];
}

int agentite_fleet_count(const Agentite_FleetManager *fm) {
    AGENTITE_VALIDATE_PTR_RET(fm, 0);
    return fm->fleet_count;
}

int agentite_fleet_get_by_owner(
    const Agentite_FleetManager *fm,
    int32_t owner_id,
    int *out_ids,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(fm, 0);
    AGENTITE_VALIDATE_PTR_RET(out_ids, 0);

    int count = 0;
    for (int i = 0; i < AGENTITE_FLEET_MAX && count < max_count; i++) {
        if (fm->fleet_active[i] && fm->fleets[i].owner_id == owner_id) {
            out_ids[count++] = i;
        }
    }
    return count;
}

/*============================================================================
 * Unit Management
 *============================================================================*/

bool agentite_fleet_add_units(
    Agentite_FleetManager *fm,
    int fleet_id,
    Agentite_UnitClass unit_class,
    int count)
{
    AGENTITE_VALIDATE_PTR_RET(fm, false);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet || count <= 0) return false;

    /* Check domain compatibility */
    const Agentite_UnitStats *stats = &g_unit_stats[unit_class];
    bool is_space = (stats->domain == AGENTITE_DOMAIN_SPACE);
    if (fleet->unit_count > 0 && fleet->is_space_fleet != is_space) {
        agentite_set_error("Fleet: Cannot mix space and ground units (fleet is %s)", fleet->is_space_fleet ? "space" : "ground");
        return false;
    }

    int group_idx = find_unit_group(fleet, unit_class);
    if (group_idx >= 0) {
        fleet->units[group_idx].count += count;
    } else {
        if (fleet->unit_count >= AGENTITE_FLEET_MAX_UNIT_GROUPS) {
            agentite_set_error("Fleet: Maximum unit groups reached (%d/%d)", fleet->unit_count, AGENTITE_FLEET_MAX_UNIT_GROUPS);
            return false;
        }

        Agentite_UnitGroup *group = &fleet->units[fleet->unit_count++];
        group->unit_class = unit_class;
        group->count = count;
        group->health = stats->hp;
        group->experience = 0;
        group->kills = 0;

        fleet->is_space_fleet = is_space;
    }

    return true;
}

int agentite_fleet_remove_units(
    Agentite_FleetManager *fm,
    int fleet_id,
    Agentite_UnitClass unit_class,
    int count)
{
    AGENTITE_VALIDATE_PTR_RET(fm, 0);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet || count <= 0) return 0;

    int group_idx = find_unit_group(fleet, unit_class);
    if (group_idx < 0) return 0;

    Agentite_UnitGroup *group = &fleet->units[group_idx];
    int removed = count < group->count ? count : group->count;
    group->count -= removed;

    /* Remove empty group */
    if (group->count <= 0) {
        for (int i = group_idx; i < fleet->unit_count - 1; i++) {
            fleet->units[i] = fleet->units[i + 1];
        }
        fleet->unit_count--;
    }

    return removed;
}

int agentite_fleet_get_unit_count(
    const Agentite_FleetManager *fm,
    int fleet_id,
    int unit_class)
{
    AGENTITE_VALIDATE_PTR_RET(fm, 0);

    const Agentite_Fleet *fleet = agentite_fleet_get_const(fm, fleet_id);
    if (!fleet) return 0;

    if (unit_class < 0) {
        return get_total_units(fleet);
    }

    for (int i = 0; i < fleet->unit_count; i++) {
        if (fleet->units[i].unit_class == (Agentite_UnitClass)unit_class) {
            return fleet->units[i].count;
        }
    }
    return 0;
}

int agentite_fleet_get_strength(const Agentite_FleetManager *fm, int fleet_id) {
    AGENTITE_VALIDATE_PTR_RET(fm, 0);

    const Agentite_Fleet *fleet = agentite_fleet_get_const(fm, fleet_id);
    if (!fleet) return 0;

    return calculate_fleet_attack(fleet) + calculate_fleet_defense(fleet);
}

const Agentite_UnitStats *agentite_unit_get_stats(Agentite_UnitClass unit_class) {
    if (unit_class < 0 || unit_class >= AGENTITE_UNIT_CLASS_COUNT) {
        return NULL;
    }
    return &g_unit_stats[unit_class];
}

Agentite_Effectiveness agentite_unit_get_effectiveness(
    Agentite_UnitClass attacker,
    Agentite_UnitClass defender)
{
    const Agentite_UnitStats *atk_stats = &g_unit_stats[attacker];

    /* Check if attacker is strong against defender */
    for (int i = 0; i < atk_stats->strong_count; i++) {
        if (atk_stats->strong_against[i] == defender) {
            return AGENTITE_EFFECT_COUNTER;
        }
    }

    /* Check if attacker is weak against defender */
    for (int i = 0; i < atk_stats->weak_count; i++) {
        if (atk_stats->weak_against[i] == defender) {
            return AGENTITE_EFFECT_HARD_COUNTER;
        }
    }

    /* Check reverse (defender strong against attacker) */
    const Agentite_UnitStats *def_stats = &g_unit_stats[defender];
    for (int i = 0; i < def_stats->strong_count; i++) {
        if (def_stats->strong_against[i] == attacker) {
            return AGENTITE_EFFECT_WEAK;
        }
    }

    return AGENTITE_EFFECT_NEUTRAL;
}

float agentite_effectiveness_multiplier(Agentite_Effectiveness effectiveness) {
    switch (effectiveness) {
        case AGENTITE_EFFECT_HARD_COUNTER: return 0.5f;
        case AGENTITE_EFFECT_WEAK:         return 0.75f;
        case AGENTITE_EFFECT_NEUTRAL:      return 1.0f;
        case AGENTITE_EFFECT_STRONG:       return 1.25f;
        case AGENTITE_EFFECT_COUNTER:      return 1.5f;
        default:                           return 1.0f;
    }
}

/*============================================================================
 * Commander Management
 *============================================================================*/

bool agentite_fleet_set_commander(
    Agentite_FleetManager *fm,
    int fleet_id,
    const Agentite_Commander *commander)
{
    AGENTITE_VALIDATE_PTR_RET(fm, false);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet) return false;

    if (commander) {
        memcpy(&fleet->commander, commander, sizeof(Agentite_Commander));
        fleet->has_commander = true;

        /* Ensure minimum level */
        if (fleet->commander.level < 1) {
            fleet->commander.level = 1;
        }
    } else {
        memset(&fleet->commander, 0, sizeof(Agentite_Commander));
        fleet->has_commander = false;
    }

    return true;
}

Agentite_Commander *agentite_fleet_get_commander(Agentite_FleetManager *fm, int fleet_id) {
    AGENTITE_VALIDATE_PTR_RET(fm, NULL);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet || !fleet->has_commander) return NULL;

    return &fleet->commander;
}

bool agentite_fleet_commander_add_xp(Agentite_FleetManager *fm, int fleet_id, int xp) {
    AGENTITE_VALIDATE_PTR_RET(fm, false);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet || !fleet->has_commander) return false;

    fleet->commander.experience += xp;

    /* Level up check (100 XP per level) */
    int xp_needed = fleet->commander.level * 100;
    if (fleet->commander.experience >= xp_needed && fleet->commander.level < 10) {
        fleet->commander.experience -= xp_needed;
        fleet->commander.level++;

        /* Increase bonuses on level up */
        fleet->commander.attack_bonus += 2;
        fleet->commander.defense_bonus += 2;
        fleet->commander.morale_bonus += 1;

        return true;  /* Leveled up */
    }

    return false;
}

int agentite_commander_get_bonus(const Agentite_Commander *commander, int stat) {
    if (!commander) return 0;

    /* Base bonus from level */
    int base = commander->level * 2;

    switch (stat) {
        case 0: return commander->attack_bonus + base;
        case 1: return commander->defense_bonus + base;
        case 2: return commander->morale_bonus + commander->level;
        case 3: return commander->speed_bonus + commander->level;
        default: return 0;
    }
}

/*============================================================================
 * Fleet Operations
 *============================================================================*/

bool agentite_fleet_merge(Agentite_FleetManager *fm, int dst_id, int src_id) {
    AGENTITE_VALIDATE_PTR_RET(fm, false);

    Agentite_Fleet *dst = agentite_fleet_get(fm, dst_id);
    Agentite_Fleet *src = agentite_fleet_get(fm, src_id);
    if (!dst || !src) return false;

    /* Check domain compatibility */
    if (dst->unit_count > 0 && src->unit_count > 0) {
        if (dst->is_space_fleet != src->is_space_fleet) {
            agentite_set_error("Cannot merge space and ground fleets");
            return false;
        }
    }

    /* Merge units */
    for (int i = 0; i < src->unit_count; i++) {
        Agentite_UnitGroup *src_group = &src->units[i];
        int dst_idx = find_unit_group(dst, src_group->unit_class);

        if (dst_idx >= 0) {
            dst->units[dst_idx].count += src_group->count;
            /* Average experience */
            dst->units[dst_idx].experience =
                (dst->units[dst_idx].experience + src_group->experience) / 2;
        } else {
            if (dst->unit_count < AGENTITE_FLEET_MAX_UNIT_GROUPS) {
                dst->units[dst->unit_count++] = *src_group;
            }
        }
    }

    /* Remove source fleet */
    agentite_fleet_remove(fm, src_id);
    return true;
}

int agentite_fleet_split(
    Agentite_FleetManager *fm,
    int src_id,
    Agentite_UnitClass unit_class,
    int count,
    const char *new_fleet_name)
{
    AGENTITE_VALIDATE_PTR_RET(fm, AGENTITE_FLEET_INVALID_ID);

    Agentite_Fleet *src = agentite_fleet_get(fm, src_id);
    if (!src) return AGENTITE_FLEET_INVALID_ID;

    int removed = agentite_fleet_remove_units(fm, src_id, unit_class, count);
    if (removed <= 0) return AGENTITE_FLEET_INVALID_ID;

    Agentite_Fleet new_fleet = {0};
    if (new_fleet_name) {
        strncpy(new_fleet.name, new_fleet_name, sizeof(new_fleet.name) - 1);
    } else {
        snprintf(new_fleet.name, sizeof(new_fleet.name), "%s (Split)", src->name);
    }
    new_fleet.owner_id = src->owner_id;
    new_fleet.morale = src->morale;
    new_fleet.is_space_fleet = src->is_space_fleet;
    new_fleet.position_x = src->position_x;
    new_fleet.position_y = src->position_y;
    new_fleet.sector_id = src->sector_id;

    int new_id = agentite_fleet_add(fm, &new_fleet);
    if (new_id < 0) {
        /* Restore units to source */
        agentite_fleet_add_units(fm, src_id, unit_class, removed);
        return AGENTITE_FLEET_INVALID_ID;
    }

    agentite_fleet_add_units(fm, new_id, unit_class, removed);
    return new_id;
}

void agentite_fleet_update_morale(Agentite_FleetManager *fm, int fleet_id, int delta) {
    AGENTITE_VALIDATE_PTR(fm);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet) return;

    fleet->morale += delta;

    /* Apply commander morale bonus */
    if (delta < 0 && fleet->has_commander) {
        fleet->morale += fleet->commander.morale_bonus / 2;
    }

    if (fleet->morale < 0) fleet->morale = 0;
    if (fleet->morale > 100) fleet->morale = 100;
}

void agentite_fleet_repair(Agentite_FleetManager *fm, int fleet_id, int heal_amount) {
    AGENTITE_VALIDATE_PTR(fm);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet) return;

    for (int i = 0; i < fleet->unit_count; i++) {
        Agentite_UnitGroup *group = &fleet->units[i];
        const Agentite_UnitStats *stats = &g_unit_stats[group->unit_class];
        group->health += heal_amount;
        if (group->health > stats->hp) {
            group->health = stats->hp;
        }
    }
}

/*============================================================================
 * Battle System
 *============================================================================*/

bool agentite_fleet_preview_battle(
    const Agentite_FleetManager *fm,
    int attacker_id,
    int defender_id,
    Agentite_BattlePreview *out_preview)
{
    AGENTITE_VALIDATE_PTR_RET(fm, false);
    AGENTITE_VALIDATE_PTR_RET(out_preview, false);

    const Agentite_Fleet *attacker = agentite_fleet_get_const(fm, attacker_id);
    const Agentite_Fleet *defender = agentite_fleet_get_const(fm, defender_id);
    if (!attacker || !defender) return false;

    memset(out_preview, 0, sizeof(Agentite_BattlePreview));

    /* Calculate strengths */
    int atk_strength = calculate_fleet_attack(attacker) + calculate_fleet_defense(attacker);
    int def_strength = calculate_fleet_attack(defender) + calculate_fleet_defense(defender);

    out_preview->attacker_strength = atk_strength;
    out_preview->defender_strength = def_strength;

    /* Lanchester's square law estimation */
    float total = (float)(atk_strength + def_strength);
    if (total <= 0) {
        out_preview->attacker_win_chance = 0.5f;
        out_preview->defender_win_chance = 0.5f;
        return true;
    }

    float atk_ratio = (float)atk_strength / total;
    float def_ratio = (float)def_strength / total;

    /* Win chance based on strength ratio squared */
    float atk_squared = atk_ratio * atk_ratio;
    float def_squared = def_ratio * def_ratio;
    float total_squared = atk_squared + def_squared;

    out_preview->attacker_win_chance = atk_squared / total_squared;
    out_preview->defender_win_chance = def_squared / total_squared;

    /* Estimate casualties */
    int atk_units = get_total_units(attacker);
    int def_units = get_total_units(defender);

    out_preview->estimated_attacker_losses =
        (int)(atk_units * (1.0f - out_preview->attacker_win_chance) * 0.6f);
    out_preview->estimated_defender_losses =
        (int)(def_units * (1.0f - out_preview->defender_win_chance) * 0.6f);

    /* Warnings */
    out_preview->attacker_outmatched = (atk_strength < def_strength * 0.5f);
    out_preview->defender_outmatched = (def_strength < atk_strength * 0.5f);

    return true;
}

bool agentite_fleet_battle(
    Agentite_FleetManager *fm,
    int attacker_id,
    int defender_id,
    Agentite_BattleResult *out_result)
{
    AGENTITE_VALIDATE_PTR_RET(fm, false);
    AGENTITE_VALIDATE_PTR_RET(out_result, false);

    Agentite_Fleet *attacker = agentite_fleet_get(fm, attacker_id);
    Agentite_Fleet *defender = agentite_fleet_get(fm, defender_id);
    if (!attacker || !defender) return false;

    memset(out_result, 0, sizeof(Agentite_BattleResult));
    out_result->attacker_id = attacker_id;
    out_result->defender_id = defender_id;

    int initial_atk_units = get_total_units(attacker);
    int initial_def_units = get_total_units(defender);

    attacker->in_combat = true;
    defender->in_combat = true;

    /* Battle rounds */
    int max_rounds = 20;
    for (int round = 0; round < max_rounds; round++) {
        int atk_units = get_total_units(attacker);
        int def_units = get_total_units(defender);

        if (atk_units <= 0 || def_units <= 0) break;
        if (attacker->is_retreating || defender->is_retreating) break;

        Agentite_BattleRound *br = &out_result->rounds[round];
        br->round_number = round + 1;
        br->attacker_morale = attacker->morale;
        br->defender_morale = defender->morale;

        /* Calculate damage */
        int atk_damage = calculate_fleet_attack(attacker);
        int def_damage = calculate_fleet_attack(defender);

        /* First strike bonus */
        if (round == 0) {
            if (attacker->has_commander &&
                attacker->commander.ability == AGENTITE_ABILITY_FIRST_STRIKE) {
                atk_damage = (int)(atk_damage * 1.25f);
            }
            if (defender->has_commander &&
                defender->commander.ability == AGENTITE_ABILITY_FIRST_STRIKE) {
                def_damage = (int)(def_damage * 1.25f);
            }
        }

        br->attacker_damage = atk_damage;
        br->defender_damage = def_damage;

        /* Calculate casualties based on damage vs defense */
        int def_defense = calculate_fleet_defense(defender);
        int atk_defense = calculate_fleet_defense(attacker);

        int def_casualties = (atk_damage - def_defense / 2) / 20;
        int atk_casualties = (def_damage - atk_defense / 2) / 20;

        if (def_casualties < 0) def_casualties = 0;
        if (atk_casualties < 0) atk_casualties = 0;

        /* Minimum 1 casualty if damage dealt */
        if (atk_damage > 0 && def_casualties == 0 && def_units > 0) def_casualties = 1;
        if (def_damage > 0 && atk_casualties == 0 && atk_units > 0) atk_casualties = 1;

        br->attacker_losses = atk_casualties;
        br->defender_losses = def_casualties;

        apply_casualties(attacker, atk_casualties);
        apply_casualties(defender, def_casualties);

        /* Update morale */
        int atk_morale_loss = 5 + (atk_casualties * 2);
        int def_morale_loss = 5 + (def_casualties * 2);

        agentite_fleet_update_morale(fm, attacker_id, -atk_morale_loss);
        agentite_fleet_update_morale(fm, defender_id, -def_morale_loss);

        /* Check for retreat */
        if (attacker->morale < 20) {
            attacker->is_retreating = true;
        }
        if (defender->morale < 20) {
            defender->is_retreating = true;
        }

        out_result->rounds_fought++;

        /* Emit callback */
        if (fm->battle_callback) {
            fm->battle_callback(fm, br, fm->battle_userdata);
        }
    }

    /* Determine outcome */
    int final_atk_units = get_total_units(attacker);
    int final_def_units = get_total_units(defender);

    out_result->attacker_units_lost = initial_atk_units - final_atk_units;
    out_result->defender_units_lost = initial_def_units - final_def_units;
    out_result->attacker_units_remaining = final_atk_units;
    out_result->defender_units_remaining = final_def_units;

    if (attacker->is_retreating) {
        out_result->outcome = AGENTITE_BATTLE_ATTACKER_RETREAT;
        out_result->winner_id = defender_id;
    } else if (defender->is_retreating) {
        out_result->outcome = AGENTITE_BATTLE_DEFENDER_RETREAT;
        out_result->winner_id = attacker_id;
    } else if (final_atk_units <= 0 && final_def_units <= 0) {
        out_result->outcome = AGENTITE_BATTLE_DRAW;
        out_result->winner_id = -1;
    } else if (final_def_units <= 0) {
        out_result->outcome = AGENTITE_BATTLE_ATTACKER_WIN;
        out_result->winner_id = attacker_id;
    } else if (final_atk_units <= 0) {
        out_result->outcome = AGENTITE_BATTLE_DEFENDER_WIN;
        out_result->winner_id = defender_id;
    } else {
        out_result->outcome = AGENTITE_BATTLE_DRAW;
        out_result->winner_id = -1;
    }

    /* Award experience */
    out_result->attacker_xp = out_result->defender_units_lost * 10;
    out_result->defender_xp = out_result->attacker_units_lost * 10;

    if (out_result->winner_id == attacker_id) {
        out_result->attacker_xp += 50;
    } else if (out_result->winner_id == defender_id) {
        out_result->defender_xp += 50;
    }

    agentite_fleet_add_unit_xp(fm, attacker_id, -1, out_result->attacker_xp);
    agentite_fleet_add_unit_xp(fm, defender_id, -1, out_result->defender_xp);

    if (attacker->has_commander) {
        agentite_fleet_commander_add_xp(fm, attacker_id, out_result->attacker_xp / 5);
    }
    if (defender->has_commander) {
        agentite_fleet_commander_add_xp(fm, defender_id, out_result->defender_xp / 5);
    }

    attacker->in_combat = false;
    defender->in_combat = false;
    attacker->is_retreating = false;
    defender->is_retreating = false;

    return true;
}

void agentite_fleet_set_battle_callback(
    Agentite_FleetManager *fm,
    Agentite_BattleCallback callback,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(fm);
    fm->battle_callback = callback;
    fm->battle_userdata = userdata;
}

bool agentite_fleet_retreat(Agentite_FleetManager *fm, int fleet_id) {
    AGENTITE_VALIDATE_PTR_RET(fm, false);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet || !fleet->in_combat) return false;

    fleet->is_retreating = true;

    /* Tactical retreat ability reduces morale loss */
    int morale_loss = 20;
    if (fleet->has_commander &&
        fleet->commander.ability == AGENTITE_ABILITY_TACTICAL_RETREAT) {
        morale_loss = 10;
    }

    agentite_fleet_update_morale(fm, fleet_id, -morale_loss);
    return true;
}

/*============================================================================
 * Experience System
 *============================================================================*/

void agentite_fleet_add_unit_xp(
    Agentite_FleetManager *fm,
    int fleet_id,
    int unit_class,
    int xp)
{
    AGENTITE_VALIDATE_PTR(fm);

    Agentite_Fleet *fleet = agentite_fleet_get(fm, fleet_id);
    if (!fleet || xp <= 0) return;

    /* Veteran training ability bonus */
    float xp_mult = 1.0f;
    if (fleet->has_commander &&
        fleet->commander.ability == AGENTITE_ABILITY_VETERAN_TRAINING) {
        xp_mult = 1.5f;
    }

    int adjusted_xp = (int)(xp * xp_mult);

    for (int i = 0; i < fleet->unit_count; i++) {
        if (unit_class < 0 || fleet->units[i].unit_class == (Agentite_UnitClass)unit_class) {
            fleet->units[i].experience += adjusted_xp;
            if (fleet->units[i].experience > 1000) {
                fleet->units[i].experience = 1000;  /* Cap at veteran */
            }
        }
    }
}

float agentite_unit_xp_bonus(const Agentite_UnitGroup *group) {
    if (!group) return 1.0f;

    /* 0-1000 XP maps to 1.0-1.5 damage multiplier */
    return 1.0f + (group->experience / 2000.0f);
}

/*============================================================================
 * Utility
 *============================================================================*/

const char *agentite_unit_class_name(Agentite_UnitClass unit_class) {
    if (unit_class < 0 || unit_class >= AGENTITE_UNIT_CLASS_COUNT) {
        return "Unknown";
    }
    return g_unit_stats[unit_class].name;
}

const char *agentite_unit_domain_name(Agentite_UnitDomain domain) {
    switch (domain) {
        case AGENTITE_DOMAIN_SPACE:  return "Space";
        case AGENTITE_DOMAIN_GROUND: return "Ground";
        case AGENTITE_DOMAIN_AIR:    return "Air";
        case AGENTITE_DOMAIN_NAVAL:  return "Naval";
        default:                     return "Unknown";
    }
}

const char *agentite_battle_outcome_name(Agentite_BattleOutcome outcome) {
    switch (outcome) {
        case AGENTITE_BATTLE_ATTACKER_WIN:     return "Attacker Victory";
        case AGENTITE_BATTLE_DEFENDER_WIN:     return "Defender Victory";
        case AGENTITE_BATTLE_DRAW:             return "Draw";
        case AGENTITE_BATTLE_ATTACKER_RETREAT: return "Attacker Retreated";
        case AGENTITE_BATTLE_DEFENDER_RETREAT: return "Defender Retreated";
        default:                               return "Unknown";
    }
}

const char *agentite_commander_ability_name(Agentite_CommanderAbility ability) {
    switch (ability) {
        case AGENTITE_ABILITY_NONE:             return "None";
        case AGENTITE_ABILITY_FIRST_STRIKE:     return "First Strike";
        case AGENTITE_ABILITY_TACTICAL_RETREAT: return "Tactical Retreat";
        case AGENTITE_ABILITY_INSPIRATION:      return "Inspiration";
        case AGENTITE_ABILITY_FLANKING:         return "Flanking";
        case AGENTITE_ABILITY_FORTIFY:          return "Fortify";
        case AGENTITE_ABILITY_BLITZ:            return "Blitz";
        case AGENTITE_ABILITY_LOGISTICS:        return "Logistics";
        case AGENTITE_ABILITY_VETERAN_TRAINING: return "Veteran Training";
        default:                                return "Unknown";
    }
}

int agentite_fleet_get_upkeep(const Agentite_FleetManager *fm, int fleet_id) {
    AGENTITE_VALIDATE_PTR_RET(fm, 0);

    const Agentite_Fleet *fleet = agentite_fleet_get_const(fm, fleet_id);
    if (!fleet) return 0;

    int total = 0;
    for (int i = 0; i < fleet->unit_count; i++) {
        const Agentite_UnitGroup *group = &fleet->units[i];
        const Agentite_UnitStats *stats = &g_unit_stats[group->unit_class];
        total += stats->upkeep * group->count;
    }

    /* Logistics ability reduces upkeep */
    if (fleet->has_commander &&
        fleet->commander.ability == AGENTITE_ABILITY_LOGISTICS) {
        total = (int)(total * 0.8f);
    }

    return total;
}

int agentite_fleet_get_cost(const Agentite_FleetManager *fm, int fleet_id) {
    AGENTITE_VALIDATE_PTR_RET(fm, 0);

    const Agentite_Fleet *fleet = agentite_fleet_get_const(fm, fleet_id);
    if (!fleet) return 0;

    int total = 0;
    for (int i = 0; i < fleet->unit_count; i++) {
        const Agentite_UnitGroup *group = &fleet->units[i];
        const Agentite_UnitStats *stats = &g_unit_stats[group->unit_class];
        total += stats->cost * group->count;
    }

    return total;
}
