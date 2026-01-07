/**
 * @file physics.cpp
 * @brief Simple 2D Kinematic Physics System Implementation
 */

#include "agentite/agentite.h"
#include "agentite/physics.h"
#include "agentite/collision.h"
#include "agentite/error.h"
#include "agentite/gizmos.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Agentite_PhysicsBody {
    Agentite_PhysicsWorld *world;
    bool active;
    bool enabled;

    /* Type and properties */
    Agentite_BodyType type;
    float mass;
    float inv_mass;  /* 1/mass for efficiency */
    float drag;
    float angular_drag;
    float bounce;
    float friction;
    float gravity_scale;
    Agentite_CollisionResponse response;
    bool is_trigger;
    bool fixed_rotation;

    /* Transform */
    float x, y;
    float rotation;

    /* Velocity */
    float vx, vy;
    float angular_velocity;

    /* Forces (accumulated each frame) */
    float fx, fy;
    float torque;

    /* Collision */
    Agentite_CollisionShape *shape;  /* Borrowed */
    Agentite_ColliderId collider_id;
    uint32_t layer;
    uint32_t mask;

    /* User data */
    void *user_data;

    /* Linked list for world management */
    Agentite_PhysicsBody *next;
    Agentite_PhysicsBody *prev;
};

struct Agentite_PhysicsWorld {
    /* Bodies */
    Agentite_PhysicsBody *bodies_head;
    uint32_t body_count;
    uint32_t max_bodies;

    /* Collision */
    Agentite_CollisionWorld *collision_world;  /* Borrowed */

    /* World properties */
    float gravity_x;
    float gravity_y;
    float fixed_timestep;
    int max_substeps;
    float time_accumulator;

    /* Callbacks */
    Agentite_PhysicsCollisionCallback collision_callback;
    void *collision_callback_data;
    Agentite_PhysicsTriggerCallback trigger_callback;
    void *trigger_callback_data;
};

/* ============================================================================
 * Math Helpers
 * ============================================================================ */

static inline float clampf(float v, float min_v, float max_v) {
    if (v < min_v) return min_v;
    if (v > max_v) return max_v;
    return v;
}

/* ============================================================================
 * Physics World Lifecycle
 * ============================================================================ */

Agentite_PhysicsWorld *agentite_physics_world_create(
    const Agentite_PhysicsWorldConfig *config)
{
    Agentite_PhysicsWorldConfig cfg = AGENTITE_PHYSICS_WORLD_DEFAULT;
    if (config) cfg = *config;

    Agentite_PhysicsWorld *world = AGENTITE_ALLOC(Agentite_PhysicsWorld);
    if (!world) {
        agentite_set_error("Physics: Failed to allocate world");
        return NULL;
    }

    world->bodies_head = NULL;
    world->body_count = 0;
    world->max_bodies = cfg.max_bodies;
    world->collision_world = NULL;
    world->gravity_x = cfg.gravity_x;
    world->gravity_y = cfg.gravity_y;
    world->fixed_timestep = cfg.fixed_timestep;
    world->max_substeps = cfg.max_substeps;
    world->time_accumulator = 0.0f;
    world->collision_callback = NULL;
    world->collision_callback_data = NULL;
    world->trigger_callback = NULL;
    world->trigger_callback_data = NULL;

    return world;
}

void agentite_physics_world_destroy(Agentite_PhysicsWorld *world) {
    if (!world) return;

    /* Destroy all bodies */
    Agentite_PhysicsBody *body = world->bodies_head;
    while (body) {
        Agentite_PhysicsBody *next = body->next;
        /* Remove from collision world */
        if (world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
            agentite_collision_remove(world->collision_world, body->collider_id);
        }
        free(body);
        body = next;
    }

    free(world);
}

