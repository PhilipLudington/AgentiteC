# Resource Lifecycle Patterns

This document describes patterns for managing resources (files, handles, connections) safely.

## Core Principle: RAII in C

Resources should be acquired and released in matched pairs, with clear ownership.

---

## Pattern 1: Create/Destroy Pair

The fundamental pattern for resource management.

```c
// Create acquires resource
Resource *resource_create(void) {
    Resource *r = calloc(1, sizeof(Resource));
    if (!r) {
        set_error("Failed to allocate Resource");
        return NULL;
    }

    r->handle = acquire_system_resource();
    if (r->handle == INVALID_HANDLE) {
        set_error("Failed to acquire system resource");
        free(r);
        return NULL;
    }

    return r;
}

// Destroy releases resource
void resource_destroy(Resource *r) {
    if (!r) return;

    if (r->handle != INVALID_HANDLE) {
        release_system_resource(r->handle);
    }

    free(r);
}
```

**Guarantees:**
- `_create` returns valid object or NULL
- `_destroy` is always safe to call (even with NULL)
- No resource leaks on error paths

---

## Pattern 2: Scoped Operations (Begin/End)

For operations that must be paired.

```c
// Begin starts the operation
bool render_pass_begin(RenderPass *pass, const RenderTarget *target) {
    if (pass->active) {
        set_error("render_pass_begin called while already active");
        return false;
    }

    // Acquire GPU state
    pass->active = true;
    pass->target = target;
    gpu_begin_render_pass(target);

    return true;
}

// End completes the operation
void render_pass_end(RenderPass *pass) {
    if (!pass->active) {
        return;  // Already ended or never started
    }

    gpu_end_render_pass();
    pass->active = false;
    pass->target = NULL;
}

// Usage
if (render_pass_begin(&pass, &target)) {
    draw_sprites(&pass);
    draw_ui(&pass);
    render_pass_end(&pass);
}
```

**Enforcement:**
- Track active state to detect misuse
- `_end` is idempotent (safe to call multiple times)
- Consider: assert if `_end` called without `_begin`

---

## Pattern 3: Resource Manager

Centralize ownership of multiple resources.

```c
typedef struct {
    Texture **textures;
    size_t count;
    size_t capacity;
    char **names;  // For lookup
} TextureManager;

TextureManager *texture_manager_create(void) {
    TextureManager *m = calloc(1, sizeof(TextureManager));
    if (!m) return NULL;

    m->capacity = 64;
    m->textures = calloc(m->capacity, sizeof(Texture *));
    m->names = calloc(m->capacity, sizeof(char *));

    if (!m->textures || !m->names) {
        free(m->textures);
        free(m->names);
        free(m);
        return NULL;
    }

    return m;
}

void texture_manager_destroy(TextureManager *m) {
    if (!m) return;

    // Free all managed resources
    for (size_t i = 0; i < m->count; i++) {
        texture_destroy(m->textures[i]);
        free(m->names[i]);
    }

    free(m->textures);
    free(m->names);
    free(m);
}

// Load returns managed texture (manager owns it)
Texture *texture_manager_load(TextureManager *m, const char *path) {
    // Check if already loaded
    for (size_t i = 0; i < m->count; i++) {
        if (strcmp(m->names[i], path) == 0) {
            return m->textures[i];  // Return existing
        }
    }

    // Load new
    Texture *tex = texture_load(path);
    if (!tex) return NULL;

    // Add to manager
    if (m->count >= m->capacity) {
        // Grow arrays...
    }

    m->textures[m->count] = tex;
    m->names[m->count] = strdup(path);
    m->count++;

    return tex;
}
```

**Benefits:**
- Single cleanup point
- Automatic deduplication
- Easy resource tracking for debugging

---

## Pattern 4: Handle-Based Resources

Use integer handles instead of pointers for extra safety.

```c
typedef uint32_t TextureHandle;
#define INVALID_TEXTURE ((TextureHandle)0)

typedef struct {
    Texture *textures[MAX_TEXTURES];
    uint32_t generations[MAX_TEXTURES];  // Detect stale handles
    uint32_t next_free;
} TexturePool;

// Handle encodes index + generation
static TextureHandle make_handle(uint32_t index, uint32_t gen) {
    return (gen << 16) | (index & 0xFFFF);
}

static uint32_t handle_index(TextureHandle h) {
    return h & 0xFFFF;
}

static uint32_t handle_generation(TextureHandle h) {
    return h >> 16;
}

TextureHandle texture_pool_alloc(TexturePool *pool, const char *path) {
    uint32_t index = pool->next_free;
    if (index >= MAX_TEXTURES) {
        set_error("Texture pool full");
        return INVALID_TEXTURE;
    }

    Texture *tex = texture_load(path);
    if (!tex) return INVALID_TEXTURE;

    pool->textures[index] = tex;
    pool->generations[index]++;
    pool->next_free++;

    return make_handle(index, pool->generations[index]);
}

Texture *texture_pool_get(TexturePool *pool, TextureHandle handle) {
    if (handle == INVALID_TEXTURE) return NULL;

    uint32_t index = handle_index(handle);
    uint32_t gen = handle_generation(handle);

    if (index >= MAX_TEXTURES) return NULL;
    if (pool->generations[index] != gen) {
        // Handle is stale (texture was freed and slot reused)
        return NULL;
    }

    return pool->textures[index];
}
```

**Benefits:**
- Detects use-after-free (stale handles)
- Stable across reallocations
- Can be serialized (useful for save games)

---

## Pattern 5: Reference Counting

For resources shared between multiple owners.

