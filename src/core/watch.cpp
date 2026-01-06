/**
 * Agentite Engine - File Watcher System Implementation
 *
 * Cross-platform file system monitoring using native APIs:
 * - macOS: FSEvents
 * - Linux: inotify
 * - Windows: ReadDirectoryChangesW
 *
 * Architecture:
 * - Background thread monitors filesystem using platform APIs
 * - Events are queued thread-safely with debouncing
 * - Main thread polls events via agentite_watch_update()
 * - Callbacks invoked on main thread only
 */

#include "agentite/watch.h"
#include "agentite/error.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>

/* ============================================================================
 * Constants
 * ============================================================================ */

#define MAX_WATCHED_PATHS 64
#define DEFAULT_EVENT_QUEUE_CAPACITY 256
#define PATH_BUFFER_SIZE 512

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/**
 * Watched path entry.
 */
typedef struct WatchedPath {
    char path[PATH_BUFFER_SIZE];
    bool active;
    void *platform_handle;  /* Platform-specific watch handle */
} WatchedPath;

/**
 * Event queue entry with debounce support.
 */
typedef struct QueuedEvent {
    Agentite_WatchEvent event;
    uint64_t debounce_deadline;  /* When debounce period ends */
    bool pending;                /* True if waiting for debounce */
} QueuedEvent;

/**
 * File watcher structure.
 */
struct Agentite_FileWatcher {
    /* Configuration */
    Agentite_FileWatcherConfig config;

    /* Watched paths */
    WatchedPath paths[MAX_WATCHED_PATHS];
    size_t path_count;
    SDL_Mutex *paths_mutex;

    /* Background thread */
    SDL_Thread *watch_thread;
    std::atomic<bool> shutdown;
    std::atomic<bool> enabled;

    /* Event queue */
    QueuedEvent *event_queue;
    size_t event_queue_capacity;
    size_t event_queue_head;
    size_t event_queue_tail;
    std::atomic<size_t> pending_count;
    SDL_Mutex *event_mutex;

    /* Callback */
    Agentite_WatchCallback callback;
    void *callback_userdata;

    /* Platform-specific data */
    void *platform_data;
};

/* ============================================================================
 * Platform-Specific Declarations
 * ============================================================================ */

/* Platform-specific initialization */
static bool platform_init(Agentite_FileWatcher *watcher);
static void platform_shutdown(Agentite_FileWatcher *watcher);

/* Platform-specific path watching */
static void *platform_watch_path(Agentite_FileWatcher *watcher, const char *path);
static void platform_unwatch_path(Agentite_FileWatcher *watcher, void *handle);

/* Background thread entry point */
static int watch_thread_func(void *data);

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Get current time in milliseconds.
 */
static uint64_t get_time_ms(void)
{
    return SDL_GetTicks();
}

/**
 * Normalize a path (resolve to absolute, remove trailing slashes).
 */
static void normalize_path(const char *input, char *output, size_t output_size)
{
    /* Use realpath on POSIX, _fullpath on Windows */
#ifdef _WIN32
    char *result = _fullpath(output, input, output_size);
    if (!result) {
        strncpy(output, input, output_size - 1);
        output[output_size - 1] = '\0';
    }
#else
    char *result = realpath(input, NULL);
    if (result) {
        strncpy(output, result, output_size - 1);
        output[output_size - 1] = '\0';
        free(result);
    } else {
        strncpy(output, input, output_size - 1);
        output[output_size - 1] = '\0';
    }
#endif

    /* Remove trailing slash */
    size_t len = strlen(output);
    if (len > 1 && (output[len - 1] == '/' || output[len - 1] == '\\')) {
        output[len - 1] = '\0';
    }
}

/**
 * Find a watched path entry by path string.
 * Caller must hold paths_mutex.
 */
static WatchedPath *find_watched_path(Agentite_FileWatcher *watcher, const char *path)
{
    char normalized[PATH_BUFFER_SIZE];
    normalize_path(path, normalized, sizeof(normalized));

    for (size_t i = 0; i < MAX_WATCHED_PATHS; i++) {
        if (watcher->paths[i].active &&
            strcmp(watcher->paths[i].path, normalized) == 0) {
            return &watcher->paths[i];
        }
    }
    return NULL;
}

/**
 * Find an empty watched path slot.
 * Caller must hold paths_mutex.
 */
