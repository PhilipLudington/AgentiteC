/**
 * Agentite Engine - Hot Reload System Implementation
 *
 * Coordinates automatic asset reloading when files change on disk.
 * Integrates with the file watcher and various asset systems.
 */

#include "agentite/hotreload.h"
#include "agentite/watch.h"
#include "agentite/asset.h"
#include "agentite/sprite.h"
#include "agentite/audio.h"
#include "agentite/event.h"
#include "agentite/error.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_CUSTOM_HANDLERS 32
#define MAX_PENDING_RELOADS 256
#define PATH_BUFFER_SIZE 512

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * Extension to reload type mapping.
 */
typedef struct ExtensionMapping {
    const char *extension;
    Agentite_ReloadType type;
} ExtensionMapping;

/**
 * Custom handler entry.
 */
typedef struct CustomHandler {
    char extension[32];
    Agentite_ReloadHandler handler;
    void *userdata;
    bool active;
} CustomHandler;

/**
 * Pending reload entry.
 */
typedef struct PendingReload {
    char path[PATH_BUFFER_SIZE];
    Agentite_ReloadType type;
    bool active;
} PendingReload;

/**
 * Hot reload manager structure.
 */
struct Agentite_HotReloadManager {
    /* Configuration */
    Agentite_HotReloadConfig config;

    /* State */
    bool enabled;
    bool auto_reload;
    size_t reload_count;

    /* Custom handlers */
    CustomHandler custom_handlers[MAX_CUSTOM_HANDLERS];
    size_t custom_handler_count;

    /* Pending reloads (when auto_reload is disabled) */
    PendingReload pending[MAX_PENDING_RELOADS];
    size_t pending_count;

    /* Callback */
    Agentite_ReloadCallback callback;
    void *callback_userdata;
};

/* ============================================================================
 * Extension Mapping
 * ============================================================================ */

static const ExtensionMapping s_extension_map[] = {
    /* Textures */
    { ".png",    AGENTITE_RELOAD_TEXTURE },
    { ".jpg",    AGENTITE_RELOAD_TEXTURE },
    { ".jpeg",   AGENTITE_RELOAD_TEXTURE },
    { ".bmp",    AGENTITE_RELOAD_TEXTURE },
    { ".tga",    AGENTITE_RELOAD_TEXTURE },

    /* Audio */
    { ".wav",    AGENTITE_RELOAD_SOUND },
    { ".ogg",    AGENTITE_RELOAD_MUSIC },
    { ".mp3",    AGENTITE_RELOAD_MUSIC },

    /* Data */
    { ".toml",   AGENTITE_RELOAD_DATA },

    /* Prefabs and Scenes */
    { ".prefab", AGENTITE_RELOAD_PREFAB },
    { ".scene",  AGENTITE_RELOAD_SCENE },

    { NULL, AGENTITE_RELOAD_UNKNOWN }
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Get file extension from path (including dot).
 */
static const char *get_extension(const char *path)
{
    const char *dot = strrchr(path, '.');
    if (!dot || dot == path) {
        return "";
    }
    return dot;
}

/**
 * Compare extensions case-insensitively.
 */
static bool ext_equal(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) {
            return false;
        }
        a++;
        b++;
    }
    return *a == *b;
}

/**
 * Determine reload type from file extension.
 */
static Agentite_ReloadType get_reload_type(Agentite_HotReloadManager *manager, const char *path)
{
    const char *ext = get_extension(path);
    if (!ext || ext[0] == '\0') {
        return AGENTITE_RELOAD_UNKNOWN;
    }

    /* Check custom handlers first */
    for (size_t i = 0; i < MAX_CUSTOM_HANDLERS; i++) {
        if (manager->custom_handlers[i].active &&
            ext_equal(ext, manager->custom_handlers[i].extension)) {
            return AGENTITE_RELOAD_CUSTOM;
        }
    }

    /* Check built-in mappings */
    for (const ExtensionMapping *m = s_extension_map; m->extension; m++) {
        if (ext_equal(ext, m->extension)) {
            return m->type;
        }
    }

    return AGENTITE_RELOAD_UNKNOWN;
}

/**
 * Find custom handler for extension.
 */
static CustomHandler *find_custom_handler(Agentite_HotReloadManager *manager, const char *extension)
{
    for (size_t i = 0; i < MAX_CUSTOM_HANDLERS; i++) {
        if (manager->custom_handlers[i].active &&
            ext_equal(extension, manager->custom_handlers[i].extension)) {
            return &manager->custom_handlers[i];
        }
    }
    return NULL;
}

/**
 * Add pending reload entry.
 */
