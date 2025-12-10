/**
 * Carbon Game Query API Implementation
 *
 * Read-only state queries with structured results for clean UI integration.
 */

#include "agentite/agentite.h"
#include "agentite/query.h"
#include "agentite/error.h"
#include "agentite/validate.h"

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
    uint8_t data[AGENTITE_QUERY_MAX_RESULT_SIZE];  /* Cached result */
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
    char name[AGENTITE_QUERY_MAX_NAME_LEN];
    Agentite_QueryFunc query_fn;
    size_t result_size;
    void *userdata;
    bool registered;

    /* Cache */
    QueryCache cache;
    bool cache_enabled;
    Agentite_QueryCacheKeyFunc key_fn;
    void *key_userdata;

    /* Tags */
    char tags[MAX_TAGS_PER_QUERY][TAG_MAX_LEN];
    int tag_count;
} RegisteredQuery;

/**
 * Query system.
 */
struct Agentite_QuerySystem {
    RegisteredQuery queries[AGENTITE_QUERY_MAX_QUERIES];
    int query_count;

    /* Timestamp for cache */
    uint32_t timestamp;

    /* Invalidation callback */
    Agentite_QueryInvalidateCallback invalidate_callback;
    void *invalidate_userdata;

    /* Statistics */
    Agentite_QueryStats stats;
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

static RegisteredQuery *find_query(Agentite_QuerySystem *sys, const char *name) {
    for (int i = 0; i < sys->query_count; i++) {
        if (sys->queries[i].registered &&
            strcmp(sys->queries[i].name, name) == 0) {
            return &sys->queries[i];
        }
    }
    return NULL;
}

static RegisteredQuery *find_query_const(const Agentite_QuerySystem *sys, const char *name) {
    return find_query((Agentite_QuerySystem *)sys, name);
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

    if (max_entries > AGENTITE_QUERY_MAX_CACHE_SIZE) {
        max_entries = AGENTITE_QUERY_MAX_CACHE_SIZE;
    }

    /* Realloc if different size */
    if (cache->max_entries != max_entries) {
        free_cache(cache);
        cache->entries = AGENTITE_ALLOC_ARRAY(QueryCacheEntry, max_entries);
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

static uint64_t default_hash_params(const Agentite_QueryParams *params) {
    if (!params || params->count == 0) {
        return 0;
    }
    return fnv1a_hash(params->params, params->count * sizeof(Agentite_QueryParam));
}

/*============================================================================
 * Lifecycle
 *============================================================================*/

Agentite_QuerySystem *agentite_query_create(void) {
    Agentite_QuerySystem *sys = AGENTITE_ALLOC(Agentite_QuerySystem);
    if (!sys) {
        agentite_set_error("agentite_query_create: allocation failed");
        return NULL;
    }

    sys->query_count = 0;
    sys->timestamp = 1;
    sys->invalidate_callback = NULL;
    sys->invalidate_userdata = NULL;
    memset(&sys->stats, 0, sizeof(sys->stats));

    return sys;
}

void agentite_query_destroy(Agentite_QuerySystem *sys) {
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

bool agentite_query_register(Agentite_QuerySystem *sys,
                            const char *name,
                            Agentite_QueryFunc query_fn,
                            size_t result_size) {
    return agentite_query_register_ex(sys, name, query_fn, result_size, NULL);
}

bool agentite_query_register_ex(Agentite_QuerySystem *sys,
                               const char *name,
                               Agentite_QueryFunc query_fn,
                               size_t result_size,
                               void *userdata) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);
    AGENTITE_VALIDATE_PTR_RET(query_fn, false);

    if (result_size > AGENTITE_QUERY_MAX_RESULT_SIZE) {
        agentite_set_error("agentite_query_register: result_size %zu exceeds max %d",
                         result_size, AGENTITE_QUERY_MAX_RESULT_SIZE);
        return false;
    }

    /* Check if already registered */
    if (find_query(sys, name)) {
        agentite_set_error("agentite_query_register: query '%s' already registered", name);
        return false;
    }

    /* Check capacity */
    if (sys->query_count >= AGENTITE_QUERY_MAX_QUERIES) {
        agentite_set_error("agentite_query_register: max queries reached");
        return false;
    }

    RegisteredQuery *q = &sys->queries[sys->query_count];
    memset(q, 0, sizeof(*q));

    strncpy(q->name, name, AGENTITE_QUERY_MAX_NAME_LEN - 1);
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

bool agentite_query_unregister(Agentite_QuerySystem *sys, const char *name) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) {
        return false;
    }

    free_cache(&q->cache);
    q->registered = false;
    sys->stats.registered_count--;

    return true;
}

bool agentite_query_is_registered(const Agentite_QuerySystem *sys, const char *name) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);
    return find_query_const(sys, name) != NULL;
}

