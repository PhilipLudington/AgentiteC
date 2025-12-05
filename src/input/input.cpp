/*
 * Carbon Input System Implementation
 */

#include "carbon/carbon.h"
#include "carbon/input.h"
#include <stdlib.h>
#include <string.h>

#define MAX_GAMEPADS 4
#define MAX_KEYS 512

/* Internal input state */
struct Carbon_Input {
    /* Actions */
    Carbon_Action actions[CARBON_INPUT_MAX_ACTIONS];
    int action_count;

    /* Keyboard state */
    bool keys[MAX_KEYS];
    bool keys_prev[MAX_KEYS];

    /* Mouse state */
    Carbon_MouseState mouse;
    float mouse_prev_x, mouse_prev_y;

    /* Gamepad state */
    Carbon_GamepadState gamepads[MAX_GAMEPADS];
    int gamepad_count;
};

/* Helper: clamp value to range */
static float clampf(float val, float min, float max) {
    if (val < min) return min;
    if (val > max) return max;
    return val;
}

Carbon_Input *carbon_input_init(void) {
    Carbon_Input *input = CARBON_ALLOC(Carbon_Input);
    if (!input) return NULL;

    /* Initialize SDL gamepad subsystem if not already */
    if (!SDL_WasInit(SDL_INIT_GAMEPAD)) {
        if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            SDL_Log("Warning: Failed to init gamepad subsystem: %s", SDL_GetError());
            /* Continue anyway, gamepad just won't work */
        }
    }

    /* Open any already-connected gamepads */
    int num_joysticks;
    SDL_JoystickID *joysticks = SDL_GetJoysticks(&num_joysticks);
    if (joysticks) {
        for (int i = 0; i < num_joysticks && input->gamepad_count < MAX_GAMEPADS; i++) {
            if (SDL_IsGamepad(joysticks[i])) {
                SDL_Gamepad *pad = SDL_OpenGamepad(joysticks[i]);
                if (pad) {
                    input->gamepads[input->gamepad_count].handle = pad;
                    input->gamepads[input->gamepad_count].connected = true;
                    input->gamepad_count++;
                    SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(pad));
                }
            }
        }
        SDL_free(joysticks);
    }

    /* Get initial mouse position */
    SDL_GetMouseState(&input->mouse.x, &input->mouse.y);
    input->mouse_prev_x = input->mouse.x;
    input->mouse_prev_y = input->mouse.y;

    return input;
}

void carbon_input_shutdown(Carbon_Input *input) {
    if (!input) return;

    /* Close all gamepads */
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        if (input->gamepads[i].handle) {
            SDL_CloseGamepad(input->gamepads[i].handle);
        }
    }

    free(input);
}

void carbon_input_begin_frame(Carbon_Input *input) {
    if (!input) return;

    /* Store previous mouse position for delta calculation */
    input->mouse_prev_x = input->mouse.x;
    input->mouse_prev_y = input->mouse.y;

    /* Reset per-frame mouse state */
    input->mouse.dx = 0;
    input->mouse.dy = 0;
    input->mouse.scroll_x = 0;
    input->mouse.scroll_y = 0;
    memset(input->mouse.buttons_pressed, 0, sizeof(input->mouse.buttons_pressed));
    memset(input->mouse.buttons_released, 0, sizeof(input->mouse.buttons_released));

    /* Store previous keyboard state */
    memcpy(input->keys_prev, input->keys, sizeof(input->keys));

    /* Reset per-frame gamepad state */
    for (int i = 0; i < MAX_GAMEPADS; i++) {
        memset(input->gamepads[i].buttons_pressed, 0,
               sizeof(input->gamepads[i].buttons_pressed));
        memset(input->gamepads[i].buttons_released, 0,
               sizeof(input->gamepads[i].buttons_released));
    }

    /* Reset per-frame action state */
    for (int i = 0; i < input->action_count; i++) {
        input->actions[i].just_pressed = false;
        input->actions[i].just_released = false;
    }
}

