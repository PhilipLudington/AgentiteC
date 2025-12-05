#ifndef CARBON_RESOURCE_H
#define CARBON_RESOURCE_H

#include <stdbool.h>

// Single resource type (lightweight, can be used as ECS component)
typedef struct Carbon_Resource {
    int current;
    int maximum;            // 0 = unlimited
    int per_turn_base;
    float per_turn_modifier; // Multiplier (1.0 = normal)
} Carbon_Resource;

// Initialize with defaults
void carbon_resource_init(Carbon_Resource *r, int initial, int maximum, int per_turn);

// Per-turn tick (adds per_turn_base * per_turn_modifier, clamped to max)
void carbon_resource_tick(Carbon_Resource *r);

// Spending
bool carbon_resource_can_afford(const Carbon_Resource *r, int amount);
bool carbon_resource_spend(Carbon_Resource *r, int amount);

// Adding (respects maximum)
void carbon_resource_add(Carbon_Resource *r, int amount);

// Set values
void carbon_resource_set(Carbon_Resource *r, int value);
void carbon_resource_set_modifier(Carbon_Resource *r, float modifier);
void carbon_resource_set_per_turn(Carbon_Resource *r, int per_turn);
void carbon_resource_set_max(Carbon_Resource *r, int maximum);

// Calculate how much would be gained next tick
int carbon_resource_preview_tick(const Carbon_Resource *r);

#endif // CARBON_RESOURCE_H
