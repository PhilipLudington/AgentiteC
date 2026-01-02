/**
 * @file physics2d.h
 * @brief Chipmunk2D Physics Integration
 *
 * High-level wrapper around Chipmunk2D for full 2D rigid body physics.
 * Use this for games requiring joints, constraints, continuous collision,
 * or realistic physics simulation.
 *
 * For simple gameplay physics (platformers, basic movement), consider using
 * the lighter-weight kinematic physics system in physics.h instead.
 *
 * Features:
 * - Rigid body physics with mass, moment of inertia
 * - Shape types: circle, box, polygon, segment
 * - Constraints: pin, slide, pivot, groove, damped spring, gear, motor
 * - Collision filtering and callbacks
 * - Sleeping bodies for performance
 * - Debug drawing integration
 * - ECS component integration
 *
 * Usage:
 *   // Create physics space
 *   Agentite_Physics2DConfig config = AGENTITE_PHYSICS2D_DEFAULT;
 *   config.gravity_y = 500.0f;
 *   Agentite_Physics2DSpace *space = agentite_physics2d_space_create(&config);
 *
 *   // Create a dynamic body
 *   Agentite_Physics2DBody *body = agentite_physics2d_body_create_dynamic(space, 1.0f, 1.0f);
 *   agentite_physics2d_body_set_position(body, 100, 100);
 *
 *   // Add a circle shape
 *   Agentite_Physics2DShape *shape = agentite_physics2d_shape_circle(body, 16.0f, 0, 0);
 *   agentite_physics2d_shape_set_friction(shape, 0.7f);
 *
 *   // Each frame:
 *   agentite_physics2d_space_step(space, delta_time);
 *
 *   // Get position for rendering
 *   float x, y;
 *   agentite_physics2d_body_get_position(body, &x, &y);
 *
 *   // Cleanup
 *   agentite_physics2d_space_destroy(space);  // Frees all bodies/shapes
 */

#ifndef AGENTITE_PHYSICS2D_H
#define AGENTITE_PHYSICS2D_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_Physics2DSpace Agentite_Physics2DSpace;
typedef struct Agentite_Physics2DBody Agentite_Physics2DBody;
typedef struct Agentite_Physics2DShape Agentite_Physics2DShape;
typedef struct Agentite_Physics2DConstraint Agentite_Physics2DConstraint;

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Collision type for filtering - use any unique integer */
typedef uint64_t Agentite_Physics2DCollisionType;

/** Group for collision filtering */
typedef uint32_t Agentite_Physics2DGroup;

/** Bitmask for collision filtering */
typedef uint32_t Agentite_Physics2DBitmask;

/** Special collision group values */
#define AGENTITE_PHYSICS2D_NO_GROUP 0

/** Wildcard collision type - matches any type */
#define AGENTITE_PHYSICS2D_WILDCARD_TYPE UINT64_MAX

/* ============================================================================
 * Vector Type
 * ============================================================================ */

/** 2D vector for physics calculations */
typedef struct Agentite_Physics2DVec {
    float x;
    float y;
} Agentite_Physics2DVec;

/** Create a vector inline */
#define AGENTITE_VEC2(x, y) ((Agentite_Physics2DVec){(x), (y)})

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/** Physics space configuration */
typedef struct Agentite_Physics2DConfig {
    float gravity_x;              /**< Gravity X (default: 0) */
    float gravity_y;              /**< Gravity Y (default: 0) */
    int iterations;               /**< Solver iterations (default: 10) */
    float damping;                /**< Global damping 0-1 (default: 1.0 = no damping) */
    float sleep_time_threshold;   /**< Time before bodies sleep (default: disabled) */
    float idle_speed_threshold;   /**< Speed threshold for idle (default: 0 = auto) */
    float collision_slop;         /**< Penetration allowance (default: 0.1) */
    float collision_bias;         /**< Overlap correction rate (default: 0.1) */
} Agentite_Physics2DConfig;

