#include "state.h"
#include "carbon/ecs.h"
#include "../components.h"

/* Playing state userdata */
typedef struct {
    bool pause_requested;
} PlayingStateData;

static PlayingStateData playing_data = {0};

static void playing_enter(Carbon_GameContext *ctx, void *userdata) {
    (void)ctx;
    PlayingStateData *data = (PlayingStateData *)userdata;
    data->pause_requested = false;

    /* Initialize game world entities here if needed */
    /* This could load a level, spawn player, etc. */
}

static void playing_exit(Carbon_GameContext *ctx, void *userdata) {
    (void)ctx;
    (void)userdata;

    /* Cleanup game world if needed */
}

static void playing_update(Carbon_GameContext *ctx, float dt, void *userdata) {
    PlayingStateData *data = (PlayingStateData *)userdata;

    /* Check for pause input */
    if (carbon_input_key_just_pressed(ctx->input, SDL_SCANCODE_ESCAPE)) {
        data->pause_requested = true;
    }

    /* Update ECS world */
    if (ctx->ecs) {
        carbon_ecs_progress(ctx->ecs, dt);
    }

    /* Game logic update would go here:
     * - Handle player input
     * - Update game state
     * - Check win/lose conditions
     */
}

static void playing_render(Carbon_GameContext *ctx,
                           SDL_GPUCommandBuffer *cmd,
                           SDL_GPURenderPass *pass,
                           void *userdata) {
    (void)userdata;
    (void)cmd;
    (void)pass;

    /* Render game world
     * - Tilemap (if using)
     * - Sprites for entities
     * - Effects
     * - UI overlay (health, score, etc.)
     */

    /* Example: Draw a simple HUD */
    if (ctx->ui) {
        /* Mini HUD in corner */
        if (cui_begin_panel(ctx->ui, "##hud", 10, 10, 150, 50, 0)) {
            cui_label(ctx->ui, "Playing...");
            cui_label(ctx->ui, "ESC to pause");
            cui_end_panel(ctx->ui);
        }
    }
}

GameState game_state_playing_create(void) {
    return (GameState){
        .name = "Playing",
        .enter = playing_enter,
        .exit = playing_exit,
        .update = playing_update,
        .render = playing_render,
        .userdata = &playing_data
    };
}

/* Access for game.c to check if pause was requested */
bool game_state_playing_pause_requested(void) {
    return playing_data.pause_requested;
}

void game_state_playing_clear_pause(void) {
    playing_data.pause_requested = false;
}
