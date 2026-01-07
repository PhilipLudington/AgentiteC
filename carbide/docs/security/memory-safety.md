# Memory Safety

Memory safety vulnerabilities are the most common source of security issues in C programs. This document covers detection and prevention.

## Categories of Memory Safety Issues

1. **Buffer Overflow** - Writing beyond allocated bounds
2. **Use After Free** - Accessing freed memory
3. **Double Free** - Freeing memory twice
4. **Memory Leak** - Not freeing allocated memory
5. **Null Pointer Dereference** - Accessing memory at address 0
6. **Uninitialized Memory** - Reading before writing
7. **Integer Overflow** - Arithmetic wraparound affecting memory operations

---

## Use After Free (CWE-416)

### The Vulnerability

```c
// VULNERABLE
Player *player = player_create();
player_destroy(player);
printf("Health: %d\n", player->health);  // Use after free!
```

**Consequences:**
- Crashes (if memory unmapped)
- Data corruption (if memory reused)
- **Exploitable** (attacker can control reused memory)

### Prevention

#### 1. NULL After Free

```c
void player_destroy(Player **player_ptr) {
    if (!player_ptr || !*player_ptr) return;

    Player *player = *player_ptr;
    free(player->name);
    free(player);

    *player_ptr = NULL;  // Prevent use after free
}

// Usage
Player *player = player_create();
player_destroy(&player);
// player is now NULL, dereferencing crashes immediately (easier to debug)
```

#### 2. Ownership Discipline

```c
// Clear documentation of ownership
/**
 * Takes ownership of 'data'. Caller must not use 'data' after this call.
 */
void container_add(Container *c, Data *data);

/**
 * Returns borrowed reference. Valid until container is modified or destroyed.
 */
const Data *container_get(const Container *c, size_t index);
```

#### 3. Reference Counting (When Shared)

```c
typedef struct {
    int ref_count;
    char *data;
} SharedBuffer;

SharedBuffer *buffer_create(const char *data) {
    SharedBuffer *buf = calloc(1, sizeof(SharedBuffer));
    if (!buf) return NULL;

    buf->ref_count = 1;
    buf->data = strdup(data);
    return buf;
}

void buffer_retain(SharedBuffer *buf) {
    if (buf) buf->ref_count++;
}

void buffer_release(SharedBuffer *buf) {
    if (!buf) return;
    buf->ref_count--;
    if (buf->ref_count <= 0) {
        free(buf->data);
        free(buf);
    }
}
```

---

## Double Free (CWE-415)

### The Vulnerability

```c
// VULNERABLE
char *data = malloc(100);
free(data);
free(data);  // Double free! Heap corruption
```

### Prevention

```c
// Pattern 1: NULL after free
free(data);
data = NULL;
free(data);  // Safe: free(NULL) is defined as no-op

// Pattern 2: NULL-safe destroy function
void safe_free(void **ptr) {
    if (ptr && *ptr) {
        free(*ptr);
        *ptr = NULL;
    }
}

// Pattern 3: Clear ownership semantics
// If function takes ownership, document it clearly
```

---

## Null Pointer Dereference (CWE-476)

### The Vulnerability

```c
// VULNERABLE
Player *player = get_player(id);  // Might return NULL
player->health = 100;  // Crash if player is NULL!
```

### Prevention

```c
// Pattern 1: Check immediately
Player *player = get_player(id);
if (!player) {
    set_error("Player %d not found", id);
    return false;
}
player->health = 100;  // Safe: player is guaranteed non-NULL

// Pattern 2: Early return in functions
void player_update(Player *player) {
    if (!player) return;  // Safe no-op

    player->x += player->vx;
    player->y += player->vy;
}

// Pattern 3: Assert for programming errors
void player_damage(Player *player, int amount) {
    assert(player != NULL);  // Bug if called with NULL
    player->health -= amount;
}
```

---

## Uninitialized Memory (CWE-457)

### The Vulnerability

```c
// VULNERABLE
Player player;  // Uninitialized!
if (player.health > 0) {  // Reading garbage
    // ...
}
```

### Prevention

```c
// Pattern 1: Zero-initialize
Player player = {0};

// Pattern 2: Use calloc instead of malloc
Player *player = calloc(1, sizeof(Player));

// Pattern 3: Initialize all fields explicitly
Player player;
player.health = 100;
player.x = 0;
player.y = 0;
player.name = NULL;

// Pattern 4: Init function
void player_init(Player *player) {
    memset(player, 0, sizeof(Player));
    player->health = 100;
}
```

---

## Integer Overflow in Memory Operations (CWE-190)

### The Vulnerability

