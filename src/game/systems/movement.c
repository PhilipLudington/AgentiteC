#include "systems.h"
#include "../components.h"
#include <math.h>

void MovementSystem(ecs_iter_t *it) {
    C_Position *pos = ecs_field(it, C_Position, 0);
    C_Velocity *vel = ecs_field(it, C_Velocity, 1);

    float dt = it->delta_time;

    for (int i = 0; i < it->count; i++) {
        pos[i].x += vel[i].vx * dt;
        pos[i].y += vel[i].vy * dt;
    }
}

void PlayerInputSystem(ecs_iter_t *it) {
    C_PlayerInput *input = ecs_field(it, C_PlayerInput, 0);
    C_Velocity *vel = ecs_field(it, C_Velocity, 1);
    C_Speed *speed = ecs_field(it, C_Speed, 2);

    float dt = it->delta_time;

    for (int i = 0; i < it->count; i++) {
        /* Calculate target velocity based on input */
        float target_vx = input[i].move_x * speed[i].speed;
        float target_vy = input[i].move_y * speed[i].speed;

        /* Accelerate towards target velocity */
        float accel = speed[i].acceleration * dt;

        if (fabsf(target_vx - vel[i].vx) < accel) {
            vel[i].vx = target_vx;
        } else if (target_vx > vel[i].vx) {
            vel[i].vx += accel;
        } else {
            vel[i].vx -= accel;
        }

        if (fabsf(target_vy - vel[i].vy) < accel) {
            vel[i].vy = target_vy;
        } else if (target_vy > vel[i].vy) {
            vel[i].vy += accel;
        } else {
            vel[i].vy -= accel;
        }
    }
}

void FrictionSystem(ecs_iter_t *it) {
    C_Velocity *vel = ecs_field(it, C_Velocity, 0);
    C_Speed *speed = ecs_field(it, C_Speed, 1);

    float dt = it->delta_time;

    for (int i = 0; i < it->count; i++) {
        float friction = speed[i].friction * dt;

        /* Apply friction to x velocity */
        if (vel[i].vx > 0) {
            vel[i].vx -= friction;
            if (vel[i].vx < 0) vel[i].vx = 0;
        } else if (vel[i].vx < 0) {
            vel[i].vx += friction;
            if (vel[i].vx > 0) vel[i].vx = 0;
        }

        /* Apply friction to y velocity */
        if (vel[i].vy > 0) {
            vel[i].vy -= friction;
            if (vel[i].vy < 0) vel[i].vy = 0;
        } else if (vel[i].vy < 0) {
            vel[i].vy += friction;
            if (vel[i].vy > 0) vel[i].vy = 0;
        }
    }
}
