/**
 * Carbon Game Query API
 *
 * Read-only state queries with structured results for clean UI integration.
 * Provides query registration, cached query results, query invalidation on
 * state change, and structured result formats.
 *
 * Usage:
 *   // Create query system
 *   Carbon_QuerySystem *queries = carbon_query_create();
 *
 *   // Register a query
 *   carbon_query_register(queries, "faction_resources", query_faction_resources,
 *                         sizeof(FactionResourcesResult));
 *
 *   // Execute query
 *   FactionResourcesResult result;
 *   if (carbon_query_exec(queries, "faction_resources", game_state, &params, &result)) {
 *       // Use result
 *   }
 *
 *   // Enable caching for frequently-used queries
 *   carbon_query_enable_cache(queries, "faction_resources", 16);
 *
 *   // Invalidate cache on state change
 *   carbon_query_invalidate(queries, "faction_resources");
 *   carbon_query_invalidate_all(queries);
 *
 *   // Cleanup
 *   carbon_query_destroy(queries);
 */

#ifndef CARBON_QUERY_H
#define CARBON_QUERY_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * Constants
 *============================================================================*/

#define CARBON_QUERY_MAX_QUERIES     64    /* Maximum registered queries */
#define CARBON_QUERY_MAX_NAME_LEN    32    /* Maximum query name length */
#define CARBON_QUERY_MAX_RESULT_SIZE 4096  /* Maximum result size in bytes */
#define CARBON_QUERY_MAX_CACHE_SIZE  32    /* Maximum cache entries per query */
#define CARBON_QUERY_CACHE_KEY_SIZE  64    /* Size of cache key buffer */

/*============================================================================
 * Query Result Status
 *============================================================================*/

/**
 * Query execution status.
 */
typedef enum Carbon_QueryStatus {
    CARBON_QUERY_OK = 0,          /* Query succeeded */
    CARBON_QUERY_NOT_FOUND,       /* Query not registered */
    CARBON_QUERY_INVALID_PARAMS,  /* Invalid parameters */
    CARBON_QUERY_FAILED,          /* Query execution failed */
    CARBON_QUERY_NO_RESULT,       /* Query returned no results */
    CARBON_QUERY_CACHE_HIT,       /* Result returned from cache */
} Carbon_QueryStatus;

/*============================================================================
 * Query Parameter Types
 *============================================================================*/

/**
 * Query parameter value types.
 */
typedef enum Carbon_QueryParamType {
    CARBON_QUERY_PARAM_NONE = 0,
    CARBON_QUERY_PARAM_INT,
    CARBON_QUERY_PARAM_INT64,
    CARBON_QUERY_PARAM_FLOAT,
    CARBON_QUERY_PARAM_DOUBLE,
    CARBON_QUERY_PARAM_BOOL,
    CARBON_QUERY_PARAM_STRING,
    CARBON_QUERY_PARAM_PTR,
    CARBON_QUERY_PARAM_ENTITY,
    CARBON_QUERY_PARAM_RECT,       /* x, y, w, h */
    CARBON_QUERY_PARAM_POINT,      /* x, y */
} Carbon_QueryParamType;

/**
 * Query parameter value.
 */
typedef struct Carbon_QueryParam {
    Carbon_QueryParamType type;
    union {
        int32_t i32;
        int64_t i64;
        float f32;
        double f64;
        bool b;
        char str[CARBON_QUERY_MAX_NAME_LEN];
        void *ptr;
        uint32_t entity;
        struct { int32_t x, y, w, h; } rect;
        struct { int32_t x, y; } point;
    };
} Carbon_QueryParam;

/**
 * Query parameters container.
 */
#define CARBON_QUERY_MAX_PARAMS 8

typedef struct Carbon_QueryParams {
    Carbon_QueryParam params[CARBON_QUERY_MAX_PARAMS];
    int count;
} Carbon_QueryParams;

/*============================================================================
 * Query Result Container
 *============================================================================*/

/**
 * Generic query result header.
 * All result structures should start with this.
 */
typedef struct Carbon_QueryResultHeader {
    Carbon_QueryStatus status;    /* Query status */
    int32_t result_count;         /* Number of results (for list queries) */
    uint64_t cache_key;           /* Hash of params used for caching */
    uint32_t timestamp;           /* Monotonic timestamp when cached */
} Carbon_QueryResultHeader;

