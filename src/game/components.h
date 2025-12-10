#ifndef GAME_COMPONENTS_H
#define GAME_COMPONENTS_H

#include "agentite/ecs.h"

/**
 * Game-Specific ECS Components
 *
 * Define your game's custom components here. These are registered
 * with the ECS world during game_init().
 *
 * Built-in Carbon components (from carbon/ecs.h):
 *   - C_Position: x, y coordinates
 *   - C_Velocity: vx, vy velocities
 *   - C_Size: width, height
 *   - C_Color: r, g, b, a
 *   - C_Name: name string
 *   - C_Active: is_active flag
 *   - C_Health: health, max_health
 *   - C_RenderLayer: layer number
 */

/*============================================================================
 * Player Components
 *============================================================================*/

/**
 * Tag component for the player entity.
 */
typedef struct C_Player {
    int player_index;       /* For multiplayer (0 = player 1, etc.) */
} C_Player;

/**
 * Player input state component.
 */
typedef struct C_PlayerInput {
    float move_x;           /* Movement input (-1 to 1) */
    float move_y;           /* Movement input (-1 to 1) */
    bool action_primary;    /* Primary action (attack, select, etc.) */
    bool action_secondary;  /* Secondary action (cancel, alternative, etc.) */
} C_PlayerInput;

/*============================================================================
 * Movement/Physics Components
 *============================================================================*/

/**
 * Speed component for movement systems.
 */
typedef struct C_Speed {
    float speed;            /* Base movement speed */
    float acceleration;     /* Acceleration rate */
    float friction;         /* Friction/deceleration */
} C_Speed;

/**
 * Collision bounds component.
 */
typedef struct C_Collider {
    float offset_x;         /* Offset from position */
    float offset_y;
    float width;            /* Collision width */
    float height;           /* Collision height */
    bool solid;             /* Blocks movement */
    bool trigger;           /* Triggers events but doesn't block */
} C_Collider;

/*============================================================================
 * Combat Components
 *============================================================================*/

/**
 * Tag component for enemy entities.
 */
typedef struct C_Enemy {
    int enemy_type;         /* Type identifier for AI behavior */
    float aggro_range;      /* Distance to detect player */
} C_Enemy;

/**
 * Damage component for attacks/projectiles.
 */
typedef struct C_Damage {
    int amount;             /* Damage amount */
    int damage_type;        /* Type (physical, magic, etc.) */
} C_Damage;

/**
 * Projectile component.
 */
typedef struct C_Projectile {
    ecs_entity_t owner;     /* Entity that fired this */
    float lifetime;         /* Remaining lifetime in seconds */
    float max_lifetime;     /* Maximum lifetime */
} C_Projectile;

/*============================================================================
 * AI Components
 *============================================================================*/

/**
 * Simple AI state component.
 */
typedef struct C_AIState {
    int state;              /* Current AI state (idle, chase, attack, etc.) */
    float state_timer;      /* Time in current state */
    ecs_entity_t target;    /* Current target entity */
} C_AIState;

/**
 * Pathfinding component.
 */
typedef struct C_PathFollow {
    int path_index;         /* Current waypoint index */
    int path_length;        /* Total waypoints */
    float waypoint_x;       /* Current waypoint position */
    float waypoint_y;
    float path_tolerance;   /* Distance to consider waypoint reached */
} C_PathFollow;

/*============================================================================
 * Visual Components
 *============================================================================*/

/**
 * Sprite reference component.
 */
typedef struct C_Sprite {
    int sprite_id;          /* Sprite/texture identifier */
    float origin_x;         /* Origin for rotation (0-1) */
    float origin_y;
    bool flip_x;            /* Horizontal flip */
    bool flip_y;            /* Vertical flip */
} C_Sprite;

/**
 * Animation reference component.
 */
typedef struct C_Animated {
    int animation_id;       /* Current animation identifier */
    float speed_multiplier; /* Animation speed multiplier */
} C_Animated;

/*============================================================================
 * Component Registration
 *============================================================================*/

/**
 * Register all game-specific components with the ECS world.
 * Call this during game_init().
 *
 * @param world Flecs world
 */
void game_components_register(ecs_world_t *world);

/*============================================================================
 * Component Declarations (for Flecs)
 *============================================================================*/

/* These macros tell Flecs about our component types */
/* Use extern for C++ to avoid multiple definitions */
extern ECS_COMPONENT_DECLARE(C_Player);
extern ECS_COMPONENT_DECLARE(C_PlayerInput);
extern ECS_COMPONENT_DECLARE(C_Speed);
extern ECS_COMPONENT_DECLARE(C_Collider);
extern ECS_COMPONENT_DECLARE(C_Enemy);
extern ECS_COMPONENT_DECLARE(C_Damage);
extern ECS_COMPONENT_DECLARE(C_Projectile);
extern ECS_COMPONENT_DECLARE(C_AIState);
extern ECS_COMPONENT_DECLARE(C_PathFollow);
extern ECS_COMPONENT_DECLARE(C_Sprite);
extern ECS_COMPONENT_DECLARE(C_Animated);

#endif /* GAME_COMPONENTS_H */