/** Default space configuration */
#define AGENTITE_PHYSICS2D_DEFAULT { \
    .gravity_x = 0.0f, \
    .gravity_y = 0.0f, \
    .iterations = 10, \
    .damping = 1.0f, \
    .sleep_time_threshold = -1.0f, \
    .idle_speed_threshold = 0.0f, \
    .collision_slop = 0.1f, \
    .collision_bias = 0.1f \
}

/* ============================================================================
 * Collision Callback Types
 * ============================================================================ */

/** Contact point information */
typedef struct Agentite_Physics2DContactPoint {
    Agentite_Physics2DVec point_a;    /**< Contact point on shape A */
    Agentite_Physics2DVec point_b;    /**< Contact point on shape B */
    float distance;                    /**< Penetration distance (negative = overlap) */
} Agentite_Physics2DContactPoint;

/** Collision info passed to callbacks */
typedef struct Agentite_Physics2DCollision {
    Agentite_Physics2DShape *shape_a;  /**< First shape in collision */
    Agentite_Physics2DShape *shape_b;  /**< Second shape in collision */
    Agentite_Physics2DVec normal;      /**< Collision normal from A to B */
    int contact_count;                 /**< Number of contact points */
    Agentite_Physics2DContactPoint contacts[2]; /**< Contact points */
    float restitution;                 /**< Combined restitution */
    float friction;                    /**< Combined friction */
    Agentite_Physics2DVec surface_velocity; /**< Relative surface velocity */
} Agentite_Physics2DCollision;

/**
 * Collision begin callback.
 * Called when two shapes first start colliding.
 * Return false to ignore the collision this step.
 */
typedef bool (*Agentite_Physics2DCollisionBeginFunc)(
    Agentite_Physics2DCollision *collision,
    void *user_data);

/**
 * Collision pre-solve callback.
 * Called each step before solver runs.
 * Return false to ignore the collision this step.
 */
typedef bool (*Agentite_Physics2DCollisionPreSolveFunc)(
    Agentite_Physics2DCollision *collision,
    void *user_data);

/**
 * Collision post-solve callback.
 * Called each step after solver runs.
 */
typedef void (*Agentite_Physics2DCollisionPostSolveFunc)(
    Agentite_Physics2DCollision *collision,
    void *user_data);

/**
 * Collision separate callback.
 * Called when two shapes stop colliding.
 */
typedef void (*Agentite_Physics2DCollisionSeparateFunc)(
    Agentite_Physics2DCollision *collision,
    void *user_data);

/** Collision handler configuration */
typedef struct Agentite_Physics2DCollisionHandler {
    Agentite_Physics2DCollisionBeginFunc begin;
    Agentite_Physics2DCollisionPreSolveFunc pre_solve;
    Agentite_Physics2DCollisionPostSolveFunc post_solve;
    Agentite_Physics2DCollisionSeparateFunc separate;
    void *user_data;
} Agentite_Physics2DCollisionHandler;

/* ============================================================================
 * Space Lifecycle
 * ============================================================================ */

/**
 * Create a physics space.
 * Caller OWNS and MUST call agentite_physics2d_space_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return Physics space, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_Physics2DSpace *agentite_physics2d_space_create(
    const Agentite_Physics2DConfig *config);

/**
 * Destroy physics space and all bodies/shapes/constraints.
 * Safe to call with NULL.
 *
 * @param space Space to destroy
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics2d_space_destroy(Agentite_Physics2DSpace *space);

/**
 * Step the physics simulation.
 *
 * @param space Physics space
 * @param dt Time step in seconds
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics2d_space_step(Agentite_Physics2DSpace *space, float dt);

/* ============================================================================
 * Space Properties
 * ============================================================================ */

/**
 * Set gravity.
 */
void agentite_physics2d_space_set_gravity(Agentite_Physics2DSpace *space, float x, float y);

/**
 * Get gravity.
 */
void agentite_physics2d_space_get_gravity(const Agentite_Physics2DSpace *space, float *x, float *y);

