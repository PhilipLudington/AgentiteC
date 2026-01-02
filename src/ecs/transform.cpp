/**
 * Agentite Engine - Transform Hierarchy Implementation
 *
 * Implements parent-child transform hierarchies using Flecs EcsChildOf.
 */

#include "agentite/transform.h"
#include "agentite/ecs.h"
#include "flecs.h"
#include <math.h>

/* ============================================================================
 * Component Definitions
 * ============================================================================ */

ECS_COMPONENT_DECLARE(C_Transform);
ECS_COMPONENT_DECLARE(C_WorldTransform);

/* ============================================================================
 * Transform Math Helpers
 * ============================================================================ */

/**
 * Apply a 2D transform to a point.
 * Applies scale, rotation, then translation.
 */
static void apply_transform(float x, float y,
                            float tx, float ty,
                            float rotation,
                            float sx, float sy,
                            float *out_x, float *out_y) {
    /* Scale */
    float scaled_x = x * sx;
    float scaled_y = y * sy;

    /* Rotate */
    float cos_r = cosf(rotation);
    float sin_r = sinf(rotation);
    float rotated_x = scaled_x * cos_r - scaled_y * sin_r;
    float rotated_y = scaled_x * sin_r + scaled_y * cos_r;

    /* Translate */
    *out_x = rotated_x + tx;
    *out_y = rotated_y + ty;
}

/**
 * Combine two transforms (child relative to parent).
 * Result = parent transform applied, then child transform.
 */
static void combine_transforms(const C_Transform *local,
                                const C_WorldTransform *parent_world,
                                C_WorldTransform *out_world) {
    /* Get parent values (or identity if no parent) */
    float parent_x = parent_world ? parent_world->world_x : 0.0f;
    float parent_y = parent_world ? parent_world->world_y : 0.0f;
    float parent_rot = parent_world ? parent_world->world_rotation : 0.0f;
    float parent_sx = parent_world ? parent_world->world_scale_x : 1.0f;
    float parent_sy = parent_world ? parent_world->world_scale_y : 1.0f;

    /* Transform local position by parent */
    float world_x, world_y;
    apply_transform(local->local_x, local->local_y,
                    parent_x, parent_y,
                    parent_rot,
                    parent_sx, parent_sy,
                    &world_x, &world_y);

    /* Combine rotation and scale */
    out_world->world_x = world_x;
    out_world->world_y = world_y;
    out_world->world_rotation = parent_rot + local->rotation;
    out_world->world_scale_x = parent_sx * local->scale_x;
    out_world->world_scale_y = parent_sy * local->scale_y;
}

/* ============================================================================
 * Transform Update System
 * ============================================================================ */

/**
 * Recursively update world transforms for an entity and its children.
 */
static void update_entity_transform(ecs_world_t *world,
                                     ecs_entity_t entity,
                                     const C_WorldTransform *parent_world) {
    /* Get local transform */
    const C_Transform *local = ecs_get(world, entity, C_Transform);
    if (!local) return;

    /* Compute world transform */
    C_WorldTransform new_world;
    combine_transforms(local, parent_world, &new_world);

    /* Set world transform */
    ecs_set_id(world, entity, ecs_id(C_WorldTransform), sizeof(C_WorldTransform), &new_world);

    /* Update children recursively */
    ecs_iter_t it = ecs_children(world, entity);
    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            update_entity_transform(world, it.entities[i], &new_world);
        }
    }
}

/**
 * System callback for updating transforms.
 * Processes all root entities (entities with C_Transform but no parent).
 */
