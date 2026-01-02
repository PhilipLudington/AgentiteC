/**
 * @file collision.h
 * @brief 2D Collision Detection System
 *
 * Provides shape-based collision detection with support for various primitives,
 * collision layers, raycasting, and spatial queries. Designed for 2D games.
 *
 * Features:
 * - Shape primitives: AABB, Circle, OBB (oriented box), Capsule, Polygon
 * - Shape-vs-shape collision tests with contact generation
 * - Collision layers and masks for filtering
 * - Raycast and shape cast queries
 * - Point containment tests
 * - Broad-phase acceleration using spatial hashing
 *
 * Usage:
 *   // Create collision world
 *   Agentite_CollisionWorldConfig config = AGENTITE_COLLISION_WORLD_DEFAULT;
 *   Agentite_CollisionWorld *world = agentite_collision_world_create(&config);
 *
 *   // Create shapes
 *   Agentite_CollisionShape *circle = agentite_collision_shape_circle(16.0f);
 *   Agentite_CollisionShape *box = agentite_collision_shape_aabb(32.0f, 32.0f);
 *
 *   // Add colliders to world
 *   Agentite_ColliderId player = agentite_collision_add(world, circle, 100.0f, 100.0f);
 *   agentite_collision_set_layer(world, player, LAYER_PLAYER);
 *   agentite_collision_set_mask(world, player, LAYER_ENEMY | LAYER_WALL);
 *
 *   // Update collider position
 *   agentite_collision_set_position(world, player, new_x, new_y);
 *
 *   // Query collisions
 *   Agentite_CollisionResult results[16];
 *   int count = agentite_collision_query_collider(world, player, results, 16);
 *
 *   // Raycast
 *   Agentite_RaycastHit hit;
 *   if (agentite_collision_raycast(world, x, y, dir_x, dir_y, 100.0f, LAYER_ALL, &hit)) {
 *       // hit.point, hit.normal, hit.distance available
 *   }
 *
 *   // Cleanup
 *   agentite_collision_shape_destroy(circle);
 *   agentite_collision_shape_destroy(box);
 *   agentite_collision_world_destroy(world);
 */

#ifndef AGENTITE_COLLISION_H
#define AGENTITE_COLLISION_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Invalid collider ID */
#define AGENTITE_COLLIDER_INVALID 0

/** All collision layers */
#define AGENTITE_COLLISION_LAYER_ALL 0xFFFFFFFF

/** No collision layers */
#define AGENTITE_COLLISION_LAYER_NONE 0

/** Maximum vertices in a polygon shape */
#define AGENTITE_COLLISION_MAX_POLYGON_VERTS 8

/** Maximum contact points returned per collision */
#define AGENTITE_COLLISION_MAX_CONTACTS 2

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_CollisionWorld Agentite_CollisionWorld;
typedef struct Agentite_CollisionShape Agentite_CollisionShape;
typedef uint32_t Agentite_ColliderId;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** Shape type enumeration */
typedef enum Agentite_ShapeType {
    AGENTITE_SHAPE_CIRCLE,      /**< Circle defined by radius */
    AGENTITE_SHAPE_AABB,        /**< Axis-aligned bounding box */
    AGENTITE_SHAPE_OBB,         /**< Oriented bounding box (rotated rectangle) */
    AGENTITE_SHAPE_CAPSULE,     /**< Capsule (two circles connected by rectangle) */
    AGENTITE_SHAPE_POLYGON      /**< Convex polygon (up to MAX_POLYGON_VERTS) */
} Agentite_ShapeType;

/** Capsule orientation */
typedef enum Agentite_CapsuleAxis {
    AGENTITE_CAPSULE_X,         /**< Capsule aligned along X axis */
    AGENTITE_CAPSULE_Y          /**< Capsule aligned along Y axis */
} Agentite_CapsuleAxis;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/** 2D vector for collision math */
typedef struct Agentite_CollisionVec2 {
    float x;
    float y;
} Agentite_CollisionVec2;

