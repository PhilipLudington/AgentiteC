# Minimal Example

The absolute minimum Carbon engine setup - a window with a clear color.

## What This Demonstrates

- Engine initialization with `Agentite_Config`
- Basic game loop structure
- Render pass with clear color
- Proper cleanup

## Running

```bash
make example-minimal
```

## Code Overview (~40 lines)

```c
// 1. Configure engine
Agentite_Config config = {
    .window_title = "My Game",
    .window_width = 800,
    .window_height = 600
};

// 2. Initialize
Agentite_Engine *engine = agentite_init(&config);

// 3. Main loop
while (agentite_is_running(engine)) {
    agentite_begin_frame(engine);
    agentite_poll_events(engine);

    if (agentite_begin_render_pass(engine, r, g, b, a)) {
        // Render here
        agentite_end_render_pass(engine);
    }

    agentite_end_frame(engine);
}

// 4. Cleanup
agentite_shutdown(engine);
```

## Extending This Example

To add features, follow this order:

1. **Sprites**: Add `Agentite_SpriteRenderer` after engine init
2. **Camera**: Add `Agentite_Camera` and connect to sprite renderer
3. **Input**: Add `Agentite_Input` for keyboard/gamepad
4. **Text**: Add `Agentite_TextRenderer` for fonts
5. **UI**: Add `AUI_Context` for immediate-mode UI
6. **Audio**: Add `Agentite_Audio` for sound
7. **ECS**: Add `Agentite_World` for entity management

See the other examples for each of these features in isolation.
