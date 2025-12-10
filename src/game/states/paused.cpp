#include "state.h"
#include "agentite/ui.h"

/* Paused state userdata */
typedef struct {
    bool resume_clicked;
    bool quit_clicked;
} PausedStateData;

static PausedStateData paused_data = {0};

static void paused_enter(Agentite_GameContext *ctx, void *userdata) {
    (void)ctx;
    PausedStateData *data = (PausedStateData *)userdata;
    data->resume_clicked = false;
    data->quit_clicked = false;
}

static void paused_exit(Agentite_GameContext *ctx, void *userdata) {
    (void)ctx;
    (void)userdata;
}

static void paused_update(Agentite_GameContext *ctx, float dt, void *userdata) {
    (void)dt;
    PausedStateData *data = (PausedStateData *)userdata;

    /* Check for unpause via escape key */
    if (agentite_input_key_just_pressed(ctx->input, SDL_SCANCODE_ESCAPE)) {
        data->resume_clicked = true;
    }

    if (data->quit_clicked) {
        agentite_game_context_quit(ctx);
    }
}

static void paused_render(Agentite_GameContext *ctx,
                          SDL_GPUCommandBuffer *cmd,
                          SDL_GPURenderPass *pass,
                          void *userdata) {
    (void)cmd;
    (void)pass;
    PausedStateData *data = (PausedStateData *)userdata;

    if (!ctx->ui) return;

    /* Semi-transparent overlay would be nice but we'll just do a panel */

    /* Center the pause menu */
    float panel_width = 250;
    float panel_height = 200;
    float panel_x = (ctx->window_width - panel_width) / 2;
    float panel_y = (ctx->window_height - panel_height) / 2;

    if (aui_begin_panel(ctx->ui, "Paused", panel_x, panel_y,
                        panel_width, panel_height,
                        AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

        aui_spacing(ctx->ui, 15);

        if (aui_button(ctx->ui, "Resume")) {
            data->resume_clicked = true;
        }

        aui_spacing(ctx->ui, 10);

        if (aui_button(ctx->ui, "Options")) {
            /* TODO: Show options */
        }

        aui_spacing(ctx->ui, 10);

        if (aui_button(ctx->ui, "Quit to Menu")) {
            data->quit_clicked = true;
        }

        aui_end_panel(ctx->ui);
    }
}

GameState game_state_paused_create(void) {
    return (GameState){
        .name = "Paused",
        .enter = paused_enter,
        .exit = paused_exit,
        .update = paused_update,
        .render = paused_render,
        .userdata = &paused_data
    };
}

/* Access for game.c to check if resume was clicked */
bool game_state_paused_resume_clicked(void) {
    return paused_data.resume_clicked;
}

void game_state_paused_clear_resume(void) {
    paused_data.resume_clicked = false;
}
