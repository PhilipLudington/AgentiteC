/**
 * @file input.h
 * @brief Input system with action mapping for keyboard, mouse, and gamepad.
 *
 * This module provides an abstraction layer over raw input devices. Instead of
 * checking specific keys directly, you define named actions ("jump", "fire")
 * and bind them to one or more keys/buttons. This allows:
 * - Easy key rebinding by players
 * - Multiple input devices for the same action
 * - Clean separation between game logic and input handling
 *
 * @section input_usage Basic Usage
 * @code
 * Agentite_Input *input = agentite_input_init();
 *
 * // Define actions
 * int jump_action = agentite_input_register_action(input, "jump");
 * agentite_input_bind_key(input, jump_action, SDL_SCANCODE_SPACE);
 * agentite_input_bind_gamepad_button(input, jump_action, SDL_GAMEPAD_BUTTON_SOUTH);
 *
 * // In game loop:
 * while (running) {
 *     agentite_input_begin_frame(input);
 *
 *     SDL_Event event;
 *     while (SDL_PollEvent(&event)) {
 *         agentite_input_process_event(input, &event);
 *     }
 *
 *     agentite_input_update(input);
 *
 *     if (agentite_input_action_just_pressed(input, jump_action)) {
 *         player_jump();
 *     }
 * }
 *
 * agentite_input_shutdown(input);
 * @endcode
 *
 * @section input_frame_timing Frame Timing
 * The correct order of calls each frame is:
 * 1. agentite_input_begin_frame() - Clear per-frame state
 * 2. SDL_PollEvent() + agentite_input_process_event() - Process all events
 * 3. agentite_input_update() - Finalize frame state
 * 4. Query actions/input state
 *
 * @section input_thread_safety Thread Safety
 * All functions in this module are NOT thread-safe and must be called from
 * the main thread only. See AGENTITE_ASSERT_MAIN_THREAD() assertions.
 */

#ifndef AGENTITE_INPUT_H
#define AGENTITE_INPUT_H

#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Opaque input system handle.
 *
 * Manages all input state including actions, mouse, keyboard, and gamepads.
 * Created via agentite_input_init(), destroyed via agentite_input_shutdown().
 */
typedef struct Agentite_Input Agentite_Input;

/** @defgroup input_constants Constants
 *  @{ */

/** @brief Maximum number of actions that can be registered */
#define AGENTITE_INPUT_MAX_ACTIONS      64

/** @brief Maximum number of bindings per action */
#define AGENTITE_INPUT_MAX_BINDINGS     4

/** @brief Maximum length of action name (including null terminator) */
#define AGENTITE_INPUT_ACTION_NAME_LEN  32

/** @} */

/** @defgroup input_types Types
 *  @{ */

/**
 * @brief Type of input binding.
 *
 * Specifies what type of input device/control is bound to an action.
 */
typedef enum {
    AGENTITE_BINDING_NONE = 0,        /**< No binding (unused slot) */
    AGENTITE_BINDING_KEY,             /**< Keyboard key (SDL_Scancode) */
    AGENTITE_BINDING_MOUSE_BUTTON,    /**< Mouse button (1=left, 2=mid, 3=right) */
    AGENTITE_BINDING_GAMEPAD_BUTTON,  /**< Gamepad button (SDL_GamepadButton) */
    AGENTITE_BINDING_GAMEPAD_AXIS,    /**< Gamepad axis (triggers, sticks) */
} Agentite_BindingType;

/**
 * @brief A single input binding.
 *
 * Represents one key/button/axis bound to an action. Each action can have
 * up to AGENTITE_INPUT_MAX_BINDINGS bindings.
 */
typedef struct {
    Agentite_BindingType type;  /**< Type of binding */
    union {
        SDL_Scancode key;               /**< Keyboard scancode (for BINDING_KEY) */
        uint8_t mouse_button;           /**< Mouse button index (for BINDING_MOUSE_BUTTON) */
        SDL_GamepadButton gamepad_button; /**< Gamepad button (for BINDING_GAMEPAD_BUTTON) */
        struct {
            SDL_GamepadAxis axis;       /**< Gamepad axis identifier */
            float threshold;            /**< Value threshold to trigger (0.0-1.0) */
            bool positive;              /**< true = positive direction, false = negative */
        } gamepad_axis;                 /**< Gamepad axis configuration (for BINDING_GAMEPAD_AXIS) */
    };
} Agentite_Binding;

/**
 * @brief Action state and configuration.
 *
 * Contains an action's name, bindings, and current input state.
 * Updated each frame by agentite_input_update().
 */
