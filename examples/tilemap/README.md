# Tilemap Example

Demonstrates chunk-based tilemap rendering with camera controls.

## What This Demonstrates

- Creating tilesets from textures
- Multi-layer tilemaps
- Automatic frustum culling (only visible tiles render)
- Camera scrolling, zoom, and rotation
- Tile filling and individual tile setting

## Running

```bash
make example-tilemap
```

## Controls

| Key | Action |
|-----|--------|
| WASD | Pan camera |
| Q/E | Rotate camera |
| Mouse Wheel | Zoom |
| R | Reset camera |
| Escape | Quit |

## Key Patterns

### Creating Tilemaps
```c
// Load tileset texture (grid of tiles)
Carbon_Texture *tex = carbon_texture_load(sr, "tileset.png");

// Create tileset (texture, tile_width, tile_height)
Carbon_Tileset *tileset = carbon_tileset_create(tex, 32, 32);

// Create tilemap (tileset, map_width, map_height in tiles)
Carbon_Tilemap *tilemap = carbon_tilemap_create(tileset, 100, 100);

// Add layers (rendered back to front)
int ground = carbon_tilemap_add_layer(tilemap, "ground");
int objects = carbon_tilemap_add_layer(tilemap, "objects");
```

### Setting Tiles
```c
// Tile ID 0 = empty, 1+ = tiles from tileset (left-to-right, top-to-bottom)

// Fill region with tile
carbon_tilemap_fill(tilemap, ground, x, y, width, height, tile_id);

// Set single tile
carbon_tilemap_set_tile(tilemap, objects, x, y, tile_id);

// Get tile at position
Carbon_TileID tile = carbon_tilemap_get_tile(tilemap, ground, x, y);
```

### Layer Properties
```c
// Visibility
carbon_tilemap_set_layer_visible(tilemap, objects, false);

// Opacity (0.0 - 1.0)
carbon_tilemap_set_layer_opacity(tilemap, objects, 0.8f);
```

### Rendering
```c
// During sprite batch, before upload
carbon_sprite_begin(sprites, NULL);
carbon_tilemap_render(tilemap, sprites, camera);  // Frustum culled
// ... other sprites ...
carbon_sprite_upload(sprites, cmd);
// ... render pass ...
carbon_sprite_render(sprites, cmd, pass);
```

### Coordinate Conversion
```c
// World position to tile coordinates
int tile_x, tile_y;
carbon_tilemap_world_to_tile(tilemap, world_x, world_y, &tile_x, &tile_y);

// Tile to world position
float world_x, world_y;
carbon_tilemap_tile_to_world(tilemap, tile_x, tile_y, &world_x, &world_y);
```

## Performance Notes

- Tilemap uses 32x32 tile chunks internally
- Only chunks intersecting camera bounds are rendered
- All tiles share the same tileset texture (single batch)
- For very large maps, this system handles 1000x1000+ tiles efficiently
