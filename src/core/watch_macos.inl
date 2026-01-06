/**
 * Agentite Engine - File Watcher macOS Implementation
 *
 * Uses the FSEvents API for efficient file system monitoring.
 * FSEvents is the same API used by Spotlight and Time Machine.
 *
 * This file is included directly into watch.cpp and should not be compiled separately.
 */

#include <CoreServices/CoreServices.h>
#include <dispatch/dispatch.h>
#include <sys/stat.h>

/* ============================================================================
 * Platform-Specific Types
 * ============================================================================ */

/**
 * macOS-specific watch data.
 */
typedef struct MacOSWatchData {
    FSEventStreamRef stream;
    dispatch_queue_t dispatch_queue;
    CFRunLoopRef run_loop;
} MacOSWatchData;

/**
 * Per-path watch handle for macOS.
 */
typedef struct MacOSPathHandle {
    char path[PATH_BUFFER_SIZE];
    FSEventStreamRef stream;
} MacOSPathHandle;

/* ============================================================================
 * FSEvents Callback
 * ============================================================================ */

/**
 * FSEvents callback - called when file system changes are detected.
 */
static void fsevents_callback(
    ConstFSEventStreamRef streamRef,
    void *clientCallBackInfo,
    size_t numEvents,
    void *eventPaths,
    const FSEventStreamEventFlags eventFlags[],
    const FSEventStreamEventId eventIds[])
{
    Agentite_FileWatcher *watcher = (Agentite_FileWatcher *)clientCallBackInfo;
    char **paths = (char **)eventPaths;

    for (size_t i = 0; i < numEvents; i++) {
        const char *path = paths[i];
        FSEventStreamEventFlags flags = eventFlags[i];

        /* Determine event type */
        Agentite_WatchEventType type;

        if (flags & kFSEventStreamEventFlagItemCreated) {
            type = AGENTITE_WATCH_CREATED;
        } else if (flags & kFSEventStreamEventFlagItemRemoved) {
            type = AGENTITE_WATCH_DELETED;
        } else if (flags & kFSEventStreamEventFlagItemRenamed) {
            type = AGENTITE_WATCH_RENAMED;
        } else if (flags & kFSEventStreamEventFlagItemModified) {
            type = AGENTITE_WATCH_MODIFIED;
        } else if (flags & kFSEventStreamEventFlagItemInodeMetaMod) {
            /* Metadata change (permissions, etc.) - treat as modified */
            type = AGENTITE_WATCH_MODIFIED;
        } else {
            /* Unknown or uninteresting event */
            continue;
        }

        /* Skip directories unless created/deleted */
        if (flags & kFSEventStreamEventFlagItemIsDir) {
            if (type != AGENTITE_WATCH_CREATED && type != AGENTITE_WATCH_DELETED) {
                continue;
            }
        }

        /* Find relative path from watched root */
        const char *relative_path = path;

        /* Check each watched path to find the root */
        SDL_LockMutex(watcher->paths_mutex);
        for (size_t j = 0; j < MAX_WATCHED_PATHS; j++) {
            if (!watcher->paths[j].active) continue;

            const char *root = watcher->paths[j].path;
            size_t root_len = strlen(root);

            if (strncmp(path, root, root_len) == 0) {
                /* Found matching root */
                relative_path = path + root_len;
                /* Skip leading slash */
                if (relative_path[0] == '/') {
                    relative_path++;
                }
                break;
            }
        }
        SDL_UnlockMutex(watcher->paths_mutex);

        /* Notify the watcher */
        agentite_watch_notify(watcher, type, relative_path, NULL);
    }
}

/* ============================================================================
 * Platform Implementation
 * ============================================================================ */

/**
 * Initialize macOS-specific resources.
 */
static bool platform_init(Agentite_FileWatcher *watcher)
{
    MacOSWatchData *data = (MacOSWatchData *)calloc(1, sizeof(MacOSWatchData));
    if (!data) {
        agentite_set_error("watch: failed to allocate macOS watch data");
        return false;
    }

    /* Create a dispatch queue for FSEvents callbacks */
    data->dispatch_queue = dispatch_queue_create("com.agentite.filewatcher",
                                                  DISPATCH_QUEUE_SERIAL);
    if (!data->dispatch_queue) {
        agentite_set_error("watch: failed to create dispatch queue");
        free(data);
        return false;
    }

    watcher->platform_data = data;
    return true;
}

