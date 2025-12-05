/**
 * @file spatial.c
 * @brief Spatial Hash Index implementation
 *
 * Uses open addressing with linear probing for the hash table.
 * Each bucket stores up to CARBON_SPATIAL_MAX_PER_CELL entities.
 */

#include "carbon/carbon.h"
#include "carbon/spatial.h"
#include "carbon/error.h"
#include "carbon/validate.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * Internal Structures
 * ========================================================================= */

/**
 * @brief Hash bucket for a single grid cell
 */
typedef struct Carbon_SpatialBucket {
    int32_t x;                                      /**< Grid X (-1 = empty) */
    int32_t y;                                      /**< Grid Y */
    uint32_t entities[CARBON_SPATIAL_MAX_PER_CELL]; /**< Entity IDs */
    int count;                                      /**< Number of entities */
} Carbon_SpatialBucket;

/**
 * @brief Spatial index structure
 */
struct Carbon_SpatialIndex {
    Carbon_SpatialBucket *buckets;  /**< Hash table buckets */
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
static Carbon_SpatialBucket *find_bucket(Carbon_SpatialIndex *index, int x, int y, bool create) {
    uint32_t hash = hash_coords(x, y);
    int start = hash % index->capacity;
    int i = start;

    do {
        Carbon_SpatialBucket *bucket = &index->buckets[i];

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
 * @brief Grow the hash table when load factor is too high
 */
static bool grow_table(Carbon_SpatialIndex *index) {
    int new_capacity = index->capacity * 2;
    Carbon_SpatialBucket *new_buckets = CARBON_ALLOC_ARRAY(Carbon_SpatialBucket, new_capacity);
    if (!new_buckets) {
        carbon_set_error("Spatial: Failed to allocate buckets");
        return false;
    }

    /* Mark all new buckets as empty */
    for (int i = 0; i < new_capacity; i++) {
        new_buckets[i].x = -1;
        new_buckets[i].count = 0;
    }

    /* Rehash existing entries */
    Carbon_SpatialBucket *old_buckets = index->buckets;
    int old_capacity = index->capacity;

    index->buckets = new_buckets;
    index->capacity = new_capacity;
    index->occupied = 0;

    for (int i = 0; i < old_capacity; i++) {
        Carbon_SpatialBucket *old = &old_buckets[i];
        if (old->x != -1 && old->count > 0) {
            Carbon_SpatialBucket *bucket = find_bucket(index, old->x, old->y, true);
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

Carbon_SpatialIndex *carbon_spatial_create(int capacity) {
    if (capacity < 16) capacity = 16;

    Carbon_SpatialIndex *index = CARBON_ALLOC(Carbon_SpatialIndex);
    if (!index) {
        carbon_set_error("Spatial: Failed to allocate index");
        return NULL;
    }

    index->buckets = CARBON_ALLOC_ARRAY(Carbon_SpatialBucket, capacity);
    if (!index->buckets) {
        carbon_set_error("Spatial: Failed to allocate buckets");
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

void carbon_spatial_destroy(Carbon_SpatialIndex *index) {
    if (!index) return;
    free(index->buckets);
    free(index);
}

void carbon_spatial_clear(Carbon_SpatialIndex *index) {
    CARBON_VALIDATE_PTR(index);

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

bool carbon_spatial_add(Carbon_SpatialIndex *index, int x, int y, uint32_t entity_id) {
    CARBON_VALIDATE_PTR_RET(index, false);
    if (entity_id == CARBON_SPATIAL_INVALID) return false;

    /* Grow if load factor > 0.7 */
    float load = (float)index->occupied / (float)index->capacity;
    if (load > 0.7f) {
        if (!grow_table(index)) {
            return false;
        }
    }

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, true);
    if (!bucket) {
        carbon_set_error("Spatial: Hash table full");
        return false;
    }

    if (bucket->count >= CARBON_SPATIAL_MAX_PER_CELL) {
        carbon_set_error("Spatial: Cell full (max %d entities)", CARBON_SPATIAL_MAX_PER_CELL);
        return false;
    }

    bucket->entities[bucket->count++] = entity_id;
    index->total_entities++;
    return true;
}

bool carbon_spatial_remove(Carbon_SpatialIndex *index, int x, int y, uint32_t entity_id) {
    CARBON_VALIDATE_PTR_RET(index, false);
    if (entity_id == CARBON_SPATIAL_INVALID) return false;

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
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

bool carbon_spatial_move(Carbon_SpatialIndex *index,
                         int old_x, int old_y,
                         int new_x, int new_y,
                         uint32_t entity_id) {
    CARBON_VALIDATE_PTR_RET(index, false);
    if (entity_id == CARBON_SPATIAL_INVALID) return false;

    /* Same cell, no-op */
    if (old_x == new_x && old_y == new_y) {
        return true;
    }

    /* Remove from old position (may fail if not there) */
    carbon_spatial_remove(index, old_x, old_y, entity_id);

    /* Add to new position */
    return carbon_spatial_add(index, new_x, new_y, entity_id);
}

/* ============================================================================
 * Query Operations
 * ========================================================================= */

bool carbon_spatial_has(Carbon_SpatialIndex *index, int x, int y) {
    CARBON_VALIDATE_PTR_RET(index, false);

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
    return bucket && bucket->count > 0;
}

uint32_t carbon_spatial_query(Carbon_SpatialIndex *index, int x, int y) {
    CARBON_VALIDATE_PTR_RET(index, CARBON_SPATIAL_INVALID);

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
    if (!bucket || bucket->count == 0) {
        return CARBON_SPATIAL_INVALID;
    }

    return bucket->entities[0];
}

int carbon_spatial_query_all(Carbon_SpatialIndex *index, int x, int y,
                             uint32_t *out_entities, int max_entities) {
    CARBON_VALIDATE_PTR_RET(index, 0);
    CARBON_VALIDATE_PTR_RET(out_entities, 0);
    if (max_entities <= 0) return 0;

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
    if (!bucket || bucket->count == 0) {
        return 0;
    }

    int count = bucket->count;
    if (count > max_entities) count = max_entities;

    memcpy(out_entities, bucket->entities, count * sizeof(uint32_t));
    return count;
}

int carbon_spatial_count_at(Carbon_SpatialIndex *index, int x, int y) {
    CARBON_VALIDATE_PTR_RET(index, 0);

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
    return bucket ? bucket->count : 0;
}

bool carbon_spatial_has_entity(Carbon_SpatialIndex *index, int x, int y, uint32_t entity_id) {
    CARBON_VALIDATE_PTR_RET(index, false);
    if (entity_id == CARBON_SPATIAL_INVALID) return false;

    Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
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

int carbon_spatial_query_rect(Carbon_SpatialIndex *index,
                              int x1, int y1, int x2, int y2,
                              Carbon_SpatialQueryResult *out_results, int max_results) {
    CARBON_VALIDATE_PTR_RET(index, 0);
    CARBON_VALIDATE_PTR_RET(out_results, 0);
    if (max_results <= 0) return 0;

    /* Normalize coordinates */
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }

    int count = 0;

    for (int y = y1; y <= y2 && count < max_results; y++) {
        for (int x = x1; x <= x2 && count < max_results; x++) {
            Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
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

int carbon_spatial_query_radius(Carbon_SpatialIndex *index,
                                int center_x, int center_y, int radius,
                                Carbon_SpatialQueryResult *out_results, int max_results) {
    if (radius < 0) radius = 0;

    return carbon_spatial_query_rect(index,
                                     center_x - radius, center_y - radius,
                                     center_x + radius, center_y + radius,
                                     out_results, max_results);
}

int carbon_spatial_query_circle(Carbon_SpatialIndex *index,
                                int center_x, int center_y, int radius,
                                Carbon_SpatialQueryResult *out_results, int max_results) {
    CARBON_VALIDATE_PTR_RET(index, 0);
    CARBON_VALIDATE_PTR_RET(out_results, 0);
    if (max_results <= 0 || radius < 0) return 0;

    int radius_sq = radius * radius;
    int count = 0;

    for (int y = center_y - radius; y <= center_y + radius && count < max_results; y++) {
        for (int x = center_x - radius; x <= center_x + radius && count < max_results; x++) {
            int dx = x - center_x;
            int dy = y - center_y;
            if (dx * dx + dy * dy > radius_sq) continue;

            Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
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

Carbon_SpatialIterator carbon_spatial_iter_begin(Carbon_SpatialIndex *index, int x, int y) {
    Carbon_SpatialIterator iter = {0};
    iter.index = index;
    iter.x = x;
    iter.y = y;
    iter.current = 0;
    iter.count = 0;

    if (index) {
        Carbon_SpatialBucket *bucket = find_bucket(index, x, y, false);
        if (bucket) {
            iter.count = bucket->count;
        }
    }

    return iter;
}

bool carbon_spatial_iter_valid(const Carbon_SpatialIterator *iter) {
    return iter && iter->current < iter->count;
}

uint32_t carbon_spatial_iter_get(const Carbon_SpatialIterator *iter) {
    if (!iter || iter->current >= iter->count) {
        return CARBON_SPATIAL_INVALID;
    }

    Carbon_SpatialBucket *bucket = find_bucket(iter->index, iter->x, iter->y, false);
    if (!bucket || iter->current >= bucket->count) {
        return CARBON_SPATIAL_INVALID;
    }

    return bucket->entities[iter->current];
}

void carbon_spatial_iter_next(Carbon_SpatialIterator *iter) {
    if (iter) {
        iter->current++;
    }
}

/* ============================================================================
 * Statistics
 * ========================================================================= */

int carbon_spatial_total_count(Carbon_SpatialIndex *index) {
    CARBON_VALIDATE_PTR_RET(index, 0);
    return index->total_entities;
}

int carbon_spatial_occupied_cells(Carbon_SpatialIndex *index) {
    CARBON_VALIDATE_PTR_RET(index, 0);

    /* Count non-empty buckets */
    int count = 0;
    for (int i = 0; i < index->capacity; i++) {
        if (index->buckets[i].count > 0) {
            count++;
        }
    }
    return count;
}

float carbon_spatial_load_factor(Carbon_SpatialIndex *index) {
    CARBON_VALIDATE_PTR_RET(index, 0.0f);
    return (float)index->occupied / (float)index->capacity;
}
