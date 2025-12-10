/**
 * Carbon Trade Route / Supply Line System
 *
 * Economic connections between locations with efficiency calculations,
 * protection mechanics, and specialized route types.
 */

#include "agentite/agentite.h"
#include "agentite/trade.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Constants
 *============================================================================*/

#define AGENTITE_TRADE_MAX_FACTIONS 16   /* Maximum tracked factions */

/*============================================================================
 * Internal Data Structures
 *============================================================================*/

/**
 * Per-faction tax rate
 */
typedef struct {
    int32_t faction_id;
    float tax_rate;
    bool used;
} FactionTax;

/**
 * Trade system internal structure
 */
struct Agentite_TradeSystem {
    /* Routes */
    Agentite_TradeRoute routes[AGENTITE_TRADE_MAX_ROUTES];
    uint32_t next_route_id;

    /* Supply hubs */
    Agentite_SupplyHub hubs[AGENTITE_TRADE_MAX_HUBS];
    int hub_count;

    /* Faction tax rates */
    FactionTax taxes[AGENTITE_TRADE_MAX_FACTIONS];

    /* Callbacks */
    Agentite_DistanceFunc distance_fn;
    void *distance_userdata;
    Agentite_RouteValueFunc value_fn;
    void *value_userdata;
    Agentite_RouteEventFunc event_fn;
    void *event_userdata;
};

/*============================================================================
 * Helper Functions
 *============================================================================*/

/**
 * Find route by ID
 */
static Agentite_TradeRoute *find_route(Agentite_TradeSystem *trade, uint32_t route_id) {
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        if (trade->routes[i].active && trade->routes[i].id == route_id) {
            return &trade->routes[i];
        }
    }
    return NULL;
}

/**
 * Find free route slot
 */
static Agentite_TradeRoute *alloc_route(Agentite_TradeSystem *trade) {
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        if (!trade->routes[i].active) {
            return &trade->routes[i];
        }
    }
    return NULL;
}

/**
 * Find hub by location
 */
static Agentite_SupplyHub *find_hub(Agentite_TradeSystem *trade, uint32_t location) {
    for (int i = 0; i < AGENTITE_TRADE_MAX_HUBS; i++) {
        if (trade->hubs[i].active && trade->hubs[i].location == location) {
            return &trade->hubs[i];
        }
    }
    return NULL;
}

/**
 * Find free hub slot
 */
static Agentite_SupplyHub *alloc_hub(Agentite_TradeSystem *trade) {
    for (int i = 0; i < AGENTITE_TRADE_MAX_HUBS; i++) {
        if (!trade->hubs[i].active) {
            return &trade->hubs[i];
        }
    }
    return NULL;
}

/**
 * Get or create faction tax entry
 */
static FactionTax *get_faction_tax(Agentite_TradeSystem *trade, int32_t faction_id) {
    /* Find existing */
    for (int i = 0; i < AGENTITE_TRADE_MAX_FACTIONS; i++) {
        if (trade->taxes[i].used && trade->taxes[i].faction_id == faction_id) {
            return &trade->taxes[i];
        }
    }

    /* Find free slot */
    for (int i = 0; i < AGENTITE_TRADE_MAX_FACTIONS; i++) {
        if (!trade->taxes[i].used) {
            trade->taxes[i].used = true;
            trade->taxes[i].faction_id = faction_id;
            trade->taxes[i].tax_rate = 0.0f;
            return &trade->taxes[i];
        }
    }

    return NULL;
}

/**
 * Calculate distance between locations
 */
static float calc_distance(Agentite_TradeSystem *trade, uint32_t source, uint32_t dest) {
    if (trade->distance_fn) {
        return trade->distance_fn(source, dest, trade->distance_userdata);
    }
    /* Default: assume distance of 1 */
    return 1.0f;
}

/**
 * Calculate route efficiency based on distance and protection
 */