int agentite_query_count(const Agentite_QuerySystem *sys) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);
    return sys->stats.registered_count;
}

int agentite_query_get_names(const Agentite_QuerySystem *sys, const char **names, int max) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);
    AGENTITE_VALIDATE_PTR_RET(names, 0);

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

Agentite_QueryStatus agentite_query_exec(Agentite_QuerySystem *sys,
                                       const char *name,
                                       void *game_state,
                                       const Agentite_QueryParams *params,
                                       void *result) {
    AGENTITE_VALIDATE_PTR_RET(sys, AGENTITE_QUERY_INVALID_PARAMS);
    AGENTITE_VALIDATE_PTR_RET(name, AGENTITE_QUERY_INVALID_PARAMS);
    AGENTITE_VALIDATE_PTR_RET(result, AGENTITE_QUERY_INVALID_PARAMS);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) {
        return AGENTITE_QUERY_NOT_FOUND;
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
            return AGENTITE_QUERY_CACHE_HIT;
        }

        q->cache.misses++;
        sys->stats.total_cache_misses++;
    }

    /* Execute query */
    Agentite_QueryStatus status = q->query_fn(game_state, params, result,
                                             q->result_size, q->userdata);

    if (status != AGENTITE_QUERY_OK) {
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

    return AGENTITE_QUERY_OK;
}

Agentite_QueryStatus agentite_query_exec_int(Agentite_QuerySystem *sys,
                                           const char *name,
                                           void *game_state,
                                           int32_t param,
                                           void *result) {
    Agentite_QueryParams params;
    agentite_query_params_init(&params);
    agentite_query_params_add_int(&params, param);
    return agentite_query_exec(sys, name, game_state, &params, result);
}

Agentite_QueryStatus agentite_query_exec_entity(Agentite_QuerySystem *sys,
                                              const char *name,
                                              void *game_state,
                                              uint32_t entity,
                                              void *result) {
    Agentite_QueryParams params;
    agentite_query_params_init(&params);
    agentite_query_params_add_entity(&params, entity);
    return agentite_query_exec(sys, name, game_state, &params, result);
}

Agentite_QueryStatus agentite_query_exec_point(Agentite_QuerySystem *sys,
                                             const char *name,
                                             void *game_state,
                                             int32_t x, int32_t y,
                                             void *result) {
    Agentite_QueryParams params;
    agentite_query_params_init(&params);
    agentite_query_params_add_point(&params, x, y);
    return agentite_query_exec(sys, name, game_state, &params, result);
}

Agentite_QueryStatus agentite_query_exec_rect(Agentite_QuerySystem *sys,
                                            const char *name,
                                            void *game_state,
                                            int32_t x, int32_t y,
                                            int32_t w, int32_t h,
                                            void *result) {
    Agentite_QueryParams params;
    agentite_query_params_init(&params);
    agentite_query_params_add_rect(&params, x, y, w, h);
    return agentite_query_exec(sys, name, game_state, &params, result);
}

/*============================================================================
 * Caching
 *============================================================================*/

bool agentite_query_enable_cache(Agentite_QuerySystem *sys, const char *name, int max_cached) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) {
        return false;
    }

    if (max_cached <= 0) {
        agentite_query_disable_cache(sys, name);
        return true;
    }

    if (!init_cache(&q->cache, max_cached)) {
        return false;
    }

    q->cache_enabled = true;
    sys->stats.cached_count++;

    return true;
}

void agentite_query_disable_cache(Agentite_QuerySystem *sys, const char *name) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query(sys, name);
    if (!q || !q->cache_enabled) {
        return;
    }

    free_cache(&q->cache);
    q->cache_enabled = false;
    sys->stats.cached_count--;
}

bool agentite_query_is_cached(const Agentite_QuerySystem *sys, const char *name) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);

    RegisteredQuery *q = find_query_const(sys, name);
    return q && q->cache_enabled;
}

void agentite_query_set_cache_key_func(Agentite_QuerySystem *sys,
                                       const char *name,
                                       Agentite_QueryCacheKeyFunc key_fn,
                                       void *userdata) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) return;

    q->key_fn = key_fn;
    q->key_userdata = userdata;
}

void agentite_query_invalidate(Agentite_QuerySystem *sys, const char *name) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(name);

    RegisteredQuery *q = find_query(sys, name);
    if (!q || !q->cache_enabled) {
        return;
    }

    cache_invalidate_all(&q->cache);

    if (sys->invalidate_callback) {
        sys->invalidate_callback(sys, name, sys->invalidate_userdata);
    }
}

