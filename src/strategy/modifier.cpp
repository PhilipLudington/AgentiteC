#include "carbon/modifier.h"
#include <string.h>

void carbon_modifier_init(Carbon_ModifierStack *stack) {
    if (!stack) return;
    memset(stack, 0, sizeof(Carbon_ModifierStack));
}

bool carbon_modifier_add(Carbon_ModifierStack *stack, const char *source, float value) {
    if (!stack || !source || stack->count >= CARBON_MODIFIER_MAX) return false;

    // Check for duplicate
    for (int i = 0; i < stack->count; i++) {
        if (strcmp(stack->modifiers[i].source, source) == 0) {
            return false;  // Already exists
        }
    }

    Carbon_Modifier *mod = &stack->modifiers[stack->count];
    strncpy(mod->source, source, sizeof(mod->source) - 1);
    mod->source[sizeof(mod->source) - 1] = '\0';
    mod->value = value;
    stack->count++;

    return true;
}

bool carbon_modifier_remove(Carbon_ModifierStack *stack, const char *source) {
    if (!stack || !source) return false;

    for (int i = 0; i < stack->count; i++) {
        if (strcmp(stack->modifiers[i].source, source) == 0) {
            // Shift remaining modifiers down
            for (int j = i; j < stack->count - 1; j++) {
                stack->modifiers[j] = stack->modifiers[j + 1];
            }
            stack->count--;
            return true;
        }
    }

    return false;
}

bool carbon_modifier_has(const Carbon_ModifierStack *stack, const char *source) {
    if (!stack || !source) return false;

    for (int i = 0; i < stack->count; i++) {
        if (strcmp(stack->modifiers[i].source, source) == 0) {
            return true;
        }
    }

    return false;
}

bool carbon_modifier_set(Carbon_ModifierStack *stack, const char *source, float value) {
    if (!stack || !source) return false;

    for (int i = 0; i < stack->count; i++) {
        if (strcmp(stack->modifiers[i].source, source) == 0) {
            stack->modifiers[i].value = value;
            return true;
        }
    }

    return false;
}

float carbon_modifier_apply(const Carbon_ModifierStack *stack, float base_value) {
    if (!stack || stack->count == 0) return base_value;

    float result = base_value;
    for (int i = 0; i < stack->count; i++) {
        result *= (1.0f + stack->modifiers[i].value);
    }

    return result;
}

float carbon_modifier_apply_additive(const Carbon_ModifierStack *stack, float base_value) {
    if (!stack || stack->count == 0) return base_value;

    float sum = 0.0f;
    for (int i = 0; i < stack->count; i++) {
        sum += stack->modifiers[i].value;
    }

    return base_value * (1.0f + sum);
}

float carbon_modifier_total(const Carbon_ModifierStack *stack) {
    if (!stack || stack->count == 0) return 0.0f;

    float sum = 0.0f;
    for (int i = 0; i < stack->count; i++) {
        sum += stack->modifiers[i].value;
    }

    return sum;
}

void carbon_modifier_clear(Carbon_ModifierStack *stack) {
    if (!stack) return;
    stack->count = 0;
}

int carbon_modifier_count(const Carbon_ModifierStack *stack) {
    if (!stack) return 0;
    return stack->count;
}

const Carbon_Modifier *carbon_modifier_get(const Carbon_ModifierStack *stack, int index) {
    if (!stack || index < 0 || index >= stack->count) return NULL;
    return &stack->modifiers[index];
}
