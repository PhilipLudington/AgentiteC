# Pathfinding Example

Demonstrates the A* pathfinding system in Agentite.

## Features Shown

- Creating a walkability grid with `agentite_pathfinder_create()`
- Marking tiles as blocked with `agentite_pathfinder_set_walkable()`
- Finding paths with `agentite_pathfinder_find_ex()`
- Iterating path points with `agentite_path_get_point()`
- Dynamic obstacle updates (re-pathfinding when grid changes)
- Diagonal vs cardinal-only movement

## Controls

| Input | Action |
|-------|--------|
| Left-click | Set destination (agent will path to it) |
| Right-click | Toggle wall at cursor position |
| WASD | Pan camera |
| Scroll | Zoom camera |
| Space | Step agent one tile along path |
| 1 | Toggle diagonal movement |
| R | Reset grid (clear all walls) |

## Key Concepts

### Creating the Pathfinder

```c
// Create a grid for pathfinding
Agentite_Pathfinder *pf = agentite_pathfinder_create(width, height);

// Mark obstacles
agentite_pathfinder_set_walkable(pf, x, y, false);

// Set movement costs (optional - default is 1.0)
agentite_pathfinder_set_cost(pf, x, y, 2.0f);  // Rough terrain
```

### Finding a Path

```c
// Using default options
Agentite_Path *path = agentite_pathfinder_find(pf, start_x, start_y, end_x, end_y);

// Using custom options
Agentite_PathOptions options = AGENTITE_PATH_OPTIONS_DEFAULT;
options.allow_diagonal = false;
Agentite_Path *path = agentite_pathfinder_find_ex(pf, start_x, start_y, end_x, end_y, &options);

// Check result
if (path) {
    printf("Found path with %d steps\n", path->length);

    // Iterate path points
    for (int i = 0; i < path->length; i++) {
        const Agentite_PathPoint *pt = agentite_path_get_point(path, i);
        // Move entity to pt->x, pt->y
    }

    // Don't forget to free!
    agentite_path_destroy(path);
}
```

### Dynamic Updates

When the grid changes, simply re-run pathfinding:

```c
// Player places a wall
agentite_pathfinder_set_walkable(pf, wall_x, wall_y, false);

// Recalculate affected paths
if (current_path) {
    agentite_path_destroy(current_path);
    current_path = agentite_pathfinder_find(pf, agent_x, agent_y, goal_x, goal_y);
}
```

## Build

From the project root:

```bash
make examples  # Build all examples
# Or manually:
g++ -std=c++17 -I include -I lib examples/pathfinding/main.cpp src/...  -o pathfinding
```