/**
 * Set damping (velocity retained per second, 1.0 = no damping).
 */
void agentite_physics2d_space_set_damping(Agentite_Physics2DSpace *space, float damping);

/**
 * Get damping.
 */
float agentite_physics2d_space_get_damping(const Agentite_Physics2DSpace *space);

/**
 * Set solver iterations.
 */
void agentite_physics2d_space_set_iterations(Agentite_Physics2DSpace *space, int iterations);

/**
 * Get current time step (useful in callbacks).
 */
float agentite_physics2d_space_get_current_timestep(const Agentite_Physics2DSpace *space);

/**
 * Check if space is locked (in callback, can't add/remove objects).
 */
bool agentite_physics2d_space_is_locked(const Agentite_Physics2DSpace *space);

/* ============================================================================
 * Body Creation
 * ============================================================================ */

/**
 * Create a dynamic body.
 * Dynamic bodies are affected by gravity and collisions.
 *
 * @param space Physics space
 * @param mass Mass (must be > 0)
 * @param moment Moment of inertia (use helpers or 0 for auto)
 * @return Body, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_Physics2DBody *agentite_physics2d_body_create_dynamic(
    Agentite_Physics2DSpace *space,
    float mass,
    float moment);

/**
 * Create a kinematic body.
 * Kinematic bodies move only by velocity, not affected by forces.
 *
 * @param space Physics space
 * @return Body, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_Physics2DBody *agentite_physics2d_body_create_kinematic(
    Agentite_Physics2DSpace *space);

/**
 * Create a static body.
 * Static bodies never move and have infinite mass.
 * Each space has a built-in static body; use this for additional ones.
 *
 * @param space Physics space
 * @return Body, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_Physics2DBody *agentite_physics2d_body_create_static(
    Agentite_Physics2DSpace *space);

/**
 * Get the built-in static body for the space.
 * Use this for static scenery like walls and platforms.
 *
 * @param space Physics space
 * @return Static body (owned by space, do not destroy)
 */
Agentite_Physics2DBody *agentite_physics2d_space_get_static_body(
    Agentite_Physics2DSpace *space);

/**
 * Destroy a body and remove all attached shapes/constraints.
 * Safe to call with NULL.
 *
 * @param body Body to destroy
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics2d_body_destroy(Agentite_Physics2DBody *body);

/* ============================================================================
 * Moment of Inertia Helpers
 * ============================================================================ */

/**
 * Calculate moment of inertia for a circle.
 */
float agentite_physics2d_moment_for_circle(float mass, float inner_radius, float outer_radius,
                                           float offset_x, float offset_y);

/**
 * Calculate moment of inertia for a box.
 */
float agentite_physics2d_moment_for_box(float mass, float width, float height);

/**
 * Calculate moment of inertia for a polygon.
 */
float agentite_physics2d_moment_for_polygon(float mass, int vertex_count,
                                            const Agentite_Physics2DVec *vertices,
                                            float offset_x, float offset_y,
                                            float radius);

/**
 * Calculate moment of inertia for a segment (line).
 */
float agentite_physics2d_moment_for_segment(float mass, float ax, float ay,
                                            float bx, float by, float radius);

/* ============================================================================
 * Body Transform
 * ============================================================================ */

void agentite_physics2d_body_set_position(Agentite_Physics2DBody *body, float x, float y);
void agentite_physics2d_body_get_position(const Agentite_Physics2DBody *body, float *x, float *y);

void agentite_physics2d_body_set_angle(Agentite_Physics2DBody *body, float radians);
float agentite_physics2d_body_get_angle(const Agentite_Physics2DBody *body);

void agentite_physics2d_body_set_velocity(Agentite_Physics2DBody *body, float vx, float vy);
void agentite_physics2d_body_get_velocity(const Agentite_Physics2DBody *body, float *vx, float *vy);

