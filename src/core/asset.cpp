/**
 * Agentite Engine - Asset Handle System Implementation
 */

#include "agentite/asset.h"
#include "agentite/error.h"

#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

/* ============================================================================
 * Constants and Handle Packing
 * ============================================================================ */

/* Handle layout: [generation:8][index:24] */
#define HANDLE_INDEX_BITS 24
#define HANDLE_GEN_BITS 8
#define HANDLE_INDEX_MASK ((1u << HANDLE_INDEX_BITS) - 1)
#define HANDLE_GEN_MASK ((1u << HANDLE_GEN_BITS) - 1)
#define HANDLE_GEN_SHIFT HANDLE_INDEX_BITS

/* Maximum assets = 2^24 - 1 (index 0 is reserved for invalid) */
#define MAX_ASSETS (HANDLE_INDEX_MASK)

/* Initial capacity for asset slots */
#define INITIAL_CAPACITY 64

/* Hash table load factor threshold for resize */
#define HASH_LOAD_FACTOR 0.75f

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Asset slot in the registry */
typedef struct AssetSlot {
    char *path;                  /* Heap-allocated path string (NULL if free) */
    void *data;                  /* Asset data pointer */
    Agentite_AssetType type;     /* Asset type */
    uint32_t refcount;           /* Reference count */
    uint8_t generation;          /* Generation counter for stale detection */
    uint32_t next_free;          /* Next free slot index (when in free list) */
} AssetSlot;

/* Hash table entry for path lookup */
typedef struct HashEntry {
    uint32_t hash;               /* Path hash (0 = empty slot) */
    uint32_t slot_index;         /* Index into slots array */
} HashEntry;

/* Asset registry structure */
struct Agentite_AssetRegistry {
    AssetSlot *slots;            /* Dynamic array of asset slots */
    size_t slot_count;           /* Number of slots in use (including free) */
    size_t slot_capacity;        /* Allocated slot capacity */
    size_t live_count;           /* Number of live (non-free) assets */

    uint32_t free_head;          /* Head of free slot list (0 = none) */

    HashEntry *hash_table;       /* Hash table for path → slot lookup */
    size_t hash_capacity;        /* Hash table size (power of 2) */
    size_t hash_count;           /* Number of entries in hash table */

    Agentite_AssetDestructor destructor;
    void *destructor_userdata;
};

/* ============================================================================
 * Hash Functions
 * ============================================================================ */

/* FNV-1a hash for strings */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 2166136261u;
    while (*str) {
        hash ^= (uint8_t)*str++;
        hash *= 16777619u;
    }
    /* Ensure hash is never 0 (reserved for empty slots) */
    return hash ? hash : 1;
}

/* ============================================================================
 * Handle Packing/Unpacking
 * ============================================================================ */

static Agentite_AssetHandle pack_handle(uint32_t index, uint8_t generation) {
    Agentite_AssetHandle h;
    h.value = (((uint32_t)generation & HANDLE_GEN_MASK) << HANDLE_GEN_SHIFT) |
              (index & HANDLE_INDEX_MASK);
    return h;
}

static uint32_t unpack_index(Agentite_AssetHandle handle) {
    return handle.value & HANDLE_INDEX_MASK;
}

static uint8_t unpack_generation(Agentite_AssetHandle handle) {
    return (uint8_t)((handle.value >> HANDLE_GEN_SHIFT) & HANDLE_GEN_MASK);
}

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/* Check if slot at index is valid for given handle */
static bool is_slot_valid(const Agentite_AssetRegistry *registry,
                          Agentite_AssetHandle handle) {
    if (!registry || handle.value == 0) return false;

    uint32_t index = unpack_index(handle);
    if (index == 0 || index >= registry->slot_count) return false;

    const AssetSlot *slot = &registry->slots[index];
    if (!slot->path) return false;  /* Slot is free */

    return slot->generation == unpack_generation(handle);
}

/* Grow slots array */
static bool grow_slots(Agentite_AssetRegistry *registry) {
    size_t new_capacity = registry->slot_capacity * 2;
    if (new_capacity > MAX_ASSETS) {
        new_capacity = MAX_ASSETS;
        if (registry->slot_count >= new_capacity) {
            agentite_set_error("asset: maximum asset count reached");
            return false;
        }
    }

    AssetSlot *new_slots = (AssetSlot *)realloc(
        registry->slots, new_capacity * sizeof(AssetSlot));
    if (!new_slots) {
        agentite_set_error("asset: failed to grow slot array");
        return false;
    }

    /* Zero-initialize new slots */
    memset(&new_slots[registry->slot_capacity], 0,
           (new_capacity - registry->slot_capacity) * sizeof(AssetSlot));

    registry->slots = new_slots;
    registry->slot_capacity = new_capacity;
    return true;
}

