# Power Network System

Grid-based power distribution for factory and strategy games. Buildings connect to power networks through poles/substations, with automatic network merging and splitting.

## Quick Start

```c
#include "agentite/power.h"

// Create power system (100x100 grid)
Agentite_PowerSystem *ps = agentite_power_create(100, 100);

// Add power poles to create network
int pole1 = agentite_power_add_pole(ps, 10, 10, 5);  // Radius 5
int pole2 = agentite_power_add_pole(ps, 14, 10, 5);  // Auto-connects to pole1

// Add generator (produces 100 power units)
int gen = agentite_power_add_producer(ps, 10, 10, 100);

// Add consumer building (requires 50 power)
int factory = agentite_power_add_consumer(ps, 12, 10, 50);

// Check power status at a position
Agentite_PowerStatus status = agentite_power_get_status_at(ps, 12, 10);
if (status == AGENTITE_POWER_POWERED) {
    // Building is powered
}

// Check specific consumer
if (agentite_power_is_consumer_powered(ps, factory)) {
    run_factory_production();
}

agentite_power_destroy(ps);
```

## Key Concepts

### Power Poles and Coverage

Poles define the power grid. Each pole has a radius that determines coverage:

```c
// Regular pole with radius 5
int pole = agentite_power_add_pole(ps, x, y, 5);

// Substation with larger coverage and connection range
int sub = agentite_power_add_substation(ps, x, y, 10);
```

Poles automatically connect when their connection ranges overlap (radius × 2 by default).

### Power Networks

Connected poles form networks. Production and consumption is tracked per-network:

```c
// Get all networks
int networks[64];
int count = agentite_power_get_networks(ps, networks, 64);

// Get network statistics
Agentite_NetworkStats stats;
agentite_power_get_network_stats(ps, network_id, &stats);

printf("Network %d: %d/%d power (%d%% satisfied)\n",
       stats.network_id,
       stats.total_consumption,
       stats.total_production,
       (int)(stats.satisfaction_ratio * 100));
```

### Power Status

Three power states:
- `AGENTITE_POWER_UNPOWERED` - No power available (not connected to network)
- `AGENTITE_POWER_BROWNOUT` - Partial power (demand exceeds supply)
- `AGENTITE_POWER_POWERED` - Full power available

```c
Agentite_PowerStatus status = agentite_power_get_status_at(ps, x, y);

switch (status) {
    case AGENTITE_POWER_UNPOWERED:
        draw_unpowered_indicator(x, y);
        break;
    case AGENTITE_POWER_BROWNOUT:
        draw_brownout_indicator(x, y);
        // Factory runs at reduced speed
        break;
    case AGENTITE_POWER_POWERED:
        // Full operation
        break;
}
```

### Network Changes

When poles are added or removed, networks automatically reconfigure:

```c
// Set callback for network changes
void on_power_change(Agentite_PowerSystem *ps, int network_id,
                     Agentite_PowerStatus old_status,
                     Agentite_PowerStatus new_status, void *userdata) {
    if (new_status == AGENTITE_POWER_BROWNOUT) {
        show_notification("Power shortage in network %d!", network_id);
    } else if (old_status == AGENTITE_POWER_BROWNOUT &&
               new_status == AGENTITE_POWER_POWERED) {
        show_notification("Power restored in network %d", network_id);
    }
}

agentite_power_set_callback(ps, on_power_change, NULL);
```

Removing a pole may split a network:

```c
// This may create two separate networks
agentite_power_remove_pole(ps, bridging_pole_id);
```

## Key Functions

| Function | Description |
|----------|-------------|
| `agentite_power_create(w, h)` | Create power system |
| `agentite_power_destroy(ps)` | Free power system |
| `agentite_power_add_pole(ps, x, y, radius)` | Add power pole |
| `agentite_power_add_substation(ps, x, y, radius)` | Add substation (larger range) |
| `agentite_power_remove_pole(ps, id)` | Remove pole (may split network) |
| `agentite_power_add_producer(ps, x, y, amount)` | Add generator |
| `agentite_power_add_consumer(ps, x, y, amount)` | Add consumer building |
| `agentite_power_remove_producer(ps, id)` | Remove generator |
| `agentite_power_remove_consumer(ps, id)` | Remove consumer |
| `agentite_power_is_covered(ps, x, y)` | Check if position has pole coverage |
| `agentite_power_get_status_at(ps, x, y)` | Get power status at position |
| `agentite_power_is_consumer_powered(ps, id)` | Check if consumer is powered |
| `agentite_power_get_network_at(ps, x, y)` | Get network ID at position |
| `agentite_power_get_network_stats(ps, id, out)` | Get network statistics |
| `agentite_power_get_total_production(ps)` | Total production (all networks) |
| `agentite_power_get_total_consumption(ps)` | Total consumption (all networks) |
| `agentite_power_recalculate(ps)` | Force network recalculation |

## Producer/Consumer Control

```c
// Toggle generator on/off
agentite_power_set_producer_active(ps, gen_id, false);

// Update production value
agentite_power_set_production(ps, gen_id, 150);

// Toggle consumer on/off
agentite_power_set_consumer_active(ps, consumer_id, false);

// Update consumption value
agentite_power_set_consumption(ps, consumer_id, 75);
```

## Coverage Visualization

For rendering power grid overlays:

```c
// Get all cells covered by a pole
int cells[256];
int count = agentite_power_get_pole_coverage(ps, pole_id, cells, 256);

for (int i = 0; i < count; i += 2) {
    int x = cells[i], y = cells[i + 1];
    draw_coverage_indicator(x, y);
}

// Get all cells in a network
count = agentite_power_get_network_coverage(ps, network_id, cells, 256);

// Find nearest pole to unpowered location
int nearest_x, nearest_y;
int dist = agentite_power_find_nearest_pole(ps, x, y, &nearest_x, &nearest_y);

if (dist >= 0) {
    draw_line_to_nearest_pole(x, y, nearest_x, nearest_y);
}
```

## Configuration

```c
// Change how far poles can connect to each other
// Default: radius * 2.0
agentite_power_set_connection_multiplier(ps, 2.5f);

// Change brownout threshold
// Default: 0.5 (brownout when supply < 50% of demand)
agentite_power_set_brownout_threshold(ps, 0.7f);
```

## Batch Updates

For performance when placing multiple buildings:

```c
// Add many buildings without recalculating each time
for (int i = 0; i < building_count; i++) {
    agentite_power_add_consumer(ps, buildings[i].x, buildings[i].y,
                                 buildings[i].consumption);
}

// Recalculate all networks once at the end
agentite_power_recalculate(ps);
```

## Notes

- Producers and consumers must be within pole coverage to connect
- Networks automatically merge when poles are added within connection range
- Networks automatically split when bridging poles are removed
- Status callbacks fire after any operation that changes power state
- Multiple networks can exist independently on the same grid