void agentite_query_invalidate_tag(Agentite_QuerySystem *sys, const char *tag) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(tag);

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

void agentite_query_invalidate_all(Agentite_QuerySystem *sys) {
    AGENTITE_VALIDATE_PTR(sys);

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

void agentite_query_get_cache_stats(const Agentite_QuerySystem *sys,
                                    const char *name,
                                    uint32_t *hits,
                                    uint32_t *misses,
                                    uint32_t *evictions) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(name);

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

void agentite_query_clear_cache_stats(Agentite_QuerySystem *sys, const char *name) {
    AGENTITE_VALIDATE_PTR(sys);

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

bool agentite_query_add_tag(Agentite_QuerySystem *sys, const char *name, const char *tag) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);
    AGENTITE_VALIDATE_PTR_RET(tag, false);

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

bool agentite_query_remove_tag(Agentite_QuerySystem *sys, const char *name, const char *tag) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);
    AGENTITE_VALIDATE_PTR_RET(tag, false);

    RegisteredQuery *q = find_query(sys, name);
    if (!q) return false;

    for (int i = 0; i < q->tag_count; i++) {
        if (strcmp(q->tags[i], tag) == 0) {
            /* Remove by shifting */
            for (int j = i; j < q->tag_count - 1; j++) {
                strncpy(q->tags[j], q->tags[j + 1], TAG_MAX_LEN - 1);
                q->tags[j][TAG_MAX_LEN - 1] = '\0';
            }
            q->tag_count--;
            return true;
        }
    }

    return false;
}

bool agentite_query_has_tag(const Agentite_QuerySystem *sys, const char *name, const char *tag) {
    AGENTITE_VALIDATE_PTR_RET(sys, false);
    AGENTITE_VALIDATE_PTR_RET(name, false);
    AGENTITE_VALIDATE_PTR_RET(tag, false);

    RegisteredQuery *q = find_query_const(sys, name);
    if (!q) return false;

    for (int i = 0; i < q->tag_count; i++) {
        if (strcmp(q->tags[i], tag) == 0) {
            return true;
        }
    }

    return false;
}

int agentite_query_get_by_tag(const Agentite_QuerySystem *sys,
                             const char *tag,
                             const char **names,
                             int max) {
    AGENTITE_VALIDATE_PTR_RET(sys, 0);
    AGENTITE_VALIDATE_PTR_RET(tag, 0);
    AGENTITE_VALIDATE_PTR_RET(names, 0);

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

void agentite_query_set_invalidate_callback(Agentite_QuerySystem *sys,
                                            Agentite_QueryInvalidateCallback callback,
                                            void *userdata) {
    AGENTITE_VALIDATE_PTR(sys);
    sys->invalidate_callback = callback;
    sys->invalidate_userdata = userdata;
}

/*============================================================================
 * Parameter Builders
 *============================================================================*/

void agentite_query_params_init(Agentite_QueryParams *params) {
    if (!params) return;
    memset(params, 0, sizeof(*params));
}

void agentite_query_params_clear(Agentite_QueryParams *params) {
    agentite_query_params_init(params);
}

bool agentite_query_params_add_int(Agentite_QueryParams *params, int32_t value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_INT;
    params->params[params->count].i32 = value;
    params->count++;
    return true;
}

bool agentite_query_params_add_int64(Agentite_QueryParams *params, int64_t value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_INT64;
    params->params[params->count].i64 = value;
    params->count++;
    return true;
}

bool agentite_query_params_add_float(Agentite_QueryParams *params, float value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_FLOAT;
    params->params[params->count].f32 = value;
    params->count++;
    return true;
}

bool agentite_query_params_add_double(Agentite_QueryParams *params, double value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_DOUBLE;
    params->params[params->count].f64 = value;
    params->count++;
    return true;
}

bool agentite_query_params_add_bool(Agentite_QueryParams *params, bool value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_BOOL;
    params->params[params->count].b = value;
    params->count++;
    return true;
}

bool agentite_query_params_add_string(Agentite_QueryParams *params, const char *value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_STRING;
    if (value) {
        strncpy(params->params[params->count].str, value, AGENTITE_QUERY_MAX_NAME_LEN - 1);
    } else {
        params->params[params->count].str[0] = '\0';
    }
    params->count++;
    return true;
}

bool agentite_query_params_add_ptr(Agentite_QueryParams *params, void *value) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_PTR;
    params->params[params->count].ptr = value;
    params->count++;
    return true;
}

bool agentite_query_params_add_entity(Agentite_QueryParams *params, uint32_t entity) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_ENTITY;
    params->params[params->count].entity = entity;
    params->count++;
    return true;
}