static bool add_pending_reload(Agentite_HotReloadManager *manager,
                               const char *path,
                               Agentite_ReloadType type)
{
    /* Check if already pending */
    for (size_t i = 0; i < MAX_PENDING_RELOADS; i++) {
        if (manager->pending[i].active &&
            strcmp(manager->pending[i].path, path) == 0) {
            /* Update type in case it changed */
            manager->pending[i].type = type;
            return true;
        }
    }

    /* Find empty slot */
    for (size_t i = 0; i < MAX_PENDING_RELOADS; i++) {
        if (!manager->pending[i].active) {
            strncpy(manager->pending[i].path, path, sizeof(manager->pending[i].path) - 1);
            manager->pending[i].type = type;
            manager->pending[i].active = true;
            manager->pending_count++;
            return true;
        }
    }

    return false;  /* Queue full */
}

/**
 * Emit reload event.
 */
static void emit_reload_event(Agentite_HotReloadManager *manager,
                               const char *path,
                               Agentite_ReloadType type,
                               bool success)
{
    (void)path;  /* Used in event data, but may be unused in some builds */

    if (!manager->config.events || !manager->config.emit_events) {
        return;
    }

    Agentite_Event event;
    memset(&event, 0, sizeof(event));
    event.type = success ? AGENTITE_EVENT_ASSET_RELOADED : AGENTITE_EVENT_ASSET_RELOAD_FAILED;
    event.custom.id = (int32_t)type;
    /* Note: path pointer is valid only during callback, don't store */

    agentite_event_emit(manager->config.events, &event);
}

/**
 * Invoke reload callback.
 */
static void invoke_callback(Agentite_HotReloadManager *manager,
                            const char *path,
                            Agentite_ReloadType type,
                            bool success,
                            const char *error)
{
    if (!manager->callback) {
        return;
    }

    Agentite_ReloadResult result = {
        .success = success,
        .path = path,
        .type = type,
        .error = error
    };

    manager->callback(&result, manager->callback_userdata);
}

/* ============================================================================
 * Reload Handlers
 * ============================================================================ */

/**
 * Reload a texture.
 */
static bool reload_texture(Agentite_HotReloadManager *manager, const char *path)
{
    if (!manager->config.sprites) {
        agentite_set_error("hotreload: no sprite renderer configured for texture reload");
        return false;
    }

    /* Find existing texture in asset registry */
    if (manager->config.assets) {
        Agentite_AssetHandle handle = agentite_asset_lookup(manager->config.assets, path);
        if (agentite_asset_is_valid(handle)) {
            Agentite_Texture *texture = (Agentite_Texture *)agentite_asset_get_data(
                manager->config.assets, handle);
            if (texture) {
                return agentite_texture_reload(manager->config.sprites, texture, path);
            }
        }
    }

    /* No existing texture found - can't reload something we haven't loaded */
    agentite_set_error("hotreload: texture not found in asset registry: %s", path);
    return false;
}

/**
 * Reload a data file.
 */
static bool reload_data(Agentite_HotReloadManager *manager, const char *path)
{
    /* Data file reload needs to be handled by game code via custom handler
     * since we don't know what data structure it maps to */
    (void)manager;
    (void)path;

    /* Check if it's in a locales directory - trigger localization reload */
    if (manager->config.localization && strstr(path, "locale") != NULL) {
        /* Would need agentite_loc_reload() - for now just log */
        SDL_Log("hotreload: localization file changed: %s", path);
        return true;
    }

    SDL_Log("hotreload: data file changed (no handler): %s", path);
    return true;
}

/**
 * Reload a prefab.
 */
static bool reload_prefab(Agentite_HotReloadManager *manager, const char *path)
{
    if (!manager->config.prefabs) {
        /* No prefab registry - just log */
        SDL_Log("hotreload: prefab file changed (no registry): %s", path);
        return true;
    }

    /* Would need agentite_prefab_reload() - for now just log */
    SDL_Log("hotreload: prefab file changed: %s", path);
    return true;
}

/**
 * Reload a scene.
 */
static bool reload_scene(Agentite_HotReloadManager *manager, const char *path)
{
    if (!manager->config.scenes) {
        /* No scene manager - just log */
        SDL_Log("hotreload: scene file changed (no manager): %s", path);
        return true;
    }

    /* Would need agentite_scene_reload() - for now just log */
    SDL_Log("hotreload: scene file changed: %s", path);
    return true;
}

/**
 * Process a single reload.
 */
