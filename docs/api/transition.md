# Transition System

The Agentite Transition System provides smooth visual transitions between game screens and scenes. It supports various effects including fade, wipe, dissolve, slide, and iris/circle transitions.

## Header

```c
#include "agentite/transition.h"
```

## Quick Start

```c
// Create transition system
Agentite_TransitionConfig config = AGENTITE_TRANSITION_CONFIG_DEFAULT;
config.duration = 0.5f;
config.effect = AGENTITE_TRANSITION_FADE;
Agentite_Transition *trans = agentite_transition_create(shader_system, window, &config);

// When changing scenes:
// 1. Capture the current scene
agentite_transition_capture_source(trans, cmd, current_scene_texture);

// 2. Start the transition
agentite_transition_start(trans);

// 3. In your render loop:
if (agentite_transition_is_active(trans)) {
    agentite_transition_update(trans, delta_time);

    // Check if we should switch scenes (at midpoint)
    if (agentite_transition_past_midpoint(trans) && !scene_switched) {
        switch_to_new_scene();
        scene_switched = true;
    }

    // Render the transition effect
    agentite_transition_render(trans, cmd, pass, new_scene_texture);
}

// Cleanup
agentite_transition_destroy(trans);
```

## Transition Effects

### Fade Effects
| Effect | Description |
|--------|-------------|
| `AGENTITE_TRANSITION_FADE` | Fade through a solid color (default: black) |
| `AGENTITE_TRANSITION_CROSSFADE` | Cross-dissolve directly between scenes |

### Wipe Effects
| Effect | Description |
|--------|-------------|
| `AGENTITE_TRANSITION_WIPE_LEFT` | Wipe from right to left |
| `AGENTITE_TRANSITION_WIPE_RIGHT` | Wipe from left to right |
| `AGENTITE_TRANSITION_WIPE_UP` | Wipe from bottom to top |
| `AGENTITE_TRANSITION_WIPE_DOWN` | Wipe from top to bottom |
| `AGENTITE_TRANSITION_WIPE_DIAGONAL` | Diagonal wipe from corner |

### Dissolve Effects
| Effect | Description |
|--------|-------------|
| `AGENTITE_TRANSITION_DISSOLVE` | Noise-based dissolve pattern |
| `AGENTITE_TRANSITION_PIXELATE` | Pixelate out then in |

### Slide Effects
| Effect | Description |
|--------|-------------|
| `AGENTITE_TRANSITION_SLIDE_LEFT` | New scene slides in from right |
| `AGENTITE_TRANSITION_SLIDE_RIGHT` | New scene slides in from left |
| `AGENTITE_TRANSITION_SLIDE_UP` | New scene slides in from bottom |
| `AGENTITE_TRANSITION_SLIDE_DOWN` | New scene slides in from top |

### Push Effects
| Effect | Description |
|--------|-------------|
| `AGENTITE_TRANSITION_PUSH_LEFT` | Old scene pushes off to left |
| `AGENTITE_TRANSITION_PUSH_RIGHT` | Old scene pushes off to right |
| `AGENTITE_TRANSITION_PUSH_UP` | Old scene pushes off upward |
| `AGENTITE_TRANSITION_PUSH_DOWN` | Old scene pushes off downward |

### Circle/Iris Effects
| Effect | Description |
|--------|-------------|
| `AGENTITE_TRANSITION_CIRCLE_OPEN` | Circle opens to reveal new scene |
| `AGENTITE_TRANSITION_CIRCLE_CLOSE` | Circle closes over old scene |

## Easing Functions

Control the timing curve of transitions:

| Easing | Description |
|--------|-------------|
| `AGENTITE_EASING_LINEAR` | Constant speed |
| `AGENTITE_EASING_EASE_IN` | Slow start |
| `AGENTITE_EASING_EASE_OUT` | Slow end |
| `AGENTITE_EASING_EASE_IN_OUT` | Slow start and end |
| `AGENTITE_EASING_QUAD_IN` | Quadratic acceleration |
| `AGENTITE_EASING_QUAD_OUT` | Quadratic deceleration |
| `AGENTITE_EASING_QUAD_IN_OUT` | Quadratic both |
| `AGENTITE_EASING_CUBIC_IN` | Cubic acceleration |
| `AGENTITE_EASING_CUBIC_OUT` | Cubic deceleration |
| `AGENTITE_EASING_CUBIC_IN_OUT` | Cubic both |
| `AGENTITE_EASING_BACK_IN` | Overshoot at start |
| `AGENTITE_EASING_BACK_OUT` | Overshoot at end |
| `AGENTITE_EASING_BOUNCE_OUT` | Bounce effect at end |

