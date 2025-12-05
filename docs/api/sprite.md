# Sprite System

Batched sprite rendering with transform support.

## Quick Start

```c
#include "carbon/sprite.h"

// Initialize
Carbon_SpriteRenderer *sr = carbon_sprite_init(gpu, window);

// Load texture
Carbon_Texture *tex = carbon_texture_load(sr, "assets/player.png");
Carbon_Sprite sprite = carbon_sprite_from_texture(tex);

// In render loop (before render pass):
carbon_sprite_begin(sr, NULL);
carbon_sprite_draw(sr, &sprite, 100.0f, 200.0f);
carbon_sprite_upload(sr, cmd);

// During render pass
carbon_sprite_render(sr, cmd, pass);

// Cleanup
carbon_texture_destroy(sr, tex);
carbon_sprite_shutdown(sr);
```

## Key Functions

| Function | Description |
|----------|-------------|
| `carbon_sprite_init` | Create sprite renderer |
| `carbon_texture_load` | Load texture from file |
| `carbon_sprite_from_texture` | Create sprite from texture |
| `carbon_sprite_begin` | Begin sprite batch |
| `carbon_sprite_draw` | Draw sprite at position |
| `carbon_sprite_draw_scaled` | Draw with scale |
| `carbon_sprite_draw_ex` | Draw with full transform (scale, rotation, origin) |
| `carbon_sprite_draw_tinted` | Draw with color tint |
| `carbon_sprite_upload` | Upload batch to GPU (before render pass) |
| `carbon_sprite_render` | Render batch (during render pass) |
| `carbon_sprite_set_camera` | Connect camera for world-space rendering |
| `carbon_texture_destroy` | Free texture |
| `carbon_sprite_shutdown` | Cleanup renderer |

## Draw Variants

```c
// Simple position
carbon_sprite_draw(sr, &sprite, x, y);

// With scale
carbon_sprite_draw_scaled(sr, &sprite, x, y, scale_x, scale_y);

// Full transform: position, scale, rotation (degrees), origin
carbon_sprite_draw_ex(sr, &sprite, x, y, sx, sy, rotation, origin_x, origin_y);

// With color tint (RGBA 0.0-1.0)
carbon_sprite_draw_tinted(sr, &sprite, x, y, r, g, b, a);
```

## Camera Integration

```c
// World-space rendering (affected by camera)
carbon_sprite_set_camera(sr, camera);
carbon_sprite_begin(sr, NULL);
// ... draw world sprites ...

// Screen-space rendering (UI, not affected by camera)
carbon_sprite_set_camera(sr, NULL);
carbon_sprite_begin(sr, NULL);
// ... draw UI sprites ...
```

## Notes

- All sprites in a batch must use the same texture
- Multiple textures require separate batches or a texture atlas
- Call `upload` before render pass, `render` during render pass
- When no camera is set, sprites render in screen-space coordinates
