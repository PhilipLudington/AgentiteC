/**
 * @file physics2d.cpp
 * @brief Chipmunk2D Physics Integration Implementation
 */

#include "agentite/physics2d.h"
#include "agentite/error.h"
#include "agentite/gizmos.h"

#include <chipmunk/chipmunk.h>
#include <chipmunk/chipmunk_structs.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ============================================================================
 * Internal Structures
 * ============================================================================ */

struct Agentite_Physics2DSpace {
    cpSpace *cp_space;
    void *user_data;

    /* Collision handler storage */
    Agentite_Physics2DCollisionHandler default_handler;
    bool has_default_handler;
};

struct Agentite_Physics2DBody {
    cpBody *cp_body;
    Agentite_Physics2DSpace *space;
    void *user_data;
    bool owned;  /* true if we should free the body on destroy */
};

struct Agentite_Physics2DShape {
    cpShape *cp_shape;
    Agentite_Physics2DBody *body;
    void *user_data;
};

struct Agentite_Physics2DConstraint {
    cpConstraint *cp_constraint;
    Agentite_Physics2DSpace *space;
    void *user_data;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static inline cpVect to_cpv(float x, float y) {
    return cpv(x, y);
}

static inline void from_cpv(cpVect v, float *x, float *y) {
    if (x) *x = (float)v.x;
    if (y) *y = (float)v.y;
}

/* Collision callback wrappers */
static cpBool collision_begin_wrapper(cpArbiter *arb, cpSpace *space, cpDataPointer data) {
    (void)data;
    Agentite_Physics2DSpace *p2d_space = (Agentite_Physics2DSpace *)cpSpaceGetUserData(space);
    if (!p2d_space || !p2d_space->has_default_handler || !p2d_space->default_handler.begin) {
        return cpTrue;
    }

    cpShape *a, *b;
    cpArbiterGetShapes(arb, &a, &b);

    Agentite_Physics2DCollision collision = {};
    collision.shape_a = (Agentite_Physics2DShape *)cpShapeGetUserData(a);
    collision.shape_b = (Agentite_Physics2DShape *)cpShapeGetUserData(b);

    cpVect n = cpArbiterGetNormal(arb);
    collision.normal.x = (float)n.x;
    collision.normal.y = (float)n.y;

    collision.contact_count = cpArbiterGetCount(arb);
    for (int i = 0; i < collision.contact_count && i < 2; i++) {
        cpVect pa = cpArbiterGetPointA(arb, i);
        cpVect pb = cpArbiterGetPointB(arb, i);
        collision.contacts[i].point_a.x = (float)pa.x;
        collision.contacts[i].point_a.y = (float)pa.y;
        collision.contacts[i].point_b.x = (float)pb.x;
        collision.contacts[i].point_b.y = (float)pb.y;
        collision.contacts[i].distance = (float)cpArbiterGetDepth(arb, i);
    }

    collision.restitution = (float)cpArbiterGetRestitution(arb);
    collision.friction = (float)cpArbiterGetFriction(arb);

    cpVect sv = cpArbiterGetSurfaceVelocity(arb);
    collision.surface_velocity.x = (float)sv.x;
    collision.surface_velocity.y = (float)sv.y;

    return p2d_space->default_handler.begin(&collision, p2d_space->default_handler.user_data) ? cpTrue : cpFalse;
}

static cpBool collision_pre_solve_wrapper(cpArbiter *arb, cpSpace *space, cpDataPointer data) {
    (void)data;
    Agentite_Physics2DSpace *p2d_space = (Agentite_Physics2DSpace *)cpSpaceGetUserData(space);
    if (!p2d_space || !p2d_space->has_default_handler || !p2d_space->default_handler.pre_solve) {
        return cpTrue;
    }

    cpShape *a, *b;
    cpArbiterGetShapes(arb, &a, &b);

    Agentite_Physics2DCollision collision = {};
    collision.shape_a = (Agentite_Physics2DShape *)cpShapeGetUserData(a);
    collision.shape_b = (Agentite_Physics2DShape *)cpShapeGetUserData(b);

    cpVect n = cpArbiterGetNormal(arb);
    collision.normal.x = (float)n.x;
    collision.normal.y = (float)n.y;

    collision.contact_count = cpArbiterGetCount(arb);
    for (int i = 0; i < collision.contact_count && i < 2; i++) {
        cpVect pa = cpArbiterGetPointA(arb, i);
        cpVect pb = cpArbiterGetPointB(arb, i);
        collision.contacts[i].point_a.x = (float)pa.x;
        collision.contacts[i].point_a.y = (float)pa.y;
        collision.contacts[i].point_b.x = (float)pb.x;
        collision.contacts[i].point_b.y = (float)pb.y;
        collision.contacts[i].distance = (float)cpArbiterGetDepth(arb, i);
    }

    return p2d_space->default_handler.pre_solve(&collision, p2d_space->default_handler.user_data) ? cpTrue : cpFalse;
}

static void collision_post_solve_wrapper(cpArbiter *arb, cpSpace *space, cpDataPointer data) {
    (void)data;
    Agentite_Physics2DSpace *p2d_space = (Agentite_Physics2DSpace *)cpSpaceGetUserData(space);
    if (!p2d_space || !p2d_space->has_default_handler || !p2d_space->default_handler.post_solve) {
        return;
    }

    cpShape *a, *b;
    cpArbiterGetShapes(arb, &a, &b);

    Agentite_Physics2DCollision collision = {};
    collision.shape_a = (Agentite_Physics2DShape *)cpShapeGetUserData(a);
    collision.shape_b = (Agentite_Physics2DShape *)cpShapeGetUserData(b);

    cpVect n = cpArbiterGetNormal(arb);
    collision.normal.x = (float)n.x;
    collision.normal.y = (float)n.y;

    collision.contact_count = cpArbiterGetCount(arb);
    for (int i = 0; i < collision.contact_count && i < 2; i++) {
        cpVect pa = cpArbiterGetPointA(arb, i);
        cpVect pb = cpArbiterGetPointB(arb, i);
        collision.contacts[i].point_a.x = (float)pa.x;
        collision.contacts[i].point_a.y = (float)pa.y;
        collision.contacts[i].point_b.x = (float)pb.x;
        collision.contacts[i].point_b.y = (float)pb.y;
        collision.contacts[i].distance = (float)cpArbiterGetDepth(arb, i);
    }

    p2d_space->default_handler.post_solve(&collision, p2d_space->default_handler.user_data);
}

static void collision_separate_wrapper(cpArbiter *arb, cpSpace *space, cpDataPointer data) {
    (void)data;
    Agentite_Physics2DSpace *p2d_space = (Agentite_Physics2DSpace *)cpSpaceGetUserData(space);
    if (!p2d_space || !p2d_space->has_default_handler || !p2d_space->default_handler.separate) {
        return;
    }

    cpShape *a, *b;
    cpArbiterGetShapes(arb, &a, &b);

    Agentite_Physics2DCollision collision = {};
    collision.shape_a = (Agentite_Physics2DShape *)cpShapeGetUserData(a);
    collision.shape_b = (Agentite_Physics2DShape *)cpShapeGetUserData(b);

    cpVect n = cpArbiterGetNormal(arb);
    collision.normal.x = (float)n.x;
    collision.normal.y = (float)n.y;

    p2d_space->default_handler.separate(&collision, p2d_space->default_handler.user_data);
}

/* ============================================================================
 * Space Implementation
 * ============================================================================ */

Agentite_Physics2DSpace *agentite_physics2d_space_create(
    const Agentite_Physics2DConfig *config)
{
    Agentite_Physics2DSpace *space = (Agentite_Physics2DSpace *)calloc(1, sizeof(Agentite_Physics2DSpace));
    if (!space) {
        agentite_set_error("Failed to allocate physics2d space");
        return NULL;
    }

    space->cp_space = cpSpaceNew();
    if (!space->cp_space) {
        free(space);
        agentite_set_error("Failed to create Chipmunk space");
        return NULL;
    }

    cpSpaceSetUserData(space->cp_space, space);

    if (config) {
        cpSpaceSetGravity(space->cp_space, to_cpv(config->gravity_x, config->gravity_y));
        cpSpaceSetIterations(space->cp_space, config->iterations);
        cpSpaceSetDamping(space->cp_space, (cpFloat)config->damping);

        if (config->sleep_time_threshold >= 0) {
            cpSpaceSetSleepTimeThreshold(space->cp_space, (cpFloat)config->sleep_time_threshold);
        }
        if (config->idle_speed_threshold > 0) {
            cpSpaceSetIdleSpeedThreshold(space->cp_space, (cpFloat)config->idle_speed_threshold);
        }
        cpSpaceSetCollisionSlop(space->cp_space, (cpFloat)config->collision_slop);
        cpSpaceSetCollisionBias(space->cp_space, cpfpow(1.0 - config->collision_bias, 60.0));
    }

    return space;
}

/* Free shape iterator callback */
static void free_shape_callback(cpBody *body, cpShape *shape, void *data) {
    (void)body; (void)data;
    Agentite_Physics2DShape *p2d_shape = (Agentite_Physics2DShape *)cpShapeGetUserData(shape);
    if (p2d_shape) {
        free(p2d_shape);
    }
}

/* Free constraint iterator callback */
static void free_constraint_callback(cpBody *body, cpConstraint *constraint, void *data) {
    (void)body; (void)data;
    Agentite_Physics2DConstraint *p2d_constraint =
        (Agentite_Physics2DConstraint *)cpConstraintGetUserData(constraint);
    if (p2d_constraint) {
        free(p2d_constraint);
    }
}

/* Body iterator for cleanup */
static void cleanup_body_callback(cpBody *body, void *data) {
    (void)data;
    cpBodyEachShape(body, free_shape_callback, NULL);
    cpBodyEachConstraint(body, free_constraint_callback, NULL);

    Agentite_Physics2DBody *p2d_body = (Agentite_Physics2DBody *)cpBodyGetUserData(body);
    if (p2d_body) {
        free(p2d_body);
    }
}

void agentite_physics2d_space_destroy(Agentite_Physics2DSpace *space) {
    if (!space) return;

    if (space->cp_space) {
        /* Free all our wrapper objects first */
        cpSpaceEachBody(space->cp_space, cleanup_body_callback, NULL);

        /* Free static body shapes */
        cpBody *static_body = cpSpaceGetStaticBody(space->cp_space);
        cpBodyEachShape(static_body, free_shape_callback, NULL);

        cpSpaceFree(space->cp_space);
    }

    free(space);
}

void agentite_physics2d_space_step(Agentite_Physics2DSpace *space, float dt) {
    if (!space || !space->cp_space) return;
    cpSpaceStep(space->cp_space, (cpFloat)dt);
}

void agentite_physics2d_space_set_gravity(Agentite_Physics2DSpace *space, float x, float y) {
    if (!space || !space->cp_space) return;
    cpSpaceSetGravity(space->cp_space, to_cpv(x, y));
}

void agentite_physics2d_space_get_gravity(const Agentite_Physics2DSpace *space, float *x, float *y) {
    if (!space || !space->cp_space) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    from_cpv(cpSpaceGetGravity(space->cp_space), x, y);
}

void agentite_physics2d_space_set_damping(Agentite_Physics2DSpace *space, float damping) {
    if (!space || !space->cp_space) return;
    cpSpaceSetDamping(space->cp_space, (cpFloat)damping);
}

float agentite_physics2d_space_get_damping(const Agentite_Physics2DSpace *space) {
    if (!space || !space->cp_space) return 1.0f;
    return (float)cpSpaceGetDamping(space->cp_space);
}

void agentite_physics2d_space_set_iterations(Agentite_Physics2DSpace *space, int iterations) {
    if (!space || !space->cp_space) return;
    cpSpaceSetIterations(space->cp_space, iterations);
}

float agentite_physics2d_space_get_current_timestep(const Agentite_Physics2DSpace *space) {
    if (!space || !space->cp_space) return 0.0f;
    return (float)cpSpaceGetCurrentTimeStep(space->cp_space);
}

bool agentite_physics2d_space_is_locked(const Agentite_Physics2DSpace *space) {
    if (!space || !space->cp_space) return false;
    return cpSpaceIsLocked(space->cp_space) == cpTrue;
}

void agentite_physics2d_space_set_user_data(Agentite_Physics2DSpace *space, void *data) {
    if (!space) return;
    space->user_data = data;
}

void *agentite_physics2d_space_get_user_data(const Agentite_Physics2DSpace *space) {
    if (!space) return NULL;
    return space->user_data;
}

/* ============================================================================
 * Body Implementation
 * ============================================================================ */

static Agentite_Physics2DBody *create_body_wrapper(Agentite_Physics2DSpace *space, cpBody *cp_body, bool owned) {
    Agentite_Physics2DBody *body = (Agentite_Physics2DBody *)calloc(1, sizeof(Agentite_Physics2DBody));
    if (!body) {
        if (owned) cpBodyFree(cp_body);
        agentite_set_error("Failed to allocate physics2d body wrapper");
        return NULL;
    }

    body->cp_body = cp_body;
    body->space = space;
    body->owned = owned;
    cpBodySetUserData(cp_body, body);

    if (owned) {
        cpSpaceAddBody(space->cp_space, cp_body);
    }

    return body;
}

Agentite_Physics2DBody *agentite_physics2d_body_create_dynamic(
    Agentite_Physics2DSpace *space,
    float mass,
    float moment)
{
    if (!space || !space->cp_space) {
        agentite_set_error("Invalid space");
        return NULL;
    }

    if (mass <= 0) {
        agentite_set_error("Mass must be positive");
        return NULL;
    }

    cpFloat m = (cpFloat)mass;
    cpFloat i = moment > 0 ? (cpFloat)moment : 1.0;

    cpBody *cp_body = cpBodyNew(m, i);
    if (!cp_body) {
        agentite_set_error("Failed to create Chipmunk body");
        return NULL;
    }

    return create_body_wrapper(space, cp_body, true);
}

Agentite_Physics2DBody *agentite_physics2d_body_create_kinematic(
    Agentite_Physics2DSpace *space)
{
    if (!space || !space->cp_space) {
        agentite_set_error("Invalid space");
        return NULL;
    }

    cpBody *cp_body = cpBodyNewKinematic();
    if (!cp_body) {
        agentite_set_error("Failed to create Chipmunk kinematic body");
        return NULL;
    }

    return create_body_wrapper(space, cp_body, true);
}

Agentite_Physics2DBody *agentite_physics2d_body_create_static(
    Agentite_Physics2DSpace *space)
{
    if (!space || !space->cp_space) {
        agentite_set_error("Invalid space");
        return NULL;
    }

    cpBody *cp_body = cpBodyNewStatic();
    if (!cp_body) {
        agentite_set_error("Failed to create Chipmunk static body");
        return NULL;
    }

    return create_body_wrapper(space, cp_body, true);
}

Agentite_Physics2DBody *agentite_physics2d_space_get_static_body(
    Agentite_Physics2DSpace *space)
{
    if (!space || !space->cp_space) return NULL;

    cpBody *static_body = cpSpaceGetStaticBody(space->cp_space);

    /* Check if we already have a wrapper */
    Agentite_Physics2DBody *existing = (Agentite_Physics2DBody *)cpBodyGetUserData(static_body);
    if (existing) return existing;

    /* Create wrapper for built-in static body (not owned) */
    return create_body_wrapper(space, static_body, false);
}

void agentite_physics2d_body_destroy(Agentite_Physics2DBody *body) {
    if (!body) return;

    if (body->cp_body && body->owned && body->space && body->space->cp_space) {
        /* Remove all shapes first */
        cpBodyEachShape(body->cp_body, free_shape_callback, NULL);

        /* Remove all constraints */
        cpBodyEachConstraint(body->cp_body, free_constraint_callback, NULL);

        cpSpaceRemoveBody(body->space->cp_space, body->cp_body);
        cpBodyFree(body->cp_body);
    }

    free(body);
}

/* Moment of inertia helpers */
float agentite_physics2d_moment_for_circle(float mass, float inner_radius, float outer_radius,
                                           float offset_x, float offset_y) {
    return (float)cpMomentForCircle((cpFloat)mass, (cpFloat)inner_radius, (cpFloat)outer_radius,
                                    to_cpv(offset_x, offset_y));
}

float agentite_physics2d_moment_for_box(float mass, float width, float height) {
    return (float)cpMomentForBox((cpFloat)mass, (cpFloat)width, (cpFloat)height);
}

float agentite_physics2d_moment_for_polygon(float mass, int vertex_count,
                                            const Agentite_Physics2DVec *vertices,
                                            float offset_x, float offset_y,
                                            float radius) {
    cpVect *verts = (cpVect *)malloc(vertex_count * sizeof(cpVect));
    if (!verts) return 1.0f;

    for (int i = 0; i < vertex_count; i++) {
        verts[i] = to_cpv(vertices[i].x, vertices[i].y);
    }

    float result = (float)cpMomentForPoly((cpFloat)mass, vertex_count, verts,
                                          to_cpv(offset_x, offset_y), (cpFloat)radius);
    free(verts);
    return result;
}

float agentite_physics2d_moment_for_segment(float mass, float ax, float ay,
                                            float bx, float by, float radius) {
    return (float)cpMomentForSegment((cpFloat)mass, to_cpv(ax, ay), to_cpv(bx, by), (cpFloat)radius);
}

/* Body transform */
void agentite_physics2d_body_set_position(Agentite_Physics2DBody *body, float x, float y) {
    if (!body || !body->cp_body) return;
    cpBodySetPosition(body->cp_body, to_cpv(x, y));
}

void agentite_physics2d_body_get_position(const Agentite_Physics2DBody *body, float *x, float *y) {
    if (!body || !body->cp_body) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    from_cpv(cpBodyGetPosition(body->cp_body), x, y);
}

void agentite_physics2d_body_set_angle(Agentite_Physics2DBody *body, float radians) {
    if (!body || !body->cp_body) return;
    cpBodySetAngle(body->cp_body, (cpFloat)radians);
}

float agentite_physics2d_body_get_angle(const Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return 0.0f;
    return (float)cpBodyGetAngle(body->cp_body);
}

void agentite_physics2d_body_set_velocity(Agentite_Physics2DBody *body, float vx, float vy) {
    if (!body || !body->cp_body) return;
    cpBodySetVelocity(body->cp_body, to_cpv(vx, vy));
}

void agentite_physics2d_body_get_velocity(const Agentite_Physics2DBody *body, float *vx, float *vy) {
    if (!body || !body->cp_body) {
        if (vx) *vx = 0;
        if (vy) *vy = 0;
        return;
    }
    from_cpv(cpBodyGetVelocity(body->cp_body), vx, vy);
}

void agentite_physics2d_body_set_angular_velocity(Agentite_Physics2DBody *body, float w) {
    if (!body || !body->cp_body) return;
    cpBodySetAngularVelocity(body->cp_body, (cpFloat)w);
}

float agentite_physics2d_body_get_angular_velocity(const Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return 0.0f;
    return (float)cpBodyGetAngularVelocity(body->cp_body);
}

/* Body properties */
void agentite_physics2d_body_set_mass(Agentite_Physics2DBody *body, float mass) {
    if (!body || !body->cp_body) return;
    cpBodySetMass(body->cp_body, (cpFloat)mass);
}

float agentite_physics2d_body_get_mass(const Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return 0.0f;
    return (float)cpBodyGetMass(body->cp_body);
}

void agentite_physics2d_body_set_moment(Agentite_Physics2DBody *body, float moment) {
    if (!body || !body->cp_body) return;
    cpBodySetMoment(body->cp_body, (cpFloat)moment);
}

float agentite_physics2d_body_get_moment(const Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return 0.0f;
    return (float)cpBodyGetMoment(body->cp_body);
}

void agentite_physics2d_body_set_center_of_gravity(Agentite_Physics2DBody *body, float x, float y) {
    if (!body || !body->cp_body) return;
    cpBodySetCenterOfGravity(body->cp_body, to_cpv(x, y));
}

void agentite_physics2d_body_get_center_of_gravity(const Agentite_Physics2DBody *body, float *x, float *y) {
    if (!body || !body->cp_body) {
        if (x) *x = 0;
        if (y) *y = 0;
        return;
    }
    from_cpv(cpBodyGetCenterOfGravity(body->cp_body), x, y);
}

/* Body forces and impulses */
void agentite_physics2d_body_apply_force_at_world(
    Agentite_Physics2DBody *body,
    float fx, float fy,
    float px, float py)
{
    if (!body || !body->cp_body) return;
    cpBodyApplyForceAtWorldPoint(body->cp_body, to_cpv(fx, fy), to_cpv(px, py));
}

void agentite_physics2d_body_apply_force_at_local(
    Agentite_Physics2DBody *body,
    float fx, float fy,
    float px, float py)
{
    if (!body || !body->cp_body) return;
    cpBodyApplyForceAtLocalPoint(body->cp_body, to_cpv(fx, fy), to_cpv(px, py));
}

void agentite_physics2d_body_apply_impulse_at_world(
    Agentite_Physics2DBody *body,
    float ix, float iy,
    float px, float py)
{
    if (!body || !body->cp_body) return;
    cpBodyApplyImpulseAtWorldPoint(body->cp_body, to_cpv(ix, iy), to_cpv(px, py));
}

void agentite_physics2d_body_apply_impulse_at_local(
    Agentite_Physics2DBody *body,
    float ix, float iy,
    float px, float py)
{
    if (!body || !body->cp_body) return;
    cpBodyApplyImpulseAtLocalPoint(body->cp_body, to_cpv(ix, iy), to_cpv(px, py));
}

void agentite_physics2d_body_get_force(const Agentite_Physics2DBody *body, float *fx, float *fy) {
    if (!body || !body->cp_body) {
        if (fx) *fx = 0;
        if (fy) *fy = 0;
        return;
    }
    from_cpv(cpBodyGetForce(body->cp_body), fx, fy);
}

float agentite_physics2d_body_get_torque(const Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return 0.0f;
    return (float)cpBodyGetTorque(body->cp_body);
}

/* Body coordinate conversion */
void agentite_physics2d_body_local_to_world(const Agentite_Physics2DBody *body,
                                            float lx, float ly,
                                            float *wx, float *wy) {
    if (!body || !body->cp_body) {
        if (wx) *wx = lx;
        if (wy) *wy = ly;
        return;
    }
    from_cpv(cpBodyLocalToWorld(body->cp_body, to_cpv(lx, ly)), wx, wy);
}

void agentite_physics2d_body_world_to_local(const Agentite_Physics2DBody *body,
                                            float wx, float wy,
                                            float *lx, float *ly) {
    if (!body || !body->cp_body) {
        if (lx) *lx = wx;
        if (ly) *ly = wy;
        return;
    }
    from_cpv(cpBodyWorldToLocal(body->cp_body, to_cpv(wx, wy)), lx, ly);
}

void agentite_physics2d_body_velocity_at_world_point(const Agentite_Physics2DBody *body,
                                                     float px, float py,
                                                     float *vx, float *vy) {
    if (!body || !body->cp_body) {
        if (vx) *vx = 0;
        if (vy) *vy = 0;
        return;
    }
    from_cpv(cpBodyGetVelocityAtWorldPoint(body->cp_body, to_cpv(px, py)), vx, vy);
}

void agentite_physics2d_body_velocity_at_local_point(const Agentite_Physics2DBody *body,
                                                     float px, float py,
                                                     float *vx, float *vy) {
    if (!body || !body->cp_body) {
        if (vx) *vx = 0;
        if (vy) *vy = 0;
        return;
    }
    from_cpv(cpBodyGetVelocityAtLocalPoint(body->cp_body, to_cpv(px, py)), vx, vy);
}

/* Body sleep state */
bool agentite_physics2d_body_is_sleeping(const Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return false;
    return cpBodyIsSleeping(body->cp_body) == cpTrue;
}

void agentite_physics2d_body_sleep(Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return;
    cpBodySleep(body->cp_body);
}

void agentite_physics2d_body_activate(Agentite_Physics2DBody *body) {
    if (!body || !body->cp_body) return;
    cpBodyActivate(body->cp_body);
}

/* Body user data */
void agentite_physics2d_body_set_user_data(Agentite_Physics2DBody *body, void *data) {
    if (!body) return;
    body->user_data = data;
}

void *agentite_physics2d_body_get_user_data(const Agentite_Physics2DBody *body) {
    if (!body) return NULL;
    return body->user_data;
}

/* ============================================================================
 * Shape Implementation
 * ============================================================================ */

static Agentite_Physics2DShape *create_shape_wrapper(Agentite_Physics2DBody *body, cpShape *cp_shape) {
    if (!cp_shape) return NULL;

    Agentite_Physics2DShape *shape = (Agentite_Physics2DShape *)calloc(1, sizeof(Agentite_Physics2DShape));
    if (!shape) {
        cpShapeFree(cp_shape);
        agentite_set_error("Failed to allocate physics2d shape wrapper");
        return NULL;
    }

    shape->cp_shape = cp_shape;
    shape->body = body;
    cpShapeSetUserData(cp_shape, shape);

    if (body && body->space && body->space->cp_space) {
        cpSpaceAddShape(body->space->cp_space, cp_shape);
    }

    return shape;
}

Agentite_Physics2DShape *agentite_physics2d_shape_circle(
    Agentite_Physics2DBody *body,
    float radius,
    float offset_x, float offset_y)
{
    if (!body || !body->cp_body) {
        agentite_set_error("Invalid body");
        return NULL;
    }

    cpShape *cp_shape = cpCircleShapeNew(body->cp_body, (cpFloat)radius, to_cpv(offset_x, offset_y));
    return create_shape_wrapper(body, cp_shape);
}

Agentite_Physics2DShape *agentite_physics2d_shape_box(
    Agentite_Physics2DBody *body,
    float width, float height,
    float radius)
{
    if (!body || !body->cp_body) {
        agentite_set_error("Invalid body");
        return NULL;
    }

    cpShape *cp_shape = cpBoxShapeNew(body->cp_body, (cpFloat)width, (cpFloat)height, (cpFloat)radius);
    return create_shape_wrapper(body, cp_shape);
}

Agentite_Physics2DShape *agentite_physics2d_shape_box_offset(
    Agentite_Physics2DBody *body,
    float left, float bottom,
    float right, float top,
    float radius)
{
    if (!body || !body->cp_body) {
        agentite_set_error("Invalid body");
        return NULL;
    }

    cpBB box = cpBBNew((cpFloat)left, (cpFloat)bottom, (cpFloat)right, (cpFloat)top);
    cpShape *cp_shape = cpBoxShapeNew2(body->cp_body, box, (cpFloat)radius);
    return create_shape_wrapper(body, cp_shape);
}

Agentite_Physics2DShape *agentite_physics2d_shape_polygon(
    Agentite_Physics2DBody *body,
    int vertex_count,
    const Agentite_Physics2DVec *vertices,
    float radius)
{
    if (!body || !body->cp_body) {
        agentite_set_error("Invalid body");
        return NULL;
    }

    if (!vertices || vertex_count < 3) {
        agentite_set_error("Invalid polygon vertices");
        return NULL;
    }

    cpVect *verts = (cpVect *)malloc(vertex_count * sizeof(cpVect));
    if (!verts) {
        agentite_set_error("Failed to allocate vertex array");
        return NULL;
    }

    for (int i = 0; i < vertex_count; i++) {
        verts[i] = to_cpv(vertices[i].x, vertices[i].y);
    }

    cpShape *cp_shape = cpPolyShapeNewRaw(body->cp_body, vertex_count, verts, (cpFloat)radius);
    free(verts);

    return create_shape_wrapper(body, cp_shape);
}

Agentite_Physics2DShape *agentite_physics2d_shape_segment(
    Agentite_Physics2DBody *body,
    float ax, float ay,
    float bx, float by,
    float radius)
{
    if (!body || !body->cp_body) {
        agentite_set_error("Invalid body");
        return NULL;
    }

    cpShape *cp_shape = cpSegmentShapeNew(body->cp_body, to_cpv(ax, ay), to_cpv(bx, by), (cpFloat)radius);
    return create_shape_wrapper(body, cp_shape);
}

void agentite_physics2d_shape_destroy(Agentite_Physics2DShape *shape) {
    if (!shape) return;

    if (shape->cp_shape) {
        if (shape->body && shape->body->space && shape->body->space->cp_space) {
            cpSpaceRemoveShape(shape->body->space->cp_space, shape->cp_shape);
        }
        cpShapeFree(shape->cp_shape);
    }

    free(shape);
}

/* Shape properties */
void agentite_physics2d_shape_set_friction(Agentite_Physics2DShape *shape, float friction) {
    if (!shape || !shape->cp_shape) return;
    cpShapeSetFriction(shape->cp_shape, (cpFloat)friction);
}

float agentite_physics2d_shape_get_friction(const Agentite_Physics2DShape *shape) {
    if (!shape || !shape->cp_shape) return 0.0f;
    return (float)cpShapeGetFriction(shape->cp_shape);
}

void agentite_physics2d_shape_set_elasticity(Agentite_Physics2DShape *shape, float elasticity) {
    if (!shape || !shape->cp_shape) return;
    cpShapeSetElasticity(shape->cp_shape, (cpFloat)elasticity);
}

float agentite_physics2d_shape_get_elasticity(const Agentite_Physics2DShape *shape) {
    if (!shape || !shape->cp_shape) return 0.0f;
    return (float)cpShapeGetElasticity(shape->cp_shape);
}

void agentite_physics2d_shape_set_surface_velocity(Agentite_Physics2DShape *shape, float vx, float vy) {
    if (!shape || !shape->cp_shape) return;
    cpShapeSetSurfaceVelocity(shape->cp_shape, to_cpv(vx, vy));
}

void agentite_physics2d_shape_get_surface_velocity(const Agentite_Physics2DShape *shape, float *vx, float *vy) {
    if (!shape || !shape->cp_shape) {
        if (vx) *vx = 0;
        if (vy) *vy = 0;
        return;
    }
    from_cpv(cpShapeGetSurfaceVelocity(shape->cp_shape), vx, vy);
}

void agentite_physics2d_shape_set_sensor(Agentite_Physics2DShape *shape, bool is_sensor) {
    if (!shape || !shape->cp_shape) return;
    cpShapeSetSensor(shape->cp_shape, is_sensor ? cpTrue : cpFalse);
}

bool agentite_physics2d_shape_is_sensor(const Agentite_Physics2DShape *shape) {
    if (!shape || !shape->cp_shape) return false;
    return cpShapeGetSensor(shape->cp_shape) == cpTrue;
}

/* Shape collision filtering */
void agentite_physics2d_shape_set_collision_type(
    Agentite_Physics2DShape *shape,
    Agentite_Physics2DCollisionType type)
{
    if (!shape || !shape->cp_shape) return;
    cpShapeSetCollisionType(shape->cp_shape, (cpCollisionType)type);
}

Agentite_Physics2DCollisionType agentite_physics2d_shape_get_collision_type(
    const Agentite_Physics2DShape *shape)
{
    if (!shape || !shape->cp_shape) return 0;
    return (Agentite_Physics2DCollisionType)cpShapeGetCollisionType(shape->cp_shape);
}

void agentite_physics2d_shape_set_filter(
    Agentite_Physics2DShape *shape,
    Agentite_Physics2DGroup group,
    Agentite_Physics2DBitmask categories,
    Agentite_Physics2DBitmask mask)
{
    if (!shape || !shape->cp_shape) return;
    cpShapeFilter filter = cpShapeFilterNew((cpGroup)group, (cpBitmask)categories, (cpBitmask)mask);
    cpShapeSetFilter(shape->cp_shape, filter);
}

Agentite_Physics2DGroup agentite_physics2d_shape_get_filter_group(
    const Agentite_Physics2DShape *shape)
{
    if (!shape || !shape->cp_shape) return 0;
    return (Agentite_Physics2DGroup)cpShapeGetFilter(shape->cp_shape).group;
}

Agentite_Physics2DBitmask agentite_physics2d_shape_get_filter_categories(
    const Agentite_Physics2DShape *shape)
{
    if (!shape || !shape->cp_shape) return 0;
    return (Agentite_Physics2DBitmask)cpShapeGetFilter(shape->cp_shape).categories;
}

Agentite_Physics2DBitmask agentite_physics2d_shape_get_filter_mask(
    const Agentite_Physics2DShape *shape)
{
    if (!shape || !shape->cp_shape) return 0;
    return (Agentite_Physics2DBitmask)cpShapeGetFilter(shape->cp_shape).mask;
}

/* Shape user data */
void agentite_physics2d_shape_set_user_data(Agentite_Physics2DShape *shape, void *data) {
    if (!shape) return;
    shape->user_data = data;
}

void *agentite_physics2d_shape_get_user_data(const Agentite_Physics2DShape *shape) {
    if (!shape) return NULL;
    return shape->user_data;
}

Agentite_Physics2DBody *agentite_physics2d_shape_get_body(const Agentite_Physics2DShape *shape) {
    if (!shape) return NULL;
    return shape->body;
}

/* ============================================================================
 * Collision Handler Implementation
 * ============================================================================ */

void agentite_physics2d_space_set_default_collision_handler(
    Agentite_Physics2DSpace *space,
    const Agentite_Physics2DCollisionHandler *handler)
{
    if (!space || !space->cp_space) return;

    cpCollisionHandler *cp_handler = cpSpaceAddDefaultCollisionHandler(space->cp_space);

    if (handler) {
        space->default_handler = *handler;
        space->has_default_handler = true;

        cp_handler->beginFunc = handler->begin ? collision_begin_wrapper : NULL;
        cp_handler->preSolveFunc = handler->pre_solve ? collision_pre_solve_wrapper : NULL;
        cp_handler->postSolveFunc = handler->post_solve ? collision_post_solve_wrapper : NULL;
        cp_handler->separateFunc = handler->separate ? collision_separate_wrapper : NULL;
    } else {
        space->has_default_handler = false;
        cp_handler->beginFunc = NULL;
        cp_handler->preSolveFunc = NULL;
        cp_handler->postSolveFunc = NULL;
        cp_handler->separateFunc = NULL;
    }
}

void agentite_physics2d_space_add_collision_handler(
    Agentite_Physics2DSpace *space,
    Agentite_Physics2DCollisionType type_a,
    Agentite_Physics2DCollisionType type_b,
    const Agentite_Physics2DCollisionHandler *handler)
{
    if (!space || !space->cp_space || !handler) return;

    cpCollisionHandler *cp_handler = cpSpaceAddCollisionHandler(
        space->cp_space, (cpCollisionType)type_a, (cpCollisionType)type_b);

    /* Store handler in user data - would need proper storage for multiple handlers */
    /* For now, we use the same default handler approach */
    space->default_handler = *handler;
    space->has_default_handler = true;

    cp_handler->beginFunc = handler->begin ? collision_begin_wrapper : NULL;
    cp_handler->preSolveFunc = handler->pre_solve ? collision_pre_solve_wrapper : NULL;
    cp_handler->postSolveFunc = handler->post_solve ? collision_post_solve_wrapper : NULL;
    cp_handler->separateFunc = handler->separate ? collision_separate_wrapper : NULL;
}

void agentite_physics2d_space_add_wildcard_handler(
    Agentite_Physics2DSpace *space,
    Agentite_Physics2DCollisionType type,
    const Agentite_Physics2DCollisionHandler *handler)
{
    if (!space || !space->cp_space || !handler) return;

    cpCollisionHandler *cp_handler = cpSpaceAddWildcardHandler(
        space->cp_space, (cpCollisionType)type);

    space->default_handler = *handler;
    space->has_default_handler = true;

    cp_handler->beginFunc = handler->begin ? collision_begin_wrapper : NULL;
    cp_handler->preSolveFunc = handler->pre_solve ? collision_pre_solve_wrapper : NULL;
    cp_handler->postSolveFunc = handler->post_solve ? collision_post_solve_wrapper : NULL;
    cp_handler->separateFunc = handler->separate ? collision_separate_wrapper : NULL;
}

/* ============================================================================
 * Constraint Implementation
 * ============================================================================ */

static Agentite_Physics2DConstraint *create_constraint_wrapper(
    Agentite_Physics2DBody *body_a,
    cpConstraint *cp_constraint)
{
    if (!cp_constraint) return NULL;

    Agentite_Physics2DConstraint *constraint =
        (Agentite_Physics2DConstraint *)calloc(1, sizeof(Agentite_Physics2DConstraint));
    if (!constraint) {
        cpConstraintFree(cp_constraint);
        agentite_set_error("Failed to allocate physics2d constraint wrapper");
        return NULL;
    }

    constraint->cp_constraint = cp_constraint;
    constraint->space = body_a->space;
    cpConstraintSetUserData(cp_constraint, constraint);

    if (body_a->space && body_a->space->cp_space) {
        cpSpaceAddConstraint(body_a->space->cp_space, cp_constraint);
    }

    return constraint;
}

Agentite_Physics2DConstraint *agentite_physics2d_pin_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpPinJointNew(body_a->cp_body, body_b->cp_body,
                                     to_cpv(anchor_ax, anchor_ay),
                                     to_cpv(anchor_bx, anchor_by));
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_slide_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by,
    float min, float max)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpSlideJointNew(body_a->cp_body, body_b->cp_body,
                                       to_cpv(anchor_ax, anchor_ay),
                                       to_cpv(anchor_bx, anchor_by),
                                       (cpFloat)min, (cpFloat)max);
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_pivot_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float pivot_x, float pivot_y)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpPivotJointNew(body_a->cp_body, body_b->cp_body,
                                       to_cpv(pivot_x, pivot_y));
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_pivot_joint_create2(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpPivotJointNew2(body_a->cp_body, body_b->cp_body,
                                        to_cpv(anchor_ax, anchor_ay),
                                        to_cpv(anchor_bx, anchor_by));
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_groove_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float groove_ax, float groove_ay,
    float groove_bx, float groove_by,
    float anchor_bx, float anchor_by)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpGrooveJointNew(body_a->cp_body, body_b->cp_body,
                                        to_cpv(groove_ax, groove_ay),
                                        to_cpv(groove_bx, groove_by),
                                        to_cpv(anchor_bx, anchor_by));
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_damped_spring_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float anchor_ax, float anchor_ay,
    float anchor_bx, float anchor_by,
    float rest_length,
    float stiffness,
    float damping)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpDampedSpringNew(body_a->cp_body, body_b->cp_body,
                                         to_cpv(anchor_ax, anchor_ay),
                                         to_cpv(anchor_bx, anchor_by),
                                         (cpFloat)rest_length,
                                         (cpFloat)stiffness,
                                         (cpFloat)damping);
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_damped_rotary_spring_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float rest_angle,
    float stiffness,
    float damping)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpDampedRotarySpringNew(body_a->cp_body, body_b->cp_body,
                                               (cpFloat)rest_angle,
                                               (cpFloat)stiffness,
                                               (cpFloat)damping);
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_rotary_limit_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float min, float max)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpRotaryLimitJointNew(body_a->cp_body, body_b->cp_body,
                                             (cpFloat)min, (cpFloat)max);
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_ratchet_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float phase, float ratchet)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpRatchetJointNew(body_a->cp_body, body_b->cp_body,
                                         (cpFloat)phase, (cpFloat)ratchet);
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_gear_joint_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float phase, float ratio)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpGearJointNew(body_a->cp_body, body_b->cp_body,
                                      (cpFloat)phase, (cpFloat)ratio);
    return create_constraint_wrapper(body_a, cp);
}

