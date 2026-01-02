/**
 * Agentite Engine - Asset Handle System
 *
 * Provides stable asset references that survive save/load cycles. Assets are
 * identified by path strings and referenced via lightweight handles. Reference
 * counting manages asset lifetime automatically.
 *
 * Usage:
 *   Agentite_AssetRegistry *registry = agentite_asset_registry_create();
 *
 *   // Register assets (typically done by loaders)
 *   Agentite_AssetHandle tex_handle = agentite_asset_register(registry,
 *       "sprites/player.png", AGENTITE_ASSET_TEXTURE, texture_ptr);
 *
 *   // Look up by path
 *   Agentite_AssetHandle h = agentite_asset_lookup(registry, "sprites/player.png");
 *   if (agentite_asset_is_valid(h)) {
 *       Agentite_Texture *tex = agentite_asset_get_data(registry, h);
 *   }
 *
 *   // Serialization: get path from handle for save files
 *   const char *path = agentite_asset_get_path(registry, h);
 *
 *   agentite_asset_registry_destroy(registry);
 */

#ifndef AGENTITE_ASSET_H
#define AGENTITE_ASSET_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Asset Types
 * ============================================================================ */

/**
 * Asset type enumeration.
 * Used to categorize assets and ensure type-safe retrieval.
 */
typedef enum Agentite_AssetType {
    AGENTITE_ASSET_UNKNOWN = 0,
    AGENTITE_ASSET_TEXTURE,      /* Agentite_Texture* */
    AGENTITE_ASSET_SOUND,        /* Agentite_Sound* */
    AGENTITE_ASSET_MUSIC,        /* Agentite_Music* */
    AGENTITE_ASSET_FONT,         /* Agentite_Font* */
    AGENTITE_ASSET_PREFAB,       /* Agentite_Prefab* (future) */
    AGENTITE_ASSET_SCENE,        /* Agentite_Scene* (future) */
    AGENTITE_ASSET_DATA,         /* Generic data blob */
    AGENTITE_ASSET_TYPE_COUNT
} Agentite_AssetType;

/* ============================================================================
 * Asset Handle
 * ============================================================================ */

/**
 * Lightweight asset handle.
 *
 * Handles are small, copyable values that reference assets in the registry.
 * The generation counter detects use of stale/freed handles.
 *
 * Layout:
 *   - index (24 bits): Slot index in registry
 *   - generation (8 bits): Incremented on slot reuse to detect stale handles
 */
typedef struct Agentite_AssetHandle {
    uint32_t value;  /* Packed index + generation */
} Agentite_AssetHandle;

/** Invalid handle constant */
#define AGENTITE_INVALID_ASSET_HANDLE ((Agentite_AssetHandle){ 0 })

/**
 * Check if handle is valid (not null).
 * Does NOT check if handle is stale - use agentite_asset_is_live() for that.
 */
static inline bool agentite_asset_is_valid(Agentite_AssetHandle handle) {
    return handle.value != 0;
}

/**
 * Compare two handles for equality.
 */
static inline bool agentite_asset_handle_equals(Agentite_AssetHandle a,
                                                 Agentite_AssetHandle b) {
    return a.value == b.value;
}

/* ============================================================================
 * Asset Registry (opaque)
 * ============================================================================ */

typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;

/* ============================================================================
 * Registry Lifecycle
 * ============================================================================ */

/**
 * Create an asset registry.
 * Caller OWNS the returned pointer and MUST call agentite_asset_registry_destroy().
 *
 * @return New registry, or NULL on allocation failure.
 */
Agentite_AssetRegistry *agentite_asset_registry_create(void);

/**
 * Destroy an asset registry and release all tracked assets.
 * Safe to pass NULL.
 *
 * NOTE: This does NOT automatically destroy the underlying asset data (textures,
 * sounds, etc). Those must be destroyed separately before the registry, or a
 * destructor callback must be set.
 */
void agentite_asset_registry_destroy(Agentite_AssetRegistry *registry);

/* ============================================================================
 * Asset Registration
 * ============================================================================ */

/**
 * Destructor callback for automatic asset cleanup.
 * Called when asset reference count reaches zero (if set).
 *
 * @param data     The asset data pointer
 * @param type     Asset type
 * @param userdata User context passed to set_destructor
 */
typedef void (*Agentite_AssetDestructor)(void *data,
                                          Agentite_AssetType type,
                                          void *userdata);

/**
 * Set destructor callback for automatic asset cleanup.
 * The destructor is called when an asset's reference count reaches zero.
 *
 * @param registry  Asset registry
 * @param destructor Callback function (NULL to disable)
 * @param userdata   Context passed to destructor calls
 */
void agentite_asset_set_destructor(Agentite_AssetRegistry *registry,
                                    Agentite_AssetDestructor destructor,
                                    void *userdata);

