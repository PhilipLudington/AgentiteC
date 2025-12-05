/**
 * Carbon Game Query API Implementation
 *
 * Read-only state queries with structured results for clean UI integration.
 */

#include "carbon/carbon.h"
#include "carbon/query.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/*============================================================================
 * Constants
 *============================================================================*/

#define MAX_TAGS_PER_QUERY  8
#define MAX_SUBSCRIPTIONS   16
#define TAG_MAX_LEN         32

/*============================================================================
 * Internal Structures
 *============================================================================*/

/**
 * Cache entry.
 */
typedef struct QueryCacheEntry {
    uint64_t cache_key;             /* Parameter hash */
    uint32_t timestamp;             /* When cached */
    bool valid;                     /* Entry is valid */
    uint8_t data[CARBON_QUERY_MAX_RESULT_SIZE];  /* Cached result */
} QueryCacheEntry;

/**
 * Query cache.
 */
typedef struct QueryCache {
    QueryCacheEntry *entries;       /* Cache entries */
    int max_entries;                /* Maximum entries */
    int count;                      /* Current count */
    uint32_t hits;                  /* Hit count */
    uint32_t misses;                /* Miss count */
    uint32_t evictions;             /* Eviction count */
} QueryCache;

/**
 * Registered query.
 */
typedef struct RegisteredQuery {
    char name[CARBON_QUERY_MAX_NAME_LEN];
    Carbon_QueryFunc query_fn;
    size_t result_size;
    void *userdata;
    bool registered;

    /* Cache */
    QueryCache cache;
    bool cache_enabled;
    Carbon_QueryCacheKeyFunc key_fn;
    void *key_userdata;

    /* Tags */
    char tags[MAX_TAGS_PER_QUERY][TAG_MAX_LEN];
    int tag_count;
} RegisteredQuery;

/**
 * Query system.
 */
struct Carbon_QuerySystem {
    RegisteredQuery queries[CARBON_QUERY_MAX_QUERIES];
    int query_count;

    /* Timestamp for cache */
    uint32_t timestamp;

    /* Invalidation callback */
    Carbon_QueryInvalidateCallback invalidate_callback;
    void *invalidate_userdata;

    /* Statistics */
    Carbon_QueryStats stats;
};

/*============================================================================
 * Hash Function (FNV-1a)
 *============================================================================*/

static uint64_t fnv1a_hash(const void *data, size_t len) {
    const uint8_t *bytes = (const uint8_t *)data;
    uint64_t hash = 14695981039346656037ULL;  /* FNV offset basis */

    for (size_t i = 0; i < len; i++) {
        hash ^= bytes[i];
        hash *= 1099511628211ULL;  /* FNV prime */
    }

    return hash;
}

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static RegisteredQuery *find_query(Carbon_QuerySystem *sys, const char *name) {
    for (int i = 0; i < sys->query_count; i++) {
        if (sys->queries[i].registered &&
            strcmp(sys->queries[i].name, name) == 0) {
            return &sys->queries[i];
        }
    }
    return NULL;
}

static RegisteredQuery *find_query_const(const Carbon_QuerySystem *sys, const char *name) {
    return find_query((Carbon_QuerySystem *)sys, name);
}

static void free_cache(QueryCache *cache) {
    if (cache->entries) {
        free(cache->entries);
        cache->entries = NULL;
    }
    cache->max_entries = 0;
    cache->count = 0;
}

static bool init_cache(QueryCache *cache, int max_entries) {
    if (max_entries <= 0) {
        free_cache(cache);
        return true;
    }

    if (max_entries > CARBON_QUERY_MAX_CACHE_SIZE) {
        max_entries = CARBON_QUERY_MAX_CACHE_SIZE;
    }

    /* Realloc if different size */
    if (cache->max_entries != max_entries) {
        free_cache(cache);
        cache->entries = CARBON_ALLOC_ARRAY(QueryCacheEntry, max_entries);
        if (!cache->entries) {
            return false;
        }
        cache->max_entries = max_entries;
    }

    cache->count = 0;
    cache->hits = 0;
    cache->misses = 0;
    cache->evictions = 0;

    return true;
}

static QueryCacheEntry *cache_lookup(QueryCache *cache, uint64_t key) {
    if (!cache->entries) return NULL;

    for (int i = 0; i < cache->count; i++) {
        if (cache->entries[i].valid && cache->entries[i].cache_key == key) {
            return &cache->entries[i];
        }
    }
    return NULL;
}