Agentite_Physics2DConstraint *agentite_physics2d_simple_motor_create(
    Agentite_Physics2DBody *body_a,
    Agentite_Physics2DBody *body_b,
    float rate)
{
    if (!body_a || !body_a->cp_body || !body_b || !body_b->cp_body) {
        agentite_set_error("Invalid bodies for constraint");
        return NULL;
    }

    cpConstraint *cp = cpSimpleMotorNew(body_a->cp_body, body_b->cp_body, (cpFloat)rate);
    return create_constraint_wrapper(body_a, cp);
}

void agentite_physics2d_constraint_destroy(Agentite_Physics2DConstraint *constraint) {
    if (!constraint) return;

    if (constraint->cp_constraint) {
        if (constraint->space && constraint->space->cp_space) {
            cpSpaceRemoveConstraint(constraint->space->cp_space, constraint->cp_constraint);
        }
        cpConstraintFree(constraint->cp_constraint);
    }

    free(constraint);
}

/* Constraint properties */
void agentite_physics2d_constraint_set_max_force(
    Agentite_Physics2DConstraint *constraint, float max_force)
{
    if (!constraint || !constraint->cp_constraint) return;
    cpConstraintSetMaxForce(constraint->cp_constraint, (cpFloat)max_force);
}

float agentite_physics2d_constraint_get_max_force(
    const Agentite_Physics2DConstraint *constraint)
{
    if (!constraint || !constraint->cp_constraint) return 0.0f;
    return (float)cpConstraintGetMaxForce(constraint->cp_constraint);
}

