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
Agentite_Engine *engine = agentite_init(&config);
AUI_Context *ui = aui_init(...);
Agentite_SpriteRenderer *sprites = agentite_sprite_init(...);
Agentite_Camera *camera = agentite_camera_create(...);
Agentite_TextRenderer *text = agentite_text_init(...);
Agentite_World *ecs = agentite_ecs_init();
Agentite_Input *input = agentite_input_init();
Agentite_Audio *audio = agentite_audio_init();
```

### Main Loop Pattern
```c
while (agentite_is_running(engine)) {
    agentite_begin_frame(engine);
    agentite_input_begin_frame(input);

    // Poll events
    while (SDL_PollEvent(&event)) {
        if (aui_process_event(ui, &event)) continue;
        agentite_input_process_event(input, &event);
    }
    agentite_input_update(input);

    // Update logic...

    // Build batches before render pass
    agentite_sprite_begin(sprites, NULL);
    // ... draw sprites ...

    SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
    agentite_sprite_upload(sprites, cmd);
    aui_upload(ui, cmd);
    agentite_text_upload(text, cmd);

    // Render pass
    if (agentite_begin_render_pass(engine, 0.1f, 0.1f, 0.15f, 1.0f)) {
        SDL_GPURenderPass *pass = agentite_get_render_pass(engine);
        agentite_sprite_render(sprites, cmd, pass);
        aui_render(ui, cmd, pass);
        agentite_text_render(text, cmd, pass);
        agentite_end_render_pass(engine);
    }

    agentite_end_frame(engine);
}
```

### Cleanup Order (reverse of init)
```c
agentite_audio_shutdown(audio);
agentite_input_shutdown(input);
agentite_ecs_shutdown(ecs);
agentite_text_shutdown(text);
agentite_camera_destroy(camera);
agentite_sprite_shutdown(sprites);
aui_shutdown(ui);
agentite_shutdown(engine);
```