void agentite_physics2d_body_set_angular_velocity(Agentite_Physics2DBody *body, float w);
float agentite_physics2d_body_get_angular_velocity(const Agentite_Physics2DBody *body);

/* ============================================================================
 * Body Properties
 * ============================================================================ */

void agentite_physics2d_body_set_mass(Agentite_Physics2DBody *body, float mass);
float agentite_physics2d_body_get_mass(const Agentite_Physics2DBody *body);

void agentite_physics2d_body_set_moment(Agentite_Physics2DBody *body, float moment);
float agentite_physics2d_body_get_moment(const Agentite_Physics2DBody *body);

void agentite_physics2d_body_set_center_of_gravity(Agentite_Physics2DBody *body, float x, float y);
void agentite_physics2d_body_get_center_of_gravity(const Agentite_Physics2DBody *body, float *x, float *y);

/* ============================================================================
 * Body Forces and Impulses
 * ============================================================================ */

/**
 * Apply force at world point.
 */
void agentite_physics2d_body_apply_force_at_world(
    Agentite_Physics2DBody *body,
    float fx, float fy,
    float px, float py);

/**
 * Apply force at local point.
 */
void agentite_physics2d_body_apply_force_at_local(
    Agentite_Physics2DBody *body,
    float fx, float fy,
    float px, float py);

/**
 * Apply impulse at world point.
 */
void agentite_physics2d_body_apply_impulse_at_world(
    Agentite_Physics2DBody *body,
    float ix, float iy,
    float px, float py);

/**
 * Apply impulse at local point.
 */
void agentite_physics2d_body_apply_impulse_at_local(
    Agentite_Physics2DBody *body,
    float ix, float iy,
    float px, float py);

/**
 * Get force currently applied to body.
 */
void agentite_physics2d_body_get_force(const Agentite_Physics2DBody *body, float *fx, float *fy);

/**
 * Get torque currently applied to body.
 */
float agentite_physics2d_body_get_torque(const Agentite_Physics2DBody *body);

/* ============================================================================
 * Body Coordinate Conversion
 * ============================================================================ */

/** Convert local point to world coordinates */
void agentite_physics2d_body_local_to_world(const Agentite_Physics2DBody *body,
                                            float lx, float ly,
                                            float *wx, float *wy);

/** Convert world point to local coordinates */
void agentite_physics2d_body_world_to_local(const Agentite_Physics2DBody *body,
                                            float wx, float wy,
                                            float *lx, float *ly);

/** Get velocity at world point on body */
void agentite_physics2d_body_velocity_at_world_point(const Agentite_Physics2DBody *body,
                                                     float px, float py,
                                                     float *vx, float *vy);

/** Get velocity at local point on body */
void agentite_physics2d_body_velocity_at_local_point(const Agentite_Physics2DBody *body,
                                                     float px, float py,
                                                     float *vx, float *vy);

/* ============================================================================
 * Body Sleep State
 * ============================================================================ */

bool agentite_physics2d_body_is_sleeping(const Agentite_Physics2DBody *body);
void agentite_physics2d_body_sleep(Agentite_Physics2DBody *body);
void agentite_physics2d_body_activate(Agentite_Physics2DBody *body);

/* ============================================================================
 * Body User Data
 * ============================================================================ */

void agentite_physics2d_body_set_user_data(Agentite_Physics2DBody *body, void *data);
void *agentite_physics2d_body_get_user_data(const Agentite_Physics2DBody *body);

/* ============================================================================
 * Shape Creation
 * ============================================================================ */

/**
 * Create a circle shape.
 *
 * @param body Body to attach to
 * @param radius Circle radius
 * @param offset_x Center offset X
 * @param offset_y Center offset Y
 * @return Shape, or NULL on failure
 */
Agentite_Physics2DShape *agentite_physics2d_shape_circle(
    Agentite_Physics2DBody *body,
    float radius,
    float offset_x, float offset_y);

