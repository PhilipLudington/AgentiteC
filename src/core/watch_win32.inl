/**
 * Agentite Engine - File Watcher Windows Implementation
 *
 * Uses the ReadDirectoryChangesW API for efficient file system monitoring.
 * This is the standard Windows API for monitoring directory changes.
 *
 * This file is included directly into watch.cpp and should not be compiled separately.
 */

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <sys/stat.h>
#include <cstdio>

/* ============================================================================
 * Platform-Specific Types
 * ============================================================================ */

#define RDCW_BUFFER_SIZE 65536

/**
 * Per-path watch handle for Windows.
 */
typedef struct Win32PathHandle {
    char path[PATH_BUFFER_SIZE];
    HANDLE directory_handle;
    OVERLAPPED overlapped;
    uint8_t buffer[RDCW_BUFFER_SIZE];
    bool pending_read;
} Win32PathHandle;

/**
 * Windows-specific watch data.
 */
typedef struct Win32WatchData {
    HANDLE completion_port;
    Win32PathHandle **handles;
    size_t handle_count;
    size_t handle_capacity;
    SDL_Mutex *handles_mutex;
} Win32WatchData;

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

/**
 * Convert wide string to UTF-8.
 */
static void wide_to_utf8(const WCHAR *wide, char *utf8, size_t utf8_size)
{
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, utf8, (int)utf8_size, NULL, NULL);
}

/**
 * Convert UTF-8 to wide string.
 */
static void utf8_to_wide(const char *utf8, WCHAR *wide, size_t wide_count)
{
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide, (int)wide_count);
}

/**
 * Issue a new ReadDirectoryChangesW request.
 */
static bool issue_read_request(Win32PathHandle *handle, HANDLE completion_port)
{
    DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME |
                   FILE_NOTIFY_CHANGE_DIR_NAME |
                   FILE_NOTIFY_CHANGE_SIZE |
                   FILE_NOTIFY_CHANGE_LAST_WRITE |
                   FILE_NOTIFY_CHANGE_CREATION;

    memset(&handle->overlapped, 0, sizeof(handle->overlapped));
    handle->overlapped.hEvent = (HANDLE)handle;  /* Store handle pointer for callback */

    BOOL success = ReadDirectoryChangesW(
        handle->directory_handle,
        handle->buffer,
        RDCW_BUFFER_SIZE,
        TRUE,  /* Watch subtree */
        filter,
        NULL,
        &handle->overlapped,
        NULL
    );

    if (!success) {
        DWORD error = GetLastError();
        if (error != ERROR_IO_PENDING) {
            return false;
        }
    }

    handle->pending_read = true;
    return true;
}

/* ============================================================================
 * Platform Implementation
 * ============================================================================ */

/**
 * Initialize Windows-specific resources.
 */
static bool platform_init(Agentite_FileWatcher *watcher)
{
    Win32WatchData *data = (Win32WatchData *)calloc(1, sizeof(Win32WatchData));
    if (!data) {
        agentite_set_error("watch: failed to allocate Windows watch data");
        return false;
    }

    /* Create I/O completion port */
    data->completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 1);
    if (data->completion_port == NULL) {
        agentite_set_error("watch: CreateIoCompletionPort failed: %lu", GetLastError());
        free(data);
        return false;
    }

    /* Create mutex for handles array */
    data->handles_mutex = SDL_CreateMutex();
    if (!data->handles_mutex) {
        CloseHandle(data->completion_port);
        free(data);
        agentite_set_error("watch: failed to create handles mutex");
        return false;
    }

    /* Initial handles array */
    data->handle_capacity = 16;
    data->handles = (Win32PathHandle **)calloc(data->handle_capacity, sizeof(Win32PathHandle *));
    if (!data->handles) {
        SDL_DestroyMutex(data->handles_mutex);
        CloseHandle(data->completion_port);
        free(data);
        agentite_set_error("watch: failed to allocate handles array");
        return false;
    }

    watcher->platform_data = data;
    return true;
}

/**
 * Shutdown Windows-specific resources.
 */
static void platform_shutdown(Agentite_FileWatcher *watcher)
{
    Win32WatchData *data = (Win32WatchData *)watcher->platform_data;
    if (!data) return;

    /* Post completion status to wake up thread */
    if (data->completion_port) {
        PostQueuedCompletionStatus(data->completion_port, 0, 0, NULL);
        CloseHandle(data->completion_port);
    }

    if (data->handles_mutex) {
        SDL_DestroyMutex(data->handles_mutex);
    }

    free(data->handles);
    free(data);
    watcher->platform_data = NULL;
}

/**
 * Start watching a path on Windows.
 */
