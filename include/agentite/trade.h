#ifndef AGENTITE_TRADE_H
#define AGENTITE_TRADE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Carbon Trade Route / Supply Line System
 *
 * Economic connections between locations with efficiency calculations,
 * protection mechanics, and specialized route types.
 *
 * Usage:
 *   // Create trade system
 *   Agentite_TradeSystem *trade = agentite_trade_create();
 *
 *   // Create routes between locations
 *   uint32_t route = agentite_trade_create_route(trade, city_a, city_b,
 *                                               AGENTITE_ROUTE_TRADE);
 *   agentite_trade_set_route_protection(trade, route, 0.8f);  // 80% protected
 *
 *   // Set distance callback for efficiency calculation
 *   agentite_trade_set_distance_callback(trade, calc_distance, map_data);
 *
 *   // Calculate faction income
 *   int32_t income = agentite_trade_calculate_income(trade, player_faction);
 *
 *   // Supply hubs provide bonuses
 *   agentite_trade_set_hub(trade, capital_location, true);
 *   Agentite_SupplyBonus bonus = agentite_trade_get_supply_bonus(trade, location);
 *
 *   // Cleanup
 *   agentite_trade_destroy(trade);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define AGENTITE_TRADE_MAX_ROUTES      128   /* Maximum routes in system */
#define AGENTITE_TRADE_MAX_HUBS        16    /* Maximum supply hubs */
#define AGENTITE_TRADE_INVALID         0     /* Invalid route handle */

/*============================================================================
 * Route Types
 *============================================================================*/

/**
 * Types of routes with different effects
 */
typedef enum Agentite_RouteType {
    AGENTITE_ROUTE_TRADE = 0,     /* Resource income */
    AGENTITE_ROUTE_MILITARY,      /* Ship repair, reinforcement speed */
    AGENTITE_ROUTE_COLONIAL,      /* Population growth bonus */
    AGENTITE_ROUTE_RESEARCH,      /* Research speed bonus */
    AGENTITE_ROUTE_TYPE_COUNT,

    /* User-defined route types start here */
    AGENTITE_ROUTE_USER = 100,
} Agentite_RouteType;

/**
 * Route status
 */
typedef enum Agentite_RouteStatus {
    AGENTITE_ROUTE_ACTIVE = 0,    /* Route is operational */
    AGENTITE_ROUTE_DISRUPTED,     /* Partially blocked (reduced efficiency) */
    AGENTITE_ROUTE_BLOCKED,       /* Fully blocked (no benefits) */
    AGENTITE_ROUTE_ESTABLISHING,  /* Being set up (not yet active) */
} Agentite_RouteStatus;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Trade route between two locations
 */
typedef struct Agentite_TradeRoute {
    uint32_t id;                /* Unique route identifier */
    uint32_t source;            /* Source location ID */
    uint32_t dest;              /* Destination location ID */
    Agentite_RouteType type;      /* Route type */
    Agentite_RouteStatus status;  /* Current status */

    /* Route properties */
    int32_t base_value;         /* Base value/income of route */
    float protection;           /* Protection level (0.0-1.0) */
    float efficiency;           /* Calculated efficiency (0.0-1.0) */
    float distance;             /* Cached distance between endpoints */

    /* Ownership */
    int32_t owner_faction;      /* Faction that owns this route (-1 = none) */

    /* Metadata */
    int32_t turns_active;       /* Turns since route was established */
    uint32_t metadata;          /* Game-specific data */

    bool active;                /* Is this slot in use */
} Agentite_TradeRoute;

/**
 * Supply bonus from routes and hubs
 */
typedef struct Agentite_SupplyBonus {
    float repair_rate;          /* Ship repair multiplier */
    float reinforce_rate;       /* Reinforcement speed multiplier */
    float growth_rate;          /* Population growth multiplier */
    float research_rate;        /* Research speed multiplier */
    float income_rate;          /* Income multiplier */
    int route_count;            /* Number of routes to this location */
    bool has_hub;               /* Is this location a hub */
} Agentite_SupplyBonus;

/**
 * Supply hub location
 */
typedef struct Agentite_SupplyHub {
    uint32_t location;          /* Location ID */
    int32_t faction;            /* Owning faction */
    float bonus_radius;         /* Bonus effect radius */
    float bonus_strength;       /* Bonus multiplier */
    bool active;                /* Is this slot in use */
} Agentite_SupplyHub;

