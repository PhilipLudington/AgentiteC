/**
 * Agentite Engine - Hot Reload System
 *
 * Coordinates automatic asset reloading when files change on disk.
 * Works with the file watcher to detect changes and triggers appropriate
 * reload handlers for different asset types.
 *
 * Usage:
 *   // Create hot reload manager
 *   Agentite_HotReloadConfig config = AGENTITE_HOT_RELOAD_CONFIG_DEFAULT;
 *   config.watcher = watcher;
 *   config.assets = asset_registry;
 *   config.sprites = sprite_renderer;
 *   Agentite_HotReloadManager *hr = agentite_hotreload_create(&config);
 *
 *   // In game loop - MUST call each frame on main thread
 *   while (running) {
 *       agentite_hotreload_update(hr);  // Processes file changes
 *       // ... rest of frame
 *   }
 *
 *   agentite_hotreload_destroy(hr);
 *
 * Supported Asset Types:
 *   - Textures (.png, .jpg, .bmp, .tga)
 *   - Sounds (.wav)
 *   - Music (.ogg, .mp3)
 *   - Data files (.toml)
 *   - Prefabs (.prefab)
 *   - Scenes (.scene)
 *   - Custom types via registered handlers
 *
 * Thread Safety:
 *   - All functions must be called from main thread only
 *   - File watching happens on background thread (via FileWatcher)
 *   - Reload callbacks are invoked on main thread during update()
 */

#ifndef AGENTITE_HOTRELOAD_H
#define AGENTITE_HOTRELOAD_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_FileWatcher Agentite_FileWatcher;
typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;
typedef struct Agentite_SpriteRenderer Agentite_SpriteRenderer;
typedef struct Agentite_Audio Agentite_Audio;
typedef struct Agentite_PrefabRegistry Agentite_PrefabRegistry;
typedef struct Agentite_SceneManager Agentite_SceneManager;
typedef struct Agentite_Localization Agentite_Localization;
typedef struct Agentite_EventDispatcher Agentite_EventDispatcher;
typedef struct Agentite_DataLoader Agentite_DataLoader;

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * Opaque hot reload manager handle.
 */
typedef struct Agentite_HotReloadManager Agentite_HotReloadManager;

/**
 * Asset reload types.
 */
typedef enum Agentite_ReloadType {
    AGENTITE_RELOAD_UNKNOWN = 0,
    AGENTITE_RELOAD_TEXTURE,        /* Image files (.png, .jpg, .bmp, .tga) */
    AGENTITE_RELOAD_SOUND,          /* Sound effects (.wav) */
    AGENTITE_RELOAD_MUSIC,          /* Music files (.ogg, .mp3) */
    AGENTITE_RELOAD_DATA,           /* Data files (.toml) */
    AGENTITE_RELOAD_PREFAB,         /* Prefab files (.prefab) */
    AGENTITE_RELOAD_SCENE,          /* Scene files (.scene) */
    AGENTITE_RELOAD_LOCALIZATION,   /* Localization files (locale/xx.toml) */
    AGENTITE_RELOAD_CUSTOM          /* Custom handler */
} Agentite_ReloadType;

/**
 * Reload result information.
 */
typedef struct Agentite_ReloadResult {
    bool success;               /* True if reload succeeded */
    const char *path;           /* File path that was reloaded */
    Agentite_ReloadType type;   /* Type of asset reloaded */
    const char *error;          /* Error message if failed (NULL on success) */
} Agentite_ReloadResult;

/**
 * Custom reload handler callback.
 * Called when a file with a registered extension changes.
 *
 * @param path     Full path to the changed file
 * @param type     Reload type (AGENTITE_RELOAD_CUSTOM for custom handlers)
 * @param userdata User data passed during registration
 * @return true if reload succeeded, false on failure
 */
typedef bool (*Agentite_ReloadHandler)(const char *path,
                                        Agentite_ReloadType type,
                                        void *userdata);

/**
 * Reload notification callback.
 * Called after each asset is reloaded (successfully or not).
 *
 * @param result  Result of the reload operation
 * @param userdata User data passed during registration
 */
typedef void (*Agentite_ReloadCallback)(const Agentite_ReloadResult *result,
                                         void *userdata);

/**
 * Hot reload manager configuration.
 */
typedef struct Agentite_HotReloadConfig {
    /* Required */
    Agentite_FileWatcher *watcher;          /* File watcher (required) */

    /* Optional systems - set to NULL if not used */
    Agentite_AssetRegistry *assets;         /* Asset registry for path tracking */
    Agentite_SpriteRenderer *sprites;       /* For texture reload */
    Agentite_Audio *audio;                  /* For audio reload */
    Agentite_PrefabRegistry *prefabs;       /* For prefab reload */
    Agentite_SceneManager *scenes;          /* For scene reload */
    Agentite_Localization *localization;    /* For localization reload */
    Agentite_EventDispatcher *events;       /* For reload event notifications */

    /* Configuration */
    bool auto_reload;                       /* Reload automatically on change (default: true) */
    bool emit_events;                       /* Emit events on reload (default: true) */
} Agentite_HotReloadConfig;