static WatchedPath *find_empty_path_slot(Agentite_FileWatcher *watcher)
{
    for (size_t i = 0; i < MAX_WATCHED_PATHS; i++) {
        if (!watcher->paths[i].active) {
            return &watcher->paths[i];
        }
    }
    return NULL;
}

/**
 * Queue an event with debouncing.
 * Thread-safe - called from background thread.
 */
static void queue_event(Agentite_FileWatcher *watcher, const Agentite_WatchEvent *event)
{
    if (!watcher->enabled.load()) {
        return;
    }

    SDL_LockMutex(watcher->event_mutex);

    /* Check if we should debounce (merge with existing event for same path) */
    if (watcher->config.debounce_ms > 0) {
        size_t idx = watcher->event_queue_tail;
        size_t count = 0;
        size_t capacity = watcher->event_queue_capacity;

        /* Search backwards for recent events on same path */
        while (count < watcher->pending_count.load() && count < capacity) {
            idx = (idx + capacity - 1) % capacity;
            QueuedEvent *queued = &watcher->event_queue[idx];

            if (queued->pending && strcmp(queued->event.path, event->path) == 0) {
                /* Update existing event instead of adding new one */
                queued->event.type = event->type;
                queued->event.timestamp = event->timestamp;
                queued->debounce_deadline = get_time_ms() + watcher->config.debounce_ms;
                SDL_UnlockMutex(watcher->event_mutex);
                return;
            }
            count++;
        }
    }

    /* Check capacity */
    size_t max_events = watcher->config.max_events;
    if (max_events > 0 && watcher->pending_count.load() >= max_events) {
        /* Queue full, drop oldest event */
        watcher->event_queue_head = (watcher->event_queue_head + 1) % watcher->event_queue_capacity;
        watcher->pending_count.fetch_sub(1);
    }

    /* Add new event */
    QueuedEvent *queued = &watcher->event_queue[watcher->event_queue_tail];
    queued->event = *event;
    queued->debounce_deadline = get_time_ms() + watcher->config.debounce_ms;
    queued->pending = true;

    watcher->event_queue_tail = (watcher->event_queue_tail + 1) % watcher->event_queue_capacity;
    watcher->pending_count.fetch_add(1);

    SDL_UnlockMutex(watcher->event_mutex);
}

/**
 * Called from platform code when a file change is detected.
 */
void agentite_watch_notify(Agentite_FileWatcher *watcher,
                           Agentite_WatchEventType type,
                           const char *path,
                           const char *old_path)
{
    if (!watcher || !path) return;

    Agentite_WatchEvent event;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.timestamp = get_time_ms();
    strncpy(event.path, path, sizeof(event.path) - 1);
    if (old_path) {
        strncpy(event.old_path, old_path, sizeof(event.old_path) - 1);
    }

    queue_event(watcher, &event);
}

/* ============================================================================
 * Platform Implementations
 * ============================================================================ */

#if defined(__APPLE__)
    #include "watch_macos.inl"
#elif defined(__linux__)
    #include "watch_linux.inl"
#elif defined(_WIN32)
    #include "watch_win32.inl"
#else
    /* Stub implementation for unsupported platforms */
    static bool platform_init(Agentite_FileWatcher *watcher) {
        agentite_set_error("watch: file watching not supported on this platform");
        return false;
    }
    static void platform_shutdown(Agentite_FileWatcher *watcher) {}
    static void *platform_watch_path(Agentite_FileWatcher *watcher, const char *path) {
        return NULL;
    }
    static void platform_unwatch_path(Agentite_FileWatcher *watcher, void *handle) {}
    static int watch_thread_func(void *data) { return 0; }
#endif

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