/**
 * Faction trade statistics
 */
typedef struct Agentite_TradeStats {
    int32_t total_income;       /* Total income from all routes */
    int32_t total_routes;       /* Number of owned routes */
    int32_t active_routes;      /* Number of active routes */
    int32_t trade_routes;       /* Number of trade routes */
    int32_t military_routes;    /* Number of military routes */
    int32_t colonial_routes;    /* Number of colonial routes */
    int32_t research_routes;    /* Number of research routes */
    float average_efficiency;   /* Average route efficiency */
    float average_protection;   /* Average protection level */
} Agentite_TradeStats;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Distance calculation callback
 *
 * @param source     Source location ID
 * @param dest       Destination location ID
 * @param userdata   User context
 * @return Distance between locations
 */
typedef float (*Agentite_DistanceFunc)(uint32_t source, uint32_t dest, void *userdata);

/**
 * Route value calculation callback
 *
 * @param route      Route being evaluated
 * @param userdata   User context
 * @return Calculated value/income for this route
 */
typedef int32_t (*Agentite_RouteValueFunc)(const Agentite_TradeRoute *route, void *userdata);

/**
 * Route event callback
 *
 * @param trade      Trade system
 * @param route_id   Route that changed
 * @param event      Event type (0=created, 1=destroyed, 2=status changed)
 * @param userdata   User context
 */
typedef void (*Agentite_RouteEventFunc)(void *trade, uint32_t route_id,
                                       int event, void *userdata);

/*============================================================================
 * Trade System
 *============================================================================*/

typedef struct Agentite_TradeSystem Agentite_TradeSystem;

/**
 * Create a new trade system.
 *
 * @return New trade system or NULL on failure
 */
Agentite_TradeSystem *agentite_trade_create(void);

/**
 * Destroy a trade system and free resources.
 *
 * @param trade Trade system to destroy
 */
void agentite_trade_destroy(Agentite_TradeSystem *trade);

/*============================================================================
 * Route Management
 *============================================================================*/

/**
 * Create a new trade route.
 *
 * @param trade  Trade system
 * @param source Source location ID
 * @param dest   Destination location ID
 * @param type   Route type
 * @return Route ID or AGENTITE_TRADE_INVALID on failure
 */
uint32_t agentite_trade_create_route(Agentite_TradeSystem *trade,
                                    uint32_t source, uint32_t dest,
                                    Agentite_RouteType type);

/**
 * Create route with extended options.
 *
 * @param trade      Trade system
 * @param source     Source location ID
 * @param dest       Destination location ID
 * @param type       Route type
 * @param faction    Owning faction
 * @param base_value Base value of route
 * @return Route ID or AGENTITE_TRADE_INVALID on failure
 */
uint32_t agentite_trade_create_route_ex(Agentite_TradeSystem *trade,
                                       uint32_t source, uint32_t dest,
                                       Agentite_RouteType type,
                                       int32_t faction, int32_t base_value);

/**
 * Remove a route.
 *
 * @param trade    Trade system
 * @param route_id Route to remove
 */
void agentite_trade_remove_route(Agentite_TradeSystem *trade, uint32_t route_id);

/**
 * Get a route by ID.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route pointer or NULL if not found
 */
const Agentite_TradeRoute *agentite_trade_get_route(const Agentite_TradeSystem *trade,
                                                 uint32_t route_id);

/**
 * Get mutable route for modification.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route pointer or NULL if not found
 */
Agentite_TradeRoute *agentite_trade_get_route_mut(Agentite_TradeSystem *trade,
                                               uint32_t route_id);

/*============================================================================
 * Route Properties
 *============================================================================*/

/**
 * Set route protection level.
 *
 * @param trade      Trade system
 * @param route_id   Route ID
 * @param protection Protection level (0.0-1.0)
 */
void agentite_trade_set_route_protection(Agentite_TradeSystem *trade,
                                        uint32_t route_id, float protection);

/**
 * Get route protection level.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Protection level (0.0 if not found)
 */
float agentite_trade_get_route_protection(const Agentite_TradeSystem *trade,
                                         uint32_t route_id);

/**
 * Set route status.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param status   New status
 */
void agentite_trade_set_route_status(Agentite_TradeSystem *trade,
                                    uint32_t route_id, Agentite_RouteStatus status);