/** Contact point information */
typedef struct Agentite_ContactPoint {
    Agentite_CollisionVec2 point;       /**< Contact point in world space */
    float depth;                         /**< Penetration depth (positive = overlapping) */
} Agentite_ContactPoint;

/** Collision result between two shapes */
typedef struct Agentite_CollisionResult {
    Agentite_ColliderId collider_a;      /**< First collider ID */
    Agentite_ColliderId collider_b;      /**< Second collider ID */
    bool is_colliding;                   /**< True if shapes overlap */
    Agentite_CollisionVec2 normal;       /**< Collision normal (from A to B) */
    float depth;                         /**< Maximum penetration depth */
    Agentite_ContactPoint contacts[AGENTITE_COLLISION_MAX_CONTACTS]; /**< Contact points */
    int contact_count;                   /**< Number of valid contacts (0-2) */
} Agentite_CollisionResult;

/** Raycast hit information */
typedef struct Agentite_RaycastHit {
    Agentite_ColliderId collider;        /**< Collider that was hit */
    Agentite_CollisionVec2 point;        /**< Hit point in world space */
    Agentite_CollisionVec2 normal;       /**< Surface normal at hit point */
    float distance;                      /**< Distance from ray origin to hit */
    float fraction;                      /**< Fraction along ray (0-1) */
} Agentite_RaycastHit;

/** Shape cast (sweep) result */
typedef struct Agentite_ShapeCastHit {
    Agentite_ColliderId collider;        /**< Collider that was hit */
    Agentite_CollisionVec2 point;        /**< Point of first contact */
    Agentite_CollisionVec2 normal;       /**< Surface normal at contact */
    float fraction;                      /**< Fraction of sweep distance (0-1) */
} Agentite_ShapeCastHit;

/** Axis-aligned bounding box */
typedef struct Agentite_AABB {
    float min_x, min_y;                  /**< Minimum corner */
    float max_x, max_y;                  /**< Maximum corner */
} Agentite_AABB;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/** Configuration for collision world */
typedef struct Agentite_CollisionWorldConfig {
    uint32_t max_colliders;              /**< Maximum colliders (default: 1024) */
    float cell_size;                     /**< Spatial hash cell size (default: 64.0) */
    uint32_t spatial_capacity;           /**< Spatial hash initial capacity (default: 256) */
} Agentite_CollisionWorldConfig;

/** Default world configuration */
#define AGENTITE_COLLISION_WORLD_DEFAULT { \
    .max_colliders = 1024, \
    .cell_size = 64.0f, \
    .spatial_capacity = 256 \
}

/* ============================================================================
 * Collision World Lifecycle
 * ============================================================================ */

/**
 * Create a collision world.
 * Caller OWNS the returned pointer and MUST call agentite_collision_world_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return Collision world, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_CollisionWorld *agentite_collision_world_create(
    const Agentite_CollisionWorldConfig *config);

/**
 * Destroy collision world and all colliders.
 * Safe to call with NULL.
 *
 * @param world Collision world to destroy
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_world_destroy(Agentite_CollisionWorld *world);

/**
 * Remove all colliders from the world.
 * Does not destroy the world itself.
 *
 * @param world Collision world
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_world_clear(Agentite_CollisionWorld *world);

/* ============================================================================
 * Shape Creation
 * ============================================================================ */

/**
 * Create a circle shape.
 * Caller OWNS the returned pointer and MUST call agentite_collision_shape_destroy().
 *
 * @param radius Circle radius (must be > 0)
 * @return Shape, or NULL on failure
 *
 * Thread Safety: Thread-safe (no shared state)
 */
Agentite_CollisionShape *agentite_collision_shape_circle(float radius);

/**
 * Create an axis-aligned bounding box shape.
 * Caller OWNS the returned pointer and MUST call agentite_collision_shape_destroy().
 *
 * @param width Box width (must be > 0)
 * @param height Box height (must be > 0)
 * @return Shape, or NULL on failure
 *
 * Thread Safety: Thread-safe (no shared state)
 */