/**
 * Query result wrapper with data buffer.
 */
typedef struct Carbon_QueryResult {
    Carbon_QueryResultHeader header;
    uint8_t data[CARBON_QUERY_MAX_RESULT_SIZE];  /* Result data */
} Carbon_QueryResult;

/*============================================================================
 * Query System Forward Declaration
 *============================================================================*/

typedef struct Carbon_QuerySystem Carbon_QuerySystem;

/*============================================================================
 * Callback Types
 *============================================================================*/

/**
 * Query function callback.
 * Executes the query and populates the result.
 *
 * @param game_state  Game state pointer
 * @param params      Query parameters (may be NULL)
 * @param result      Output result buffer
 * @param result_size Size of result buffer
 * @param userdata    User context from registration
 * @return Query status
 */
typedef Carbon_QueryStatus (*Carbon_QueryFunc)(void *game_state,
                                                const Carbon_QueryParams *params,
                                                void *result,
                                                size_t result_size,
                                                void *userdata);

/**
 * Cache key generator callback.
 * Generates a cache key from query parameters.
 * If not provided, parameters are hashed automatically.
 *
 * @param params   Query parameters
 * @param userdata User context
 * @return Cache key hash
 */
typedef uint64_t (*Carbon_QueryCacheKeyFunc)(const Carbon_QueryParams *params,
                                              void *userdata);

/**
 * Query invalidation callback.
 * Called when a query cache is invalidated.
 *
 * @param sys        Query system
 * @param query_name Query that was invalidated
 * @param userdata   User context
 */
typedef void (*Carbon_QueryInvalidateCallback)(Carbon_QuerySystem *sys,
                                                 const char *query_name,
                                                 void *userdata);

/*============================================================================
 * Lifecycle
 *============================================================================*/

/**
 * Create a new query system.
 *
 * @return New query system or NULL on failure
 */
Carbon_QuerySystem *carbon_query_create(void);

/**
 * Destroy a query system and free resources.
 *
 * @param sys Query system to destroy
 */
void carbon_query_destroy(Carbon_QuerySystem *sys);

/*============================================================================
 * Query Registration
 *============================================================================*/

/**
 * Register a query.
 *
 * @param sys         Query system
 * @param name        Unique query name
 * @param query_fn    Query function
 * @param result_size Size of result structure
 * @return true if registered successfully
 */
bool carbon_query_register(Carbon_QuerySystem *sys,
                            const char *name,
                            Carbon_QueryFunc query_fn,
                            size_t result_size);

/**
 * Register a query with user context.
 *
 * @param sys         Query system
 * @param name        Unique query name
 * @param query_fn    Query function
 * @param result_size Size of result structure
 * @param userdata    User context passed to query function
 * @return true if registered successfully
 */
bool carbon_query_register_ex(Carbon_QuerySystem *sys,
                               const char *name,
                               Carbon_QueryFunc query_fn,
                               size_t result_size,
                               void *userdata);

/**
 * Unregister a query.
 *
 * @param sys  Query system
 * @param name Query name
 * @return true if unregistered
 */
bool carbon_query_unregister(Carbon_QuerySystem *sys, const char *name);

/**
 * Check if a query is registered.
 *
 * @param sys  Query system
 * @param name Query name
 * @return true if registered
 */
bool carbon_query_is_registered(const Carbon_QuerySystem *sys, const char *name);

/**
 * Get number of registered queries.
 *
 * @param sys Query system
 * @return Number of registered queries
 */
int carbon_query_count(const Carbon_QuerySystem *sys);

/**
 * Get all registered query names.
 *
 * @param sys   Query system
 * @param names Output array of name pointers
 * @param max   Maximum names to return
 * @return Number of names returned
 */
int carbon_query_get_names(const Carbon_QuerySystem *sys, const char **names, int max);

/*============================================================================
 * Query Execution
 *============================================================================*/

/**
 * Execute a query.
 *
 * @param sys        Query system
 * @param name       Query name
 * @param game_state Game state pointer
 * @param params     Query parameters (NULL for parameterless queries)
 * @param result     Output result buffer
 * @return Query status
 */