static QueryCacheEntry *cache_get_slot(QueryCache *cache) {
    if (!cache->entries) return NULL;

    /* Find empty slot or oldest entry */
    if (cache->count < cache->max_entries) {
        return &cache->entries[cache->count++];
    }

    /* Evict oldest (simple FIFO for now) */
    QueryCacheEntry *oldest = &cache->entries[0];
    uint32_t oldest_time = oldest->timestamp;

    for (int i = 1; i < cache->count; i++) {
        if (cache->entries[i].timestamp < oldest_time) {
            oldest = &cache->entries[i];
            oldest_time = oldest->timestamp;
        }
    }

    cache->evictions++;
    oldest->valid = false;
    return oldest;
}

static void cache_invalidate_all(QueryCache *cache) {
    if (!cache->entries) return;

    for (int i = 0; i < cache->count; i++) {
        cache->entries[i].valid = false;
    }
    cache->count = 0;
}

static uint64_t default_hash_params(const Carbon_QueryParams *params) {
    if (!params || params->count == 0) {
        return 0;
    }
    return fnv1a_hash(params->params, params->count * sizeof(Carbon_QueryParam));
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

Carbon_QuerySystem *carbon_query_create(void) {
    Carbon_QuerySystem *sys = CARBON_ALLOC(Carbon_QuerySystem);
    if (!sys) {
        carbon_set_error("carbon_query_create: allocation failed");
        return NULL;
    }

    sys->query_count = 0;
    sys->timestamp = 1;
    sys->invalidate_callback = NULL;
    sys->invalidate_userdata = NULL;
    memset(&sys->stats, 0, sizeof(sys->stats));

    return sys;
}

void carbon_query_destroy(Carbon_QuerySystem *sys) {
    if (!sys) return;

    /* Free caches */
    for (int i = 0; i < sys->query_count; i++) {
        if (sys->queries[i].registered) {
            free_cache(&sys->queries[i].cache);
        }
    }

    free(sys);
}

/*============================================================================
 * Query Registration
 *============================================================================*/

bool carbon_query_register(Carbon_QuerySystem *sys,
                            const char *name,
                            Carbon_QueryFunc query_fn,
                            size_t result_size) {
    return carbon_query_register_ex(sys, name, query_fn, result_size, NULL);
}

bool carbon_query_register_ex(Carbon_QuerySystem *sys,
                               const char *name,
                               Carbon_QueryFunc query_fn,
                               size_t result_size,
                               void *userdata) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);
    CARBON_VALIDATE_PTR_RET(query_fn, false);

    if (result_size > CARBON_QUERY_MAX_RESULT_SIZE) {
        carbon_set_error("carbon_query_register: result_size %zu exceeds max %d",
                         result_size, CARBON_QUERY_MAX_RESULT_SIZE);
        return false;
    }

    /* Check if already registered */
    if (find_query(sys, name)) {
        carbon_set_error("carbon_query_register: query '%s' already registered", name);
        return false;
    }

    /* Check capacity */
    if (sys->query_count >= CARBON_QUERY_MAX_QUERIES) {
        carbon_set_error("carbon_query_register: max queries reached");
        return false;
    }

    RegisteredQuery *q = &sys->queries[sys->query_count];
    memset(q, 0, sizeof(*q));

    strncpy(q->name, name, CARBON_QUERY_MAX_NAME_LEN - 1);
    q->query_fn = query_fn;
    q->result_size = result_size;
    q->userdata = userdata;
    q->registered = true;
    q->cache_enabled = false;
    q->key_fn = NULL;
    q->tag_count = 0;

    sys->query_count++;
    sys->stats.registered_count++;

    return true;
}

bool carbon_query_unregister(Carbon_QuerySystem *sys, const char *name) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) {
        return false;
    }

    free_cache(&q->cache);
    q->registered = false;
    sys->stats.registered_count--;

    return true;
}

bool carbon_query_is_registered(const Carbon_QuerySystem *sys, const char *name) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);
    return find_query_const(sys, name) != NULL;
}

int carbon_query_count(const Carbon_QuerySystem *sys) {
    CARBON_VALIDATE_PTR_RET(sys, 0);
    return sys->stats.registered_count;
}