static bool process_reload(Agentite_HotReloadManager *manager,
                           const char *path,
                           Agentite_ReloadType type)
{
    bool success = false;
    const char *error = NULL;

    switch (type) {
        case AGENTITE_RELOAD_TEXTURE:
            success = reload_texture(manager, path);
            break;

        case AGENTITE_RELOAD_SOUND:
        case AGENTITE_RELOAD_MUSIC:
            /* Audio reload would need similar infrastructure */
            SDL_Log("hotreload: audio file changed: %s", path);
            success = true;
            break;

        case AGENTITE_RELOAD_DATA:
            success = reload_data(manager, path);
            break;

        case AGENTITE_RELOAD_PREFAB:
            success = reload_prefab(manager, path);
            break;

        case AGENTITE_RELOAD_SCENE:
            success = reload_scene(manager, path);
            break;

        case AGENTITE_RELOAD_CUSTOM: {
            const char *ext = get_extension(path);
            CustomHandler *handler = find_custom_handler(manager, ext);
            if (handler && handler->handler) {
                success = handler->handler(path, type, handler->userdata);
            } else {
                success = false;
                error = "no handler registered";
            }
            break;
        }

        default:
            success = false;
            error = "unknown reload type";
            break;
    }

    if (!success && !error) {
        error = agentite_get_last_error();
    }

    /* Update statistics */
    if (success) {
        manager->reload_count++;
    }

    /* Notify */
    emit_reload_event(manager, path, type, success);
    invoke_callback(manager, path, type, success, error);

    return success;
}

/* ============================================================================
 * File Watcher Callback
 * ============================================================================ */

/**
 * Callback from file watcher when a file changes.
 */