static void *platform_watch_path(Agentite_FileWatcher *watcher, const char *path)
{
    Win32WatchData *data = (Win32WatchData *)watcher->platform_data;
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

    /* Allocate handle */
    Win32PathHandle *handle = (Win32PathHandle *)calloc(1, sizeof(Win32PathHandle));
    if (!handle) {
        agentite_set_error("watch: failed to allocate path handle");
        return NULL;
    }
    strncpy(handle->path, path, sizeof(handle->path) - 1);

    /* Convert path to wide string */
    WCHAR wide_path[MAX_PATH];
    utf8_to_wide(path, wide_path, MAX_PATH);

    /* Open directory handle */
    handle->directory_handle = CreateFileW(
        wide_path,
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
        NULL
    );

    if (handle->directory_handle == INVALID_HANDLE_VALUE) {
        agentite_set_error("watch: CreateFileW failed for %s: %lu", path, GetLastError());
        free(handle);
        return NULL;
    }

    /* Associate with completion port */
    if (CreateIoCompletionPort(handle->directory_handle, data->completion_port,
                                (ULONG_PTR)handle, 0) == NULL) {
        agentite_set_error("watch: failed to associate with completion port: %lu", GetLastError());
        CloseHandle(handle->directory_handle);
        free(handle);
        return NULL;
    }

    /* Issue first read request */
    if (!issue_read_request(handle, data->completion_port)) {
        agentite_set_error("watch: ReadDirectoryChangesW failed: %lu", GetLastError());
        CloseHandle(handle->directory_handle);
        free(handle);
        return NULL;
    }

    /* Add to handles array */
    SDL_LockMutex(data->handles_mutex);
    if (data->handle_count >= data->handle_capacity) {
        size_t new_capacity = data->handle_capacity * 2;
        Win32PathHandle **new_handles = (Win32PathHandle **)realloc(
            data->handles, new_capacity * sizeof(Win32PathHandle *));
        if (new_handles) {
            data->handles = new_handles;
            data->handle_capacity = new_capacity;
        }
    }
    if (data->handle_count < data->handle_capacity) {
        data->handles[data->handle_count++] = handle;
    }
    SDL_UnlockMutex(data->handles_mutex);

    return handle;
}

/**
 * Stop watching a path on Windows.
 */
static void platform_unwatch_path(Agentite_FileWatcher *watcher, void *handle_ptr)
{
    Win32WatchData *data = (Win32WatchData *)watcher->platform_data;
    Win32PathHandle *handle = (Win32PathHandle *)handle_ptr;
    if (!data || !handle) return;

    /* Cancel pending I/O */
    if (handle->pending_read) {
        CancelIo(handle->directory_handle);
    }

    /* Close handle */
    CloseHandle(handle->directory_handle);

    /* Remove from array */
    SDL_LockMutex(data->handles_mutex);
    for (size_t i = 0; i < data->handle_count; i++) {
        if (data->handles[i] == handle) {
            data->handles[i] = data->handles[data->handle_count - 1];
            data->handle_count--;
            break;
        }
    }
    SDL_UnlockMutex(data->handles_mutex);

    free(handle);
}

/**
 * Background thread function for Windows.
 * Uses I/O completion ports for efficient event handling.
 */
static int watch_thread_func(void *userdata)
{
    Agentite_FileWatcher *watcher = (Agentite_FileWatcher *)userdata;
    Win32WatchData *data = (Win32WatchData *)watcher->platform_data;

    while (!watcher->shutdown.load()) {
        DWORD bytes_transferred;
        ULONG_PTR completion_key;
        LPOVERLAPPED overlapped = NULL;

        BOOL success = GetQueuedCompletionStatus(
            data->completion_port,
            &bytes_transferred,
            &completion_key,
            &overlapped,
            100  /* 100ms timeout */
        );

        if (!success) {
            if (GetLastError() == WAIT_TIMEOUT) {
                continue;
            }
            /* Other error - continue */
            continue;
        }

        /* Shutdown signal (NULL overlapped) */
        if (overlapped == NULL) {
            break;
        }

        /* Get handle from completion key */
        Win32PathHandle *handle = (Win32PathHandle *)completion_key;
        if (!handle || bytes_transferred == 0) {
            continue;
        }

        /* Process notifications */
        FILE_NOTIFY_INFORMATION *info = (FILE_NOTIFY_INFORMATION *)handle->buffer;
        while (info) {
            /* Convert filename to UTF-8 */
            char filename[PATH_BUFFER_SIZE];
            int len = WideCharToMultiByte(CP_UTF8, 0,
                info->FileName, info->FileNameLength / sizeof(WCHAR),
                filename, sizeof(filename) - 1, NULL, NULL);
            filename[len] = '\0';

            /* Replace backslashes with forward slashes */
            for (char *p = filename; *p; p++) {
                if (*p == '\\') *p = '/';
            }

            /* Determine event type */
            Agentite_WatchEventType type;
            switch (info->Action) {
                case FILE_ACTION_ADDED:
                    type = AGENTITE_WATCH_CREATED;
                    break;
                case FILE_ACTION_REMOVED:
                    type = AGENTITE_WATCH_DELETED;
                    break;
                case FILE_ACTION_MODIFIED:
                    type = AGENTITE_WATCH_MODIFIED;
                    break;
                case FILE_ACTION_RENAMED_OLD_NAME:
                case FILE_ACTION_RENAMED_NEW_NAME:
                    type = AGENTITE_WATCH_RENAMED;
                    break;
                default:
                    goto next_info;
            }

            /* Build full relative path */
            char relative_path[PATH_BUFFER_SIZE];
            snprintf(relative_path, sizeof(relative_path), "%s", filename);

            /* Notify watcher */
            agentite_watch_notify(watcher, type, relative_path, NULL);

        next_info:
            /* Move to next entry */
            if (info->NextEntryOffset == 0) {
                break;
            }
            info = (FILE_NOTIFY_INFORMATION *)((uint8_t *)info + info->NextEntryOffset);
        }

        /* Issue next read request */
        handle->pending_read = false;
        issue_read_request(handle, data->completion_port);
    }

    return 0;
}