/**
 * Create a box shape centered on body.
 *
 * @param body Body to attach to
 * @param width Box width
 * @param height Box height
 * @param radius Corner radius (0 for sharp corners)
 * @return Shape, or NULL on failure
 */
Agentite_Physics2DShape *agentite_physics2d_shape_box(
    Agentite_Physics2DBody *body,
    float width, float height,
    float radius);

/**
 * Create a box shape with offset.
 *
 * @param body Body to attach to
 * @param left Left edge X
 * @param bottom Bottom edge Y
 * @param right Right edge X
 * @param top Top edge Y
 * @param radius Corner radius
 * @return Shape, or NULL on failure
 */
Agentite_Physics2DShape *agentite_physics2d_shape_box_offset(
    Agentite_Physics2DBody *body,
    float left, float bottom,
    float right, float top,
    float radius);

/**
 * Create a convex polygon shape.
 *
 * @param body Body to attach to
 * @param vertex_count Number of vertices
 * @param vertices Vertex array (convex, counter-clockwise)
 * @param radius Skin radius
 * @return Shape, or NULL on failure
 */
Agentite_Physics2DShape *agentite_physics2d_shape_polygon(
    Agentite_Physics2DBody *body,
    int vertex_count,
    const Agentite_Physics2DVec *vertices,
    float radius);

/**
 * Create a segment (line) shape.
 *
 * @param body Body to attach to
 * @param ax Start point X
 * @param ay Start point Y
 * @param bx End point X
 * @param by End point Y
 * @param radius Thickness/radius
 * @return Shape, or NULL on failure
 */
Agentite_Physics2DShape *agentite_physics2d_shape_segment(
    Agentite_Physics2DBody *body,
    float ax, float ay,
    float bx, float by,
    float radius);

/**
 * Destroy a shape.
 * Safe to call with NULL.
 *
 * @param shape Shape to destroy
 */
void agentite_physics2d_shape_destroy(Agentite_Physics2DShape *shape);

/* ============================================================================
 * Shape Properties
 * ============================================================================ */

void agentite_physics2d_shape_set_friction(Agentite_Physics2DShape *shape, float friction);
float agentite_physics2d_shape_get_friction(const Agentite_Physics2DShape *shape);

void agentite_physics2d_shape_set_elasticity(Agentite_Physics2DShape *shape, float elasticity);
float agentite_physics2d_shape_get_elasticity(const Agentite_Physics2DShape *shape);

void agentite_physics2d_shape_set_surface_velocity(Agentite_Physics2DShape *shape, float vx, float vy);
void agentite_physics2d_shape_get_surface_velocity(const Agentite_Physics2DShape *shape, float *vx, float *vy);

void agentite_physics2d_shape_set_sensor(Agentite_Physics2DShape *shape, bool is_sensor);
bool agentite_physics2d_shape_is_sensor(const Agentite_Physics2DShape *shape);

/* ============================================================================
 * Shape Collision Filtering
 * ============================================================================ */

/**
 * Set collision type for collision handler matching.
 */
void agentite_physics2d_shape_set_collision_type(
    Agentite_Physics2DShape *shape,
    Agentite_Physics2DCollisionType type);

/**
 * Get collision type.
 */
Agentite_Physics2DCollisionType agentite_physics2d_shape_get_collision_type(
    const Agentite_Physics2DShape *shape);

/**
 * Set collision filter.
 *
 * Group: Shapes in same non-zero group never collide.
 * Categories: Bitmask of categories this shape belongs to.
 * Mask: Bitmask of categories this shape collides with.
 *
 * Collision occurs when:
 *   (a.group == 0 || a.group != b.group) &&
 *   (a.categories & b.mask) != 0 &&
 *   (b.categories & a.mask) != 0
 */
void agentite_physics2d_shape_set_filter(
    Agentite_Physics2DShape *shape,
    Agentite_Physics2DGroup group,
    Agentite_Physics2DBitmask categories,
    Agentite_Physics2DBitmask mask);

/**
 * Get collision filter group.
 */