```c
// VULNERABLE
size_t count = get_user_count();      // Could be huge
size_t size = count * sizeof(Item);   // OVERFLOW!
Item *items = malloc(size);           // Allocates wrong size
```

### Prevention

```c
// Pattern 1: Check before multiply
void *safe_array_alloc(size_t count, size_t element_size) {
    // Check for overflow
    if (count > 0 && element_size > SIZE_MAX / count) {
        set_error("Allocation would overflow");
        return NULL;
    }

    size_t size = count * element_size;
    if (size == 0) size = 1;  // malloc(0) is implementation-defined

    return calloc(1, size);  // calloc also checks internally
}

// Pattern 2: Use calloc (it checks for overflow)
Item *items = calloc(count, sizeof(Item));  // Safer than malloc(count * sizeof)

// Pattern 3: Limit input values
if (count > MAX_ITEMS) {
    set_error("Too many items: %zu (max %d)", count, MAX_ITEMS);
    return NULL;
}
```

---

## Memory Leaks (CWE-401)

### The Vulnerability

```c
// VULNERABLE - leak on error path
char *load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    char *buffer = malloc(1024);
    if (!buffer) return NULL;  // LEAK: f not closed!

    // ...
}
```

### Prevention

```c
// Pattern 1: Single cleanup point (goto)
char *load_file(const char *path) {
    FILE *f = NULL;
    char *buffer = NULL;
    char *result = NULL;

    f = fopen(path, "r");
    if (!f) goto cleanup;

    buffer = malloc(1024);
    if (!buffer) goto cleanup;

    // ... read file ...

    result = buffer;
    buffer = NULL;  // Transfer ownership to result

cleanup:
    free(buffer);
    if (f) fclose(f);
    return result;
}

// Pattern 2: RAII-style with nested functions
char *load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    char *result = load_file_content(f);  // Separate function

    fclose(f);  // Always closes
    return result;
}
```

---

## Detection Tools

### AddressSanitizer (ASan)

Detects buffer overflows, use-after-free, double-free.

```makefile
CFLAGS += -fsanitize=address -fno-omit-frame-pointer
LDFLAGS += -fsanitize=address
```

### UndefinedBehaviorSanitizer (UBSan)

Detects undefined behavior including integer overflow.

```makefile
CFLAGS += -fsanitize=undefined
LDFLAGS += -fsanitize=undefined
```

### MemorySanitizer (MSan)

Detects uninitialized memory reads. (Clang only, Linux only)

```makefile
CFLAGS += -fsanitize=memory -fno-omit-frame-pointer
LDFLAGS += -fsanitize=memory
```

### LeakSanitizer (LSan)

Detects memory leaks. Often bundled with ASan.

```makefile
CFLAGS += -fsanitize=leak
LDFLAGS += -fsanitize=leak
```

### Valgrind

Comprehensive memory checker (slower but thorough).

```bash
valgrind --leak-check=full --show-leak-kinds=all ./my_program
```

---

## Compiler Hardening Options

```makefile
# Stack protection
CFLAGS += -fstack-protector-strong

# Position Independent Executable (ASLR)
CFLAGS += -fPIE
LDFLAGS += -pie

# Non-executable stack
LDFLAGS += -Wl,-z,noexecstack

# Relocation read-only (GOT protection)
LDFLAGS += -Wl,-z,relro,-z,now

# FORTIFY_SOURCE (requires optimization)
CFLAGS += -D_FORTIFY_SOURCE=2 -O2
```

---

## Safe Coding Patterns Summary

### Allocation

```c
// 1. Always check allocation result
void *p = malloc(size);
if (!p) { /* handle error */ }

// 2. Prefer calloc for arrays (checks overflow, zeros memory)
int *arr = calloc(count, sizeof(int));

// 3. Check size calculations for overflow
if (count > SIZE_MAX / sizeof(Item)) { /* overflow */ }
```

### Deallocation

```c
// 1. NULL after free
free(p);
p = NULL;

// 2. NULL-safe destroy functions
void thing_destroy(Thing *t) {
    if (!t) return;
    free(t->data);
    free(t);
}

// 3. Clear ownership documentation
```

### Access

```c
// 1. Check for NULL before dereference
if (!p) return;

// 2. Check bounds before array access
if (index >= count) return;

// 3. Initialize before use
Type var = {0};
```

---

## Checklist

Before submitting code:

- [ ] All allocations checked for failure
- [ ] All memory initialized before use
- [ ] No use-after-free patterns
- [ ] No double-free patterns
- [ ] All error paths free resources
- [ ] Size calculations check for overflow
- [ ] Pointers NULLed after free
- [ ] NULL checks before dereference
- [ ] Tests run with sanitizers enabled
