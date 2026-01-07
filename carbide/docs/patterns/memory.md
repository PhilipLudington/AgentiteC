# Memory Management Patterns

This document describes memory management patterns for safe, predictable C code.

## Core Principle: Explicit Ownership

Every pointer has exactly one owner. The owner is responsible for freeing the memory.

---

## Pattern 1: Create/Destroy

Use for objects that manage their own memory.

```c
// Header
typedef struct Player Player;  // Opaque type

Player *player_create(const char *name);
void player_destroy(Player *player);

// Implementation
struct Player {
    char *name;
    int health;
};

Player *player_create(const char *name) {
    Player *p = calloc(1, sizeof(Player));
    if (!p) {
        set_error("Failed to allocate Player");
        return NULL;
    }

    if (name) {
        p->name = strdup(name);
        if (!p->name) {
            set_error("Failed to allocate name");
            free(p);
            return NULL;
        }
    }

    p->health = 100;
    return p;
}

void player_destroy(Player *player) {
    if (!player) return;  // Safe to call with NULL
    free(player->name);
    free(player);
}
```

**Rules:**
- `_create` allocates and initializes
- `_destroy` frees all owned resources
- `_destroy` must be NULL-safe
- Caller is responsible for calling `_destroy`

---

## Pattern 2: Init/Deinit

Use for objects with caller-provided memory.

```c
// Header
typedef struct {
    char name[64];
    int health;
} Player;

bool player_init(Player *player, const char *name);
void player_deinit(Player *player);

// Implementation
bool player_init(Player *player, const char *name) {
    if (!player) {
        set_error("player is NULL");
        return false;
    }

    memset(player, 0, sizeof(Player));

    if (name) {
        strncpy(player->name, name, sizeof(player->name) - 1);
    }
    player->health = 100;

    return true;
}

void player_deinit(Player *player) {
    if (!player) return;
    // Free any dynamically allocated fields
    // (none in this example)
    memset(player, 0, sizeof(Player));  // Clear for safety
}
```

**When to use:**
- Embedded systems (no heap)
- Stack allocation
- Array of objects
- Object pools

```c
// Usage
Player players[10];
for (int i = 0; i < 10; i++) {
    player_init(&players[i], "Player");
}
// No destroy needed - stack allocated
```

---

## Pattern 3: Borrowed References

Return pointers that remain valid only while the source object exists.

```c
// Header
const char *player_get_name(const Player *player);
Weapon *player_get_weapon(Player *player);

// Implementation
const char *player_get_name(const Player *player) {
    if (!player) return NULL;
    return player->name;  // Borrowed - valid while player exists
}

Weapon *player_get_weapon(Player *player) {
    if (!player) return NULL;
    return &player->weapon;  // Borrowed - valid while player exists
}
```

**Rules:**
- Document lifetime: "valid while X exists"
- Use `const` for read-only borrows
- Never free borrowed pointers

---

## Pattern 4: Transfer Ownership

Explicitly transfer ownership from caller to callee.

```c
// Header
// Takes ownership of 'texture' - caller must not free
void sprite_set_texture(Sprite *sprite, Texture *texture);

// Implementation
void sprite_set_texture(Sprite *sprite, Texture *texture) {
    if (!sprite) return;

    // Free old texture if we owned it
    if (sprite->texture) {
        texture_destroy(sprite->texture);
    }

    sprite->texture = texture;  // Take ownership
}
```

**Documentation is critical:**
```c
/**
 * Set sprite texture.
 *
 * @param sprite Target sprite
 * @param texture Texture to use. Ownership transfers to sprite.
 *                Caller must not free or use after this call.
 */
void sprite_set_texture(Sprite *sprite, Texture *texture);
```

---

## Pattern 5: Out Parameters

Caller provides pointer to receive data.

```c
// Header
bool player_get_position(const Player *player, float *out_x, float *out_y);

// Implementation
bool player_get_position(const Player *player, float *out_x, float *out_y) {
    if (!player) {
        set_error("player is NULL");
        return false;
    }

    if (out_x) *out_x = player->x;
    if (out_y) *out_y = player->y;

    return true;
}
```

**Rules:**
- Check `out` pointer if it might be NULL
- Document what gets written
- Return success/failure status

---

## Pattern 6: Resource Manager

Centralize ownership in a manager object.

```c
// Header
typedef struct TextureManager TextureManager;

TextureManager *texture_manager_create(void);
void texture_manager_destroy(TextureManager *manager);

// Load texture - manager owns it
Texture *texture_manager_load(TextureManager *manager, const char *path);

// Get existing texture - borrowed reference
Texture *texture_manager_get(TextureManager *manager, const char *name);

// Implementation
struct TextureManager {
    Texture **textures;
    size_t count;
    size_t capacity;
};

void texture_manager_destroy(TextureManager *manager) {
    if (!manager) return;

    // Free all managed textures
    for (size_t i = 0; i < manager->count; i++) {
        texture_destroy(manager->textures[i]);
    }
    free(manager->textures);
    free(manager);
}
```

**Benefits:**
- Single point of cleanup
- Easy to track leaks
- Batch operations possible

---

## Pattern 7: Double-Pointer Destroy

Set pointer to NULL after destroy.

```c
// Header
void player_destroy_ptr(Player **player);

// Implementation
void player_destroy_ptr(Player **player) {
    if (!player || !*player) return;

    free((*player)->name);
    free(*player);
    *player = NULL;  // Prevent use-after-free
}

// Usage
Player *p = player_create("Test");
player_destroy_ptr(&p);
// p is now NULL
```

---

## Anti-Patterns to Avoid

### 1. Ambiguous Ownership

```c
// BAD: Who frees this?
char *get_player_name(Player *player);

// GOOD: Clear ownership
const char *player_get_name(const Player *player);  // Borrowed
char *player_copy_name(const Player *player);       // Caller owns copy
```

### 2. Forgetting Cleanup on Error

```c
// BAD: Leaks 'a' on failure
bool init(Context *ctx) {
    ctx->a = create_a();
    ctx->b = create_b();
    if (!ctx->b) return false;  // 'a' leaked!
    return true;
}

// GOOD: Clean up on error
bool init(Context *ctx) {
    ctx->a = create_a();
    if (!ctx->a) return false;

    ctx->b = create_b();
    if (!ctx->b) {
        destroy_a(ctx->a);
        ctx->a = NULL;
        return false;
    }
    return true;
}
```

### 3. Using After Free

```c
// BAD: Use after free
player_destroy(player);
printf("Name: %s\n", player->name);  // Undefined behavior!

// GOOD: NULL after free
player_destroy(player);
player = NULL;
// Attempting to use player now crashes immediately (easier to debug)
```

### 4. Double Free

```c
// BAD: Double free
player_destroy(player);
player_destroy(player);  // Undefined behavior!

// GOOD: NULL-safe destroy
void player_destroy(Player *player) {
    if (!player) return;  // Safe to call multiple times if NULLed
    free(player);
}
```

---

## Checklist

Before submitting code, verify:

- [ ] Every `malloc`/`calloc` has a corresponding `free`
- [ ] Every `_create` function has a matching `_destroy`
- [ ] All `_destroy` functions are NULL-safe
- [ ] Ownership is documented for all pointer parameters/returns
- [ ] Error paths free allocated resources
- [ ] No use-after-free patterns
- [ ] No double-free patterns
