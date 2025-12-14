# Carbide - AI Development Guide

This document helps Claude Code generate safe, consistent, and maintainable C/C++ code following Carbide standards.

## Quick Reference

### Before Writing Code

1. **Read STANDARDS.md** for naming conventions and patterns
2. **Check docs/patterns/** for the appropriate pattern to use
3. **Check docs/security/** if handling user input or external data

### Core Principles

1. **Explicit ownership**: Every pointer must have a clear owner
2. **No hidden allocations**: Allocation sites should be obvious
3. **Fail fast**: Check errors immediately, propagate clearly
4. **Validate at boundaries**: Trust internal code, validate external input

---

## Memory Management Rules

### Rule M1: Clear Ownership

Every pointer has exactly one owner. Document ownership in comments or naming.

```c
// GOOD: Ownership is clear
Player *player = player_create();  // Caller owns, must call player_destroy()
const char *name = player_get_name(player);  // Borrowed, valid while player exists

// BAD: Ownership unclear
char *get_data();  // Who frees this? Caller? Callee? Leaked?
```

### Rule M2: Create/Destroy Pairs

Resources that allocate must have corresponding deallocation.

```c
// GOOD: Matching pairs
Texture *texture_create(const char *path);
void texture_destroy(Texture *texture);

// GOOD: Init/deinit for caller-owned memory
void config_init(Config *config);
void config_deinit(Config *config);
```

### Rule M3: NULL Checks After Allocation

Always check allocation results.

```c
// GOOD
Player *player = malloc(sizeof(Player));
if (!player) {
    set_error("Failed to allocate player");
    return NULL;
}

// BAD: No check
Player *player = malloc(sizeof(Player));
player->health = 100;  // Crash if malloc failed
```

### Rule M4: Initialize All Memory

Never use uninitialized memory.

```c
// GOOD: Zero-initialize
Player *player = calloc(1, sizeof(Player));

// GOOD: Explicit initialization
Player player = {0};
player.health = 100;

// BAD: Uninitialized
Player player;
player.health = 100;  // Other fields are garbage
```

### Rule M5: No Use After Free

Set pointers to NULL after freeing.

```c
// GOOD
free(player);
player = NULL;

// GOOD: Destroy function handles this
void player_destroy(Player **player) {
    if (!player || !*player) return;
    free(*player);
    *player = NULL;
}
```

---

## Error Handling Rules

### Rule E1: Return Values Indicate Success

Use consistent return value patterns.

```c
// Pattern A: NULL means failure (for pointer-returning functions)
Texture *tex = texture_load("sprite.png");
if (!tex) {
    printf("Error: %s\n", get_last_error());
    return false;
}

// Pattern B: bool for success/failure
bool success = config_load(&config, "game.toml");
if (!success) {
    printf("Error: %s\n", get_last_error());
    return false;
}

// Pattern C: int error codes (0 = success, negative = error)
int result = network_connect(host, port);
if (result < 0) {
    printf("Error code: %d\n", result);
    return false;
}
```

### Rule E2: Thread-Local Error Messages

Use a thread-local error buffer for detailed messages.

```c
// Implementation
static _Thread_local char g_error_buffer[1024];
static _Thread_local bool g_has_error = false;

void set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(g_error_buffer, sizeof(g_error_buffer), fmt, args);
    va_end(args);
    g_has_error = true;
}

const char *get_last_error(void) {
    return g_has_error ? g_error_buffer : "";
}

void clear_error(void) {
    g_has_error = false;
    g_error_buffer[0] = '\0';
}
```

### Rule E3: Check Errors Immediately

Don't defer error checking.

```c
// GOOD: Immediate check
FILE *file = fopen(path, "r");
if (!file) {
    set_error("Failed to open file: %s", path);
    return NULL;
}

// BAD: Deferred check
FILE *file = fopen(path, "r");
// ... other code ...
if (!file) {  // Too late, may have already used file
```

### Rule E4: Propagate Errors Up

Don't swallow errors silently.

```c
// GOOD: Propagate
bool game_init(Game *game) {
    if (!graphics_init(&game->graphics)) {
        return false;  // Error already set by graphics_init
    }
    if (!audio_init(&game->audio)) {
        graphics_deinit(&game->graphics);  // Cleanup on failure
        return false;
    }
    return true;
}

// BAD: Swallowed error
bool game_init(Game *game) {
    graphics_init(&game->graphics);  // Ignoring return value!
    audio_init(&game->audio);
    return true;  // Always returns true even on failure
}
```

---

## API Design Rules

### Rule A1: Naming Conventions

Follow consistent naming patterns.

| Element | Convention | Example |
|---------|------------|---------|
| Types (structs, enums) | `PascalCase` | `PlayerState`, `TextureFormat` |
| Functions | `snake_case` | `player_create`, `texture_load` |
| Constants/Macros | `UPPER_SNAKE` | `MAX_PLAYERS`, `DEFAULT_WIDTH` |
| Local variables | `snake_case` | `player_count`, `is_valid` |
| Struct fields | `snake_case` | `health`, `max_speed` |

### Rule A2: Prefix Public APIs

All public symbols should have a consistent prefix.

```c
// GOOD: Prefixed
typedef struct MyLib_Player MyLib_Player;
MyLib_Player *mylib_player_create(void);
void mylib_player_destroy(MyLib_Player *player);

// BAD: No prefix (pollutes namespace)
typedef struct Player Player;
Player *player_create(void);
```

### Rule A3: Opaque Types for Encapsulation

Hide implementation details behind opaque pointers.

```c
// In public header (mylib/player.h)
typedef struct MyLib_Player MyLib_Player;  // Opaque

MyLib_Player *mylib_player_create(void);
void mylib_player_destroy(MyLib_Player *player);
int mylib_player_get_health(const MyLib_Player *player);
void mylib_player_set_health(MyLib_Player *player, int health);

// In implementation (player.c)
struct MyLib_Player {
    int health;
    int max_health;
    float position[3];
    // Users can't access these directly
};
```

### Rule A4: Config Structs with Defaults

Use config structs for functions with many parameters.

```c
// Define config with default macro
typedef struct {
    int width;
    int height;
    bool fullscreen;
    bool vsync;
    const char *title;
} WindowConfig;

#define WINDOW_CONFIG_DEFAULT { \
    .width = 1280, \
    .height = 720, \
    .fullscreen = false, \
    .vsync = true, \
    .title = "Carbide Game" \
}

// Usage
WindowConfig config = WINDOW_CONFIG_DEFAULT;
config.title = "My Game";  // Override only what you need
Window *window = window_create(&config);

// NULL config uses defaults
Window *window = window_create(NULL);
```

### Rule A5: Const Correctness

Use `const` to indicate read-only access.

```c
// GOOD: Const indicates no modification
int player_get_health(const Player *player);
void player_render(const Player *player, const Camera *camera);

// GOOD: Const for string parameters that aren't modified
Texture *texture_load(const char *path);
```

---

## Security Rules

### Rule S1: Validate All External Input

Never trust user input, file data, or network data.

```c
// GOOD: Validate bounds
bool load_player_data(const uint8_t *data, size_t size) {
    if (size < sizeof(PlayerHeader)) {
        set_error("Data too small for header");
        return false;
    }

    PlayerHeader header;
    memcpy(&header, data, sizeof(header));

    if (header.name_length > MAX_NAME_LENGTH) {
        set_error("Name too long: %zu", header.name_length);
        return false;
    }
    // ... continue with validated data
}
```

### Rule S2: Use Safe String Functions

Avoid unbounded string operations.

```c
// GOOD: Bounded operations
char buffer[256];
snprintf(buffer, sizeof(buffer), "Player: %s", name);

// GOOD: Safe copy
strncpy(dest, src, sizeof(dest) - 1);
dest[sizeof(dest) - 1] = '\0';

// BAD: Unbounded
sprintf(buffer, "Player: %s", name);  // Buffer overflow risk
strcpy(dest, src);  // Buffer overflow risk
```

### Rule S3: Check Array Bounds

Always validate indices before array access.

```c
// GOOD: Bounds check
int get_item(const Inventory *inv, size_t index) {
    if (index >= inv->count) {
        set_error("Index out of bounds: %zu >= %zu", index, inv->count);
        return -1;
    }
    return inv->items[index];
}

// BAD: No bounds check
int get_item(const Inventory *inv, size_t index) {
    return inv->items[index];  // Out of bounds = undefined behavior
}
```

### Rule S4: Avoid Integer Overflow

Check for overflow before arithmetic.

```c
// GOOD: Check before multiply
size_t safe_array_alloc(size_t count, size_t element_size) {
    if (count > 0 && element_size > SIZE_MAX / count) {
        set_error("Allocation would overflow");
        return 0;
    }
    return count * element_size;
}

// BAD: Overflow risk
size_t size = count * element_size;  // May overflow silently
```

---

## Common Patterns

### Pattern: Resource Manager

```c
typedef struct {
    Texture **textures;
    size_t count;
    size_t capacity;
} TextureManager;

TextureManager *texture_manager_create(void);
void texture_manager_destroy(TextureManager *manager);  // Frees all textures

Texture *texture_manager_load(TextureManager *manager, const char *path);
Texture *texture_manager_get(TextureManager *manager, const char *name);
```

### Pattern: Handle-Based Resources

```c
typedef uint32_t TextureHandle;
#define INVALID_TEXTURE_HANDLE 0

TextureHandle texture_create(TextureManager *mgr, const char *path);
void texture_destroy(TextureManager *mgr, TextureHandle handle);
bool texture_is_valid(TextureManager *mgr, TextureHandle handle);
```

### Pattern: Callbacks with User Data

```c
typedef void (*ButtonCallback)(void *user_data);

void button_set_callback(Button *button, ButtonCallback callback, void *user_data);

// Usage
void on_click(void *user_data) {
    Game *game = (Game *)user_data;
    game_start(game);
}
button_set_callback(start_button, on_click, game);
```

### Pattern: Builder/Fluent API

```c
SpriteBuilder *sprite_builder_create(void);
SpriteBuilder *sprite_builder_texture(SpriteBuilder *b, Texture *tex);
SpriteBuilder *sprite_builder_position(SpriteBuilder *b, float x, float y);
SpriteBuilder *sprite_builder_scale(SpriteBuilder *b, float scale);
Sprite *sprite_builder_build(SpriteBuilder *b);  // Consumes builder

// Usage
Sprite *sprite = sprite_builder_build(
    sprite_builder_scale(
        sprite_builder_position(
            sprite_builder_texture(
                sprite_builder_create(), texture),
            100, 200),
        2.0f));
```

---

## Troubleshooting

### Problem: Segmentation Fault

1. Check for NULL pointer dereference
2. Check for use-after-free
3. Check array bounds
4. Run with AddressSanitizer: `make test SANITIZE=address`

### Problem: Memory Leak

1. Ensure every `_create` has matching `_destroy`
2. Check error paths - are resources freed on failure?
3. Run with LeakSanitizer: `make test SANITIZE=leak`

### Problem: Undefined Behavior

1. Check for uninitialized variables
2. Check for integer overflow
3. Check for signed integer UB (shift, divide by zero)
4. Run with UBSanitizer: `make test SANITIZE=undefined`

### Problem: Double Free

1. Set pointers to NULL after free
2. Use destroy functions that take pointer-to-pointer
3. Check ownership - is something being freed twice?

---

## File Organization

### Header File Template

```c
#ifndef MYLIB_MODULE_H
#define MYLIB_MODULE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Types
 * ============================================================ */

