/**
 * @file physics.h
 * @brief Simple 2D Kinematic Physics System
 *
 * Provides basic physics simulation for 2D games including movement, gravity,
 * collision response, and trigger volumes. Designed for simple gameplay physics
 * without requiring a full physics engine.
 *
 * ## When to Use This (Simple Physics) vs Chipmunk2D (physics2d.h)
 *
 * Use THIS system (agentite_physics_*) for:
 * - Platformers with simple movement
 * - Top-down games with basic movement
 * - Games needing only AABB/circle collision response
 * - Arcade-style physics (not realistic)
 * - When you want full control over movement behavior
 * - Lower CPU overhead for many simple objects
 *
 * Use CHIPMUNK2D (agentite_physics2d_*) for:
 * - Realistic rigid body physics (rotation, momentum)
 * - Games requiring joints/constraints (ragdolls, chains, vehicles)
 * - Physics puzzles (Angry Birds, Cut the Rope style)
 * - Complex shapes (convex polygons)
 * - Stacking/piling objects realistically
 * - Continuous collision detection (fast-moving objects)
 * - When objects need to interact physically with each other
 *
 * Both systems can coexist - use simple physics for player/enemies
 * and Chipmunk2D for physics-based puzzle elements.
 *
 * Features:
 * - Kinematic bodies with velocity and acceleration
 * - Global and per-body gravity
 * - Drag and friction
 * - Collision response: bounce, slide, stop
 * - Trigger volumes for detection without physical response
 * - Fixed timestep with accumulator
 * - Integration with collision system
 *
 * Usage:
 *   // Create physics world
 *   Agentite_PhysicsWorldConfig config = AGENTITE_PHYSICS_WORLD_DEFAULT;
 *   config.gravity_y = 400.0f;  // Pixels/sec^2 downward
 *   Agentite_PhysicsWorld *physics = agentite_physics_world_create(&config);
 *
 *   // Create a collision world for physics to use
 *   Agentite_CollisionWorld *collision = agentite_collision_world_create(NULL);
 *   agentite_physics_set_collision_world(physics, collision);
 *
 *   // Add a body
 *   Agentite_PhysicsBodyConfig body_cfg = AGENTITE_PHYSICS_BODY_DEFAULT;
 *   body_cfg.type = AGENTITE_BODY_DYNAMIC;
 *   body_cfg.mass = 1.0f;
 *   body_cfg.drag = 0.01f;
 *   Agentite_PhysicsBody *body = agentite_physics_body_create(physics, &body_cfg);
 *   agentite_physics_body_set_position(body, 100, 100);
 *
 *   // Attach a collider shape
 *   Agentite_CollisionShape *shape = agentite_collision_shape_circle(16.0f);
 *   agentite_physics_body_set_shape(body, shape);
 *
 *   // Each frame:
 *   agentite_physics_world_step(physics, delta_time);
 *
 *   // Cleanup
 *   agentite_physics_body_destroy(body);
 *   agentite_collision_shape_destroy(shape);
 *   agentite_collision_world_destroy(collision);
 *   agentite_physics_world_destroy(physics);
 */

#ifndef AGENTITE_PHYSICS_H
#define AGENTITE_PHYSICS_H

#include "collision.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_PhysicsWorld Agentite_PhysicsWorld;
typedef struct Agentite_PhysicsBody Agentite_PhysicsBody;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** Body type determines how it moves and interacts */
typedef enum Agentite_BodyType {
    AGENTITE_BODY_STATIC,       /**< Never moves, infinite mass */
    AGENTITE_BODY_KINEMATIC,    /**< Moves by velocity, ignores forces */
    AGENTITE_BODY_DYNAMIC       /**< Moves by forces, responds to gravity */
} Agentite_BodyType;

/** Collision response behavior */
typedef enum Agentite_CollisionResponse {
    AGENTITE_RESPONSE_NONE,     /**< Trigger only, no physical response */
    AGENTITE_RESPONSE_STOP,     /**< Stop on collision (zero velocity) */
    AGENTITE_RESPONSE_SLIDE,    /**< Slide along surface */
    AGENTITE_RESPONSE_BOUNCE    /**< Bounce off surface */
} Agentite_CollisionResponse;