bool carbon_input_process_event(Carbon_Input *input, const SDL_Event *event) {
    if (!input || !event) return false;

    switch (event->type) {
    case SDL_EVENT_KEY_DOWN:
        if (event->key.scancode < MAX_KEYS) {
            input->keys[event->key.scancode] = true;
        }
        return true;

    case SDL_EVENT_KEY_UP:
        if (event->key.scancode < MAX_KEYS) {
            input->keys[event->key.scancode] = false;
        }
        return true;

    case SDL_EVENT_MOUSE_MOTION:
        input->mouse.x = event->motion.x;
        input->mouse.y = event->motion.y;
        input->mouse.dx += event->motion.xrel;
        input->mouse.dy += event->motion.yrel;
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        if (event->button.button <= 5) {
            int idx = event->button.button - 1;
            input->mouse.buttons[idx] = true;
            input->mouse.buttons_pressed[idx] = true;
        }
        return true;

    case SDL_EVENT_MOUSE_BUTTON_UP:
        if (event->button.button <= 5) {
            int idx = event->button.button - 1;
            input->mouse.buttons[idx] = false;
            input->mouse.buttons_released[idx] = true;
        }
        return true;

    case SDL_EVENT_MOUSE_WHEEL:
        input->mouse.scroll_x += event->wheel.x;
        input->mouse.scroll_y += event->wheel.y;
        return true;

    case SDL_EVENT_GAMEPAD_ADDED: {
        /* New gamepad connected */
        if (input->gamepad_count < MAX_GAMEPADS) {
            SDL_Gamepad *pad = SDL_OpenGamepad(event->gdevice.which);
            if (pad) {
                /* Find empty slot */
                for (int i = 0; i < MAX_GAMEPADS; i++) {
                    if (!input->gamepads[i].connected) {
                        input->gamepads[i].handle = pad;
                        input->gamepads[i].connected = true;
                        memset(input->gamepads[i].axes, 0, sizeof(input->gamepads[i].axes));
                        memset(input->gamepads[i].buttons, 0, sizeof(input->gamepads[i].buttons));
                        input->gamepad_count++;
                        SDL_Log("Gamepad connected: %s", SDL_GetGamepadName(pad));
                        break;
                    }
                }
            }
        }
        return true;
    }

    case SDL_EVENT_GAMEPAD_REMOVED: {
        /* Gamepad disconnected */
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (input->gamepads[i].handle &&
                SDL_GetGamepadID(input->gamepads[i].handle) == event->gdevice.which) {
                SDL_Log("Gamepad disconnected");
                SDL_CloseGamepad(input->gamepads[i].handle);
                input->gamepads[i].handle = NULL;
                input->gamepads[i].connected = false;
                input->gamepad_count--;
                break;
            }
        }
        return true;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (input->gamepads[i].handle &&
                SDL_GetGamepadID(input->gamepads[i].handle) == event->gbutton.which) {
                int btn = event->gbutton.button;
                if (btn < SDL_GAMEPAD_BUTTON_COUNT) {
                    input->gamepads[i].buttons[btn] = true;
                    input->gamepads[i].buttons_pressed[btn] = true;
                }
                break;
            }
        }
        return true;
    }

    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (input->gamepads[i].handle &&
                SDL_GetGamepadID(input->gamepads[i].handle) == event->gbutton.which) {
                int btn = event->gbutton.button;
                if (btn < SDL_GAMEPAD_BUTTON_COUNT) {
                    input->gamepads[i].buttons[btn] = false;
                    input->gamepads[i].buttons_released[btn] = true;
                }
                break;
            }
        }
        return true;
    }

    case SDL_EVENT_GAMEPAD_AXIS_MOTION: {
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (input->gamepads[i].handle &&
                SDL_GetGamepadID(input->gamepads[i].handle) == event->gaxis.which) {
                int axis = event->gaxis.axis;
                if (axis < SDL_GAMEPAD_AXIS_COUNT) {
                    /* Normalize to -1.0 to 1.0 */
                    input->gamepads[i].axes[axis] = event->gaxis.value / 32767.0f;
                }
                break;
            }
        }
        return true;
    }

    default:
        return false;
    }
}

