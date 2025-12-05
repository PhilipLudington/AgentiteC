/**
 * Carbon Engine - Game Template Main Entry Point
 *
 * This is a minimal bootstrap that demonstrates the recommended
 * game structure using the Carbon_GameContext and Game template.
 *
 * For a comprehensive feature demo, see examples/demo/main.c
 */

#include "carbon/game_context.h"
#include "carbon/error.h"
#include "game/game.h"
#include <stdio.h>

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    /* Configure game context */
    Carbon_GameContextConfig config = CARBON_GAME_CONTEXT_DEFAULT;
    config.window_title = "Carbon Game";
    config.window_width = 1280;
    config.window_height = 720;

    /* Optional: Enable fonts for UI and text rendering */
    config.font_path = "assets/fonts/Roboto-Regular.ttf";
    config.font_size = 18.0f;
    config.ui_font_path = "assets/fonts/Roboto-Regular.ttf";
    config.ui_font_size = 16.0f;

    /* Create game context (initializes all engine systems) */
    Carbon_GameContext *ctx = carbon_game_context_create(&config);
    if (!ctx) {
        fprintf(stderr, "Failed to initialize: %s\n", carbon_get_last_error());
        return 1;
    }

    /* Initialize game (state machine, ECS systems, etc.) */
    Game *game = game_init(ctx);
    if (!game) {
        fprintf(stderr, "Failed to initialize game: %s\n", carbon_get_last_error());
        carbon_game_context_destroy(ctx);
        return 1;
    }

    /* Main game loop */
    while (carbon_game_context_is_running(ctx)) {
        /* Begin frame and poll events */
        carbon_game_context_begin_frame(ctx);
        carbon_game_context_poll_events(ctx);

        /* Update game logic */
        game_update(game, ctx);

        /* Begin rendering */
        SDL_GPUCommandBuffer *cmd = carbon_game_context_begin_render(ctx);
        if (cmd) {
            /* Begin UI frame for this state */
            if (ctx->ui) {
                cui_begin_frame(ctx->ui, ctx->delta_time);
            }

            /* Let game render its current state (includes UI) */
            /* Note: game_render adds UI widgets but doesn't upload yet */

            /* Build sprite batch (if you have world sprites to draw) */
            carbon_sprite_begin(ctx->sprites, NULL);
            /* Draw world sprites here... */
            carbon_sprite_upload(ctx->sprites, cmd);

            /* End UI frame and upload */
            if (ctx->ui) {
                cui_end_frame(ctx->ui);
                cui_upload(ctx->ui, cmd);
            }

            /* Begin render pass */
            if (carbon_game_context_begin_render_pass(ctx, 0.1f, 0.1f, 0.15f, 1.0f)) {
                SDL_GPURenderPass *pass = carbon_get_render_pass(ctx->engine);

                /* Render sprites (world) */
                carbon_sprite_render(ctx->sprites, cmd, pass);

                /* Render game state (UI) */
                game_render(game, ctx, cmd, pass);

                /* Render UI on top */
                if (ctx->ui) {
                    cui_render(ctx->ui, cmd, pass);
                }

                carbon_game_context_end_render_pass(ctx);
            }

            /* End sprite batch */
            carbon_sprite_end(ctx->sprites, NULL, NULL);
        }

        carbon_game_context_end_frame(ctx);
    }

    /* Cleanup */
    game_shutdown(game);
    carbon_game_context_destroy(ctx);

    return 0;
}
