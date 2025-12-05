# Turn/Phase System

Turn-based game flow management with phases and events.

## Quick Start

```c
#include "carbon/turn.h"

Carbon_TurnManager *turns = carbon_turn_create();

// Register phases
carbon_turn_add_phase(turns, "upkeep");
carbon_turn_add_phase(turns, "player_action");
carbon_turn_add_phase(turns, "ai_action");
carbon_turn_add_phase(turns, "resolution");

// Start game
carbon_turn_start(turns);
```

## Game Loop Integration

```c
// Check current state
int turn = carbon_turn_get_current(turns);
const char *phase = carbon_turn_get_current_phase(turns);

// Advance phases
if (phase_complete) {
    carbon_turn_next_phase(turns);
}

// Check for turn end
if (carbon_turn_is_last_phase(turns)) {
    // Process end of turn
}
```

## Callbacks

```c
void on_turn_start(int turn, void *userdata) {
    printf("Turn %d started\n", turn);
}

void on_phase_start(const char *phase, void *userdata) {
    printf("Phase: %s\n", phase);
}

carbon_turn_set_turn_callback(turns, on_turn_start, game);
carbon_turn_set_phase_callback(turns, on_phase_start, game);
```

## Event Integration

```c
// With event dispatcher
Carbon_TurnManager *turns = carbon_turn_create_with_events(events);
// Automatically emits CARBON_EVENT_TURN_STARTED, TURN_ENDED,
// PHASE_STARTED, PHASE_ENDED
```

## Query State

```c
int current_turn = carbon_turn_get_current(turns);
int phase_index = carbon_turn_get_phase_index(turns);
int total_phases = carbon_turn_get_phase_count(turns);
```
