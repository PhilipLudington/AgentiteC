/**
 * Agentite Engine - Mod System
 *
 * Provides mod loading, management, and virtual filesystem for asset overrides.
 * Mods are loaded from directories containing a mod.toml manifest file.
 *
 * Usage:
 *   // Create mod manager
 *   Agentite_ModManagerConfig config = AGENTITE_MOD_MANAGER_CONFIG_DEFAULT;
 *   config.assets = asset_registry;
 *   config.allow_overrides = true;
 *   Agentite_ModManager *mods = agentite_mod_manager_create(&config);
 *
 *   // Add mod directories to scan
 *   agentite_mod_add_search_path(mods, "mods/");
 *
 *   // Scan for available mods
 *   size_t found = agentite_mod_scan(mods);
 *   printf("Found %zu mods\n", found);
 *
 *   // Load enabled mods
 *   const char *enabled[] = {"texture_pack", "expanded_maps"};
 *   agentite_mod_load_all(mods, enabled, 2);
 *
 *   // Resolve asset paths through mod system
 *   const char *path = agentite_mod_resolve_path(mods, "textures/player.png");
 *   // Returns mod path if override exists, otherwise original path
 *
 *   agentite_mod_manager_destroy(mods);
 *
 * Mod Manifest (mod.toml):
 *   [mod]
 *   id = "my_mod"
 *   name = "My Awesome Mod"
 *   version = "1.0.0"
 *   author = "Author Name"
 *   description = "Description of the mod"
 *   min_engine_version = "0.1.0"
 *
 *   [dependencies]
 *   other_mod = ">=1.0.0"
 *
 *   [conflicts]
 *   incompatible_mod = "*"
 *
 *   [load_order]
 *   before = ["mod_to_load_after"]
 *   after = ["mod_to_load_before"]
 *
 * Thread Safety:
 *   - All functions must be called from main thread only
 */

#ifndef AGENTITE_MOD_H
#define AGENTITE_MOD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;
typedef struct Agentite_HotReloadManager Agentite_HotReloadManager;
typedef struct Agentite_EventDispatcher Agentite_EventDispatcher;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * Opaque mod manager handle.
 */
typedef struct Agentite_ModManager Agentite_ModManager;

/**
 * Mod state.
 */
typedef enum Agentite_ModState {
    AGENTITE_MOD_UNLOADED = 0,    /* Not loaded */
    AGENTITE_MOD_DISCOVERED,       /* Found but not loaded */
    AGENTITE_MOD_LOADING,          /* Currently loading */
    AGENTITE_MOD_LOADED,           /* Successfully loaded */
    AGENTITE_MOD_FAILED,           /* Load failed */
    AGENTITE_MOD_DISABLED          /* Explicitly disabled */
} Agentite_ModState;

/**
 * Mod information (read-only).
 */
typedef struct Agentite_ModInfo {
    char id[64];                   /* Unique mod identifier */
    char name[128];                /* Display name */
    char version[32];              /* Semantic version string */
    char author[64];               /* Author name */
    char description[512];         /* Mod description */
    char path[512];                /* Filesystem path to mod directory */
    char min_engine_version[32];   /* Minimum engine version required */
    Agentite_ModState state;       /* Current state */
    size_t dependency_count;       /* Number of dependencies */
    size_t conflict_count;         /* Number of conflicts */
} Agentite_ModInfo;

/**
 * Mod load callback.
 * Called when a mod is loaded or unloaded.
 *
 * @param mod_id   ID of the mod
 * @param state    New state of the mod
 * @param userdata User data passed during registration
 */
typedef void (*Agentite_ModCallback)(const char *mod_id,
                                      Agentite_ModState state,
                                      void *userdata);

/**
 * Mod manager configuration.
 */
typedef struct Agentite_ModManagerConfig {
    Agentite_AssetRegistry *assets;         /* Asset registry (optional) */
    Agentite_HotReloadManager *hotreload;   /* Hot reload manager (optional) */
    Agentite_EventDispatcher *events;       /* Event dispatcher (optional) */
    bool allow_overrides;                   /* Allow mods to override base assets (default: true) */
    bool emit_events;                       /* Emit events on mod load/unload (default: true) */
} Agentite_ModManagerConfig;

/** Default configuration */
#define AGENTITE_MOD_MANAGER_CONFIG_DEFAULT \
    ((Agentite_ModManagerConfig){ \
        .assets = NULL, \
        .hotreload = NULL, \
        .events = NULL, \
        .allow_overrides = true, \
        .emit_events = true \
    })

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create a mod manager with the given configuration.
 * Caller OWNS the returned pointer and MUST call agentite_mod_manager_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return New manager, or NULL on failure
 */
Agentite_ModManager *agentite_mod_manager_create(const Agentite_ModManagerConfig *config);