static float calc_efficiency(const Agentite_TradeRoute *route) {
    float eff = 1.0f;

    /* Distance penalty: efficiency decreases with distance */
    /* E = 1 / (1 + distance * 0.1) gives reasonable curve */
    if (route->distance > 0) {
        eff = 1.0f / (1.0f + route->distance * 0.1f);
    }

    /* Protection bonus: higher protection = higher efficiency */
    /* Protection of 1.0 means no loss, 0.0 means 50% loss */
    float protection_factor = 0.5f + route->protection * 0.5f;
    eff *= protection_factor;

    /* Status penalty */
    switch (route->status) {
        case AGENTITE_ROUTE_ACTIVE:
            /* No penalty */
            break;
        case AGENTITE_ROUTE_DISRUPTED:
            eff *= 0.5f;
            break;
        case AGENTITE_ROUTE_BLOCKED:
        case AGENTITE_ROUTE_ESTABLISHING:
            eff = 0.0f;
            break;
    }

    /* Clamp to valid range */
    if (eff < 0.0f) eff = 0.0f;
    if (eff > 1.0f) eff = 1.0f;

    return eff;
}

/**
 * Emit route event
 */
static void emit_event(Agentite_TradeSystem *trade, uint32_t route_id, int event) {
    if (trade->event_fn) {
        trade->event_fn(trade, route_id, event, trade->event_userdata);
    }
}

/*============================================================================
 * Creation and Destruction
 *============================================================================*/

Agentite_TradeSystem *agentite_trade_create(void) {
    Agentite_TradeSystem *trade = AGENTITE_ALLOC(Agentite_TradeSystem);
    if (!trade) {
        agentite_set_error("agentite_trade_create: allocation failed");
        return NULL;
    }
    trade->next_route_id = 1;
    return trade;
}

void agentite_trade_destroy(Agentite_TradeSystem *trade) {
    if (trade) {
        free(trade);
    }
}

/*============================================================================
 * Route Management
 *============================================================================*/

uint32_t agentite_trade_create_route(Agentite_TradeSystem *trade,
                                    uint32_t source, uint32_t dest,
                                    Agentite_RouteType type) {
    return agentite_trade_create_route_ex(trade, source, dest, type, -1, 100);
}

uint32_t agentite_trade_create_route_ex(Agentite_TradeSystem *trade,
                                       uint32_t source, uint32_t dest,
                                       Agentite_RouteType type,
                                       int32_t faction, int32_t base_value) {
    if (!trade) return AGENTITE_TRADE_INVALID;

    Agentite_TradeRoute *route = alloc_route(trade);
    if (!route) {
        agentite_set_error("agentite_trade_create_route: max routes reached");
        return AGENTITE_TRADE_INVALID;
    }

    memset(route, 0, sizeof(Agentite_TradeRoute));
    route->id = trade->next_route_id++;
    route->source = source;
    route->dest = dest;
    route->type = type;
    route->status = AGENTITE_ROUTE_ACTIVE;
    route->base_value = base_value;
    route->protection = 0.5f;  /* Default 50% protection */
    route->owner_faction = faction;
    route->turns_active = 0;
    route->active = true;

    /* Calculate distance and efficiency */
    route->distance = calc_distance(trade, source, dest);
    route->efficiency = calc_efficiency(route);

    emit_event(trade, route->id, 0);  /* Created */

    return route->id;
}

void agentite_trade_remove_route(Agentite_TradeSystem *trade, uint32_t route_id) {
    if (!trade) return;

    Agentite_TradeRoute *route = find_route(trade, route_id);
    if (route) {
        emit_event(trade, route_id, 1);  /* Destroyed */
        route->active = false;
    }
}

const Agentite_TradeRoute *agentite_trade_get_route(const Agentite_TradeSystem *trade,
                                                 uint32_t route_id) {
    if (!trade) return NULL;
    return find_route((Agentite_TradeSystem *)trade, route_id);
}

Agentite_TradeRoute *agentite_trade_get_route_mut(Agentite_TradeSystem *trade,
                                               uint32_t route_id) {
    if (!trade) return NULL;
    return find_route(trade, route_id);
}

/*============================================================================
 * Route Properties
 *============================================================================*/

