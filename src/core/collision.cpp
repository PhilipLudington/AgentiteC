/**
 * @file collision.cpp
 * @brief 2D Collision Detection System Implementation
 *
 * Provides shape-based collision detection with spatial hash acceleration.
 */

#include "agentite/agentite.h"
#include "agentite/collision.h"
#include "agentite/error.h"
#include "agentite/gizmos.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Constants
 * ============================================================================ */

#define COLLISION_EPSILON 0.0001f

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Circle shape data */
typedef struct CircleData {
    float radius;
} CircleData;

/* AABB shape data (half-extents from center) */
typedef struct AABBData {
    float half_width;
    float half_height;
} AABBData;

/* OBB shape data (same as AABB, but rotation is applied) */
typedef struct OBBData {
    float half_width;
    float half_height;
} OBBData;

/* Capsule shape data */
typedef struct CapsuleData {
    float radius;
    float half_length;  /* Half of center segment */
    Agentite_CapsuleAxis axis;
} CapsuleData;

/* Polygon shape data */
typedef struct PolygonData {
    Agentite_CollisionVec2 vertices[AGENTITE_COLLISION_MAX_POLYGON_VERTS];
    Agentite_CollisionVec2 normals[AGENTITE_COLLISION_MAX_POLYGON_VERTS];
    int count;
} PolygonData;

/* Shape union */
typedef union ShapeData {
    CircleData circle;
    AABBData aabb;
    OBBData obb;
    CapsuleData capsule;
    PolygonData polygon;
} ShapeData;

/* Collision shape structure */
struct Agentite_CollisionShape {
    Agentite_ShapeType type;
    ShapeData data;
};

/* Single collider in the world */
typedef struct Collider {
    bool active;
    Agentite_CollisionShape *shape;  /* Borrowed reference */
    float x, y;                      /* Position */
    float rotation;                  /* Rotation in radians */
    uint32_t layer;                  /* What layer this belongs to */
    uint32_t mask;                   /* What layers to collide with */
    bool enabled;                    /* Is collision active */
    void *user_data;                 /* Game-specific data */
    Agentite_AABB cached_aabb;       /* Cached world-space AABB */
    bool aabb_dirty;                 /* Needs AABB recalculation */
} Collider;

/* Spatial hash cell */
typedef struct SpatialCell {
    int32_t cell_x, cell_y;
    Agentite_ColliderId *colliders;
    int count;
    int capacity;
} SpatialCell;

/* Spatial hash table */
typedef struct SpatialHash {
    SpatialCell *cells;
    int capacity;
    int occupied;
    float cell_size;
    float inv_cell_size;
} SpatialHash;

/* Collision world structure */
struct Agentite_CollisionWorld {
    Collider *colliders;
    uint32_t max_colliders;
    uint32_t count;
    uint32_t next_id;
    SpatialHash spatial;
};

/* ============================================================================
 * Math Helpers
 * ============================================================================ */

static inline float vec2_dot(Agentite_CollisionVec2 a, Agentite_CollisionVec2 b) {
    return a.x * b.x + a.y * b.y;
}

static inline float vec2_length_sq(Agentite_CollisionVec2 v) {
    return v.x * v.x + v.y * v.y;
}

static inline float vec2_length(Agentite_CollisionVec2 v) {
    return sqrtf(vec2_length_sq(v));
}

static inline Agentite_CollisionVec2 vec2_normalize(Agentite_CollisionVec2 v) {
    float len = vec2_length(v);
    if (len < COLLISION_EPSILON) return (Agentite_CollisionVec2){0.0f, 0.0f};
    return (Agentite_CollisionVec2){v.x / len, v.y / len};
}

static inline Agentite_CollisionVec2 vec2_sub(Agentite_CollisionVec2 a, Agentite_CollisionVec2 b) {
    return (Agentite_CollisionVec2){a.x - b.x, a.y - b.y};
}

static inline Agentite_CollisionVec2 vec2_add(Agentite_CollisionVec2 a, Agentite_CollisionVec2 b) {
    return (Agentite_CollisionVec2){a.x + b.x, a.y + b.y};
}

static inline Agentite_CollisionVec2 vec2_scale(Agentite_CollisionVec2 v, float s) {
    return (Agentite_CollisionVec2){v.x * s, v.y * s};
}

static inline Agentite_CollisionVec2 vec2_neg(Agentite_CollisionVec2 v) {
    return (Agentite_CollisionVec2){-v.x, -v.y};
}

static inline Agentite_CollisionVec2 vec2_perp(Agentite_CollisionVec2 v) {
    return (Agentite_CollisionVec2){-v.y, v.x};
}

static inline float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

static inline float minf(float a, float b) { return a < b ? a : b; }
static inline float maxf(float a, float b) { return a > b ? a : b; }
static inline float absf(float a) { return a < 0 ? -a : a; }

/* Rotate a point around origin */
static inline Agentite_CollisionVec2 vec2_rotate(Agentite_CollisionVec2 v, float cos_r, float sin_r) {
    return (Agentite_CollisionVec2){
        v.x * cos_r - v.y * sin_r,
        v.x * sin_r + v.y * cos_r
    };
}

/* ============================================================================
 * Spatial Hash Implementation
 * ============================================================================ */

static uint32_t hash_cell(int32_t x, int32_t y) {
    uint64_t key = ((uint64_t)(uint32_t)x << 32) | (uint64_t)(uint32_t)y;
    uint64_t hash = 14695981039346656037ULL;
    for (int i = 0; i < 8; i++) {
        hash ^= (key >> (i * 8)) & 0xFF;
        hash *= 1099511628211ULL;
    }
    return (uint32_t)(hash ^ (hash >> 32));
}

static SpatialCell *spatial_find_cell(SpatialHash *spatial, int32_t cx, int32_t cy, bool create) {
    uint32_t hash = hash_cell(cx, cy);
    int start = hash % spatial->capacity;
    int i = start;

    do {
        SpatialCell *cell = &spatial->cells[i];

        /* Empty cell */
        if (cell->colliders == NULL && cell->count == 0) {
            if (create) {
                cell->cell_x = cx;
                cell->cell_y = cy;
                cell->capacity = 8;
                cell->colliders = (Agentite_ColliderId*)calloc(cell->capacity, sizeof(Agentite_ColliderId));
                cell->count = 0;
                spatial->occupied++;
                return cell;
            }
            return NULL;
        }

        /* Found matching cell */
        if (cell->cell_x == cx && cell->cell_y == cy) {
            return cell;
        }

        i = (i + 1) % spatial->capacity;
    } while (i != start);

    return NULL;
}

static bool spatial_init(SpatialHash *spatial, int capacity, float cell_size) {
    spatial->capacity = capacity;
    spatial->occupied = 0;
    spatial->cell_size = cell_size;
    spatial->inv_cell_size = 1.0f / cell_size;
    spatial->cells = (SpatialCell*)calloc(capacity, sizeof(SpatialCell));
    return spatial->cells != NULL;
}

static void spatial_destroy(SpatialHash *spatial) {
    if (!spatial->cells) return;
    for (int i = 0; i < spatial->capacity; i++) {
        free(spatial->cells[i].colliders);
    }
    free(spatial->cells);
    spatial->cells = NULL;
}

static void spatial_clear(SpatialHash *spatial) {
    for (int i = 0; i < spatial->capacity; i++) {
        spatial->cells[i].count = 0;
    }
}

static bool spatial_grow(SpatialHash *spatial) {
    int new_capacity = spatial->capacity * 2;
    SpatialCell *new_cells = (SpatialCell*)calloc(new_capacity, sizeof(SpatialCell));
    if (!new_cells) return false;

    SpatialCell *old_cells = spatial->cells;
    int old_capacity = spatial->capacity;

    spatial->cells = new_cells;
    spatial->capacity = new_capacity;
    spatial->occupied = 0;

    /* Rehash */
    for (int i = 0; i < old_capacity; i++) {
        SpatialCell *old = &old_cells[i];
        if (old->colliders && old->count > 0) {
            SpatialCell *cell = spatial_find_cell(spatial, old->cell_x, old->cell_y, true);
            if (cell) {
                free(cell->colliders);
                cell->colliders = old->colliders;
                cell->count = old->count;
                cell->capacity = old->capacity;
            }
        } else {
            free(old->colliders);
        }
    }

    free(old_cells);
    return true;
}

static void spatial_add(SpatialHash *spatial, int32_t cx, int32_t cy, Agentite_ColliderId id) {
    if ((float)spatial->occupied / (float)spatial->capacity > 0.7f) {
        spatial_grow(spatial);
    }

    SpatialCell *cell = spatial_find_cell(spatial, cx, cy, true);
    if (!cell) return;

    /* Grow cell array if needed */
    if (cell->count >= cell->capacity) {
        int new_cap = cell->capacity * 2;
        Agentite_ColliderId *new_arr = (Agentite_ColliderId*)realloc(cell->colliders, new_cap * sizeof(Agentite_ColliderId));
        if (!new_arr) return;
        cell->colliders = new_arr;
        cell->capacity = new_cap;
    }

    cell->colliders[cell->count++] = id;
}