Agentite_Physics2DGroup agentite_physics2d_shape_get_filter_group(
    const Agentite_Physics2DShape *shape);

/**
 * Get collision filter categories.
 */
Agentite_Physics2DBitmask agentite_physics2d_shape_get_filter_categories(
    const Agentite_Physics2DShape *shape);

/**
 * Get collision filter mask.
 */
Agentite_Physics2DBitmask agentite_physics2d_shape_get_filter_mask(
    const Agentite_Physics2DShape *shape);

/* ============================================================================
 * Shape User Data
 * ============================================================================ */

void agentite_physics2d_shape_set_user_data(Agentite_Physics2DShape *shape, void *data);
void *agentite_physics2d_shape_get_user_data(const Agentite_Physics2DShape *shape);

/** Get the body a shape is attached to */
Agentite_Physics2DBody *agentite_physics2d_shape_get_body(const Agentite_Physics2DShape *shape);

/* ============================================================================
 * Collision Handlers
 * ============================================================================ */

/**
 * Set default collision handler for all collisions.
 *
 * @param space Physics space
 * @param handler Handler configuration
 */
void agentite_physics2d_space_set_default_collision_handler(
    Agentite_Physics2DSpace *space,
    const Agentite_Physics2DCollisionHandler *handler);

/**
 * Add collision handler for specific collision types.
 *
 * @param space Physics space
 * @param type_a First collision type
 * @param type_b Second collision type
 * @param handler Handler configuration
 */
void agentite_physics2d_space_add_collision_handler(
    Agentite_Physics2DSpace *space,
    Agentite_Physics2DCollisionType type_a,
    Agentite_Physics2DCollisionType type_b,
    const Agentite_Physics2DCollisionHandler *handler);

/**
 * Add wildcard collision handler.
 * Called for any collision involving shapes with the given type.
 *
 * @param space Physics space
 * @param type Collision type
 * @param handler Handler configuration
 */
void agentite_physics2d_space_add_wildcard_handler(
    Agentite_Physics2DSpace *space,
    Agentite_Physics2DCollisionType type,
    const Agentite_Physics2DCollisionHandler *handler);

/* ============================================================================
 * Constraints (Joints)
 * ============================================================================ */