static void TransformPropagationSystem(ecs_iter_t *it) {
    C_Transform *transforms = ecs_field(it, C_Transform, 0);

    for (int i = 0; i < it->count; i++) {
        ecs_entity_t entity = it->entities[i];
        C_Transform *local = &transforms[i];

        /* Only process root entities (no parent) */
        ecs_entity_t parent = ecs_get_parent(it->world, entity);
        if (parent != 0) continue;

        /* Compute world transform for root */
        C_WorldTransform world_tf = {
            local->local_x,
            local->local_y,
            local->rotation,
            local->scale_x,
            local->scale_y
        };

        ecs_set_id(it->world, entity, ecs_id(C_WorldTransform),
                   sizeof(C_WorldTransform), &world_tf);

        /* Update children recursively */
        ecs_iter_t child_it = ecs_children(it->world, entity);
        while (ecs_children_next(&child_it)) {
            for (int j = 0; j < child_it.count; j++) {
                update_entity_transform(it->world, child_it.entities[j], &world_tf);
            }
        }
    }
}

/* ============================================================================
 * Registration
 * ============================================================================ */

void agentite_transform_register(ecs_world_t *world) {
    if (!world) return;

    /* Register components */
    ECS_COMPONENT_DEFINE(world, C_Transform);
    ECS_COMPONENT_DEFINE(world, C_WorldTransform);

    /* Register transform propagation system */
    ecs_entity_desc_t entity_desc = {};
    entity_desc.name = "TransformPropagationSystem";

    ecs_system_desc_t sys_desc = {};
    sys_desc.entity = ecs_entity_init(world, &entity_desc);
    sys_desc.query.terms[0].id = ecs_id(C_Transform);
    sys_desc.callback = TransformPropagationSystem;

    ecs_entity_t system = ecs_system_init(world, &sys_desc);

    /* Add dependency on EcsPostUpdate phase */
    ecs_add_pair(world, system, EcsDependsOn, EcsPostUpdate);
    ecs_add_id(world, system, EcsPostUpdate);
}

void agentite_transform_register_world(Agentite_World *world) {
    if (!world) return;
    agentite_transform_register(agentite_ecs_get_world(world));
}

/* ============================================================================
 * Parent-Child Hierarchy Functions
 * ============================================================================ */

void agentite_transform_set_parent(ecs_world_t *world,
                                    ecs_entity_t child,
                                    ecs_entity_t parent) {
    if (!world || !child) return;

    /* Ensure child has transform components */
    if (!ecs_has(world, child, C_Transform)) {
        C_Transform default_tf = C_TRANSFORM_DEFAULT;
        ecs_set_ptr(world, child, C_Transform, &default_tf);
    }

    if (!ecs_has(world, child, C_WorldTransform)) {
        C_WorldTransform default_world = {0, 0, 0, 1, 1};
        ecs_set_ptr(world, child, C_WorldTransform, &default_world);
    }

    /* Remove existing parent if any */
    ecs_entity_t old_parent = ecs_get_parent(world, child);
    if (old_parent != 0) {
        ecs_remove_pair(world, child, EcsChildOf, old_parent);
    }

    /* Set new parent */
    if (parent != 0) {
        ecs_add_pair(world, child, EcsChildOf, parent);
    }
}

ecs_entity_t agentite_transform_get_parent(ecs_world_t *world,
                                            ecs_entity_t entity) {
    if (!world || !entity) return 0;
    return ecs_get_parent(world, entity);
}

bool agentite_transform_has_parent(ecs_world_t *world, ecs_entity_t entity) {
    return agentite_transform_get_parent(world, entity) != 0;
}

int agentite_transform_get_children(ecs_world_t *world,
                                     ecs_entity_t parent,
                                     ecs_entity_t *out_children,
                                     int max_count) {
    if (!world || !parent) return 0;

    int count = 0;
    ecs_iter_t it = ecs_children(world, parent);

    while (ecs_children_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            if (out_children && count < max_count) {
                out_children[count] = it.entities[i];
            }
            count++;
        }
    }

    return count;
}

int agentite_transform_get_child_count(ecs_world_t *world,
                                        ecs_entity_t parent) {
    return agentite_transform_get_children(world, parent, NULL, 0);
}