static void spatial_remove(SpatialHash *spatial, int32_t cx, int32_t cy, Agentite_ColliderId id) {
    SpatialCell *cell = spatial_find_cell(spatial, cx, cy, false);
    if (!cell) return;

    for (int i = 0; i < cell->count; i++) {
        if (cell->colliders[i] == id) {
            cell->colliders[i] = cell->colliders[cell->count - 1];
            cell->count--;
            return;
        }
    }
}

/* Get all cell coordinates that an AABB overlaps */
static void spatial_get_cells(SpatialHash *spatial, const Agentite_AABB *aabb,
                              int32_t *out_x1, int32_t *out_y1,
                              int32_t *out_x2, int32_t *out_y2) {
    *out_x1 = (int32_t)floorf(aabb->min_x * spatial->inv_cell_size);
    *out_y1 = (int32_t)floorf(aabb->min_y * spatial->inv_cell_size);
    *out_x2 = (int32_t)floorf(aabb->max_x * spatial->inv_cell_size);
    *out_y2 = (int32_t)floorf(aabb->max_y * spatial->inv_cell_size);
}

/* ============================================================================
 * AABB Computation
 * ============================================================================ */

static void compute_shape_aabb(const Agentite_CollisionShape *shape,
                               float x, float y, float rotation,
                               Agentite_AABB *out) {
    float cos_r = cosf(rotation);
    float sin_r = sinf(rotation);

    switch (shape->type) {
        case AGENTITE_SHAPE_CIRCLE: {
            float r = shape->data.circle.radius;
            out->min_x = x - r;
            out->min_y = y - r;
            out->max_x = x + r;
            out->max_y = y + r;
            break;
        }

        case AGENTITE_SHAPE_AABB: {
            float hw = shape->data.aabb.half_width;
            float hh = shape->data.aabb.half_height;
            out->min_x = x - hw;
            out->min_y = y - hh;
            out->max_x = x + hw;
            out->max_y = y + hh;
            break;
        }

        case AGENTITE_SHAPE_OBB: {
            float hw = shape->data.obb.half_width;
            float hh = shape->data.obb.half_height;

            /* Corners of the OBB */
            Agentite_CollisionVec2 corners[4] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };

            out->min_x = out->min_y = FLT_MAX;
            out->max_x = out->max_y = -FLT_MAX;

            for (int i = 0; i < 4; i++) {
                Agentite_CollisionVec2 rotated = vec2_rotate(corners[i], cos_r, sin_r);
                float px = x + rotated.x;
                float py = y + rotated.y;
                out->min_x = minf(out->min_x, px);
                out->min_y = minf(out->min_y, py);
                out->max_x = maxf(out->max_x, px);
                out->max_y = maxf(out->max_y, py);
            }
            break;
        }

        case AGENTITE_SHAPE_CAPSULE: {
            float r = shape->data.capsule.radius;
            float hl = shape->data.capsule.half_length;
            bool x_axis = (shape->data.capsule.axis == AGENTITE_CAPSULE_X);

            /* Two end circles */
            Agentite_CollisionVec2 offset = x_axis ?
                (Agentite_CollisionVec2){hl, 0} : (Agentite_CollisionVec2){0, hl};

            offset = vec2_rotate(offset, cos_r, sin_r);

            float p1x = x + offset.x, p1y = y + offset.y;
            float p2x = x - offset.x, p2y = y - offset.y;

            out->min_x = minf(p1x, p2x) - r;
            out->min_y = minf(p1y, p2y) - r;
            out->max_x = maxf(p1x, p2x) + r;
            out->max_y = maxf(p1y, p2y) + r;
            break;
        }

        case AGENTITE_SHAPE_POLYGON: {
            const PolygonData *poly = &shape->data.polygon;
            out->min_x = out->min_y = FLT_MAX;
            out->max_x = out->max_y = -FLT_MAX;

            for (int i = 0; i < poly->count; i++) {
                Agentite_CollisionVec2 rotated = vec2_rotate(poly->vertices[i], cos_r, sin_r);
                float px = x + rotated.x;
                float py = y + rotated.y;
                out->min_x = minf(out->min_x, px);
                out->min_y = minf(out->min_y, py);
                out->max_x = maxf(out->max_x, px);
                out->max_y = maxf(out->max_y, py);
            }
            break;
        }
    }
}

/* ============================================================================
 * Shape Creation
 * ============================================================================ */

Agentite_CollisionShape *agentite_collision_shape_circle(float radius) {
    if (radius <= 0.0f) {
        agentite_set_error("Collision: Circle radius must be positive");
        return NULL;
    }

    Agentite_CollisionShape *shape = AGENTITE_ALLOC(Agentite_CollisionShape);
    if (!shape) {
        agentite_set_error("Collision: Failed to allocate circle shape");
        return NULL;
    }

    shape->type = AGENTITE_SHAPE_CIRCLE;
    shape->data.circle.radius = radius;
    return shape;
}

Agentite_CollisionShape *agentite_collision_shape_aabb(float width, float height) {
    if (width <= 0.0f || height <= 0.0f) {
        agentite_set_error("Collision: AABB dimensions must be positive");
        return NULL;
    }

    Agentite_CollisionShape *shape = AGENTITE_ALLOC(Agentite_CollisionShape);
    if (!shape) {
        agentite_set_error("Collision: Failed to allocate AABB shape");
        return NULL;
    }

    shape->type = AGENTITE_SHAPE_AABB;
    shape->data.aabb.half_width = width * 0.5f;
    shape->data.aabb.half_height = height * 0.5f;
    return shape;
}

Agentite_CollisionShape *agentite_collision_shape_obb(float width, float height) {
    if (width <= 0.0f || height <= 0.0f) {
        agentite_set_error("Collision: OBB dimensions must be positive");
        return NULL;
    }

    Agentite_CollisionShape *shape = AGENTITE_ALLOC(Agentite_CollisionShape);
    if (!shape) {
        agentite_set_error("Collision: Failed to allocate OBB shape");
        return NULL;
    }

    shape->type = AGENTITE_SHAPE_OBB;
    shape->data.obb.half_width = width * 0.5f;
    shape->data.obb.half_height = height * 0.5f;
    return shape;
}

Agentite_CollisionShape *agentite_collision_shape_capsule(
    float radius, float length, Agentite_CapsuleAxis axis)
{
    if (radius <= 0.0f) {
        agentite_set_error("Collision: Capsule radius must be positive");
        return NULL;
    }

    Agentite_CollisionShape *shape = AGENTITE_ALLOC(Agentite_CollisionShape);
    if (!shape) {
        agentite_set_error("Collision: Failed to allocate capsule shape");
        return NULL;
    }

    shape->type = AGENTITE_SHAPE_CAPSULE;
    shape->data.capsule.radius = radius;
    shape->data.capsule.half_length = length * 0.5f;
    shape->data.capsule.axis = axis;
    return shape;
}

Agentite_CollisionShape *agentite_collision_shape_polygon(
    const Agentite_CollisionVec2 *vertices, int count)
{
    if (count < 3 || count > AGENTITE_COLLISION_MAX_POLYGON_VERTS) {
        agentite_set_error("Collision: Polygon must have 3-%d vertices", AGENTITE_COLLISION_MAX_POLYGON_VERTS);
        return NULL;
    }

    if (!vertices) {
        agentite_set_error("Collision: Vertices array is NULL");
        return NULL;
    }

    Agentite_CollisionShape *shape = AGENTITE_ALLOC(Agentite_CollisionShape);
    if (!shape) {
        agentite_set_error("Collision: Failed to allocate polygon shape");
        return NULL;
    }

    shape->type = AGENTITE_SHAPE_POLYGON;
    PolygonData *poly = &shape->data.polygon;
    poly->count = count;

    /* Copy vertices and compute centroid */
    float cx = 0, cy = 0;
    for (int i = 0; i < count; i++) {
        poly->vertices[i] = vertices[i];
        cx += vertices[i].x;
        cy += vertices[i].y;
    }
    cx /= count;
    cy /= count;

    /* Center vertices around origin */
    for (int i = 0; i < count; i++) {
        poly->vertices[i].x -= cx;
        poly->vertices[i].y -= cy;
    }

    /* Compute edge normals (outward-facing) */
    for (int i = 0; i < count; i++) {
        int j = (i + 1) % count;
        Agentite_CollisionVec2 edge = vec2_sub(poly->vertices[j], poly->vertices[i]);
        Agentite_CollisionVec2 normal = vec2_normalize(vec2_perp(edge));
        poly->normals[i] = normal;
    }

    /* TODO: Verify convexity */

    return shape;
}

