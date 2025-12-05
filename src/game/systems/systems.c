#include "systems.h"
#include "../components.h"

void game_systems_register(ecs_world_t *world) {
    if (!world) return;

    /* Movement systems */
    ECS_SYSTEM(world, MovementSystem, EcsOnUpdate, C_Position, C_Velocity);
    ECS_SYSTEM(world, PlayerInputSystem, EcsOnUpdate, C_PlayerInput, C_Velocity, C_Speed);
    ECS_SYSTEM(world, FrictionSystem, EcsOnUpdate, C_Velocity, C_Speed);

    /* Collision systems */
    ECS_SYSTEM(world, CollisionSystem, EcsOnUpdate, C_Position, C_Collider);
    ECS_SYSTEM(world, ProjectileSystem, EcsOnUpdate, C_Projectile);
    ECS_SYSTEM(world, DamageSystem, EcsOnUpdate, C_Damage, C_Position, C_Collider);

    /* AI systems */
    ECS_SYSTEM(world, AIBehaviorSystem, EcsOnUpdate, C_AIState, C_Position, C_Enemy);
    ECS_SYSTEM(world, PathFollowSystem, EcsOnUpdate, C_PathFollow, C_Position, C_Velocity);
}