void agentite_transform_remove_parent(ecs_world_t *world, ecs_entity_t entity) {
    agentite_transform_set_parent(world, entity, (ecs_entity_t)0);
}

/* ============================================================================
 * World Transform Access
 * ============================================================================ */

bool agentite_transform_get_world_position(ecs_world_t *world,
                                            ecs_entity_t entity,
                                            float *out_x,
                                            float *out_y) {
    if (!world || !entity) return false;

    const C_WorldTransform *wt = ecs_get(world, entity, C_WorldTransform);
    if (!wt) {
        /* Fallback to C_Transform if no world transform */
        const C_Transform *t = ecs_get(world, entity, C_Transform);
        if (!t) return false;

        if (out_x) *out_x = t->local_x;
        if (out_y) *out_y = t->local_y;
        return true;
    }

    if (out_x) *out_x = wt->world_x;
    if (out_y) *out_y = wt->world_y;
    return true;
}

float agentite_transform_get_world_rotation(ecs_world_t *world,
                                             ecs_entity_t entity) {
    if (!world || !entity) return 0.0f;

    const C_WorldTransform *wt = ecs_get(world, entity, C_WorldTransform);
    if (wt) return wt->world_rotation;

    const C_Transform *t = ecs_get(world, entity, C_Transform);
    if (t) return t->rotation;

    return 0.0f;
}

bool agentite_transform_get_world_scale(ecs_world_t *world,
                                         ecs_entity_t entity,
                                         float *out_sx,
                                         float *out_sy) {
    if (!world || !entity) return false;

    const C_WorldTransform *wt = ecs_get(world, entity, C_WorldTransform);
    if (!wt) {
        const C_Transform *t = ecs_get(world, entity, C_Transform);
        if (!t) return false;

        if (out_sx) *out_sx = t->scale_x;
        if (out_sy) *out_sy = t->scale_y;
        return true;
    }

    if (out_sx) *out_sx = wt->world_scale_x;
    if (out_sy) *out_sy = wt->world_scale_y;
    return true;
}

/* ============================================================================
 * Coordinate Conversion
 * ============================================================================ */

bool agentite_transform_local_to_world(ecs_world_t *world,
                                        ecs_entity_t entity,
                                        float local_x,
                                        float local_y,
                                        float *out_world_x,
                                        float *out_world_y) {
    if (!world || !entity) return false;

    const C_WorldTransform *wt = ecs_get(world, entity, C_WorldTransform);
    if (!wt) {
        /* No transform - just pass through */
        if (out_world_x) *out_world_x = local_x;
        if (out_world_y) *out_world_y = local_y;
        return true;
    }

    float wx, wy;
    apply_transform(local_x, local_y,
                    wt->world_x, wt->world_y,
                    wt->world_rotation,
                    wt->world_scale_x, wt->world_scale_y,
                    &wx, &wy);

    if (out_world_x) *out_world_x = wx;
    if (out_world_y) *out_world_y = wy;
    return true;
}

bool agentite_transform_world_to_local(ecs_world_t *world,
                                        ecs_entity_t entity,
                                        float world_x,
                                        float world_y,
                                        float *out_local_x,
                                        float *out_local_y) {
    if (!world || !entity) return false;

    const C_WorldTransform *wt = ecs_get(world, entity, C_WorldTransform);
    if (!wt) {
        if (out_local_x) *out_local_x = world_x;
        if (out_local_y) *out_local_y = world_y;
        return true;
    }

    /* Inverse transform: untranslate, unrotate, unscale */
    float dx = world_x - wt->world_x;
    float dy = world_y - wt->world_y;

    /* Inverse rotation */
    float cos_r = cosf(-wt->world_rotation);
    float sin_r = sinf(-wt->world_rotation);
    float unrotated_x = dx * cos_r - dy * sin_r;
    float unrotated_y = dx * sin_r + dy * cos_r;

    /* Inverse scale (with divide-by-zero protection) */
    float inv_sx = (wt->world_scale_x != 0.0f) ? (1.0f / wt->world_scale_x) : 1.0f;
    float inv_sy = (wt->world_scale_y != 0.0f) ? (1.0f / wt->world_scale_y) : 1.0f;

    if (out_local_x) *out_local_x = unrotated_x * inv_sx;
    if (out_local_y) *out_local_y = unrotated_y * inv_sy;
    return true;
}