/**
 * Create a pin joint (fixed distance constraint).
 *
 * @param body_a First body
 * @param body_b Second body
 * @param anchor_ax Anchor point on body A (local coords)
 * @param anchor_ay Anchor point on body A (local coords)
 * @param anchor_bx Anchor point on body B (local coords)
 * @param anchor_by Anchor point on body B (local coords)
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_pin_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by);

/**
 * Create a slide joint (min/max distance constraint).
 *
 * @param body_a First body
 * @param body_b Second body
 * @param anchor_ax Anchor point on body A (local coords)
 * @param anchor_ay Anchor point on body A (local coords)
 * @param anchor_bx Anchor point on body B (local coords)
 * @param anchor_by Anchor point on body B (local coords)
 * @param min Minimum distance
 * @param max Maximum distance
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_slide_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by,
    float min, float max);

/**
 * Create a pivot joint (rotation around shared point).
 *
 * @param body_a First body
 * @param body_b Second body
 * @param pivot_x World pivot point X
 * @param pivot_y World pivot point Y
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_pivot_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float pivot_x, float pivot_y);

/**
 * Create a pivot joint with separate anchors.
 *
 * @param body_a First body
 * @param body_b Second body
 * @param anchor_ax Anchor on body A (local coords)
 * @param anchor_ay Anchor on body A (local coords)
 * @param anchor_bx Anchor on body B (local coords)
 * @param anchor_by Anchor on body B (local coords)
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_pivot_joint_create2(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by);

/**
 * Create a groove joint (pivot on line segment).
 *
 * @param body_a Body with groove
 * @param body_b Body with pivot
 * @param groove_ax Groove start (local to A)
 * @param groove_ay Groove start (local to A)
 * @param groove_bx Groove end (local to A)
 * @param groove_by Groove end (local to A)
 * @param anchor_bx Anchor on body B (local coords)
 * @param anchor_by Anchor on body B (local coords)
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_groove_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float groove_ax, float groove_ay,
    float groove_bx, float groove_by,
    float anchor_bx, float anchor_by);

/**
 * Create a damped spring.
 *
 * @param body_a First body
 * @param body_b Second body
 * @param anchor_ax Anchor on body A (local coords)
 * @param anchor_ay Anchor on body A (local coords)
 * @param anchor_bx Anchor on body B (local coords)
 * @param anchor_by Anchor on body B (local coords)
 * @param rest_length Rest length of spring
 * @param stiffness Spring constant
 * @param damping Damping coefficient
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_damped_spring_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by,
    float rest_length,
    float stiffness,
    float damping);

/**
 * Create a damped rotary spring.
 *
 * @param body_a First body
 * @param body_b Second body
 * @param rest_angle Rest angle between bodies
 * @param stiffness Spring constant
 * @param damping Damping coefficient
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_damped_rotary_spring_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float rest_angle,
    float stiffness,
    float damping);

/**
 * Create a rotary limit joint.
 *
 * @param body_a First body
 * @param body_b Second body
 * @param min Minimum angle
 * @param max Maximum angle
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_rotary_limit_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float min, float max);

/**
 * Create a ratchet joint (one-way rotation).
 *
 * @param body_a First body
 * @param body_b Second body
 * @param phase Initial offset
 * @param ratchet Angular distance between clicks
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_ratchet_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float phase, float ratchet);

/**
 * Create a gear joint (linked rotation).
 *
 * @param body_a First body
 * @param body_b Second body
 * @param phase Initial angular offset
 * @param ratio Gear ratio
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_gear_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float phase, float ratio);

/**
 * Create a simple motor.
 *
 * @param body_a First body
 * @param body_b Second body
 * @param rate Angular velocity
 * @return Constraint, or NULL on failure
 */
Agentite_Physics2DConstraint *agentite_physics2d_simple_motor_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float rate);

/**
 * Destroy a constraint.
 * Safe to call with NULL.
 *
 * @param constraint Constraint to destroy
 */
void agentite_physics2d_constraint_destroy(Agentite_Physics2DConstraint *constraint);

/* ============================================================================
 * Constraint Properties
 * ============================================================================ */

/**
 * Set maximum force the constraint can apply.
 */
void agentite_physics2d_constraint_set_max_force(
    Agentite_Physics2DConstraint *constraint, float max_force);

/**
 * Get maximum force.
 */
float agentite_physics2d_constraint_get_max_force(
    const Agentite_Physics2DConstraint *constraint);

/**
 * Set error bias (correction rate).
 */
void agentite_physics2d_constraint_set_error_bias(
    Agentite_Physics2DConstraint *constraint, float bias);

/**
 * Get error bias.
 */
float agentite_physics2d_constraint_get_error_bias(
    const Agentite_Physics2DConstraint *constraint);

/**
 * Set maximum error bias.
 */
void agentite_physics2d_constraint_set_max_bias(
    Agentite_Physics2DConstraint *constraint, float max_bias);

/**
 * Get maximum error bias.
 */
float agentite_physics2d_constraint_get_max_bias(
    const Agentite_Physics2DConstraint *constraint);

/**
 * Set whether bodies can collide.
 */
void agentite_physics2d_constraint_set_collide_bodies(
    Agentite_Physics2DConstraint *constraint, bool collide);

/**
 * Get whether bodies can collide.
 */
bool agentite_physics2d_constraint_get_collide_bodies(
    const Agentite_Physics2DConstraint *constraint);

/**
 * Get impulse applied last step.
 */
float agentite_physics2d_constraint_get_impulse(
    const Agentite_Physics2DConstraint *constraint);

/* ============================================================================
 * Constraint User Data
 * ============================================================================ */