void agentite_trade_set_route_protection(Agentite_TradeSystem *trade,
                                        uint32_t route_id, float protection) {
    if (!trade) return;

    Agentite_TradeRoute *route = find_route(trade, route_id);
    if (route) {
        if (protection < 0.0f) protection = 0.0f;
        if (protection > 1.0f) protection = 1.0f;
        route->protection = protection;
        route->efficiency = calc_efficiency(route);
    }
}

float agentite_trade_get_route_protection(const Agentite_TradeSystem *trade,
                                         uint32_t route_id) {
    if (!trade) return 0.0f;

    const Agentite_TradeRoute *route = agentite_trade_get_route(trade, route_id);
    return route ? route->protection : 0.0f;
}

void agentite_trade_set_route_status(Agentite_TradeSystem *trade,
                                    uint32_t route_id, Agentite_RouteStatus status) {
    if (!trade) return;

    Agentite_TradeRoute *route = find_route(trade, route_id);
    if (route && route->status != status) {
        route->status = status;
        route->efficiency = calc_efficiency(route);
        emit_event(trade, route_id, 2);  /* Status changed */
    }
}

Agentite_RouteStatus agentite_trade_get_route_status(const Agentite_TradeSystem *trade,
                                                  uint32_t route_id) {
    if (!trade) return AGENTITE_ROUTE_BLOCKED;

    const Agentite_TradeRoute *route = agentite_trade_get_route(trade, route_id);
    return route ? route->status : AGENTITE_ROUTE_BLOCKED;
}

void agentite_trade_set_route_owner(Agentite_TradeSystem *trade,
                                   uint32_t route_id, int32_t faction) {
    if (!trade) return;

    Agentite_TradeRoute *route = find_route(trade, route_id);
    if (route) {
        route->owner_faction = faction;
    }
}

void agentite_trade_set_route_value(Agentite_TradeSystem *trade,
                                   uint32_t route_id, int32_t value) {
    if (!trade) return;

    Agentite_TradeRoute *route = find_route(trade, route_id);
    if (route) {
        route->base_value = value;
    }
}

void agentite_trade_set_route_metadata(Agentite_TradeSystem *trade,
                                      uint32_t route_id, uint32_t metadata) {
    if (!trade) return;

    Agentite_TradeRoute *route = find_route(trade, route_id);
    if (route) {
        route->metadata = metadata;
    }
}

/*============================================================================
 * Efficiency Calculation
 *============================================================================*/

float agentite_trade_get_efficiency(const Agentite_TradeSystem *trade,
                                   uint32_t route_id) {
    if (!trade) return 0.0f;

    const Agentite_TradeRoute *route = agentite_trade_get_route(trade, route_id);
    return route ? route->efficiency : 0.0f;
}

void agentite_trade_set_distance_callback(Agentite_TradeSystem *trade,
                                         Agentite_DistanceFunc distance_fn,
                                         void *userdata) {
    if (trade) {
        trade->distance_fn = distance_fn;
        trade->distance_userdata = userdata;
    }
}

void agentite_trade_set_value_callback(Agentite_TradeSystem *trade,
                                      Agentite_RouteValueFunc value_fn,
                                      void *userdata) {
    if (trade) {
        trade->value_fn = value_fn;
        trade->value_userdata = userdata;
    }
}

void agentite_trade_recalculate_efficiency(Agentite_TradeSystem *trade) {
    if (!trade) return;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active) {
            route->distance = calc_distance(trade, route->source, route->dest);
            route->efficiency = calc_efficiency(route);
        }
    }
}

/*============================================================================
 * Income Calculation
 *============================================================================*/

int32_t agentite_trade_calculate_route_income(const Agentite_TradeSystem *trade,
                                             uint32_t route_id) {
    if (!trade) return 0;

    const Agentite_TradeRoute *route = agentite_trade_get_route(trade, route_id);
    if (!route) return 0;

    /* Use custom value function if provided */
    int32_t base_value = route->base_value;
    if (trade->value_fn) {
        base_value = trade->value_fn(route, ((Agentite_TradeSystem *)trade)->value_userdata);
    }

    /* Apply efficiency */
    float value = (float)base_value * route->efficiency;

    return (int32_t)value;
}