/* ============================================================================
 * Transform Manipulation
 * ============================================================================ */

/**
 * Helper to get or create a mutable transform.
 */
static C_Transform *get_or_create_transform(ecs_world_t *world,
                                             ecs_entity_t entity) {
    if (!ecs_has(world, entity, C_Transform)) {
        C_Transform default_tf = C_TRANSFORM_DEFAULT;
        ecs_set_ptr(world, entity, C_Transform, &default_tf);
    }
    return ecs_get_mut(world, entity, C_Transform);
}

void agentite_transform_set_local_position(ecs_world_t *world,
                                            ecs_entity_t entity,
                                            float x,
                                            float y) {
    if (!world || !entity) return;

    C_Transform *t = get_or_create_transform(world, entity);
    if (t) {
        t->local_x = x;
        t->local_y = y;
        ecs_modified(world, entity, C_Transform);
    }
}

void agentite_transform_set_local_rotation(ecs_world_t *world,
                                            ecs_entity_t entity,
                                            float radians) {
    if (!world || !entity) return;

    C_Transform *t = get_or_create_transform(world, entity);
    if (t) {
        t->rotation = radians;
        ecs_modified(world, entity, C_Transform);
    }
}

void agentite_transform_set_local_scale(ecs_world_t *world,
                                         ecs_entity_t entity,
                                         float scale_x,
                                         float scale_y) {
    if (!world || !entity) return;

    C_Transform *t = get_or_create_transform(world, entity);
    if (t) {
        t->scale_x = scale_x;
        t->scale_y = scale_y;
        ecs_modified(world, entity, C_Transform);
    }
}

void agentite_transform_translate(ecs_world_t *world,
                                   ecs_entity_t entity,
                                   float dx,
                                   float dy) {
    if (!world || !entity) return;

    C_Transform *t = get_or_create_transform(world, entity);
    if (t) {
        t->local_x += dx;
        t->local_y += dy;
        ecs_modified(world, entity, C_Transform);
    }
}

void agentite_transform_rotate(ecs_world_t *world,
                                ecs_entity_t entity,
                                float delta_rad) {
    if (!world || !entity) return;

    C_Transform *t = get_or_create_transform(world, entity);
    if (t) {
        t->rotation += delta_rad;
        ecs_modified(world, entity, C_Transform);
    }
}

/* ============================================================================
 * Manual Transform Update
 * ============================================================================ */

void agentite_transform_update(ecs_world_t *world, ecs_entity_t entity) {
    if (!world || !entity) return;

    /* Get parent's world transform if any */
    const C_WorldTransform *parent_world = NULL;
    ecs_entity_t parent = ecs_get_parent(world, entity);
    if (parent != 0) {
        parent_world = ecs_get(world, parent, C_WorldTransform);
    }

    update_entity_transform(world, entity, parent_world);
}

void agentite_transform_update_all(ecs_world_t *world) {
    if (!world) return;

    /* Query all root transforms (transforms with no parent) */
    ecs_query_desc_t query_desc = {};
    query_desc.terms[0].id = ecs_id(C_Transform);

    ecs_query_t *q = ecs_query_init(world, &query_desc);
    if (!q) return;

    ecs_iter_t it = ecs_query_iter(world, q);
    while (ecs_query_next(&it)) {
        for (int i = 0; i < it.count; i++) {
            ecs_entity_t entity = it.entities[i];

            /* Only process roots */
            if (ecs_get_parent(world, entity) != 0) continue;

            update_entity_transform(world, entity, NULL);
        }
    }

    ecs_query_fini(q);
}
