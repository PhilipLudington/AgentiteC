#include "systems.h"
#include "../components.h"
#include <math.h>

/* AI States */
enum AIState {
    AI_STATE_IDLE = 0,
    AI_STATE_CHASE,
    AI_STATE_ATTACK,
    AI_STATE_FLEE
};

void AIBehaviorSystem(ecs_iter_t *it) {
    C_AIState *ai = ecs_field(it, C_AIState, 0);
    C_Position *pos = ecs_field(it, C_Position, 1);
    C_Enemy *enemy = ecs_field(it, C_Enemy, 2);

    float dt = it->delta_time;

    for (int i = 0; i < it->count; i++) {
        /* Update state timer */
        ai[i].state_timer += dt;

        /* Get target position if we have a valid target */
        float target_x = 0, target_y = 0;
        bool has_target = false;

        if (ai[i].target != 0 && ecs_is_alive(it->world, ai[i].target)) {
            const C_Position *target_pos = ecs_get(it->world, ai[i].target, C_Position);
            if (target_pos) {
                target_x = target_pos->x;
                target_y = target_pos->y;
                has_target = true;
            }
        }

        /* Calculate distance to target */
        float dx = target_x - pos[i].x;
        float dy = target_y - pos[i].y;
        float dist = sqrtf(dx * dx + dy * dy);

        /* State machine */
        switch (ai[i].state) {
            case AI_STATE_IDLE:
                /* Look for targets in aggro range */
                if (has_target && dist <= enemy[i].aggro_range) {
                    ai[i].state = AI_STATE_CHASE;
                    ai[i].state_timer = 0;
                }
                break;

            case AI_STATE_CHASE:
                /* Move towards target */
                if (!has_target || dist > enemy[i].aggro_range * 1.5f) {
                    /* Lost target, go idle */
                    ai[i].state = AI_STATE_IDLE;
                    ai[i].state_timer = 0;
                } else if (dist < 50.0f) {
                    /* Close enough to attack */
                    ai[i].state = AI_STATE_ATTACK;
                    ai[i].state_timer = 0;
                }
                /* Movement is handled by setting velocity */
                /* The entity should also have C_Velocity for actual movement */
                break;

            case AI_STATE_ATTACK:
                /* Attack cooldown */
                if (ai[i].state_timer > 1.0f) {
                    if (dist > 50.0f) {
                        ai[i].state = AI_STATE_CHASE;
                    }
                    ai[i].state_timer = 0;
                }
                break;

            case AI_STATE_FLEE:
                /* Run away from target */
                if (dist > enemy[i].aggro_range * 2.0f) {
                    ai[i].state = AI_STATE_IDLE;
                    ai[i].state_timer = 0;
                }
                break;
        }
    }
}

void PathFollowSystem(ecs_iter_t *it) {
    C_PathFollow *path = ecs_field(it, C_PathFollow, 0);
    C_Position *pos = ecs_field(it, C_Position, 1);
    C_Velocity *vel = ecs_field(it, C_Velocity, 2);

    for (int i = 0; i < it->count; i++) {
        if (path[i].path_index >= path[i].path_length) {
            /* Path complete - stop moving */
            vel[i].vx = 0;
            vel[i].vy = 0;
            continue;
        }

        /* Calculate direction to current waypoint */
        float dx = path[i].waypoint_x - pos[i].x;
        float dy = path[i].waypoint_y - pos[i].y;
        float dist = sqrtf(dx * dx + dy * dy);

        if (dist < path[i].path_tolerance) {
            /* Reached waypoint - advance to next */
            path[i].path_index++;
            /* Note: The next waypoint position should be set by the game logic */
            /* when updating the path, as we don't store the full path here */
        } else {
            /* Move towards waypoint */
            float speed = 100.0f;  /* Should come from C_Speed if present */
            vel[i].vx = (dx / dist) * speed;
            vel[i].vy = (dy / dist) * speed;
        }
    }
}
