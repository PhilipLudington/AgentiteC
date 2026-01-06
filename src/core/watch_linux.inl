/**
 * Agentite Engine - File Watcher Linux Implementation
 *
 * Uses the inotify API for efficient file system monitoring.
 * inotify is the standard Linux kernel interface for monitoring
 * file system events.
 *
 * This file is included directly into watch.cpp and should not be compiled separately.
 */

#include <sys/inotify.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include <errno.h>

/* ============================================================================
 * Platform-Specific Types
 * ============================================================================ */

/**
 * Maximum events to read at once.
 */
#define INOTIFY_BUFFER_SIZE (sizeof(struct inotify_event) + NAME_MAX + 1) * 64

/**
 * inotify watch descriptor entry.
 */
typedef struct InotifyWatch {
    int wd;                         /* Watch descriptor */
    char path[PATH_BUFFER_SIZE];    /* Full path being watched */
    bool active;
} InotifyWatch;

#define MAX_INOTIFY_WATCHES 1024

/**
 * Linux-specific watch data.
 */
typedef struct LinuxWatchData {
    int inotify_fd;                             /* inotify file descriptor */
    InotifyWatch watches[MAX_INOTIFY_WATCHES];  /* Watch descriptor mappings */
    size_t watch_count;
    SDL_Mutex *watches_mutex;
} LinuxWatchData;

/**
 * Per-path watch handle for Linux.
 */
typedef struct LinuxPathHandle {
    char root_path[PATH_BUFFER_SIZE];
    int *watch_descriptors;     /* Array of watch descriptors for this path tree */
    size_t wd_count;
    size_t wd_capacity;
} LinuxPathHandle;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Find watch entry by descriptor.
 */
static InotifyWatch *find_watch_by_wd(LinuxWatchData *data, int wd)
{
    for (size_t i = 0; i < MAX_INOTIFY_WATCHES; i++) {
        if (data->watches[i].active && data->watches[i].wd == wd) {
            return &data->watches[i];
        }
    }
    return NULL;
}

/**
 * Find empty watch slot.
 */
static InotifyWatch *find_empty_watch_slot(LinuxWatchData *data)
{
    for (size_t i = 0; i < MAX_INOTIFY_WATCHES; i++) {
        if (!data->watches[i].active) {
            return &data->watches[i];
        }
    }
    return NULL;
}

/**
 * Add a single directory to inotify.
 */
static int add_inotify_watch(Agentite_FileWatcher *watcher,
                              LinuxWatchData *data,
                              const char *path)
{
    uint32_t mask = IN_CREATE | IN_DELETE | IN_MODIFY |
                    IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE;

    int wd = inotify_add_watch(data->inotify_fd, path, mask);
    if (wd < 0) {
        if (errno == ENOSPC) {
            agentite_set_error("watch: inotify watch limit reached");
        } else {
            agentite_set_error("watch: inotify_add_watch failed for %s: %s",
                              path, strerror(errno));
        }
        return -1;
    }

    /* Record watch mapping */
    SDL_LockMutex(data->watches_mutex);
    InotifyWatch *watch = find_empty_watch_slot(data);
    if (watch) {
        watch->wd = wd;
        strncpy(watch->path, path, sizeof(watch->path) - 1);
        watch->active = true;
        data->watch_count++;
    }
    SDL_UnlockMutex(data->watches_mutex);

    return wd;
}

/**
 * Recursively add watches for a directory tree.
 */
static bool add_watches_recursive(Agentite_FileWatcher *watcher,
                                   LinuxWatchData *data,
                                   LinuxPathHandle *handle,
                                   const char *path)
{
    /* Add watch for this directory */
    int wd = add_inotify_watch(watcher, data, path);
    if (wd < 0) {
        return false;
    }

    /* Record in handle */
    if (handle->wd_count >= handle->wd_capacity) {
        size_t new_capacity = handle->wd_capacity * 2;
        if (new_capacity < 64) new_capacity = 64;
        int *new_wds = (int *)realloc(handle->watch_descriptors,
                                       new_capacity * sizeof(int));
        if (!new_wds) {
            return false;
        }
        handle->watch_descriptors = new_wds;
        handle->wd_capacity = new_capacity;
    }
    handle->watch_descriptors[handle->wd_count++] = wd;

    /* If not recursive, we're done */
    if (!watcher->config.recursive) {
        return true;
    }

    /* Scan subdirectories */
    DIR *dir = opendir(path);
    if (!dir) {
        return true;  /* Can't open, but main dir is watched */
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        /* Skip . and .. */
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char subpath[PATH_BUFFER_SIZE];
        snprintf(subpath, sizeof(subpath), "%s/%s", path, entry->d_name);

        struct stat st;
        if (stat(subpath, &st) == 0 && S_ISDIR(st.st_mode)) {
            /* Recursively add watches for subdirectory */
            add_watches_recursive(watcher, data, handle, subpath);
        }
    }

    closedir(dir);
    return true;
}

