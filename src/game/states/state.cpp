#include "carbon/carbon.h"
#include "state.h"
#include <stdlib.h>
#include <string.h>

GameStateMachine *game_state_machine_create(void) {
    GameStateMachine *sm = CARBON_ALLOC(GameStateMachine);
    if (!sm) return NULL;

    sm->current = GAME_STATE_NONE;
    sm->previous = GAME_STATE_NONE;
    sm->pending = GAME_STATE_NONE;
    sm->changing = false;

    return sm;
}

void game_state_machine_destroy(GameStateMachine *sm) {
    if (!sm) return;
    free(sm);
}

void game_state_machine_register(GameStateMachine *sm, GameStateID id,
                                  const GameState *state) {
    if (!sm || id < 0 || id >= GAME_MAX_STATES || !state) return;
    sm->states[id] = *state;
}

void game_state_machine_change(GameStateMachine *sm, GameStateID id,
                                Carbon_GameContext *ctx) {
    if (!sm || id < 0 || id >= GAME_MAX_STATES) return;

    /* Exit current state */
    if (sm->current != GAME_STATE_NONE && sm->states[sm->current].exit) {
        sm->states[sm->current].exit(ctx, sm->states[sm->current].userdata);
    }

    /* Update state tracking */
    sm->previous = sm->current;
    sm->current = id;

    /* Enter new state */
    if (sm->states[id].enter) {
        sm->states[id].enter(ctx, sm->states[id].userdata);
    }
}

void game_state_machine_update(GameStateMachine *sm, Carbon_GameContext *ctx,
                                float dt) {
    if (!sm || sm->current == GAME_STATE_NONE) return;

    /* Handle pending state change */
    if (sm->pending != GAME_STATE_NONE) {
        GameStateID next = sm->pending;
        sm->pending = GAME_STATE_NONE;
        game_state_machine_change(sm, next, ctx);
    }

    /* Update current state */
    if (sm->states[sm->current].update) {
        sm->states[sm->current].update(ctx, dt, sm->states[sm->current].userdata);
    }
}

void game_state_machine_render(GameStateMachine *sm, Carbon_GameContext *ctx,
                                SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass) {
    if (!sm || sm->current == GAME_STATE_NONE) return;

    if (sm->states[sm->current].render) {
        sm->states[sm->current].render(ctx, cmd, pass, sm->states[sm->current].userdata);
    }
}

GameStateID game_state_machine_current(GameStateMachine *sm) {
    return sm ? sm->current : GAME_STATE_NONE;
}

GameStateID game_state_machine_previous(GameStateMachine *sm) {
    return sm ? sm->previous : GAME_STATE_NONE;
}

void game_state_machine_back(GameStateMachine *sm, Carbon_GameContext *ctx) {
    if (!sm || sm->previous == GAME_STATE_NONE) return;
    game_state_machine_change(sm, sm->previous, ctx);
}
