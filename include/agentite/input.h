/*
 * Carbon Input System
 *
 * Abstracts keyboard, mouse, and gamepad input with action mapping.
 * Instead of checking raw keys, you define named actions ("jump", "fire")
 * and bind them to one or more keys/buttons.
 */

#ifndef AGENTITE_INPUT_H
#define AGENTITE_INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct Agentite_Input Agentite_Input;

/* Maximum limits */
#define AGENTITE_INPUT_MAX_ACTIONS      64
#define AGENTITE_INPUT_MAX_BINDINGS     4   /* Max bindings per action */
#define AGENTITE_INPUT_ACTION_NAME_LEN  32

/* Input binding types */
typedef enum {
    AGENTITE_BINDING_NONE = 0,
    AGENTITE_BINDING_KEY,           /* Keyboard key */
    AGENTITE_BINDING_MOUSE_BUTTON,  /* Mouse button */
    AGENTITE_BINDING_GAMEPAD_BUTTON,/* Gamepad button */
    AGENTITE_BINDING_GAMEPAD_AXIS,  /* Gamepad axis (triggers, sticks) */
} Agentite_BindingType;

/* A single input binding */
typedef struct {
    Agentite_BindingType type;
    union {
        SDL_Scancode key;         /* For AGENTITE_BINDING_KEY */
        uint8_t mouse_button;     /* For AGENTITE_BINDING_MOUSE_BUTTON (1=left, 2=mid, 3=right) */
        SDL_GamepadButton gamepad_button;
        struct {
            SDL_GamepadAxis axis;
            float threshold;      /* Axis value threshold to trigger (e.g., 0.5) */
            bool positive;        /* true = positive direction, false = negative */
        } gamepad_axis;
    };
} Agentite_Binding;

/* Action state */
typedef struct {
    char name[AGENTITE_INPUT_ACTION_NAME_LEN];
    Agentite_Binding bindings[AGENTITE_INPUT_MAX_BINDINGS];
    int binding_count;
    bool pressed;      /* Currently held down */
    bool just_pressed; /* Pressed this frame */
    bool just_released;/* Released this frame */
    float value;       /* Analog value (0.0-1.0 for buttons, -1.0 to 1.0 for axes) */
} Agentite_Action;

/* Mouse state */
typedef struct {
    float x, y;           /* Current position */
    float dx, dy;         /* Delta since last frame */
    float scroll_x;       /* Horizontal scroll this frame */
    float scroll_y;       /* Vertical scroll this frame */
    bool buttons[5];      /* Button states (0=left, 1=middle, 2=right, 3=x1, 4=x2) */
    bool buttons_pressed[5];
    bool buttons_released[5];
} Agentite_MouseState;

/* Gamepad state */
typedef struct {
    SDL_Gamepad *handle;
    bool connected;
    float axes[SDL_GAMEPAD_AXIS_COUNT];
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
    bool buttons_pressed[SDL_GAMEPAD_BUTTON_COUNT];
    bool buttons_released[SDL_GAMEPAD_BUTTON_COUNT];
} Agentite_GamepadState;

/*
 * Initialize the input system
 * Returns NULL on failure
 */
Agentite_Input *agentite_input_init(void);

/*
 * Shutdown the input system
 */
void agentite_input_shutdown(Agentite_Input *input);

/*
 * Process an SDL event. Call this for each event in your event loop.
 * Returns true if the event was consumed by the input system.
 */
bool agentite_input_process_event(Agentite_Input *input, const SDL_Event *event);

/*
 * Update input state. Call once per frame AFTER processing all events.
 * This updates "just_pressed" and "just_released" states.
 */
void agentite_input_update(Agentite_Input *input);

/*
 * Reset per-frame state (scroll, deltas, just_pressed/released).
 * Call at the START of each frame, before processing events.
 */
void agentite_input_begin_frame(Agentite_Input *input);

/* ============ Action Management ============ */

