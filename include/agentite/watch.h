/**
 * Agentite Engine - File Watcher System
 *
 * Cross-platform file system monitoring for hot reload support.
 * Monitors directories for file changes and emits events when assets
 * are created, modified, deleted, or renamed.
 *
 * Usage:
 *   Agentite_FileWatcherConfig config = AGENTITE_FILE_WATCHER_CONFIG_DEFAULT;
 *   Agentite_FileWatcher *watcher = agentite_watch_create(&config);
 *
 *   agentite_watch_add_path(watcher, "assets/");
 *   agentite_watch_set_callback(watcher, on_file_changed, userdata);
 *
 *   // In game loop - MUST call each frame on main thread
 *   while (running) {
 *       agentite_watch_update(watcher);  // Processes pending events
 *       // ... rest of frame
 *   }
 *
 *   agentite_watch_destroy(watcher);
 *
 * Thread Safety:
 *   - agentite_watch_create/destroy: NOT thread-safe, main thread only
 *   - agentite_watch_update: NOT thread-safe, main thread only
 *   - agentite_watch_add_path/remove_path: Thread-safe
 *   - agentite_watch_is_watching: Thread-safe
 *   - Callbacks are always invoked on the main thread during update()
 *
 * Platform Support:
 *   - macOS: FSEvents API
 *   - Linux: inotify API
 *   - Windows: ReadDirectoryChangesW API
 */

#ifndef AGENTITE_WATCH_H
#define AGENTITE_WATCH_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Types
 * ============================================================================ */

/**
 * Opaque file watcher handle.
 * Manages a background thread that monitors the filesystem and queues events.
 */
typedef struct Agentite_FileWatcher Agentite_FileWatcher;

/**
 * Watch event types.
 */
typedef enum Agentite_WatchEventType {
    AGENTITE_WATCH_CREATED,     /* File or directory was created */
    AGENTITE_WATCH_MODIFIED,    /* File was modified */
    AGENTITE_WATCH_DELETED,     /* File or directory was deleted */
    AGENTITE_WATCH_RENAMED      /* File or directory was renamed */
} Agentite_WatchEventType;

/**
 * Watch event data.
 * Delivered to the callback when a file system change is detected.
 */
typedef struct Agentite_WatchEvent {
    Agentite_WatchEventType type;   /* Type of change */
    char path[512];                  /* Path relative to watched root */
    char old_path[512];              /* Previous path (RENAMED only, empty otherwise) */
    uint64_t timestamp;              /* Event timestamp (milliseconds since epoch) */
} Agentite_WatchEvent;

/**
 * Callback function for file watch events.
 * Called on the main thread during agentite_watch_update().
 *
 * @param event   The watch event that occurred
 * @param userdata User data passed during callback registration
 */
typedef void (*Agentite_WatchCallback)(const Agentite_WatchEvent *event, void *userdata);

/**
 * File watcher configuration.
 */
typedef struct Agentite_FileWatcherConfig {
    bool recursive;             /* Watch subdirectories (default: true) */
    uint32_t debounce_ms;       /* Coalesce rapid changes, in milliseconds (default: 100) */
    size_t max_events;          /* Maximum queued events (0 = unlimited, default: 1024) */
} Agentite_FileWatcherConfig;

/** Default configuration */
#define AGENTITE_FILE_WATCHER_CONFIG_DEFAULT \
    ((Agentite_FileWatcherConfig){ \
        .recursive = true, \
        .debounce_ms = 100, \
        .max_events = 1024 \
    })

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

/**
 * Create a file watcher with the given configuration.
 * Caller OWNS the returned pointer and MUST call agentite_watch_destroy().
 *
 * The watcher starts immediately but doesn't watch any paths until
 * agentite_watch_add_path() is called.
 *
 * @param config Configuration (NULL for defaults)
 * @return New watcher, or NULL on failure
 */
Agentite_FileWatcher *agentite_watch_create(const Agentite_FileWatcherConfig *config);

/**
 * Destroy a file watcher.
 * Stops the background monitoring thread and frees all resources.
 * Safe to pass NULL.
 *
 * @param watcher Watcher to destroy
 */
void agentite_watch_destroy(Agentite_FileWatcher *watcher);

/* ============================================================================
 * Watch Management
 * ============================================================================ */

/**
 * Add a directory path to watch.
 * The path must exist and be a directory.
 * Thread-safe.
 *
 * @param watcher File watcher
 * @param path    Directory path to watch
 * @return true on success, false on failure
 */
bool agentite_watch_add_path(Agentite_FileWatcher *watcher, const char *path);

/**
 * Remove a watched directory path.
 * Thread-safe.
 *
 * @param watcher File watcher
 * @param path    Directory path to stop watching
 * @return true if path was being watched and is now removed
 */
bool agentite_watch_remove_path(Agentite_FileWatcher *watcher, const char *path);

/**
 * Check if a path is currently being watched.
 * Thread-safe.
 *
 * @param watcher File watcher
 * @param path    Directory path to check
 * @return true if path is being watched
 */
bool agentite_watch_is_watching(const Agentite_FileWatcher *watcher, const char *path);

/**
 * Get the number of watched paths.
 * Thread-safe.
 *
 * @param watcher File watcher
 * @return Number of paths being watched
 */
size_t agentite_watch_path_count(const Agentite_FileWatcher *watcher);

/* ============================================================================
 * Event Processing
 * ============================================================================ */

/**
 * Process pending file watch events.
 * MUST be called on the main thread each frame.
 * Invokes the registered callback for each pending event.
 *
 * @param watcher File watcher
 */
void agentite_watch_update(Agentite_FileWatcher *watcher);

/**
 * Set the callback for watch events.
 * Only one callback can be registered at a time.
 * Pass NULL to unregister the callback.
 *
 * @param watcher  File watcher
 * @param callback Function to call for each event (NULL to unregister)
 * @param userdata User data passed to callback
 */
void agentite_watch_set_callback(Agentite_FileWatcher *watcher,
                                  Agentite_WatchCallback callback,
                                  void *userdata);

/**
 * Get the number of pending events waiting to be processed.
 * Thread-safe.
 *
 * @param watcher File watcher
 * @return Number of pending events
 */
size_t agentite_watch_pending_count(const Agentite_FileWatcher *watcher);

/**
 * Clear all pending events without processing them.
 *
 * @param watcher File watcher
 */
void agentite_watch_clear_pending(Agentite_FileWatcher *watcher);

/* ============================================================================
 * Configuration
 * ============================================================================ */

/**
 * Enable or disable file watching.
 * When disabled, no events are queued even if files change.
 * Useful for temporarily pausing monitoring during bulk operations.
 *
 * @param watcher File watcher
 * @param enabled true to enable, false to disable
 */
void agentite_watch_set_enabled(Agentite_FileWatcher *watcher, bool enabled);

/**
 * Check if file watching is enabled.
 *
 * @param watcher File watcher
 * @return true if enabled
 */
bool agentite_watch_is_enabled(const Agentite_FileWatcher *watcher);

/**
 * Set debounce time for coalescing rapid changes.
 * Changes to the same file within the debounce window are merged into one event.
 *
 * @param watcher     File watcher
 * @param debounce_ms Debounce time in milliseconds (0 to disable)
 */
void agentite_watch_set_debounce(Agentite_FileWatcher *watcher, uint32_t debounce_ms);

/* ============================================================================
 * Utility
 * ============================================================================ */

/**
 * Get a human-readable name for a watch event type.
 *
 * @param type Event type
 * @return Static string name
 */
const char *agentite_watch_event_type_name(Agentite_WatchEventType type);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_WATCH_H */