int carbon_query_get_names(const Carbon_QuerySystem *sys, const char **names, int max) {
    CARBON_VALIDATE_PTR_RET(sys, 0);
    CARBON_VALIDATE_PTR_RET(names, 0);

    int count = 0;
    for (int i = 0; i < sys->query_count && count < max; i++) {
        if (sys->queries[i].registered) {
            names[count++] = sys->queries[i].name;
        }
    }
    return count;
}

/*============================================================================
 * Query Execution
 *============================================================================*/

Carbon_QueryStatus carbon_query_exec(Carbon_QuerySystem *sys,
                                       const char *name,
                                       void *game_state,
                                       const Carbon_QueryParams *params,
                                       void *result) {
    CARBON_VALIDATE_PTR_RET(sys, CARBON_QUERY_INVALID_PARAMS);
    CARBON_VALIDATE_PTR_RET(name, CARBON_QUERY_INVALID_PARAMS);
    CARBON_VALIDATE_PTR_RET(result, CARBON_QUERY_INVALID_PARAMS);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) {
        return CARBON_QUERY_NOT_FOUND;
    }

    sys->stats.total_executions++;

    /* Check cache */
    if (q->cache_enabled && q->cache.entries) {
        uint64_t cache_key = q->key_fn ?
            q->key_fn(params, q->key_userdata) :
            default_hash_params(params);

        QueryCacheEntry *entry = cache_lookup(&q->cache, cache_key);
        if (entry && entry->valid) {
            /* Cache hit */
            q->cache.hits++;
            sys->stats.total_cache_hits++;
            memcpy(result, entry->data, q->result_size);
            return CARBON_QUERY_CACHE_HIT;
        }

        q->cache.misses++;
        sys->stats.total_cache_misses++;
    }

    /* Execute query */
    Carbon_QueryStatus status = q->query_fn(game_state, params, result,
                                             q->result_size, q->userdata);

    if (status != CARBON_QUERY_OK) {
        sys->stats.total_failures++;
        return status;
    }

    /* Cache result */
    if (q->cache_enabled && q->cache.entries) {
        uint64_t cache_key = q->key_fn ?
            q->key_fn(params, q->key_userdata) :
            default_hash_params(params);

        QueryCacheEntry *slot = cache_get_slot(&q->cache);
        if (slot) {
            slot->cache_key = cache_key;
            slot->timestamp = sys->timestamp++;
            slot->valid = true;
            memcpy(slot->data, result, q->result_size);
        }
    }

    return CARBON_QUERY_OK;
}

Carbon_QueryStatus carbon_query_exec_int(Carbon_QuerySystem *sys,
                                           const char *name,
                                           void *game_state,
                                           int32_t param,
                                           void *result) {
    Carbon_QueryParams params;
    carbon_query_params_init(&params);
    carbon_query_params_add_int(&params, param);
    return carbon_query_exec(sys, name, game_state, &params, result);
}

Carbon_QueryStatus carbon_query_exec_entity(Carbon_QuerySystem *sys,
                                              const char *name,
                                              void *game_state,
                                              uint32_t entity,
                                              void *result) {
    Carbon_QueryParams params;
    carbon_query_params_init(&params);
    carbon_query_params_add_entity(&params, entity);
    return carbon_query_exec(sys, name, game_state, &params, result);
}

Carbon_QueryStatus carbon_query_exec_point(Carbon_QuerySystem *sys,
                                             const char *name,
                                             void *game_state,
                                             int32_t x, int32_t y,
                                             void *result) {
    Carbon_QueryParams params;
    carbon_query_params_init(&params);
    carbon_query_params_add_point(&params, x, y);
    return carbon_query_exec(sys, name, game_state, &params, result);
}

Carbon_QueryStatus carbon_query_exec_rect(Carbon_QuerySystem *sys,
                                            const char *name,
                                            void *game_state,
                                            int32_t x, int32_t y,
                                            int32_t w, int32_t h,
                                            void *result) {
    Carbon_QueryParams params;
    carbon_query_params_init(&params);
    carbon_query_params_add_rect(&params, x, y, w, h);
    return carbon_query_exec(sys, name, game_state, &params, result);
}

/*============================================================================
 * Caching
 *============================================================================*/

bool carbon_query_enable_cache(Carbon_QuerySystem *sys, const char *name, int max_cached) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) {
        return false;
    }

    if (max_cached <= 0) {
        carbon_query_disable_cache(sys, name);
        return true;
    }

    if (!init_cache(&q->cache, max_cached)) {
        return false;
    }

    q->cache_enabled = true;
    sys->stats.cached_count++;

    return true;
}

