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

### 5.1 C-Style API Design

**RULE**: Carbide APIs use C-style design for maximum compatibility and clarity.

| Avoid (C++ Style) | Use (C Style) |
|-------------------|---------------|
| Methods (`obj.method()`) | Free functions (`module_action(obj)`) |
| Constructors/destructors | `_create`/`_destroy` functions |
| Function overloading | Distinct function names |
| Exceptions | Return values + `get_last_error()` |
| RAII | Explicit `_init`/`_deinit` pairs |
| Templates in public APIs | Macros or callbacks |
| Default parameters | Config structs with `_DEFAULT` macros |
| Implicit `this` | Explicit first parameter |

```c
// GOOD: C-style API
Player *player_create(const char *name);
int player_get_health(const Player *player);
void player_set_health(Player *player, int health);
void player_destroy(Player *player);

// BAD: C++ style API
class Player {
public:
    Player(const char *name);
    int getHealth() const;
    void setHealth(int health);
    ~Player();
};
```

**Rationale**: C-style APIs provide:
- Binary compatibility across compilers and languages
- Explicit resource management (no hidden allocations)
- Clear ownership semantics
- FFI compatibility (Python, Rust, etc.)

### 5.2 Config Structs

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

### 5.3 Const Correctness

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

### 5.4 Header Guards

**RULE**: Use `#ifndef` guards with format `PROJECT_MODULE_H`.

```c
#ifndef CARBIDE_PLAYER_H
#define CARBIDE_PLAYER_H

// ... content ...

#endif /* CARBIDE_PLAYER_H */
```

### 5.5 C++ Compatibility

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

## 10. Testing

### 10.1 Test Organization

**RULE**: One test file per module in `tests/test_<module>.c`.

```
tests/
├── test_player.c
├── test_inventory.c
└── test_network.c
```

### 10.2 Test Naming

**RULE**: Name test functions as `test_<module>_<function>_<scenario>`.

```c
void test_player_create_success(void);
void test_player_create_null_name(void);
void test_player_destroy_null_safe(void);
void test_buffer_write_overflow(void);
```

### 10.3 Required Test Categories

Every public function should have tests covering:

| Category | Description | Example |
|----------|-------------|---------|
| Happy path | Normal successful operation | Valid input returns expected output |
| Edge cases | Boundary conditions | Empty input, zero, max values |
| Error conditions | Invalid input handling | NULL pointers, invalid parameters |
| Resource cleanup | No leaks on any path | Error paths free allocations |

### 10.4 Test Structure

```c
void test_player_create_success(void) {
    // Arrange
    const char *name = "TestPlayer";

    // Act
    Player *player = player_create(name);

    // Assert
    ASSERT(player != NULL);
    ASSERT_STR_EQ(player_get_name(player), name);

    // Cleanup
    player_destroy(player);
}
```

### 10.5 Sanitizer Testing

**RULE**: CI builds MUST run tests with sanitizers.

```makefile
# AddressSanitizer - memory errors
test-asan: CFLAGS += -fsanitize=address
test-asan: test

# UndefinedBehaviorSanitizer - undefined behavior
test-ubsan: CFLAGS += -fsanitize=undefined
test-ubsan: test

# LeakSanitizer - memory leaks
test-leak: CFLAGS += -fsanitize=leak
test-leak: test
```

---

## 11. Concurrency

### 11.1 Thread Safety Documentation

**RULE**: Document thread safety of every public function.

```c
/**
 * Get the player's current health.
 * Thread-safe: Yes (read-only access)
 */
int player_get_health(const Player *player);

/**
 * Update player state for one frame.
 * Thread-safe: No (caller must hold player lock)
 */
void player_update(Player *player, float delta_time);
```

### 11.2 Mutex Rules

**RULE C1**: Always pair lock and unlock.

```c
void safe_increment(Counter *counter) {
    mtx_lock(&counter->mutex);
    counter->value++;
    mtx_unlock(&counter->mutex);
}
```

**RULE C2**: Release locks in reverse order of acquisition.

```c
// Acquire in order
mtx_lock(&lock_a);
mtx_lock(&lock_b);

// Release in reverse order
mtx_unlock(&lock_b);
mtx_unlock(&lock_a);
```

**RULE C3**: Never hold locks while calling callbacks or external functions.