/* ============================================================================
 * Callback Types
 * ============================================================================ */

/**
 * Collision callback function type.
 *
 * @param body_a     First body in collision
 * @param body_b     Second body in collision
 * @param result     Collision result with contact information
 * @param user_data  User data passed to callback registration
 * @return true to allow default collision response, false to skip it
 */
typedef bool (*Agentite_PhysicsCollisionCallback)(
    Agentite_PhysicsBody *body_a,
    Agentite_PhysicsBody *body_b,
    const Agentite_CollisionResult *result,
    void *user_data);

/**
 * Trigger callback function type.
 *
 * @param trigger    The trigger body
 * @param other      The body that entered/exited the trigger
 * @param is_enter   true if entering, false if exiting
 * @param user_data  User data passed to callback registration
 */
typedef void (*Agentite_PhysicsTriggerCallback)(
    Agentite_PhysicsBody *trigger,
    Agentite_PhysicsBody *other,
    bool is_enter,
    void *user_data);

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/** Physics world configuration */
typedef struct Agentite_PhysicsWorldConfig {
    float gravity_x;             /**< Global gravity X (default: 0) */
    float gravity_y;             /**< Global gravity Y (default: 0) */
    float fixed_timestep;        /**< Fixed step interval (default: 1/60) */
    int max_substeps;            /**< Max substeps per frame (default: 8) */
    uint32_t max_bodies;         /**< Maximum bodies (default: 1024) */
} Agentite_PhysicsWorldConfig;

/** Default world configuration */
#define AGENTITE_PHYSICS_WORLD_DEFAULT { \
    .gravity_x = 0.0f, \
    .gravity_y = 0.0f, \
    .fixed_timestep = 1.0f / 60.0f, \
    .max_substeps = 8, \
    .max_bodies = 1024 \
}

/** Physics body configuration */
typedef struct Agentite_PhysicsBodyConfig {
    Agentite_BodyType type;      /**< Body type (default: dynamic) */
    float mass;                  /**< Mass, affects impulse response (default: 1) */
    float drag;                  /**< Linear drag coefficient (default: 0) */
    float angular_drag;          /**< Angular drag coefficient (default: 0) */
    float bounce;                /**< Bounciness/restitution 0-1 (default: 0) */
    float friction;              /**< Surface friction 0-1 (default: 0.5) */
    float gravity_scale;         /**< Per-body gravity multiplier (default: 1) */
    Agentite_CollisionResponse response; /**< Collision response type */
    bool is_trigger;             /**< True for trigger volumes */
    bool fixed_rotation;         /**< Prevent rotation */
} Agentite_PhysicsBodyConfig;

/** Default body configuration */
#define AGENTITE_PHYSICS_BODY_DEFAULT { \
    .type = AGENTITE_BODY_DYNAMIC, \
    .mass = 1.0f, \
    .drag = 0.0f, \
    .angular_drag = 0.0f, \
    .bounce = 0.0f, \
    .friction = 0.5f, \
    .gravity_scale = 1.0f, \
    .response = AGENTITE_RESPONSE_SLIDE, \
    .is_trigger = false, \
    .fixed_rotation = false \
}

/* ============================================================================
 * Physics World Lifecycle
 * ============================================================================ */

/**
 * Create a physics world.
 * Caller OWNS the returned pointer and MUST call agentite_physics_world_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return Physics world, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_PhysicsWorld *agentite_physics_world_create(
    const Agentite_PhysicsWorldConfig *config);

/**
 * Destroy physics world and all bodies.
 * Safe to call with NULL.
 *
 * @param world Physics world to destroy
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics_world_destroy(Agentite_PhysicsWorld *world);

/**
 * Set the collision world for physics to use.
 * The physics world will use this for collision detection.
 *
 * @param world Physics world
 * @param collision Collision world (NULL to disable collision)
 *
 * Note: The collision world is NOT owned by physics; caller must ensure
 * it outlives the physics world.
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics_set_collision_world(
    Agentite_PhysicsWorld *world,
    Agentite_CollisionWorld *collision);

/**
 * Step the physics simulation.
 * Uses fixed timestep with accumulator internally.
 *
 * @param world Physics world
 * @param delta_time Frame delta time in seconds
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics_world_step(Agentite_PhysicsWorld *world, float delta_time);

/**
 * Clear all bodies from the world.
 *
 * @param world Physics world
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics_world_clear(Agentite_PhysicsWorld *world);

/* ============================================================================
 * Physics World Properties
 * ============================================================================ */