typedef struct {
    char name[AGENTITE_INPUT_ACTION_NAME_LEN];  /**< Action name (e.g., "jump") */
    Agentite_Binding bindings[AGENTITE_INPUT_MAX_BINDINGS]; /**< Bound inputs */
    int binding_count;    /**< Number of active bindings */
    bool pressed;         /**< Currently held down */
    bool just_pressed;    /**< Pressed this frame */
    bool just_released;   /**< Released this frame */
    float value;          /**< Analog value (0-1 for buttons, -1 to 1 for axes) */
} Agentite_Action;

/**
 * @brief Mouse input state.
 *
 * Contains current mouse position, movement delta, scroll, and button states.
 * Retrieved via agentite_input_get_mouse().
 */
typedef struct {
    float x, y;           /**< Current cursor position in screen pixels */
    float dx, dy;         /**< Movement delta since last frame */
    float scroll_x;       /**< Horizontal scroll this frame */
    float scroll_y;       /**< Vertical scroll this frame */
    bool buttons[5];      /**< Button states (0=left, 1=middle, 2=right, 3=x1, 4=x2) */
    bool buttons_pressed[5];  /**< Buttons pressed this frame */
    bool buttons_released[5]; /**< Buttons released this frame */
} Agentite_MouseState;

/**
 * @brief Gamepad input state.
 *
 * Contains axis values and button states for a connected gamepad.
 * Retrieved via agentite_input_get_gamepad().
 */
typedef struct {
    SDL_Gamepad *handle;  /**< SDL gamepad handle */
    bool connected;       /**< true if gamepad is connected */
    float axes[SDL_GAMEPAD_AXIS_COUNT];     /**< Axis values (-1.0 to 1.0) */
    bool buttons[SDL_GAMEPAD_BUTTON_COUNT]; /**< Button states */
    bool buttons_pressed[SDL_GAMEPAD_BUTTON_COUNT];  /**< Buttons pressed this frame */
    bool buttons_released[SDL_GAMEPAD_BUTTON_COUNT]; /**< Buttons released this frame */
} Agentite_GamepadState;

/** @} */ /* end of input_types */

/** @defgroup input_lifecycle Lifecycle Functions
 *  @{ */

/**
 * @brief Initialize the input system.
 *
 * Creates and initializes the input system with default state.
 * Initializes gamepad support via SDL.
 *
 * @return Input system handle on success, NULL on failure
 *
 * @ownership Caller OWNS the returned pointer and MUST call agentite_input_shutdown().
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
Agentite_Input *agentite_input_init(void);

/**
 * @brief Shutdown the input system.
 *
 * Releases all resources including gamepad handles.
 *
 * @param input Input system to shutdown (NULL is safely ignored)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_input_shutdown(Agentite_Input *input);

/**
 * @brief Process an SDL event.
 *
 * Updates internal input state based on the event. Call this for each
 * event in your SDL event loop.
 *
 * @param input Input system (must not be NULL)
 * @param event SDL event to process (must not be NULL)
 *
 * @return true if the event was consumed by the input system
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
bool agentite_input_process_event(Agentite_Input *input, const SDL_Event *event);

/**
 * @brief Update input state at end of frame.
 *
 * Finalizes input state for the frame, updating "just_pressed" and
 * "just_released" states for all actions. Call once per frame AFTER
 * processing all SDL events.
 *
 * @param input Input system (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_input_update(Agentite_Input *input);

/**
 * @brief Reset per-frame state at start of frame.
 *
 * Clears scroll deltas, mouse deltas, and just_pressed/released states.
 * Call at the START of each frame, BEFORE processing events.
 *
 * @param input Input system (must not be NULL)
 *
 * @note NOT thread-safe. Must be called from main thread.
 */
void agentite_input_begin_frame(Agentite_Input *input);

/** @} */ /* end of input_lifecycle */

/** @defgroup input_actions Action Management
 *
 * Actions are named input bindings that abstract away specific keys/buttons.
 * Register actions once at startup, then query them during gameplay.
 *
 * @{
 */

/**
 * @brief Register a new action.
 *
 * Creates a new named action that can have keys/buttons bound to it.
 * Action names must be unique and not exceed AGENTITE_INPUT_ACTION_NAME_LEN.
 *
 * @param input Input system (must not be NULL)
 * @param name  Action name (must not be NULL, must be unique)
 *
 * @return Action ID on success (>= 0), -1 on failure
 *
 * @code
 * int jump_id = agentite_input_register_action(input, "jump");
 * agentite_input_bind_key(input, jump_id, SDL_SCANCODE_SPACE);
 * @endcode
 */
int agentite_input_register_action(Agentite_Input *input, const char *name);

/**
 * @brief Find an action by name.
 *
 * Looks up an action ID by its name. Useful for config file loading
 * or when action IDs aren't stored.
 *
 * @param input Input system (must not be NULL)
 * @param name  Action name to find (must not be NULL)
 *
 * @return Action ID if found (>= 0), -1 if not found
 */