void agentite_physics2d_constraint_set_error_bias(
    Agentite_Physics2DConstraint *constraint, float bias)
{
    if (!constraint || !constraint->cp_constraint) return;
    cpConstraintSetErrorBias(constraint->cp_constraint, cpfpow(1.0 - bias, 60.0));
}

float agentite_physics2d_constraint_get_error_bias(
    const Agentite_Physics2DConstraint *constraint)
{
    if (!constraint || !constraint->cp_constraint) return 0.0f;
    return (float)cpConstraintGetErrorBias(constraint->cp_constraint);
}

void agentite_physics2d_constraint_set_max_bias(
    Agentite_Physics2DConstraint *constraint, float max_bias)
{
    if (!constraint || !constraint->cp_constraint) return;
    cpConstraintSetMaxBias(constraint->cp_constraint, (cpFloat)max_bias);
}

float agentite_physics2d_constraint_get_max_bias(
    const Agentite_Physics2DConstraint *constraint)
{
    if (!constraint || !constraint->cp_constraint) return 0.0f;
    return (float)cpConstraintGetMaxBias(constraint->cp_constraint);
}

void agentite_physics2d_constraint_set_collide_bodies(
    Agentite_Physics2DConstraint *constraint, bool collide)
{
    if (!constraint || !constraint->cp_constraint) return;
    cpConstraintSetCollideBodies(constraint->cp_constraint, collide ? cpTrue : cpFalse);
}