void agentite_collision_shape_destroy(Agentite_CollisionShape *shape) {
    free(shape);
}

Agentite_ShapeType agentite_collision_shape_get_type(const Agentite_CollisionShape *shape) {
    return shape ? shape->type : AGENTITE_SHAPE_CIRCLE;
}

bool agentite_collision_shape_compute_aabb(
    const Agentite_CollisionShape *shape,
    float x, float y, float rotation,
    Agentite_AABB *out_aabb)
{
    if (!shape || !out_aabb) return false;
    compute_shape_aabb(shape, x, y, rotation, out_aabb);
    return true;
}

/* ============================================================================
 * Collision World Lifecycle
 * ============================================================================ */

Agentite_CollisionWorld *agentite_collision_world_create(
    const Agentite_CollisionWorldConfig *config)
{
    Agentite_CollisionWorldConfig cfg = AGENTITE_COLLISION_WORLD_DEFAULT;
    if (config) cfg = *config;

    Agentite_CollisionWorld *world = AGENTITE_ALLOC(Agentite_CollisionWorld);
    if (!world) {
        agentite_set_error("Collision: Failed to allocate world");
        return NULL;
    }

    world->colliders = (Collider*)calloc(cfg.max_colliders, sizeof(Collider));
    if (!world->colliders) {
        agentite_set_error("Collision: Failed to allocate colliders");
        free(world);
        return NULL;
    }

    if (!spatial_init(&world->spatial, cfg.spatial_capacity, cfg.cell_size)) {
        agentite_set_error("Collision: Failed to allocate spatial hash");
        free(world->colliders);
        free(world);
        return NULL;
    }

    world->max_colliders = cfg.max_colliders;
    world->count = 0;
    world->next_id = 1;  /* ID 0 is invalid */

    return world;
}

void agentite_collision_world_destroy(Agentite_CollisionWorld *world) {
    if (!world) return;
    spatial_destroy(&world->spatial);
    free(world->colliders);
    free(world);
}

void agentite_collision_world_clear(Agentite_CollisionWorld *world) {
    if (!world) return;
    spatial_clear(&world->spatial);
    memset(world->colliders, 0, world->max_colliders * sizeof(Collider));
    world->count = 0;
}

/* ============================================================================
 * Collider Management
 * ============================================================================ */

static Collider *get_collider(Agentite_CollisionWorld *world, Agentite_ColliderId id) {
    if (!world || id == AGENTITE_COLLIDER_INVALID) return NULL;
    uint32_t index = id - 1;
    if (index >= world->max_colliders) return NULL;
    if (!world->colliders[index].active) return NULL;
    return &world->colliders[index];
}

static void update_spatial_for_collider(Agentite_CollisionWorld *world, Agentite_ColliderId id, Collider *col) {
    /* Remove from old cells */
    int32_t old_x1, old_y1, old_x2, old_y2;
    spatial_get_cells(&world->spatial, &col->cached_aabb, &old_x1, &old_y1, &old_x2, &old_y2);
    for (int32_t cy = old_y1; cy <= old_y2; cy++) {
        for (int32_t cx = old_x1; cx <= old_x2; cx++) {
            spatial_remove(&world->spatial, cx, cy, id);
        }
    }

    /* Compute new AABB */
    compute_shape_aabb(col->shape, col->x, col->y, col->rotation, &col->cached_aabb);
    col->aabb_dirty = false;

    /* Add to new cells */
    int32_t new_x1, new_y1, new_x2, new_y2;
    spatial_get_cells(&world->spatial, &col->cached_aabb, &new_x1, &new_y1, &new_x2, &new_y2);
    for (int32_t cy = new_y1; cy <= new_y2; cy++) {
        for (int32_t cx = new_x1; cx <= new_x2; cx++) {
            spatial_add(&world->spatial, cx, cy, id);
        }
    }
}

Agentite_ColliderId agentite_collision_add(
    Agentite_CollisionWorld *world,
    Agentite_CollisionShape *shape,
    float x, float y)
{
    if (!world || !shape) {
        agentite_set_error("Collision: World or shape is NULL");
        return AGENTITE_COLLIDER_INVALID;
    }

    if (world->count >= world->max_colliders) {
        agentite_set_error("Collision: Maximum colliders reached");
        return AGENTITE_COLLIDER_INVALID;
    }

    /* Find free slot */
    uint32_t index = 0;
    for (uint32_t i = 0; i < world->max_colliders; i++) {
        if (!world->colliders[i].active) {
            index = i;
            break;
        }
    }

    Collider *col = &world->colliders[index];
    col->active = true;
    col->shape = shape;
    col->x = x;
    col->y = y;
    col->rotation = 0.0f;
    col->layer = AGENTITE_COLLISION_LAYER_ALL;
    col->mask = AGENTITE_COLLISION_LAYER_ALL;
    col->enabled = true;
    col->user_data = NULL;
    col->aabb_dirty = true;

    Agentite_ColliderId id = index + 1;
    world->count++;

    /* Add to spatial hash */
    compute_shape_aabb(shape, x, y, 0, &col->cached_aabb);
    col->aabb_dirty = false;

    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, &col->cached_aabb, &x1, &y1, &x2, &y2);
    for (int32_t cy = y1; cy <= y2; cy++) {
        for (int32_t cx = x1; cx <= x2; cx++) {
            spatial_add(&world->spatial, cx, cy, id);
        }
    }

    return id;
}

bool agentite_collision_remove(Agentite_CollisionWorld *world, Agentite_ColliderId collider) {
    Collider *col = get_collider(world, collider);
    if (!col) return false;

    /* Remove from spatial hash */
    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, &col->cached_aabb, &x1, &y1, &x2, &y2);
    for (int32_t cy = y1; cy <= y2; cy++) {
        for (int32_t cx = x1; cx <= x2; cx++) {
            spatial_remove(&world->spatial, cx, cy, collider);
        }
    }

    col->active = false;
    world->count--;
    return true;
}

bool agentite_collision_is_valid(const Agentite_CollisionWorld *world, Agentite_ColliderId collider) {
    if (!world || collider == AGENTITE_COLLIDER_INVALID) return false;
    uint32_t index = collider - 1;
    if (index >= world->max_colliders) return false;
    return world->colliders[index].active;
}

/* ============================================================================
 * Collider Transform
 * ============================================================================ */

void agentite_collision_set_position(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    float x, float y)
{
    Collider *col = get_collider(world, collider);
    if (!col) return;

    col->x = x;
    col->y = y;
    update_spatial_for_collider(world, collider, col);
}

void agentite_collision_get_position(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    float *out_x, float *out_y)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    if (!col) return;
    if (out_x) *out_x = col->x;
    if (out_y) *out_y = col->y;
}

void agentite_collision_set_rotation(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    float radians)
{
    Collider *col = get_collider(world, collider);
    if (!col) return;

    col->rotation = radians;
    update_spatial_for_collider(world, collider, col);
}

float agentite_collision_get_rotation(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    return col ? col->rotation : 0.0f;
}

bool agentite_collision_get_aabb(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    Agentite_AABB *out_aabb)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    if (!col || !out_aabb) return false;
    *out_aabb = col->cached_aabb;
    return true;
}

/* ============================================================================
 * Collision Layers
 * ============================================================================ */

void agentite_collision_set_layer(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    uint32_t layer)
{
    Collider *col = get_collider(world, collider);
    if (col) col->layer = layer;
}

uint32_t agentite_collision_get_layer(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    return col ? col->layer : 0;
}

void agentite_collision_set_mask(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    uint32_t mask)
{
    Collider *col = get_collider(world, collider);
    if (col) col->mask = mask;
}

uint32_t agentite_collision_get_mask(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    return col ? col->mask : 0;
}

void agentite_collision_set_user_data(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    void *user_data)
{
    Collider *col = get_collider(world, collider);
    if (col) col->user_data = user_data;
}

void *agentite_collision_get_user_data(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    return col ? col->user_data : NULL;
}

void agentite_collision_set_enabled(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    bool enabled)
{
    Collider *col = get_collider(world, collider);
    if (col) col->enabled = enabled;
}

bool agentite_collision_is_enabled(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    return col ? col->enabled : false;
}

/* ============================================================================
 * Shape vs Shape Collision Tests
 * ============================================================================ */

/* Circle vs Circle */
static bool test_circle_circle(
    float x1, float y1, float r1,
    float x2, float y2, float r2,
    Agentite_CollisionResult *out)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float dist_sq = dx * dx + dy * dy;
    float r_sum = r1 + r2;

    if (dist_sq > r_sum * r_sum) {
        if (out) out->is_colliding = false;
        return false;
    }

    if (out) {
        out->is_colliding = true;
        float dist = sqrtf(dist_sq);
        if (dist > COLLISION_EPSILON) {
            out->normal.x = dx / dist;
            out->normal.y = dy / dist;
            out->depth = r_sum - dist;
        } else {
            out->normal = (Agentite_CollisionVec2){1, 0};
            out->depth = r_sum;
        }
        out->contact_count = 1;
        out->contacts[0].point.x = x1 + out->normal.x * r1;
        out->contacts[0].point.y = y1 + out->normal.y * r1;
        out->contacts[0].depth = out->depth;
    }
    return true;
}