Carbon_QueryStatus carbon_query_exec(Carbon_QuerySystem *sys,
                                       const char *name,
                                       void *game_state,
                                       const Carbon_QueryParams *params,
                                       void *result);

/**
 * Execute a query with integer parameter.
 * Convenience wrapper for single-parameter queries.
 *
 * @param sys        Query system
 * @param name       Query name
 * @param game_state Game state pointer
 * @param param      Integer parameter
 * @param result     Output result buffer
 * @return Query status
 */
Carbon_QueryStatus carbon_query_exec_int(Carbon_QuerySystem *sys,
                                           const char *name,
                                           void *game_state,
                                           int32_t param,
                                           void *result);

/**
 * Execute a query with entity parameter.
 *
 * @param sys        Query system
 * @param name       Query name
 * @param game_state Game state pointer
 * @param entity     Entity parameter
 * @param result     Output result buffer
 * @return Query status
 */
Carbon_QueryStatus carbon_query_exec_entity(Carbon_QuerySystem *sys,
                                              const char *name,
                                              void *game_state,
                                              uint32_t entity,
                                              void *result);

/**
 * Execute a query with point parameter.
 *
 * @param sys        Query system
 * @param name       Query name
 * @param game_state Game state pointer
 * @param x          X coordinate
 * @param y          Y coordinate
 * @param result     Output result buffer
 * @return Query status
 */
Carbon_QueryStatus carbon_query_exec_point(Carbon_QuerySystem *sys,
                                             const char *name,
                                             void *game_state,
                                             int32_t x, int32_t y,
                                             void *result);

/**
 * Execute a query with rectangle parameter.
 *
 * @param sys        Query system
 * @param name       Query name
 * @param game_state Game state pointer
 * @param x          Rectangle X
 * @param y          Rectangle Y
 * @param w          Rectangle width
 * @param h          Rectangle height
 * @param result     Output result buffer
 * @return Query status
 */
Carbon_QueryStatus carbon_query_exec_rect(Carbon_QuerySystem *sys,
                                            const char *name,
                                            void *game_state,
                                            int32_t x, int32_t y,
                                            int32_t w, int32_t h,
                                            void *result);

/*============================================================================
 * Caching
 *============================================================================*/

/**
 * Enable caching for a query.
 *
 * @param sys        Query system
 * @param name       Query name
 * @param max_cached Maximum cached entries (0 to disable)
 * @return true if enabled
 */
bool carbon_query_enable_cache(Carbon_QuerySystem *sys, const char *name, int max_cached);

/**
 * Disable caching for a query.
 *
 * @param sys  Query system
 * @param name Query name
 */
void carbon_query_disable_cache(Carbon_QuerySystem *sys, const char *name);

/**
 * Check if caching is enabled for a query.
 *
 * @param sys  Query system
 * @param name Query name
 * @return true if caching is enabled
 */
bool carbon_query_is_cached(const Carbon_QuerySystem *sys, const char *name);

/**
 * Set custom cache key generator.
 *
 * @param sys     Query system
 * @param name    Query name
 * @param key_fn  Cache key function (NULL for default)
 * @param userdata User context
 */
void carbon_query_set_cache_key_func(Carbon_QuerySystem *sys,
                                       const char *name,
                                       Carbon_QueryCacheKeyFunc key_fn,
                                       void *userdata);

/**
 * Invalidate cache for a specific query.
 *
 * @param sys  Query system
 * @param name Query name
 */
void carbon_query_invalidate(Carbon_QuerySystem *sys, const char *name);

/**
 * Invalidate cache for queries matching a tag.
 *
 * @param sys Query system
 * @param tag Tag to match
 */
void carbon_query_invalidate_tag(Carbon_QuerySystem *sys, const char *tag);

/**
 * Invalidate all query caches.
 *
 * @param sys Query system
 */
void carbon_query_invalidate_all(Carbon_QuerySystem *sys);

/**
 * Get cache statistics for a query.
 *
 * @param sys       Query system
 * @param name      Query name
 * @param hits      Output hit count (NULL to skip)
 * @param misses    Output miss count (NULL to skip)
 * @param evictions Output eviction count (NULL to skip)
 */