```c
typedef struct {
    int ref_count;
    // ... resource data ...
} SharedResource;

SharedResource *shared_resource_create(void) {
    SharedResource *r = calloc(1, sizeof(SharedResource));
    if (!r) return NULL;

    r->ref_count = 1;  // Creator holds first reference
    return r;
}

void shared_resource_retain(SharedResource *r) {
    if (!r) return;
    r->ref_count++;
}

void shared_resource_release(SharedResource *r) {
    if (!r) return;

    r->ref_count--;
    if (r->ref_count <= 0) {
        // Last reference released - free resource
        free(r);
    }
}

// Usage
SharedResource *r = shared_resource_create();  // ref_count = 1
shared_resource_retain(r);                      // ref_count = 2
shared_resource_release(r);                     // ref_count = 1
shared_resource_release(r);                     // ref_count = 0, freed
```

**Caution:**
- Reference counting is error-prone
- Prefer single ownership when possible
- Watch for cycles (A holds B, B holds A)

---

## Pattern 6: Deferred Cleanup

Batch cleanup for better performance.

```c
typedef struct {
    void **pending;
    size_t count;
    size_t capacity;
} DeferredCleanup;

void cleanup_defer(DeferredCleanup *c, void *resource) {
    if (c->count >= c->capacity) {
        // Grow array...
    }
    c->pending[c->count++] = resource;
}

void cleanup_flush(DeferredCleanup *c) {
    for (size_t i = 0; i < c->count; i++) {
        free(c->pending[i]);
    }
    c->count = 0;
}

// Usage - batch deletes for GPU resources
void entity_destroy(Entity *e, DeferredCleanup *cleanup) {
    // Don't free GPU resources immediately (might be in use)
    cleanup_defer(cleanup, e->texture);
    cleanup_defer(cleanup, e->mesh);
    free(e);
}

// At end of frame, when GPU is idle
cleanup_flush(&frame_cleanup);
```

---

## Pattern 7: Arena Allocator

Allocate many objects, free all at once.

```c
typedef struct {
    char *memory;
    size_t size;
    size_t used;
} Arena;

Arena *arena_create(size_t size) {
    Arena *a = malloc(sizeof(Arena));
    if (!a) return NULL;

    a->memory = malloc(size);
    if (!a->memory) {
        free(a);
        return NULL;
    }

    a->size = size;
    a->used = 0;
    return a;
}

void *arena_alloc(Arena *a, size_t size) {
    // Align to 8 bytes
    size = (size + 7) & ~7;

    if (a->used + size > a->size) {
        return NULL;  // Out of space
    }

    void *ptr = a->memory + a->used;
    a->used += size;
    return ptr;
}

void arena_reset(Arena *a) {
    a->used = 0;  // "Free" all allocations
}

void arena_destroy(Arena *a) {
    if (!a) return;
    free(a->memory);
    free(a);
}

// Usage - per-frame allocations
Arena *frame_arena = arena_create(1024 * 1024);  // 1MB

void game_update(void) {
    // Allocate temporary data
    TempData *data = arena_alloc(frame_arena, sizeof(TempData));
    // ... use data ...

    // At end of frame, reset (no individual frees needed)
    arena_reset(frame_arena);
}
```

**Benefits:**
- Very fast allocation (just bump pointer)
- No fragmentation
- Single reset frees everything

---

## Pattern 8: File Resource Wrapper

RAII-style file handling.

```c
typedef struct {
    FILE *file;
    char *path;
    bool owned;
} FileHandle;

FileHandle file_open(const char *path, const char *mode) {
    FileHandle h = {0};

    h.file = fopen(path, mode);
    if (!h.file) {
        set_error("Failed to open '%s': %s", path, strerror(errno));
        return h;
    }

    h.path = strdup(path);
    h.owned = true;
    return h;
}

void file_close(FileHandle *h) {
    if (!h) return;

    if (h->file && h->owned) {
        fclose(h->file);
    }
    free(h->path);

    h->file = NULL;
    h->path = NULL;
    h->owned = false;
}

bool file_is_valid(const FileHandle *h) {
    return h && h->file != NULL;
}

// Usage
FileHandle f = file_open("data.txt", "r");
if (!file_is_valid(&f)) {
    return false;
}

// ... use f.file ...

file_close(&f);  // Always closes, even if file was invalid
```

---

## Anti-Patterns to Avoid

### 1. Unmatched Acquire/Release

```c
// BAD: Easy to forget close
FILE *f = fopen(path, "r");
if (condition) {
    return;  // File leaked!
}
fclose(f);
```

### 2. Double Release

```c
// BAD: Double free
resource_destroy(r);
resource_destroy(r);  // Undefined behavior!

// GOOD: NULL-safe destroy
void resource_destroy(Resource *r) {
    if (!r) return;
    // ...
}
```

### 3. Use After Release

```c
// BAD: Use after free
resource_destroy(r);
resource_do_something(r);  // Undefined behavior!

// GOOD: NULL after destroy
resource_destroy(r);
r = NULL;
```

### 4. Forgetting to Check Acquire

```c
// BAD: No check
Resource *r = resource_create();
resource_use(r);  // Crash if create failed!

// GOOD: Always check
Resource *r = resource_create();
if (!r) {
    return false;
}
```

---

## Checklist

Before submitting code:

- [ ] Every resource acquire has matching release
- [ ] Error paths release acquired resources
- [ ] Destroy functions are NULL-safe
- [ ] No use-after-free patterns
- [ ] No double-free patterns
- [ ] Resources are released in reverse order of acquisition
- [ ] Scoped operations track active state