bool agentite_physics2d_constraint_get_collide_bodies(
    const Agentite_Physics2DConstraint *constraint)
{
    if (!constraint || !constraint->cp_constraint) return false;
    return cpConstraintGetCollideBodies(constraint->cp_constraint) == cpTrue;
}

float agentite_physics2d_constraint_get_impulse(
    const Agentite_Physics2DConstraint *constraint)
{
    if (!constraint || !constraint->cp_constraint) return 0.0f;
    return (float)cpConstraintGetImpulse(constraint->cp_constraint);
}

/* Constraint user data */
void agentite_physics2d_constraint_set_user_data(
    Agentite_Physics2DConstraint *constraint, void *data)
{
    if (!constraint) return;
    constraint->user_data = data;
}

void *agentite_physics2d_constraint_get_user_data(
    const Agentite_Physics2DConstraint *constraint)
{
    if (!constraint) return NULL;
    return constraint->user_data;
}

/* ============================================================================
 * Space Queries Implementation
 * ============================================================================ */

Agentite_Physics2DShape *agentite_physics2d_space_point_query_nearest(
    Agentite_Physics2DSpace *space,
    float px, float py,
    float radius,
    Agentite_Physics2DGroup filter_group,
    Agentite_Physics2DBitmask filter_categories,
    Agentite_Physics2DBitmask filter_mask,
    Agentite_Physics2DPointQueryInfo *out_info)
{
    if (!space || !space->cp_space) return NULL;

    cpShapeFilter filter = cpShapeFilterNew((cpGroup)filter_group,
                                            (cpBitmask)filter_categories,
                                            (cpBitmask)filter_mask);

    cpPointQueryInfo info;
    cpShape *cp_shape = cpSpacePointQueryNearest(space->cp_space, to_cpv(px, py),
                                                 (cpFloat)radius, filter, &info);

    if (!cp_shape) return NULL;

    if (out_info) {
        out_info->shape = (Agentite_Physics2DShape *)cpShapeGetUserData(cp_shape);
        out_info->point_x = (float)info.point.x;
        out_info->point_y = (float)info.point.y;
        out_info->distance = (float)info.distance;
        out_info->gradient_x = (float)info.gradient.x;
        out_info->gradient_y = (float)info.gradient.y;
    }

    return (Agentite_Physics2DShape *)cpShapeGetUserData(cp_shape);
}