void carbon_query_get_cache_stats(const Carbon_QuerySystem *sys,
                                    const char *name,
                                    uint32_t *hits,
                                    uint32_t *misses,
                                    uint32_t *evictions);

/**
 * Clear cache statistics.
 *
 * @param sys  Query system
 * @param name Query name (NULL for all)
 */
void carbon_query_clear_cache_stats(Carbon_QuerySystem *sys, const char *name);

/*============================================================================
 * Query Tags
 *============================================================================*/

/**
 * Add a tag to a query for group invalidation.
 *
 * @param sys  Query system
 * @param name Query name
 * @param tag  Tag to add
 * @return true if tag added
 */
bool carbon_query_add_tag(Carbon_QuerySystem *sys, const char *name, const char *tag);

/**
 * Remove a tag from a query.
 *
 * @param sys  Query system
 * @param name Query name
 * @param tag  Tag to remove
 * @return true if tag removed
 */
bool carbon_query_remove_tag(Carbon_QuerySystem *sys, const char *name, const char *tag);

/**
 * Check if a query has a tag.
 *
 * @param sys  Query system
 * @param name Query name
 * @param tag  Tag to check
 * @return true if query has tag
 */
bool carbon_query_has_tag(const Carbon_QuerySystem *sys, const char *name, const char *tag);

/**
 * Get queries with a specific tag.
 *
 * @param sys   Query system
 * @param tag   Tag to match
 * @param names Output array of query names
 * @param max   Maximum names to return
 * @return Number of matching queries
 */
int carbon_query_get_by_tag(const Carbon_QuerySystem *sys,
                             const char *tag,
                             const char **names,
                             int max);

/*============================================================================
 * Callbacks
 *============================================================================*/

/**
 * Set invalidation callback.
 * Called when any query cache is invalidated.
 *
 * @param sys      Query system
 * @param callback Callback function (NULL to clear)
 * @param userdata User context
 */
void carbon_query_set_invalidate_callback(Carbon_QuerySystem *sys,
                                            Carbon_QueryInvalidateCallback callback,
                                            void *userdata);

/*============================================================================
 * Parameter Builders
 *============================================================================*/

/**
 * Initialize empty query parameters.
 *
 * @param params Parameters to initialize
 */
void carbon_query_params_init(Carbon_QueryParams *params);

/**
 * Clear query parameters.
 *
 * @param params Parameters to clear
 */
void carbon_query_params_clear(Carbon_QueryParams *params);

/**
 * Add integer parameter.
 *
 * @param params Parameters
 * @param value  Integer value
 * @return true if added
 */
bool carbon_query_params_add_int(Carbon_QueryParams *params, int32_t value);

/**
 * Add 64-bit integer parameter.
 *
 * @param params Parameters
 * @param value  Integer value
 * @return true if added
 */
bool carbon_query_params_add_int64(Carbon_QueryParams *params, int64_t value);

/**
 * Add float parameter.
 *
 * @param params Parameters
 * @param value  Float value
 * @return true if added
 */
bool carbon_query_params_add_float(Carbon_QueryParams *params, float value);

/**
 * Add double parameter.
 *
 * @param params Parameters
 * @param value  Double value
 * @return true if added
 */
bool carbon_query_params_add_double(Carbon_QueryParams *params, double value);

/**
 * Add boolean parameter.
 *
 * @param params Parameters
 * @param value  Boolean value
 * @return true if added
 */
bool carbon_query_params_add_bool(Carbon_QueryParams *params, bool value);

/**
 * Add string parameter.
 *
 * @param params Parameters
 * @param value  String value (copied)
 * @return true if added
 */
bool carbon_query_params_add_string(Carbon_QueryParams *params, const char *value);

/**
 * Add pointer parameter.
 *
 * @param params Parameters
 * @param value  Pointer value
 * @return true if added
 */
bool carbon_query_params_add_ptr(Carbon_QueryParams *params, void *value);

/**
 * Add entity parameter.
 *
 * @param params Parameters
 * @param entity Entity ID
 * @return true if added
 */
bool carbon_query_params_add_entity(Carbon_QueryParams *params, uint32_t entity);

/**
 * Add point parameter.
 *
 * @param params Parameters
 * @param x      X coordinate
 * @param y      Y coordinate
 * @return true if added
 */
