/*
 * Carbon Input System
 *
 * Abstracts keyboard, mouse, and gamepad input with action mapping.
 * Instead of checking raw keys, you define named actions ("jump", "fire")
 * and bind them to one or more keys/buttons.
 */

#ifndef CARBON_INPUT_H
#define CARBON_INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/* Forward declarations */
typedef struct Carbon_Input Carbon_Input;

/* Maximum limits */
#define CARBON_INPUT_MAX_ACTIONS      64
#define CARBON_INPUT_MAX_BINDINGS     4   /* Max bindings per action */
#define CARBON_INPUT_ACTION_NAME_LEN  32

/* Input binding types */
typedef enum {
    CARBON_BINDING_NONE = 0,
    CARBON_BINDING_KEY,           /* Keyboard key */
    CARBON_BINDING_MOUSE_BUTTON,  /* Mouse button */
    CARBON_BINDING_GAMEPAD_BUTTON,/* Gamepad button */
    CARBON_BINDING_GAMEPAD_AXIS,  /* Gamepad axis (triggers, sticks) */
} Carbon_BindingType;

/* A single input binding */
typedef struct {
    Carbon_BindingType type;
    union {
        SDL_Scancode key;         /* For CARBON_BINDING_KEY */
        uint8_t mouse_button;     /* For CARBON_BINDING_MOUSE_BUTTON (1=left, 2=mid, 3=right) */
        SDL_GamepadButton gamepad_button;
        struct {
            SDL_GamepadAxis axis;
            float threshold;      /* Axis value threshold to trigger (e.g., 0.5) */
            bool positive;        /* true = positive direction, false = negative */
        } gamepad_axis;
    };
} Carbon_Binding;

/* Action state */
typedef struct {
    char name[CARBON_INPUT_ACTION_NAME_LEN];
    Carbon_Binding bindings[CARBON_INPUT_MAX_BINDINGS];
    int binding_count;
    bool pressed;      /* Currently held down */
    bool just_pressed; /* Pressed this frame */
    bool just_released;/* Released this frame */
    float value;       /* Analog value (0.0-1.0 for buttons, -1.0 to 1.0 for axes) */
} Carbon_Action;

/* Mouse state */
typedef struct {
    float x, y;           /* Current position */
    float dx, dy;         /* Delta since last frame */
    float scroll_x;       /* Horizontal scroll this frame */
    float scroll_y;       /* Vertical scroll this frame */
    bool buttons[5];      /* Button states (0=left, 1=middle, 2=right, 3=x1, 4=x2) */
    bool buttons_pressed[5];
    bool buttons_released[5];
} Carbon_MouseState;

/* Gamepad state */
typedef struct {
    SDL_Gamepad *handle;
    bool connected;
    float axes[SDL_GAMEPAD_AXIS_COUNT];
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT];
    bool buttons_pressed[SDL_GAMEPAD_BUTTON_COUNT];
    bool buttons_released[SDL_GAMEPAD_BUTTON_COUNT];
} Carbon_GamepadState;

/*
 * Initialize the input system
 * Returns NULL on failure
 */
Carbon_Input *carbon_input_init(void);

/*
 * Shutdown the input system
 */
void carbon_input_shutdown(Carbon_Input *input);

/*
 * Process an SDL event. Call this for each event in your event loop.
 * Returns true if the event was consumed by the input system.
 */
bool carbon_input_process_event(Carbon_Input *input, const SDL_Event *event);

/*
 * Update input state. Call once per frame AFTER processing all events.
 * This updates "just_pressed" and "just_released" states.
 */
void carbon_input_update(Carbon_Input *input);

/*
 * Reset per-frame state (scroll, deltas, just_pressed/released).
 * Call at the START of each frame, before processing events.
 */
void carbon_input_begin_frame(Carbon_Input *input);

/* ============ Action Management ============ */

/*
 * Register a new action. Returns the action ID, or -1 on failure.
 * Action names must be unique.
 */