Agentite_Physics2DShape *agentite_physics2d_space_segment_query_first(
    Agentite_Physics2DSpace *space,
    float ax, float ay,
    float bx, float by,
    float radius,
    Agentite_Physics2DGroup filter_group,
    Agentite_Physics2DBitmask filter_categories,
    Agentite_Physics2DBitmask filter_mask,
    Agentite_Physics2DSegmentQueryInfo *out_info)
{
    if (!space || !space->cp_space) return NULL;

    cpShapeFilter filter = cpShapeFilterNew((cpGroup)filter_group,
                                            (cpBitmask)filter_categories,
                                            (cpBitmask)filter_mask);

    cpSegmentQueryInfo info;
    cpShape *cp_shape = cpSpaceSegmentQueryFirst(space->cp_space,
                                                 to_cpv(ax, ay), to_cpv(bx, by),
                                                 (cpFloat)radius, filter, &info);

    if (!cp_shape) return NULL;

    if (out_info) {
        out_info->shape = (Agentite_Physics2DShape *)cpShapeGetUserData(cp_shape);
        out_info->point_x = (float)info.point.x;
        out_info->point_y = (float)info.point.y;
        out_info->normal_x = (float)info.normal.x;
        out_info->normal_y = (float)info.normal.y;
        out_info->alpha = (float)info.alpha;
    }

    return (Agentite_Physics2DShape *)cpShapeGetUserData(cp_shape);
}