/**
 * Shutdown macOS-specific resources.
 */
static void platform_shutdown(Agentite_FileWatcher *watcher)
{
    MacOSWatchData *data = (MacOSWatchData *)watcher->platform_data;
    if (!data) return;

    if (data->dispatch_queue) {
        dispatch_release(data->dispatch_queue);
    }

    free(data);
    watcher->platform_data = NULL;
}

/**
 * Start watching a path on macOS.
 */
static void *platform_watch_path(Agentite_FileWatcher *watcher, const char *path)
{
    MacOSWatchData *data = (MacOSWatchData *)watcher->platform_data;
    if (!data) {
        agentite_set_error("watch: platform not initialized");
        return NULL;
    }

    /* Verify path exists and is a directory */
    struct stat st;
    if (stat(path, &st) != 0) {
        agentite_set_error("watch: path does not exist: %s", path);
        return NULL;
    }
    if (!S_ISDIR(st.st_mode)) {
        agentite_set_error("watch: path is not a directory: %s", path);
        return NULL;
    }

    /* Allocate path handle */
    MacOSPathHandle *handle = (MacOSPathHandle *)calloc(1, sizeof(MacOSPathHandle));
    if (!handle) {
        agentite_set_error("watch: failed to allocate path handle");
        return NULL;
    }
    strncpy(handle->path, path, sizeof(handle->path) - 1);

    /* Create CFString for path */
    CFStringRef cf_path = CFStringCreateWithCString(kCFAllocatorDefault, path,
                                                     kCFStringEncodingUTF8);
    if (!cf_path) {
        agentite_set_error("watch: failed to create CFString for path");
        free(handle);
        return NULL;
    }

    /* Create path array */
    CFArrayRef path_array = CFArrayCreate(kCFAllocatorDefault,
                                          (const void **)&cf_path, 1,
                                          &kCFTypeArrayCallBacks);
    CFRelease(cf_path);

    if (!path_array) {
        agentite_set_error("watch: failed to create path array");
        free(handle);
        return NULL;
    }

    /* Set up stream context */
    FSEventStreamContext context = {
        .version = 0,
        .info = watcher,
        .retain = NULL,
        .release = NULL,
        .copyDescription = NULL
    };

    /* Create FSEvents stream */
    FSEventStreamCreateFlags flags = kFSEventStreamCreateFlagFileEvents |
                                      kFSEventStreamCreateFlagNoDefer;

    handle->stream = FSEventStreamCreate(
        kCFAllocatorDefault,
        fsevents_callback,
        &context,
        path_array,
        kFSEventStreamEventIdSinceNow,
        0.1,  /* Latency in seconds (100ms) */
        flags
    );

    CFRelease(path_array);

    if (!handle->stream) {
        agentite_set_error("watch: failed to create FSEvents stream for: %s", path);
        free(handle);
        return NULL;
    }

    /* Schedule stream on dispatch queue */
    FSEventStreamSetDispatchQueue(handle->stream, data->dispatch_queue);

    /* Start stream */
    if (!FSEventStreamStart(handle->stream)) {
        agentite_set_error("watch: failed to start FSEvents stream for: %s", path);
        FSEventStreamInvalidate(handle->stream);
        FSEventStreamRelease(handle->stream);
        free(handle);
        return NULL;
    }

    return handle;
}

/**
 * Stop watching a path on macOS.
 */
static void platform_unwatch_path(Agentite_FileWatcher *watcher, void *handle_ptr)
{
    (void)watcher;

    MacOSPathHandle *handle = (MacOSPathHandle *)handle_ptr;
    if (!handle) return;

    if (handle->stream) {
        FSEventStreamStop(handle->stream);
        FSEventStreamInvalidate(handle->stream);
        FSEventStreamRelease(handle->stream);
    }

    free(handle);
}

/**
 * Background thread function for macOS.
 * On macOS, FSEvents uses GCD dispatch queues, so this thread just sleeps.
 */
static int watch_thread_func(void *data)
{
    Agentite_FileWatcher *watcher = (Agentite_FileWatcher *)data;

    /* FSEvents callbacks are handled by the dispatch queue, not this thread.
     * This thread just keeps running to satisfy the watcher interface.
     * We could eliminate this thread entirely for macOS, but keeping it
     * maintains consistency with other platforms. */
    while (!watcher->shutdown.load()) {
        SDL_Delay(100);  /* Sleep 100ms */
    }

    return 0;
}