/* ============================================================================
 * Platform Implementation
 * ============================================================================ */

/**
 * Initialize Linux-specific resources.
 */
static bool platform_init(Agentite_FileWatcher *watcher)
{
    LinuxWatchData *data = (LinuxWatchData *)calloc(1, sizeof(LinuxWatchData));
    if (!data) {
        agentite_set_error("watch: failed to allocate Linux watch data");
        return false;
    }

    /* Create inotify instance */
    data->inotify_fd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (data->inotify_fd < 0) {
        agentite_set_error("watch: inotify_init failed: %s", strerror(errno));
        free(data);
        return false;
    }

    /* Create mutex for watch mappings */
    data->watches_mutex = SDL_CreateMutex();
    if (!data->watches_mutex) {
        close(data->inotify_fd);
        free(data);
        agentite_set_error("watch: failed to create watches mutex");
        return false;
    }

    watcher->platform_data = data;
    return true;
}

/**
 * Shutdown Linux-specific resources.
 */
static void platform_shutdown(Agentite_FileWatcher *watcher)
{
    LinuxWatchData *data = (LinuxWatchData *)watcher->platform_data;
    if (!data) return;

    if (data->inotify_fd >= 0) {
        close(data->inotify_fd);
    }

    if (data->watches_mutex) {
        SDL_DestroyMutex(data->watches_mutex);
    }

    free(data);
    watcher->platform_data = NULL;
}

/**
 * Start watching a path on Linux.
 */
static void *platform_watch_path(Agentite_FileWatcher *watcher, const char *path)
{
    LinuxWatchData *data = (LinuxWatchData *)watcher->platform_data;
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
    LinuxPathHandle *handle = (LinuxPathHandle *)calloc(1, sizeof(LinuxPathHandle));
    if (!handle) {
        agentite_set_error("watch: failed to allocate path handle");
        return NULL;
    }
    strncpy(handle->root_path, path, sizeof(handle->root_path) - 1);

    /* Add watches recursively */
    if (!add_watches_recursive(watcher, data, handle, path)) {
        /* Cleanup any watches we did add */
        for (size_t i = 0; i < handle->wd_count; i++) {
            inotify_rm_watch(data->inotify_fd, handle->watch_descriptors[i]);
        }
        free(handle->watch_descriptors);
        free(handle);
        return NULL;
    }

    return handle;
}

/**
 * Stop watching a path on Linux.
 */
static void platform_unwatch_path(Agentite_FileWatcher *watcher, void *handle_ptr)
{
    LinuxWatchData *data = (LinuxWatchData *)watcher->platform_data;
    LinuxPathHandle *handle = (LinuxPathHandle *)handle_ptr;
    if (!data || !handle) return;

    /* Remove all watch descriptors */
    SDL_LockMutex(data->watches_mutex);
    for (size_t i = 0; i < handle->wd_count; i++) {
        int wd = handle->watch_descriptors[i];
        inotify_rm_watch(data->inotify_fd, wd);

        /* Clear from mapping */
        InotifyWatch *watch = find_watch_by_wd(data, wd);
        if (watch) {
            watch->active = false;
            data->watch_count--;
        }
    }
    SDL_UnlockMutex(data->watches_mutex);

    free(handle->watch_descriptors);
    free(handle);
}

/**
 * Background thread function for Linux.
 * Reads inotify events and queues them for main thread processing.
 */