Agentite_FileWatcher *agentite_watch_create(const Agentite_FileWatcherConfig *config)
{
    Agentite_FileWatcher *watcher = (Agentite_FileWatcher *)calloc(1, sizeof(Agentite_FileWatcher));
    if (!watcher) {
        agentite_set_error("watch: failed to allocate watcher");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        watcher->config = *config;
    } else {
        watcher->config = AGENTITE_FILE_WATCHER_CONFIG_DEFAULT;
    }

    /* Initialize atomics */
    watcher->shutdown.store(false);
    watcher->enabled.store(true);
    watcher->pending_count.store(0);

    /* Create mutexes */
    watcher->paths_mutex = SDL_CreateMutex();
    watcher->event_mutex = SDL_CreateMutex();
    if (!watcher->paths_mutex || !watcher->event_mutex) {
        agentite_set_error("watch: failed to create mutexes");
        goto cleanup;
    }

    /* Allocate event queue */
    watcher->event_queue_capacity = DEFAULT_EVENT_QUEUE_CAPACITY;
    if (watcher->config.max_events > 0 && watcher->config.max_events < DEFAULT_EVENT_QUEUE_CAPACITY) {
        watcher->event_queue_capacity = watcher->config.max_events;
    }
    watcher->event_queue = (QueuedEvent *)calloc(watcher->event_queue_capacity, sizeof(QueuedEvent));
    if (!watcher->event_queue) {
        agentite_set_error("watch: failed to allocate event queue");
        goto cleanup;
    }

    /* Initialize platform-specific resources */
    if (!platform_init(watcher)) {
        goto cleanup;
    }

    /* Start background thread */
    watcher->watch_thread = SDL_CreateThread(watch_thread_func, "FileWatcher", watcher);
    if (!watcher->watch_thread) {
        agentite_set_error("watch: failed to create watch thread: %s", SDL_GetError());
        platform_shutdown(watcher);
        goto cleanup;
    }

    return watcher;

cleanup:
    if (watcher->event_queue) free(watcher->event_queue);
    if (watcher->event_mutex) SDL_DestroyMutex(watcher->event_mutex);
    if (watcher->paths_mutex) SDL_DestroyMutex(watcher->paths_mutex);
    free(watcher);
    return NULL;
}

void agentite_watch_destroy(Agentite_FileWatcher *watcher)
{
    if (!watcher) return;

    /* Signal shutdown */
    watcher->shutdown.store(true);

    /* Wait for thread to exit */
    if (watcher->watch_thread) {
        SDL_WaitThread(watcher->watch_thread, NULL);
    }

    /* Cleanup platform resources */
    platform_shutdown(watcher);

    /* Unwatch all paths */
    SDL_LockMutex(watcher->paths_mutex);
    for (size_t i = 0; i < MAX_WATCHED_PATHS; i++) {
        if (watcher->paths[i].active && watcher->paths[i].platform_handle) {
            platform_unwatch_path(watcher, watcher->paths[i].platform_handle);
        }
    }
    SDL_UnlockMutex(watcher->paths_mutex);

    /* Free resources */
    free(watcher->event_queue);
    SDL_DestroyMutex(watcher->event_mutex);
    SDL_DestroyMutex(watcher->paths_mutex);
    free(watcher);
}

/* ============================================================================
 * Watch Management
 * ============================================================================ */

bool agentite_watch_add_path(Agentite_FileWatcher *watcher, const char *path)
{
    if (!watcher || !path) {
        agentite_set_error("watch: invalid parameters");
        return false;
    }

    char normalized[PATH_BUFFER_SIZE];
    normalize_path(path, normalized, sizeof(normalized));

    SDL_LockMutex(watcher->paths_mutex);

    /* Check if already watching */
    if (find_watched_path(watcher, normalized)) {
        SDL_UnlockMutex(watcher->paths_mutex);
        return true;  /* Already watching, not an error */
    }

    /* Find empty slot */
    WatchedPath *slot = find_empty_path_slot(watcher);
    if (!slot) {
        SDL_UnlockMutex(watcher->paths_mutex);
        agentite_set_error("watch: maximum watched paths reached (%d)", MAX_WATCHED_PATHS);
        return false;
    }

    /* Start watching */
    void *handle = platform_watch_path(watcher, normalized);
    if (!handle) {
        SDL_UnlockMutex(watcher->paths_mutex);
        /* Error already set by platform_watch_path */
        return false;
    }

    /* Record the path */
    strncpy(slot->path, normalized, sizeof(slot->path) - 1);
    slot->platform_handle = handle;
    slot->active = true;
    watcher->path_count++;

    SDL_UnlockMutex(watcher->paths_mutex);
    return true;
}

