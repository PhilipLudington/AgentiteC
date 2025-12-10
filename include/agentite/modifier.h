#ifndef AGENTITE_MODIFIER_H
#define AGENTITE_MODIFIER_H

#include <stdbool.h>

#define AGENTITE_MODIFIER_MAX 32

// Named modifier source (for UI display/debugging)
typedef struct Agentite_Modifier {
    char source[32];        // E.g., "policy_renewable", "tech_efficiency"
    float value;            // Multiplier delta: 0.1 = +10%, -0.05 = -5%
} Agentite_Modifier;

// Stack of modifiers
typedef struct Agentite_ModifierStack {
    Agentite_Modifier modifiers[AGENTITE_MODIFIER_MAX];
    int count;
} Agentite_ModifierStack;

// Initialize empty stack
void agentite_modifier_init(Agentite_ModifierStack *stack);

// Add/remove modifiers
bool agentite_modifier_add(Agentite_ModifierStack *stack, const char *source, float value);
bool agentite_modifier_remove(Agentite_ModifierStack *stack, const char *source);
bool agentite_modifier_has(const Agentite_ModifierStack *stack, const char *source);

// Update existing modifier value
bool agentite_modifier_set(Agentite_ModifierStack *stack, const char *source, float value);

// Calculate final value: base * (1 + mod1) * (1 + mod2) * ...
float agentite_modifier_apply(const Agentite_ModifierStack *stack, float base_value);

// Alternative: additive stacking: base * (1 + sum(modifiers))
float agentite_modifier_apply_additive(const Agentite_ModifierStack *stack, float base_value);

// Get total modifier for display: e.g., "+15%" or "-8%"
float agentite_modifier_total(const Agentite_ModifierStack *stack);

// Clear all modifiers
void agentite_modifier_clear(Agentite_ModifierStack *stack);

// Iterate for UI display
int agentite_modifier_count(const Agentite_ModifierStack *stack);
const Agentite_Modifier *agentite_modifier_get(const Agentite_ModifierStack *stack, int index);

#endif // AGENTITE_MODIFIER_H