**RULE C4**: Keep critical sections minimal.

### 11.3 Atomic Operations

**RULE**: Use `_Atomic` for simple shared variables.

```c
#include <stdatomic.h>

_Atomic int g_request_count = 0;

void handle_request(void) {
    atomic_fetch_add(&g_request_count, 1);
    // ...
}
```

### 11.4 Thread Creation

```c
typedef struct {
    int id;
    const char *name;
} ThreadArgs;

int thread_func(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    // Use args...
    free(args);  // Thread owns the args
    return 0;
}

bool start_worker(int id, const char *name) {
    // Heap-allocate args (not stack!)
    ThreadArgs *args = malloc(sizeof(ThreadArgs));
    if (!args) return false;

    args->id = id;
    args->name = name;

    thrd_t thread;
    if (thrd_create(&thread, thread_func, args) != thrd_success) {
        free(args);
        return false;
    }
    thrd_detach(thread);
    return true;
}
```

---

## 12. Preprocessor

### 12.1 Macro Hygiene

**RULE P1**: Parenthesize all macro parameters.

**RULE P2**: Parenthesize the entire macro expression.

```c
// GOOD
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SQUARE(x) ((x) * (x))

// BAD - operator precedence bugs
#define MAX(a, b) a > b ? a : b
#define SQUARE(x) x * x
```

**RULE P3**: Use `do { ... } while(0)` for multi-statement macros.

```c
// GOOD
#define SAFE_FREE(ptr) do { \
    free(ptr);              \
    (ptr) = NULL;           \
} while(0)

// BAD - breaks if/else
#define SAFE_FREE(ptr) { free(ptr); ptr = NULL; }
```

### 12.2 Side Effects

**RULE P4**: Never pass expressions with side effects to macros.

```c
// DANGER: i incremented twice!
int max = MAX(i++, j++);

// SAFE: Use inline function instead
static inline int max_int(int a, int b) {
    return a > b ? a : b;
}
```

### 12.3 When to Use Macros vs Functions

| Use Case | Recommendation |
|----------|---------------|
| Constants | `#define` or `enum` |
| Type-generic operations | Macro (with care) or `_Generic` |
| Type-safe operations | `static inline` function |
| Conditional compilation | `#ifdef` |
| Debug/logging helpers | Macro (for `__FILE__`, `__LINE__`) |

### 12.4 Conditional Compilation

```c
// Platform detection
#ifdef _WIN32
    #include "platform_win32.h"
#elif defined(__linux__)
    #include "platform_linux.h"
#elif defined(__APPLE__)
    #include "platform_macos.h"
#else
    #error "Unsupported platform"
#endif

// Feature flags
#ifdef ENABLE_DEBUG_LOGGING
    #define DEBUG_LOG(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
    #define DEBUG_LOG(fmt, ...) ((void)0)
#endif
```

---

## 13. Logging

### 13.1 Log Levels

| Level | Use For | Example |
|-------|---------|---------|
| ERROR | Failures preventing operation | "Failed to open config file" |
| WARN | Unexpected but recoverable | "Config missing, using defaults" |
| INFO | Significant events | "Server started on port 8080" |
| DEBUG | Troubleshooting details | "Parsed 150 entities from file" |
| TRACE | Detailed execution flow | "Entering player_update()" |

### 13.2 Log Message Format

**RULE**: Include context in log messages.

```c
// GOOD: Includes relevant context
LOG_ERROR("player: failed to load (path=%s, error=%s)", path, get_last_error());
LOG_INFO("server: client connected (addr=%s, id=%u)", client_addr, client_id);
LOG_DEBUG("inventory: added item (player_id=%u, item=%s, count=%d)", pid, item, count);

// BAD: No context
LOG_ERROR("Load failed");
LOG_INFO("Connected");
```

### 13.3 Logging Rules

**RULE L1**: Never log secrets (passwords, tokens, API keys).

```c
// BAD
LOG_DEBUG("Authenticating user=%s password=%s", user, password);

// GOOD
LOG_DEBUG("Authenticating user=%s", user);
```

**RULE L2**: Never log large data blobs.

**RULE L3**: Avoid logging in tight loops.

