#ifndef AGENTITE_TURN_H
#define AGENTITE_TURN_H

#include <stdbool.h>

// Turn phases (game can define meaning)
typedef enum {
    AGENTITE_PHASE_WORLD_UPDATE = 0,  // AI/simulation runs
    AGENTITE_PHASE_EVENTS,            // Events trigger
    AGENTITE_PHASE_PLAYER_INPUT,      // Player makes decisions
    AGENTITE_PHASE_RESOLUTION,        // Apply player actions
    AGENTITE_PHASE_END_CHECK,         // Victory/defeat check
    AGENTITE_PHASE_COUNT
} Agentite_TurnPhase;

// Phase callback
typedef void (*Agentite_PhaseCallback)(void *userdata, int turn_number);

// Turn manager (lightweight, can be stack allocated)
typedef struct Agentite_TurnManager {
    int turn_number;
    Agentite_TurnPhase current_phase;
    Agentite_PhaseCallback phase_callbacks[AGENTITE_PHASE_COUNT];
    void *phase_userdata[AGENTITE_PHASE_COUNT];
    bool turn_in_progress;
} Agentite_TurnManager;

// Initialize with default phases
void agentite_turn_init(Agentite_TurnManager *tm);

// Set phase callback
void agentite_turn_set_callback(Agentite_TurnManager *tm, Agentite_TurnPhase phase,
                               Agentite_PhaseCallback callback, void *userdata);

// Advance to next phase (calls callback)
// Returns true if turn completed (wrapped back to first phase)
bool agentite_turn_advance(Agentite_TurnManager *tm);

// Skip to specific phase
void agentite_turn_skip_to(Agentite_TurnManager *tm, Agentite_TurnPhase phase);

// Query
Agentite_TurnPhase agentite_turn_current_phase(const Agentite_TurnManager *tm);
int agentite_turn_number(const Agentite_TurnManager *tm);
const char *agentite_turn_phase_name(Agentite_TurnPhase phase);

#endif // AGENTITE_TURN_H