/**
 * Register an asset with the registry.
 *
 * If an asset with the same path already exists, returns the existing handle
 * and increments its reference count. The `data` parameter is ignored in this case.
 *
 * @param registry Asset registry
 * @param path     Unique asset path (e.g., "sprites/player.png")
 * @param type     Asset type
 * @param data     Pointer to asset data (ownership depends on destructor setting)
 * @return Handle to the asset, or AGENTITE_INVALID_ASSET_HANDLE on failure
 */
Agentite_AssetHandle agentite_asset_register(Agentite_AssetRegistry *registry,
                                              const char *path,
                                              Agentite_AssetType type,
                                              void *data);

/**
 * Unregister an asset from the registry.
 * Decrements reference count. If count reaches zero and destructor is set,
 * the destructor is called.
 *
 * @param registry Asset registry
 * @param handle   Handle to unregister
 */
void agentite_asset_unregister(Agentite_AssetRegistry *registry,
                                Agentite_AssetHandle handle);

/* ============================================================================
 * Asset Lookup
 * ============================================================================ */

/**
 * Look up an asset by path.
 *
 * @param registry Asset registry
 * @param path     Asset path to look up
 * @return Handle if found, AGENTITE_INVALID_ASSET_HANDLE if not found
 */
Agentite_AssetHandle agentite_asset_lookup(const Agentite_AssetRegistry *registry,
                                            const char *path);

/**
 * Check if a handle refers to a live asset.
 * Returns false for invalid handles, stale handles, or freed assets.
 *
 * @param registry Asset registry
 * @param handle   Handle to check
 * @return true if handle refers to a live asset
 */
bool agentite_asset_is_live(const Agentite_AssetRegistry *registry,
                             Agentite_AssetHandle handle);

/**
 * Get asset data pointer.
 * Returns NULL if handle is invalid or stale.
 *
 * @param registry Asset registry
 * @param handle   Asset handle
 * @return Asset data pointer, or NULL
 */
void *agentite_asset_get_data(const Agentite_AssetRegistry *registry,
                               Agentite_AssetHandle handle);

/**
 * Get asset type.
 * Returns AGENTITE_ASSET_UNKNOWN if handle is invalid or stale.
 *
 * @param registry Asset registry
 * @param handle   Asset handle
 * @return Asset type
 */
Agentite_AssetType agentite_asset_get_type(const Agentite_AssetRegistry *registry,
                                            Agentite_AssetHandle handle);

/**
 * Get asset path (for serialization).
 * Returns NULL if handle is invalid or stale.
 *
 * @param registry Asset registry
 * @param handle   Asset handle
 * @return Asset path string (owned by registry, valid until asset freed)
 */
const char *agentite_asset_get_path(const Agentite_AssetRegistry *registry,
                                     Agentite_AssetHandle handle);

/* ============================================================================
 * Reference Counting
 * ============================================================================ */

/**
 * Increment asset reference count.
 * Call when storing an additional reference to an asset.
 *
 * @param registry Asset registry
 * @param handle   Asset handle
 * @return true on success, false if handle is invalid/stale
 */
bool agentite_asset_addref(Agentite_AssetRegistry *registry,
                            Agentite_AssetHandle handle);

/**
 * Decrement asset reference count.
 * If count reaches zero and destructor is set, the asset is destroyed.
 *
 * @param registry Asset registry
 * @param handle   Asset handle
 * @return true on success, false if handle is invalid/stale
 */
bool agentite_asset_release(Agentite_AssetRegistry *registry,
                             Agentite_AssetHandle handle);

/**
 * Get current reference count.
 * Returns 0 if handle is invalid or stale.
 *
 * @param registry Asset registry
 * @param handle   Asset handle
 * @return Reference count
 */
uint32_t agentite_asset_get_refcount(const Agentite_AssetRegistry *registry,
                                      Agentite_AssetHandle handle);

/* ============================================================================
 * Iteration
 * ============================================================================ */

/**
 * Get the number of registered assets.
 *
 * @param registry Asset registry
 * @return Number of live assets
 */
size_t agentite_asset_count(const Agentite_AssetRegistry *registry);

/**
 * Iterate over all registered assets.
 *
 * @param registry Asset registry
 * @param out_handles Output array to fill with handles
 * @param max_count Maximum number of handles to return
 * @return Number of handles written to out_handles
 */
size_t agentite_asset_get_all(const Agentite_AssetRegistry *registry,
                               Agentite_AssetHandle *out_handles,
                               size_t max_count);

/* ============================================================================
 * Serialization Helpers
 * ============================================================================ */

/**
 * Get human-readable name for asset type.
 *
 * @param type Asset type
 * @return Type name string (e.g., "texture", "sound")
 */
const char *agentite_asset_type_name(Agentite_AssetType type);

/**
 * Parse asset type from string.
 *
 * @param name Type name (case-insensitive)
 * @return Asset type, or AGENTITE_ASSET_UNKNOWN if not recognized
 */
Agentite_AssetType agentite_asset_type_from_name(const char *name);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_ASSET_H */