bool agentite_watch_remove_path(Agentite_FileWatcher *watcher, const char *path)
{
    if (!watcher || !path) return false;

    SDL_LockMutex(watcher->paths_mutex);

    WatchedPath *watched = find_watched_path(watcher, path);
    if (!watched) {
        SDL_UnlockMutex(watcher->paths_mutex);
        return false;
    }

    /* Stop watching */
    if (watched->platform_handle) {
        platform_unwatch_path(watcher, watched->platform_handle);
    }

    /* Clear slot */
    memset(watched, 0, sizeof(*watched));
    watcher->path_count--;

    SDL_UnlockMutex(watcher->paths_mutex);
    return true;
}

bool agentite_watch_is_watching(const Agentite_FileWatcher *watcher, const char *path)
{
    if (!watcher || !path) return false;

    SDL_LockMutex(((Agentite_FileWatcher *)watcher)->paths_mutex);
    bool result = find_watched_path((Agentite_FileWatcher *)watcher, path) != NULL;
    SDL_UnlockMutex(((Agentite_FileWatcher *)watcher)->paths_mutex);

    return result;
}

size_t agentite_watch_path_count(const Agentite_FileWatcher *watcher)
{
    if (!watcher) return 0;
    return watcher->path_count;
}

/* ============================================================================
 * Event Processing
 * ============================================================================ */

void agentite_watch_update(Agentite_FileWatcher *watcher)
{
    if (!watcher) return;

    uint64_t now = get_time_ms();

    SDL_LockMutex(watcher->event_mutex);

    /* Process events whose debounce period has expired */
    while (watcher->pending_count.load() > 0) {
        QueuedEvent *queued = &watcher->event_queue[watcher->event_queue_head];

        /* Check if debounce period has expired */
        if (queued->pending && now < queued->debounce_deadline) {
            /* Still waiting for debounce, stop processing */
            break;
        }

        /* Remove from queue */
        queued->pending = false;
        watcher->event_queue_head = (watcher->event_queue_head + 1) % watcher->event_queue_capacity;
        watcher->pending_count.fetch_sub(1);

        /* Copy event data before releasing lock */
        Agentite_WatchEvent event = queued->event;

        /* Release lock during callback */
        SDL_UnlockMutex(watcher->event_mutex);

        /* Invoke callback */
        if (watcher->callback) {
            watcher->callback(&event, watcher->callback_userdata);
        }

        SDL_LockMutex(watcher->event_mutex);
    }

    SDL_UnlockMutex(watcher->event_mutex);
}

void agentite_watch_set_callback(Agentite_FileWatcher *watcher,
                                  Agentite_WatchCallback callback,
                                  void *userdata)
{
    if (!watcher) return;
    watcher->callback = callback;
    watcher->callback_userdata = userdata;
}

size_t agentite_watch_pending_count(const Agentite_FileWatcher *watcher)
{
    if (!watcher) return 0;
    return watcher->pending_count.load();
}

void agentite_watch_clear_pending(Agentite_FileWatcher *watcher)
{
    if (!watcher) return;

    SDL_LockMutex(watcher->event_mutex);
    watcher->event_queue_head = 0;
    watcher->event_queue_tail = 0;
    watcher->pending_count.store(0);
    for (size_t i = 0; i < watcher->event_queue_capacity; i++) {
        watcher->event_queue[i].pending = false;
    }
    SDL_UnlockMutex(watcher->event_mutex);
}

/* ============================================================================
 * Configuration
 * ============================================================================ */

void agentite_watch_set_enabled(Agentite_FileWatcher *watcher, bool enabled)
{
    if (!watcher) return;
    watcher->enabled.store(enabled);
}

bool agentite_watch_is_enabled(const Agentite_FileWatcher *watcher)
{
    if (!watcher) return false;
    return watcher->enabled.load();
}

void agentite_watch_set_debounce(Agentite_FileWatcher *watcher, uint32_t debounce_ms)
{
    if (!watcher) return;
    watcher->config.debounce_ms = debounce_ms;
}

/* ============================================================================
 * Utility
 * ============================================================================ */

const char *agentite_watch_event_type_name(Agentite_WatchEventType type)
{
    switch (type) {
        case AGENTITE_WATCH_CREATED:  return "CREATED";
        case AGENTITE_WATCH_MODIFIED: return "MODIFIED";
        case AGENTITE_WATCH_DELETED:  return "DELETED";
        case AGENTITE_WATCH_RENAMED:  return "RENAMED";
        default:                       return "UNKNOWN";
    }
}