int32_t agentite_trade_calculate_income(const Agentite_TradeSystem *trade,
                                       int32_t faction_id) {
    if (!trade) return 0;

    int32_t total = 0;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active && route->owner_faction == faction_id &&
            route->type == AGENTITE_ROUTE_TRADE) {
            total += agentite_trade_calculate_route_income(trade, route->id);
        }
    }

    /* Apply tax rate */
    float tax_rate = agentite_trade_get_tax_rate(trade, faction_id);
    if (tax_rate > 0.0f) {
        total = (int32_t)((float)total * (1.0f + tax_rate));
    }

    return total;
}

void agentite_trade_set_tax_rate(Agentite_TradeSystem *trade,
                                int32_t faction_id, float rate) {
    if (!trade) return;

    FactionTax *tax = get_faction_tax(trade, faction_id);
    if (tax) {
        if (rate < 0.0f) rate = 0.0f;
        if (rate > 1.0f) rate = 1.0f;
        tax->tax_rate = rate;
    }
}

float agentite_trade_get_tax_rate(const Agentite_TradeSystem *trade,
                                 int32_t faction_id) {
    if (!trade) return 0.0f;

    for (int i = 0; i < AGENTITE_TRADE_MAX_FACTIONS; i++) {
        if (trade->taxes[i].used && trade->taxes[i].faction_id == faction_id) {
            return trade->taxes[i].tax_rate;
        }
    }
    return 0.0f;
}

/*============================================================================
 * Supply Hubs
 *============================================================================*/

void agentite_trade_set_hub(Agentite_TradeSystem *trade, uint32_t location, bool is_hub) {
    if (!trade) return;

    Agentite_SupplyHub *hub = find_hub(trade, location);

    if (is_hub) {
        if (!hub) {
            hub = alloc_hub(trade);
            if (!hub) {
                agentite_set_error("agentite_trade_set_hub: max hubs reached");
                return;
            }
            trade->hub_count++;
        }
        hub->location = location;
        hub->faction = -1;
        hub->bonus_radius = 5.0f;
        hub->bonus_strength = 1.0f;
        hub->active = true;
    } else if (hub) {
        hub->active = false;
        trade->hub_count--;
    }
}

void agentite_trade_set_hub_ex(Agentite_TradeSystem *trade, uint32_t location,
                              int32_t faction, float radius, float strength) {
    if (!trade) return;

    Agentite_SupplyHub *hub = find_hub(trade, location);
    if (!hub) {
        hub = alloc_hub(trade);
        if (!hub) {
            agentite_set_error("agentite_trade_set_hub_ex: max hubs reached");
            return;
        }
        trade->hub_count++;
    }

    hub->location = location;
    hub->faction = faction;
    hub->bonus_radius = radius;
    hub->bonus_strength = strength;
    hub->active = true;
}

bool agentite_trade_is_hub(const Agentite_TradeSystem *trade, uint32_t location) {
    if (!trade) return false;
    return find_hub((Agentite_TradeSystem *)trade, location) != NULL;
}

const Agentite_SupplyHub *agentite_trade_get_hub(const Agentite_TradeSystem *trade,
                                              uint32_t location) {
    if (!trade) return NULL;
    return find_hub((Agentite_TradeSystem *)trade, location);
}

int agentite_trade_get_hub_connections(const Agentite_TradeSystem *trade,
                                      uint32_t hub_location,
                                      uint32_t *out_connections, int max) {
    if (!trade || !out_connections || max <= 0) return 0;

    int count = 0;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES && count < max; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (!route->active) continue;

        if (route->source == hub_location) {
            out_connections[count++] = route->dest;
        } else if (route->dest == hub_location) {
            out_connections[count++] = route->source;
        }
    }

    return count;
}

