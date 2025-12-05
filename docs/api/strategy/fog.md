# Fog of War / Exploration System

Per-cell exploration tracking with visibility radius.

## Quick Start

```c
#include "carbon/fog.h"

Carbon_FogOfWar *fog = carbon_fog_create(100, 100);  // Map dimensions
carbon_fog_set_shroud_alpha(fog, 0.5f);  // Explored but not visible

// Add vision sources
Carbon_VisionSource unit = carbon_fog_add_source(fog, 50, 50, 8);  // Radius 8

// Update visibility
carbon_fog_update(fog);
```

## Vision Source Management

```c
// Move source
carbon_fog_move_source(fog, unit, new_x, new_y);

// Update radius
carbon_fog_set_source_radius(fog, unit, 10);

// Remove source
carbon_fog_remove_source(fog, unit);
```

## Query Visibility

```c
Carbon_VisibilityState state = carbon_fog_get_state(fog, x, y);
switch (state) {
    case CARBON_VIS_UNEXPLORED: /* Never seen */ break;
    case CARBON_VIS_EXPLORED:   /* Seen before, shroud */ break;
    case CARBON_VIS_VISIBLE:    /* Currently visible */ break;
}

// Convenience checks
if (carbon_fog_is_visible(fog, x, y)) { }
if (carbon_fog_is_explored(fog, x, y)) { }

// Get alpha for rendering
float alpha = carbon_fog_get_alpha(fog, x, y);
```

## Manual Exploration

```c
carbon_fog_explore_cell(fog, x, y);
carbon_fog_explore_rect(fog, 0, 0, 10, 10);
carbon_fog_explore_circle(fog, 50, 50, 5);

// Cheat commands
carbon_fog_reveal_all(fog);
carbon_fog_reset(fog);
```

## Line of Sight

```c
bool is_wall(int x, int y, void *userdata) {
    return tilemap_get_tile(map, x, y) == TILE_WALL;
}
carbon_fog_set_los_callback(fog, is_wall, tilemap);

// Check LOS between points
if (carbon_fog_has_los(fog, x1, y1, x2, y2)) { }
```

## Exploration Callback

```c
void on_explored(Carbon_FogOfWar *fog, int x, int y, void *userdata) {
    printf("Explored (%d, %d)\n", x, y);
}
carbon_fog_set_exploration_callback(fog, on_explored, NULL);
```

## Statistics

```c
float explored_pct = carbon_fog_get_exploration_percent(fog);
int unexplored, explored, visible;
carbon_fog_get_stats(fog, &unexplored, &explored, &visible);
```

## Visibility States

| State | Description |
|-------|-------------|
| `UNEXPLORED` | Never seen, don't render |
| `EXPLORED` | Seen before, render with shroud |
| `VISIBLE` | Currently visible, full render |