/* Grow and rehash the hash table */
static bool grow_hash_table(Agentite_AssetRegistry *registry) {
    size_t new_capacity = registry->hash_capacity * 2;
    HashEntry *new_table = (HashEntry *)calloc(new_capacity, sizeof(HashEntry));
    if (!new_table) {
        agentite_set_error("asset: failed to grow hash table");
        return false;
    }

    /* Rehash all existing entries */
    for (size_t i = 0; i < registry->hash_capacity; i++) {
        HashEntry *old_entry = &registry->hash_table[i];
        if (old_entry->hash == 0) continue;

        /* Find new slot using linear probing */
        size_t idx = old_entry->hash & (new_capacity - 1);
        while (new_table[idx].hash != 0) {
            idx = (idx + 1) & (new_capacity - 1);
        }
        new_table[idx] = *old_entry;
    }

    free(registry->hash_table);
    registry->hash_table = new_table;
    registry->hash_capacity = new_capacity;
    return true;
}

/* Insert path into hash table */
static bool hash_insert(Agentite_AssetRegistry *registry,
                        const char *path, uint32_t slot_index) {
    /* Check load factor */
    if ((float)(registry->hash_count + 1) / registry->hash_capacity > HASH_LOAD_FACTOR) {
        if (!grow_hash_table(registry)) return false;
    }

    uint32_t hash = hash_string(path);
    size_t idx = hash & (registry->hash_capacity - 1);

    /* Linear probing */
    while (registry->hash_table[idx].hash != 0) {
        idx = (idx + 1) & (registry->hash_capacity - 1);
    }

    registry->hash_table[idx].hash = hash;
    registry->hash_table[idx].slot_index = slot_index;
    registry->hash_count++;
    return true;
}

/* Find path in hash table, returns slot index or 0 if not found */
static uint32_t hash_find(const Agentite_AssetRegistry *registry, const char *path) {
    if (!registry->hash_table || registry->hash_count == 0) return 0;

    uint32_t hash = hash_string(path);
    size_t idx = hash & (registry->hash_capacity - 1);
    size_t start = idx;

    while (registry->hash_table[idx].hash != 0) {
        if (registry->hash_table[idx].hash == hash) {
            uint32_t slot_index = registry->hash_table[idx].slot_index;
            if (slot_index < registry->slot_count &&
                registry->slots[slot_index].path &&
                strcmp(registry->slots[slot_index].path, path) == 0) {
                return slot_index;
            }
        }
        idx = (idx + 1) & (registry->hash_capacity - 1);
        if (idx == start) break;  /* Full circle */
    }
    return 0;
}

/* Remove path from hash table */
static void hash_remove(Agentite_AssetRegistry *registry, const char *path) {
    if (!registry->hash_table || registry->hash_count == 0) return;

    uint32_t hash = hash_string(path);
    size_t idx = hash & (registry->hash_capacity - 1);
    size_t start = idx;

    while (registry->hash_table[idx].hash != 0) {
        if (registry->hash_table[idx].hash == hash) {
            uint32_t slot_index = registry->hash_table[idx].slot_index;
            if (slot_index < registry->slot_count &&
                registry->slots[slot_index].path &&
                strcmp(registry->slots[slot_index].path, path) == 0) {
                /* Found - mark as deleted and rehash following entries */
                registry->hash_table[idx].hash = 0;
                registry->hash_table[idx].slot_index = 0;
                registry->hash_count--;

                /* Rehash entries that may have been displaced */
                size_t next = (idx + 1) & (registry->hash_capacity - 1);
                while (registry->hash_table[next].hash != 0) {
                    HashEntry temp = registry->hash_table[next];
                    registry->hash_table[next].hash = 0;
                    registry->hash_table[next].slot_index = 0;
                    registry->hash_count--;

                    /* Reinsert */
                    uint32_t sidx = temp.slot_index;
                    if (sidx < registry->slot_count && registry->slots[sidx].path) {
                        hash_insert(registry, registry->slots[sidx].path, sidx);
                    }
                    next = (next + 1) & (registry->hash_capacity - 1);
                }
                return;
            }
        }
        idx = (idx + 1) & (registry->hash_capacity - 1);
        if (idx == start) break;
    }
}

/* Allocate a slot (from free list or by growing) */
static uint32_t allocate_slot(Agentite_AssetRegistry *registry) {
    /* Try free list first */
    if (registry->free_head != 0) {
        uint32_t index = registry->free_head;
        registry->free_head = registry->slots[index].next_free;
        return index;
    }

    /* Need a new slot */
    if (registry->slot_count >= registry->slot_capacity) {
        if (!grow_slots(registry)) return 0;
    }

    /* Slot 0 is reserved for invalid handle */
    if (registry->slot_count == 0) {
        registry->slot_count = 1;
    }

    return (uint32_t)registry->slot_count++;
}