/**
 * Set global gravity.
 *
 * @param world Physics world
 * @param x Gravity X component
 * @param y Gravity Y component
 */
void agentite_physics_set_gravity(Agentite_PhysicsWorld *world, float x, float y);

/**
 * Get global gravity.
 *
 * @param world Physics world
 * @param out_x Output X component (can be NULL)
 * @param out_y Output Y component (can be NULL)
 */
void agentite_physics_get_gravity(
    const Agentite_PhysicsWorld *world, float *out_x, float *out_y);

/**
 * Set fixed timestep interval.
 *
 * @param world Physics world
 * @param timestep Fixed timestep in seconds
 */
void agentite_physics_set_fixed_timestep(Agentite_PhysicsWorld *world, float timestep);

/* ============================================================================
 * Physics Body Lifecycle
 * ============================================================================ */

/**
 * Create a physics body.
 * Caller OWNS the returned pointer and MUST call agentite_physics_body_destroy().
 *
 * @param world Physics world
 * @param config Body configuration (NULL for defaults)
 * @return Physics body, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_PhysicsBody *agentite_physics_body_create(
    Agentite_PhysicsWorld *world,
    const Agentite_PhysicsBodyConfig *config);

/**
 * Destroy a physics body.
 * Safe to call with NULL.
 *
 * @param body Body to destroy
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_physics_body_destroy(Agentite_PhysicsBody *body);

/* ============================================================================
 * Physics Body Transform
 * ============================================================================ */

/**
 * Set body position directly.
 * For dynamic bodies, prefer applying forces/impulses.
 *
 * @param body Physics body
 * @param x Position X
 * @param y Position Y
 */
void agentite_physics_body_set_position(Agentite_PhysicsBody *body, float x, float y);

/**
 * Get body position.
 *
 * @param body Physics body
 * @param out_x Output X (can be NULL)
 * @param out_y Output Y (can be NULL)
 */
void agentite_physics_body_get_position(
    const Agentite_PhysicsBody *body, float *out_x, float *out_y);

/**
 * Set body rotation.
 *
 * @param body Physics body
 * @param radians Rotation in radians
 */
void agentite_physics_body_set_rotation(Agentite_PhysicsBody *body, float radians);

/**
 * Get body rotation.
 *
 * @param body Physics body
 * @return Rotation in radians
 */
float agentite_physics_body_get_rotation(const Agentite_PhysicsBody *body);

/* ============================================================================
 * Physics Body Velocity
 * ============================================================================ */

/**
 * Set body velocity.
 *
 * @param body Physics body
 * @param vx Velocity X
 * @param vy Velocity Y
 */
void agentite_physics_body_set_velocity(Agentite_PhysicsBody *body, float vx, float vy);

/**
 * Get body velocity.
 *
 * @param body Physics body
 * @param out_vx Output velocity X (can be NULL)
 * @param out_vy Output velocity Y (can be NULL)
 */
void agentite_physics_body_get_velocity(
    const Agentite_PhysicsBody *body, float *out_vx, float *out_vy);

/**
 * Set angular velocity.
 *
 * @param body Physics body
 * @param omega Angular velocity in radians/second
 */
void agentite_physics_body_set_angular_velocity(Agentite_PhysicsBody *body, float omega);

/**
 * Get angular velocity.
 *
 * @param body Physics body
 * @return Angular velocity in radians/second
 */
float agentite_physics_body_get_angular_velocity(const Agentite_PhysicsBody *body);

/* ============================================================================
 * Physics Body Forces
 * ============================================================================ */