void agentite_physics_set_collision_world(
    Agentite_PhysicsWorld *world,
    Agentite_CollisionWorld *collision)
{
    if (!world) return;
    world->collision_world = collision;

    /* Re-register all bodies with the collision world */
    Agentite_PhysicsBody *body = world->bodies_head;
    while (body) {
        if (body->shape && collision) {
            if (body->collider_id != AGENTITE_COLLIDER_INVALID && world->collision_world) {
                agentite_collision_remove(world->collision_world, body->collider_id);
            }
            body->collider_id = agentite_collision_add(collision, body->shape, body->x, body->y);
            if (body->collider_id != AGENTITE_COLLIDER_INVALID) {
                agentite_collision_set_rotation(collision, body->collider_id, body->rotation);
                agentite_collision_set_layer(collision, body->collider_id, body->layer);
                agentite_collision_set_mask(collision, body->collider_id, body->mask);
                agentite_collision_set_user_data(collision, body->collider_id, body);
            }
        }
        body = body->next;
    }
}

void agentite_physics_world_clear(Agentite_PhysicsWorld *world) {
    if (!world) return;

    Agentite_PhysicsBody *body = world->bodies_head;
    while (body) {
        Agentite_PhysicsBody *next = body->next;
        if (world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
            agentite_collision_remove(world->collision_world, body->collider_id);
        }
        free(body);
        body = next;
    }

    world->bodies_head = NULL;
    world->body_count = 0;
}

/* ============================================================================
 * Physics World Properties
 * ============================================================================ */

void agentite_physics_set_gravity(Agentite_PhysicsWorld *world, float x, float y) {
    if (world) {
        world->gravity_x = x;
        world->gravity_y = y;
    }
}

void agentite_physics_get_gravity(
    const Agentite_PhysicsWorld *world, float *out_x, float *out_y)
{
    if (!world) return;
    if (out_x) *out_x = world->gravity_x;
    if (out_y) *out_y = world->gravity_y;
}

void agentite_physics_set_fixed_timestep(Agentite_PhysicsWorld *world, float timestep) {
    if (world && timestep > 0.0f) {
        world->fixed_timestep = timestep;
    }
}

/* ============================================================================
 * Physics Body Lifecycle
 * ============================================================================ */

Agentite_PhysicsBody *agentite_physics_body_create(
    Agentite_PhysicsWorld *world,
    const Agentite_PhysicsBodyConfig *config)
{
    if (!world) {
        agentite_set_error("Physics: World is NULL");
        return NULL;
    }

    if (world->body_count >= world->max_bodies) {
        agentite_set_error("Physics: Maximum bodies reached (%d/%d)", world->body_count, world->max_bodies);
        return NULL;
    }

    Agentite_PhysicsBodyConfig cfg = AGENTITE_PHYSICS_BODY_DEFAULT;
    if (config) cfg = *config;

    Agentite_PhysicsBody *body = AGENTITE_ALLOC(Agentite_PhysicsBody);
    if (!body) {
        agentite_set_error("Physics: Failed to allocate body");
        return NULL;
    }

    body->world = world;
    body->active = true;
    body->enabled = true;

    body->type = cfg.type;
    body->mass = cfg.mass > 0.0f ? cfg.mass : 1.0f;
    body->inv_mass = (cfg.type == AGENTITE_BODY_STATIC) ? 0.0f : (1.0f / body->mass);
    body->drag = cfg.drag;
    body->angular_drag = cfg.angular_drag;
    body->bounce = clampf(cfg.bounce, 0.0f, 1.0f);
    body->friction = clampf(cfg.friction, 0.0f, 1.0f);
    body->gravity_scale = cfg.gravity_scale;
    body->response = cfg.response;
    body->is_trigger = cfg.is_trigger;
    body->fixed_rotation = cfg.fixed_rotation;

    body->x = body->y = 0.0f;
    body->rotation = 0.0f;
    body->vx = body->vy = 0.0f;
    body->angular_velocity = 0.0f;
    body->fx = body->fy = 0.0f;
    body->torque = 0.0f;

    body->shape = NULL;
    body->collider_id = AGENTITE_COLLIDER_INVALID;
    body->layer = AGENTITE_COLLISION_LAYER_ALL;
    body->mask = AGENTITE_COLLISION_LAYER_ALL;
    body->user_data = NULL;

    /* Add to linked list */
    body->next = world->bodies_head;
    body->prev = NULL;
    if (world->bodies_head) {
        world->bodies_head->prev = body;
    }
    world->bodies_head = body;
    world->body_count++;

    return body;
}