/* Circle vs AABB */
static bool test_circle_aabb(
    float cx, float cy, float radius,
    float bx, float by, float hw, float hh,
    Agentite_CollisionResult *out)
{
    /* Find closest point on AABB to circle center */
    float closest_x = clampf(cx, bx - hw, bx + hw);
    float closest_y = clampf(cy, by - hh, by + hh);

    float dx = cx - closest_x;
    float dy = cy - closest_y;
    float dist_sq = dx * dx + dy * dy;

    if (dist_sq > radius * radius) {
        if (out) out->is_colliding = false;
        return false;
    }

    if (out) {
        out->is_colliding = true;
        float dist = sqrtf(dist_sq);

        if (dist > COLLISION_EPSILON) {
            out->normal.x = dx / dist;
            out->normal.y = dy / dist;
            out->depth = radius - dist;
        } else {
            /* Circle center is inside AABB */
            float pen_x = hw - absf(cx - bx);
            float pen_y = hh - absf(cy - by);
            if (pen_x < pen_y) {
                out->normal.x = (cx > bx) ? 1.0f : -1.0f;
                out->normal.y = 0;
                out->depth = pen_x + radius;
            } else {
                out->normal.x = 0;
                out->normal.y = (cy > by) ? 1.0f : -1.0f;
                out->depth = pen_y + radius;
            }
        }

        out->contact_count = 1;
        out->contacts[0].point.x = closest_x;
        out->contacts[0].point.y = closest_y;
        out->contacts[0].depth = out->depth;
    }
    return true;
}

/* AABB vs AABB */
static bool test_aabb_aabb(
    float x1, float y1, float hw1, float hh1,
    float x2, float y2, float hw2, float hh2,
    Agentite_CollisionResult *out)
{
    float dx = x2 - x1;
    float dy = y2 - y1;
    float ox = hw1 + hw2 - absf(dx);
    float oy = hh1 + hh2 - absf(dy);

    if (ox <= 0 || oy <= 0) {
        if (out) out->is_colliding = false;
        return false;
    }

    if (out) {
        out->is_colliding = true;
        if (ox < oy) {
            out->normal.x = (dx > 0) ? 1.0f : -1.0f;
            out->normal.y = 0;
            out->depth = ox;
        } else {
            out->normal.x = 0;
            out->normal.y = (dy > 0) ? 1.0f : -1.0f;
            out->depth = oy;
        }

        /* Contact points at overlap region */
        float left = maxf(x1 - hw1, x2 - hw2);
        float right = minf(x1 + hw1, x2 + hw2);
        float top = maxf(y1 - hh1, y2 - hh2);
        float bottom = minf(y1 + hh1, y2 + hh2);

        out->contact_count = 1;
        out->contacts[0].point.x = (left + right) * 0.5f;
        out->contacts[0].point.y = (top + bottom) * 0.5f;
        out->contacts[0].depth = out->depth;
    }
    return true;
}

/* Point vs line segment - closest point */
static Agentite_CollisionVec2 closest_point_on_segment(
    Agentite_CollisionVec2 point,
    Agentite_CollisionVec2 a,
    Agentite_CollisionVec2 b)
{
    Agentite_CollisionVec2 ab = vec2_sub(b, a);
    float t = vec2_dot(vec2_sub(point, a), ab) / vec2_dot(ab, ab);
    t = clampf(t, 0.0f, 1.0f);
    return vec2_add(a, vec2_scale(ab, t));
}

/* Circle vs Capsule */
static bool test_circle_capsule(
    float cx, float cy, float cr,
    float capx, float capy, float capr, float half_len,
    Agentite_CapsuleAxis axis, float rotation,
    Agentite_CollisionResult *out)
{
    float cos_r = cosf(rotation);
    float sin_r = sinf(rotation);

    /* Capsule segment endpoints */
    Agentite_CollisionVec2 offset = (axis == AGENTITE_CAPSULE_X) ?
        (Agentite_CollisionVec2){half_len, 0} : (Agentite_CollisionVec2){0, half_len};
    offset = vec2_rotate(offset, cos_r, sin_r);

    Agentite_CollisionVec2 a = {capx - offset.x, capy - offset.y};
    Agentite_CollisionVec2 b = {capx + offset.x, capy + offset.y};
    Agentite_CollisionVec2 p = {cx, cy};

    Agentite_CollisionVec2 closest = closest_point_on_segment(p, a, b);

    return test_circle_circle(cx, cy, cr, closest.x, closest.y, capr, out);
}

/* Capsule vs Capsule */
static bool test_capsule_capsule(
    float x1, float y1, float r1, float hl1, Agentite_CapsuleAxis ax1, float rot1,
    float x2, float y2, float r2, float hl2, Agentite_CapsuleAxis ax2, float rot2,
    Agentite_CollisionResult *out)
{
    /* Get segment endpoints for both capsules */
    float cos1 = cosf(rot1), sin1 = sinf(rot1);
    float cos2 = cosf(rot2), sin2 = sinf(rot2);

    Agentite_CollisionVec2 off1 = (ax1 == AGENTITE_CAPSULE_X) ?
        (Agentite_CollisionVec2){hl1, 0} : (Agentite_CollisionVec2){0, hl1};
    off1 = vec2_rotate(off1, cos1, sin1);

    Agentite_CollisionVec2 off2 = (ax2 == AGENTITE_CAPSULE_X) ?
        (Agentite_CollisionVec2){hl2, 0} : (Agentite_CollisionVec2){0, hl2};
    off2 = vec2_rotate(off2, cos2, sin2);

    Agentite_CollisionVec2 a1 = {x1 - off1.x, y1 - off1.y};
    Agentite_CollisionVec2 b1 = {x1 + off1.x, y1 + off1.y};
    Agentite_CollisionVec2 a2 = {x2 - off2.x, y2 - off2.y};
    Agentite_CollisionVec2 b2 = {x2 + off2.x, y2 + off2.y};

    /* Find closest points between the two segments */
    /* Simplified: test all endpoint combinations */
    float min_dist_sq = FLT_MAX;
    Agentite_CollisionVec2 closest1, closest2;

    Agentite_CollisionVec2 pts1[2] = {a1, b1};
    Agentite_CollisionVec2 pts2[2] = {a2, b2};

    for (int i = 0; i < 2; i++) {
        Agentite_CollisionVec2 c = closest_point_on_segment(pts1[i], a2, b2);
        float d = vec2_length_sq(vec2_sub(pts1[i], c));
        if (d < min_dist_sq) {
            min_dist_sq = d;
            closest1 = pts1[i];
            closest2 = c;
        }
    }
    for (int i = 0; i < 2; i++) {
        Agentite_CollisionVec2 c = closest_point_on_segment(pts2[i], a1, b1);
        float d = vec2_length_sq(vec2_sub(pts2[i], c));
        if (d < min_dist_sq) {
            min_dist_sq = d;
            closest1 = c;
            closest2 = pts2[i];
        }
    }

    return test_circle_circle(closest1.x, closest1.y, r1, closest2.x, closest2.y, r2, out);
}

/* OBB vs OBB using SAT */
static bool test_obb_obb(
    float x1, float y1, float hw1, float hh1, float rot1,
    float x2, float y2, float hw2, float hh2, float rot2,
    Agentite_CollisionResult *out)
{
    float cos1 = cosf(rot1), sin1 = sinf(rot1);
    float cos2 = cosf(rot2), sin2 = sinf(rot2);

    /* Axes to test: both box's X and Y axes */
    Agentite_CollisionVec2 axes[4] = {
        {cos1, sin1}, {-sin1, cos1},
        {cos2, sin2}, {-sin2, cos2}
    };

    float min_overlap = FLT_MAX;
    Agentite_CollisionVec2 min_axis = {0, 0};

    /* Get corners for both OBBs */
    Agentite_CollisionVec2 corners1[4], corners2[4];

    for (int i = 0; i < 4; i++) {
        float sx = (i & 1) ? 1 : -1;
        float sy = (i & 2) ? 1 : -1;
        Agentite_CollisionVec2 local1 = {sx * hw1, sy * hh1};
        Agentite_CollisionVec2 local2 = {sx * hw2, sy * hh2};
        corners1[i] = vec2_add((Agentite_CollisionVec2){x1, y1}, vec2_rotate(local1, cos1, sin1));
        corners2[i] = vec2_add((Agentite_CollisionVec2){x2, y2}, vec2_rotate(local2, cos2, sin2));
    }

    for (int a = 0; a < 4; a++) {
        float min1 = FLT_MAX, max1 = -FLT_MAX;
        float min2 = FLT_MAX, max2 = -FLT_MAX;

        for (int i = 0; i < 4; i++) {
            float p1 = vec2_dot(corners1[i], axes[a]);
            float p2 = vec2_dot(corners2[i], axes[a]);
            min1 = minf(min1, p1); max1 = maxf(max1, p1);
            min2 = minf(min2, p2); max2 = maxf(max2, p2);
        }

        float overlap = minf(max1, max2) - maxf(min1, min2);
        if (overlap <= 0) {
            if (out) out->is_colliding = false;
            return false;
        }

        if (overlap < min_overlap) {
            min_overlap = overlap;
            min_axis = axes[a];
            /* Ensure normal points from 1 to 2 */
            Agentite_CollisionVec2 d = {x2 - x1, y2 - y1};
            if (vec2_dot(d, min_axis) < 0) {
                min_axis = vec2_neg(min_axis);
            }
        }
    }

    if (out) {
        out->is_colliding = true;
        out->normal = min_axis;
        out->depth = min_overlap;
        out->contact_count = 1;
        /* Approximate contact point */
        out->contacts[0].point.x = (x1 + x2) * 0.5f;
        out->contacts[0].point.y = (y1 + y2) * 0.5f;
        out->contacts[0].depth = min_overlap;
    }
    return true;
}

