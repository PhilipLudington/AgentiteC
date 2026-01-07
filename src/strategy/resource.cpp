#include "agentite/resource.h"
#include <climits>

void agentite_resource_init(Agentite_Resource *r, int initial, int maximum, int per_turn) {
    if (!r) return;

    r->current = initial;
    r->maximum = maximum;
    r->per_turn_base = per_turn;
    r->per_turn_modifier = 1.0f;
}

void agentite_resource_tick(Agentite_Resource *r) {
    if (!r) return;

    int gain = (int)(r->per_turn_base * r->per_turn_modifier);
    agentite_resource_add(r, gain);
}

bool agentite_resource_can_afford(const Agentite_Resource *r, int amount) {
    if (!r) return false;
    return r->current >= amount;
}

bool agentite_resource_spend(Agentite_Resource *r, int amount) {
    if (!r || amount < 0) return false;
    if (r->current < amount) return false;

    r->current -= amount;
    return true;
}

void agentite_resource_add(Agentite_Resource *r, int amount) {
    if (!r) return;

    // Overflow-safe addition
    if (amount > 0) {
        // Check for positive overflow
        if (r->current > INT_MAX - amount) {
            r->current = INT_MAX;
        } else {
            r->current += amount;
        }
    } else if (amount < 0) {
        // Check for negative overflow
        if (r->current < INT_MIN - amount) {
            r->current = INT_MIN;
        } else {
            r->current += amount;
        }
    }

    // Clamp to maximum if set
    if (r->maximum > 0 && r->current > r->maximum) {
        r->current = r->maximum;
    }

    // Don't go negative
    if (r->current < 0) {
        r->current = 0;
    }
}

void agentite_resource_set(Agentite_Resource *r, int value) {
    if (!r) return;

    r->current = value;

    if (r->maximum > 0 && r->current > r->maximum) {
        r->current = r->maximum;
    }
    if (r->current < 0) {
        r->current = 0;
    }
}

void agentite_resource_set_modifier(Agentite_Resource *r, float modifier) {
    if (!r) return;
    r->per_turn_modifier = modifier;
}

void agentite_resource_set_per_turn(Agentite_Resource *r, int per_turn) {
    if (!r) return;
    r->per_turn_base = per_turn;
}

void agentite_resource_set_max(Agentite_Resource *r, int maximum) {
    if (!r) return;
    r->maximum = maximum;

    // Clamp current if needed
    if (r->maximum > 0 && r->current > r->maximum) {
        r->current = r->maximum;
    }
}

int agentite_resource_preview_tick(const Agentite_Resource *r) {
    if (!r) return 0;
    return (int)(r->per_turn_base * r->per_turn_modifier);
}
