# Carbon Demo Example

This is a comprehensive demo showcasing most Carbon engine features.

## Features Demonstrated

- **Tilemap System**: 50x50 tile map with multiple layers and various terrain types
- **Sprite Rendering**: Static, rotating, pulsing, and tinted sprites
- **Camera Controls**: Pan (WASD), zoom (mouse wheel), rotation (Q/E), reset (R)
- **Text Rendering**: Both bitmap fonts and MSDF fonts with effects
- **UI System**: Panels, buttons, checkboxes, sliders, dropdowns, textboxes, listboxes
- **ECS Integration**: Player and enemy entities with position, velocity, health components
- **Input System**: Action-based input with keyboard and gamepad support
- **Audio System**: Procedurally generated test sounds with volume controls

## Controls

| Key/Button | Action |
|------------|--------|
| WASD / Arrow Keys | Pan camera |
| Q / E | Rotate camera |
| R | Reset camera |
| Mouse Wheel | Zoom |
| Space | Play test sound |
| Escape | Quit |

Gamepad is also supported with left stick for pan, triggers for zoom, and shoulder buttons for rotation.

## Running

```bash
# From the Carbon root directory
make run-demo
```

## Files

- `main.c` - Complete demo application (~900 lines)

## Key Patterns

### Initialization Order
```c
Carbon_Engine *engine = carbon_init(&config);
CUI_Context *ui = cui_init(...);
Carbon_SpriteRenderer *sprites = carbon_sprite_init(...);
Carbon_Camera *camera = carbon_camera_create(...);
Carbon_TextRenderer *text = carbon_text_init(...);
Carbon_World *ecs = carbon_ecs_init();
Carbon_Input *input = carbon_input_init();
Carbon_Audio *audio = carbon_audio_init();
```

### Main Loop Pattern
```c
while (carbon_is_running(engine)) {
    carbon_begin_frame(engine);
    carbon_input_begin_frame(input);

    // Poll events
    while (SDL_PollEvent(&event)) {
        if (cui_process_event(ui, &event)) continue;
        carbon_input_process_event(input, &event);
    }
    carbon_input_update(input);

    // Update logic...

    // Build batches before render pass
    carbon_sprite_begin(sprites, NULL);
    // ... draw sprites ...

    SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
    carbon_sprite_upload(sprites, cmd);
    cui_upload(ui, cmd);
    carbon_text_upload(text, cmd);

    // Render pass
    if (carbon_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
        SDL_GPURenderPass *pass = carbon_get_render_pass(engine);
        carbon_sprite_render(sprites, cmd, pass);
        cui_render(ui, cmd, pass);
        carbon_text_render(text, cmd, pass);
        carbon_end_render_pass(engine);
    }

    carbon_end_frame(engine);
}
```

### Cleanup Order (reverse of init)
```c
carbon_audio_shutdown(audio);
carbon_input_shutdown(input);
carbon_ecs_shutdown(ecs);
carbon_text_shutdown(text);
carbon_camera_destroy(camera);
carbon_sprite_shutdown(sprites);
cui_shutdown(ui);
carbon_shutdown(engine);
```