/* ============================================================================
 * Statistics Implementation
 * ============================================================================ */

/* Body counter callback */
static void count_body_callback(cpBody *body, void *data) {
    (void)body;
    int *count = (int *)data;
    (*count)++;
}

/* Shape counter callback */
static void count_shape_callback(cpShape *shape, void *data) {
    (void)shape;
    int *count = (int *)data;
    (*count)++;
}

/* Constraint counter callback */
static void count_constraint_callback(cpConstraint *constraint, void *data) {
    (void)constraint;
    int *count = (int *)data;
    (*count)++;
}

int agentite_physics2d_space_get_body_count(const Agentite_Physics2DSpace *space) {
    if (!space || !space->cp_space) return 0;

    int count = 0;
    cpSpaceEachBody(space->cp_space, count_body_callback, &count);
    return count;
}

int agentite_physics2d_space_get_shape_count(const Agentite_Physics2DSpace *space) {
    if (!space || !space->cp_space) return 0;

    int count = 0;
    cpSpaceEachShape(space->cp_space, count_shape_callback, &count);
    return count;
}

int agentite_physics2d_space_get_constraint_count(const Agentite_Physics2DSpace *space) {
    if (!space || !space->cp_space) return 0;

    int count = 0;
    cpSpaceEachConstraint(space->cp_space, count_constraint_callback, &count);
    return count;
}

