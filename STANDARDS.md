# Carbide Coding Standards

This document defines the coding standards for Carbide projects. These rules are designed to be unambiguous and enforceable, enabling both humans and AI assistants to write consistent, safe code.

---

## 1. Language Standards

| Language | Standard | Flag |
|----------|----------|------|
| C | C11 | `-std=c11` |
| C++ | C++17 | `-std=c++17` |

**Rationale**: C11 provides thread-local storage, static assertions, and anonymous structs. C++17 adds structured bindings, `if constexpr`, and `std::optional`.

---

## 2. Naming Conventions

### 2.1 General Rules

| Element | Convention | Example |
|---------|------------|---------|
| Types (struct, enum, typedef) | `PascalCase` | `PlayerState`, `TextureFormat` |
| Functions | `snake_case` | `player_create`, `texture_load` |
| Variables (local, parameters) | `snake_case` | `player_count`, `is_valid` |
| Struct/union fields | `snake_case` | `health`, `max_speed` |
| Macros and constants | `UPPER_SNAKE_CASE` | `MAX_PLAYERS`, `PI` |
| Enum values | `UPPER_SNAKE_CASE` | `STATE_IDLE`, `STATE_RUNNING` |

### 2.2 Prefixing

**RULE**: All public symbols MUST have a project prefix.

```c
// Public API (header)
typedef struct Carbide_Player Carbide_Player;
Carbide_Player *carbide_player_create(void);

// Private/static symbols (source file) - no prefix required
static void update_position(Player *p);
```

### 2.3 Function Naming Patterns

| Pattern | Meaning | Example |
|---------|---------|---------|
| `_create` / `_destroy` | Allocate/deallocate resource | `player_create()`, `player_destroy()` |
| `_init` / `_deinit` | Initialize/cleanup caller-owned memory | `config_init()`, `config_deinit()` |
| `_load` / `_unload` | Load from external source | `texture_load()`, `level_unload()` |
| `_get_` / `_set_` | Accessor/mutator | `player_get_health()`, `player_set_health()` |
| `_is_` / `_has_` / `_can_` | Boolean query | `player_is_alive()`, `inventory_has_item()` |
| `_begin` / `_end` | Scoped operation pair | `batch_begin()`, `batch_end()` |
| `_add` / `_remove` | Collection modification | `list_add()`, `list_remove()` |
| `_find` / `_get` | Lookup (find may fail, get assumes valid) | `map_find()`, `array_get()` |

### 2.4 Naming Booleans

**RULE**: Boolean variables and functions MUST read as true/false statements.

```c
// GOOD
bool is_valid;
bool has_children;
bool can_attack;
bool player_is_alive(const Player *p);

// BAD
bool valid;      // "if (valid)" doesn't read well
bool children;   // Ambiguous - count or existence?
bool attack;     // Verb without is/can/has
```

---

## 3. Memory Management

### 3.1 Ownership Rules

**RULE**: Every pointer has exactly one owner. Ownership transfers are explicit.

| Return Type | Ownership | Caller Responsibility |
|-------------|-----------|----------------------|
| `T *func_create()` | Caller owns | Must call `func_destroy()` |
| `const T *func_get_*()` | Borrowed | Valid only while source lives |
| `T *func_get_*()` (mutable) | Borrowed | Valid only while source lives |
| `void func(T *out)` | Caller owns `out` | Caller allocated, caller frees |

### 3.2 Allocation Rules

**RULE M1**: Check all allocation results.
```c
void *ptr = malloc(size);
if (!ptr) { /* handle error */ }
```

**RULE M2**: Initialize all allocated memory.
```c
// Preferred: zero-initialize
Player *p = calloc(1, sizeof(Player));

// Alternative: explicit initialization
Player *p = malloc(sizeof(Player));
if (p) { memset(p, 0, sizeof(Player)); }
```

