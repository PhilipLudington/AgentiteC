#ifndef AGENTITE_RESOURCE_H
#define AGENTITE_RESOURCE_H

#include <stdbool.h>

// Single resource type (lightweight, can be used as ECS component)
typedef struct Agentite_Resource {
    int current;
    int maximum;            // 0 = unlimited
    int per_turn_base;
    float per_turn_modifier; // Multiplier (1.0 = normal)
} Agentite_Resource;

// Initialize with defaults
void agentite_resource_init(Agentite_Resource *r, int initial, int maximum, int per_turn);

// Per-turn tick (adds per_turn_base * per_turn_modifier, clamped to max)
void agentite_resource_tick(Agentite_Resource *r);

// Spending
bool agentite_resource_can_afford(const Agentite_Resource *r, int amount);
bool agentite_resource_spend(Agentite_Resource *r, int amount);

// Adding (respects maximum)
void agentite_resource_add(Agentite_Resource *r, int amount);

// Set values
void agentite_resource_set(Agentite_Resource *r, int value);
void agentite_resource_set_modifier(Agentite_Resource *r, float modifier);
void agentite_resource_set_per_turn(Agentite_Resource *r, int per_turn);
void agentite_resource_set_max(Agentite_Resource *r, int maximum);

// Calculate how much would be gained next tick
int agentite_resource_preview_tick(const Agentite_Resource *r);

#endif // AGENTITE_RESOURCE_H