/* General shape test dispatcher */
bool agentite_collision_test_shapes(
    const Agentite_CollisionShape *shape_a,
    float pos_a_x, float pos_a_y, float rot_a,
    const Agentite_CollisionShape *shape_b,
    float pos_b_x, float pos_b_y, float rot_b,
    Agentite_CollisionResult *out_result)
{
    if (!shape_a || !shape_b) return false;

    /* Initialize result */
    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
    }

    Agentite_ShapeType ta = shape_a->type;
    Agentite_ShapeType tb = shape_b->type;

    /* Circle vs Circle */
    if (ta == AGENTITE_SHAPE_CIRCLE && tb == AGENTITE_SHAPE_CIRCLE) {
        return test_circle_circle(
            pos_a_x, pos_a_y, shape_a->data.circle.radius,
            pos_b_x, pos_b_y, shape_b->data.circle.radius,
            out_result);
    }

    /* Circle vs AABB */
    if (ta == AGENTITE_SHAPE_CIRCLE && tb == AGENTITE_SHAPE_AABB) {
        return test_circle_aabb(
            pos_a_x, pos_a_y, shape_a->data.circle.radius,
            pos_b_x, pos_b_y, shape_b->data.aabb.half_width, shape_b->data.aabb.half_height,
            out_result);
    }
    if (ta == AGENTITE_SHAPE_AABB && tb == AGENTITE_SHAPE_CIRCLE) {
        bool result = test_circle_aabb(
            pos_b_x, pos_b_y, shape_b->data.circle.radius,
            pos_a_x, pos_a_y, shape_a->data.aabb.half_width, shape_a->data.aabb.half_height,
            out_result);
        if (out_result && result) {
            out_result->normal = vec2_neg(out_result->normal);
        }
        return result;
    }

    /* AABB vs AABB */
    if (ta == AGENTITE_SHAPE_AABB && tb == AGENTITE_SHAPE_AABB) {
        return test_aabb_aabb(
            pos_a_x, pos_a_y, shape_a->data.aabb.half_width, shape_a->data.aabb.half_height,
            pos_b_x, pos_b_y, shape_b->data.aabb.half_width, shape_b->data.aabb.half_height,
            out_result);
    }

    /* OBB vs OBB */
    if (ta == AGENTITE_SHAPE_OBB && tb == AGENTITE_SHAPE_OBB) {
        return test_obb_obb(
            pos_a_x, pos_a_y, shape_a->data.obb.half_width, shape_a->data.obb.half_height, rot_a,
            pos_b_x, pos_b_y, shape_b->data.obb.half_width, shape_b->data.obb.half_height, rot_b,
            out_result);
    }

    /* Circle vs OBB - transform circle to OBB local space and test as AABB */
    if (ta == AGENTITE_SHAPE_CIRCLE && tb == AGENTITE_SHAPE_OBB) {
        float cos_r = cosf(-rot_b), sin_r = sinf(-rot_b);
        float local_x = (pos_a_x - pos_b_x) * cos_r - (pos_a_y - pos_b_y) * sin_r;
        float local_y = (pos_a_x - pos_b_x) * sin_r + (pos_a_y - pos_b_y) * cos_r;
        bool result = test_circle_aabb(
            local_x, local_y, shape_a->data.circle.radius,
            0, 0, shape_b->data.obb.half_width, shape_b->data.obb.half_height,
            out_result);
        if (out_result && result) {
            /* Transform normal back to world space */
            Agentite_CollisionVec2 n = out_result->normal;
            out_result->normal = vec2_rotate(n, cosf(rot_b), sinf(rot_b));
        }
        return result;
    }
    if (ta == AGENTITE_SHAPE_OBB && tb == AGENTITE_SHAPE_CIRCLE) {
        float cos_r = cosf(-rot_a), sin_r = sinf(-rot_a);
        float local_x = (pos_b_x - pos_a_x) * cos_r - (pos_b_y - pos_a_y) * sin_r;
        float local_y = (pos_b_x - pos_a_x) * sin_r + (pos_b_y - pos_a_y) * cos_r;
        bool result = test_circle_aabb(
            local_x, local_y, shape_b->data.circle.radius,
            0, 0, shape_a->data.obb.half_width, shape_a->data.obb.half_height,
            out_result);
        if (out_result && result) {
            Agentite_CollisionVec2 n = out_result->normal;
            out_result->normal = vec2_neg(vec2_rotate(n, cosf(rot_a), sinf(rot_a)));
        }
        return result;
    }

    /* Circle vs Capsule */
    if (ta == AGENTITE_SHAPE_CIRCLE && tb == AGENTITE_SHAPE_CAPSULE) {
        return test_circle_capsule(
            pos_a_x, pos_a_y, shape_a->data.circle.radius,
            pos_b_x, pos_b_y, shape_b->data.capsule.radius, shape_b->data.capsule.half_length,
            shape_b->data.capsule.axis, rot_b,
            out_result);
    }
    if (ta == AGENTITE_SHAPE_CAPSULE && tb == AGENTITE_SHAPE_CIRCLE) {
        bool result = test_circle_capsule(
            pos_b_x, pos_b_y, shape_b->data.circle.radius,
            pos_a_x, pos_a_y, shape_a->data.capsule.radius, shape_a->data.capsule.half_length,
            shape_a->data.capsule.axis, rot_a,
            out_result);
        if (out_result && result) {
            out_result->normal = vec2_neg(out_result->normal);
        }
        return result;
    }

    /* Capsule vs Capsule */
    if (ta == AGENTITE_SHAPE_CAPSULE && tb == AGENTITE_SHAPE_CAPSULE) {
        return test_capsule_capsule(
            pos_a_x, pos_a_y, shape_a->data.capsule.radius, shape_a->data.capsule.half_length,
            shape_a->data.capsule.axis, rot_a,
            pos_b_x, pos_b_y, shape_b->data.capsule.radius, shape_b->data.capsule.half_length,
            shape_b->data.capsule.axis, rot_b,
            out_result);
    }

    /* Fallback: AABB overlap test */
    Agentite_AABB aabb_a, aabb_b;
    compute_shape_aabb(shape_a, pos_a_x, pos_a_y, rot_a, &aabb_a);
    compute_shape_aabb(shape_b, pos_b_x, pos_b_y, rot_b, &aabb_b);

    if (aabb_a.max_x < aabb_b.min_x || aabb_a.min_x > aabb_b.max_x ||
        aabb_a.max_y < aabb_b.min_y || aabb_a.min_y > aabb_b.max_y) {
        if (out_result) out_result->is_colliding = false;
        return false;
    }

    if (out_result) {
        out_result->is_colliding = true;
        out_result->normal = (Agentite_CollisionVec2){1, 0};
        out_result->depth = 0;
    }
    return true;
}

/* ============================================================================
 * Collision Queries
 * ============================================================================ */

bool agentite_collision_test(
    const Agentite_CollisionWorld *world,
    Agentite_ColliderId a, Agentite_ColliderId b,
    Agentite_CollisionResult *out_result)
{
    const Collider *ca = get_collider((Agentite_CollisionWorld*)world, a);
    const Collider *cb = get_collider((Agentite_CollisionWorld*)world, b);

    if (!ca || !cb) return false;

    bool result = agentite_collision_test_shapes(
        ca->shape, ca->x, ca->y, ca->rotation,
        cb->shape, cb->x, cb->y, cb->rotation,
        out_result);

    if (out_result) {
        out_result->collider_a = a;
        out_result->collider_b = b;
    }

    return result;
}