**RULE M3**: Match allocations with deallocations.
```c
// malloc/calloc/realloc → free
// fopen → fclose
// custom_create → custom_destroy
```

**RULE M4**: Set pointers to NULL after free.
```c
free(ptr);
ptr = NULL;
```

**RULE M5**: Destroy functions SHOULD accept NULL safely.
```c
void player_destroy(Player *player) {
    if (!player) return;  // Safe to call with NULL
    free(player->name);
    free(player);
}
```

### 3.3 Opaque Types

**RULE**: Hide implementation details behind opaque pointers.

```c
// Header: forward declaration only
typedef struct Player Player;

// Source: full definition
struct Player {
    int health;
    char name[64];
};
```

---

## 4. Error Handling

### 4.1 Return Value Conventions

| Return Type | Success | Failure |
|-------------|---------|---------|
| Pointer | Valid pointer | `NULL` |
| `bool` | `true` | `false` |
| `int` (status) | `0` | Negative error code |
| `size_t` / unsigned | Valid value | `0` or `SIZE_MAX` (context-dependent) |

### 4.2 Error Message Pattern

**RULE**: Use thread-local error buffer for detailed messages.

```c
// Required functions
void set_error(const char *fmt, ...);
const char *get_last_error(void);
bool has_error(void);
void clear_error(void);
```

### 4.3 Error Checking Rules

**RULE E1**: Check errors immediately after the call.
```c
FILE *f = fopen(path, "r");
if (!f) {
    set_error("Failed to open: %s", path);
    return NULL;
}
```

**RULE E2**: Propagate errors, don't swallow them.
```c
// GOOD: Propagate
if (!subsystem_init()) {
    return false;  // Caller sees failure
}

// BAD: Swallowed
subsystem_init();  // Ignoring return value
```

**RULE E3**: Clean up on error paths.
```c
bool init(Context *ctx) {
    ctx->a = create_a();
    if (!ctx->a) return false;

    ctx->b = create_b();
    if (!ctx->b) {
        destroy_a(ctx->a);  // Clean up a
        return false;
    }
    return true;
}
```

---

## 5. API Design

### 5.1 Config Structs

**RULE**: Functions with 3+ parameters SHOULD use config structs.

```c
typedef struct {
    int width;
    int height;
    bool fullscreen;
} WindowConfig;

#define WINDOW_CONFIG_DEFAULT { \
    .width = 1280, \
    .height = 720, \
    .fullscreen = false \
}

// Accept NULL for defaults
Window *window_create(const WindowConfig *config);
```

### 5.2 Const Correctness

**RULE**: Use `const` for:
- Parameters not modified by the function
- Return values that shouldn't be modified
- Pointers to read-only data

```c
// Parameter not modified
int player_get_health(const Player *player);

// String literal (read-only)
const char *get_version(void);

// Input buffer
bool parse_data(const uint8_t *data, size_t size);
```

### 5.3 Header Guards

**RULE**: Use `#ifndef` guards with format `PROJECT_MODULE_H`.

```c
#ifndef CARBIDE_PLAYER_H
#define CARBIDE_PLAYER_H

// ... content ...

#endif /* CARBIDE_PLAYER_H */
```

### 5.4 C++ Compatibility

**RULE**: All C headers MUST have extern "C" guards.

```c
#ifdef __cplusplus
extern "C" {
#endif

// ... declarations ...

#ifdef __cplusplus
}
#endif
```

---

## 6. Security

### 6.1 Input Validation

**RULE S1**: Validate all external input before use.
- File data
- Network data
- User input
- Environment variables

```c
bool load_data(const uint8_t *data, size_t size) {
    if (!data || size == 0) {
        set_error("Invalid input");
        return false;
    }
    if (size < HEADER_SIZE) {
        set_error("Data too small");
        return false;
    }
    // Now safe to process
}
```

### 6.2 Buffer Safety

**RULE S2**: Use bounded string functions.