Agentite_CollisionShape *agentite_collision_shape_aabb(float width, float height);

/**
 * Create an oriented bounding box (rotatable rectangle).
 * Caller OWNS the returned pointer and MUST call agentite_collision_shape_destroy().
 *
 * @param width Box width (must be > 0)
 * @param height Box height (must be > 0)
 * @return Shape, or NULL on failure
 *
 * Note: Rotation is set per-collider using agentite_collision_set_rotation().
 *
 * Thread Safety: Thread-safe (no shared state)
 */
Agentite_CollisionShape *agentite_collision_shape_obb(float width, float height);

/**
 * Create a capsule shape (stadium shape).
 * Caller OWNS the returned pointer and MUST call agentite_collision_shape_destroy().
 *
 * @param radius Radius of end caps (must be > 0)
 * @param length Length of center segment (can be 0 for circle)
 * @param axis Orientation (X or Y axis)
 * @return Shape, or NULL on failure
 *
 * Thread Safety: Thread-safe (no shared state)
 */
Agentite_CollisionShape *agentite_collision_shape_capsule(
    float radius, float length, Agentite_CapsuleAxis axis);

/**
 * Create a convex polygon shape.
 * Caller OWNS the returned pointer and MUST call agentite_collision_shape_destroy().
 *
 * @param vertices Array of vertices in counter-clockwise order
 * @param count Number of vertices (3 to AGENTITE_COLLISION_MAX_POLYGON_VERTS)
 * @return Shape, or NULL on failure (invalid count, non-convex, etc.)
 *
 * Note: Vertices are relative to shape center. The centroid will be computed
 * and vertices adjusted so the shape center is at (0,0).
 *
 * Thread Safety: Thread-safe (no shared state)
 */
Agentite_CollisionShape *agentite_collision_shape_polygon(
    const Agentite_CollisionVec2 *vertices, int count);

/**
 * Destroy a collision shape.
 * Safe to call with NULL.
 *
 * Warning: Do not destroy a shape while colliders are using it.
 *
 * @param shape Shape to destroy
 *
 * Thread Safety: NOT thread-safe (check no colliders reference it)
 */
void agentite_collision_shape_destroy(Agentite_CollisionShape *shape);

/**
 * Get the type of a shape.
 *
 * @param shape Shape to query
 * @return Shape type
 *
 * Thread Safety: Thread-safe (read-only)
 */
Agentite_ShapeType agentite_collision_shape_get_type(const Agentite_CollisionShape *shape);

/**
 * Compute the axis-aligned bounding box of a shape at a given transform.
 *
 * @param shape Shape
 * @param x Position X
 * @param y Position Y
 * @param rotation Rotation in radians
 * @param out_aabb Output AABB
 * @return true on success
 *
 * Thread Safety: Thread-safe (read-only)
 */
bool agentite_collision_shape_compute_aabb(
    const Agentite_CollisionShape *shape,
    float x, float y, float rotation,
    Agentite_AABB *out_aabb);

/* ============================================================================
 * Collider Management
 * ============================================================================ */

/**
 * Add a collider to the world.
 *
 * @param world Collision world
 * @param shape Shape for this collider (must outlive the collider)
 * @param x Initial X position
 * @param y Initial Y position
 * @return Collider ID, or AGENTITE_COLLIDER_INVALID on failure
 *
 * Note: The shape is NOT owned by the collider. The caller must ensure
 * the shape remains valid for the lifetime of the collider.
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_ColliderId agentite_collision_add(
    Agentite_CollisionWorld *world,
    Agentite_CollisionShape *shape,
    float x, float y);

/**
 * Remove a collider from the world.
 *
 * @param world Collision world
 * @param collider Collider ID to remove
 * @return true if removed, false if not found
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_remove(Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/**
 * Check if a collider ID is valid.
 *
 * @param world Collision world
 * @param collider Collider ID to check
 * @return true if valid and active
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_is_valid(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/* ============================================================================
 * Collider Transform
 * ============================================================================ */