typedef struct MyLib_Thing MyLib_Thing;

typedef struct {
    int option_a;
    bool option_b;
} MyLib_ThingConfig;

#define MYLIB_THING_CONFIG_DEFAULT { \
    .option_a = 42, \
    .option_b = true \
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

MyLib_Thing *mylib_thing_create(const MyLib_ThingConfig *config);
void mylib_thing_destroy(MyLib_Thing *thing);

/* ============================================================
 * Operations
 * ============================================================ */

bool mylib_thing_do_something(MyLib_Thing *thing, int param);
int mylib_thing_get_value(const MyLib_Thing *thing);

#ifdef __cplusplus
}
#endif

#endif /* MYLIB_MODULE_H */
```

### Source File Template

```c
#include "mylib/module.h"
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Private Types
 * ============================================================ */

struct MyLib_Thing {
    int value;
    bool initialized;
};

/* ============================================================
 * Private Functions
 * ============================================================ */

static bool validate_config(const MyLib_ThingConfig *config) {
    if (config->option_a < 0) {
        set_error("option_a must be non-negative");
        return false;
    }
    return true;
}

/* ============================================================
 * Public Functions - Lifecycle
 * ============================================================ */

MyLib_Thing *mylib_thing_create(const MyLib_ThingConfig *config) {
    MyLib_ThingConfig default_config = MYLIB_THING_CONFIG_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    if (!validate_config(config)) {
        return NULL;
    }

    MyLib_Thing *thing = calloc(1, sizeof(MyLib_Thing));
    if (!thing) {
        set_error("Failed to allocate MyLib_Thing");
        return NULL;
    }

    thing->value = config->option_a;
    thing->initialized = true;

    return thing;
}

void mylib_thing_destroy(MyLib_Thing *thing) {
    if (!thing) return;
    free(thing);
}

/* ============================================================
 * Public Functions - Operations
 * ============================================================ */

bool mylib_thing_do_something(MyLib_Thing *thing, int param) {
    if (!thing) {
        set_error("thing is NULL");
        return false;
    }
    thing->value += param;
    return true;
}

int mylib_thing_get_value(const MyLib_Thing *thing) {
    if (!thing) return 0;
    return thing->value;
}
```
