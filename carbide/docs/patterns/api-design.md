# API Design Patterns

This document describes patterns for designing clear, usable C APIs.

## Core Principle: Explicit is Better Than Implicit

Users should understand what a function does from its signature and name alone.

---

## Pattern 1: Opaque Types

Hide implementation details behind forward declarations.

```c
// Public header (player.h)
#ifndef PLAYER_H
#define PLAYER_H

// Forward declaration - users can't see inside
typedef struct Player Player;

Player *player_create(const char *name);
void player_destroy(Player *player);

int player_get_health(const Player *player);
void player_set_health(Player *player, int health);

#endif

// Private implementation (player.c)
#include "player.h"

struct Player {
    char name[64];
    int health;
    int max_health;
    float position[3];
    // Users can't access these directly
};

Player *player_create(const char *name) {
    Player *p = calloc(1, sizeof(Player));
    if (!p) return NULL;

    if (name) {
        strncpy(p->name, name, sizeof(p->name) - 1);
    }
    p->health = 100;
    p->max_health = 100;

    return p;
}
```

**Benefits:**
- Implementation can change without breaking users
- Forces use of accessor functions
- Prevents accidental misuse

---

## Pattern 2: Config Structs with Defaults

Replace long parameter lists with configuration structs.

```c
// Define config struct
typedef struct {
    int width;
    int height;
    const char *title;
    bool fullscreen;
    bool vsync;
    int target_fps;
} WindowConfig;

// Provide default values
#define WINDOW_CONFIG_DEFAULT { \
    .width = 1280, \
    .height = 720, \
    .title = "Game", \
    .fullscreen = false, \
    .vsync = true, \
    .target_fps = 60 \
}

// Accept config pointer (NULL = defaults)
Window *window_create(const WindowConfig *config) {
    WindowConfig default_config = WINDOW_CONFIG_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    Window *w = calloc(1, sizeof(Window));
    if (!w) {
        set_error("Failed to allocate Window");
        return NULL;
    }

    w->width = config->width;
    w->height = config->height;
    // ...

    return w;
}

// Usage - defaults
Window *w = window_create(NULL);

// Usage - custom
WindowConfig config = WINDOW_CONFIG_DEFAULT;
config.width = 1920;
config.height = 1080;
config.fullscreen = true;
Window *w = window_create(&config);
```

**Rules:**
- Config is copied, not stored (caller can free immediately)
- NULL config uses sensible defaults
- Document each field's purpose and valid values

---

## Pattern 3: Consistent Naming

Follow predictable naming patterns so users can guess function names.

### Lifecycle Functions

| Pattern | Meaning | Example |
|---------|---------|---------|
| `_create` | Allocate and initialize | `player_create()` |
| `_destroy` | Cleanup and deallocate | `player_destroy()` |
| `_init` | Initialize existing memory | `config_init()` |
| `_deinit` | Cleanup without dealloc | `config_deinit()` |

### Accessor Functions

| Pattern | Meaning | Example |
|---------|---------|---------|
| `_get_X` | Read property X | `player_get_health()` |
| `_set_X` | Write property X | `player_set_health()` |
| `_is_X` | Boolean query | `player_is_alive()` |
| `_has_X` | Boolean query (collection) | `inventory_has_item()` |
| `_can_X` | Boolean capability query | `player_can_attack()` |

### Collection Functions

| Pattern | Meaning | Example |
|---------|---------|---------|
| `_add` | Add item | `list_add()` |
| `_remove` | Remove item | `list_remove()` |
| `_find` | Find item (may fail) | `map_find()` |
| `_get` | Get item (must exist) | `array_get()` |
| `_count` | Get number of items | `list_count()` |
| `_clear` | Remove all items | `list_clear()` |

### Resource Functions

| Pattern | Meaning | Example |
|---------|---------|---------|
| `_load` | Load from file | `texture_load()` |
| `_save` | Save to file | `config_save()` |
| `_begin` | Start scoped operation | `render_begin()` |
| `_end` | End scoped operation | `render_end()` |

---

## Pattern 4: Prefix All Public Symbols

Avoid name collisions with a consistent prefix.

```c
// All public symbols prefixed with "mylib_"
typedef struct MyLib_Player MyLib_Player;
typedef struct MyLib_World MyLib_World;

MyLib_Player *mylib_player_create(void);
MyLib_World *mylib_world_create(void);

// Macros too
#define MYLIB_VERSION_MAJOR 1
#define MYLIB_VERSION_MINOR 0
```

**Conventions:**
- Types: `PrefixName` (PascalCase)
- Functions: `prefix_name_action` (snake_case)
- Macros: `PREFIX_NAME` (UPPER_SNAKE_CASE)

---

## Pattern 5: Const Correctness

Use `const` to communicate intent and catch mistakes.

```c
// Read-only access to player
int player_get_health(const Player *player);
const char *player_get_name(const Player *player);

// Read-only camera, read-only player
void player_render(const Player *player, const Camera *camera);

// String is read-only (not modified)
bool config_load(Config *config, const char *path);

// Array of read-only items
void render_sprites(const Sprite *sprites, size_t count);
```

