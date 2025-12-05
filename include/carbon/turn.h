#ifndef CARBON_TURN_H
#define CARBON_TURN_H

#include <stdbool.h>

// Turn phases (game can define meaning)
typedef enum {
    CARBON_PHASE_WORLD_UPDATE = 0,  // AI/simulation runs
    CARBON_PHASE_EVENTS,            // Events trigger
    CARBON_PHASE_PLAYER_INPUT,      // Player makes decisions
    CARBON_PHASE_RESOLUTION,        // Apply player actions
    CARBON_PHASE_END_CHECK,         // Victory/defeat check
    CARBON_PHASE_COUNT
} Carbon_TurnPhase;

// Phase callback
typedef void (*Carbon_PhaseCallback)(void *userdata, int turn_number);

// Turn manager (lightweight, can be stack allocated)
typedef struct Carbon_TurnManager {
    int turn_number;
    Carbon_TurnPhase current_phase;
    Carbon_PhaseCallback phase_callbacks[CARBON_PHASE_COUNT];
    void *phase_userdata[CARBON_PHASE_COUNT];
    bool turn_in_progress;
} Carbon_TurnManager;

// Initialize with default phases
void carbon_turn_init(Carbon_TurnManager *tm);

// Set phase callback
void carbon_turn_set_callback(Carbon_TurnManager *tm, Carbon_TurnPhase phase,
                               Carbon_PhaseCallback callback, void *userdata);

// Advance to next phase (calls callback)
// Returns true if turn completed (wrapped back to first phase)
bool carbon_turn_advance(Carbon_TurnManager *tm);

// Skip to specific phase
void carbon_turn_skip_to(Carbon_TurnManager *tm, Carbon_TurnPhase phase);

// Query
Carbon_TurnPhase carbon_turn_current_phase(const Carbon_TurnManager *tm);
int carbon_turn_number(const Carbon_TurnManager *tm);
const char *carbon_turn_phase_name(Carbon_TurnPhase phase);

#endif // CARBON_TURN_H
