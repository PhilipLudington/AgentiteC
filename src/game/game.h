#ifndef GAME_H
#define GAME_H

#include "carbon/game_context.h"
#include "components.h"
#include "states/state.h"

/**
 * Game Structure
 *
 * Contains all game-specific state. The engine context is passed in,
 * and the game manages its own resources and state machine.
 *
 * Usage:
 *   Game *game = game_init(ctx);
 *   while (carbon_game_context_is_running(ctx)) {
 *       game_update(game, ctx->delta_time);
 *       game_render(game, ctx);
 *   }
 *   game_shutdown(game);
 */

typedef struct Game {
    /* State machine */
    GameStateMachine *state_machine;

    /* Game-specific resources */
    /* Add your textures, sounds, etc. here */

    /* Game state flags */
    bool paused;
    bool debug_mode;
} Game;

/**
 * Initialize the game.
 * Registers components, creates systems, and sets up initial state.
 *
 * @param ctx Game context with all engine systems
 * @return Initialized game, or NULL on failure
 */
Game *game_init(Carbon_GameContext *ctx);

/**
 * Shutdown the game and free resources.
 *
 * @param game Game to shutdown
 */
void game_shutdown(Game *game);

/**
 * Update game logic.
 * Called once per frame before rendering.
 *
 * @param game Game instance
 * @param ctx Game context
 */
void game_update(Game *game, Carbon_GameContext *ctx);

/**
 * Render the game.
 * Called after update, handles all drawing.
 *
 * @param game Game instance
 * @param ctx Game context
 * @param cmd GPU command buffer
 * @param pass Render pass
 */
void game_render(Game *game, Carbon_GameContext *ctx,
                 SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass);

/**
 * Toggle pause state.
 *
 * @param game Game instance
 */
void game_toggle_pause(Game *game);

/**
 * Check if game is paused.
 *
 * @param game Game instance
 * @return true if paused
 */
bool game_is_paused(Game *game);

#endif /* GAME_H */