bool carbon_query_params_add_point(Carbon_QueryParams *params, int32_t x, int32_t y);

/**
 * Add rectangle parameter.
 *
 * @param params Parameters
 * @param x      Rectangle X
 * @param y      Rectangle Y
 * @param w      Rectangle width
 * @param h      Rectangle height
 * @return true if added
 */
bool carbon_query_params_add_rect(Carbon_QueryParams *params,
                                   int32_t x, int32_t y, int32_t w, int32_t h);

/*============================================================================
 * Parameter Getters
 *============================================================================*/

/**
 * Get integer parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Value or 0 if invalid
 */
int32_t carbon_query_params_get_int(const Carbon_QueryParams *params, int index);

/**
 * Get 64-bit integer parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Value or 0 if invalid
 */
int64_t carbon_query_params_get_int64(const Carbon_QueryParams *params, int index);

/**
 * Get float parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Value or 0.0f if invalid
 */
float carbon_query_params_get_float(const Carbon_QueryParams *params, int index);

/**
 * Get double parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Value or 0.0 if invalid
 */
double carbon_query_params_get_double(const Carbon_QueryParams *params, int index);

/**
 * Get boolean parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Value or false if invalid
 */
bool carbon_query_params_get_bool(const Carbon_QueryParams *params, int index);

/**
 * Get string parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return String or NULL if invalid
 */
const char *carbon_query_params_get_string(const Carbon_QueryParams *params, int index);

/**
 * Get pointer parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Pointer or NULL if invalid
 */
void *carbon_query_params_get_ptr(const Carbon_QueryParams *params, int index);

/**
 * Get entity parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @return Entity ID or 0 if invalid
 */
uint32_t carbon_query_params_get_entity(const Carbon_QueryParams *params, int index);

/**
 * Get point parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @param x      Output X (NULL to skip)
 * @param y      Output Y (NULL to skip)
 * @return true if valid
 */
bool carbon_query_params_get_point(const Carbon_QueryParams *params, int index,
                                    int32_t *x, int32_t *y);

/**
 * Get rectangle parameter by index.
 *
 * @param params Parameters
 * @param index  Parameter index
 * @param x      Output X (NULL to skip)
 * @param y      Output Y (NULL to skip)
 * @param w      Output width (NULL to skip)
 * @param h      Output height (NULL to skip)
 * @return true if valid
 */
bool carbon_query_params_get_rect(const Carbon_QueryParams *params, int index,
                                   int32_t *x, int32_t *y, int32_t *w, int32_t *h);

/*============================================================================
 * Statistics
 *============================================================================*/

/**
 * Query system statistics.
 */
typedef struct Carbon_QueryStats {
    int registered_count;         /* Number of registered queries */
    int cached_count;             /* Number of queries with caching enabled */
    uint32_t total_executions;    /* Total query executions */
    uint32_t total_cache_hits;    /* Total cache hits */
    uint32_t total_cache_misses;  /* Total cache misses */
    uint32_t total_failures;      /* Total query failures */
} Carbon_QueryStats;

/**
 * Get query system statistics.
 *
 * @param sys   Query system
 * @param stats Output statistics
 */
void carbon_query_get_stats(const Carbon_QuerySystem *sys, Carbon_QueryStats *stats);

/**
 * Reset all statistics.
 *
 * @param sys Query system
 */
void carbon_query_reset_stats(Carbon_QuerySystem *sys);

/*============================================================================
 * Utility Functions
 *============================================================================*/

/**
 * Get status string.
 *
 * @param status Query status
 * @return Status name string
 */
const char *carbon_query_status_name(Carbon_QueryStatus status);

/**
 * Check if status indicates success.
 *
 * @param status Query status
 * @return true if OK or CACHE_HIT
 */
static inline bool carbon_query_status_ok(Carbon_QueryStatus status) {
    return status == CARBON_QUERY_OK || status == CARBON_QUERY_CACHE_HIT;
}

/**
 * Hash parameters for cache key generation.
 *
 * @param params Parameters to hash
 * @return Hash value
 */
uint64_t carbon_query_hash_params(const Carbon_QueryParams *params);

#ifdef __cplusplus
}
#endif

#endif /* CARBON_QUERY_H */