/**
 * Apply a force to the body (applied over time).
 * Force accumulates and is applied during the next step.
 *
 * @param body Physics body
 * @param fx Force X
 * @param fy Force Y
 */
void agentite_physics_body_apply_force(Agentite_PhysicsBody *body, float fx, float fy);

/**
 * Apply force at a world point (can cause rotation).
 *
 * @param body Physics body
 * @param fx Force X
 * @param fy Force Y
 * @param px Point X (world coordinates)
 * @param py Point Y (world coordinates)
 */
void agentite_physics_body_apply_force_at(
    Agentite_PhysicsBody *body,
    float fx, float fy,
    float px, float py);

/**
 * Apply an impulse (instant velocity change).
 *
 * @param body Physics body
 * @param ix Impulse X
 * @param iy Impulse Y
 */
void agentite_physics_body_apply_impulse(Agentite_PhysicsBody *body, float ix, float iy);

/**
 * Apply impulse at a world point (can cause rotation).
 *
 * @param body Physics body
 * @param ix Impulse X
 * @param iy Impulse Y
 * @param px Point X (world coordinates)
 * @param py Point Y (world coordinates)
 */
void agentite_physics_body_apply_impulse_at(
    Agentite_PhysicsBody *body,
    float ix, float iy,
    float px, float py);

/**
 * Apply torque (rotational force).
 *
 * @param body Physics body
 * @param torque Torque value
 */
void agentite_physics_body_apply_torque(Agentite_PhysicsBody *body, float torque);

/**
 * Clear all accumulated forces on the body.
 *
 * @param body Physics body
 */
void agentite_physics_body_clear_forces(Agentite_PhysicsBody *body);

/* ============================================================================
 * Physics Body Properties
 * ============================================================================ */

/**
 * Set body type.
 *
 * @param body Physics body
 * @param type New body type
 */
void agentite_physics_body_set_type(Agentite_PhysicsBody *body, Agentite_BodyType type);

/**
 * Get body type.
 *
 * @param body Physics body
 * @return Body type
 */
Agentite_BodyType agentite_physics_body_get_type(const Agentite_PhysicsBody *body);

/**
 * Set body mass.
 *
 * @param body Physics body
 * @param mass Mass value (must be > 0)
 */
void agentite_physics_body_set_mass(Agentite_PhysicsBody *body, float mass);

/**
 * Get body mass.
 *
 * @param body Physics body
 * @return Mass value
 */
float agentite_physics_body_get_mass(const Agentite_PhysicsBody *body);

/**
 * Set linear drag.
 *
 * @param body Physics body
 * @param drag Drag coefficient (0-1, 0 = no drag)
 */
void agentite_physics_body_set_drag(Agentite_PhysicsBody *body, float drag);

/**
 * Set bounce/restitution.
 *
 * @param body Physics body
 * @param bounce Bounce coefficient (0-1, 0 = no bounce, 1 = full bounce)
 */
void agentite_physics_body_set_bounce(Agentite_PhysicsBody *body, float bounce);

/**
 * Set friction.
 *
 * @param body Physics body
 * @param friction Friction coefficient (0-1)
 */
void agentite_physics_body_set_friction(Agentite_PhysicsBody *body, float friction);

/**
 * Set gravity scale.
 *
 * @param body Physics body
 * @param scale Gravity multiplier (0 = no gravity, 1 = normal, <0 = reverse)
 */
void agentite_physics_body_set_gravity_scale(Agentite_PhysicsBody *body, float scale);

/**
 * Set collision response type.
 *
 * @param body Physics body
 * @param response Response type
 */
void agentite_physics_body_set_response(
    Agentite_PhysicsBody *body, Agentite_CollisionResponse response);

/**
 * Set as trigger volume.
 *
 * @param body Physics body
 * @param is_trigger true for trigger, false for solid
 */
void agentite_physics_body_set_trigger(Agentite_PhysicsBody *body, bool is_trigger);

/**
 * Check if body is a trigger.
 *
 * @param body Physics body
 * @return true if trigger
 */
bool agentite_physics_body_is_trigger(const Agentite_PhysicsBody *body);

/* ============================================================================
 * Physics Body Shape
 * ============================================================================ */

