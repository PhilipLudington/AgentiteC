#ifndef CARBON_MODIFIER_H
#define CARBON_MODIFIER_H

#include <stdbool.h>

#define CARBON_MODIFIER_MAX 32

// Named modifier source (for UI display/debugging)
typedef struct Carbon_Modifier {
    char source[32];        // E.g., "policy_renewable", "tech_efficiency"
    float value;            // Multiplier delta: 0.1 = +10%, -0.05 = -5%
} Carbon_Modifier;

// Stack of modifiers
typedef struct Carbon_ModifierStack {
    Carbon_Modifier modifiers[CARBON_MODIFIER_MAX];
    int count;
} Carbon_ModifierStack;

// Initialize empty stack
void carbon_modifier_init(Carbon_ModifierStack *stack);

// Add/remove modifiers
bool carbon_modifier_add(Carbon_ModifierStack *stack, const char *source, float value);
bool carbon_modifier_remove(Carbon_ModifierStack *stack, const char *source);
bool carbon_modifier_has(const Carbon_ModifierStack *stack, const char *source);

// Update existing modifier value
bool carbon_modifier_set(Carbon_ModifierStack *stack, const char *source, float value);

// Calculate final value: base * (1 + mod1) * (1 + mod2) * ...
float carbon_modifier_apply(const Carbon_ModifierStack *stack, float base_value);

// Alternative: additive stacking: base * (1 + sum(modifiers))
float carbon_modifier_apply_additive(const Carbon_ModifierStack *stack, float base_value);

// Get total modifier for display: e.g., "+15%" or "-8%"
float carbon_modifier_total(const Carbon_ModifierStack *stack);

// Clear all modifiers
void carbon_modifier_clear(Carbon_ModifierStack *stack);

// Iterate for UI display
int carbon_modifier_count(const Carbon_ModifierStack *stack);
const Carbon_Modifier *carbon_modifier_get(const Carbon_ModifierStack *stack, int index);

#endif // CARBON_MODIFIER_H