Agentite_SupplyBonus agentite_trade_get_supply_bonus(const Agentite_TradeSystem *trade,
                                                  uint32_t location) {
    Agentite_SupplyBonus bonus = {
        .repair_rate = 1.0f,
        .reinforce_rate = 1.0f,
        .growth_rate = 1.0f,
        .research_rate = 1.0f,
        .income_rate = 1.0f,
        .route_count = 0,
        .has_hub = false
    };

    if (!trade) return bonus;

    /* Check if location is a hub */
    const Agentite_SupplyHub *hub = agentite_trade_get_hub(trade, location);
    if (hub) {
        bonus.has_hub = true;
        /* Hub provides base bonus */
        float strength = hub->bonus_strength;
        bonus.repair_rate += 0.25f * strength;
        bonus.reinforce_rate += 0.25f * strength;
        bonus.growth_rate += 0.1f * strength;
        bonus.research_rate += 0.1f * strength;
        bonus.income_rate += 0.2f * strength;
    }

    /* Count and aggregate route bonuses */
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (!route->active) continue;

        /* Check if route connects to this location */
        if (route->source != location && route->dest != location) continue;

        bonus.route_count++;
        float eff = route->efficiency;

        /* Apply route type bonuses */
        switch (route->type) {
            case AGENTITE_ROUTE_TRADE:
                bonus.income_rate += 0.1f * eff;
                break;
            case AGENTITE_ROUTE_MILITARY:
                bonus.repair_rate += 0.2f * eff;
                bonus.reinforce_rate += 0.3f * eff;
                break;
            case AGENTITE_ROUTE_COLONIAL:
                bonus.growth_rate += 0.2f * eff;
                break;
            case AGENTITE_ROUTE_RESEARCH:
                bonus.research_rate += 0.2f * eff;
                break;
            default:
                break;
        }
    }

    return bonus;
}

/*============================================================================
 * Route Queries
 *============================================================================*/

int agentite_trade_get_routes_from(const Agentite_TradeSystem *trade,
                                  uint32_t source,
                                  uint32_t *out_routes, int max) {
    if (!trade || !out_routes || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES && count < max; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active && route->source == source) {
            out_routes[count++] = route->id;
        }
    }
    return count;
}

int agentite_trade_get_routes_to(const Agentite_TradeSystem *trade,
                                uint32_t dest,
                                uint32_t *out_routes, int max) {
    if (!trade || !out_routes || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES && count < max; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active && route->dest == dest) {
            out_routes[count++] = route->id;
        }
    }
    return count;
}

int agentite_trade_get_routes_by_faction(const Agentite_TradeSystem *trade,
                                        int32_t faction_id,
                                        uint32_t *out_routes, int max) {
    if (!trade || !out_routes || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES && count < max; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active && route->owner_faction == faction_id) {
            out_routes[count++] = route->id;
        }
    }
    return count;
}

int agentite_trade_get_routes_by_type(const Agentite_TradeSystem *trade,
                                     Agentite_RouteType type,
                                     uint32_t *out_routes, int max) {
    if (!trade || !out_routes || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES && count < max; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active && route->type == type) {
            out_routes[count++] = route->id;
        }
    }
    return count;
}

int agentite_trade_get_all_routes(const Agentite_TradeSystem *trade,
                                 uint32_t *out_routes, int max) {
    if (!trade || !out_routes || max <= 0) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES && count < max; i++) {
        if (trade->routes[i].active) {
            out_routes[count++] = trade->routes[i].id;
        }
    }
    return count;
}

uint32_t agentite_trade_find_route(const Agentite_TradeSystem *trade,
                                  uint32_t source, uint32_t dest) {
    if (!trade) return AGENTITE_TRADE_INVALID;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active && route->source == source && route->dest == dest) {
            return route->id;
        }
    }
    return AGENTITE_TRADE_INVALID;
}

uint32_t agentite_trade_find_route_any(const Agentite_TradeSystem *trade,
                                      uint32_t loc1, uint32_t loc2) {
    if (!trade) return AGENTITE_TRADE_INVALID;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (!route->active) continue;

        if ((route->source == loc1 && route->dest == loc2) ||
            (route->source == loc2 && route->dest == loc1)) {
            return route->id;
        }
    }
    return AGENTITE_TRADE_INVALID;
}

/*============================================================================
 * Statistics
 *============================================================================*/