static void on_file_changed(const Agentite_WatchEvent *event, void *userdata)
{
    Agentite_HotReloadManager *manager = (Agentite_HotReloadManager *)userdata;

    /* Ignore non-modification events for now */
    if (event->type != AGENTITE_WATCH_MODIFIED &&
        event->type != AGENTITE_WATCH_CREATED) {
        return;
    }

    /* Determine reload type */
    Agentite_ReloadType type = get_reload_type(manager, event->path);
    if (type == AGENTITE_RELOAD_UNKNOWN) {
        return;  /* Unknown file type, ignore */
    }

    if (manager->auto_reload) {
        /* Reload immediately */
        process_reload(manager, event->path, type);
    } else {
        /* Queue for later */
        add_pending_reload(manager, event->path, type);
    }
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_HotReloadManager *agentite_hotreload_create(const Agentite_HotReloadConfig *config)
{
    if (!config || !config->watcher) {
        agentite_set_error("hotreload: file watcher is required");
        return NULL;
    }

    Agentite_HotReloadManager *manager = (Agentite_HotReloadManager *)calloc(
        1, sizeof(Agentite_HotReloadManager));
    if (!manager) {
        agentite_set_error("hotreload: failed to allocate manager");
        return NULL;
    }

    /* Copy configuration */
    manager->config = *config;
    manager->enabled = true;
    manager->auto_reload = config->auto_reload;

    /* Register as file watcher callback */
    agentite_watch_set_callback(config->watcher, on_file_changed, manager);

    return manager;
}

void agentite_hotreload_destroy(Agentite_HotReloadManager *manager)
{
    if (!manager) return;

    /* Unregister from file watcher */
    if (manager->config.watcher) {
        agentite_watch_set_callback(manager->config.watcher, NULL, NULL);
    }

    free(manager);
}

/* ============================================================================
 * Update
 * ============================================================================ */

void agentite_hotreload_update(Agentite_HotReloadManager *manager)
{
    if (!manager || !manager->enabled) return;

    /* Update file watcher to process pending events */
    if (manager->config.watcher) {
        agentite_watch_update(manager->config.watcher);
    }
}

/* ============================================================================
 * Manual Reload
 * ============================================================================ */

bool agentite_hotreload_reload_asset(Agentite_HotReloadManager *manager, const char *path)
{
    if (!manager || !path) return false;

    Agentite_ReloadType type = get_reload_type(manager, path);
    if (type == AGENTITE_RELOAD_UNKNOWN) {
        agentite_set_error("hotreload: unknown file type: %s", path);
        return false;
    }

    return process_reload(manager, path, type);
}

size_t agentite_hotreload_reload_all(Agentite_HotReloadManager *manager, Agentite_ReloadType type)
{
    if (!manager || !manager->config.assets) return 0;

    /* This would iterate through asset registry and reload matching types
     * For now, just return 0 - full implementation would need registry iteration */
    (void)type;
    return 0;
}

/* ============================================================================
 * Custom Handlers
 * ============================================================================ */

bool agentite_hotreload_register_handler(Agentite_HotReloadManager *manager,
                                          const char *extension,
                                          Agentite_ReloadHandler handler,
                                          void *userdata)
{
    if (!manager || !extension || !handler) return false;

    /* Check if already registered */
    if (find_custom_handler(manager, extension)) {
        agentite_set_error("hotreload: handler already registered for %s", extension);
        return false;
    }

    /* Find empty slot */
    for (size_t i = 0; i < MAX_CUSTOM_HANDLERS; i++) {
        if (!manager->custom_handlers[i].active) {
            strncpy(manager->custom_handlers[i].extension, extension,
                    sizeof(manager->custom_handlers[i].extension) - 1);
            manager->custom_handlers[i].handler = handler;
            manager->custom_handlers[i].userdata = userdata;
            manager->custom_handlers[i].active = true;
            manager->custom_handler_count++;
            return true;
        }
    }

    agentite_set_error("hotreload: maximum custom handlers reached");
    return false;
}

void agentite_hotreload_unregister_handler(Agentite_HotReloadManager *manager,
                                            const char *extension)
{
    if (!manager || !extension) return;

    for (size_t i = 0; i < MAX_CUSTOM_HANDLERS; i++) {
        if (manager->custom_handlers[i].active &&
            ext_equal(extension, manager->custom_handlers[i].extension)) {
            memset(&manager->custom_handlers[i], 0, sizeof(CustomHandler));
            manager->custom_handler_count--;
            break;
        }
    }
}

/* ============================================================================
 * Callbacks
 * ============================================================================ */

void agentite_hotreload_set_callback(Agentite_HotReloadManager *manager,
                                      Agentite_ReloadCallback callback,
                                      void *userdata)
{
    if (!manager) return;
    manager->callback = callback;
    manager->callback_userdata = userdata;
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void agentite_hotreload_set_enabled(Agentite_HotReloadManager *manager, bool enabled)
{
    if (!manager) return;
    manager->enabled = enabled;
}

bool agentite_hotreload_is_enabled(const Agentite_HotReloadManager *manager)
{
    if (!manager) return false;
    return manager->enabled;
}

void agentite_hotreload_set_auto_reload(Agentite_HotReloadManager *manager, bool auto_reload)
{
    if (!manager) return;
    manager->auto_reload = auto_reload;
}

size_t agentite_hotreload_reload_pending(Agentite_HotReloadManager *manager)
{
    if (!manager) return 0;

    size_t reloaded = 0;
    for (size_t i = 0; i < MAX_PENDING_RELOADS; i++) {
        if (manager->pending[i].active) {
            if (process_reload(manager, manager->pending[i].path, manager->pending[i].type)) {
                reloaded++;
            }
            manager->pending[i].active = false;
            manager->pending_count--;
        }
    }

    return reloaded;
}

/* ============================================================================
 * Query
 * ============================================================================ */

size_t agentite_hotreload_pending_count(const Agentite_HotReloadManager *manager)
{
    if (!manager) return 0;
    return manager->pending_count;
}

size_t agentite_hotreload_get_reload_count(const Agentite_HotReloadManager *manager)
{
    if (!manager) return 0;
    return manager->reload_count;
}

Agentite_ReloadType agentite_hotreload_get_type_for_path(const char *path)
{
    if (!path) return AGENTITE_RELOAD_UNKNOWN;

    const char *ext = get_extension(path);
    if (!ext || ext[0] == '\0') {
        return AGENTITE_RELOAD_UNKNOWN;
    }

    for (const ExtensionMapping *m = s_extension_map; m->extension; m++) {
        if (ext_equal(ext, m->extension)) {
            return m->type;
        }
    }

    return AGENTITE_RELOAD_UNKNOWN;
}

const char *agentite_hotreload_type_name(Agentite_ReloadType type)
{
    switch (type) {
        case AGENTITE_RELOAD_UNKNOWN:      return "UNKNOWN";
        case AGENTITE_RELOAD_TEXTURE:      return "TEXTURE";
        case AGENTITE_RELOAD_SOUND:        return "SOUND";
        case AGENTITE_RELOAD_MUSIC:        return "MUSIC";
        case AGENTITE_RELOAD_DATA:         return "DATA";
        case AGENTITE_RELOAD_PREFAB:       return "PREFAB";
        case AGENTITE_RELOAD_SCENE:        return "SCENE";
        case AGENTITE_RELOAD_LOCALIZATION: return "LOCALIZATION";
        case AGENTITE_RELOAD_CUSTOM:       return "CUSTOM";
        default:                            return "INVALID";
    }
}