int agentite_collision_query_collider(
    Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    Agentite_CollisionResult *out_results, int max_results)
{
    if (!world || !out_results || max_results <= 0) return 0;

    Collider *col = get_collider(world, collider);
    if (!col || !col->enabled) return 0;

    int count = 0;

    /* Get cells this collider overlaps */
    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, &col->cached_aabb, &x1, &y1, &x2, &y2);

    /* Track tested colliders to avoid duplicates */
    static Agentite_ColliderId tested[1024];
    int tested_count = 0;

    for (int32_t cy = y1; cy <= y2 && count < max_results; cy++) {
        for (int32_t cx = x1; cx <= x2 && count < max_results; cx++) {
            SpatialCell *cell = spatial_find_cell(&world->spatial, cx, cy, false);
            if (!cell) continue;

            for (int i = 0; i < cell->count && count < max_results; i++) {
                Agentite_ColliderId other_id = cell->colliders[i];
                if (other_id == collider) continue;

                /* Skip if already tested */
                bool already_tested = false;
                for (int t = 0; t < tested_count; t++) {
                    if (tested[t] == other_id) {
                        already_tested = true;
                        break;
                    }
                }
                if (already_tested) continue;
                if (tested_count < 1024) tested[tested_count++] = other_id;

                Collider *other = get_collider(world, other_id);
                if (!other || !other->enabled) continue;

                /* Check layer/mask */
                if (!(col->mask & other->layer) || !(other->mask & col->layer)) continue;

                /* Test collision */
                Agentite_CollisionResult result;
                if (agentite_collision_test_shapes(
                        col->shape, col->x, col->y, col->rotation,
                        other->shape, other->x, other->y, other->rotation,
                        &result)) {
                    result.collider_a = collider;
                    result.collider_b = other_id;
                    out_results[count++] = result;
                }
            }
        }
    }

    return count;
}

int agentite_collision_query_shape(
    Agentite_CollisionWorld *world,
    const Agentite_CollisionShape *shape,
    float x, float y, float rotation,
    uint32_t layer_mask,
    Agentite_CollisionResult *out_results, int max_results)
{
    if (!world || !shape || !out_results || max_results <= 0) return 0;

    Agentite_AABB aabb;
    compute_shape_aabb(shape, x, y, rotation, &aabb);

    int count = 0;
    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, &aabb, &x1, &y1, &x2, &y2);

    static Agentite_ColliderId tested[1024];
    int tested_count = 0;

    for (int32_t cy = y1; cy <= y2 && count < max_results; cy++) {
        for (int32_t cx = x1; cx <= x2 && count < max_results; cx++) {
            SpatialCell *cell = spatial_find_cell(&world->spatial, cx, cy, false);
            if (!cell) continue;

            for (int i = 0; i < cell->count && count < max_results; i++) {
                Agentite_ColliderId id = cell->colliders[i];

                bool already_tested = false;
                for (int t = 0; t < tested_count; t++) {
                    if (tested[t] == id) {
                        already_tested = true;
                        break;
                    }
                }
                if (already_tested) continue;
                if (tested_count < 1024) tested[tested_count++] = id;

                Collider *col = get_collider(world, id);
                if (!col || !col->enabled) continue;
                if (!(layer_mask & col->layer)) continue;

                Agentite_CollisionResult result;
                if (agentite_collision_test_shapes(
                        shape, x, y, rotation,
                        col->shape, col->x, col->y, col->rotation,
                        &result)) {
                    result.collider_a = AGENTITE_COLLIDER_INVALID;
                    result.collider_b = id;
                    out_results[count++] = result;
                }
            }
        }
    }

    return count;
}

int agentite_collision_query_aabb(
    Agentite_CollisionWorld *world,
    const Agentite_AABB *aabb,
    uint32_t layer_mask,
    Agentite_ColliderId *out_colliders, int max_results)
{
    if (!world || !aabb || !out_colliders || max_results <= 0) return 0;

    int count = 0;
    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, aabb, &x1, &y1, &x2, &y2);

    static Agentite_ColliderId tested[1024];
    int tested_count = 0;

    for (int32_t cy = y1; cy <= y2 && count < max_results; cy++) {
        for (int32_t cx = x1; cx <= x2 && count < max_results; cx++) {
            SpatialCell *cell = spatial_find_cell(&world->spatial, cx, cy, false);
            if (!cell) continue;

            for (int i = 0; i < cell->count && count < max_results; i++) {
                Agentite_ColliderId id = cell->colliders[i];

                bool already_tested = false;
                for (int t = 0; t < tested_count; t++) {
                    if (tested[t] == id) {
                        already_tested = true;
                        break;
                    }
                }
                if (already_tested) continue;
                if (tested_count < 1024) tested[tested_count++] = id;

                Collider *col = get_collider(world, id);
                if (!col || !col->enabled) continue;
                if (!(layer_mask & col->layer)) continue;

                /* AABB overlap test */
                if (col->cached_aabb.max_x >= aabb->min_x &&
                    col->cached_aabb.min_x <= aabb->max_x &&
                    col->cached_aabb.max_y >= aabb->min_y &&
                    col->cached_aabb.min_y <= aabb->max_y) {
                    out_colliders[count++] = id;
                }
            }
        }
    }

    return count;
}

/* ============================================================================
 * Point Queries
 * ============================================================================ */

bool agentite_collision_point_in_shape(
    const Agentite_CollisionShape *shape,
    float shape_x, float shape_y, float shape_rot,
    float point_x, float point_y)
{
    if (!shape) return false;

    float cos_r = cosf(-shape_rot);
    float sin_r = sinf(-shape_rot);

    /* Transform point to shape's local space */
    float local_x = (point_x - shape_x) * cos_r - (point_y - shape_y) * sin_r;
    float local_y = (point_x - shape_x) * sin_r + (point_y - shape_y) * cos_r;

    switch (shape->type) {
        case AGENTITE_SHAPE_CIRCLE:
            return (local_x * local_x + local_y * local_y) <=
                   (shape->data.circle.radius * shape->data.circle.radius);

        case AGENTITE_SHAPE_AABB:
        case AGENTITE_SHAPE_OBB:
        {
            float hw = (shape->type == AGENTITE_SHAPE_AABB) ?
                       shape->data.aabb.half_width : shape->data.obb.half_width;
            float hh = (shape->type == AGENTITE_SHAPE_AABB) ?
                       shape->data.aabb.half_height : shape->data.obb.half_height;
            return absf(local_x) <= hw && absf(local_y) <= hh;
        }

        case AGENTITE_SHAPE_CAPSULE:
        {
            float r = shape->data.capsule.radius;
            float hl = shape->data.capsule.half_length;
            bool x_axis = (shape->data.capsule.axis == AGENTITE_CAPSULE_X);

            if (x_axis) {
                float clamped_x = clampf(local_x, -hl, hl);
                float dx = local_x - clamped_x;
                return (dx * dx + local_y * local_y) <= r * r;
            } else {
                float clamped_y = clampf(local_y, -hl, hl);
                float dy = local_y - clamped_y;
                return (local_x * local_x + dy * dy) <= r * r;
            }
        }

        case AGENTITE_SHAPE_POLYGON:
        {
            const PolygonData *poly = &shape->data.polygon;
            Agentite_CollisionVec2 p = {local_x, local_y};

            /* Point-in-polygon using winding number or ray casting */
            for (int i = 0; i < poly->count; i++) {
                Agentite_CollisionVec2 v = poly->vertices[i];
                if (vec2_dot(poly->normals[i], vec2_sub(p, v)) > 0) {
                    return false;
                }
            }
            return true;
        }
    }

    return false;
}

bool agentite_collision_point_test(
    const Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    float x, float y)
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    if (!col) return false;

    return agentite_collision_point_in_shape(col->shape, col->x, col->y, col->rotation, x, y);
}

int agentite_collision_query_point(
    Agentite_CollisionWorld *world,
    float x, float y,
    uint32_t layer_mask,
    Agentite_ColliderId *out_colliders, int max_results)
{
    if (!world || !out_colliders || max_results <= 0) return 0;

    int count = 0;

    /* Get the cell containing this point */
    int32_t cx = (int32_t)floorf(x * world->spatial.inv_cell_size);
    int32_t cy = (int32_t)floorf(y * world->spatial.inv_cell_size);

    SpatialCell *cell = spatial_find_cell(&world->spatial, cx, cy, false);
    if (!cell) return 0;

    for (int i = 0; i < cell->count && count < max_results; i++) {
        Agentite_ColliderId id = cell->colliders[i];
        Collider *col = get_collider(world, id);
        if (!col || !col->enabled) continue;
        if (!(layer_mask & col->layer)) continue;

        if (agentite_collision_point_in_shape(col->shape, col->x, col->y, col->rotation, x, y)) {
            out_colliders[count++] = id;
        }
    }

    return count;
}

/* ============================================================================
 * Raycast
 * ============================================================================ */

