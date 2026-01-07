/**
 * @file spatial.c
 * @brief Spatial Hash Index implementation
 *
 * Uses open addressing with linear probing for the hash table.
 * Each bucket stores up to AGENTITE_SPATIAL_MAX_PER_CELL entities.
 */

#include "agentite/agentite.h"
#include "agentite/spatial.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Hash bucket for a single grid cell
 */
typedef struct Agentite_SpatialBucket {
    int32_t x;                                      /**< Grid X (-1 = empty) */
    int32_t y;                                      /**< Grid Y */
    uint32_t entities[AGENTITE_SPATIAL_MAX_PER_CELL]; /**< Entity IDs */
    int count;                                      /**< Number of entities */
} Agentite_SpatialBucket;

/**
 * @brief Spatial index structure
 */
struct Agentite_SpatialIndex {
    Agentite_SpatialBucket *buckets;  /**< Hash table buckets */
    int capacity;                   /**< Number of buckets */
    int occupied;                   /**< Number of occupied buckets */
    int total_entities;             /**< Total entities stored */
};

/* ============================================================================
 * Hash Function
 * ========================================================================= */

/**
 * @brief Pack x,y coordinates into a 64-bit key
 */
static inline uint64_t pack_coords(int x, int y) {
    return ((uint64_t)(uint32_t)x << 32) | (uint64_t)(uint32_t)y;
}

/**
 * @brief Hash function for grid coordinates
 *
 * Uses a variation of FNV-1a for good distribution
 */
static inline uint32_t hash_coords(int x, int y) {
    uint64_t key = pack_coords(x, y);
    /* FNV-1a hash */
    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < 8; i++) {
        hash ^= (key >> (i * 8)) & 0xFF;
        hash *= 1099511628211ULL;
    }
    return (uint32_t)(hash ^ (hash >> 32));
}

/* ============================================================================
 * Internal Functions
 * ========================================================================= */

/**
 * @brief Find bucket for coordinates
 *
 * @param index Spatial index
 * @param x Grid X
 * @param y Grid Y
 * @param create If true, create bucket if not found
 * @return Bucket pointer or NULL if not found and create=false
 */
static Agentite_SpatialBucket *find_bucket(Agentite_SpatialIndex *index, int x, int y, bool create) {
    uint32_t hash = hash_coords(x, y);
    int start = hash % index->capacity;
    int i = start;

    do {
        Agentite_SpatialBucket *bucket = &index->buckets[i];

        /* Empty bucket */
        if (bucket->x == -1 && bucket->count == 0) {
            if (create) {
                bucket->x = x;
                bucket->y = y;
                bucket->count = 0;
                index->occupied++;
                return bucket;
            }
            return NULL;
        }

        /* Found matching bucket */
        if (bucket->x == x && bucket->y == y) {
            return bucket;
        }

        /* Linear probing */
        i = (i + 1) % index->capacity;
    } while (i != start);

    /* Table is full */
    return NULL;
}

/**
 * @brief Find bucket at position (const version for queries, never creates)
 */
static const Agentite_SpatialBucket *find_bucket_const(const Agentite_SpatialIndex *index, int x, int y) {
    uint32_t hash = hash_coords(x, y);
    int start = hash % index->capacity;
    int i = start;

    do {
        const Agentite_SpatialBucket *bucket = &index->buckets[i];

        /* Empty bucket */
        if (bucket->x == -1 && bucket->count == 0) {
            return NULL;
        }

        /* Found matching bucket */
        if (bucket->x == x && bucket->y == y) {
            return bucket;
        }

        /* Linear probing */
        i = (i + 1) % index->capacity;
    } while (i != start);

    /* Table is full */
    return NULL;
}

/**
 * @brief Grow the hash table when load factor is too high
 */
static bool grow_table(Agentite_SpatialIndex *index) {
    int new_capacity = index->capacity * 2;
    Agentite_SpatialBucket *new_buckets = AGENTITE_ALLOC_ARRAY(Agentite_SpatialBucket, new_capacity);
    if (!new_buckets) {
        agentite_set_error("Spatial: Failed to allocate buckets");
        return false;
    }

    /* Mark all new buckets as empty */
    for (int i = 0; i < new_capacity; i++) {
        new_buckets[i].x = -1;
        new_buckets[i].count = 0;
    }

    /* Rehash existing entries */
    Agentite_SpatialBucket *old_buckets = index->buckets;
    int old_capacity = index->capacity;

    index->buckets = new_buckets;
    index->capacity = new_capacity;
    index->occupied = 0;

    for (int i = 0; i < old_capacity; i++) {
        Agentite_SpatialBucket *old = &old_buckets[i];
        if (old->x != -1 && old->count > 0) {
            Agentite_SpatialBucket *bucket = find_bucket(index, old->x, old->y, true);
            if (bucket) {
                memcpy(bucket->entities, old->entities, old->count * sizeof(uint32_t));
                bucket->count = old->count;
            }
        }
    }

    free(old_buckets);
    return true;
}

/* ============================================================================
 * Creation and Destruction
 * ========================================================================= */

