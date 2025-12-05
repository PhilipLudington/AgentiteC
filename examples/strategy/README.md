# Strategy Game Example

Demonstrates RTS-style patterns: unit selection, movement, and A* pathfinding.

## What This Demonstrates

- RTS-style box selection
- A* pathfinding with obstacles
- Unit movement along paths
- Tilemap with blocked tiles
- Combining multiple systems (sprites, tilemap, pathfinding, UI, input)

## Running

```bash
make example-strategy
```

## Controls

| Input | Action |
|-------|--------|
| WASD | Pan camera |
| Left Click + Drag | Box select units |
| Right Click | Move selected units to location |
| Escape | Quit |

## Key Patterns

### Box Selection
```c
// Track selection state
bool selecting = false;
float sel_start_x, sel_start_y, sel_end_x, sel_end_y;

// Start selection on mouse down
if (carbon_input_mouse_button_just_pressed(input, 0)) {
    selecting = true;
    sel_start_x = world_x;
    sel_start_y = world_y;
}

// Update during drag
if (selecting && carbon_input_mouse_button(input, 0)) {
    sel_end_x = world_x;
    sel_end_y = world_y;
}

// Finish on release - select units in box
if (selecting && carbon_input_mouse_button_just_released(input, 0)) {
    selecting = false;
    // Check which units are inside the rectangle
}
```

### Pathfinding Setup
```c
// Create pathfinder matching tilemap dimensions
Carbon_Pathfinder *pf = carbon_pathfinder_create(map_width, map_height);

// Mark obstacles from tilemap
for (int y = 0; y < map_height; y++) {
    for (int x = 0; x < map_width; x++) {
        Carbon_TileID tile = carbon_tilemap_get_tile(tilemap, layer, x, y);
        if (tile == BLOCKED_TILE_ID) {
            carbon_pathfinder_set_walkable(pf, x, y, false);
        }
    }
}
```

### Path Following
```c
// Find path when commanded
Carbon_Path *path = carbon_pathfinder_find(pf, start_x, start_y, end_x, end_y);

// Follow path in update loop
if (path && path_index < path->length) {
    float target_x = path->points[path_index].x * TILE_SIZE + TILE_SIZE / 2;
    float target_y = path->points[path_index].y * TILE_SIZE + TILE_SIZE / 2;

    float dx = target_x - unit_x;
    float dy = target_y - unit_y;
    float dist = sqrtf(dx * dx + dy * dy);

    if (dist < 5.0f) {
        path_index++;  // Reached waypoint
    } else {
        // Move towards waypoint
        unit_x += (dx / dist) * speed * dt;
        unit_y += (dy / dist) * speed * dt;
    }
}
```

### Screen to Tile Conversion
```c
// Get mouse world position
float mouse_x, mouse_y;
carbon_input_get_mouse_position(input, &mouse_x, &mouse_y);

float world_x, world_y;
carbon_camera_screen_to_world(camera, mouse_x, mouse_y, &world_x, &world_y);

// Convert to tile coordinates
int tile_x = (int)(world_x / TILE_SIZE);
int tile_y = (int)(world_y / TILE_SIZE);
```

## Extending This Example

- Add unit types with different speeds
- Implement attack range and combat
- Add fog of war using a visibility layer
- Implement building placement
- Add resource gathering
- Use ECS for unit management (see game template)
