# Sprite System

Batched sprite rendering with transform support.

## Quick Start

```c
#include "agentite/sprite.h"

// Initialize
Agentite_SpriteRenderer *sr = agentite_sprite_init(gpu, window);

// Load texture
Agentite_Texture *tex = agentite_texture_load(sr, "assets/player.png");
Agentite_Sprite sprite = agentite_sprite_from_texture(tex);

// In render loop (before render pass):
agentite_sprite_begin(sr, NULL);
agentite_sprite_draw(sr, &sprite, 100.0f, 200.0f);
agentite_sprite_upload(sr, cmd);

// During render pass
agentite_sprite_render(sr, cmd, pass);

// Cleanup
agentite_texture_destroy(sr, tex);
agentite_sprite_shutdown(sr);
```

## Key Functions

| Function | Description |
|----------|-------------|
| `agentite_sprite_init` | Create sprite renderer |
| `agentite_texture_load` | Load texture from file |
| `agentite_sprite_from_texture` | Create sprite from texture |
| `agentite_sprite_begin` | Begin sprite batch |
| `agentite_sprite_draw` | Draw sprite at position |
| `agentite_sprite_draw_scaled` | Draw with scale |
| `agentite_sprite_draw_ex` | Draw with full transform (scale, rotation, origin) |
| `agentite_sprite_draw_tinted` | Draw with color tint |
| `agentite_sprite_upload` | Upload batch to GPU (before render pass) |
| `agentite_sprite_render` | Render batch (during render pass) |
| `agentite_sprite_set_camera` | Connect camera for world-space rendering |
| `agentite_texture_destroy` | Free texture |
| `agentite_sprite_shutdown` | Cleanup renderer |

## Draw Variants

```c
// Simple position
agentite_sprite_draw(sr, &sprite, x, y);

// With scale
agentite_sprite_draw_scaled(sr, &sprite, x, y, scale_x, scale_y);

// Full transform: position, scale, rotation (degrees), origin
agentite_sprite_draw_ex(sr, &sprite, x, y, sx, sy, rotation, origin_x, origin_y);

// With color tint (RGBA 0.0-1.0)
agentite_sprite_draw_tinted(sr, &sprite, x, y, r, g, b, a);
```

## Camera Integration

```c
// World-space rendering (affected by camera)
agentite_sprite_set_camera(sr, camera);
agentite_sprite_begin(sr, NULL);
// ... draw world sprites ...

// Screen-space rendering (UI, not affected by camera)
agentite_sprite_set_camera(sr, NULL);
agentite_sprite_begin(sr, NULL);
// ... draw UI sprites ...
```

## Notes

- All sprites in a batch must use the same texture
- Multiple textures require separate batches or a texture atlas
- Call `upload` before render pass, `render` during render pass
- When no camera is set, sprites render in screen-space coordinates
