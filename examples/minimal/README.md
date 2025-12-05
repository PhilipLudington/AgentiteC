# Minimal Example

The absolute minimum Carbon engine setup - a window with a clear color.

## What This Demonstrates

- Engine initialization with `Carbon_Config`
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
Carbon_Config config = {
    .window_title = "My Game",
    .window_width = 800,
    .window_height = 600
};

// 2. Initialize
Carbon_Engine *engine = carbon_init(&config);

// 3. Main loop
while (carbon_is_running(engine)) {
    carbon_begin_frame(engine);
    carbon_poll_events(engine);

    if (carbon_begin_render_pass(engine, r, g, b, a)) {
        // Render here
        carbon_end_render_pass(engine);
    }

    carbon_end_frame(engine);
}

// 4. Cleanup
carbon_shutdown(engine);
```

## Extending This Example

To add features, follow this order:

1. **Sprites**: Add `Carbon_SpriteRenderer` after engine init
2. **Camera**: Add `Carbon_Camera` and connect to sprite renderer
3. **Input**: Add `Carbon_Input` for keyboard/gamepad
4. **Text**: Add `Carbon_TextRenderer` for fonts
5. **UI**: Add `CUI_Context` for immediate-mode UI
6. **Audio**: Add `Carbon_Audio` for sound
7. **ECS**: Add `Carbon_World` for entity management

See the other examples for each of these features in isolation.