void agentite_physics_body_destroy(Agentite_PhysicsBody *body) {
    if (!body) return;

    Agentite_PhysicsWorld *world = body->world;

    /* Remove from collision world */
    if (world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_remove(world->collision_world, body->collider_id);
    }

    /* Remove from linked list */
    if (body->prev) {
        body->prev->next = body->next;
    } else {
        world->bodies_head = body->next;
    }
    if (body->next) {
        body->next->prev = body->prev;
    }
    world->body_count--;

    free(body);
}

/* ============================================================================
 * Physics Body Transform
 * ============================================================================ */

void agentite_physics_body_set_position(Agentite_PhysicsBody *body, float x, float y) {
    if (!body) return;
    body->x = x;
    body->y = y;

    if (body->world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_set_position(body->world->collision_world, body->collider_id, x, y);
    }
}

void agentite_physics_body_get_position(
    const Agentite_PhysicsBody *body, float *out_x, float *out_y)
{
    if (!body) return;
    if (out_x) *out_x = body->x;
    if (out_y) *out_y = body->y;
}

void agentite_physics_body_set_rotation(Agentite_PhysicsBody *body, float radians) {
    if (!body) return;
    body->rotation = radians;

    if (body->world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_set_rotation(body->world->collision_world, body->collider_id, radians);
    }
}

float agentite_physics_body_get_rotation(const Agentite_PhysicsBody *body) {
    return body ? body->rotation : 0.0f;
}

/* ============================================================================
 * Physics Body Velocity
 * ============================================================================ */

void agentite_physics_body_set_velocity(Agentite_PhysicsBody *body, float vx, float vy) {
    if (body) {
        body->vx = vx;
        body->vy = vy;
    }
}

void agentite_physics_body_get_velocity(
    const Agentite_PhysicsBody *body, float *out_vx, float *out_vy)
{
    if (!body) return;
    if (out_vx) *out_vx = body->vx;
    if (out_vy) *out_vy = body->vy;
}

void agentite_physics_body_set_angular_velocity(Agentite_PhysicsBody *body, float omega) {
    if (body) body->angular_velocity = omega;
}

float agentite_physics_body_get_angular_velocity(const Agentite_PhysicsBody *body) {
    return body ? body->angular_velocity : 0.0f;
}

/* ============================================================================
 * Physics Body Forces
 * ============================================================================ */

void agentite_physics_body_apply_force(Agentite_PhysicsBody *body, float fx, float fy) {
    if (body && body->type == AGENTITE_BODY_DYNAMIC) {
        body->fx += fx;
        body->fy += fy;
    }
}

void agentite_physics_body_apply_force_at(
    Agentite_PhysicsBody *body,
    float fx, float fy,
    float px, float py)
{
    if (!body || body->type != AGENTITE_BODY_DYNAMIC) return;

    body->fx += fx;
    body->fy += fy;

    if (!body->fixed_rotation) {
        /* Torque = r x F */
        float rx = px - body->x;
        float ry = py - body->y;
        body->torque += rx * fy - ry * fx;
    }
}

void agentite_physics_body_apply_impulse(Agentite_PhysicsBody *body, float ix, float iy) {
    if (body && body->type == AGENTITE_BODY_DYNAMIC) {
        body->vx += ix * body->inv_mass;
        body->vy += iy * body->inv_mass;
    }
}

void agentite_physics_body_apply_impulse_at(
    Agentite_PhysicsBody *body,
    float ix, float iy,
    float px, float py)
{
    if (!body || body->type != AGENTITE_BODY_DYNAMIC) return;

    body->vx += ix * body->inv_mass;
    body->vy += iy * body->inv_mass;

    if (!body->fixed_rotation) {
        float rx = px - body->x;
        float ry = py - body->y;
        /* Angular impulse (simplified: assume unit moment of inertia) */
        body->angular_velocity += (rx * iy - ry * ix) * body->inv_mass;
    }
}