/* ============================================================================
 * Debug Drawing Implementation
 * ============================================================================ */

/* Color helpers */
static inline uint32_t make_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | (uint32_t)a;
}

/* Shape draw callback */
static void draw_shape_callback(cpBody *body, cpShape *shape, void *data) {
    Agentite_Gizmos *gizmos = (Agentite_Gizmos *)data;
    if (!gizmos) return;

    uint32_t shape_color = make_color(0, 255, 0, 255);  /* Green */
    uint32_t line_color = make_color(0, 200, 0, 255);   /* Darker green */

    /* Get shape type and draw accordingly - access via struct (chipmunk_structs.h) */
    cpShapeType shape_type = shape->klass->type;
    switch (shape_type) {
        case CP_CIRCLE_SHAPE: {
            cpVect center = cpBodyLocalToWorld(body, cpCircleShapeGetOffset(shape));
            cpFloat radius = cpCircleShapeGetRadius(shape);

            agentite_gizmos_circle_2d(gizmos, (float)center.x, (float)center.y,
                                      (float)radius, shape_color);

            /* Draw line to show rotation */
            cpFloat angle = cpBodyGetAngle(body);
            float line_end_x = (float)(center.x + radius * cosf((float)angle));
            float line_end_y = (float)(center.y + radius * sinf((float)angle));
            agentite_gizmos_line_2d(gizmos, (float)center.x, (float)center.y,
                                    line_end_x, line_end_y, line_color);
            break;
        }

        case CP_SEGMENT_SHAPE: {
            cpVect a = cpBodyLocalToWorld(body, cpSegmentShapeGetA(shape));
            cpVect b = cpBodyLocalToWorld(body, cpSegmentShapeGetB(shape));
            cpFloat r = cpSegmentShapeGetRadius(shape);

            agentite_gizmos_line_2d(gizmos, (float)a.x, (float)a.y,
                                    (float)b.x, (float)b.y, shape_color);

            if (r > 0.1f) {
                /* Draw end caps */
                agentite_gizmos_circle_2d(gizmos, (float)a.x, (float)a.y,
                                          (float)r, line_color);
                agentite_gizmos_circle_2d(gizmos, (float)b.x, (float)b.y,
                                          (float)r, line_color);
            }
            break;
        }

        case CP_POLY_SHAPE: {
            int count = cpPolyShapeGetCount(shape);
            if (count > 0) {
                for (int i = 0; i < count; i++) {
                    cpVect v1 = cpBodyLocalToWorld(body, cpPolyShapeGetVert(shape, i));
                    cpVect v2 = cpBodyLocalToWorld(body, cpPolyShapeGetVert(shape, (i + 1) % count));
                    agentite_gizmos_line_2d(gizmos, (float)v1.x, (float)v1.y,
                                            (float)v2.x, (float)v2.y, shape_color);
                }
            }
            break;
        }

        default:
            break;
    }
}

