#include "components.h"

/* Define component IDs (declared extern in header) */
ECS_COMPONENT_DECLARE(C_Player);
ECS_COMPONENT_DECLARE(C_PlayerInput);
ECS_COMPONENT_DECLARE(C_Speed);
ECS_COMPONENT_DECLARE(C_Collider);
ECS_COMPONENT_DECLARE(C_Enemy);
ECS_COMPONENT_DECLARE(C_Damage);
ECS_COMPONENT_DECLARE(C_Projectile);
ECS_COMPONENT_DECLARE(C_AIState);
ECS_COMPONENT_DECLARE(C_PathFollow);
ECS_COMPONENT_DECLARE(C_Sprite);
ECS_COMPONENT_DECLARE(C_Animated);

void game_components_register(ecs_world_t *world) {
    if (!world) return;

    /* Register player components */
    ECS_COMPONENT_DEFINE(world, C_Player);
    ECS_COMPONENT_DEFINE(world, C_PlayerInput);

    /* Register movement/physics components */
    ECS_COMPONENT_DEFINE(world, C_Speed);
    ECS_COMPONENT_DEFINE(world, C_Collider);

    /* Register combat components */
    ECS_COMPONENT_DEFINE(world, C_Enemy);
    ECS_COMPONENT_DEFINE(world, C_Damage);
    ECS_COMPONENT_DEFINE(world, C_Projectile);

    /* Register AI components */
    ECS_COMPONENT_DEFINE(world, C_AIState);
    ECS_COMPONENT_DEFINE(world, C_PathFollow);

    /* Register visual components */
    ECS_COMPONENT_DEFINE(world, C_Sprite);
    ECS_COMPONENT_DEFINE(world, C_Animated);
}
