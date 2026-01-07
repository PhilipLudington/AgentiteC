---
paths: "**/*.{c,h,cpp,hpp}"
---

# API Design Rules

## C-Style API Design

Carbide APIs use C-style design for maximum compatibility and clarity:

- **Free functions, not methods**: `player_get_health(player)` not `player->getHealth()`
- **Explicit object parameter**: First parameter is always the object being operated on
- **No function overloading**: Use distinct names (`vec2_add`, `vec3_add`, `vec2_add_scalar`)
- **No exceptions**: Use return values and `get_last_error()` for error reporting
- **No RAII**: Use explicit `_create`/`_destroy` or `_init`/`_deinit` pairs
- **No templates in public APIs**: Use macros or callbacks for generic operations
- **C linkage**: All public functions must be callable from C (`extern "C"`)
- **No default parameters**: Use config structs with `_DEFAULT` macros instead

```c
// GOOD: C-style
Player *player_create(const char *name);
int player_get_health(const Player *player);
void player_set_health(Player *player, int health);
void player_destroy(Player *player);

// BAD: C++ style
class Player {
    Player(const char *name);
    int getHealth() const;
    void setHealth(int health);
    ~Player();
};
```

## Config Structs

- Functions with 3+ parameters SHOULD use config structs
- Provide a `_DEFAULT` macro for zero-initialization with sensible defaults
- Accept NULL config pointer to use defaults

```c
typedef struct {
    int width;
    int height;
    bool fullscreen;
} WindowConfig;

#define WINDOW_CONFIG_DEFAULT { .width = 1280, .height = 720, .fullscreen = false }

Window *window_create(const WindowConfig *config);  // NULL = defaults
```

## Const Correctness

- Use `const` for parameters not modified by the function
- Use `const` for return values that shouldn't be modified
- Use `const` for pointers to read-only data
- Document when const correctness is intentionally violated

## Header Organization

- Use `#ifndef PROJECT_MODULE_H` guards (not `#pragma once`)
- All C headers MUST have `extern "C"` guards for C++ compatibility
- Include order: corresponding header, C stdlib, system, third-party, project

## Opaque Types

- Hide implementation behind forward declarations in public headers
- Full struct definition only in .c implementation files
- Use accessor functions for all field access

## Variadic Functions

- Avoid variadic functions in public APIs when possible
- If necessary, provide type-safe wrapper macros
- Always use format string checking: `__attribute__((format(printf, N, M)))`

## Output Parameters

- For functions returning multiple values, use output parameters
- Output parameters should be the last parameters
- Document ownership of output parameter contents
- Initialize output parameters to safe values on failure
