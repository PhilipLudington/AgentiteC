#include "components.h"
#include "agentite/ecs_reflect.h"

/* Define component IDs (declared extern in header) */
ECS_COMPONENT_DECLARE(C_Player);
ECS_COMPONENT_DECLARE(C_PlayerInput);
ECS_COMPONENT_DECLARE(C_Speed);
ECS_COMPONENT_DECLARE(C_Collider);
ECS_COMPONENT_DECLARE(C_Physics2DBody);
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
    ECS_COMPONENT_DEFINE(world, C_Physics2DBody);

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

void game_components_register_reflection(ecs_world_t *world,
                                          Agentite_ReflectRegistry *registry)
{
    if (!world || !registry) return;

    /* Register built-in engine components */
    AGENTITE_REFLECT_COMPONENT(registry, world, C_Position,
        AGENTITE_FIELD(C_Position, x, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Position, y, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Velocity,
        AGENTITE_FIELD(C_Velocity, vx, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Velocity, vy, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Size,
        AGENTITE_FIELD(C_Size, width, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Size, height, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Color,
        AGENTITE_FIELD(C_Color, r, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Color, g, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Color, b, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Color, a, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Name,
        AGENTITE_FIELD(C_Name, name, AGENTITE_FIELD_STRING)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Active,
        AGENTITE_FIELD(C_Active, active, AGENTITE_FIELD_BOOL)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Health,
        AGENTITE_FIELD(C_Health, health, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Health, max_health, AGENTITE_FIELD_INT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_RenderLayer,
        AGENTITE_FIELD(C_RenderLayer, layer, AGENTITE_FIELD_INT)
    );

    /* Register game-specific components */
    AGENTITE_REFLECT_COMPONENT(registry, world, C_Player,
        AGENTITE_FIELD(C_Player, player_index, AGENTITE_FIELD_INT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_PlayerInput,
        AGENTITE_FIELD(C_PlayerInput, move_x, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_PlayerInput, move_y, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_PlayerInput, action_primary, AGENTITE_FIELD_BOOL),
        AGENTITE_FIELD(C_PlayerInput, action_secondary, AGENTITE_FIELD_BOOL)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Speed,
        AGENTITE_FIELD(C_Speed, speed, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Speed, acceleration, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Speed, friction, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Collider,
        AGENTITE_FIELD(C_Collider, offset_x, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Collider, offset_y, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Collider, width, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Collider, height, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Collider, solid, AGENTITE_FIELD_BOOL),
        AGENTITE_FIELD(C_Collider, trigger, AGENTITE_FIELD_BOOL)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Physics2DBody,
        AGENTITE_FIELD(C_Physics2DBody, sync_to_transform, AGENTITE_FIELD_BOOL),
        AGENTITE_FIELD(C_Physics2DBody, sync_from_transform, AGENTITE_FIELD_BOOL)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Enemy,
        AGENTITE_FIELD(C_Enemy, enemy_type, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Enemy, aggro_range, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Damage,
        AGENTITE_FIELD(C_Damage, amount, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Damage, damage_type, AGENTITE_FIELD_INT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Projectile,
        AGENTITE_FIELD(C_Projectile, owner, AGENTITE_FIELD_ENTITY),
        AGENTITE_FIELD(C_Projectile, lifetime, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Projectile, max_lifetime, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_AIState,
        AGENTITE_FIELD(C_AIState, state, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_AIState, state_timer, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_AIState, target, AGENTITE_FIELD_ENTITY)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_PathFollow,
        AGENTITE_FIELD(C_PathFollow, path_index, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_PathFollow, path_length, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_PathFollow, waypoint_x, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_PathFollow, waypoint_y, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_PathFollow, path_tolerance, AGENTITE_FIELD_FLOAT)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Sprite,
        AGENTITE_FIELD(C_Sprite, sprite_id, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Sprite, origin_x, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Sprite, origin_y, AGENTITE_FIELD_FLOAT),
        AGENTITE_FIELD(C_Sprite, flip_x, AGENTITE_FIELD_BOOL),
        AGENTITE_FIELD(C_Sprite, flip_y, AGENTITE_FIELD_BOOL)
    );

    AGENTITE_REFLECT_COMPONENT(registry, world, C_Animated,
        AGENTITE_FIELD(C_Animated, animation_id, AGENTITE_FIELD_INT),
        AGENTITE_FIELD(C_Animated, speed_multiplier, AGENTITE_FIELD_FLOAT)
    );
}
