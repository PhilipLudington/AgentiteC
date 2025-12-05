#ifndef CARBON_TRADE_H
#define CARBON_TRADE_H

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
 *   Carbon_TradeSystem *trade = carbon_trade_create();
 *
 *   // Create routes between locations
 *   uint32_t route = carbon_trade_create_route(trade, city_a, city_b,
 *                                               CARBON_ROUTE_TRADE);
 *   carbon_trade_set_route_protection(trade, route, 0.8f);  // 80% protected
 *
 *   // Set distance callback for efficiency calculation
 *   carbon_trade_set_distance_callback(trade, calc_distance, map_data);
 *
 *   // Calculate faction income
 *   int32_t income = carbon_trade_calculate_income(trade, player_faction);
 *
 *   // Supply hubs provide bonuses
 *   carbon_trade_set_hub(trade, capital_location, true);
 *   Carbon_SupplyBonus bonus = carbon_trade_get_supply_bonus(trade, location);
 *
 *   // Cleanup
 *   carbon_trade_destroy(trade);
 */

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_TRADE_MAX_ROUTES      128   /* Maximum routes in system */
#define CARBON_TRADE_MAX_HUBS        16    /* Maximum supply hubs */
#define CARBON_TRADE_INVALID         0     /* Invalid route handle */

/*============================================================================
 * Route Types
 *============================================================================*/

/**
 * Types of routes with different effects
 */
typedef enum Carbon_RouteType {
    CARBON_ROUTE_TRADE = 0,     /* Resource income */
    CARBON_ROUTE_MILITARY,      /* Ship repair, reinforcement speed */
    CARBON_ROUTE_COLONIAL,      /* Population growth bonus */
    CARBON_ROUTE_RESEARCH,      /* Research speed bonus */
    CARBON_ROUTE_TYPE_COUNT,

    /* User-defined route types start here */
    CARBON_ROUTE_USER = 100,
} Carbon_RouteType;

/**
 * Route status
 */
typedef enum Carbon_RouteStatus {
    CARBON_ROUTE_ACTIVE = 0,    /* Route is operational */
    CARBON_ROUTE_DISRUPTED,     /* Partially blocked (reduced efficiency) */
    CARBON_ROUTE_BLOCKED,       /* Fully blocked (no benefits) */
    CARBON_ROUTE_ESTABLISHING,  /* Being set up (not yet active) */
} Carbon_RouteStatus;

/*============================================================================
 * Data Structures
 *============================================================================*/

/**
 * Trade route between two locations
 */
typedef struct Carbon_TradeRoute {
    uint32_t id;                /* Unique route identifier */
    uint32_t source;            /* Source location ID */
    uint32_t dest;              /* Destination location ID */
    Carbon_RouteType type;      /* Route type */
    Carbon_RouteStatus status;  /* Current status */

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
} Carbon_TradeRoute;

/**
 * Supply bonus from routes and hubs
 */
typedef struct Carbon_SupplyBonus {
    float repair_rate;          /* Ship repair multiplier */
    float reinforce_rate;       /* Reinforcement speed multiplier */
    float growth_rate;          /* Population growth multiplier */
    float research_rate;        /* Research speed multiplier */
    float income_rate;          /* Income multiplier */
    int route_count;            /* Number of routes to this location */
    bool has_hub;               /* Is this location a hub */
} Carbon_SupplyBonus;

/**
 * Supply hub location
 */
typedef struct Carbon_SupplyHub {
    uint32_t location;          /* Location ID */
    int32_t faction;            /* Owning faction */
    float bonus_radius;         /* Bonus effect radius */
    float bonus_strength;       /* Bonus multiplier */
    bool active;                /* Is this slot in use */
} Carbon_SupplyHub;

/**
 * Faction trade statistics
 */
typedef struct Carbon_TradeStats {
    int32_t total_income;       /* Total income from all routes */
    int32_t total_routes;       /* Number of owned routes */
    int32_t active_routes;      /* Number of active routes */
    int32_t trade_routes;       /* Number of trade routes */
    int32_t military_routes;    /* Number of military routes */
    int32_t colonial_routes;    /* Number of colonial routes */
    int32_t research_routes;    /* Number of research routes */
    float average_efficiency;   /* Average route efficiency */
    float average_protection;   /* Average protection level */
} Carbon_TradeStats;

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
typedef float (*Carbon_DistanceFunc)(uint32_t source, uint32_t dest, void *userdata);

/**
 * Route value calculation callback
 *
 * @param route      Route being evaluated
 * @param userdata   User context
 * @return Calculated value/income for this route
 */
