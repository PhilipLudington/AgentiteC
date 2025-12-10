# Spatial Hash Index

O(1) entity lookup by grid cell for efficient spatial queries.

## Quick Start

```c
#include "agentite/spatial.h"

Agentite_SpatialIndex *spatial = agentite_spatial_create(256);

// Add entities at grid positions
agentite_spatial_add(spatial, 10, 20, player_entity);
agentite_spatial_add(spatial, 10, 20, item_entity);  // Multiple per cell
```

## Basic Queries

```c
// Check for entities
if (agentite_spatial_has(spatial, 10, 20)) {
    uint32_t first = agentite_spatial_query(spatial, 10, 20);
}

// Get all at position
uint32_t entities[16];
int count = agentite_spatial_query_all(spatial, 10, 20, entities, 16);

// Check specific entity
if (agentite_spatial_has_entity(spatial, 10, 20, player)) { }
```

## Movement & Removal

```c
agentite_spatial_move(spatial, old_x, old_y, new_x, new_y, entity_id);
agentite_spatial_remove(spatial, 10, 20, entity_id);
```

## Region Queries

```c
Agentite_SpatialQueryResult results[64];

// Rectangle
int found = agentite_spatial_query_rect(spatial, x1, y1, x2, y2, results, 64);

// Chebyshev radius (square)
found = agentite_spatial_query_radius(spatial, cx, cy, 5, results, 64);

// Euclidean radius (circle)
found = agentite_spatial_query_circle(spatial, cx, cy, 5, results, 64);

for (int i = 0; i < found; i++) {
    uint32_t entity = results[i].entity_id;
    int x = results[i].x;
    int y = results[i].y;
}
```

## Common Patterns

```c
// Item pickup
if (agentite_spatial_has(items, player_x, player_y)) {
    uint32_t item = agentite_spatial_query(items, player_x, player_y);
    pickup(item);
    agentite_spatial_remove(items, player_x, player_y, item);
}

// Collision check
if (!agentite_spatial_has(units, dest_x, dest_y)) {
    agentite_spatial_move(units, x, y, dest_x, dest_y, unit_id);
}
```
