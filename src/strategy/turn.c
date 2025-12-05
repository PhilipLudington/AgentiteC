#include "carbon/turn.h"
#include <string.h>

static const char *phase_names[CARBON_PHASE_COUNT] = {
    "World Update",
    "Events",
    "Player Input",
    "Resolution",
    "End Check"
};

void carbon_turn_init(Carbon_TurnManager *tm) {
    if (!tm) return;

    memset(tm, 0, sizeof(Carbon_TurnManager));
    tm->turn_number = 1;
    tm->current_phase = CARBON_PHASE_WORLD_UPDATE;
    tm->turn_in_progress = false;
}

void carbon_turn_set_callback(Carbon_TurnManager *tm, Carbon_TurnPhase phase,
                               Carbon_PhaseCallback callback, void *userdata) {
    if (!tm || phase < 0 || phase >= CARBON_PHASE_COUNT) return;

    tm->phase_callbacks[phase] = callback;
    tm->phase_userdata[phase] = userdata;
}

bool carbon_turn_advance(Carbon_TurnManager *tm) {
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
    tm->current_phase++;

    // Check if turn completed
    if (tm->current_phase >= CARBON_PHASE_COUNT) {
        tm->current_phase = CARBON_PHASE_WORLD_UPDATE;
        tm->turn_number++;
        tm->turn_in_progress = false;
        return true;  // Turn completed
    }

    return false;  // Turn still in progress
}

void carbon_turn_skip_to(Carbon_TurnManager *tm, Carbon_TurnPhase phase) {
    if (!tm || phase < 0 || phase >= CARBON_PHASE_COUNT) return;
    tm->current_phase = phase;
}

Carbon_TurnPhase carbon_turn_current_phase(const Carbon_TurnManager *tm) {
    if (!tm) return CARBON_PHASE_WORLD_UPDATE;
    return tm->current_phase;
}

int carbon_turn_number(const Carbon_TurnManager *tm) {
    if (!tm) return 0;
    return tm->turn_number;
}

const char *carbon_turn_phase_name(Carbon_TurnPhase phase) {
    if (phase < 0 || phase >= CARBON_PHASE_COUNT) return "Unknown";
    return phase_names[phase];
}