/* Ray vs Circle */
static bool raycast_circle(
    float cx, float cy, float radius,
    float ox, float oy, float dx, float dy, float max_dist,
    Agentite_RaycastHit *out)
{
    /* Ray: P = O + t*D, Circle: |P - C|^2 = r^2 */
    float fx = ox - cx;
    float fy = oy - cy;

    float a = dx * dx + dy * dy;
    float b = 2.0f * (fx * dx + fy * dy);
    float c = fx * fx + fy * fy - radius * radius;

    float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0) return false;

    float sqrt_disc = sqrtf(discriminant);
    float t = (-b - sqrt_disc) / (2.0f * a);

    if (t < 0) t = (-b + sqrt_disc) / (2.0f * a);
    if (t < 0 || t > max_dist) return false;

    if (out) {
        out->distance = t;
        out->fraction = t / max_dist;
        out->point.x = ox + dx * t;
        out->point.y = oy + dy * t;
        out->normal = vec2_normalize((Agentite_CollisionVec2){
            out->point.x - cx, out->point.y - cy
        });
    }
    return true;
}

/* Ray vs AABB */
static bool raycast_aabb(
    float bx, float by, float hw, float hh,
    float ox, float oy, float dx, float dy, float max_dist,
    Agentite_RaycastHit *out)
{
    float inv_dx = (absf(dx) < COLLISION_EPSILON) ? 1e10f : 1.0f / dx;
    float inv_dy = (absf(dy) < COLLISION_EPSILON) ? 1e10f : 1.0f / dy;

    float t1x = (bx - hw - ox) * inv_dx;
    float t2x = (bx + hw - ox) * inv_dx;
    float t1y = (by - hh - oy) * inv_dy;
    float t2y = (by + hh - oy) * inv_dy;

    float tmin_x = minf(t1x, t2x);
    float tmax_x = maxf(t1x, t2x);
    float tmin_y = minf(t1y, t2y);
    float tmax_y = maxf(t1y, t2y);

    float tmin = maxf(tmin_x, tmin_y);
    float tmax = minf(tmax_x, tmax_y);

    if (tmax < 0 || tmin > tmax || tmin > max_dist) return false;

    float t = (tmin >= 0) ? tmin : tmax;
    if (t > max_dist) return false;

    if (out) {
        out->distance = t;
        out->fraction = t / max_dist;
        out->point.x = ox + dx * t;
        out->point.y = oy + dy * t;

        /* Determine hit face */
        if (tmin_x > tmin_y) {
            out->normal.x = (dx > 0) ? -1.0f : 1.0f;
            out->normal.y = 0;
        } else {
            out->normal.x = 0;
            out->normal.y = (dy > 0) ? -1.0f : 1.0f;
        }
    }
    return true;
}

bool agentite_collision_raycast_shape(
    const Agentite_CollisionShape *shape,
    float shape_x, float shape_y, float shape_rot,
    float origin_x, float origin_y,
    float dir_x, float dir_y,
    float max_distance,
    Agentite_RaycastHit *out_hit)
{
    if (!shape) return false;

    /* Normalize direction */
    float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    if (len < COLLISION_EPSILON) return false;
    dir_x /= len;
    dir_y /= len;

    switch (shape->type) {
        case AGENTITE_SHAPE_CIRCLE:
            return raycast_circle(shape_x, shape_y, shape->data.circle.radius,
                                  origin_x, origin_y, dir_x, dir_y, max_distance, out_hit);

        case AGENTITE_SHAPE_AABB:
            return raycast_aabb(shape_x, shape_y,
                               shape->data.aabb.half_width, shape->data.aabb.half_height,
                               origin_x, origin_y, dir_x, dir_y, max_distance, out_hit);

        case AGENTITE_SHAPE_OBB:
        {
            /* Transform ray to OBB local space */
            float cos_r = cosf(-shape_rot);
            float sin_r = sinf(-shape_rot);
            float local_ox = (origin_x - shape_x) * cos_r - (origin_y - shape_y) * sin_r;
            float local_oy = (origin_x - shape_x) * sin_r + (origin_y - shape_y) * cos_r;
            float local_dx = dir_x * cos_r - dir_y * sin_r;
            float local_dy = dir_x * sin_r + dir_y * cos_r;

            bool hit = raycast_aabb(0, 0,
                                   shape->data.obb.half_width, shape->data.obb.half_height,
                                   local_ox, local_oy, local_dx, local_dy, max_distance, out_hit);

            if (hit && out_hit) {
                /* Transform result back to world space */
                float cos_r2 = cosf(shape_rot);
                float sin_r2 = sinf(shape_rot);
                float wx = out_hit->point.x * cos_r2 - out_hit->point.y * sin_r2 + shape_x;
                float wy = out_hit->point.x * sin_r2 + out_hit->point.y * cos_r2 + shape_y;
                out_hit->point.x = wx;
                out_hit->point.y = wy;
                float nx = out_hit->normal.x * cos_r2 - out_hit->normal.y * sin_r2;
                float ny = out_hit->normal.x * sin_r2 + out_hit->normal.y * cos_r2;
                out_hit->normal.x = nx;
                out_hit->normal.y = ny;
            }
            return hit;
        }

        case AGENTITE_SHAPE_CAPSULE:
        {
            /* Test ray against both end circles and the center rectangle */
            float cos_r = cosf(shape_rot);
            float sin_r = sinf(shape_rot);
            float r = shape->data.capsule.radius;
            float hl = shape->data.capsule.half_length;

            Agentite_CollisionVec2 offset = (shape->data.capsule.axis == AGENTITE_CAPSULE_X) ?
                (Agentite_CollisionVec2){hl, 0} : (Agentite_CollisionVec2){0, hl};
            offset = vec2_rotate(offset, cos_r, sin_r);

            float c1x = shape_x + offset.x, c1y = shape_y + offset.y;
            float c2x = shape_x - offset.x, c2y = shape_y - offset.y;

            Agentite_RaycastHit hit1, hit2;
            bool h1 = raycast_circle(c1x, c1y, r, origin_x, origin_y, dir_x, dir_y, max_distance, &hit1);
            bool h2 = raycast_circle(c2x, c2y, r, origin_x, origin_y, dir_x, dir_y, max_distance, &hit2);

            if (h1 && h2) {
                if (out_hit) *out_hit = (hit1.distance < hit2.distance) ? hit1 : hit2;
                return true;
            }
            if (h1) { if (out_hit) *out_hit = hit1; return true; }
            if (h2) { if (out_hit) *out_hit = hit2; return true; }
            return false;
        }

        default:
            return false;
    }
}

bool agentite_collision_raycast(
    Agentite_CollisionWorld *world,
    float origin_x, float origin_y,
    float dir_x, float dir_y,
    float max_distance,
    uint32_t layer_mask,
    Agentite_RaycastHit *out_hit)
{
    if (!world) return false;

    /* Normalize direction */
    float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    if (len < COLLISION_EPSILON) return false;
    dir_x /= len;
    dir_y /= len;

    /* Compute ray AABB */
    Agentite_AABB ray_aabb;
    ray_aabb.min_x = minf(origin_x, origin_x + dir_x * max_distance);
    ray_aabb.max_x = maxf(origin_x, origin_x + dir_x * max_distance);
    ray_aabb.min_y = minf(origin_y, origin_y + dir_y * max_distance);
    ray_aabb.max_y = maxf(origin_y, origin_y + dir_y * max_distance);

    bool found = false;
    Agentite_RaycastHit best_hit;
    best_hit.distance = max_distance;

    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, &ray_aabb, &x1, &y1, &x2, &y2);

    static Agentite_ColliderId tested[1024];
    int tested_count = 0;

    for (int32_t cy = y1; cy <= y2; cy++) {
        for (int32_t cx = x1; cx <= x2; cx++) {
            SpatialCell *cell = spatial_find_cell(&world->spatial, cx, cy, false);
            if (!cell) continue;

            for (int i = 0; i < cell->count; i++) {
                Agentite_ColliderId id = cell->colliders[i];

                bool already = false;
                for (int t = 0; t < tested_count; t++) {
                    if (tested[t] == id) { already = true; break; }
                }
                if (already) continue;
                if (tested_count < 1024) tested[tested_count++] = id;

                Collider *col = get_collider(world, id);
                if (!col || !col->enabled) continue;
                if (!(layer_mask & col->layer)) continue;

                Agentite_RaycastHit hit;
                if (agentite_collision_raycast_shape(
                        col->shape, col->x, col->y, col->rotation,
                        origin_x, origin_y, dir_x, dir_y, best_hit.distance, &hit)) {
                    if (hit.distance < best_hit.distance) {
                        best_hit = hit;
                        best_hit.collider = id;
                        found = true;
                    }
                }
            }
        }
    }

    if (found && out_hit) {
        *out_hit = best_hit;
    }
    return found;
}