void carbon_query_disable_cache(Carbon_QuerySystem *sys, const char *name) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query(sys, name);
    if (!q || !q->cache_enabled) {
        return;
    }

    free_cache(&q->cache);
    q->cache_enabled = false;
    sys->stats.cached_count--;
}

bool carbon_query_is_cached(const Carbon_QuerySystem *sys, const char *name) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);

    RegisteredQuery *q = find_query_const(sys, name);
    return q && q->cache_enabled;
}

void carbon_query_set_cache_key_func(Carbon_QuerySystem *sys,
                                       const char *name,
                                       Carbon_QueryCacheKeyFunc key_fn,
                                       void *userdata) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) return;

    q->key_fn = key_fn;
    q->key_userdata = userdata;
}

void carbon_query_invalidate(Carbon_QuerySystem *sys, const char *name) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query(sys, name);
    if (!q || !q->cache_enabled) {
        return;
    }

    cache_invalidate_all(&q->cache);

    if (sys->invalidate_callback) {
        sys->invalidate_callback(sys, name, sys->invalidate_userdata);
    }
}

void carbon_query_invalidate_tag(Carbon_QuerySystem *sys, const char *tag) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(tag);

    for (int i = 0; i < sys->query_count; i++) {
        RegisteredQuery *q = &sys->queries[i];
        if (!q->registered) continue;

        for (int j = 0; j < q->tag_count; j++) {
            if (strcmp(q->tags[j], tag) == 0) {
                if (q->cache_enabled) {
                    cache_invalidate_all(&q->cache);
                }
                if (sys->invalidate_callback) {
                    sys->invalidate_callback(sys, q->name, sys->invalidate_userdata);
                }
                break;
            }
        }
    }
}

void carbon_query_invalidate_all(Carbon_QuerySystem *sys) {
    CARBON_VALIDATE_PTR(sys);

    for (int i = 0; i < sys->query_count; i++) {
        RegisteredQuery *q = &sys->queries[i];
        if (q->registered && q->cache_enabled) {
            cache_invalidate_all(&q->cache);
            if (sys->invalidate_callback) {
                sys->invalidate_callback(sys, q->name, sys->invalidate_userdata);
            }
        }
    }
}

void carbon_query_get_cache_stats(const Carbon_QuerySystem *sys,
                                    const char *name,
                                    uint32_t *hits,
                                    uint32_t *misses,
                                    uint32_t *evictions) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query_const(sys, name);
    if (!q) {
        if (hits) *hits = 0;
        if (misses) *misses = 0;
        if (evictions) *evictions = 0;
        return;
    }

    if (hits) *hits = q->cache.hits;
    if (misses) *misses = q->cache.misses;
    if (evictions) *evictions = q->cache.evictions;
}

void carbon_query_clear_cache_stats(Carbon_QuerySystem *sys, const char *name) {
    CARBON_VALIDATE_PTR(sys);

    if (name) {
        RegisteredQuery *q = find_query(sys, name);
        if (q) {
            q->cache.hits = 0;
            q->cache.misses = 0;
            q->cache.evictions = 0;
        }
    } else {
        for (int i = 0; i < sys->query_count; i++) {
            RegisteredQuery *q = &sys->queries[i];
            if (q->registered) {
                q->cache.hits = 0;
                q->cache.misses = 0;
                q->cache.evictions = 0;
            }
        }
    }
}

/*============================================================================
 * Query Tags
 *============================================================================*/

bool carbon_query_add_tag(Carbon_QuerySystem *sys, const char *name, const char *tag) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);
    CARBON_VALIDATE_PTR_RET(tag, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) return false;

    /* Check if tag already exists */
    for (int i = 0; i < q->tag_count; i++) {
        if (strcmp(q->tags[i], tag) == 0) {
            return true;  /* Already has tag */
        }
    }

    if (q->tag_count >= MAX_TAGS_PER_QUERY) {
        return false;
    }

    strncpy(q->tags[q->tag_count], tag, TAG_MAX_LEN - 1);
    q->tag_count++;

    return true;
}

bool carbon_query_remove_tag(Carbon_QuerySystem *sys, const char *name, const char *tag) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);
    CARBON_VALIDATE_PTR_RET(tag, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) return false;

    for (int i = 0; i < q->tag_count; i++) {
        if (strcmp(q->tags[i], tag) == 0) {
            /* Remove by shifting */
            for (int j = i; j < q->tag_count - 1; j++) {
                strcpy(q->tags[j], q->tags[j + 1]);
            }
            q->tag_count--;
            return true;
        }
    }

    return false;
}

