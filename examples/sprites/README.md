# Sprites Example

Demonstrates sprite rendering with the Carbon engine.

## What This Demonstrates

- Creating textures procedurally with `agentite_texture_create()`
- Creating sprites from textures
- Batched sprite rendering
- Sprite transforms: position, scale, rotation, origin
- Color tinting
- Camera integration with sprites
- Mouse wheel zoom

## Running

```bash
make example-sprites
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Pan camera |
| Mouse Wheel | Zoom |
| Escape | Quit |

## Key Patterns

### Sprite Rendering Flow
```c
// 1. Begin batch
agentite_sprite_begin(sprites, NULL);

// 2. Queue draw calls
agentite_sprite_draw(sprites, &sprite, x, y);
agentite_sprite_draw_scaled(sprites, &sprite, x, y, sx, sy);
agentite_sprite_draw_ex(sprites, &sprite, x, y, sx, sy, rot, ox, oy);
agentite_sprite_draw_tinted(sprites, &sprite, x, y, r, g, b, a);

// 3. Upload to GPU (before render pass)
SDL_GPUCommandBuffer *cmd = agentite_acquire_command_buffer(engine);
agentite_sprite_upload(sprites, cmd);

// 4. Render during pass
if (agentite_begin_render_pass(engine, ...)) {
    agentite_sprite_render(sprites, cmd, pass);
    agentite_end_render_pass(engine);
}

// 5. End batch
agentite_sprite_end(sprites, NULL, NULL);
```

### Loading Textures
```c
// From file
Agentite_Texture *tex = agentite_texture_load(sprites, "path/to/image.png");

// From memory (RGBA pixels)
Agentite_Texture *tex = agentite_texture_create(sprites, width, height, pixels);

// Create sprite
Agentite_Sprite sprite = agentite_sprite_from_texture(tex);

// Cleanup
agentite_texture_destroy(sprites, tex);
```
