#include "agentite/turn.h"
#include <string.h>

static const char *phase_names[AGENTITE_PHASE_COUNT] = {
    "World Update",
    "Events",
    "Player Input",
    "Resolution",
    "End Check"
};

void agentite_turn_init(Agentite_TurnManager *tm) {
    if (!tm) return;

    memset(tm, 0, sizeof(Agentite_TurnManager));
    tm->turn_number = 1;
    tm->current_phase = AGENTITE_PHASE_WORLD_UPDATE;
    tm->turn_in_progress = false;
}

void agentite_turn_set_callback(Agentite_TurnManager *tm, Agentite_TurnPhase phase,
                               Agentite_PhaseCallback callback, void *userdata) {
    if (!tm || phase < 0 || phase >= AGENTITE_PHASE_COUNT) return;

    tm->phase_callbacks[phase] = callback;
    tm->phase_userdata[phase] = userdata;
}

bool agentite_turn_advance(Agentite_TurnManager *tm) {
    if (!tm) return false;

    tm->turn_in_progress = true;

    // Call callback for current phase
    if (tm->phase_callbacks[tm->current_phase]) {
        tm->phase_callbacks[tm->current_phase](
            tm->phase_userdata[tm->current_phase],
            tm->turn_number
        );
    }

    // Advance to next phase
    tm->current_phase = (Agentite_TurnPhase)(tm->current_phase + 1);

    // Check if turn completed
    if (tm->current_phase >= AGENTITE_PHASE_COUNT) {
        tm->current_phase = AGENTITE_PHASE_WORLD_UPDATE;
        tm->turn_number++;
        tm->turn_in_progress = false;
        return true;  // Turn completed
    }

    return false;  // Turn still in progress
}

void agentite_turn_skip_to(Agentite_TurnManager *tm, Agentite_TurnPhase phase) {
    if (!tm || phase < 0 || phase >= AGENTITE_PHASE_COUNT) return;
    tm->current_phase = phase;
}

Agentite_TurnPhase agentite_turn_current_phase(const Agentite_TurnManager *tm) {
    if (!tm) return AGENTITE_PHASE_WORLD_UPDATE;
    return tm->current_phase;
}

int agentite_turn_number(const Agentite_TurnManager *tm) {
    if (!tm) return 0;
    return tm->turn_number;
}

const char *agentite_turn_phase_name(Agentite_TurnPhase phase) {
    if (phase < 0 || phase >= AGENTITE_PHASE_COUNT) return "Unknown";
    return phase_names[phase];
}