void agentite_physics_body_apply_torque(Agentite_PhysicsBody *body, float torque) {
    if (body && body->type == AGENTITE_BODY_DYNAMIC && !body->fixed_rotation) {
        body->torque += torque;
    }
}

void agentite_physics_body_clear_forces(Agentite_PhysicsBody *body) {
    if (body) {
        body->fx = body->fy = 0.0f;
        body->torque = 0.0f;
    }
}

/* ============================================================================
 * Physics Body Properties
 * ============================================================================ */

void agentite_physics_body_set_type(Agentite_PhysicsBody *body, Agentite_BodyType type) {
    if (!body) return;
    body->type = type;
    body->inv_mass = (type == AGENTITE_BODY_STATIC) ? 0.0f : (1.0f / body->mass);
}

Agentite_BodyType agentite_physics_body_get_type(const Agentite_PhysicsBody *body) {
    return body ? body->type : AGENTITE_BODY_STATIC;
}

void agentite_physics_body_set_mass(Agentite_PhysicsBody *body, float mass) {
    if (body && mass > 0.0f) {
        body->mass = mass;
        body->inv_mass = (body->type == AGENTITE_BODY_STATIC) ? 0.0f : (1.0f / mass);
    }
}

float agentite_physics_body_get_mass(const Agentite_PhysicsBody *body) {
    return body ? body->mass : 0.0f;
}

void agentite_physics_body_set_drag(Agentite_PhysicsBody *body, float drag) {
    if (body) body->drag = drag;
}

void agentite_physics_body_set_bounce(Agentite_PhysicsBody *body, float bounce) {
    if (body) body->bounce = clampf(bounce, 0.0f, 1.0f);
}

void agentite_physics_body_set_friction(Agentite_PhysicsBody *body, float friction) {
    if (body) body->friction = clampf(friction, 0.0f, 1.0f);
}

void agentite_physics_body_set_gravity_scale(Agentite_PhysicsBody *body, float scale) {
    if (body) body->gravity_scale = scale;
}

void agentite_physics_body_set_response(
    Agentite_PhysicsBody *body, Agentite_CollisionResponse response)
{
    if (body) body->response = response;
}

void agentite_physics_body_set_trigger(Agentite_PhysicsBody *body, bool is_trigger) {
    if (body) body->is_trigger = is_trigger;
}

bool agentite_physics_body_is_trigger(const Agentite_PhysicsBody *body) {
    return body ? body->is_trigger : false;
}

/* ============================================================================
 * Physics Body Shape
 * ============================================================================ */

void agentite_physics_body_set_shape(
    Agentite_PhysicsBody *body, Agentite_CollisionShape *shape)
{
    if (!body) return;

    Agentite_PhysicsWorld *world = body->world;

    /* Remove old collider */
    if (world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_remove(world->collision_world, body->collider_id);
        body->collider_id = AGENTITE_COLLIDER_INVALID;
    }

    body->shape = shape;

    /* Add new collider */
    if (shape && world->collision_world) {
        body->collider_id = agentite_collision_add(
            world->collision_world, shape, body->x, body->y);
        if (body->collider_id != AGENTITE_COLLIDER_INVALID) {
            agentite_collision_set_rotation(world->collision_world, body->collider_id, body->rotation);
            agentite_collision_set_layer(world->collision_world, body->collider_id, body->layer);
            agentite_collision_set_mask(world->collision_world, body->collider_id, body->mask);
            agentite_collision_set_user_data(world->collision_world, body->collider_id, body);
        }
    }
}

Agentite_CollisionShape *agentite_physics_body_get_shape(const Agentite_PhysicsBody *body) {
    return body ? body->shape : NULL;
}

void agentite_physics_body_set_layer(Agentite_PhysicsBody *body, uint32_t layer) {
    if (!body) return;
    body->layer = layer;
    if (body->world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_set_layer(body->world->collision_world, body->collider_id, layer);
    }
}

void agentite_physics_body_set_mask(Agentite_PhysicsBody *body, uint32_t mask) {
    if (!body) return;
    body->mask = mask;
    if (body->world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_set_mask(body->world->collision_world, body->collider_id, mask);
    }
}

