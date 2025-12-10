#ifndef GAME_STATE_H
#define GAME_STATE_H

#include "agentite/game_context.h"
#include <stdbool.h>

/**
 * Game State Machine
 *
 * Simple state machine for managing game states (menu, playing, paused, etc.).
 * Each state has enter, exit, update, and render callbacks.
 *
 * Usage:
 *   GameStateMachine *sm = game_state_machine_create();
 *   game_state_machine_register(sm, STATE_MENU, &menu_state);
 *   game_state_machine_register(sm, STATE_PLAYING, &playing_state);
 *   game_state_machine_change(sm, STATE_MENU, ctx);
 *
 *   // In game loop:
 *   game_state_machine_update(sm, ctx, dt);
 *   game_state_machine_render(sm, ctx, cmd, pass);
 */

/* Maximum number of states */
#define GAME_MAX_STATES 16

/* State identifiers (customize for your game) */
typedef enum {
    GAME_STATE_NONE = 0,
    GAME_STATE_MENU,
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER,
    GAME_STATE_COUNT
} GameStateID;

/* Forward declarations */
typedef struct GameState GameState;
typedef struct GameStateMachine GameStateMachine;

/**
 * State callbacks.
 * All callbacks receive the game context and can return data via userdata.
 */
typedef void (*GameStateEnter)(Agentite_GameContext *ctx, void *userdata);
typedef void (*GameStateExit)(Agentite_GameContext *ctx, void *userdata);
typedef void (*GameStateUpdate)(Agentite_GameContext *ctx, float dt, void *userdata);
typedef void (*GameStateRender)(Agentite_GameContext *ctx,
                                 SDL_GPUCommandBuffer *cmd,
                                 SDL_GPURenderPass *pass,
                                 void *userdata);

/**
 * Game state definition.
 */
struct GameState {
    const char *name;           /* State name for debugging */
    GameStateEnter enter;       /* Called when entering state */
    GameStateExit exit;         /* Called when leaving state */
    GameStateUpdate update;     /* Called each frame */
    GameStateRender render;     /* Called each frame for rendering */
    void *userdata;             /* State-specific data */
};

/**
 * State machine.
 */
struct GameStateMachine {
    GameState states[GAME_MAX_STATES];
    GameStateID current;
    GameStateID previous;
    GameStateID pending;        /* For deferred state changes */
    bool changing;              /* Currently changing states */
};

/**
 * Create a new state machine.
 *
 * @return New state machine, or NULL on failure
 */
GameStateMachine *game_state_machine_create(void);

/**
 * Destroy a state machine.
 *
 * @param sm State machine to destroy
 */
void game_state_machine_destroy(GameStateMachine *sm);

/**
 * Register a state.
 *
 * @param sm State machine
 * @param id State identifier
 * @param state State definition (copied)
 */
void game_state_machine_register(GameStateMachine *sm, GameStateID id,
                                  const GameState *state);

/**
 * Change to a new state.
 * Calls exit on current state and enter on new state.
 *
 * @param sm State machine
 * @param id New state identifier
 * @param ctx Game context
 */
void game_state_machine_change(GameStateMachine *sm, GameStateID id,
                                Agentite_GameContext *ctx);

/**
 * Update the current state.
 *
 * @param sm State machine
 * @param ctx Game context
 * @param dt Delta time
 */
void game_state_machine_update(GameStateMachine *sm, Agentite_GameContext *ctx,
                                float dt);

/**
 * Render the current state.
 *
 * @param sm State machine
 * @param ctx Game context
 * @param cmd GPU command buffer
 * @param pass Render pass
 */
void game_state_machine_render(GameStateMachine *sm, Agentite_GameContext *ctx,
                                SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *pass);

/**
 * Get the current state ID.
 *
 * @param sm State machine
 * @return Current state ID
 */
GameStateID game_state_machine_current(GameStateMachine *sm);

/**
 * Get the previous state ID.
 *
 * @param sm State machine
 * @return Previous state ID
 */
GameStateID game_state_machine_previous(GameStateMachine *sm);

/**
 * Return to the previous state.
 *
 * @param sm State machine
 * @param ctx Game context
 */
void game_state_machine_back(GameStateMachine *sm, Agentite_GameContext *ctx);

/*============================================================================
 * Built-in State Implementations
 *============================================================================*/

/**
 * Create the menu state definition.
 * Displays a simple menu with Start, Options, Quit buttons.
 */
GameState game_state_menu_create(void);

/**
 * Create the playing state definition.
 * Main gameplay state.
 */
GameState game_state_playing_create(void);

/**
 * Create the paused state definition.
 * Pause overlay with Resume, Options, Quit buttons.
 */
GameState game_state_paused_create(void);

#endif /* GAME_STATE_H */