/*
 * Register a new action. Returns the action ID, or -1 on failure.
 * Action names must be unique.
 */
int agentite_input_register_action(Agentite_Input *input, const char *name);

/*
 * Find an action by name. Returns -1 if not found.
 */
int agentite_input_find_action(Agentite_Input *input, const char *name);

/*
 * Bind a keyboard key to an action.
 */
bool agentite_input_bind_key(Agentite_Input *input, int action_id, SDL_Scancode key);

/*
 * Bind a mouse button to an action. (1=left, 2=middle, 3=right)
 */
bool agentite_input_bind_mouse(Agentite_Input *input, int action_id, uint8_t button);

/*
 * Bind a gamepad button to an action.
 */
bool agentite_input_bind_gamepad_button(Agentite_Input *input, int action_id,
                                       SDL_GamepadButton button);

/*
 * Bind a gamepad axis to an action (e.g., trigger or stick direction).
 * threshold: axis value that triggers the action (0.0-1.0)
 * positive: true for positive axis direction, false for negative
 */
bool agentite_input_bind_gamepad_axis(Agentite_Input *input, int action_id,
                                     SDL_GamepadAxis axis, float threshold, bool positive);

/*
 * Remove all bindings from an action.
 */
void agentite_input_clear_bindings(Agentite_Input *input, int action_id);

/* ============ Action Queries ============ */

/*
 * Check if an action is currently pressed (held down).
 */
bool agentite_input_action_pressed(Agentite_Input *input, int action_id);

/*
 * Check if an action was just pressed this frame.
 */
bool agentite_input_action_just_pressed(Agentite_Input *input, int action_id);

/*
 * Check if an action was just released this frame.
 */
bool agentite_input_action_just_released(Agentite_Input *input, int action_id);

/*
 * Get the analog value of an action (0.0-1.0 for buttons, -1.0 to 1.0 for axes).
 */
float agentite_input_action_value(Agentite_Input *input, int action_id);

/* Convenience functions using action names (slightly slower than ID versions) */
bool agentite_input_pressed(Agentite_Input *input, const char *action);
bool agentite_input_just_pressed(Agentite_Input *input, const char *action);
bool agentite_input_just_released(Agentite_Input *input, const char *action);
float agentite_input_value(Agentite_Input *input, const char *action);

/* ============ Direct Input Queries ============ */

/*
 * Get current mouse state.
 */
const Agentite_MouseState *agentite_input_get_mouse(Agentite_Input *input);

/*
 * Get mouse position.
 */
void agentite_input_get_mouse_position(Agentite_Input *input, float *x, float *y);

/*
 * Get mouse delta since last frame.
 */
void agentite_input_get_mouse_delta(Agentite_Input *input, float *dx, float *dy);

/*
 * Check if a mouse button is pressed. (0=left, 1=middle, 2=right)
 */
bool agentite_input_mouse_button(Agentite_Input *input, int button);
bool agentite_input_mouse_button_pressed(Agentite_Input *input, int button);
bool agentite_input_mouse_button_released(Agentite_Input *input, int button);

/*
 * Get scroll wheel delta this frame.
 */
void agentite_input_get_scroll(Agentite_Input *input, float *x, float *y);

/*
 * Check if a keyboard key is currently pressed.
 */
bool agentite_input_key_pressed(Agentite_Input *input, SDL_Scancode key);

/*
 * Check if a keyboard key was just pressed this frame.
 */
bool agentite_input_key_just_pressed(Agentite_Input *input, SDL_Scancode key);

/*
 * Check if a keyboard key was just released this frame.
 */
bool agentite_input_key_just_released(Agentite_Input *input, SDL_Scancode key);

/*
 * Get gamepad state (for first connected gamepad, or NULL if none).
 */
const Agentite_GamepadState *agentite_input_get_gamepad(Agentite_Input *input, int index);

/*
 * Get number of connected gamepads.
 */
int agentite_input_get_gamepad_count(Agentite_Input *input);

#endif /* AGENTITE_INPUT_H */