bool carbon_query_has_tag(const Carbon_QuerySystem *sys, const char *name, const char *tag) {
    CARBON_VALIDATE_PTR_RET(sys, false);
    CARBON_VALIDATE_PTR_RET(name, false);
    CARBON_VALIDATE_PTR_RET(tag, false);

    RegisteredQuery *q = find_query_const(sys, name);
    if (!q) return false;

    for (int i = 0; i < q->tag_count; i++) {
        if (strcmp(q->tags[i], tag) == 0) {
            return true;
        }
    }

    return false;
}

int carbon_query_get_by_tag(const Carbon_QuerySystem *sys,
                             const char *tag,
                             const char **names,
                             int max) {
    CARBON_VALIDATE_PTR_RET(sys, 0);
    CARBON_VALIDATE_PTR_RET(tag, 0);
    CARBON_VALIDATE_PTR_RET(names, 0);

    int count = 0;
    for (int i = 0; i < sys->query_count && count < max; i++) {
        const RegisteredQuery *q = &sys->queries[i];
        if (!q->registered) continue;

        for (int j = 0; j < q->tag_count; j++) {
            if (strcmp(q->tags[j], tag) == 0) {
                names[count++] = q->name;
                break;
            }
        }
    }

    return count;
}

/*============================================================================
 * Callbacks
 *============================================================================*/

void carbon_query_set_invalidate_callback(Carbon_QuerySystem *sys,
                                            Carbon_QueryInvalidateCallback callback,
                                            void *userdata) {
    CARBON_VALIDATE_PTR(sys);
    sys->invalidate_callback = callback;
    sys->invalidate_userdata = userdata;
}

/*============================================================================
 * Parameter Builders
 *============================================================================*/

void carbon_query_params_init(Carbon_QueryParams *params) {
    if (!params) return;
    memset(params, 0, sizeof(*params));
}

void carbon_query_params_clear(Carbon_QueryParams *params) {
    carbon_query_params_init(params);
}

bool carbon_query_params_add_int(Carbon_QueryParams *params, int32_t value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_INT;
    params->params[params->count].i32 = value;
    params->count++;
    return true;
}

bool carbon_query_params_add_int64(Carbon_QueryParams *params, int64_t value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_INT64;
    params->params[params->count].i64 = value;
    params->count++;
    return true;
}

bool carbon_query_params_add_float(Carbon_QueryParams *params, float value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_FLOAT;
    params->params[params->count].f32 = value;
    params->count++;
    return true;
}

bool carbon_query_params_add_double(Carbon_QueryParams *params, double value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_DOUBLE;
    params->params[params->count].f64 = value;
    params->count++;
    return true;
}

bool carbon_query_params_add_bool(Carbon_QueryParams *params, bool value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_BOOL;
    params->params[params->count].b = value;
    params->count++;
    return true;
}

bool carbon_query_params_add_string(Carbon_QueryParams *params, const char *value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_STRING;
    if (value) {
        strncpy(params->params[params->count].str, value, CARBON_QUERY_MAX_NAME_LEN - 1);
    } else {
        params->params[params->count].str[0] = '\0';
    }
    params->count++;
    return true;
}

bool carbon_query_params_add_ptr(Carbon_QueryParams *params, void *value) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_PTR;
    params->params[params->count].ptr = value;
    params->count++;
    return true;
}

bool carbon_query_params_add_entity(Carbon_QueryParams *params, uint32_t entity) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_ENTITY;
    params->params[params->count].entity = entity;
    params->count++;
    return true;
}

bool carbon_query_params_add_point(Carbon_QueryParams *params, int32_t x, int32_t y) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_POINT;
    params->params[params->count].point.x = x;
    params->params[params->count].point.y = y;
    params->count++;
    return true;
}

bool carbon_query_params_add_rect(Carbon_QueryParams *params,
                                   int32_t x, int32_t y, int32_t w, int32_t h) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (params->count >= CARBON_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = CARBON_QUERY_PARAM_RECT;
    params->params[params->count].rect.x = x;
    params->params[params->count].rect.y = y;
    params->params[params->count].rect.w = w;
    params->params[params->count].rect.h = h;
    params->count++;
    return true;
}

/*============================================================================
 * Parameter Getters
 *============================================================================*/

