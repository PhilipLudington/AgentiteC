#include "game.h"
#include "systems/systems.h"
#include "carbon/error.h"
#include <stdlib.h>

/* External state check functions */
extern bool game_state_menu_start_clicked(void);
extern void game_state_menu_clear_start(void);
extern bool game_state_playing_pause_requested(void);
extern void game_state_playing_clear_pause(void);
extern bool game_state_paused_resume_clicked(void);
extern void game_state_paused_clear_resume(void);

Game *game_init(Carbon_GameContext *ctx) {
    if (!ctx) {
        carbon_set_error("Game init: NULL context");
        return NULL;
    }

    Game *game = calloc(1, sizeof(Game));
    if (!game) {
        carbon_set_error("Game init: Failed to allocate game");
        return NULL;
    }

    /* Register game-specific components */
    if (ctx->ecs) {
        ecs_world_t *w = carbon_ecs_get_world(ctx->ecs);
        game_components_register(w);
        game_systems_register(w);
    }

    /* Create state machine */
    game->state_machine = game_state_machine_create();
    if (!game->state_machine) {
        carbon_set_error("Game init: Failed to create state machine");
        free(game);
        return NULL;
    }

    /* Register states */
    GameState menu_state = game_state_menu_create();
    GameState playing_state = game_state_playing_create();
    GameState paused_state = game_state_paused_create();

    game_state_machine_register(game->state_machine, GAME_STATE_MENU, &menu_state);
    game_state_machine_register(game->state_machine, GAME_STATE_PLAYING, &playing_state);
    game_state_machine_register(game->state_machine, GAME_STATE_PAUSED, &paused_state);

    /* Start in menu state */
    game_state_machine_change(game->state_machine, GAME_STATE_MENU, ctx);

    game->paused = false;
    game->debug_mode = false;

    return game;
}

void game_shutdown(Game *game) {
    if (!game) return;

    if (game->state_machine) {
        game_state_machine_destroy(game->state_machine);
    }

    /* Free game-specific resources here */

    free(game);
}

void game_update(Game *game, Carbon_GameContext *ctx) {
    if (!game || !ctx) return;

    float dt = ctx->delta_time;

    /* Update state machine */
    game_state_machine_update(game->state_machine, ctx, dt);

    /* Handle state transitions based on state callbacks */
    GameStateID current = game_state_machine_current(game->state_machine);

    switch (current) {
        case GAME_STATE_MENU:
            if (game_state_menu_start_clicked()) {
                game_state_menu_clear_start();
                game_state_machine_change(game->state_machine, GAME_STATE_PLAYING, ctx);
            }
            break;

        case GAME_STATE_PLAYING:
            if (game_state_playing_pause_requested()) {
                game_state_playing_clear_pause();
                game_state_machine_change(game->state_machine, GAME_STATE_PAUSED, ctx);
            }
            break;

        case GAME_STATE_PAUSED:
            if (game_state_paused_resume_clicked()) {
                game_state_paused_clear_resume();
                game_state_machine_change(game->state_machine, GAME_STATE_PLAYING, ctx);
            }
            break;

        default:
            break;
    }
}

void game_render(Game *game, Carbon_GameContext *ctx,
                 SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass) {
    if (!game || !ctx) return;

    /* Render current state */
    game_state_machine_render(game->state_machine, ctx, cmd, pass);
}

void game_toggle_pause(Game *game) {
    if (game) {
        game->paused = !game->paused;
    }
}

bool game_is_paused(Game *game) {
    return game ? game->paused : false;
}