int agentite_input_find_action(Agentite_Input *input, const char *name);

/**
 * @brief Bind a keyboard key to an action.
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID from agentite_input_register_action()
 * @param key       SDL scancode to bind
 *
 * @return true on success, false if action_id invalid or bindings full
 */
bool agentite_input_bind_key(Agentite_Input *input, int action_id, SDL_Scancode key);

/**
 * @brief Bind a mouse button to an action.
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID from agentite_input_register_action()
 * @param button    Mouse button (1=left, 2=middle, 3=right, 4=x1, 5=x2)
 *
 * @return true on success, false if action_id invalid or bindings full
 */
bool agentite_input_bind_mouse(Agentite_Input *input, int action_id, uint8_t button);

/**
 * @brief Bind a gamepad button to an action.
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID from agentite_input_register_action()
 * @param button    SDL gamepad button to bind
 *
 * @return true on success, false if action_id invalid or bindings full
 */
bool agentite_input_bind_gamepad_button(Agentite_Input *input, int action_id,
                                        SDL_GamepadButton button);

/**
 * @brief Bind a gamepad axis to an action.
 *
 * Binds an axis direction (e.g., trigger, stick direction) to trigger
 * an action when the axis exceeds a threshold.
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID from agentite_input_register_action()
 * @param axis      SDL gamepad axis to bind
 * @param threshold Axis value threshold to trigger (0.0-1.0)
 * @param positive  true for positive direction, false for negative
 *
 * @return true on success, false if action_id invalid or bindings full
 *
 * @code
 * // Bind right trigger
 * agentite_input_bind_gamepad_axis(input, fire_id,
 *     SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, 0.5f, true);
 *
 * // Bind left stick right
 * agentite_input_bind_gamepad_axis(input, move_right_id,
 *     SDL_GAMEPAD_AXIS_LEFTX, 0.3f, true);
 * @endcode
 */
bool agentite_input_bind_gamepad_axis(Agentite_Input *input, int action_id,
                                      SDL_GamepadAxis axis, float threshold, bool positive);

/**
 * @brief Remove all bindings from an action.
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID to clear bindings from
 */
void agentite_input_clear_bindings(Agentite_Input *input, int action_id);

/** @} */ /* end of input_actions */

/** @defgroup input_action_queries Action Queries
 *
 * Query the current state of registered actions.
 * These functions use action IDs for best performance.
 *
 * @{
 */

/**
 * @brief Check if an action is currently pressed (held down).
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID to check
 *
 * @return true if any binding for the action is currently held down
 */
bool agentite_input_action_pressed(Agentite_Input *input, int action_id);

/**
 * @brief Check if an action was just pressed this frame.
 *
 * Returns true only on the first frame the action becomes pressed.
 * Use for actions that should trigger once (jump, fire, etc.).
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID to check
 *
 * @return true if action was pressed this frame
 */
bool agentite_input_action_just_pressed(Agentite_Input *input, int action_id);

/**
 * @brief Check if an action was just released this frame.
 *
 * Returns true only on the frame the action becomes released.
 * Use for actions that trigger on release (charge attacks, etc.).
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID to check
 *
 * @return true if action was released this frame
 */
bool agentite_input_action_just_released(Agentite_Input *input, int action_id);

/**
 * @brief Get the analog value of an action.
 *
 * For button bindings, returns 1.0 when pressed, 0.0 when released.
 * For axis bindings, returns the normalized axis value (-1.0 to 1.0).
 *
 * @param input     Input system (must not be NULL)
 * @param action_id Action ID to check
 *
 * @return Analog value of the action
 */
float agentite_input_action_value(Agentite_Input *input, int action_id);

/** @name Convenience Functions (by name)
 *
 * These functions look up actions by name. Slightly slower than ID versions
 * due to string comparison, but convenient for occasional use.
 *
 * @{
 */

/** @brief Check if action is pressed (by name) */
bool agentite_input_pressed(Agentite_Input *input, const char *action);

/** @brief Check if action was just pressed (by name) */
bool agentite_input_just_pressed(Agentite_Input *input, const char *action);

/** @brief Check if action was just released (by name) */
bool agentite_input_just_released(Agentite_Input *input, const char *action);

/** @brief Get action analog value (by name) */
float agentite_input_value(Agentite_Input *input, const char *action);

/** @} */

/** @} */ /* end of input_action_queries */

/** @defgroup input_direct Direct Input Queries
 *
 * Query raw input state directly (keyboard, mouse, gamepad) without
 * using the action system. Useful for UI, editors, or when specific
 * keys are required.
 *
 * @{
 */

/** @name Mouse Input
 *  @{ */

