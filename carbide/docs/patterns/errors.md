# Error Handling Patterns

This document describes error handling patterns for robust, debuggable C code.

## Core Principle: Fail Fast, Fail Loud

Detect errors early, report them clearly, and propagate them immediately.

---

## Pattern 1: Thread-Local Error Buffer

Provides detailed error messages without complex return types.

```c
// Header (error.h)
#ifndef ERROR_H
#define ERROR_H

#include <stdbool.h>

void set_error(const char *fmt, ...);
const char *get_last_error(void);
bool has_error(void);
void clear_error(void);

#endif

// Implementation (error.c)
#include "error.h"
#include <stdarg.h>
#include <stdio.h>

#define ERROR_BUFFER_SIZE 1024

static _Thread_local char g_error_buffer[ERROR_BUFFER_SIZE];
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

bool has_error(void) {
    return g_has_error;
}

void clear_error(void) {
    g_has_error = false;
    g_error_buffer[0] = '\0';
}
```

**Usage:**
```c
Texture *texture_load(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        set_error("Failed to open texture file: %s", path);
        return NULL;
    }
    // ...
}

// Caller
Texture *tex = texture_load("sprite.png");
if (!tex) {
    fprintf(stderr, "Error: %s\n", get_last_error());
    return false;
}
```

---

## Pattern 2: Return Value Conventions

Use consistent return types to indicate success/failure.

### Pointer-Returning Functions

```c
// NULL = failure, non-NULL = success
Player *player_create(const char *name) {
    Player *p = calloc(1, sizeof(Player));
    if (!p) {
        set_error("Failed to allocate Player");
        return NULL;
    }
    return p;
}

// Usage
Player *p = player_create("Test");
if (!p) {
    // Handle error
}
```

### Boolean Functions

```c
// false = failure, true = success
bool config_load(Config *config, const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        set_error("Failed to open config: %s", path);
        return false;
    }
    // ...
    fclose(f);
    return true;
}

// Usage
if (!config_load(&config, "game.toml")) {
    // Handle error
}
```

### Integer Error Codes

```c
// 0 = success, negative = error code
typedef enum {
    ERR_OK = 0,
    ERR_INVALID_ARGUMENT = -1,
    ERR_OUT_OF_MEMORY = -2,
    ERR_FILE_NOT_FOUND = -3,
    ERR_NETWORK = -4,
} ErrorCode;

int network_connect(const char *host, int port) {
    if (!host) {
        set_error("host is NULL");
        return ERR_INVALID_ARGUMENT;
    }
    // ...
    return ERR_OK;
}

// Usage
int result = network_connect(host, port);
if (result != ERR_OK) {
    fprintf(stderr, "Connect failed (%d): %s\n", result, get_last_error());
}
```

---

## Pattern 3: Immediate Error Checking

Check errors right after the operation that can fail.

```c
// GOOD: Check immediately
FILE *f = fopen(path, "r");
if (!f) {
    set_error("Failed to open: %s", path);
    return NULL;
}
// f is guaranteed valid here

// BAD: Deferred check
FILE *f = fopen(path, "r");
char *data = read_all(f);  // Crash if f is NULL!
if (!f) {
    // Too late
}
```

---

## Pattern 4: Error Propagation

Propagate errors up the call stack, don't swallow them.

```c
bool game_init(Game *game, const GameConfig *config) {
    // Initialize subsystems, propagate failures

    if (!graphics_init(&game->graphics, config->width, config->height)) {
        // Error already set by graphics_init
        return false;
    }

    if (!audio_init(&game->audio)) {
        graphics_deinit(&game->graphics);
        return false;
    }

    if (!input_init(&game->input)) {
        audio_deinit(&game->audio);
        graphics_deinit(&game->graphics);
        return false;
    }

    return true;
}
```

---

## Pattern 5: Cleanup on Error (Goto Pattern)

Use `goto` for centralized cleanup in functions with multiple failure points.

