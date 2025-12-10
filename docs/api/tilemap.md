# Tilemap System

Chunk-based tilemap for large maps with multiple layers and efficient frustum culling.

## Quick Start

```c
#include "agentite/tilemap.h"

// Create tileset from texture (e.g., 4x4 grid of 32px tiles)
Agentite_Texture *tex = agentite_texture_load(sr, "assets/tileset.png");
Agentite_Tileset *tileset = agentite_tileset_create(tex, 32, 32);

// Create tilemap (100x100 tiles)
Agentite_Tilemap *tilemap = agentite_tilemap_create(tileset, 100, 100);

// Add layers (rendered back to front)
int ground = agentite_tilemap_add_layer(tilemap, "ground");
int objects = agentite_tilemap_add_layer(tilemap, "objects");

// Set tiles (tile ID 0 = empty, 1+ = valid tile)
agentite_tilemap_fill(tilemap, ground, 0, 0, 100, 100, 1);
agentite_tilemap_set_tile(tilemap, objects, 50, 50, 5);
```

## Rendering

```c
// During sprite batch, before upload
agentite_sprite_begin(sr, NULL);
agentite_tilemap_render(tilemap, sr, camera);  // Automatic frustum culling
// ... other sprites ...
agentite_sprite_upload(sr, cmd);
// ... render pass ...
agentite_sprite_render(sr, cmd, pass);
```

## Layer Properties

```c
agentite_tilemap_set_layer_visible(tilemap, objects, true);
agentite_tilemap_set_layer_opacity(tilemap, objects, 0.8f);
```

## Coordinate Conversion

```c
int tile_x, tile_y;
agentite_tilemap_world_to_tile(tilemap, world_x, world_y, &tile_x, &tile_y);
Agentite_TileID tile = agentite_tilemap_get_tile(tilemap, ground, tile_x, tile_y);
```

## Key Features

- Chunk-based storage (32x32 tiles per chunk) for efficient large maps
- Automatic frustum culling - only visible tiles are rendered
- Multiple layers with per-layer visibility and opacity
- Uses sprite renderer batching (single tileset = single batch)

## Notes

- All tiles must use the same tileset texture
- For multiple tilesets, use a texture atlas or render separate tilemaps