typedef int32_t (*Carbon_RouteValueFunc)(const Carbon_TradeRoute *route, void *userdata);

/**
 * Route event callback
 *
 * @param trade      Trade system
 * @param route_id   Route that changed
 * @param event      Event type (0=created, 1=destroyed, 2=status changed)
 * @param userdata   User context
 */
typedef void (*Carbon_RouteEventFunc)(void *trade, uint32_t route_id,
                                       int event, void *userdata);

/*============================================================================
 * Trade System
 *============================================================================*/

typedef struct Carbon_TradeSystem Carbon_TradeSystem;

/**
 * Create a new trade system.
 *
 * @return New trade system or NULL on failure
 */
Carbon_TradeSystem *carbon_trade_create(void);

/**
 * Destroy a trade system and free resources.
 *
 * @param trade Trade system to destroy
 */
void carbon_trade_destroy(Carbon_TradeSystem *trade);

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
 * @return Route ID or CARBON_TRADE_INVALID on failure
 */
uint32_t carbon_trade_create_route(Carbon_TradeSystem *trade,
                                    uint32_t source, uint32_t dest,
                                    Carbon_RouteType type);

/**
 * Create route with extended options.
 *
 * @param trade      Trade system
 * @param source     Source location ID
 * @param dest       Destination location ID
 * @param type       Route type
 * @param faction    Owning faction
 * @param base_value Base value of route
 * @return Route ID or CARBON_TRADE_INVALID on failure
 */
uint32_t carbon_trade_create_route_ex(Carbon_TradeSystem *trade,
                                       uint32_t source, uint32_t dest,
                                       Carbon_RouteType type,
                                       int32_t faction, int32_t base_value);

/**
 * Remove a route.
 *
 * @param trade    Trade system
 * @param route_id Route to remove
 */
void carbon_trade_remove_route(Carbon_TradeSystem *trade, uint32_t route_id);

/**
 * Get a route by ID.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route pointer or NULL if not found
 */
const Carbon_TradeRoute *carbon_trade_get_route(const Carbon_TradeSystem *trade,
                                                 uint32_t route_id);

/**
 * Get mutable route for modification.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route pointer or NULL if not found
 */