/* ============================================================================
 * Physics Body User Data
 * ============================================================================ */

void agentite_physics_body_set_user_data(Agentite_PhysicsBody *body, void *data) {
    if (body) body->user_data = data;
}

void *agentite_physics_body_get_user_data(const Agentite_PhysicsBody *body) {
    return body ? body->user_data : NULL;
}

void agentite_physics_body_set_enabled(Agentite_PhysicsBody *body, bool enabled) {
    if (!body) return;
    body->enabled = enabled;
    if (body->world->collision_world && body->collider_id != AGENTITE_COLLIDER_INVALID) {
        agentite_collision_set_enabled(body->world->collision_world, body->collider_id, enabled);
    }
}

bool agentite_physics_body_is_enabled(const Agentite_PhysicsBody *body) {
    return body ? body->enabled : false;
}

/* ============================================================================
 * Callbacks
 * ============================================================================ */

void agentite_physics_set_collision_callback(
    Agentite_PhysicsWorld *world,
    Agentite_PhysicsCollisionCallback callback,
    void *user_data)
{
    if (world) {
        world->collision_callback = callback;
        world->collision_callback_data = user_data;
    }
}

void agentite_physics_set_trigger_callback(
    Agentite_PhysicsWorld *world,
    Agentite_PhysicsTriggerCallback callback,
    void *user_data)
{
    if (world) {
        world->trigger_callback = callback;
        world->trigger_callback_data = user_data;
    }
}

/* ============================================================================
 * Physics Step
 * ============================================================================ */

static void integrate_body(Agentite_PhysicsBody *body, float dt, float gx, float gy) {
    if (!body->enabled || body->type == AGENTITE_BODY_STATIC) return;

    /* Apply gravity to dynamic bodies */
    if (body->type == AGENTITE_BODY_DYNAMIC) {
        body->fx += gx * body->mass * body->gravity_scale;
        body->fy += gy * body->mass * body->gravity_scale;
    }

    /* Integrate forces to velocity (F = ma => a = F/m) */
    if (body->type == AGENTITE_BODY_DYNAMIC) {
        body->vx += body->fx * body->inv_mass * dt;
        body->vy += body->fy * body->inv_mass * dt;

        /* Angular */
        if (!body->fixed_rotation) {
            body->angular_velocity += body->torque * body->inv_mass * dt;
        }
    }

    /* Apply drag */
    if (body->drag > 0.0f) {
        float drag_factor = 1.0f - (body->drag * dt);
        if (drag_factor < 0.0f) drag_factor = 0.0f;
        body->vx *= drag_factor;
        body->vy *= drag_factor;
    }

    if (body->angular_drag > 0.0f && !body->fixed_rotation) {
        float ang_drag_factor = 1.0f - (body->angular_drag * dt);
        if (ang_drag_factor < 0.0f) ang_drag_factor = 0.0f;
        body->angular_velocity *= ang_drag_factor;
    }

    /* Integrate velocity to position */
    body->x += body->vx * dt;
    body->y += body->vy * dt;

    if (!body->fixed_rotation) {
        body->rotation += body->angular_velocity * dt;
    }

    /* Clear forces for next frame */
    body->fx = body->fy = 0.0f;
    body->torque = 0.0f;
}