int agentite_collision_raycast_all(
    Agentite_CollisionWorld *world,
    float origin_x, float origin_y,
    float dir_x, float dir_y,
    float max_distance,
    uint32_t layer_mask,
    Agentite_RaycastHit *out_hits, int max_hits)
{
    if (!world || !out_hits || max_hits <= 0) return 0;

    /* Normalize direction */
    float len = sqrtf(dir_x * dir_x + dir_y * dir_y);
    if (len < COLLISION_EPSILON) return 0;
    dir_x /= len;
    dir_y /= len;

    Agentite_AABB ray_aabb;
    ray_aabb.min_x = minf(origin_x, origin_x + dir_x * max_distance);
    ray_aabb.max_x = maxf(origin_x, origin_x + dir_x * max_distance);
    ray_aabb.min_y = minf(origin_y, origin_y + dir_y * max_distance);
    ray_aabb.max_y = maxf(origin_y, origin_y + dir_y * max_distance);

    int count = 0;

    int32_t x1, y1, x2, y2;
    spatial_get_cells(&world->spatial, &ray_aabb, &x1, &y1, &x2, &y2);

    static Agentite_ColliderId tested[1024];
    int tested_count = 0;

    for (int32_t cy = y1; cy <= y2 && count < max_hits; cy++) {
        for (int32_t cx = x1; cx <= x2 && count < max_hits; cx++) {
            SpatialCell *cell = spatial_find_cell(&world->spatial, cx, cy, false);
            if (!cell) continue;

            for (int i = 0; i < cell->count && count < max_hits; i++) {
                Agentite_ColliderId id = cell->colliders[i];

                bool already = false;
                for (int t = 0; t < tested_count; t++) {
                    if (tested[t] == id) { already = true; break; }
                }
                if (already) continue;
                if (tested_count < 1024) tested[tested_count++] = id;

                Collider *col = get_collider(world, id);
                if (!col || !col->enabled) continue;
                if (!(layer_mask & col->layer)) continue;

                Agentite_RaycastHit hit;
                if (agentite_collision_raycast_shape(
                        col->shape, col->x, col->y, col->rotation,
                        origin_x, origin_y, dir_x, dir_y, max_distance, &hit)) {
                    hit.collider = id;
                    out_hits[count++] = hit;
                }
            }
        }
    }

    /* Sort by distance */
    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            if (out_hits[j].distance < out_hits[i].distance) {
                Agentite_RaycastHit tmp = out_hits[i];
                out_hits[i] = out_hits[j];
                out_hits[j] = tmp;
            }
        }
    }

    return count;
}

/* ============================================================================
 * Shape Cast (Sweep)
 * ============================================================================ */

bool agentite_collision_shape_cast(
    Agentite_CollisionWorld *world,
    const Agentite_CollisionShape *shape,
    float start_x, float start_y,
    float end_x, float end_y,
    float rotation,
    uint32_t layer_mask,
    Agentite_ShapeCastHit *out_hit)
{
    if (!world || !shape) return false;

    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float dist = sqrtf(dx * dx + dy * dy);
    if (dist < COLLISION_EPSILON) return false;

    /* Binary search for collision point */
    float low = 0, high = 1;
    bool found = false;
    Agentite_CollisionResult last_result;
    Agentite_ColliderId hit_id = AGENTITE_COLLIDER_INVALID;

    for (int iter = 0; iter < 16; iter++) {
        float mid = (low + high) * 0.5f;
        float test_x = start_x + dx * mid;
        float test_y = start_y + dy * mid;

        Agentite_CollisionResult results[8];
        int count = agentite_collision_query_shape(world, shape, test_x, test_y, rotation,
                                                   layer_mask, results, 8);

        if (count > 0) {
            high = mid;
            found = true;
            last_result = results[0];
            hit_id = results[0].collider_b;
        } else {
            low = mid;
        }
    }

    if (found && out_hit) {
        out_hit->collider = hit_id;
        out_hit->fraction = high;
        out_hit->point.x = start_x + dx * high;
        out_hit->point.y = start_y + dy * high;
        out_hit->normal = last_result.normal;
    }

    return found;
}

bool agentite_collision_sweep(
    Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    float delta_x, float delta_y,
    Agentite_ShapeCastHit *out_hit)
{
    Collider *col = get_collider(world, collider);
    if (!col) return false;

    return agentite_collision_shape_cast(
        world, col->shape,
        col->x, col->y,
        col->x + delta_x, col->y + delta_y,
        col->rotation, col->mask, out_hit);
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int agentite_collision_world_get_count(const Agentite_CollisionWorld *world) {
    return world ? (int)world->count : 0;
}

int agentite_collision_world_get_capacity(const Agentite_CollisionWorld *world) {
    return world ? (int)world->max_colliders : 0;
}

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

void agentite_collision_debug_draw_shape(
    const Agentite_CollisionShape *shape,
    float x, float y, float rotation,
    Agentite_Gizmos *gizmos,
    float color[4])
{
    if (!shape || !gizmos) return;

    uint32_t col = ((uint32_t)(color[0] * 255) << 24) |
                   ((uint32_t)(color[1] * 255) << 16) |
                   ((uint32_t)(color[2] * 255) << 8) |
                   ((uint32_t)(color[3] * 255));

    float cos_r = cosf(rotation);
    float sin_r = sinf(rotation);

    switch (shape->type) {
        case AGENTITE_SHAPE_CIRCLE:
            agentite_gizmos_circle_2d(gizmos, x, y, shape->data.circle.radius, col);
            break;

        case AGENTITE_SHAPE_AABB:
        {
            float hw = shape->data.aabb.half_width;
            float hh = shape->data.aabb.half_height;
            agentite_gizmos_rect_2d(gizmos, x - hw, y - hh, hw * 2, hh * 2, col);
            break;
        }

        case AGENTITE_SHAPE_OBB:
        {
            float hw = shape->data.obb.half_width;
            float hh = shape->data.obb.half_height;

            Agentite_CollisionVec2 corners[4] = {
                {-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}
            };

            for (int i = 0; i < 4; i++) {
                corners[i] = vec2_rotate(corners[i], cos_r, sin_r);
                corners[i].x += x;
                corners[i].y += y;
            }

            for (int i = 0; i < 4; i++) {
                int j = (i + 1) % 4;
                agentite_gizmos_line_2d(gizmos,
                    corners[i].x, corners[i].y,
                    corners[j].x, corners[j].y, col);
            }
            break;
        }

        case AGENTITE_SHAPE_CAPSULE:
        {
            float r = shape->data.capsule.radius;
            float hl = shape->data.capsule.half_length;
            bool x_axis = (shape->data.capsule.axis == AGENTITE_CAPSULE_X);

            Agentite_CollisionVec2 offset = x_axis ?
                (Agentite_CollisionVec2){hl, 0} : (Agentite_CollisionVec2){0, hl};
            offset = vec2_rotate(offset, cos_r, sin_r);

            agentite_gizmos_circle_2d(gizmos, x + offset.x, y + offset.y, r, col);
            agentite_gizmos_circle_2d(gizmos, x - offset.x, y - offset.y, r, col);

            /* Draw connecting lines */
            Agentite_CollisionVec2 perp = vec2_perp(vec2_normalize(offset));
            perp = vec2_scale(perp, r);

            agentite_gizmos_line_2d(gizmos,
                x + offset.x + perp.x, y + offset.y + perp.y,
                x - offset.x + perp.x, y - offset.y + perp.y, col);
            agentite_gizmos_line_2d(gizmos,
                x + offset.x - perp.x, y + offset.y - perp.y,
                x - offset.x - perp.x, y - offset.y - perp.y, col);
            break;
        }

        case AGENTITE_SHAPE_POLYGON:
        {
            const PolygonData *poly = &shape->data.polygon;
            for (int i = 0; i < poly->count; i++) {
                int j = (i + 1) % poly->count;
                Agentite_CollisionVec2 v1 = vec2_rotate(poly->vertices[i], cos_r, sin_r);
                Agentite_CollisionVec2 v2 = vec2_rotate(poly->vertices[j], cos_r, sin_r);
                agentite_gizmos_line_2d(gizmos,
                    x + v1.x, y + v1.y,
                    x + v2.x, y + v2.y, col);
            }
            break;
        }
    }
}

void agentite_collision_debug_draw_collider(
    const Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    Agentite_Gizmos *gizmos,
    float color[4])
{
    const Collider *col = get_collider((Agentite_CollisionWorld*)world, collider);
    if (!col) return;

    agentite_collision_debug_draw_shape(col->shape, col->x, col->y, col->rotation, gizmos, color);
}

void agentite_collision_debug_draw(
    const Agentite_CollisionWorld *world,
    Agentite_Gizmos *gizmos,
    float color[4])
{
    if (!world || !gizmos) return;

    for (uint32_t i = 0; i < world->max_colliders; i++) {
        if (world->colliders[i].active && world->colliders[i].enabled) {
            agentite_collision_debug_draw_collider(world, i + 1, gizmos, color);
        }
    }
}