/**
 * Destroy a mod manager.
 * Unloads all mods and frees resources.
 * Safe to pass NULL.
 *
 * @param manager Manager to destroy
 */
void agentite_mod_manager_destroy(Agentite_ModManager *manager);

/* ============================================================================
 * Search Paths
 * ============================================================================ */

/**
 * Add a directory to search for mods.
 * The directory will be scanned for subdirectories containing mod.toml files.
 *
 * @param manager Mod manager
 * @param path    Directory path to search
 * @return true on success
 */
bool agentite_mod_add_search_path(Agentite_ModManager *manager, const char *path);

/**
 * Remove a search path.
 *
 * @param manager Mod manager
 * @param path    Directory path to remove
 */
void agentite_mod_remove_search_path(Agentite_ModManager *manager, const char *path);

/* ============================================================================
 * Discovery
 * ============================================================================ */

/**
 * Scan search paths for available mods.
 * Does not load mods, only discovers and parses manifests.
 *
 * @param manager Mod manager
 * @return Number of mods discovered
 */
size_t agentite_mod_scan(Agentite_ModManager *manager);

/**
 * Rescan for mod changes (new mods, removed mods).
 *
 * @param manager Mod manager
 */
void agentite_mod_refresh(Agentite_ModManager *manager);

/* ============================================================================
 * Query
 * ============================================================================ */

/**
 * Get the number of discovered mods.
 *
 * @param manager Mod manager
 * @return Number of mods
 */
size_t agentite_mod_count(const Agentite_ModManager *manager);

/**
 * Get mod info by index.
 *
 * @param manager Mod manager
 * @param index   Mod index (0 to count-1)
 * @return Mod info, or NULL if index out of range
 */
const Agentite_ModInfo *agentite_mod_get_info(const Agentite_ModManager *manager, size_t index);

/**
 * Find mod by ID.
 *
 * @param manager Mod manager
 * @param mod_id  Mod identifier
 * @return Mod info, or NULL if not found
 */
const Agentite_ModInfo *agentite_mod_find(const Agentite_ModManager *manager, const char *mod_id);

/**
 * Get mod state.
 *
 * @param manager Mod manager
 * @param mod_id  Mod identifier
 * @return Mod state, or AGENTITE_MOD_UNLOADED if not found
 */
Agentite_ModState agentite_mod_get_state(const Agentite_ModManager *manager, const char *mod_id);

/**
 * Get mod dependencies.
 *
 * @param manager    Mod manager
 * @param mod_id     Mod identifier
 * @param out_deps   Output array for dependency IDs (caller provides buffer)
 * @param max_deps   Maximum dependencies to return
 * @return Number of dependencies (may be > max_deps if truncated)
 */
size_t agentite_mod_get_dependencies(const Agentite_ModManager *manager,
                                      const char *mod_id,
                                      const char **out_deps,
                                      size_t max_deps);

/**
 * Get mods that conflict with a given mod.
 *
 * @param manager      Mod manager
 * @param mod_id       Mod identifier
 * @param out_conflicts Output array for conflict IDs (caller provides buffer)
 * @param max_conflicts Maximum conflicts to return
 * @return Number of conflicts (may be > max_conflicts if truncated)
 */
size_t agentite_mod_get_conflicts(const Agentite_ModManager *manager,
                                   const char *mod_id,
                                   const char **out_conflicts,
                                   size_t max_conflicts);

/* ============================================================================
 * Load Order Resolution
 * ============================================================================ */

/**
 * Resolve load order for a set of mods.
 * Performs topological sort based on dependencies and load_order hints.
 *
 * @param manager      Mod manager
 * @param enabled_mods Array of mod IDs to enable
 * @param enabled_count Number of mods in array
 * @param out_ordered  Output array for ordered mod IDs (caller must free via agentite_mod_free_load_order)
 * @param out_count    Output number of mods in order
 * @return true on success, false if circular dependency or other error
 */
bool agentite_mod_resolve_load_order(Agentite_ModManager *manager,
                                      const char **enabled_mods,
                                      size_t enabled_count,
                                      char ***out_ordered,
                                      size_t *out_count);

/**
 * Free load order array returned by agentite_mod_resolve_load_order.
 *
 * @param ordered Array to free
 * @param count   Number of elements
 */
void agentite_mod_free_load_order(char **ordered, size_t count);

/* ============================================================================
 * Validation
 * ============================================================================ */

/**
 * Validate a mod's manifest.
 *
 * @param manager   Mod manager
 * @param mod_id    Mod identifier
 * @param out_error Output error message (caller must free if non-NULL)
 * @return true if valid, false if invalid
 */
bool agentite_mod_validate(const Agentite_ModManager *manager,
                            const char *mod_id,
                            char **out_error);

/**
 * Check for conflicts between enabled mods.
 *
 * @param manager       Mod manager
 * @param enabled_mods  Array of mod IDs
 * @param enabled_count Number of mods
 * @param out_conflicts Output array of conflicting mod ID pairs (caller must free)
 * @param out_count     Number of conflicts
 * @return true if no conflicts, false if conflicts exist
 */