static void resolve_collision(
    Agentite_PhysicsBody *body_a,
    Agentite_PhysicsBody *body_b,
    const Agentite_CollisionResult *result)
{
    /* Handle triggers */
    if (body_a->is_trigger || body_b->is_trigger) {
        return;  /* Trigger callbacks handled separately */
    }

    /* Skip if either has no response */
    if (body_a->response == AGENTITE_RESPONSE_NONE ||
        body_b->response == AGENTITE_RESPONSE_NONE) {
        return;
    }

    /* Get effective properties */
    float bounce = (body_a->bounce + body_b->bounce) * 0.5f;
    float friction = (body_a->friction + body_b->friction) * 0.5f;

    float nx = result->normal.x;
    float ny = result->normal.y;
    float depth = result->depth;

    /* Separate bodies */
    float total_inv_mass = body_a->inv_mass + body_b->inv_mass;
    if (total_inv_mass > 0.0f) {
        float ratio_a = body_a->inv_mass / total_inv_mass;
        float ratio_b = body_b->inv_mass / total_inv_mass;

        body_a->x -= nx * depth * ratio_a;
        body_a->y -= ny * depth * ratio_a;
        body_b->x += nx * depth * ratio_b;
        body_b->y += ny * depth * ratio_b;
    }

    /* Calculate relative velocity */
    float rel_vx = body_a->vx - body_b->vx;
    float rel_vy = body_a->vy - body_b->vy;
    float rel_vel_normal = rel_vx * nx + rel_vy * ny;

    /* Moving apart? Skip impulse
     * Normal points from A toward B, so rel_vel_normal < 0 means separating */
    if (rel_vel_normal < 0) return;

    /* Calculate impulse scalar */
    float j = -(1.0f + bounce) * rel_vel_normal;
    if (total_inv_mass > 0.0f) {
        j /= total_inv_mass;
    }

    /* Apply response based on type */
    float impulse_x = j * nx;
    float impulse_y = j * ny;

    if (body_a->type == AGENTITE_BODY_DYNAMIC) {
        switch (body_a->response) {
            case AGENTITE_RESPONSE_STOP:
                body_a->vx = 0;
                body_a->vy = 0;
                break;
            case AGENTITE_RESPONSE_SLIDE:
                /* Remove velocity component along normal */
                body_a->vx += impulse_x * body_a->inv_mass;
                body_a->vy += impulse_y * body_a->inv_mass;
                /* Apply friction to tangent velocity */
                {
                    float tan_vx = rel_vx - rel_vel_normal * nx;
                    float tan_vy = rel_vy - rel_vel_normal * ny;
                    body_a->vx -= tan_vx * friction * body_a->inv_mass;
                    body_a->vy -= tan_vy * friction * body_a->inv_mass;
                }
                break;
            case AGENTITE_RESPONSE_BOUNCE:
                body_a->vx += impulse_x * body_a->inv_mass;
                body_a->vy += impulse_y * body_a->inv_mass;
                break;
            default:
                break;
        }
    }

    if (body_b->type == AGENTITE_BODY_DYNAMIC) {
        switch (body_b->response) {
            case AGENTITE_RESPONSE_STOP:
                body_b->vx = 0;
                body_b->vy = 0;
                break;
            case AGENTITE_RESPONSE_SLIDE:
                body_b->vx -= impulse_x * body_b->inv_mass;
                body_b->vy -= impulse_y * body_b->inv_mass;
                {
                    float tan_vx = rel_vx - rel_vel_normal * nx;
                    float tan_vy = rel_vy - rel_vel_normal * ny;
                    body_b->vx += tan_vx * friction * body_b->inv_mass;
                    body_b->vy += tan_vy * friction * body_b->inv_mass;
                }
                break;
            case AGENTITE_RESPONSE_BOUNCE:
                body_b->vx -= impulse_x * body_b->inv_mass;
                body_b->vy -= impulse_y * body_b->inv_mass;
                break;
            default:
                break;
        }
    }
}