```c
bool load_level(const char *path, Level *out_level) {
    FILE *f = NULL;
    char *buffer = NULL;
    Tile *tiles = NULL;
    bool success = false;

    f = fopen(path, "rb");
    if (!f) {
        set_error("Failed to open level: %s", path);
        goto cleanup;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    buffer = malloc(size);
    if (!buffer) {
        set_error("Failed to allocate buffer");
        goto cleanup;
    }

    if (fread(buffer, 1, size, f) != (size_t)size) {
        set_error("Failed to read level data");
        goto cleanup;
    }

    // Parse header
    LevelHeader *header = (LevelHeader *)buffer;
    if (header->magic != LEVEL_MAGIC) {
        set_error("Invalid level file format");
        goto cleanup;
    }

    tiles = calloc(header->width * header->height, sizeof(Tile));
    if (!tiles) {
        set_error("Failed to allocate tiles");
        goto cleanup;
    }

    // Parse tiles...
    // ...

    // Success - transfer ownership
    out_level->tiles = tiles;
    out_level->width = header->width;
    out_level->height = header->height;
    tiles = NULL;  // Prevent cleanup from freeing
    success = true;

cleanup:
    free(tiles);
    free(buffer);
    if (f) fclose(f);
    return success;
}
```

**Rules for goto cleanup:**
- Label must be at end of function
- All cleanup resources initialized to NULL/0 at start
- Set to NULL after transferring ownership
- Only jump forward, never backward

---

## Pattern 6: Assert for Invariants

Use assertions for conditions that should never be false.

```c
#include <assert.h>

void array_set(Array *arr, size_t index, int value) {
    // Precondition: caller must ensure valid index
    assert(arr != NULL);
    assert(index < arr->count);

    arr->data[index] = value;
}
```

**Rules:**
- Use `assert` for programming errors (bugs)
- Use error returns for runtime errors (user input, files)
- Assertions may be disabled in release builds (`NDEBUG`)

---

## Pattern 7: Result Struct

For functions that return both data and error information.

```c
typedef struct {
    bool ok;
    union {
        int value;
        struct {
            int code;
            const char *message;
        } error;
    };
} IntResult;

IntResult parse_int(const char *str) {
    if (!str || !*str) {
        return (IntResult){
            .ok = false,
            .error = { .code = -1, .message = "Empty string" }
        };
    }

    char *end;
    long value = strtol(str, &end, 10);

    if (*end != '\0') {
        return (IntResult){
            .ok = false,
            .error = { .code = -2, .message = "Invalid characters" }
        };
    }

    return (IntResult){ .ok = true, .value = (int)value };
}

// Usage
IntResult result = parse_int(input);
if (!result.ok) {
    printf("Parse error: %s\n", result.error.message);
} else {
    printf("Value: %d\n", result.value);
}
```

---

## Pattern 8: Error Context

Add context as errors propagate up.

```c
bool game_load(Game *game, const char *save_path) {
    if (!load_player_data(game, save_path)) {
        // Add context to existing error
        char context[256];
        snprintf(context, sizeof(context),
                 "Failed to load game from '%s': %s",
                 save_path, get_last_error());
        set_error("%s", context);
        return false;
    }
    return true;
}
```

---

## Anti-Patterns to Avoid

### 1. Ignoring Return Values

```c
// BAD: Ignoring return value
fread(buffer, 1, size, file);

// GOOD: Check return value
size_t read = fread(buffer, 1, size, file);
if (read != size) {
    set_error("Short read: expected %zu, got %zu", size, read);
    return false;
}
```

### 2. Swallowing Errors

```c
// BAD: Error swallowed
void init_all(void) {
    graphics_init();  // Might fail silently
    audio_init();     // Might fail silently
}

// GOOD: Propagate errors
bool init_all(void) {
    if (!graphics_init()) return false;
    if (!audio_init()) {
        graphics_deinit();
        return false;
    }
    return true;
}
```

### 3. Generic Error Messages

```c
// BAD: No useful information
set_error("Operation failed");

// GOOD: Specific, actionable
set_error("Failed to open config file '%s': %s", path, strerror(errno));
```

### 4. Error After Side Effects

```c
// BAD: Error after partial modification
bool set_values(Object *obj, int a, int b) {
    obj->a = a;  // Modified
    if (b < 0) {
        set_error("b must be non-negative");
        return false;  // obj->a already changed!
    }
    obj->b = b;
    return true;
}

// GOOD: Validate before modifying
bool set_values(Object *obj, int a, int b) {
    if (b < 0) {
        set_error("b must be non-negative");
        return false;
    }
    obj->a = a;
    obj->b = b;
    return true;
}
```

---

## Checklist

Before submitting code, verify:

- [ ] All function return values are checked
- [ ] Errors are propagated, not swallowed
- [ ] Error messages are specific and actionable
- [ ] Cleanup happens on all error paths
- [ ] No partial state on failure (validate before modify)
- [ ] Assertions used for invariants, errors for runtime issues
