# Sprites Example

Demonstrates sprite rendering with the Carbon engine.

## What This Demonstrates

- Creating textures procedurally with `carbon_texture_create()`
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
carbon_sprite_begin(sprites, NULL);

// 2. Queue draw calls
carbon_sprite_draw(sprites, &sprite, x, y);
carbon_sprite_draw_scaled(sprites, &sprite, x, y, sx, sy);
carbon_sprite_draw_ex(sprites, &sprite, x, y, sx, sy, rot, ox, oy);
carbon_sprite_draw_tinted(sprites, &sprite, x, y, r, g, b, a);

// 3. Upload to GPU (before render pass)
SDL_GPUCommandBuffer *cmd = carbon_acquire_command_buffer(engine);
carbon_sprite_upload(sprites, cmd);

// 4. Render during pass
if (carbon_begin_render_pass(engine, ...)) {
    carbon_sprite_render(sprites, cmd, pass);
    carbon_end_render_pass(engine);
}

// 5. End batch
carbon_sprite_end(sprites, NULL, NULL);
```

### Loading Textures
```c
// From file
Carbon_Texture *tex = carbon_texture_load(sprites, "path/to/image.png");

// From memory (RGBA pixels)
Carbon_Texture *tex = carbon_texture_create(sprites, width, height, pixels);

// Create sprite
Carbon_Sprite sprite = carbon_sprite_from_texture(tex);

// Cleanup
carbon_texture_destroy(sprites, tex);
```
