#ifndef GAME_SYSTEMS_H
#define GAME_SYSTEMS_H

#include "agentite/ecs.h"
#include "agentite/game_context.h"

/**
 * Game ECS Systems
 *
 * Systems process entities with specific component combinations.
 * Register systems during game initialization.
 *
 * System pattern:
 *   void MySystem(ecs_iter_t *it) {
 *       C_Position *pos = ecs_field(it, C_Position, 0);
 *       C_Velocity *vel = ecs_field(it, C_Velocity, 1);
 *       for (int i = 0; i < it->count; i++) {
 *           pos[i].x += vel[i].vx * it->delta_time;
 *           pos[i].y += vel[i].vy * it->delta_time;
 *       }
 *   }
 */

/**
 * Register all game systems with the ECS world.
 *
 * @param world Flecs world
 */
void game_systems_register(ecs_world_t *world);

/*============================================================================
 * Movement Systems
 *============================================================================*/

/**
 * Apply velocity to position.
 * Processes entities with: C_Position, C_Velocity
 */
void MovementSystem(ecs_iter_t *it);

/**
 * Apply player input to velocity.
 * Processes entities with: C_PlayerInput, C_Velocity, C_Speed
 */
void PlayerInputSystem(ecs_iter_t *it);

/**
 * Apply friction to slow down entities.
 * Processes entities with: C_Velocity, C_Speed
 */
void FrictionSystem(ecs_iter_t *it);

/*============================================================================
 * Collision Systems
 *============================================================================*/

/**
 * Check collisions between entities with colliders.
 * Processes entities with: C_Position, C_Collider
 */
void CollisionSystem(ecs_iter_t *it);

/*============================================================================
 * Combat Systems
 *============================================================================*/

/**
 * Update projectile lifetime and destroy expired projectiles.
 * Processes entities with: C_Projectile
 */
void ProjectileSystem(ecs_iter_t *it);

/**
 * Apply damage when entities collide.
 * Processes entities with: C_Damage, C_Position, C_Collider
 */
void DamageSystem(ecs_iter_t *it);

/*============================================================================
 * AI Systems
 *============================================================================*/

/**
 * Simple AI behavior (chase player, etc.).
 * Processes entities with: C_AIState, C_Position, C_Enemy
 */
void AIBehaviorSystem(ecs_iter_t *it);

/**
 * Follow a path (for pathfinding).
 * Processes entities with: C_PathFollow, C_Position, C_Velocity
 */
void PathFollowSystem(ecs_iter_t *it);

#endif /* GAME_SYSTEMS_H */