int32_t carbon_query_params_get_int(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, 0);
    if (index < 0 || index >= params->count) return 0;
    if (params->params[index].type != CARBON_QUERY_PARAM_INT) return 0;
    return params->params[index].i32;
}

int64_t carbon_query_params_get_int64(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, 0);
    if (index < 0 || index >= params->count) return 0;
    if (params->params[index].type != CARBON_QUERY_PARAM_INT64) return 0;
    return params->params[index].i64;
}

float carbon_query_params_get_float(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, 0.0f);
    if (index < 0 || index >= params->count) return 0.0f;
    if (params->params[index].type != CARBON_QUERY_PARAM_FLOAT) return 0.0f;
    return params->params[index].f32;
}

double carbon_query_params_get_double(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, 0.0);
    if (index < 0 || index >= params->count) return 0.0;
    if (params->params[index].type != CARBON_QUERY_PARAM_DOUBLE) return 0.0;
    return params->params[index].f64;
}

bool carbon_query_params_get_bool(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (index < 0 || index >= params->count) return false;
    if (params->params[index].type != CARBON_QUERY_PARAM_BOOL) return false;
    return params->params[index].b;
}

const char *carbon_query_params_get_string(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, NULL);
    if (index < 0 || index >= params->count) return NULL;
    if (params->params[index].type != CARBON_QUERY_PARAM_STRING) return NULL;
    return params->params[index].str;
}

void *carbon_query_params_get_ptr(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, NULL);
    if (index < 0 || index >= params->count) return NULL;
    if (params->params[index].type != CARBON_QUERY_PARAM_PTR) return NULL;
    return params->params[index].ptr;
}

uint32_t carbon_query_params_get_entity(const Carbon_QueryParams *params, int index) {
    CARBON_VALIDATE_PTR_RET(params, 0);
    if (index < 0 || index >= params->count) return 0;
    if (params->params[index].type != CARBON_QUERY_PARAM_ENTITY) return 0;
    return params->params[index].entity;
}

bool carbon_query_params_get_point(const Carbon_QueryParams *params, int index,
                                    int32_t *x, int32_t *y) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (index < 0 || index >= params->count) return false;
    if (params->params[index].type != CARBON_QUERY_PARAM_POINT) return false;

    if (x) *x = params->params[index].point.x;
    if (y) *y = params->params[index].point.y;
    return true;
}

bool carbon_query_params_get_rect(const Carbon_QueryParams *params, int index,
                                   int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    CARBON_VALIDATE_PTR_RET(params, false);
    if (index < 0 || index >= params->count) return false;
    if (params->params[index].type != CARBON_QUERY_PARAM_RECT) return false;

    if (x) *x = params->params[index].rect.x;
    if (y) *y = params->params[index].rect.y;
    if (w) *w = params->params[index].rect.w;
    if (h) *h = params->params[index].rect.h;
    return true;
}

/*============================================================================
 * Statistics
 *============================================================================*/

void carbon_query_get_stats(const Carbon_QuerySystem *sys, Carbon_QueryStats *stats) {
    CARBON_VALIDATE_PTR(sys);
    CARBON_VALIDATE_PTR(stats);
    *stats = sys->stats;
}

void carbon_query_reset_stats(Carbon_QuerySystem *sys) {
    CARBON_VALIDATE_PTR(sys);

    /* Preserve registered/cached counts */
    int registered = sys->stats.registered_count;
    int cached = sys->stats.cached_count;

    memset(&sys->stats, 0, sizeof(sys->stats));

    sys->stats.registered_count = registered;
    sys->stats.cached_count = cached;

    /* Also clear per-query stats */
    carbon_query_clear_cache_stats(sys, NULL);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *carbon_query_status_name(Carbon_QueryStatus status) {
    switch (status) {
        case CARBON_QUERY_OK:             return "OK";
        case CARBON_QUERY_NOT_FOUND:      return "Not Found";
        case CARBON_QUERY_INVALID_PARAMS: return "Invalid Params";
        case CARBON_QUERY_FAILED:         return "Failed";
        case CARBON_QUERY_NO_RESULT:      return "No Result";
        case CARBON_QUERY_CACHE_HIT:      return "Cache Hit";
        default:                          return "Unknown";
    }
}

uint64_t carbon_query_hash_params(const Carbon_QueryParams *params) {
    if (!params) return 0;
    return default_hash_params(params);
}
