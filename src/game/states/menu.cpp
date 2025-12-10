#include "state.h"
#include "agentite/ui.h"

/* Menu state userdata */
typedef struct {
    bool start_clicked;
    bool quit_clicked;
} MenuStateData;

static MenuStateData menu_data = {0};

static void menu_enter(Agentite_GameContext *ctx, void *userdata) {
    (void)ctx;
    MenuStateData *data = (MenuStateData *)userdata;
    data->start_clicked = false;
    data->quit_clicked = false;
}

static void menu_exit(Agentite_GameContext *ctx, void *userdata) {
    (void)ctx;
    (void)userdata;
}

static void menu_update(Agentite_GameContext *ctx, float dt, void *userdata) {
    (void)dt;
    MenuStateData *data = (MenuStateData *)userdata;

    if (data->quit_clicked) {
        agentite_game_context_quit(ctx);
    }

    /* Start game is handled by the game.c which checks start_clicked */
}

static void menu_render(Agentite_GameContext *ctx,
                        SDL_GPUCommandBuffer *cmd,
                        SDL_GPURenderPass *pass,
                        void *userdata) {
    (void)cmd;
    (void)pass;
    MenuStateData *data = (MenuStateData *)userdata;

    if (!ctx->ui) return;

    /* Center the menu on screen */
    float panel_width = 300;
    float panel_height = 250;
    float panel_x = (ctx->window_width - panel_width) / 2;
    float panel_y = (ctx->window_height - panel_height) / 2;

    if (aui_begin_panel(ctx->ui, "Main Menu", panel_x, panel_y,
                        panel_width, panel_height,
                        AUI_PANEL_TITLE_BAR | AUI_PANEL_BORDER)) {

        aui_spacing(ctx->ui, 20);

        /* Center buttons */
        float button_width = 200;
        float offset = (panel_width - button_width) / 2 - 10;

        aui_spacing(ctx->ui, offset);

        if (aui_button(ctx->ui, "Start Game")) {
            data->start_clicked = true;
        }

        aui_spacing(ctx->ui, 10);

        if (aui_button(ctx->ui, "Options")) {
            /* TODO: Show options menu */
        }

        aui_spacing(ctx->ui, 10);

        if (aui_button(ctx->ui, "Quit")) {
            data->quit_clicked = true;
        }

        aui_end_panel(ctx->ui);
    }
}

GameState game_state_menu_create(void) {
    return (GameState){
        .name = "Menu",
        .enter = menu_enter,
        .exit = menu_exit,
        .update = menu_update,
        .render = menu_render,
        .userdata = &menu_data
    };
}

/* Access for game.c to check if start was clicked */
bool game_state_menu_start_clicked(void) {
    return menu_data.start_clicked;
}

void game_state_menu_clear_start(void) {
    menu_data.start_clicked = false;
}