Carbon_TradeRoute *carbon_trade_get_route_mut(Carbon_TradeSystem *trade,
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
void carbon_trade_set_route_protection(Carbon_TradeSystem *trade,
                                        uint32_t route_id, float protection);

/**
 * Get route protection level.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Protection level (0.0 if not found)
 */
float carbon_trade_get_route_protection(const Carbon_TradeSystem *trade,
                                         uint32_t route_id);

/**
 * Set route status.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param status   New status
 */
void carbon_trade_set_route_status(Carbon_TradeSystem *trade,
                                    uint32_t route_id, Carbon_RouteStatus status);

/**
 * Get route status.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route status
 */
Carbon_RouteStatus carbon_trade_get_route_status(const Carbon_TradeSystem *trade,
                                                  uint32_t route_id);

/**
 * Set route owner faction.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param faction  Owner faction (-1 for none)
 */
void carbon_trade_set_route_owner(Carbon_TradeSystem *trade,
                                   uint32_t route_id, int32_t faction);

/**
 * Set route base value.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param value    Base value
 */
void carbon_trade_set_route_value(Carbon_TradeSystem *trade,
                                   uint32_t route_id, int32_t value);

/**
 * Set route metadata.
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @param metadata Game-specific metadata
 */
void carbon_trade_set_route_metadata(Carbon_TradeSystem *trade,
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
float carbon_trade_get_efficiency(const Carbon_TradeSystem *trade,
                                   uint32_t route_id);

/**
 * Set distance calculation callback.
 *
 * @param trade       Trade system
 * @param distance_fn Distance function
 * @param userdata    User context
 */
void carbon_trade_set_distance_callback(Carbon_TradeSystem *trade,
                                         Carbon_DistanceFunc distance_fn,
                                         void *userdata);

/**
 * Set route value calculation callback.
 *
 * @param trade    Trade system
 * @param value_fn Value calculation function
 * @param userdata User context
 */
void carbon_trade_set_value_callback(Carbon_TradeSystem *trade,
                                      Carbon_RouteValueFunc value_fn,
                                      void *userdata);

/**
 * Recalculate all route efficiencies.
 * Call after changing distances or protection levels.
 *
 * @param trade Trade system
 */
void carbon_trade_recalculate_efficiency(Carbon_TradeSystem *trade);

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
int32_t carbon_trade_calculate_income(const Carbon_TradeSystem *trade,
                                       int32_t faction_id);

/**
 * Set tax rate for a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction ID
 * @param rate       Tax rate (0.0-1.0, applied to income)
 */
void carbon_trade_set_tax_rate(Carbon_TradeSystem *trade,
                                int32_t faction_id, float rate);

/**
 * Get tax rate for a faction.
 *
 * @param trade      Trade system
 * @param faction_id Faction ID
 * @return Tax rate
 */
float carbon_trade_get_tax_rate(const Carbon_TradeSystem *trade,
                                 int32_t faction_id);

/**
 * Calculate route income (before tax).
 *
 * @param trade    Trade system
 * @param route_id Route ID
 * @return Route income
 */
int32_t carbon_trade_calculate_route_income(const Carbon_TradeSystem *trade,
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
void carbon_trade_set_hub(Carbon_TradeSystem *trade, uint32_t location, bool is_hub);

/**
 * Set hub with extended options.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @param faction  Owning faction
 * @param radius   Bonus effect radius
 * @param strength Bonus multiplier
 */
void carbon_trade_set_hub_ex(Carbon_TradeSystem *trade, uint32_t location,
                              int32_t faction, float radius, float strength);

/**
 * Check if a location is a hub.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @return true if location is a hub
 */
bool carbon_trade_is_hub(const Carbon_TradeSystem *trade, uint32_t location);

/**
 * Get hub info.
 *
 * @param trade    Trade system
 * @param location Location ID
 * @return Hub pointer or NULL if not a hub
 */
const Carbon_SupplyHub *carbon_trade_get_hub(const Carbon_TradeSystem *trade,
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
int carbon_trade_get_hub_connections(const Carbon_TradeSystem *trade,
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
Carbon_SupplyBonus carbon_trade_get_supply_bonus(const Carbon_TradeSystem *trade,
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
int carbon_trade_get_routes_from(const Carbon_TradeSystem *trade,
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
int carbon_trade_get_routes_to(const Carbon_TradeSystem *trade,
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
int carbon_trade_get_routes_by_faction(const Carbon_TradeSystem *trade,
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
int carbon_trade_get_routes_by_type(const Carbon_TradeSystem *trade,
                                     Carbon_RouteType type,
                                     uint32_t *out_routes, int max);

/**
 * Get all routes.
 *
 * @param trade      Trade system
 * @param out_routes Output array of route IDs
 * @param max        Maximum routes to return
 * @return Number of routes
 */
int carbon_trade_get_all_routes(const Carbon_TradeSystem *trade,
                                 uint32_t *out_routes, int max);

/**
 * Check if route exists between two locations.
 *
 * @param trade  Trade system
 * @param source Source location
 * @param dest   Destination location
 * @return Route ID or CARBON_TRADE_INVALID if no route
 */
uint32_t carbon_trade_find_route(const Carbon_TradeSystem *trade,
                                  uint32_t source, uint32_t dest);

/**
 * Check if route exists (either direction).
 *
 * @param trade Trade system
 * @param loc1  First location
 * @param loc2  Second location
 * @return Route ID or CARBON_TRADE_INVALID if no route
 */
uint32_t carbon_trade_find_route_any(const Carbon_TradeSystem *trade,
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
void carbon_trade_get_stats(const Carbon_TradeSystem *trade,
                             int32_t faction_id,
                             Carbon_TradeStats *out_stats);

/**
 * Get total route count.
 *
 * @param trade Trade system
 * @return Number of routes
 */
int carbon_trade_count(const Carbon_TradeSystem *trade);

/**
 * Get total hub count.
 *
 * @param trade Trade system
 * @return Number of hubs
 */
int carbon_trade_hub_count(const Carbon_TradeSystem *trade);

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
void carbon_trade_set_event_callback(Carbon_TradeSystem *trade,
                                      Carbon_RouteEventFunc callback,
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
void carbon_trade_update(Carbon_TradeSystem *trade);

/**
 * Clear all routes.
 *
 * @param trade Trade system
 */
void carbon_trade_clear(Carbon_TradeSystem *trade);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get route type name.
 *
 * @param type Route type
 * @return Static string name
 */
const char *carbon_trade_route_type_name(Carbon_RouteType type);

/**
 * Get route status name.
 *
 * @param status Route status
 * @return Static string name
 */
const char *carbon_trade_route_status_name(Carbon_RouteStatus status);

#endif /* CARBON_TRADE_H */