/* Free a slot (add to free list) */
static void free_slot(Agentite_AssetRegistry *registry, uint32_t index) {
    AssetSlot *slot = &registry->slots[index];

    /* Increment generation to invalidate old handles */
    slot->generation = (slot->generation + 1) & HANDLE_GEN_MASK;

    /* Free path string */
    free(slot->path);
    slot->path = NULL;
    slot->data = NULL;
    slot->type = AGENTITE_ASSET_UNKNOWN;
    slot->refcount = 0;

    /* Add to free list */
    slot->next_free = registry->free_head;
    registry->free_head = index;

    registry->live_count--;
}

/* ============================================================================
 * Public API - Registry Lifecycle
 * ============================================================================ */

Agentite_AssetRegistry *agentite_asset_registry_create(void) {
    Agentite_AssetRegistry *registry = (Agentite_AssetRegistry *)calloc(
        1, sizeof(Agentite_AssetRegistry));
    if (!registry) {
        agentite_set_error("asset: failed to allocate registry");
        return NULL;
    }

    registry->slots = (AssetSlot *)calloc(INITIAL_CAPACITY, sizeof(AssetSlot));
    if (!registry->slots) {
        agentite_set_error("asset: failed to allocate slots");
        free(registry);
        return NULL;
    }
    registry->slot_capacity = INITIAL_CAPACITY;
    registry->slot_count = 1;  /* Reserve slot 0 for invalid */

    /* Hash table must be power of 2 */
    registry->hash_capacity = INITIAL_CAPACITY;
    registry->hash_table = (HashEntry *)calloc(
        registry->hash_capacity, sizeof(HashEntry));
    if (!registry->hash_table) {
        agentite_set_error("asset: failed to allocate hash table");
        free(registry->slots);
        free(registry);
        return NULL;
    }

    return registry;
}

void agentite_asset_registry_destroy(Agentite_AssetRegistry *registry) {
    if (!registry) return;

    /* Call destructor for all live assets if set */
    if (registry->destructor) {
        for (size_t i = 1; i < registry->slot_count; i++) {
            AssetSlot *slot = &registry->slots[i];
            if (slot->path && slot->data) {
                registry->destructor(slot->data, slot->type,
                                     registry->destructor_userdata);
            }
        }
    }

    /* Free all path strings */
    for (size_t i = 1; i < registry->slot_count; i++) {
        free(registry->slots[i].path);
    }

    free(registry->hash_table);
    free(registry->slots);
    free(registry);
}

/* ============================================================================
 * Public API - Registration
 * ============================================================================ */

void agentite_asset_set_destructor(Agentite_AssetRegistry *registry,
                                    Agentite_AssetDestructor destructor,
                                    void *userdata) {
    if (!registry) return;
    registry->destructor = destructor;
    registry->destructor_userdata = userdata;
}