/* Body velocity draw callback */
static void draw_body_velocity_callback(cpBody *body, void *data) {
    Agentite_Gizmos *gizmos = (Agentite_Gizmos *)data;
    if (!gizmos) return;

    cpVect pos = cpBodyGetPosition(body);
    cpVect vel = cpBodyGetVelocity(body);

    /* Draw velocity vector (scaled down) */
    float scale = 0.1f;
    uint32_t vel_color = make_color(255, 0, 0, 255);  /* Red */
    agentite_gizmos_line_2d(gizmos, (float)pos.x, (float)pos.y,
                            (float)(pos.x + vel.x * scale),
                            (float)(pos.y + vel.y * scale), vel_color);
}

void agentite_physics2d_debug_draw(
    const Agentite_Physics2DSpace *space,
    struct Agentite_Gizmos *gizmos)
{
    if (!space || !space->cp_space || !gizmos) return;

    /* Draw all shapes */
    cpSpaceEachBody(space->cp_space, [](cpBody *body, void *data) {
        cpBodyEachShape(body, draw_shape_callback, data);
    }, gizmos);

    /* Also draw static body shapes */
    cpBody *static_body = cpSpaceGetStaticBody(space->cp_space);
    cpBodyEachShape(static_body, draw_shape_callback, gizmos);

    /* Draw body velocities */
    cpSpaceEachBody(space->cp_space, draw_body_velocity_callback, gizmos);
}