void agentite_physics2d_constraint_set_user_data(
    Agentite_Physics2DConstraint *constraint, void *data);

void *agentite_physics2d_constraint_get_user_data(
    const Agentite_Physics2DConstraint *constraint);

/* ============================================================================
 * Space Queries
 * ============================================================================ */

/** Point query result */
typedef struct Agentite_Physics2DPointQueryInfo {
    Agentite_Physics2DShape *shape;  /**< Hit shape */
    float point_x;                    /**< Closest point X */
    float point_y;                    /**< Closest point Y */
    float distance;                   /**< Distance to point (negative = inside) */
    float gradient_x;                 /**< Outward direction X */
    float gradient_y;                 /**< Outward direction Y */
} Agentite_Physics2DPointQueryInfo;

/** Segment query result */
typedef struct Agentite_Physics2DSegmentQueryInfo {
    Agentite_Physics2DShape *shape;  /**< Hit shape */
    float point_x;                    /**< Hit point X */
    float point_y;                    /**< Hit point Y */
    float normal_x;                   /**< Surface normal X */
    float normal_y;                   /**< Surface normal Y */
    float alpha;                      /**< Hit fraction along segment (0-1) */
} Agentite_Physics2DSegmentQueryInfo;

/**
 * Query for shape at point.
 *
 * @param space Physics space
 * @param px Point X
 * @param py Point Y
 * @param radius Query radius
 * @param filter_group Group filter
 * @param filter_categories Categories filter
 * @param filter_mask Mask filter
 * @param out_info Output query info (can be NULL)
 * @return Nearest shape, or NULL if none
 */
Agentite_Physics2DShape *agentite_physics2d_space_point_query_nearest(
    Agentite_Physics2DSpace *space,
    float px, float py,
    float radius,
    Agentite_Physics2DGroup filter_group,
    Agentite_Physics2DBitmask filter_categories,
    Agentite_Physics2DBitmask filter_mask,
    Agentite_Physics2DPointQueryInfo *out_info);

/**
 * Query for shape along segment.
 *
 * @param space Physics space
 * @param ax Segment start X
 * @param ay Segment start Y
 * @param bx Segment end X
 * @param by Segment end Y
 * @param radius Query radius
 * @param filter_group Group filter
 * @param filter_categories Categories filter
 * @param filter_mask Mask filter
 * @param out_info Output query info (can be NULL)
 * @return First hit shape, or NULL if none
 */
Agentite_Physics2DShape *agentite_physics2d_space_segment_query_first(
    Agentite_Physics2DSpace *space,
    float ax, float ay,
    float bx, float by,
    float radius,
    Agentite_Physics2DGroup filter_group,
    Agentite_Physics2DBitmask filter_categories,
    Agentite_Physics2DBitmask filter_mask,
    Agentite_Physics2DSegmentQueryInfo *out_info);

/* ============================================================================
 * Debug Drawing
 * ============================================================================ */

/* Forward declaration */
struct Agentite_Gizmos;

/**
 * Draw physics debug visualization.
 *
 * @param space Physics space
 * @param gizmos Gizmos renderer
 */
void agentite_physics2d_debug_draw(
    const Agentite_Physics2DSpace *space,
    struct Agentite_Gizmos *gizmos);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * Get number of bodies in space.
 */
int agentite_physics2d_space_get_body_count(const Agentite_Physics2DSpace *space);

/**
 * Get number of shapes in space.
 */
int agentite_physics2d_space_get_shape_count(const Agentite_Physics2DSpace *space);

/**
 * Get number of constraints in space.
 */
int agentite_physics2d_space_get_constraint_count(const Agentite_Physics2DSpace *space);

/* ============================================================================
 * Space User Data
 * ============================================================================ */

void agentite_physics2d_space_set_user_data(Agentite_Physics2DSpace *space, void *data);
void *agentite_physics2d_space_get_user_data(const Agentite_Physics2DSpace *space);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_PHYSICS2D_H */