Agentite_SpatialIndex *agentite_spatial_create(int capacity) {
    if (capacity < 16) capacity = 16;

    Agentite_SpatialIndex *index = AGENTITE_ALLOC(Agentite_SpatialIndex);
    if (!index) {
        agentite_set_error("Spatial: Failed to allocate index");
        return NULL;
    }

    index->buckets = AGENTITE_ALLOC_ARRAY(Agentite_SpatialBucket, capacity);
    if (!index->buckets) {
        agentite_set_error("Spatial: Failed to allocate buckets");
        free(index);
        return NULL;
    }

    /* Mark all buckets as empty */
    for (int i = 0; i < capacity; i++) {
        index->buckets[i].x = -1;
        index->buckets[i].count = 0;
    }

    index->capacity = capacity;
    index->occupied = 0;
    index->total_entities = 0;

    return index;
}

void agentite_spatial_destroy(Agentite_SpatialIndex *index) {
    if (!index) return;
    free(index->buckets);
    free(index);
}

void agentite_spatial_clear(Agentite_SpatialIndex *index) {
    AGENTITE_VALIDATE_PTR(index);

    for (int i = 0; i < index->capacity; i++) {
        index->buckets[i].x = -1;
        index->buckets[i].count = 0;
    }
    index->occupied = 0;
    index->total_entities = 0;
}

/* ============================================================================
 * Basic Operations
 * ========================================================================= */

bool agentite_spatial_add(Agentite_SpatialIndex *index, int x, int y, uint32_t entity_id) {
    AGENTITE_VALIDATE_PTR_RET(index, false);
    if (entity_id == AGENTITE_SPATIAL_INVALID) return false;

    /* Grow if load factor > 0.7 */
    float load = (float)index->occupied / (float)index->capacity;
    if (load > 0.7f) {
        if (!grow_table(index)) {
            return false;
        }
    }

    Agentite_SpatialBucket *bucket = find_bucket(index, x, y, true);
    if (!bucket) {
        agentite_set_error("Spatial: Hash table full");
        return false;
    }

    if (bucket->count >= AGENTITE_SPATIAL_MAX_PER_CELL) {
        agentite_set_error("Spatial: Cell full (max %d entities)", AGENTITE_SPATIAL_MAX_PER_CELL);
        return false;
    }

    bucket->entities[bucket->count++] = entity_id;
    index->total_entities++;
    return true;
}

bool agentite_spatial_remove(Agentite_SpatialIndex *index, int x, int y, uint32_t entity_id) {
    AGENTITE_VALIDATE_PTR_RET(index, false);
    if (entity_id == AGENTITE_SPATIAL_INVALID) return false;

    Agentite_SpatialBucket *bucket = find_bucket(index, x, y, false);
    if (!bucket || bucket->count == 0) {
        return false;
    }

    /* Find and remove entity */
    for (int i = 0; i < bucket->count; i++) {
        if (bucket->entities[i] == entity_id) {
            /* Swap with last and decrement count */
            bucket->entities[i] = bucket->entities[bucket->count - 1];
            bucket->count--;
            index->total_entities--;

            /* Note: We don't remove empty buckets to avoid rehashing issues
             * with linear probing. Empty buckets act as tombstones. */
            return true;
        }
    }

    return false;
}

bool agentite_spatial_move(Agentite_SpatialIndex *index,
                         int old_x, int old_y,
                         int new_x, int new_y,
                         uint32_t entity_id) {
    AGENTITE_VALIDATE_PTR_RET(index, false);
    if (entity_id == AGENTITE_SPATIAL_INVALID) return false;

    /* Same cell, no-op */
    if (old_x == new_x && old_y == new_y) {
        return true;
    }

    /* Remove from old position (may fail if not there) */
    agentite_spatial_remove(index, old_x, old_y, entity_id);

    /* Add to new position */
    return agentite_spatial_add(index, new_x, new_y, entity_id);
}

/* ============================================================================
 * Query Operations
 * ========================================================================= */

bool agentite_spatial_has(const Agentite_SpatialIndex *index, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(index, false);

    const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
    return bucket && bucket->count > 0;
}

uint32_t agentite_spatial_query(const Agentite_SpatialIndex *index, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(index, AGENTITE_SPATIAL_INVALID);

    const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
    if (!bucket || bucket->count == 0) {
        return AGENTITE_SPATIAL_INVALID;
    }

    return bucket->entities[0];
}

int agentite_spatial_query_all(const Agentite_SpatialIndex *index, int x, int y,
                             uint32_t *out_entities, int max_entities) {
    AGENTITE_VALIDATE_PTR_RET(index, 0);
    AGENTITE_VALIDATE_PTR_RET(out_entities, 0);
    if (max_entities <= 0) return 0;

    const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
    if (!bucket || bucket->count == 0) {
        return 0;
    }

    int count = bucket->count;
    if (count > max_entities) count = max_entities;

    memcpy(out_entities, bucket->entities, count * sizeof(uint32_t));
    return count;
}

int agentite_spatial_count_at(const Agentite_SpatialIndex *index, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(index, 0);

    const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
    return bucket ? bucket->count : 0;
}