bool agentite_mod_check_conflicts(const Agentite_ModManager *manager,
                                   const char **enabled_mods,
                                   size_t enabled_count,
                                   char ***out_conflicts,
                                   size_t *out_count);

/**
 * Free conflicts array returned by agentite_mod_check_conflicts.
 *
 * @param conflicts Array to free
 * @param count     Number of elements
 */
void agentite_mod_free_conflicts(char **conflicts, size_t count);

/* ============================================================================
 * Loading
 * ============================================================================ */

/**
 * Load a single mod.
 * Automatically loads dependencies.
 *
 * @param manager Mod manager
 * @param mod_id  Mod identifier
 * @return true on success
 */
bool agentite_mod_load(Agentite_ModManager *manager, const char *mod_id);

/**
 * Load multiple mods in resolved order.
 * Resolves load order and loads all mods.
 *
 * @param manager      Mod manager
 * @param enabled_mods Array of mod IDs
 * @param enabled_count Number of mods
 * @return true if all mods loaded successfully
 */
bool agentite_mod_load_all(Agentite_ModManager *manager,
                            const char **enabled_mods,
                            size_t enabled_count);

/**
 * Unload a mod.
 * Also unloads mods that depend on it.
 *
 * @param manager Mod manager
 * @param mod_id  Mod identifier
 */
void agentite_mod_unload(Agentite_ModManager *manager, const char *mod_id);

/**
 * Unload all mods.
 *
 * @param manager Mod manager
 */
void agentite_mod_unload_all(Agentite_ModManager *manager);

/* ============================================================================
 * Virtual Filesystem
 * ============================================================================ */

/**
 * Resolve a virtual asset path through the mod system.
 * Checks loaded mods (in reverse load order) for the asset,
 * returns the first match or the original path if no override exists.
 *
 * @param manager      Mod manager
 * @param virtual_path Virtual asset path (e.g., "textures/player.png")
 * @return Resolved path (may be mod path or original). Do not free.
 *         Returns NULL if path is NULL.
 */
const char *agentite_mod_resolve_path(const Agentite_ModManager *manager,
                                       const char *virtual_path);

/**
 * Check if an asset path has a mod override.
 *
 * @param manager      Mod manager
 * @param virtual_path Virtual asset path
 * @return true if a mod provides this asset
 */
bool agentite_mod_has_override(const Agentite_ModManager *manager,
                                const char *virtual_path);

/**
 * Get the mod that provides an asset override.
 *
 * @param manager      Mod manager
 * @param virtual_path Virtual asset path
 * @return Mod ID providing the override, or NULL if no override
 */
const char *agentite_mod_get_override_source(const Agentite_ModManager *manager,
                                              const char *virtual_path);

/* ============================================================================
 * Enable/Disable
 * ============================================================================ */

/**
 * Enable or disable a mod.
 * Disabled mods are not loaded but remain discovered.
 *
 * @param manager Mod manager
 * @param mod_id  Mod identifier
 * @param enabled true to enable, false to disable
 * @return true if state changed
 */
bool agentite_mod_set_enabled(Agentite_ModManager *manager, const char *mod_id, bool enabled);

/**
 * Check if a mod is enabled.
 *
 * @param manager Mod manager
 * @param mod_id  Mod identifier
 * @return true if enabled
 */
bool agentite_mod_is_enabled(const Agentite_ModManager *manager, const char *mod_id);

/* ============================================================================
 * Persistence
 * ============================================================================ */

/**
 * Save enabled mods list to file.
 *
 * @param manager Mod manager
 * @param path    File path (TOML format)
 * @return true on success
 */
bool agentite_mod_save_enabled(const Agentite_ModManager *manager, const char *path);

/**
 * Load enabled mods list from file.
 *
 * @param manager Mod manager
 * @param path    File path (TOML format)
 * @return true on success
 */
bool agentite_mod_load_enabled(Agentite_ModManager *manager, const char *path);

/* ============================================================================
 * Callbacks
 * ============================================================================ */

/**
 * Set callback for mod state changes.
 *
 * @param manager  Mod manager
 * @param callback Callback function (NULL to unregister)
 * @param userdata User data passed to callback
 */
void agentite_mod_set_callback(Agentite_ModManager *manager,
                                Agentite_ModCallback callback,
                                void *userdata);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * Get a human-readable name for a mod state.
 *
 * @param state Mod state
 * @return Static string name
 */
const char *agentite_mod_state_name(Agentite_ModState state);

/**
 * Get the number of loaded mods.
 *
 * @param manager Mod manager
 * @return Number of loaded mods
 */
size_t agentite_mod_loaded_count(const Agentite_ModManager *manager);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_MOD_H */
