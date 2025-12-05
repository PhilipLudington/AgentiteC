# Modifier Stack System

Value modifiers with stacking, sources, and expiration.

## Quick Start

```c
#include "carbon/modifier.h"

Carbon_ModifierStack *stack = carbon_modifier_stack_create(100);  // Base value

// Add modifiers
carbon_modifier_add_flat(stack, 10, "buff_strength");      // +10 flat
carbon_modifier_add_percent(stack, 0.2f, "tech_bonus");    // +20%
carbon_modifier_add_multiply(stack, 1.5f, "weapon_bonus"); // x1.5

// Get final value
int32_t final = carbon_modifier_get_value(stack);  // 100 + 10 = 110, *1.2 = 132, *1.5 = 198
```

## Modifier Types

| Type | Description |
|------|-------------|
| `add_flat` | Add fixed amount (applied first) |
| `add_percent` | Add percentage of base (applied second) |
| `add_multiply` | Multiply result (applied last) |

## Timed Modifiers

```c
// Modifier that expires after 5 turns
carbon_modifier_add_flat_timed(stack, 20, "temporary_buff", 5);

// Update each turn
carbon_modifier_update(stack);
```

## Remove Modifiers

```c
// By source
carbon_modifier_remove_source(stack, "buff_strength");

// All modifiers
carbon_modifier_clear(stack);
```

## Query Modifiers

```c
int count = carbon_modifier_count(stack);
float total_flat = carbon_modifier_get_flat_total(stack);
float total_percent = carbon_modifier_get_percent_total(stack);
float total_multiply = carbon_modifier_get_multiply_total(stack);

// Check if source exists
if (carbon_modifier_has_source(stack, "tech_bonus")) { }
```

## Change Base Value

```c
carbon_modifier_set_base(stack, 150);
int32_t base = carbon_modifier_get_base(stack);
```
