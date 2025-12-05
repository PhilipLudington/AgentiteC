#include "carbon/resource.h"

void carbon_resource_init(Carbon_Resource *r, int initial, int maximum, int per_turn) {
    if (!r) return;

    r->current = initial;
    r->maximum = maximum;
    r->per_turn_base = per_turn;
    r->per_turn_modifier = 1.0f;
}

void carbon_resource_tick(Carbon_Resource *r) {
    if (!r) return;

    int gain = (int)(r->per_turn_base * r->per_turn_modifier);
    carbon_resource_add(r, gain);
}

bool carbon_resource_can_afford(const Carbon_Resource *r, int amount) {
    if (!r) return false;
    return r->current >= amount;
}

bool carbon_resource_spend(Carbon_Resource *r, int amount) {
    if (!r || amount < 0) return false;
    if (r->current < amount) return false;

    r->current -= amount;
    return true;
}

void carbon_resource_add(Carbon_Resource *r, int amount) {
    if (!r) return;

    r->current += amount;

    // Clamp to maximum if set
    if (r->maximum > 0 && r->current > r->maximum) {
        r->current = r->maximum;
    }

    // Don't go negative
    if (r->current < 0) {
        r->current = 0;
    }
}

void carbon_resource_set(Carbon_Resource *r, int value) {
    if (!r) return;

    r->current = value;

    if (r->maximum > 0 && r->current > r->maximum) {
        r->current = r->maximum;
    }
    if (r->current < 0) {
        r->current = 0;
    }
}

void carbon_resource_set_modifier(Carbon_Resource *r, float modifier) {
    if (!r) return;
    r->per_turn_modifier = modifier;
}

void carbon_resource_set_per_turn(Carbon_Resource *r, int per_turn) {
    if (!r) return;
    r->per_turn_base = per_turn;
}

void carbon_resource_set_max(Carbon_Resource *r, int maximum) {
    if (!r) return;
    r->maximum = maximum;

    // Clamp current if needed
    if (r->maximum > 0 && r->current > r->maximum) {
        r->current = r->maximum;
    }
}

int carbon_resource_preview_tick(const Carbon_Resource *r) {
    if (!r) return 0;
    return (int)(r->per_turn_base * r->per_turn_modifier);
}