Agentite_AssetHandle agentite_asset_register(Agentite_AssetRegistry *registry,
                                              const char *path,
                                              Agentite_AssetType type,
                                              void *data) {
    if (!registry || !path || path[0] == '\0') {
        agentite_set_error("asset: invalid parameters to register");
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    /* Check if already registered */
    uint32_t existing = hash_find(registry, path);
    if (existing != 0) {
        /* Increment refcount and return existing handle */
        AssetSlot *slot = &registry->slots[existing];
        slot->refcount++;
        return pack_handle(existing, slot->generation);
    }

    /* Allocate new slot */
    uint32_t index = allocate_slot(registry);
    if (index == 0) {
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    AssetSlot *slot = &registry->slots[index];

    /* Duplicate path string */
    slot->path = strdup(path);
    if (!slot->path) {
        agentite_set_error("asset: failed to duplicate path");
        free_slot(registry, index);
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    slot->data = data;
    slot->type = type;
    slot->refcount = 1;
    /* generation already set (preserved or initialized to 0) */

    /* Add to hash table */
    if (!hash_insert(registry, path, index)) {
        free(slot->path);
        slot->path = NULL;
        free_slot(registry, index);
        return AGENTITE_INVALID_ASSET_HANDLE;
    }

    registry->live_count++;
    return pack_handle(index, slot->generation);
}

void agentite_asset_unregister(Agentite_AssetRegistry *registry,
                                Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return;

    uint32_t index = unpack_index(handle);
    AssetSlot *slot = &registry->slots[index];

    /* Decrement refcount */
    if (slot->refcount > 0) {
        slot->refcount--;
    }

    /* If refcount hits zero, free the slot */
    if (slot->refcount == 0) {
        /* Call destructor if set */
        if (registry->destructor && slot->data) {
            registry->destructor(slot->data, slot->type,
                                 registry->destructor_userdata);
        }

        /* Remove from hash table */
        hash_remove(registry, slot->path);

        /* Free the slot */
        free_slot(registry, index);
    }
}

/* ============================================================================
 * Public API - Lookup
 * ============================================================================ */

Agentite_AssetHandle agentite_asset_lookup(const Agentite_AssetRegistry *registry,
                                            const char *path) {
    if (!registry || !path) return AGENTITE_INVALID_ASSET_HANDLE;

    uint32_t index = hash_find(registry, path);
    if (index == 0) return AGENTITE_INVALID_ASSET_HANDLE;

    const AssetSlot *slot = &registry->slots[index];
    return pack_handle(index, slot->generation);
}

bool agentite_asset_is_live(const Agentite_AssetRegistry *registry,
                             Agentite_AssetHandle handle) {
    return is_slot_valid(registry, handle);
}

void *agentite_asset_get_data(const Agentite_AssetRegistry *registry,
                               Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return NULL;
    return registry->slots[unpack_index(handle)].data;
}

Agentite_AssetType agentite_asset_get_type(const Agentite_AssetRegistry *registry,
                                            Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return AGENTITE_ASSET_UNKNOWN;
    return registry->slots[unpack_index(handle)].type;
}

const char *agentite_asset_get_path(const Agentite_AssetRegistry *registry,
                                     Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return NULL;
    return registry->slots[unpack_index(handle)].path;
}

/* ============================================================================
 * Public API - Reference Counting
 * ============================================================================ */

bool agentite_asset_addref(Agentite_AssetRegistry *registry,
                            Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return false;

    AssetSlot *slot = &registry->slots[unpack_index(handle)];
    slot->refcount++;
    return true;
}

bool agentite_asset_release(Agentite_AssetRegistry *registry,
                             Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return false;

    uint32_t index = unpack_index(handle);
    AssetSlot *slot = &registry->slots[index];

    if (slot->refcount == 0) return false;

    slot->refcount--;

    if (slot->refcount == 0) {
        /* Call destructor if set */
        if (registry->destructor && slot->data) {
            registry->destructor(slot->data, slot->type,
                                 registry->destructor_userdata);
        }

        /* Remove from hash table */
        hash_remove(registry, slot->path);

        /* Free the slot */
        free_slot(registry, index);
    }

    return true;
}

uint32_t agentite_asset_get_refcount(const Agentite_AssetRegistry *registry,
                                      Agentite_AssetHandle handle) {
    if (!is_slot_valid(registry, handle)) return 0;
    return registry->slots[unpack_index(handle)].refcount;
}

/* ============================================================================
 * Public API - Iteration
 * ============================================================================ */

size_t agentite_asset_count(const Agentite_AssetRegistry *registry) {
    if (!registry) return 0;
    return registry->live_count;
}

size_t agentite_asset_get_all(const Agentite_AssetRegistry *registry,
                               Agentite_AssetHandle *out_handles,
                               size_t max_count) {
    if (!registry || !out_handles || max_count == 0) return 0;

    size_t written = 0;
    for (size_t i = 1; i < registry->slot_count && written < max_count; i++) {
        const AssetSlot *slot = &registry->slots[i];
        if (slot->path) {  /* Live slot */
            out_handles[written++] = pack_handle((uint32_t)i, slot->generation);
        }
    }
    return written;
}

/* ============================================================================
 * Public API - Serialization Helpers
 * ============================================================================ */

const char *agentite_asset_type_name(Agentite_AssetType type) {
    switch (type) {
        case AGENTITE_ASSET_TEXTURE: return "texture";
        case AGENTITE_ASSET_SOUND:   return "sound";
        case AGENTITE_ASSET_MUSIC:   return "music";
        case AGENTITE_ASSET_FONT:    return "font";
        case AGENTITE_ASSET_PREFAB:  return "prefab";
        case AGENTITE_ASSET_SCENE:   return "scene";
        case AGENTITE_ASSET_DATA:    return "data";
        default:                     return "unknown";
    }
}

Agentite_AssetType agentite_asset_type_from_name(const char *name) {
    if (!name) return AGENTITE_ASSET_UNKNOWN;

    /* Case-insensitive comparison */
    if (SDL_strcasecmp(name, "texture") == 0) return AGENTITE_ASSET_TEXTURE;
    if (SDL_strcasecmp(name, "sound") == 0)   return AGENTITE_ASSET_SOUND;
    if (SDL_strcasecmp(name, "music") == 0)   return AGENTITE_ASSET_MUSIC;
    if (SDL_strcasecmp(name, "font") == 0)    return AGENTITE_ASSET_FONT;
    if (SDL_strcasecmp(name, "prefab") == 0)  return AGENTITE_ASSET_PREFAB;
    if (SDL_strcasecmp(name, "scene") == 0)   return AGENTITE_ASSET_SCENE;
    if (SDL_strcasecmp(name, "data") == 0)    return AGENTITE_ASSET_DATA;

    return AGENTITE_ASSET_UNKNOWN;
}