/* Helper: check if a binding is currently active */
static bool binding_active(Carbon_Input *input, const Carbon_Binding *binding, float *value) {
    switch (binding->type) {
    case CARBON_BINDING_KEY:
        if (binding->key < MAX_KEYS && input->keys[binding->key]) {
            *value = 1.0f;
            return true;
        }
        break;

    case CARBON_BINDING_MOUSE_BUTTON:
        if (binding->mouse_button >= 1 && binding->mouse_button <= 5) {
            if (input->mouse.buttons[binding->mouse_button - 1]) {
                *value = 1.0f;
                return true;
            }
        }
        break;

    case CARBON_BINDING_GAMEPAD_BUTTON:
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (input->gamepads[i].connected &&
                input->gamepads[i].buttons[binding->gamepad_button]) {
                *value = 1.0f;
                return true;
            }
        }
        break;

    case CARBON_BINDING_GAMEPAD_AXIS:
        for (int i = 0; i < MAX_GAMEPADS; i++) {
            if (input->gamepads[i].connected) {
                float axis_val = input->gamepads[i].axes[binding->gamepad_axis.axis];
                if (binding->gamepad_axis.positive) {
                    if (axis_val >= binding->gamepad_axis.threshold) {
                        *value = axis_val;
                        return true;
                    }
                } else {
                    if (axis_val <= -binding->gamepad_axis.threshold) {
                        *value = -axis_val;
                        return true;
                    }
                }
            }
        }
        break;

    default:
        break;
    }

    *value = 0.0f;
    return false;
}

void carbon_input_update(Carbon_Input *input) {
    if (!input) return;

    /* Update action states based on bindings */
    for (int i = 0; i < input->action_count; i++) {
        Carbon_Action *action = &input->actions[i];
        bool was_pressed = action->pressed;
        bool is_pressed = false;
        float max_value = 0.0f;

        /* Check all bindings for this action */
        for (int b = 0; b < action->binding_count; b++) {
            float val;
            if (binding_active(input, &action->bindings[b], &val)) {
                is_pressed = true;
                if (val > max_value) max_value = val;
            }
        }

        action->pressed = is_pressed;
        action->value = max_value;
        action->just_pressed = is_pressed && !was_pressed;
        action->just_released = !is_pressed && was_pressed;
    }
}

/* ============ Action Management ============ */

int carbon_input_register_action(Carbon_Input *input, const char *name) {
    if (!input || !name) return -1;
    if (input->action_count >= CARBON_INPUT_MAX_ACTIONS) return -1;

    /* Check for duplicate */
    for (int i = 0; i < input->action_count; i++) {
        if (strcmp(input->actions[i].name, name) == 0) {
            return i; /* Already exists, return existing ID */
        }
    }

    /* Create new action */
    int id = input->action_count++;
    Carbon_Action *action = &input->actions[id];
    strncpy(action->name, name, CARBON_INPUT_ACTION_NAME_LEN - 1);
    action->name[CARBON_INPUT_ACTION_NAME_LEN - 1] = '\0';
    action->binding_count = 0;
    action->pressed = false;
    action->just_pressed = false;
    action->just_released = false;
    action->value = 0.0f;

    return id;
}

int carbon_input_find_action(Carbon_Input *input, const char *name) {
    if (!input || !name) return -1;

    for (int i = 0; i < input->action_count; i++) {
        if (strcmp(input->actions[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

bool carbon_input_bind_key(Carbon_Input *input, int action_id, SDL_Scancode key) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;

    Carbon_Action *action = &input->actions[action_id];
    if (action->binding_count >= CARBON_INPUT_MAX_BINDINGS) return false;

    Carbon_Binding *binding = &action->bindings[action->binding_count++];
    binding->type = CARBON_BINDING_KEY;
    binding->key = key;
    return true;
}

bool carbon_input_bind_mouse(Carbon_Input *input, int action_id, uint8_t button) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;
    if (button < 1 || button > 5) return false;

    Carbon_Action *action = &input->actions[action_id];
    if (action->binding_count >= CARBON_INPUT_MAX_BINDINGS) return false;

    Carbon_Binding *binding = &action->bindings[action->binding_count++];
    binding->type = CARBON_BINDING_MOUSE_BUTTON;
    binding->mouse_button = button;
    return true;
}

bool carbon_input_bind_gamepad_button(Carbon_Input *input, int action_id,
                                       SDL_GamepadButton button) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;

    Carbon_Action *action = &input->actions[action_id];
    if (action->binding_count >= CARBON_INPUT_MAX_BINDINGS) return false;

    Carbon_Binding *binding = &action->bindings[action->binding_count++];
    binding->type = CARBON_BINDING_GAMEPAD_BUTTON;
    binding->gamepad_button = button;
    return true;
}

bool carbon_input_bind_gamepad_axis(Carbon_Input *input, int action_id,
                                     SDL_GamepadAxis axis, float threshold, bool positive) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;

    Carbon_Action *action = &input->actions[action_id];
    if (action->binding_count >= CARBON_INPUT_MAX_BINDINGS) return false;

    Carbon_Binding *binding = &action->bindings[action->binding_count++];
    binding->type = CARBON_BINDING_GAMEPAD_AXIS;
    binding->gamepad_axis.axis = axis;
    binding->gamepad_axis.threshold = clampf(threshold, 0.0f, 1.0f);
    binding->gamepad_axis.positive = positive;
    return true;
}

void carbon_input_clear_bindings(Carbon_Input *input, int action_id) {
    if (!input || action_id < 0 || action_id >= input->action_count) return;
    input->actions[action_id].binding_count = 0;
}

/* ============ Action Queries ============ */

bool carbon_input_action_pressed(Carbon_Input *input, int action_id) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;
    return input->actions[action_id].pressed;
}