int carbon_input_register_action(Carbon_Input *input, const char *name);

/*
 * Find an action by name. Returns -1 if not found.
 */
int carbon_input_find_action(Carbon_Input *input, const char *name);

/*
 * Bind a keyboard key to an action.
 */
bool carbon_input_bind_key(Carbon_Input *input, int action_id, SDL_Scancode key);

/*
 * Bind a mouse button to an action. (1=left, 2=middle, 3=right)
 */
bool carbon_input_bind_mouse(Carbon_Input *input, int action_id, uint8_t button);

/*
 * Bind a gamepad button to an action.
 */
bool carbon_input_bind_gamepad_button(Carbon_Input *input, int action_id,
                                       SDL_GamepadButton button);

/*
 * Bind a gamepad axis to an action (e.g., trigger or stick direction).
 * threshold: axis value that triggers the action (0.0-1.0)
 * positive: true for positive axis direction, false for negative
 */
bool carbon_input_bind_gamepad_axis(Carbon_Input *input, int action_id,
                                     SDL_GamepadAxis axis, float threshold, bool positive);

/*
 * Remove all bindings from an action.
 */
void carbon_input_clear_bindings(Carbon_Input *input, int action_id);

/* ============ Action Queries ============ */

/*
 * Check if an action is currently pressed (held down).
 */
bool carbon_input_action_pressed(Carbon_Input *input, int action_id);

/*
 * Check if an action was just pressed this frame.
 */
bool carbon_input_action_just_pressed(Carbon_Input *input, int action_id);

/*
 * Check if an action was just released this frame.
 */
bool carbon_input_action_just_released(Carbon_Input *input, int action_id);

/*
 * Get the analog value of an action (0.0-1.0 for buttons, -1.0 to 1.0 for axes).
 */
float carbon_input_action_value(Carbon_Input *input, int action_id);

/* Convenience functions using action names (slightly slower than ID versions) */
bool carbon_input_pressed(Carbon_Input *input, const char *action);
bool carbon_input_just_pressed(Carbon_Input *input, const char *action);
bool carbon_input_just_released(Carbon_Input *input, const char *action);
float carbon_input_value(Carbon_Input *input, const char *action);

/* ============ Direct Input Queries ============ */

/*
 * Get current mouse state.
 */
const Carbon_MouseState *carbon_input_get_mouse(Carbon_Input *input);

/*
 * Get mouse position.
 */
void carbon_input_get_mouse_position(Carbon_Input *input, float *x, float *y);

/*
 * Get mouse delta since last frame.
 */
void carbon_input_get_mouse_delta(Carbon_Input *input, float *dx, float *dy);

/*
 * Check if a mouse button is pressed. (0=left, 1=middle, 2=right)
 */
bool carbon_input_mouse_button(Carbon_Input *input, int button);
bool carbon_input_mouse_button_pressed(Carbon_Input *input, int button);
bool carbon_input_mouse_button_released(Carbon_Input *input, int button);

/*
 * Get scroll wheel delta this frame.
 */
void carbon_input_get_scroll(Carbon_Input *input, float *x, float *y);

/*
 * Check if a keyboard key is currently pressed.
 */
bool carbon_input_key_pressed(Carbon_Input *input, SDL_Scancode key);

/*
 * Check if a keyboard key was just pressed this frame.
 */
bool carbon_input_key_just_pressed(Carbon_Input *input, SDL_Scancode key);

/*
 * Check if a keyboard key was just released this frame.
 */
bool carbon_input_key_just_released(Carbon_Input *input, SDL_Scancode key);

/*
 * Get gamepad state (for first connected gamepad, or NULL if none).
 */
const Carbon_GamepadState *carbon_input_get_gamepad(Carbon_Input *input, int index);

/*
 * Get number of connected gamepads.
 */
int carbon_input_get_gamepad_count(Carbon_Input *input);

#endif /* CARBON_INPUT_H */
