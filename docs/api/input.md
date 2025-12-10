# Input System

Action-based input abstraction supporting keyboard, mouse, and gamepad.

## Quick Start

```c
#include "agentite/input.h"

// Initialize
Agentite_Input *input = agentite_input_init();

// Register actions
int action_jump = agentite_input_register_action(input, "jump");
int action_fire = agentite_input_register_action(input, "fire");

// Bind inputs (up to 4 bindings per action)
agentite_input_bind_key(input, action_jump, SDL_SCANCODE_SPACE);
agentite_input_bind_key(input, action_jump, SDL_SCANCODE_W);
agentite_input_bind_mouse(input, action_fire, 1);  // Left mouse
agentite_input_bind_gamepad_button(input, action_jump, SDL_GAMEPAD_BUTTON_SOUTH);
```

## Game Loop

```c
agentite_input_begin_frame(input);  // Reset per-frame state

while (SDL_PollEvent(&event)) {
    if (aui_process_event(ui, &event)) continue;  // UI first
    agentite_input_process_event(input, &event);
    if (event.type == SDL_EVENT_QUIT) agentite_quit(engine);
}

agentite_input_update(input);  // Compute just_pressed/released
```

## Query Actions

```c
// By ID (faster)
if (agentite_input_action_pressed(input, action_jump)) { /* held down */ }
if (agentite_input_action_just_pressed(input, action_jump)) { /* this frame */ }
if (agentite_input_action_just_released(input, action_jump)) { /* released */ }
float val = agentite_input_action_value(input, action_move);  // Analog: -1.0 to 1.0

// By name (convenience)
if (agentite_input_pressed(input, "jump")) { }
```

## Direct Input Queries

```c
// Mouse
float mx, my;
agentite_input_get_mouse_position(input, &mx, &my);
agentite_input_get_scroll(input, &scroll_x, &scroll_y);
if (agentite_input_mouse_button(input, 0)) { }  // 0=left, 1=mid, 2=right

// Keyboard
if (agentite_input_key_just_pressed(input, SDL_SCANCODE_F1)) { }

// Gamepad
int count = agentite_input_get_gamepad_count(input);
const Agentite_GamepadState *pad = agentite_input_get_gamepad(input, 0);
```

## Gamepad Axis Binding

```c
// Bind axis with deadzone and invert option
agentite_input_bind_gamepad_axis(input, action_move_left,
                                SDL_GAMEPAD_AXIS_LEFTX, 0.3f, false);
```

## Key Features

- Action mapping: One action → multiple inputs
- Just pressed/released detection for frame-perfect input
- Analog values from gamepad axes and triggers
- Automatic gamepad hot-plug support