## Configuration

```c
typedef struct Agentite_TransitionConfig {
    Agentite_TransitionEffect effect;   // Effect type
    Agentite_TransitionEasing easing;   // Timing curve
    float duration;                      // Duration in seconds

    float fade_color[4];                 // RGBA for fade effect
    float edge_softness;                 // Wipe/dissolve edge softness (0-1)
    float pixel_size;                    // Max pixel size for pixelate

    float circle_center_x;               // Circle center X (0-1)
    float circle_center_y;               // Circle center Y (0-1)

    // Callbacks
    Agentite_TransitionCallback on_start;
    Agentite_TransitionCallback on_midpoint;
    Agentite_TransitionCallback on_complete;
    void *callback_user_data;
} Agentite_TransitionConfig;

#define AGENTITE_TRANSITION_CONFIG_DEFAULT { ... }
```

## Callbacks

Register callbacks for transition events:

```c
void on_transition_midpoint(Agentite_Transition *trans, void *user_data) {
    GameState *state = (GameState *)user_data;
    state->switch_scene();
}

void on_transition_complete(Agentite_Transition *trans, void *user_data) {
    GameState *state = (GameState *)user_data;
    state->transition_finished = true;
}

agentite_transition_set_callbacks(trans,
    NULL,                    // on_start
    on_transition_midpoint,  // on_midpoint - switch scene here
    on_transition_complete,  // on_complete
    game_state);             // user_data
```

## Integration with Scene System

The transition system works well with the scene system:

```c
// Scene transition with visual effect
void change_scene(const char *new_scene_path) {
    // Capture current scene to texture
    render_scene_to_texture(current_scene, render_target);
    agentite_transition_capture_source(trans, cmd, render_target);

    // Store path for loading at midpoint
    pending_scene_path = new_scene_path;

    // Start transition
    agentite_transition_start(trans);
}

// In update loop
void update(float dt) {
    if (agentite_transition_is_running(trans)) {
        agentite_transition_update(trans, dt);

        // Load new scene at midpoint
        if (agentite_transition_past_midpoint(trans) && pending_scene_path) {
            agentite_scene_transition(scene_manager, pending_scene_path, world, &ctx);
            pending_scene_path = NULL;
        }
    }
}

// In render loop
void render() {
    if (agentite_transition_is_active(trans)) {
        // Render current scene to texture
        render_current_scene_to_texture();

        // Apply transition effect
        agentite_transition_render(trans, cmd, pass, current_texture);
    } else {
        // Normal rendering
        render_scene();
    }
}
```

## API Reference

### Lifecycle

| Function | Description |
|----------|-------------|
| `agentite_transition_create` | Create transition system |
| `agentite_transition_destroy` | Destroy transition system |
| `agentite_transition_resize` | Resize render targets |

### Configuration

| Function | Description |
|----------|-------------|
| `agentite_transition_set_effect` | Change effect type |
| `agentite_transition_set_easing` | Change easing function |
| `agentite_transition_set_duration` | Change duration |
| `agentite_transition_set_fade_color` | Set fade color |
| `agentite_transition_set_callbacks` | Set event callbacks |

### Control

| Function | Description |
|----------|-------------|
| `agentite_transition_capture_source` | Capture outgoing scene |
| `agentite_transition_start` | Begin transition |
| `agentite_transition_start_with_effect` | Begin with specific effect |
| `agentite_transition_cancel` | Cancel transition |
| `agentite_transition_update` | Update transition state |

### Rendering

| Function | Description |
|----------|-------------|
| `agentite_transition_render` | Render transition effect |
| `agentite_transition_render_blend` | Lower-level blend rendering |

### State Queries

| Function | Description |
|----------|-------------|
| `agentite_transition_is_active` | Check if active |
| `agentite_transition_is_running` | Check if running |
| `agentite_transition_is_complete` | Check if just completed |
| `agentite_transition_get_state` | Get current state |
| `agentite_transition_get_progress` | Get progress (0-1) |
| `agentite_transition_get_eased_progress` | Get eased progress |
| `agentite_transition_get_remaining` | Get remaining time |
| `agentite_transition_past_midpoint` | Check if past 50% |

### Utilities

| Function | Description |
|----------|-------------|
| `agentite_transition_apply_easing` | Apply easing to value |
| `agentite_transition_effect_name` | Get effect name string |
| `agentite_transition_easing_name` | Get easing name string |

## Thread Safety

All transition functions are NOT thread-safe and must be called from the main/render thread only.