| Unsafe | Safe Alternative |
|--------|------------------|
| `sprintf` | `snprintf` |
| `strcpy` | `strncpy` + null terminate, or `strlcpy` |
| `strcat` | `strncat` or `snprintf` |
| `gets` | `fgets` |
| `scanf("%s")` | `scanf("%Ns")` with width |

```c
// GOOD
char buf[64];
snprintf(buf, sizeof(buf), "Hello, %s", name);

// BAD
char buf[64];
sprintf(buf, "Hello, %s", name);  // Overflow risk
```

### 6.3 Integer Safety

**RULE S3**: Check for overflow before arithmetic.

```c
// Before multiplication
if (a > 0 && b > SIZE_MAX / a) {
    // Overflow would occur
}

// Before addition
if (a > SIZE_MAX - b) {
    // Overflow would occur
}
```

### 6.4 Array Bounds

**RULE S4**: Validate indices before array access.

```c
int array_get(const Array *arr, size_t index) {
    if (index >= arr->count) {
        set_error("Index out of bounds");
        return -1;
    }
    return arr->data[index];
}
```

---

## 7. Code Organization

### 7.1 File Structure

```
project/
├── include/project/    # Public headers
│   ├── module_a.h
│   └── module_b.h
├── src/                # Implementation
│   ├── module_a.c
│   └── module_b.c
├── tests/              # Test files
│   └── test_module_a.c
├── Makefile
└── README.md
```

### 7.2 Header File Order

```c
// 1. Corresponding header (for .c files)
#include "project/module.h"

// 2. C standard library
#include <stddef.h>
#include <stdbool.h>

// 3. System/platform headers
#include <sys/types.h>

// 4. Third-party library headers
#include <SDL3/SDL.h>

// 5. Project headers
#include "project/other.h"
```

### 7.3 Section Comments

**RULE**: Use section comments to organize code.

```c
/* ============================================================
 * Types
 * ============================================================ */

/* ============================================================
 * Private Functions
 * ============================================================ */

/* ============================================================
 * Public Functions
 * ============================================================ */
```

---

## 8. Compiler Warnings

### 8.1 Required Warning Flags

```makefile
CFLAGS += -Wall -Wextra -Wpedantic
```

### 8.2 Recommended Additional Warnings

```makefile
CFLAGS += -Wshadow -Wconversion -Wdouble-promotion
CFLAGS += -Wformat=2 -Wformat-security
CFLAGS += -Wnull-dereference -Wstack-protector
```

### 8.3 Treat Warnings as Errors (CI)

```makefile
CFLAGS += -Werror  # For CI builds
```

---

## 9. Documentation

### 9.1 Function Documentation

**RULE**: Document all public functions.

```c
/**
 * Create a new player with the given name.
 *
 * @param name Player name (copied, can be NULL for default)
 * @return New player, or NULL on failure (check get_last_error())
 */
Player *player_create(const char *name);
```

### 9.2 Required Documentation

- Purpose of the function
- Parameter descriptions (especially ownership)
- Return value meaning
- Error conditions

### 9.3 When to Comment

- **DO**: Document public APIs, complex algorithms, non-obvious decisions
- **DON'T**: Comment obvious code, restate what the code does

```c
// BAD: Restates code
i++;  // Increment i

// GOOD: Explains why
i++;  // Skip header row
```

---

## 10. Quick Reference Card

### Naming
- Types: `PascalCase`
- Functions: `snake_case`
- Macros: `UPPER_SNAKE_CASE`
- Prefix all public symbols

### Memory
- Check allocations
- Initialize memory
- Match create/destroy
- NULL after free

### Errors
- NULL = failure (pointers)
- false = failure (bool)
- Check immediately
- Clean up on error paths

### Security
- Validate external input
- Use bounded functions
- Check array bounds
- Prevent overflow

### Files
- Headers in `include/project/`
- Sources in `src/`
- Tests in `tests/`