bool agentite_spatial_has_entity(const Agentite_SpatialIndex *index, int x, int y, uint32_t entity_id) {
    AGENTITE_VALIDATE_PTR_RET(index, false);
    if (entity_id == AGENTITE_SPATIAL_INVALID) return false;

    const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
    if (!bucket || bucket->count == 0) {
        return false;
    }

    for (int i = 0; i < bucket->count; i++) {
        if (bucket->entities[i] == entity_id) {
            return true;
        }
    }

    return false;
}

/* ============================================================================
 * Region Queries
 * ========================================================================= */

int agentite_spatial_query_rect(const Agentite_SpatialIndex *index,
                              int x1, int y1, int x2, int y2,
                              Agentite_SpatialQueryResult *out_results, int max_results) {
    AGENTITE_VALIDATE_PTR_RET(index, 0);
    AGENTITE_VALIDATE_PTR_RET(out_results, 0);
    if (max_results <= 0) return 0;

    /* Normalize coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    int count = 0;

    for (int y = y1; y <= y2 && count < max_results; y++) {
        for (int x = x1; x <= x2 && count < max_results; x++) {
            const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
            if (bucket && bucket->count > 0) {
                for (int i = 0; i < bucket->count && count < max_results; i++) {
                    out_results[count].entity_id = bucket->entities[i];
                    out_results[count].x = x;
                    out_results[count].y = y;
                    count++;
                }
            }
        }
    }

    return count;
}

int agentite_spatial_query_radius(const Agentite_SpatialIndex *index,
                                int center_x, int center_y, int radius,
                                Agentite_SpatialQueryResult *out_results, int max_results) {
    if (radius < 0) radius = 0;

    return agentite_spatial_query_rect(index,
                                     center_x - radius, center_y - radius,
                                     center_x + radius, center_y + radius,
                                     out_results, max_results);
}

int agentite_spatial_query_circle(const Agentite_SpatialIndex *index,
                                int center_x, int center_y, int radius,
                                Agentite_SpatialQueryResult *out_results, int max_results) {
    AGENTITE_VALIDATE_PTR_RET(index, 0);
    AGENTITE_VALIDATE_PTR_RET(out_results, 0);
    if (max_results <= 0 || radius < 0) return 0;

    int radius_sq = radius * radius;
    int count = 0;

    for (int y = center_y - radius; y <= center_y + radius && count < max_results; y++) {
        for (int x = center_x - radius; x <= center_x + radius && count < max_results; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx * dx + dy * dy > radius_sq) continue;

            const Agentite_SpatialBucket *bucket = find_bucket_const(index, x, y);
            if (bucket && bucket->count > 0) {
                for (int i = 0; i < bucket->count && count < max_results; i++) {
                    out_results[count].entity_id = bucket->entities[i];
                    out_results[count].x = x;
                    out_results[count].y = y;
                    count++;
                }
            }
        }
    }

    return count;
}

/* ============================================================================
 * Iteration
 * ========================================================================= */

Agentite_SpatialIterator agentite_spatial_iter_begin(Agentite_SpatialIndex *index, int x, int y) {
    Agentite_SpatialIterator iter = {0};
    iter.index = index;
    iter.x = x;
    iter.y = y;
    iter.current = 0;
    iter.count = 0;

    if (index) {
        Agentite_SpatialBucket *bucket = find_bucket(index, x, y, false);
        if (bucket) {
            iter.count = bucket->count;
        }
    }

    return iter;
}

bool agentite_spatial_iter_valid(const Agentite_SpatialIterator *iter) {
    return iter && iter->current < iter->count;
}

uint32_t agentite_spatial_iter_get(const Agentite_SpatialIterator *iter) {
    if (!iter || iter->current >= iter->count) {
        return AGENTITE_SPATIAL_INVALID;
    }

    Agentite_SpatialBucket *bucket = find_bucket(iter->index, iter->x, iter->y, false);
    if (!bucket || iter->current >= bucket->count) {
        return AGENTITE_SPATIAL_INVALID;
    }

    return bucket->entities[iter->current];
}

void agentite_spatial_iter_next(Agentite_SpatialIterator *iter) {
    if (iter) {
        iter->current++;
    }
}

/* ============================================================================
 * Statistics
 * ========================================================================= */

int agentite_spatial_total_count(const Agentite_SpatialIndex *index) {
    AGENTITE_VALIDATE_PTR_RET(index, 0);
    return index->total_entities;
}

int agentite_spatial_occupied_cells(const Agentite_SpatialIndex *index) {
    AGENTITE_VALIDATE_PTR_RET(index, 0);

    /* Count non-empty buckets */
    int count = 0;
    for (int i = 0; i < index->capacity; i++) {
        if (index->buckets[i].count > 0) {
            count++;
        }
    }
    return count;
}

float agentite_spatial_load_factor(const Agentite_SpatialIndex *index) {
    AGENTITE_VALIDATE_PTR_RET(index, 0.0f);
    return (float)index->occupied / (float)index->capacity;
}