/**
 * Set the collision shape for this body.
 * The shape is NOT owned by the body; caller must manage its lifetime.
 *
 * @param body Physics body
 * @param shape Collision shape (NULL to remove)
 */
void agentite_physics_body_set_shape(
    Agentite_PhysicsBody *body, Agentite_CollisionShape *shape);

/**
 * Get the collision shape.
 *
 * @param body Physics body
 * @return Current shape, or NULL if none
 */
Agentite_CollisionShape *agentite_physics_body_get_shape(const Agentite_PhysicsBody *body);

/**
 * Set collision layer for the body.
 *
 * @param body Physics body
 * @param layer Layer bitmask
 */
void agentite_physics_body_set_layer(Agentite_PhysicsBody *body, uint32_t layer);

/**
 * Set collision mask for the body.
 *
 * @param body Physics body
 * @param mask Mask bitmask
 */
void agentite_physics_body_set_mask(Agentite_PhysicsBody *body, uint32_t mask);

/* ============================================================================
 * Physics Body User Data
 * ============================================================================ */

/**
 * Set user data.
 *
 * @param body Physics body
 * @param data User data pointer (not owned)
 */
void agentite_physics_body_set_user_data(Agentite_PhysicsBody *body, void *data);

/**
 * Get user data.
 *
 * @param body Physics body
 * @return User data pointer
 */
void *agentite_physics_body_get_user_data(const Agentite_PhysicsBody *body);

/**
 * Enable/disable a body.
 *
 * @param body Physics body
 * @param enabled true to enable
 */
void agentite_physics_body_set_enabled(Agentite_PhysicsBody *body, bool enabled);

/**
 * Check if body is enabled.
 *
 * @param body Physics body
 * @return true if enabled
 */
bool agentite_physics_body_is_enabled(const Agentite_PhysicsBody *body);

/* ============================================================================
 * Callbacks
 * ============================================================================ */

/**
 * Set collision callback.
 * Called when two bodies collide (after response is computed).
 *
 * @param world Physics world
 * @param callback Callback function (NULL to remove)
 * @param user_data User data passed to callback
 */
void agentite_physics_set_collision_callback(
    Agentite_PhysicsWorld *world,
    Agentite_PhysicsCollisionCallback callback,
    void *user_data);

/**
 * Set trigger callback.
 * Called when a body enters or exits a trigger.
 *
 * @param world Physics world
 * @param callback Callback function (NULL to remove)
 * @param user_data User data passed to callback
 */
void agentite_physics_set_trigger_callback(
    Agentite_PhysicsWorld *world,
    Agentite_PhysicsTriggerCallback callback,
    void *user_data);

/* ============================================================================
 * Queries
 * ============================================================================ */

/**
 * Get the collision world associated with this physics world.
 *
 * @param world Physics world
 * @return Collision world, or NULL if not set
 */
Agentite_CollisionWorld *agentite_physics_get_collision_world(
    const Agentite_PhysicsWorld *world);

/**
 * Get the collider ID for a physics body.
 *
 * @param body Physics body
 * @return Collider ID, or AGENTITE_COLLIDER_INVALID if no shape
 */
Agentite_ColliderId agentite_physics_body_get_collider(const Agentite_PhysicsBody *body);

/* ============================================================================
 * Statistics
 * ============================================================================ */

/**
 * Get body count.
 *
 * @param world Physics world
 * @return Number of active bodies
 */
int agentite_physics_world_get_body_count(const Agentite_PhysicsWorld *world);

/**
 * Get body capacity.
 *
 * @param world Physics world
 * @return Maximum bodies
 */
int agentite_physics_world_get_body_capacity(const Agentite_PhysicsWorld *world);

/* ============================================================================
 * Debug
 * ============================================================================ */

/* Forward declaration */
struct Agentite_Gizmos;

/**
 * Draw physics debug info (velocities, forces).
 *
 * @param world Physics world
 * @param gizmos Gizmos renderer
 */
void agentite_physics_debug_draw(
    const Agentite_PhysicsWorld *world,
    struct Agentite_Gizmos *gizmos);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_PHYSICS_H */
