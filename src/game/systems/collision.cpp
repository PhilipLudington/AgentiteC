#include "systems.h"
#include "../components.h"
#include <math.h>

/* Helper: Check AABB collision */
static bool aabb_overlap(float x1, float y1, float w1, float h1,
                         float x2, float y2, float w2, float h2) {
    return x1 < x2 + w2 && x1 + w1 > x2 &&
           y1 < y2 + h2 && y1 + h1 > y2;
}

void CollisionSystem(ecs_iter_t *it) {
    C_Position *pos = ecs_field(it, C_Position, 0);
    C_Collider *col = ecs_field(it, C_Collider, 1);

    /* Simple O(n^2) collision detection */
    /* For larger games, use spatial partitioning (grid, quadtree) */
    for (int i = 0; i < it->count; i++) {
        float x1 = pos[i].x + col[i].offset_x;
        float y1 = pos[i].y + col[i].offset_y;

        for (int j = i + 1; j < it->count; j++) {
            float x2 = pos[j].x + col[j].offset_x;
            float y2 = pos[j].y + col[j].offset_y;

            if (aabb_overlap(x1, y1, col[i].width, col[i].height,
                            x2, y2, col[j].width, col[j].height)) {
                /* Collision detected between entities[i] and entities[j] */

                /* Handle solid collisions (push apart) */
                if (col[i].solid && col[j].solid) {
                    /* Calculate overlap */
                    float overlap_x = fminf(x1 + col[i].width - x2,
                                           x2 + col[j].width - x1);
                    float overlap_y = fminf(y1 + col[i].height - y2,
                                           y2 + col[j].height - y1);

                    /* Push apart along smallest axis */
                    if (overlap_x < overlap_y) {
                        float push = overlap_x / 2.0f;
                        if (x1 < x2) {
                            pos[i].x -= push;
                            pos[j].x += push;
                        } else {
                            pos[i].x += push;
                            pos[j].x -= push;
                        }
                    } else {
                        float push = overlap_y / 2.0f;
                        if (y1 < y2) {
                            pos[i].y -= push;
                            pos[j].y += push;
                        } else {
                            pos[i].y += push;
                            pos[j].y -= push;
                        }
                    }
                }

                /* Trigger collisions can be handled here or via events */
                /* For now, just detect - game logic can query colliders */
            }
        }
    }
}

void ProjectileSystem(ecs_iter_t *it) {
    C_Projectile *proj = ecs_field(it, C_Projectile, 0);

    float dt = it->delta_time;

    for (int i = 0; i < it->count; i++) {
        proj[i].lifetime -= dt;

        /* Mark for deletion if expired */
        if (proj[i].lifetime <= 0) {
            ecs_delete(it->world, it->entities[i]);
        }
    }
}

void DamageSystem(ecs_iter_t *it) {
    C_Damage *dmg = ecs_field(it, C_Damage, 0);
    C_Position *pos = ecs_field(it, C_Position, 1);
    C_Collider *col = ecs_field(it, C_Collider, 2);

    (void)dmg;  /* Used when checking for collision with health entities */
    (void)pos;
    (void)col;

    /* This system would check for collisions with entities that have health */
    /* and apply damage. Implementation depends on your collision system. */

    /* Example pattern:
     * for (int i = 0; i < it->count; i++) {
     *     // Find entities with C_Health that overlap with this damage source
     *     // Apply dmg[i].amount to their health
     *     // Optionally destroy the damage source (for projectiles)
     * }
     */
}
