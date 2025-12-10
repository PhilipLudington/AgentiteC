# Pathfinding System

A* pathfinding for tile-based maps with diagonal movement and weighted costs.

## Quick Start

```c
#include "agentite/pathfinding.h"

// Create pathfinder matching tilemap dimensions
Agentite_Pathfinder *pf = agentite_pathfinder_create(100, 100);

// Set obstacles
agentite_pathfinder_set_walkable(pf, 10, 10, false);
agentite_pathfinder_fill_walkable(pf, 20, 20, 5, 5, false);  // Block 5x5

// Set movement costs (rough terrain)
agentite_pathfinder_set_cost(pf, 5, 5, 2.0f);

// Find path
Agentite_Path *path = agentite_pathfinder_find(pf, start_x, start_y, end_x, end_y);
if (path) {
    for (int i = 0; i < path->length; i++) {
        int tx = path->points[i].x;
        int ty = path->points[i].y;
        // Move to (tx, ty)...
    }
    agentite_path_destroy(path);
}
```

## Tilemap Integration

```c
// Sync from tilemap layer with blocked tile IDs
uint16_t blocked[] = { 2, 3, 4 };  // Wall tiles
agentite_pathfinder_sync_tilemap(pf, tilemap, collision_layer, blocked, 3);

// Or with custom cost function
float terrain_cost(uint16_t tile_id, void *userdata) {
    switch (tile_id) {
        case 1: return 1.0f;  // Grass
        case 2: return 0.0f;  // Water (blocked)
        case 3: return 2.0f;  // Mud (slow)
        default: return 1.0f;
    }
}
agentite_pathfinder_sync_tilemap_ex(pf, tilemap, ground_layer, terrain_cost, NULL);
```

## Path Options

```c
Agentite_PathOptions opts = AGENTITE_PATH_OPTIONS_DEFAULT;
opts.allow_diagonal = false;       // 4-directional only
opts.max_iterations = 1000;        // Limit search
opts.cut_corners = true;           // Allow diagonal past corners
Agentite_Path *path = agentite_pathfinder_find_ex(pf, x1, y1, x2, y2, &opts);
```

## Quick Checks

```c
// Path exists?
if (agentite_pathfinder_has_path(pf, x1, y1, x2, y2)) { }

// Direct line clear?
if (agentite_pathfinder_line_clear(pf, x1, y1, x2, y2)) { }

// Simplify path (remove redundant waypoints)
path = agentite_path_simplify(path);
```

## Distance Utilities

```c
int manhattan = agentite_pathfinder_distance_manhattan(x1, y1, x2, y2);
float euclidean = agentite_pathfinder_distance_euclidean(x1, y1, x2, y2);
int chebyshev = agentite_pathfinder_distance_chebyshev(x1, y1, x2, y2);
```

## Key Features

- A* algorithm with binary heap
- 4-directional or 8-directional movement
- Per-tile movement costs
- Tilemap integration
- Path simplification
- Line-of-sight checking (Bresenham)

## Performance Notes

- Create pathfinder once, reuse for many searches
- Use `max_iterations` to limit search on large maps