```c
// BAD: Logs millions of times
for (size_t i = 0; i < million; i++) {
    LOG_DEBUG("Processing item %zu", i);
    process(items[i]);
}

// GOOD: Log summary
LOG_DEBUG("Processing %zu items", million);
for (size_t i = 0; i < million; i++) {
    process(items[i]);
}
LOG_DEBUG("Finished processing");
```

### 13.4 Log Implementation Pattern

```c
typedef enum {
    LOG_LEVEL_ERROR = 0,
    LOG_LEVEL_WARN,
    LOG_LEVEL_INFO,
    LOG_LEVEL_DEBUG,
    LOG_LEVEL_TRACE
} LogLevel;

extern LogLevel g_log_level;

#define LOG_ERROR(fmt, ...) log_write(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  log_write(LOG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  log_write(LOG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define LOG_DEBUG(fmt, ...) \
    do { if (g_log_level >= LOG_LEVEL_DEBUG) log_write(LOG_LEVEL_DEBUG, fmt, ##__VA_ARGS__); } while(0)
```

---

## 14. Portability

### 14.1 Integer Types

**RULE PT1**: Use fixed-width types for data structures and APIs.

```c
#include <stdint.h>

// GOOD: Explicit sizes
typedef struct {
    uint32_t id;
    int32_t x, y;
    uint16_t flags;
    uint8_t type;
} Entity;

// BAD: Platform-dependent sizes
typedef struct {
    unsigned int id;
    int x, y;
    unsigned short flags;
    unsigned char type;
} Entity;
```

**RULE PT2**: Use `size_t` for sizes and array indices.

**RULE PT3**: Use `ptrdiff_t` for pointer differences.

### 14.2 Byte Order

**RULE PT4**: Never assume host byte order for serialized data.

```c
#include <endian.h>  // Linux, or implement your own

void write_uint32_le(uint8_t *buf, uint32_t value) {
    buf[0] = (uint8_t)(value);
    buf[1] = (uint8_t)(value >> 8);
    buf[2] = (uint8_t)(value >> 16);
    buf[3] = (uint8_t)(value >> 24);
}

uint32_t read_uint32_le(const uint8_t *buf) {
    return (uint32_t)buf[0]
         | ((uint32_t)buf[1] << 8)
         | ((uint32_t)buf[2] << 16)
         | ((uint32_t)buf[3] << 24);
}
```

### 14.3 Path Handling

**RULE PT5**: Use forward slash `/` as path separator (works everywhere).

**RULE PT6**: Never hardcode absolute paths.

```c
// GOOD
const char *config_path = "data/config.toml";
char path[PATH_MAX];
snprintf(path, sizeof(path), "%s/%s", base_dir, filename);

// BAD
const char *config_path = "C:\\Program Files\\MyApp\\config.toml";
```

### 14.4 Platform Abstraction

**RULE**: Isolate platform-specific code.

```
src/
├── platform.h          # Common interface
├── platform_win32.c    # Windows implementation
├── platform_posix.c    # POSIX implementation
└── game.c              # Uses platform.h only
```

```c
// platform.h
uint64_t platform_get_time_ms(void);
bool platform_file_exists(const char *path);
void platform_sleep_ms(uint32_t ms);
```

### 14.5 Compiler Compatibility

**RULE**: Provide fallbacks for compiler extensions.

```c
#ifdef __GNUC__
    #define PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
    #define UNUSED __attribute__((unused))
    #define LIKELY(x) __builtin_expect(!!(x), 1)
#else
    #define PRINTF_FORMAT(fmt, args)
    #define UNUSED
    #define LIKELY(x) (x)
#endif

void log_write(LogLevel level, const char *fmt, ...) PRINTF_FORMAT(2, 3);
```

---

## 15. Quick Reference Card

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

### Testing
- One test file per module
- Test happy path + edge cases + errors
- Run sanitizers in CI

### Concurrency
- Document thread safety
- Lock/unlock in pairs
- Minimal critical sections

### Preprocessor
- Parenthesize macro args
- Use `do { } while(0)`
- Prefer inline functions

### Logging
- Include context
- Never log secrets
- Avoid logging in loops

### Portability
- Use fixed-width integers
- Handle byte order explicitly
- Abstract platform code

### Files
- Headers in `include/project/`
- Sources in `src/`
- Tests in `tests/`