bool agentite_query_params_add_point(Agentite_QueryParams *params, int32_t x, int32_t y) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_POINT;
    params->params[params->count].point.x = x;
    params->params[params->count].point.y = y;
    params->count++;
    return true;
}

bool agentite_query_params_add_rect(Agentite_QueryParams *params,
                                   int32_t x, int32_t y, int32_t w, int32_t h) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (params->count >= AGENTITE_QUERY_MAX_PARAMS) return false;

    params->params[params->count].type = AGENTITE_QUERY_PARAM_RECT;
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

int32_t agentite_query_params_get_int(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, 0);
    if (index < 0 || index >= params->count) return 0;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_INT) return 0;
    return params->params[index].i32;
}

int64_t agentite_query_params_get_int64(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, 0);
    if (index < 0 || index >= params->count) return 0;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_INT64) return 0;
    return params->params[index].i64;
}

float agentite_query_params_get_float(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, 0.0f);
    if (index < 0 || index >= params->count) return 0.0f;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_FLOAT) return 0.0f;
    return params->params[index].f32;
}

double agentite_query_params_get_double(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, 0.0);
    if (index < 0 || index >= params->count) return 0.0;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_DOUBLE) return 0.0;
    return params->params[index].f64;
}

bool agentite_query_params_get_bool(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (index < 0 || index >= params->count) return false;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_BOOL) return false;
    return params->params[index].b;
}

const char *agentite_query_params_get_string(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, NULL);
    if (index < 0 || index >= params->count) return NULL;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_STRING) return NULL;
    return params->params[index].str;
}

void *agentite_query_params_get_ptr(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, NULL);
    if (index < 0 || index >= params->count) return NULL;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_PTR) return NULL;
    return params->params[index].ptr;
}

uint32_t agentite_query_params_get_entity(const Agentite_QueryParams *params, int index) {
    AGENTITE_VALIDATE_PTR_RET(params, 0);
    if (index < 0 || index >= params->count) return 0;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_ENTITY) return 0;
    return params->params[index].entity;
}

bool agentite_query_params_get_point(const Agentite_QueryParams *params, int index,
                                    int32_t *x, int32_t *y) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (index < 0 || index >= params->count) return false;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_POINT) return false;

    if (x) *x = params->params[index].point.x;
    if (y) *y = params->params[index].point.y;
    return true;
}

bool agentite_query_params_get_rect(const Agentite_QueryParams *params, int index,
                                   int32_t *x, int32_t *y, int32_t *w, int32_t *h) {
    AGENTITE_VALIDATE_PTR_RET(params, false);
    if (index < 0 || index >= params->count) return false;
    if (params->params[index].type != AGENTITE_QUERY_PARAM_RECT) return false;

    if (x) *x = params->params[index].rect.x;
    if (y) *y = params->params[index].rect.y;
    if (w) *w = params->params[index].rect.w;
    if (h) *h = params->params[index].rect.h;
    return true;
}

/*============================================================================
 * Statistics
 *============================================================================*/

void agentite_query_get_stats(const Agentite_QuerySystem *sys, Agentite_QueryStats *stats) {
    AGENTITE_VALIDATE_PTR(sys);
    AGENTITE_VALIDATE_PTR(stats);
    *stats = sys->stats;
}

void agentite_query_reset_stats(Agentite_QuerySystem *sys) {
    AGENTITE_VALIDATE_PTR(sys);

    /* Preserve registered/cached counts */
    int registered = sys->stats.registered_count;
    int cached = sys->stats.cached_count;

    memset(&sys->stats, 0, sizeof(sys->stats));

    sys->stats.registered_count = registered;
    sys->stats.cached_count = cached;

    /* Also clear per-query stats */
    agentite_query_clear_cache_stats(sys, NULL);
}

/*============================================================================
 * Utility Functions
 *============================================================================*/

const char *agentite_query_status_name(Agentite_QueryStatus status) {
    switch (status) {
        case AGENTITE_QUERY_OK:             return "OK";
        case AGENTITE_QUERY_NOT_FOUND:      return "Not Found";
        case AGENTITE_QUERY_INVALID_PARAMS: return "Invalid Params";
        case AGENTITE_QUERY_FAILED:         return "Failed";
        case AGENTITE_QUERY_NO_RESULT:      return "No Result";
        case AGENTITE_QUERY_CACHE_HIT:      return "Cache Hit";
        default:                          return "Unknown";
    }
}

uint64_t agentite_query_hash_params(const Agentite_QueryParams *params) {
    if (!params) return 0;
    return default_hash_params(params);
}