/**
 * Get route status.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route status
 */
Agentite_RouteStatus agentite_trade_get_route_status(const Agentite_TradeSystem *trade,
                                                  uint32_t route_id);

/**
 * Set route owner faction.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param faction  Owner faction (-1 for none)
 */
void agentite_trade_set_route_owner(Agentite_TradeSystem *trade,
                                   uint32_t route_id, int32_t faction);

/**
 * Set route base value.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param value    Base value
 */
void agentite_trade_set_route_value(Agentite_TradeSystem *trade,
                                   uint32_t route_id, int32_t value);

/**
 * Set route metadata.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param metadata Game-specific metadata
 */
void agentite_trade_set_route_metadata(Agentite_TradeSystem *trade,
                                      uint32_t route_id, uint32_t metadata);

/*============================================================================
 * Efficiency Calculation
 *============================================================================*/

/**
 * Get route efficiency.
 * Efficiency is based on distance, protection, and status.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Efficiency (0.0-1.0)
 */
float agentite_trade_get_efficiency(const Agentite_TradeSystem *trade,
                                   uint32_t route_id);

/**
 * Set distance calculation callback.
 *
 * @param trade       Trade system
 * @param distance_fn Distance function
 * @param userdata    User context
 */
void agentite_trade_set_distance_callback(Agentite_TradeSystem *trade,
                                         Agentite_DistanceFunc distance_fn,
                                         void *userdata);

/**
 * Set route value calculation callback.
 *
 * @param trade    Trade system
 * @param value_fn Value calculation function
 * @param userdata User context
 */
void agentite_trade_set_value_callback(Agentite_TradeSystem *trade,
                                      Agentite_RouteValueFunc value_fn,
                                      void *userdata);

/**
 * Recalculate all route efficiencies.
 * Call after changing distances or protection levels.
 *
 * @param trade Trade system
 */
void agentite_trade_recalculate_efficiency(Agentite_TradeSystem *trade);

/*============================================================================
 * Income Calculation
 *============================================================================*/

/**
 * Calculate total income for a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction to calculate for
 * @return Total income from all routes
 */
int32_t agentite_trade_calculate_income(const Agentite_TradeSystem *trade,
                                       int32_t faction_id);

/**
 * Set tax rate for a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction ID
 * @param rate       Tax rate (0.0-1.0, applied to income)
 */
void agentite_trade_set_tax_rate(Agentite_TradeSystem *trade,
                                int32_t faction_id, float rate);

/**
 * Get tax rate for a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction ID
 * @return Tax rate
 */
float agentite_trade_get_tax_rate(const Agentite_TradeSystem *trade,
                                 int32_t faction_id);

/**
 * Calculate route income (before tax).
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route income
 */
int32_t agentite_trade_calculate_route_income(const Agentite_TradeSystem *trade,
                                             uint32_t route_id);

/*============================================================================
 * Supply Hubs
 *============================================================================*/

/**
 * Set a location as a supply hub.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @param is_hub   Whether this is a hub
 */
void agentite_trade_set_hub(Agentite_TradeSystem *trade, uint32_t location, bool is_hub);

/**
 * Set hub with extended options.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @param faction  Owning faction
 * @param radius   Bonus effect radius
 * @param strength Bonus multiplier
 */
void agentite_trade_set_hub_ex(Agentite_TradeSystem *trade, uint32_t location,
                              int32_t faction, float radius, float strength);

/**
 * Check if a location is a hub.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @return true if location is a hub
 */
bool agentite_trade_is_hub(const Agentite_TradeSystem *trade, uint32_t location);

/**
 * Get hub info.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @return Hub pointer or NULL if not a hub
 */
const Agentite_SupplyHub *agentite_trade_get_hub(const Agentite_TradeSystem *trade,
                                              uint32_t location);

/**
 * Get all hub connections from a hub.
 *
 * @param trade           Trade system
 * @param hub_location    Hub location ID
 * @param out_connections Output array of connected locations
 * @param max             Maximum connections to return
 * @return Number of connections
 */
int agentite_trade_get_hub_connections(const Agentite_TradeSystem *trade,
                                      uint32_t hub_location,
                                      uint32_t *out_connections, int max);

/**
 * Get supply bonus for a location.
 * Aggregates bonuses from all routes and nearby hubs.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @return Supply bonus
 */