/** Default configuration */
#define AGENTITE_HOT_RELOAD_CONFIG_DEFAULT \
    ((Agentite_HotReloadConfig){ \
        .watcher = NULL, \
        .assets = NULL, \
        .sprites = NULL, \
        .audio = NULL, \
        .prefabs = NULL, \
        .scenes = NULL, \
        .localization = NULL, \
        .events = NULL, \
        .auto_reload = true, \
        .emit_events = true \
    })

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create a hot reload manager with the given configuration.
 * Caller OWNS the returned pointer and MUST call agentite_hotreload_destroy().
 *
 * @param config Configuration (watcher is required, others optional)
 * @return New manager, or NULL on failure
 */
Agentite_HotReloadManager *agentite_hotreload_create(const Agentite_HotReloadConfig *config);

/**
 * Destroy a hot reload manager.
 * Safe to pass NULL.
 *
 * @param manager Manager to destroy
 */
void agentite_hotreload_destroy(Agentite_HotReloadManager *manager);

/* ============================================================================
 * Update
 * ============================================================================ */

/**
 * Process pending file changes and trigger reloads.
 * MUST be called on the main thread each frame.
 *
 * This function:
 * 1. Polls the file watcher for changes
 * 2. Determines asset types from file extensions
 * 3. Triggers appropriate reload handlers
 * 4. Invokes notification callbacks
 * 5. Emits events if configured
 *
 * @param manager Hot reload manager
 */
void agentite_hotreload_update(Agentite_HotReloadManager *manager);

/* ============================================================================
 * Manual Reload
 * ============================================================================ */

/**
 * Manually trigger reload of a specific asset.
 * Useful for force-refreshing assets regardless of file changes.
 *
 * @param manager Hot reload manager
 * @param path    File path to reload
 * @return true on success, false on failure
 */
bool agentite_hotreload_reload_asset(Agentite_HotReloadManager *manager, const char *path);

/**
 * Manually trigger reload of all assets of a specific type.
 *
 * @param manager Hot reload manager
 * @param type    Asset type to reload
 * @return Number of assets reloaded
 */
size_t agentite_hotreload_reload_all(Agentite_HotReloadManager *manager, Agentite_ReloadType type);

/* ============================================================================
 * Custom Handlers
 * ============================================================================ */

/**
 * Register a custom reload handler for a file extension.
 * The handler will be called when files with this extension change.
 *
 * @param manager   Hot reload manager
 * @param extension File extension including dot (e.g., ".json")
 * @param handler   Handler function
 * @param userdata  User data passed to handler
 * @return true on success, false if extension already registered
 */
bool agentite_hotreload_register_handler(Agentite_HotReloadManager *manager,
                                          const char *extension,
                                          Agentite_ReloadHandler handler,
                                          void *userdata);

/**
 * Unregister a custom reload handler.
 *
 * @param manager   Hot reload manager
 * @param extension File extension to unregister
 */
void agentite_hotreload_unregister_handler(Agentite_HotReloadManager *manager,
                                            const char *extension);

/* ============================================================================
 * Callbacks
 * ============================================================================ */

/**
 * Set callback for reload notifications.
 * Called after each asset reload (successful or not).
 * Only one callback can be registered at a time.
 *
 * @param manager  Hot reload manager
 * @param callback Callback function (NULL to unregister)
 * @param userdata User data passed to callback
 */
void agentite_hotreload_set_callback(Agentite_HotReloadManager *manager,
                                      Agentite_ReloadCallback callback,
                                      void *userdata);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Enable or disable hot reload.
 * When disabled, file changes are still detected but not processed.
 *
 * @param manager Hot reload manager
 * @param enabled true to enable, false to disable
 */
void agentite_hotreload_set_enabled(Agentite_HotReloadManager *manager, bool enabled);

/**
 * Check if hot reload is enabled.
 *
 * @param manager Hot reload manager
 * @return true if enabled
 */
bool agentite_hotreload_is_enabled(const Agentite_HotReloadManager *manager);

/**
 * Set auto-reload mode.
 * When enabled, assets are reloaded immediately on file change.
 * When disabled, use agentite_hotreload_reload_pending() to trigger reloads.
 *
 * @param manager     Hot reload manager
 * @param auto_reload true for automatic reload
 */
void agentite_hotreload_set_auto_reload(Agentite_HotReloadManager *manager, bool auto_reload);

/**
 * Process pending reloads (when auto_reload is disabled).
 * Call this when ready to apply queued file changes.
 *
 * @param manager Hot reload manager
 * @return Number of assets reloaded
 */
size_t agentite_hotreload_reload_pending(Agentite_HotReloadManager *manager);

/* ============================================================================
 * Query
 * ============================================================================ */

/**
 * Get the number of pending file changes.
 *
 * @param manager Hot reload manager
 * @return Number of pending changes
 */
size_t agentite_hotreload_pending_count(const Agentite_HotReloadManager *manager);

/**
 * Get total reload count (for statistics).
 *
 * @param manager Hot reload manager
 * @return Total number of reloads performed
 */
size_t agentite_hotreload_get_reload_count(const Agentite_HotReloadManager *manager);

/**
 * Determine the reload type for a file path based on extension.
 *
 * @param path File path to check
 * @return Reload type, or AGENTITE_RELOAD_UNKNOWN if not recognized
 */
Agentite_ReloadType agentite_hotreload_get_type_for_path(const char *path);

/**
 * Get a human-readable name for a reload type.
 *
 * @param type Reload type
 * @return Static string name
 */
const char *agentite_hotreload_type_name(Agentite_ReloadType type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_HOTRELOAD_H */