**Rules:**
- Use `const` for parameters not modified
- Use `const` return for borrowed data
- Prefer `const` over mutable when possible

---

## Pattern 6: Output Parameters

Return multiple values through output parameters.

```c
// Single output
bool player_get_position(const Player *player, Position *out_pos);

// Multiple outputs
bool player_get_position(const Player *player,
                         float *out_x,
                         float *out_y,
                         float *out_z);

// Implementation
bool player_get_position(const Player *player,
                         float *out_x,
                         float *out_y,
                         float *out_z) {
    if (!player) {
        set_error("player is NULL");
        return false;
    }

    // Allow NULL for outputs caller doesn't need
    if (out_x) *out_x = player->x;
    if (out_y) *out_y = player->y;
    if (out_z) *out_z = player->z;

    return true;
}
```

**Naming conventions:**
- `out_` prefix for output parameters
- Output parameters come last
- Check for NULL if optional

---

## Pattern 7: Callback Patterns

Allow users to customize behavior through callbacks.

```c
// Callback type with user data
typedef void (*ButtonCallback)(void *user_data);

// Register callback
void button_set_on_click(Button *button,
                        ButtonCallback callback,
                        void *user_data);

// Implementation stores both
struct Button {
    ButtonCallback on_click;
    void *on_click_data;
};

void button_set_on_click(Button *button,
                        ButtonCallback callback,
                        void *user_data) {
    if (!button) return;
    button->on_click = callback;
    button->on_click_data = user_data;
}

// Usage
void handle_start_click(void *user_data) {
    Game *game = (Game *)user_data;
    game_start(game);
}

button_set_on_click(start_button, handle_start_click, game);
```

**Always include user_data:**
```c
// BAD: No way to pass context
typedef void (*Callback)(int value);

// GOOD: User data allows context
typedef void (*Callback)(int value, void *user_data);
```

---

## Pattern 8: Versioned Structs

Allow forward-compatible struct changes.

```c
typedef struct {
    size_t struct_size;  // Always first field
    int width;
    int height;
    // New fields added here in future versions
} WindowConfig;

Window *window_create(const WindowConfig *config) {
    if (config && config->struct_size < sizeof(WindowConfig)) {
        // Config from older version - use defaults for new fields
    }
    // ...
}

// Usage - always set struct_size
WindowConfig config = {
    .struct_size = sizeof(WindowConfig),
    .width = 1280,
    .height = 720,
};
```

---

## Pattern 9: Header Organization

Structure headers for clarity.

```c
#ifndef MYLIB_PLAYER_H
#define MYLIB_PLAYER_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================
 * Types
 * ============================================================ */

typedef struct MyLib_Player MyLib_Player;

typedef struct {
    const char *name;
    int starting_health;
} MyLib_PlayerConfig;

#define MYLIB_PLAYER_CONFIG_DEFAULT { \
    .name = "Player", \
    .starting_health = 100 \
}

/* ============================================================
 * Lifecycle
 * ============================================================ */

MyLib_Player *mylib_player_create(const MyLib_PlayerConfig *config);
void mylib_player_destroy(MyLib_Player *player);

/* ============================================================
 * Properties
 * ============================================================ */

const char *mylib_player_get_name(const MyLib_Player *player);
int mylib_player_get_health(const MyLib_Player *player);
void mylib_player_set_health(MyLib_Player *player, int health);

/* ============================================================
 * Actions
 * ============================================================ */

bool mylib_player_take_damage(MyLib_Player *player, int amount);
bool mylib_player_heal(MyLib_Player *player, int amount);

#ifdef __cplusplus
}
#endif

#endif /* MYLIB_PLAYER_H */
```

---

## Anti-Patterns to Avoid

### 1. Too Many Parameters

```c
// BAD: Hard to use correctly
Window *window_create(int w, int h, const char *title,
                     bool fs, bool vsync, int fps, int msaa);

// GOOD: Config struct
Window *window_create(const WindowConfig *config);
```

### 2. Boolean Blindness

```c
// BAD: What does true mean?
window_set_mode(window, true, false, true);

// GOOD: Named parameters via struct or enums
window_set_fullscreen(window, true);
window_set_vsync(window, false);
window_set_resizable(window, true);
```

### 3. Out Parameters for Simple Returns

```c
// BAD: Unnecessarily complex
void player_get_health(const Player *p, int *out_health);

// GOOD: Simple return
int player_get_health(const Player *p);
```

### 4. Inconsistent Naming

```c
// BAD: Inconsistent patterns
player_create();
CreateEnemy();      // Different style
makeWeapon();       // Different style
new_item();         // Different style

// GOOD: Consistent
player_create();
enemy_create();
weapon_create();
item_create();
```

---

## Checklist

Before publishing an API:

- [ ] All public types have project prefix
- [ ] All public functions have project prefix
- [ ] Naming follows consistent patterns
- [ ] Functions with 3+ params use config structs
- [ ] Config structs have DEFAULT macros
- [ ] `const` used appropriately
- [ ] Header has include guards
- [ ] Header has extern "C" guards
- [ ] Functions are documented
- [ ] Opaque types used where appropriate