Agentite_SupplyBonus agentite_trade_get_supply_bonus(const Agentite_TradeSystem *trade,
                                                  uint32_t location);

/*============================================================================
 * Route Queries
 *============================================================================*/

/**
 * Get routes from a source location.
 *
 * @param trade      Trade system
 * @param source     Source location ID
 * @param out_routes Output array of route IDs
 * @param max        Maximum routes to return
 * @return Number of routes
 */
int agentite_trade_get_routes_from(const Agentite_TradeSystem *trade,
                                  uint32_t source,
                                  uint32_t *out_routes, int max);

/**
 * Get routes to a destination location.
 *
 * @param trade      Trade system
 * @param dest       Destination location ID
 * @param out_routes Output array of route IDs
 * @param max        Maximum routes to return
 * @return Number of routes
 */
int agentite_trade_get_routes_to(const Agentite_TradeSystem *trade,
                                uint32_t dest,
                                uint32_t *out_routes, int max);

/**
 * Get routes owned by a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction ID
 * @param out_routes Output array of route IDs
 * @param max        Maximum routes to return
 * @return Number of routes
 */
int agentite_trade_get_routes_by_faction(const Agentite_TradeSystem *trade,
                                        int32_t faction_id,
                                        uint32_t *out_routes, int max);

/**
 * Get routes by type.
 *
 * @param trade      Trade system
 * @param type       Route type
 * @param out_routes Output array of route IDs
 * @param max        Maximum routes to return
 * @return Number of routes
 */
int agentite_trade_get_routes_by_type(const Agentite_TradeSystem *trade,
                                     Agentite_RouteType type,
                                     uint32_t *out_routes, int max);

/**
 * Get all routes.
 *
 * @param trade      Trade system
 * @param out_routes Output array of route IDs
 * @param max        Maximum routes to return
 * @return Number of routes
 */
int agentite_trade_get_all_routes(const Agentite_TradeSystem *trade,
                                 uint32_t *out_routes, int max);

/**
 * Check if route exists between two locations.
 *
 * @param trade  Trade system
 * @param source Source location
 * @param dest   Destination location
 * @return Route ID or AGENTITE_TRADE_INVALID if no route
 */
uint32_t agentite_trade_find_route(const Agentite_TradeSystem *trade,
                                  uint32_t source, uint32_t dest);

/**
 * Check if route exists (either direction).
 *
 * @param trade Trade system
 * @param loc1  First location
 * @param loc2  Second location
 * @return Route ID or AGENTITE_TRADE_INVALID if no route
 */
uint32_t agentite_trade_find_route_any(const Agentite_TradeSystem *trade,
                                      uint32_t loc1, uint32_t loc2);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Get trade statistics for a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction ID
 * @param out_stats  Output statistics
 */
void agentite_trade_get_stats(const Agentite_TradeSystem *trade,
                             int32_t faction_id,
                             Agentite_TradeStats *out_stats);

/**
 * Get total route count.
 *
 * @param trade Trade system
 * @return Number of routes
 */
int agentite_trade_count(const Agentite_TradeSystem *trade);

/**
 * Get total hub count.
 *
 * @param trade Trade system
 * @return Number of hubs
 */
int agentite_trade_hub_count(const Agentite_TradeSystem *trade);

/*============================================================================
 * Event Callback
 *============================================================================*/

/**
 * Set route event callback.
 *
 * @param trade    Trade system
 * @param callback Event callback function
 * @param userdata User context
 */
void agentite_trade_set_event_callback(Agentite_TradeSystem *trade,
                                      Agentite_RouteEventFunc callback,
                                      void *userdata);

/*============================================================================
 * Turn Management
 *============================================================================*/

/**
 * Update trade system (call each turn).
 * Ages routes and recalculates efficiencies.
 *
 * @param trade Trade system
 */
void agentite_trade_update(Agentite_TradeSystem *trade);

/**
 * Clear all routes.
 *
 * @param trade Trade system
 */
void agentite_trade_clear(Agentite_TradeSystem *trade);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get route type name.
 *
 * @param type Route type
 * @return Static string name
 */
const char *agentite_trade_route_type_name(Agentite_RouteType type);

/**
 * Get route status name.
 *
 * @param status Route status
 * @return Static string name
 */
const char *agentite_trade_route_status_name(Agentite_RouteStatus status);

#endif /* AGENTITE_TRADE_H */