bool carbon_input_action_just_pressed(Carbon_Input *input, int action_id) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;
    return input->actions[action_id].just_pressed;
}

bool carbon_input_action_just_released(Carbon_Input *input, int action_id) {
    if (!input || action_id < 0 || action_id >= input->action_count) return false;
    return input->actions[action_id].just_released;
}

float carbon_input_action_value(Carbon_Input *input, int action_id) {
    if (!input || action_id < 0 || action_id >= input->action_count) return 0.0f;
    return input->actions[action_id].value;
}

/* Convenience name-based functions */
bool carbon_input_pressed(Carbon_Input *input, const char *action) {
    return carbon_input_action_pressed(input, carbon_input_find_action(input, action));
}

bool carbon_input_just_pressed(Carbon_Input *input, const char *action) {
    return carbon_input_action_just_pressed(input, carbon_input_find_action(input, action));
}

bool carbon_input_just_released(Carbon_Input *input, const char *action) {
    return carbon_input_action_just_released(input, carbon_input_find_action(input, action));
}

float carbon_input_value(Carbon_Input *input, const char *action) {
    return carbon_input_action_value(input, carbon_input_find_action(input, action));
}

/* ============ Direct Input Queries ============ */

const Carbon_MouseState *carbon_input_get_mouse(Carbon_Input *input) {
    if (!input) return NULL;
    return &input->mouse;
}

void carbon_input_get_mouse_position(Carbon_Input *input, float *x, float *y) {
    if (!input) return;
    if (x) *x = input->mouse.x;
    if (y) *y = input->mouse.y;
}

void carbon_input_get_mouse_delta(Carbon_Input *input, float *dx, float *dy) {
    if (!input) return;
    if (dx) *dx = input->mouse.dx;
    if (dy) *dy = input->mouse.dy;
}

bool carbon_input_mouse_button(Carbon_Input *input, int button) {
    if (!input || button < 0 || button >= 5) return false;
    return input->mouse.buttons[button];
}

bool carbon_input_mouse_button_pressed(Carbon_Input *input, int button) {
    if (!input || button < 0 || button >= 5) return false;
    return input->mouse.buttons_pressed[button];
}

bool carbon_input_mouse_button_released(Carbon_Input *input, int button) {
    if (!input || button < 0 || button >= 5) return false;
    return input->mouse.buttons_released[button];
}

void carbon_input_get_scroll(Carbon_Input *input, float *x, float *y) {
    if (!input) return;
    if (x) *x = input->mouse.scroll_x;
    if (y) *y = input->mouse.scroll_y;
}

bool carbon_input_key_pressed(Carbon_Input *input, SDL_Scancode key) {
    if (!input || key >= MAX_KEYS) return false;
    return input->keys[key];
}

bool carbon_input_key_just_pressed(Carbon_Input *input, SDL_Scancode key) {
    if (!input || key >= MAX_KEYS) return false;
    return input->keys[key] && !input->keys_prev[key];
}

bool carbon_input_key_just_released(Carbon_Input *input, SDL_Scancode key) {
    if (!input || key >= MAX_KEYS) return false;
    return !input->keys[key] && input->keys_prev[key];
}

const Carbon_GamepadState *carbon_input_get_gamepad(Carbon_Input *input, int index) {
    if (!input || index < 0 || index >= MAX_GAMEPADS) return NULL;
    if (!input->gamepads[index].connected) return NULL;
    return &input->gamepads[index];
}

int carbon_input_get_gamepad_count(Carbon_Input *input) {
    if (!input) return 0;
    return input->gamepad_count;
}