static void physics_step_fixed(Agentite_PhysicsWorld *world, float dt) {
    /* Integrate all bodies */
    Agentite_PhysicsBody *body = world->bodies_head;
    while (body) {
        integrate_body(body, dt, world->gravity_x, world->gravity_y);
        body = body->next;
    }

    /* Update collision positions */
    if (world->collision_world) {
        body = world->bodies_head;
        while (body) {
            if (body->enabled && body->collider_id != AGENTITE_COLLIDER_INVALID) {
                agentite_collision_set_position(world->collision_world, body->collider_id,
                                                body->x, body->y);
                agentite_collision_set_rotation(world->collision_world, body->collider_id,
                                                body->rotation);
            }
            body = body->next;
        }

        /* Detect and resolve collisions */
        body = world->bodies_head;
        while (body) {
            if (!body->enabled || body->type == AGENTITE_BODY_STATIC ||
                body->collider_id == AGENTITE_COLLIDER_INVALID) {
                body = body->next;
                continue;
            }

            Agentite_CollisionResult results[16];
            int count = agentite_collision_query_collider(
                world->collision_world, body->collider_id, results, 16);

            for (int i = 0; i < count; i++) {
                Agentite_PhysicsBody *other = (Agentite_PhysicsBody*)
                    agentite_collision_get_user_data(world->collision_world, results[i].collider_b);
                if (!other) continue;

                /* Call callback */
                bool do_response = true;
                if (world->collision_callback) {
                    do_response = world->collision_callback(
                        body, other, &results[i], world->collision_callback_data);
                }

                /* Handle triggers */
                if (body->is_trigger || other->is_trigger) {
                    if (world->trigger_callback) {
                        world->trigger_callback(
                            body->is_trigger ? body : other,
                            body->is_trigger ? other : body,
                            true,  /* TODO: Track enter/exit properly */
                            world->trigger_callback_data);
                    }
                    continue;
                }

                /* Resolve physical collision */
                if (do_response) {
                    resolve_collision(body, other, &results[i]);

                    /* Update collision positions after resolution */
                    agentite_collision_set_position(world->collision_world, body->collider_id,
                                                    body->x, body->y);
                    if (other->collider_id != AGENTITE_COLLIDER_INVALID) {
                        agentite_collision_set_position(world->collision_world, other->collider_id,
                                                        other->x, other->y);
                    }
                }
            }

            body = body->next;
        }
    }
}

void agentite_physics_world_step(Agentite_PhysicsWorld *world, float delta_time) {
    if (!world || delta_time <= 0.0f) return;

    world->time_accumulator += delta_time;

    int substeps = 0;
    while (world->time_accumulator >= world->fixed_timestep && substeps < world->max_substeps) {
        physics_step_fixed(world, world->fixed_timestep);
        world->time_accumulator -= world->fixed_timestep;
        substeps++;
    }

    /* Clamp accumulator to prevent spiral of death */
    if (world->time_accumulator > world->fixed_timestep * world->max_substeps) {
        world->time_accumulator = 0.0f;
    }
}

/* ============================================================================
 * Queries
 * ============================================================================ */

Agentite_CollisionWorld *agentite_physics_get_collision_world(
    const Agentite_PhysicsWorld *world)
{
    return world ? world->collision_world : NULL;
}

Agentite_ColliderId agentite_physics_body_get_collider(const Agentite_PhysicsBody *body) {
    return body ? body->collider_id : AGENTITE_COLLIDER_INVALID;
}

/* ============================================================================
 * Statistics
 * ============================================================================ */

int agentite_physics_world_get_body_count(const Agentite_PhysicsWorld *world) {
    return world ? (int)world->body_count : 0;
}

int agentite_physics_world_get_body_capacity(const Agentite_PhysicsWorld *world) {
    return world ? (int)world->max_bodies : 0;
}

/* ============================================================================
 * Debug
 * ============================================================================ */

void agentite_physics_debug_draw(
    const Agentite_PhysicsWorld *world,
    struct Agentite_Gizmos *gizmos)
{
    if (!world || !gizmos) return;

    const Agentite_PhysicsBody *body = world->bodies_head;
    while (body) {
        if (body->enabled) {
            /* Draw velocity vector */
            float vel_scale = 0.1f;
            agentite_gizmos_line_2d(gizmos,
                body->x, body->y,
                body->x + body->vx * vel_scale,
                body->y + body->vy * vel_scale,
                0x00FF00FF);  /* Green */

            /* Draw body center */
            uint32_t color;
            switch (body->type) {
                case AGENTITE_BODY_STATIC: color = 0x888888FF; break;
                case AGENTITE_BODY_KINEMATIC: color = 0xFFFF00FF; break;
                case AGENTITE_BODY_DYNAMIC: color = 0x00FFFFFF; break;
                default: color = 0xFFFFFFFF;
            }
            if (body->is_trigger) color = 0xFF00FFFF;  /* Magenta for triggers */

            agentite_gizmos_circle_2d(gizmos, body->x, body->y, 4.0f, color);
        }
        body = body->next;
    }
}