/**
 * @brief Get complete mouse state.
 *
 * @param input Input system (must not be NULL)
 *
 * @return Pointer to mouse state (borrowed, valid until next frame)
 */
const Agentite_MouseState *agentite_input_get_mouse(Agentite_Input *input);

/**
 * @brief Get current mouse position.
 *
 * @param input Input system (must not be NULL)
 * @param x     Output for X position (may be NULL if not needed)
 * @param y     Output for Y position (may be NULL if not needed)
 */
void agentite_input_get_mouse_position(Agentite_Input *input, float *x, float *y);

/**
 * @brief Get mouse movement delta since last frame.
 *
 * @param input Input system (must not be NULL)
 * @param dx    Output for X delta (may be NULL if not needed)
 * @param dy    Output for Y delta (may be NULL if not needed)
 */
void agentite_input_get_mouse_delta(Agentite_Input *input, float *dx, float *dy);

/**
 * @brief Check if a mouse button is currently held.
 *
 * @param input  Input system (must not be NULL)
 * @param button Button index (0=left, 1=middle, 2=right, 3=x1, 4=x2)
 *
 * @return true if button is held down
 */
bool agentite_input_mouse_button(Agentite_Input *input, int button);

/** @brief Check if mouse button was just pressed this frame */
bool agentite_input_mouse_button_pressed(Agentite_Input *input, int button);

/** @brief Check if mouse button was just released this frame */
bool agentite_input_mouse_button_released(Agentite_Input *input, int button);

/**
 * @brief Get scroll wheel delta this frame.
 *
 * @param input Input system (must not be NULL)
 * @param x     Output for horizontal scroll (may be NULL)
 * @param y     Output for vertical scroll (may be NULL)
 */
void agentite_input_get_scroll(Agentite_Input *input, float *x, float *y);

/** @} */ /* end of Mouse Input */

/** @name Keyboard Input
 *  @{ */

/**
 * @brief Check if a keyboard key is currently held.
 *
 * @param input Input system (must not be NULL)
 * @param key   SDL scancode to check
 *
 * @return true if key is held down
 */
bool agentite_input_key_pressed(Agentite_Input *input, SDL_Scancode key);

/**
 * @brief Check if a keyboard key was just pressed this frame.
 *
 * @param input Input system (must not be NULL)
 * @param key   SDL scancode to check
 *
 * @return true if key was pressed this frame
 */
bool agentite_input_key_just_pressed(Agentite_Input *input, SDL_Scancode key);

/**
 * @brief Check if a keyboard key was just released this frame.
 *
 * @param input Input system (must not be NULL)
 * @param key   SDL scancode to check
 *
 * @return true if key was released this frame
 */
bool agentite_input_key_just_released(Agentite_Input *input, SDL_Scancode key);

/** @} */ /* end of Keyboard Input */

/** @name Gamepad Input
 *  @{ */

/**
 * @brief Get gamepad state by index.
 *
 * @param input Input system (must not be NULL)
 * @param index Gamepad index (0 = first gamepad)
 *
 * @return Pointer to gamepad state, or NULL if index out of range or disconnected
 */
const Agentite_GamepadState *agentite_input_get_gamepad(Agentite_Input *input, int index);

/**
 * @brief Get number of connected gamepads.
 *
 * @param input Input system (must not be NULL)
 *
 * @return Number of connected gamepads (0 if none)
 */
int agentite_input_get_gamepad_count(Agentite_Input *input);

/** @} */ /* end of Gamepad Input */

/** @} */ /* end of input_direct */

/** @defgroup input_debug Event Debugging
 *
 * Debug utilities for logging and describing input events.
 * Requires SDL 3.4.0+ for some features.
 *
 * @{
 */

/**
 * @brief Enable or disable event logging.
 *
 * When enabled, all processed events are logged with their descriptions.
 * Useful for debugging input issues.
 *
 * @param input   Input system (must not be NULL)
 * @param enabled true to enable logging, false to disable
 */
void agentite_input_set_event_logging(Agentite_Input *input, bool enabled);

/**
 * @brief Check if event logging is enabled.
 *
 * @param input Input system (must not be NULL)
 *
 * @return true if event logging is enabled
 */
bool agentite_input_get_event_logging(Agentite_Input *input);

/**
 * @brief Get a human-readable description of an SDL event.
 *
 * Uses SDL_GetEventDescription() (SDL 3.4.0+) to generate a description.
 *
 * @param event  Event to describe (must not be NULL)
 * @param buf    Output buffer (must not be NULL)
 * @param buflen Buffer size in bytes
 *
 * @return Number of characters written (not including null terminator)
 */
int agentite_input_describe_event(const SDL_Event *event, char *buf, int buflen);

/** @} */ /* end of input_debug */

#endif /* AGENTITE_INPUT_H */