void agentite_trade_get_stats(const Agentite_TradeSystem *trade,
                             int32_t faction_id,
                             Agentite_TradeStats *out_stats) {
    if (!out_stats) return;

    memset(out_stats, 0, sizeof(Agentite_TradeStats));

    if (!trade) return;

    float total_eff = 0.0f;
    float total_prot = 0.0f;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        const Agentite_TradeRoute *route = &trade->routes[i];
        if (!route->active || route->owner_faction != faction_id) continue;

        out_stats->total_routes++;

        if (route->status == AGENTITE_ROUTE_ACTIVE) {
            out_stats->active_routes++;
        }

        switch (route->type) {
            case AGENTITE_ROUTE_TRADE:
                out_stats->trade_routes++;
                out_stats->total_income += agentite_trade_calculate_route_income(trade, route->id);
                break;
            case AGENTITE_ROUTE_MILITARY:
                out_stats->military_routes++;
                break;
            case AGENTITE_ROUTE_COLONIAL:
                out_stats->colonial_routes++;
                break;
            case AGENTITE_ROUTE_RESEARCH:
                out_stats->research_routes++;
                break;
            default:
                break;
        }

        total_eff += route->efficiency;
        total_prot += route->protection;
    }

    if (out_stats->total_routes > 0) {
        out_stats->average_efficiency = total_eff / out_stats->total_routes;
        out_stats->average_protection = total_prot / out_stats->total_routes;
    }
}

int agentite_trade_count(const Agentite_TradeSystem *trade) {
    if (!trade) return 0;

    int count = 0;
    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        if (trade->routes[i].active) count++;
    }
    return count;
}

int agentite_trade_hub_count(const Agentite_TradeSystem *trade) {
    return trade ? trade->hub_count : 0;
}

/*============================================================================
 * Event Callback
 *============================================================================*/

void agentite_trade_set_event_callback(Agentite_TradeSystem *trade,
                                      Agentite_RouteEventFunc callback,
                                      void *userdata) {
    if (trade) {
        trade->event_fn = callback;
        trade->event_userdata = userdata;
    }
}

/*============================================================================
 * Turn Management
 *============================================================================*/

void agentite_trade_update(Agentite_TradeSystem *trade) {
    if (!trade) return;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        Agentite_TradeRoute *route = &trade->routes[i];
        if (route->active) {
            route->turns_active++;

            /* Routes that are establishing become active after 1 turn */
            if (route->status == AGENTITE_ROUTE_ESTABLISHING && route->turns_active > 0) {
                route->status = AGENTITE_ROUTE_ACTIVE;
                route->efficiency = calc_efficiency(route);
                emit_event(trade, route->id, 2);  /* Status changed */
            }
        }
    }
}

void agentite_trade_clear(Agentite_TradeSystem *trade) {
    if (!trade) return;

    for (int i = 0; i < AGENTITE_TRADE_MAX_ROUTES; i++) {
        trade->routes[i].active = false;
    }
    for (int i = 0; i < AGENTITE_TRADE_MAX_HUBS; i++) {
        trade->hubs[i].active = false;
    }
    trade->hub_count = 0;
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_trade_route_type_name(Agentite_RouteType type) {
    switch (type) {
        case AGENTITE_ROUTE_TRADE:    return "Trade";
        case AGENTITE_ROUTE_MILITARY: return "Military";
        case AGENTITE_ROUTE_COLONIAL: return "Colonial";
        case AGENTITE_ROUTE_RESEARCH: return "Research";
        default:
            if (type >= AGENTITE_ROUTE_USER) return "Custom";
            return "Unknown";
    }
}

const char *agentite_trade_route_status_name(Agentite_RouteStatus status) {
    switch (status) {
        case AGENTITE_ROUTE_ACTIVE:       return "Active";
        case AGENTITE_ROUTE_DISRUPTED:    return "Disrupted";
        case AGENTITE_ROUTE_BLOCKED:      return "Blocked";
        case AGENTITE_ROUTE_ESTABLISHING: return "Establishing";
        default:                        return "Unknown";
    }
}
