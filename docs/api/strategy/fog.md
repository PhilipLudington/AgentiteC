# Fog of War / Exploration System

Per-cell exploration tracking with visibility radius.

## Quick Start

```c
#include "agentite/fog.h"

Agentite_FogOfWar *fog = agentite_fog_create(100, 100);  // Map dimensions
agentite_fog_set_shroud_alpha(fog, 0.5f);  // Explored but not visible

// Add vision sources
Agentite_VisionSource unit = agentite_fog_add_source(fog, 50, 50, 8);  // Radius 8

// Update visibility
agentite_fog_update(fog);
```

## Vision Source Management

```c
// Move source
agentite_fog_move_source(fog, unit, new_x, new_y);

// Update radius
agentite_fog_set_source_radius(fog, unit, 10);

// Remove source
agentite_fog_remove_source(fog, unit);
```

## Query Visibility

```c
Agentite_VisibilityState state = agentite_fog_get_state(fog, x, y);
switch (state) {
    case AGENTITE_VIS_UNEXPLORED: /* Never seen */ break;
    case AGENTITE_VIS_EXPLORED:   /* Seen before, shroud */ break;
    case AGENTITE_VIS_VISIBLE:    /* Currently visible */ break;
}

// Convenience checks
if (agentite_fog_is_visible(fog, x, y)) { }
if (agentite_fog_is_explored(fog, x, y)) { }

// Get alpha for rendering
float alpha = agentite_fog_get_alpha(fog, x, y);
```

## Manual Exploration

```c
agentite_fog_explore_cell(fog, x, y);
agentite_fog_explore_rect(fog, 0, 0, 10, 10);
agentite_fog_explore_circle(fog, 50, 50, 5);

// Cheat commands
agentite_fog_reveal_all(fog);
agentite_fog_reset(fog);
```

## Line of Sight

```c
bool is_wall(int x, int y, void *userdata) {
    return tilemap_get_tile(map, x, y) == TILE_WALL;
}
agentite_fog_set_los_callback(fog, is_wall, tilemap);

// Check LOS between points
if (agentite_fog_has_los(fog, x1, y1, x2, y2)) { }
```

## Exploration Callback

```c
void on_explored(Agentite_FogOfWar *fog, int x, int y, void *userdata) {
    printf("Explored (%d, %d)\n", x, y);
}
agentite_fog_set_exploration_callback(fog, on_explored, NULL);
```

## Statistics

```c
float explored_pct = agentite_fog_get_exploration_percent(fog);
int unexplored, explored, visible;
agentite_fog_get_stats(fog, &unexplored, &explored, &visible);
```

## Visibility States

| State | Description |
|-------|-------------|
| `UNEXPLORED` | Never seen, don't render |
| `EXPLORED` | Seen before, render with shroud |
| `VISIBLE` | Currently visible, full render |