static int watch_thread_func(void *userdata)
{
    Agentite_FileWatcher *watcher = (Agentite_FileWatcher *)userdata;
    LinuxWatchData *data = (LinuxWatchData *)watcher->platform_data;

    char buffer[INOTIFY_BUFFER_SIZE];
    char rename_old_path[PATH_BUFFER_SIZE] = {0};
    uint32_t rename_cookie = 0;

    while (!watcher->shutdown.load()) {
        /* Read events with timeout using select */
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(data->inotify_fd, &fds);

        struct timeval timeout = { .tv_sec = 0, .tv_usec = 100000 };  /* 100ms */
        int ret = select(data->inotify_fd + 1, &fds, NULL, NULL, &timeout);

        if (ret <= 0) {
            continue;  /* Timeout or error */
        }

        ssize_t len = read(data->inotify_fd, buffer, sizeof(buffer));
        if (len <= 0) {
            continue;
        }

        /* Process events */
        for (char *ptr = buffer; ptr < buffer + len; ) {
            struct inotify_event *event = (struct inotify_event *)ptr;
            ptr += sizeof(struct inotify_event) + event->len;

            /* Skip events without a name (shouldn't happen with file events) */
            if (event->len == 0) {
                continue;
            }

            /* Find the watch path */
            SDL_LockMutex(data->watches_mutex);
            InotifyWatch *watch = find_watch_by_wd(data, event->wd);
            char full_path[PATH_BUFFER_SIZE] = {0};
            if (watch) {
                snprintf(full_path, sizeof(full_path), "%s/%s",
                        watch->path, event->name);
            }
            SDL_UnlockMutex(data->watches_mutex);

            if (full_path[0] == '\0') {
                continue;
            }

            /* Determine event type */
            Agentite_WatchEventType type;
            const char *old_path = NULL;

            if (event->mask & IN_CREATE) {
                type = AGENTITE_WATCH_CREATED;

                /* If a directory was created, add watch for it */
                if ((event->mask & IN_ISDIR) && watcher->config.recursive) {
                    /* Add watch for new directory */
                    /* Note: This is a simplified approach; a production implementation
                     * might need to handle this more carefully */
                }
            } else if (event->mask & IN_DELETE) {
                type = AGENTITE_WATCH_DELETED;
            } else if (event->mask & (IN_MODIFY | IN_CLOSE_WRITE)) {
                type = AGENTITE_WATCH_MODIFIED;
            } else if (event->mask & IN_MOVED_FROM) {
                /* First part of rename - save for pairing */
                rename_cookie = event->cookie;
                strncpy(rename_old_path, full_path, sizeof(rename_old_path) - 1);
                continue;
            } else if (event->mask & IN_MOVED_TO) {
                if (event->cookie == rename_cookie && rename_old_path[0] != '\0') {
                    /* Paired with MOVED_FROM - this is a rename */
                    type = AGENTITE_WATCH_RENAMED;
                    old_path = rename_old_path;
                    rename_cookie = 0;
                    rename_old_path[0] = '\0';
                } else {
                    /* Moved from outside watched area - treat as create */
                    type = AGENTITE_WATCH_CREATED;
                }
            } else {
                continue;
            }

            /* Find relative path */
            const char *relative_path = full_path;
            SDL_LockMutex(watcher->paths_mutex);
            for (size_t j = 0; j < MAX_WATCHED_PATHS; j++) {
                if (!watcher->paths[j].active) continue;
                const char *root = watcher->paths[j].path;
                size_t root_len = strlen(root);
                if (strncmp(full_path, root, root_len) == 0) {
                    relative_path = full_path + root_len;
                    if (relative_path[0] == '/') relative_path++;
                    break;
                }
            }
            SDL_UnlockMutex(watcher->paths_mutex);

            /* Handle relative old_path for renames */
            char relative_old[PATH_BUFFER_SIZE] = {0};
            if (old_path) {
                SDL_LockMutex(watcher->paths_mutex);
                for (size_t j = 0; j < MAX_WATCHED_PATHS; j++) {
                    if (!watcher->paths[j].active) continue;
                    const char *root = watcher->paths[j].path;
                    size_t root_len = strlen(root);
                    if (strncmp(old_path, root, root_len) == 0) {
                        const char *rel = old_path + root_len;
                        if (rel[0] == '/') rel++;
                        strncpy(relative_old, rel, sizeof(relative_old) - 1);
                        break;
                    }
                }
                SDL_UnlockMutex(watcher->paths_mutex);
                old_path = relative_old[0] ? relative_old : NULL;
            }

            /* Notify watcher */
            agentite_watch_notify(watcher, type, relative_path, old_path);
        }
    }

    return 0;
}
