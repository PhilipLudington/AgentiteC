# Turn/Phase System

Turn-based game flow management with phases and events.

## Quick Start

```c
#include "agentite/turn.h"

Agentite_TurnManager *turns = agentite_turn_create();

// Register phases
agentite_turn_add_phase(turns, "upkeep");
agentite_turn_add_phase(turns, "player_action");
agentite_turn_add_phase(turns, "ai_action");
agentite_turn_add_phase(turns, "resolution");

// Start game
agentite_turn_start(turns);
```

## Game Loop Integration

```c
// Check current state
int turn = agentite_turn_get_current(turns);
const char *phase = agentite_turn_get_current_phase(turns);

// Advance phases
if (phase_complete) {
    agentite_turn_next_phase(turns);
}

// Check for turn end
if (agentite_turn_is_last_phase(turns)) {
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

agentite_turn_set_turn_callback(turns, on_turn_start, game);
agentite_turn_set_phase_callback(turns, on_phase_start, game);
```

## Event Integration

```c
// With event dispatcher
Agentite_TurnManager *turns = agentite_turn_create_with_events(events);
// Automatically emits AGENTITE_EVENT_TURN_STARTED, TURN_ENDED,
// PHASE_STARTED, PHASE_ENDED
```

## Query State

```c
int current_turn = agentite_turn_get_current(turns);
int phase_index = agentite_turn_get_phase_index(turns);
int total_phases = agentite_turn_get_phase_count(turns);
```