/**
 * Set collider position.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param x New X position
 * @param y New Y position
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_set_position(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    float x, float y);

/**
 * Get collider position.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param out_x Output X position (can be NULL)
 * @param out_y Output Y position (can be NULL)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_get_position(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    float *out_x, float *out_y);

/**
 * Set collider rotation (for OBB and polygon shapes).
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param radians Rotation in radians
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_set_rotation(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    float radians);

/**
 * Get collider rotation.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @return Rotation in radians
 *
 * Thread Safety: NOT thread-safe
 */
float agentite_collision_get_rotation(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/**
 * Get the world-space AABB of a collider.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param out_aabb Output AABB
 * @return true on success
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_get_aabb(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    Agentite_AABB *out_aabb);

/* ============================================================================
 * Collision Layers
 * ============================================================================ */

/**
 * Set the collision layer for a collider.
 * A collider's layer determines what "group" it belongs to.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param layer Layer bitmask (can be multiple layers OR'd together)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_set_layer(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    uint32_t layer);

/**
 * Get the collision layer for a collider.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @return Layer bitmask
 *
 * Thread Safety: NOT thread-safe
 */
uint32_t agentite_collision_get_layer(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/**
 * Set the collision mask for a collider.
 * A collider only collides with other colliders whose layer matches this mask.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param mask Layer mask (collides with layers where (other.layer & mask) != 0)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_set_mask(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    uint32_t mask);

/**
 * Get the collision mask for a collider.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @return Collision mask
 *
 * Thread Safety: NOT thread-safe
 */
uint32_t agentite_collision_get_mask(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/**
 * Set user data for a collider.
 * This can be used to associate game-specific data with each collider.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param user_data Arbitrary user data (not owned by collision system)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_set_user_data(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    void *user_data);

/**
 * Get user data for a collider.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @return User data, or NULL if not set
 *
 * Thread Safety: NOT thread-safe
 */
void *agentite_collision_get_user_data(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/**
 * Enable or disable a collider.
 * Disabled colliders are skipped in all queries.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @param enabled true to enable, false to disable
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_set_enabled(
    Agentite_CollisionWorld *world, Agentite_ColliderId collider,
    bool enabled);

/**
 * Check if a collider is enabled.
 *
 * @param world Collision world
 * @param collider Collider ID
 * @return true if enabled
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_is_enabled(
    const Agentite_CollisionWorld *world, Agentite_ColliderId collider);

/* ============================================================================
 * Collision Queries
 * ============================================================================ */

/**
 * Test collision between two specific colliders.
 *
 * @param world Collision world
 * @param a First collider
 * @param b Second collider
 * @param out_result Output collision result (can be NULL for boolean test only)
 * @return true if colliding
 *
 * Note: Ignores layer/mask settings.
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_test(
    const Agentite_CollisionWorld *world,
    Agentite_ColliderId a, Agentite_ColliderId b,
    Agentite_CollisionResult *out_result);

/**
 * Query all collisions for a specific collider.
 * Uses broad-phase spatial hash for efficiency.
 *
 * @param world Collision world
 * @param collider Collider to test
 * @param out_results Array to fill with results
 * @param max_results Maximum results to return
 * @return Number of collisions found
 *
 * Note: Respects layer/mask settings.
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_query_collider(
    Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    Agentite_CollisionResult *out_results, int max_results);

/**
 * Query all collisions for a shape at a position (without adding to world).
 *
 * @param world Collision world
 * @param shape Shape to test
 * @param x Position X
 * @param y Position Y
 * @param rotation Rotation in radians
 * @param layer_mask Only test against colliders matching this mask
 * @param out_results Array to fill with results
 * @param max_results Maximum results to return
 * @return Number of collisions found
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_query_shape(
    Agentite_CollisionWorld *world,
    const Agentite_CollisionShape *shape,
    float x, float y, float rotation,
    uint32_t layer_mask,
    Agentite_CollisionResult *out_results, int max_results);

/**
 * Query all colliders within an AABB region.
 *
 * @param world Collision world
 * @param aabb Region to query
 * @param layer_mask Only include colliders matching this mask
 * @param out_colliders Array to fill with collider IDs
 * @param max_results Maximum results to return
 * @return Number of colliders found
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_query_aabb(
    Agentite_CollisionWorld *world,
    const Agentite_AABB *aabb,
    uint32_t layer_mask,
    Agentite_ColliderId *out_colliders, int max_results);

/* ============================================================================
 * Point Queries
 * ============================================================================ */

/**
 * Check if a point is inside a collider.
 *
 * @param world Collision world
 * @param collider Collider to test
 * @param x Point X
 * @param y Point Y
 * @return true if point is inside
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_point_test(
    const Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    float x, float y);

/**
 * Find all colliders containing a point.
 *
 * @param world Collision world
 * @param x Point X
 * @param y Point Y
 * @param layer_mask Only include colliders matching this mask
 * @param out_colliders Array to fill with collider IDs
 * @param max_results Maximum results to return
 * @return Number of colliders containing the point
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_query_point(
    Agentite_CollisionWorld *world,
    float x, float y,
    uint32_t layer_mask,
    Agentite_ColliderId *out_colliders, int max_results);

/* ============================================================================
 * Raycast Queries
 * ============================================================================ */

/**
 * Cast a ray and find the first collision.
 *
 * @param world Collision world
 * @param origin_x Ray origin X
 * @param origin_y Ray origin Y
 * @param dir_x Ray direction X (does not need to be normalized)
 * @param dir_y Ray direction Y
 * @param max_distance Maximum ray distance
 * @param layer_mask Only hit colliders matching this mask
 * @param out_hit Output hit information
 * @return true if something was hit
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_raycast(
    Agentite_CollisionWorld *world,
    float origin_x, float origin_y,
    float dir_x, float dir_y,
    float max_distance,
    uint32_t layer_mask,
    Agentite_RaycastHit *out_hit);

/**
 * Cast a ray and find all collisions along it.
 *
 * @param world Collision world
 * @param origin_x Ray origin X
 * @param origin_y Ray origin Y
 * @param dir_x Ray direction X
 * @param dir_y Ray direction Y
 * @param max_distance Maximum ray distance
 * @param layer_mask Only hit colliders matching this mask
 * @param out_hits Array to fill with hits (sorted by distance)
 * @param max_hits Maximum hits to return
 * @return Number of hits
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_raycast_all(
    Agentite_CollisionWorld *world,
    float origin_x, float origin_y,
    float dir_x, float dir_y,
    float max_distance,
    uint32_t layer_mask,
    Agentite_RaycastHit *out_hits, int max_hits);

/* ============================================================================
 * Shape Cast (Sweep) Queries
 * ============================================================================ */

/**
 * Sweep a shape along a path and find the first collision.
 *
 * @param world Collision world
 * @param shape Shape to sweep
 * @param start_x Starting position X
 * @param start_y Starting position Y
 * @param end_x Ending position X
 * @param end_y Ending position Y
 * @param rotation Shape rotation in radians
 * @param layer_mask Only hit colliders matching this mask
 * @param out_hit Output hit information
 * @return true if something was hit
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_shape_cast(
    Agentite_CollisionWorld *world,
    const Agentite_CollisionShape *shape,
    float start_x, float start_y,
    float end_x, float end_y,
    float rotation,
    uint32_t layer_mask,
    Agentite_ShapeCastHit *out_hit);

/**
 * Sweep a collider along a path and find the first collision.
 *
 * @param world Collision world
 * @param collider Collider to sweep
 * @param delta_x Movement delta X
 * @param delta_y Movement delta Y
 * @param out_hit Output hit information
 * @return true if something would be hit
 *
 * Note: Respects layer/mask settings.
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_collision_sweep(
    Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    float delta_x, float delta_y,
    Agentite_ShapeCastHit *out_hit);

/* ============================================================================
 * Direct Shape-vs-Shape Tests
 * ============================================================================ */

/**
 * Test collision between two shapes (without using world).
 *
 * @param shape_a First shape
 * @param pos_a_x Position of first shape
 * @param pos_a_y Position of first shape
 * @param rot_a Rotation of first shape (radians)
 * @param shape_b Second shape
 * @param pos_b_x Position of second shape
 * @param pos_b_y Position of second shape
 * @param rot_b Rotation of second shape (radians)
 * @param out_result Output collision result (can be NULL)
 * @return true if shapes are colliding
 *
 * Thread Safety: Thread-safe (read-only shapes)
 */
bool agentite_collision_test_shapes(
    const Agentite_CollisionShape *shape_a,
    float pos_a_x, float pos_a_y, float rot_a,
    const Agentite_CollisionShape *shape_b,
    float pos_b_x, float pos_b_y, float rot_b,
    Agentite_CollisionResult *out_result);

/**
 * Test if a point is inside a shape.
 *
 * @param shape Shape to test
 * @param shape_x Shape position X
 * @param shape_y Shape position Y
 * @param shape_rot Shape rotation (radians)
 * @param point_x Point X
 * @param point_y Point Y
 * @return true if point is inside shape
 *
 * Thread Safety: Thread-safe (read-only)
 */
bool agentite_collision_point_in_shape(
    const Agentite_CollisionShape *shape,
    float shape_x, float shape_y, float shape_rot,
    float point_x, float point_y);

/**
 * Cast a ray against a single shape.
 *
 * @param shape Shape to test
 * @param shape_x Shape position X
 * @param shape_y Shape position Y
 * @param shape_rot Shape rotation (radians)
 * @param origin_x Ray origin X
 * @param origin_y Ray origin Y
 * @param dir_x Ray direction X (does not need to be normalized)
 * @param dir_y Ray direction Y
 * @param max_distance Maximum distance
 * @param out_hit Output hit information
 * @return true if ray hits shape
 *
 * Thread Safety: Thread-safe (read-only)
 */
bool agentite_collision_raycast_shape(
    const Agentite_CollisionShape *shape,
    float shape_x, float shape_y, float shape_rot,
    float origin_x, float origin_y,
    float dir_x, float dir_y,
    float max_distance,
    Agentite_RaycastHit *out_hit);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * Get total number of active colliders.
 *
 * @param world Collision world
 * @return Number of colliders
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_world_get_count(const Agentite_CollisionWorld *world);

/**
 * Get maximum collider capacity.
 *
 * @param world Collision world
 * @return Maximum colliders
 *
 * Thread Safety: NOT thread-safe
 */
int agentite_collision_world_get_capacity(const Agentite_CollisionWorld *world);

/* ============================================================================
 * Debug Visualization
 * ============================================================================ */

/* Forward declaration */
struct Agentite_Gizmos;

/**
 * Draw all colliders using gizmos for debugging.
 *
 * @param world Collision world
 * @param gizmos Gizmos renderer
 * @param color Color for shapes (RGBA 0-1)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_debug_draw(
    const Agentite_CollisionWorld *world,
    struct Agentite_Gizmos *gizmos,
    float color[4]);

/**
 * Draw a single collider using gizmos.
 *
 * @param world Collision world
 * @param collider Collider to draw
 * @param gizmos Gizmos renderer
 * @param color Color for shape (RGBA 0-1)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_debug_draw_collider(
    const Agentite_CollisionWorld *world,
    Agentite_ColliderId collider,
    struct Agentite_Gizmos *gizmos,
    float color[4]);

/**
 * Draw a shape at a position using gizmos.
 *
 * @param shape Shape to draw
 * @param x Position X
 * @param y Position Y
 * @param rotation Rotation in radians
 * @param gizmos Gizmos renderer
 * @param color Color for shape (RGBA 0-1)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_collision_debug_draw_shape(
    const Agentite_CollisionShape *shape,
    float x, float y, float rotation,
    struct Agentite_Gizmos *gizmos,
    float color[4]);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_COLLISION_H */
