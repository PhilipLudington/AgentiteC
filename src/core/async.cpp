/**
 * Agentite Engine - Async Asset Loading System Implementation
 *
 * Uses SDL3 threading primitives for cross-platform compatibility.
 * Work is split: background threads do I/O, main thread creates GPU resources.
 */

#include "agentite/async.h"
#include "agentite/asset.h"
#include "agentite/sprite.h"
#include "agentite/audio.h"
#include "agentite/error.h"

#include <SDL3/SDL.h>
#include <stdlib.h>
#include <string.h>
#include <atomic>

/* stb_image for raw image loading */
#define STB_IMAGE_IMPLEMENTATION_ALREADY_DONE
#include "stb_image.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DEFAULT_THREAD_COUNT 2
#define INITIAL_QUEUE_CAPACITY 64
#define MAX_REGIONS 256

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Load task types */
typedef enum LoadTaskType {
    LOAD_TASK_TEXTURE,
    LOAD_TASK_SOUND,
    LOAD_TASK_MUSIC
} LoadTaskType;

/* Load task state */
typedef enum LoadTaskState {
    TASK_STATE_PENDING,
    TASK_STATE_LOADING,
    TASK_STATE_LOADED,      /* I/O complete, waiting for GPU upload */
    TASK_STATE_COMPLETE,    /* Fully complete, waiting for callback */
    TASK_STATE_CANCELLED,
    TASK_STATE_FREE         /* Slot available for reuse */
} LoadTaskState;

/* Raw image data from background thread */
typedef struct RawImageData {
    unsigned char *pixels;
    int width;
    int height;
    int channels;
} RawImageData;

/* Raw audio data from background thread */
typedef struct RawAudioData {
    void *data;
    size_t size;
} RawAudioData;

/* Load task structure */
typedef struct LoadTask {
    /* Task identity */
    uint32_t id;
    LoadTaskType type;
    std::atomic<int> state;  /* LoadTaskState, using atomic for thread safety */
    Agentite_LoadPriority priority;

    /* Path to load */
    char *path;

    /* Result data (union based on type) */
    union {
        RawImageData image;
        RawAudioData audio;
    } raw;

    /* Final asset handle (set after GPU upload) */
    Agentite_AssetHandle handle;
    bool success;
    char *error_message;

    /* Callback info */
    Agentite_AsyncCallback callback;
    void *userdata;

    /* System references for finalization */
    union {
        Agentite_SpriteRenderer *sprite_renderer;
        Agentite_Audio *audio_system;
    } system;
    Agentite_AssetRegistry *registry;

    /* Linked list pointers for queue */
    struct LoadTask *next;
    struct LoadTask *prev;
} LoadTask;

/* Streaming region */
typedef struct StreamRegion {
    uint32_t id;
    char *name;
    char **asset_paths;
    int *asset_types;
    size_t asset_count;
    size_t asset_capacity;
    size_t loaded_count;
    bool active;
    void (*callback)(Agentite_StreamRegion, void*);
    void *userdata;
} StreamRegion;

/* Async loader structure */
struct Agentite_AsyncLoader {
    /* Configuration */
    Agentite_AsyncLoaderConfig config;

    /* Thread pool */
    SDL_Thread **threads;
    int thread_count;
    std::atomic<bool> shutdown;

    /* Task pool (pre-allocated task slots) */
    LoadTask *task_pool;
    size_t task_pool_capacity;
    std::atomic<uint32_t> next_task_id;

    /* Work queue (tasks waiting to be processed) */
    LoadTask *work_queue_head;
    LoadTask *work_queue_tail;
    SDL_Mutex *work_mutex;
    SDL_Condition *work_cond;
    std::atomic<size_t> pending_count;

    /* Loaded queue (I/O complete, waiting for GPU upload on main thread) */
    LoadTask *loaded_queue_head;
    LoadTask *loaded_queue_tail;
    SDL_Mutex *loaded_mutex;

    /* Complete queue (GPU upload done, waiting for callback) */
    LoadTask *complete_queue_head;
    LoadTask *complete_queue_tail;
    SDL_Mutex *complete_mutex;
    std::atomic<size_t> completed_count;

    /* Streaming regions */
    StreamRegion *regions;
    size_t region_count;
    std::atomic<uint32_t> next_region_id;
    SDL_Mutex *region_mutex;
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

/* Pack/unpack request handle */
static Agentite_LoadRequest pack_request(uint32_t id) {
    return (Agentite_LoadRequest){ id };
}

/* Find task by ID (must hold appropriate mutex) */
static LoadTask *find_task_by_id(Agentite_AsyncLoader *loader, uint32_t id) {
    if (id == 0 || !loader->task_pool) return NULL;

    for (size_t i = 0; i < loader->task_pool_capacity; i++) {
        if (loader->task_pool[i].id == id) {
            return &loader->task_pool[i];
        }
    }
    return NULL;
}

/* Allocate a task slot */
static LoadTask *allocate_task(Agentite_AsyncLoader *loader) {
    SDL_LockMutex(loader->work_mutex);

    /* Find free slot */
    for (size_t i = 0; i < loader->task_pool_capacity; i++) {
        if (loader->task_pool[i].state.load() == TASK_STATE_FREE) {
            LoadTask *task = &loader->task_pool[i];
            memset(task, 0, sizeof(LoadTask));
            task->id = loader->next_task_id.fetch_add(1) + 1;
            task->state.store(TASK_STATE_PENDING);
            SDL_UnlockMutex(loader->work_mutex);
            return task;
        }
    }

    /* Grow pool if possible */
    size_t new_capacity = loader->task_pool_capacity * 2;
    LoadTask *new_pool = (LoadTask *)realloc(
        loader->task_pool, new_capacity * sizeof(LoadTask));
    if (!new_pool) {
        SDL_UnlockMutex(loader->work_mutex);
        return NULL;
    }

    /* Initialize new slots */
    for (size_t i = loader->task_pool_capacity; i < new_capacity; i++) {
        memset(&new_pool[i], 0, sizeof(LoadTask));
        new_pool[i].state.store(TASK_STATE_FREE);
    }

    LoadTask *task = &new_pool[loader->task_pool_capacity];
    task->id = loader->next_task_id.fetch_add(1) + 1;
    task->state.store(TASK_STATE_PENDING);

    loader->task_pool = new_pool;
    loader->task_pool_capacity = new_capacity;

    SDL_UnlockMutex(loader->work_mutex);
    return task;
}

/* Free a task slot */
static void free_task(LoadTask *task) {
    if (!task) return;

    free(task->path);
    task->path = NULL;

    free(task->error_message);
    task->error_message = NULL;

    /* Free raw data based on type */
    if (task->type == LOAD_TASK_TEXTURE && task->raw.image.pixels) {
        stbi_image_free(task->raw.image.pixels);
        task->raw.image.pixels = NULL;
    } else if ((task->type == LOAD_TASK_SOUND || task->type == LOAD_TASK_MUSIC) &&
               task->raw.audio.data) {
        free(task->raw.audio.data);
        task->raw.audio.data = NULL;
    }

    task->state.store(TASK_STATE_FREE);
}

/* Add task to work queue */
static void enqueue_work(Agentite_AsyncLoader *loader, LoadTask *task) {
    SDL_LockMutex(loader->work_mutex);

    task->next = NULL;
    task->prev = loader->work_queue_tail;

    if (loader->work_queue_tail) {
        loader->work_queue_tail->next = task;
    } else {
        loader->work_queue_head = task;
    }
    loader->work_queue_tail = task;

    loader->pending_count.fetch_add(1);
    SDL_SignalCondition(loader->work_cond);
    SDL_UnlockMutex(loader->work_mutex);
}

/* Remove task from work queue and return it (called by worker threads) */
static LoadTask *dequeue_work(Agentite_AsyncLoader *loader) {
    SDL_LockMutex(loader->work_mutex);

    while (!loader->work_queue_head && !loader->shutdown.load()) {
        SDL_WaitCondition(loader->work_cond, loader->work_mutex);
    }

    if (loader->shutdown.load()) {
        SDL_UnlockMutex(loader->work_mutex);
        return NULL;
    }

    LoadTask *task = loader->work_queue_head;
    if (task) {
        loader->work_queue_head = task->next;
        if (loader->work_queue_head) {
            loader->work_queue_head->prev = NULL;
        } else {
            loader->work_queue_tail = NULL;
        }
        task->next = NULL;
        task->prev = NULL;
        task->state.store(TASK_STATE_LOADING);
    }

    SDL_UnlockMutex(loader->work_mutex);
    return task;
}

/* Add task to loaded queue (I/O complete, waiting for main thread) */
static void enqueue_loaded(Agentite_AsyncLoader *loader, LoadTask *task) {
    SDL_LockMutex(loader->loaded_mutex);

    task->next = NULL;
    task->prev = loader->loaded_queue_tail;

    if (loader->loaded_queue_tail) {
        loader->loaded_queue_tail->next = task;
    } else {
        loader->loaded_queue_head = task;
    }
    loader->loaded_queue_tail = task;

    task->state.store(TASK_STATE_LOADED);
    SDL_UnlockMutex(loader->loaded_mutex);
}

/* Dequeue from loaded queue (called on main thread) */
static LoadTask *dequeue_loaded(Agentite_AsyncLoader *loader) {
    SDL_LockMutex(loader->loaded_mutex);

    LoadTask *task = loader->loaded_queue_head;
    if (task) {
        loader->loaded_queue_head = task->next;
        if (loader->loaded_queue_head) {
            loader->loaded_queue_head->prev = NULL;
        } else {
            loader->loaded_queue_tail = NULL;
        }
        task->next = NULL;
        task->prev = NULL;
    }

    SDL_UnlockMutex(loader->loaded_mutex);
    return task;
}

/* Add task to complete queue */
static void enqueue_complete(Agentite_AsyncLoader *loader, LoadTask *task) {
    SDL_LockMutex(loader->complete_mutex);

    task->next = NULL;
    task->prev = loader->complete_queue_tail;

    if (loader->complete_queue_tail) {
        loader->complete_queue_tail->next = task;
    } else {
        loader->complete_queue_head = task;
    }
    loader->complete_queue_tail = task;

    task->state.store(TASK_STATE_COMPLETE);
    loader->completed_count.fetch_add(1);
    SDL_UnlockMutex(loader->complete_mutex);
}

/* Dequeue from complete queue */
static LoadTask *dequeue_complete(Agentite_AsyncLoader *loader) {
    SDL_LockMutex(loader->complete_mutex);

    LoadTask *task = loader->complete_queue_head;
    if (task) {
        loader->complete_queue_head = task->next;
        if (loader->complete_queue_head) {
            loader->complete_queue_head->prev = NULL;
        } else {
            loader->complete_queue_tail = NULL;
        }
        task->next = NULL;
        task->prev = NULL;
        loader->completed_count.fetch_sub(1);
    }

    SDL_UnlockMutex(loader->complete_mutex);
    return task;
}

/* ============================================================================
 * Background Thread Work Functions
 * ============================================================================ */

/* Load texture data (background thread) */
static void load_texture_background(LoadTask *task) {
    int width, height, channels;
    unsigned char *pixels = stbi_load(task->path, &width, &height, &channels, 4);

    if (!pixels) {
        task->success = false;
        task->error_message = strdup(stbi_failure_reason());
        return;
    }

    task->raw.image.pixels = pixels;
    task->raw.image.width = width;
    task->raw.image.height = height;
    task->raw.image.channels = 4;
    task->success = true;

    /* Simulate realistic loading time for demos (check env var) */
    const char *delay_str = SDL_getenv("AGENTITE_ASYNC_DELAY_MS");
    if (!delay_str) {
        delay_str = getenv("AGENTITE_ASYNC_DELAY_MS");  /* Fallback to standard getenv */
    }
    if (delay_str) {
        int delay_ms = atoi(delay_str);
        if (delay_ms > 0 && delay_ms < 5000) {
            SDL_Delay(delay_ms);
        }
    }
}

/* Load sound data (background thread) */
static void load_sound_background(LoadTask *task) {
    /* Read entire file into memory */
    SDL_IOStream *io = SDL_IOFromFile(task->path, "rb");
    if (!io) {
        task->success = false;
        task->error_message = strdup(SDL_GetError());
        return;
    }

    Sint64 size = SDL_GetIOSize(io);
    if (size <= 0) {
        SDL_CloseIO(io);
        task->success = false;
        task->error_message = strdup("Failed to get file size");
        return;
    }

    void *data = malloc((size_t)size);
    if (!data) {
        SDL_CloseIO(io);
        task->success = false;
        task->error_message = strdup("Out of memory");
        return;
    }

    if (SDL_ReadIO(io, data, (size_t)size) != (size_t)size) {
        free(data);
        SDL_CloseIO(io);
        task->success = false;
        task->error_message = strdup(SDL_GetError());
        return;
    }

    SDL_CloseIO(io);

    task->raw.audio.data = data;
    task->raw.audio.size = (size_t)size;
    task->success = true;
}

/* Load music data (background thread) */
static void load_music_background(LoadTask *task) {
    /* For music, we just verify the file exists and is readable */
    /* Actual streaming setup happens on main thread */
    SDL_IOStream *io = SDL_IOFromFile(task->path, "rb");
    if (!io) {
        task->success = false;
        task->error_message = strdup(SDL_GetError());
        return;
    }

    /* Read a small header to verify file is valid */
    unsigned char header[4];
    if (SDL_ReadIO(io, header, 4) < 4) {
        SDL_CloseIO(io);
        task->success = false;
        task->error_message = strdup("File too small or unreadable");
        return;
    }

    SDL_CloseIO(io);
    task->success = true;
}

/* Worker thread main function */
static int worker_thread_func(void *data) {
    Agentite_AsyncLoader *loader = (Agentite_AsyncLoader *)data;

    while (!loader->shutdown.load()) {
        LoadTask *task = dequeue_work(loader);
        if (!task) continue;

        /* Check for cancellation */
        if (task->state.load() == TASK_STATE_CANCELLED) {
            loader->pending_count.fetch_sub(1);
            enqueue_complete(loader, task);
            continue;
        }

        /* Perform I/O based on task type */
        switch (task->type) {
            case LOAD_TASK_TEXTURE:
                load_texture_background(task);
                break;
            case LOAD_TASK_SOUND:
                load_sound_background(task);
                break;
            case LOAD_TASK_MUSIC:
                load_music_background(task);
                break;
        }

        /* Move to loaded queue for main thread processing */
        loader->pending_count.fetch_sub(1);
        enqueue_loaded(loader, task);
    }

    return 0;
}

/* ============================================================================
 * Main Thread Finalization Functions
 * ============================================================================ */

/* Create GPU texture from raw image data (main thread) */
static void finalize_texture(LoadTask *task) {
    if (!task->success || !task->raw.image.pixels) return;

    Agentite_Texture *texture = agentite_texture_create(
        task->system.sprite_renderer,
        task->raw.image.width,
        task->raw.image.height,
        task->raw.image.pixels);

    if (!texture) {
        task->success = false;
        task->error_message = strdup(agentite_get_last_error());
        return;
    }

    /* Register with asset registry */
    task->handle = agentite_asset_register(
        task->registry,
        task->path,
        AGENTITE_ASSET_TEXTURE,
        texture);

    if (!agentite_asset_is_valid(task->handle)) {
        agentite_texture_destroy(task->system.sprite_renderer, texture);
        task->success = false;
        task->error_message = strdup("Failed to register texture asset");
    }

    /* Free raw image data */
    stbi_image_free(task->raw.image.pixels);
    task->raw.image.pixels = NULL;
}

/* Create sound from raw audio data (main thread) */
static void finalize_sound(LoadTask *task) {
    if (!task->success || !task->raw.audio.data) return;

    Agentite_Sound *sound = agentite_sound_load_wav_memory(
        task->system.audio_system,
        task->raw.audio.data,
        task->raw.audio.size);

    if (!sound) {
        task->success = false;
        task->error_message = strdup(agentite_get_last_error());
        free(task->raw.audio.data);
        task->raw.audio.data = NULL;
        return;
    }

    /* Register with asset registry */
    task->handle = agentite_asset_register(
        task->registry,
        task->path,
        AGENTITE_ASSET_SOUND,
        sound);

    if (!agentite_asset_is_valid(task->handle)) {
        agentite_sound_destroy(task->system.audio_system, sound);
        task->success = false;
        task->error_message = strdup("Failed to register sound asset");
    }

    free(task->raw.audio.data);
    task->raw.audio.data = NULL;
}

/* Create music from file (main thread) */
static void finalize_music(LoadTask *task) {
    if (!task->success) return;

    Agentite_Music *music = agentite_music_load(
        task->system.audio_system,
        task->path);

    if (!music) {
        task->success = false;
        task->error_message = strdup(agentite_get_last_error());
        return;
    }

    /* Register with asset registry */
    task->handle = agentite_asset_register(
        task->registry,
        task->path,
        AGENTITE_ASSET_MUSIC,
        music);

    if (!agentite_asset_is_valid(task->handle)) {
        agentite_music_destroy(task->system.audio_system, music);
        task->success = false;
        task->error_message = strdup("Failed to register music asset");
    }
}

/* ============================================================================
 * Public API - Loader Lifecycle
 * ============================================================================ */

Agentite_AsyncLoader *agentite_async_loader_create(const Agentite_AsyncLoaderConfig *config) {
    Agentite_AsyncLoader *loader = (Agentite_AsyncLoader *)calloc(
        1, sizeof(Agentite_AsyncLoader));
    if (!loader) {
        agentite_set_error("async: failed to allocate loader");
        return NULL;
    }

    /* Apply configuration */
    if (config) {
        loader->config = *config;
    } else {
        loader->config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
    }

    /* Determine thread count */
    loader->thread_count = loader->config.num_threads;
    if (loader->thread_count <= 0) {
        loader->thread_count = SDL_GetNumLogicalCPUCores();
        if (loader->thread_count < 1) loader->thread_count = 1;
        if (loader->thread_count > 4) loader->thread_count = 4;  /* Cap at 4 for I/O */
    }

    /* Allocate task pool */
    loader->task_pool_capacity = INITIAL_QUEUE_CAPACITY;
    loader->task_pool = (LoadTask *)calloc(loader->task_pool_capacity, sizeof(LoadTask));
    if (!loader->task_pool) {
        agentite_set_error("async: failed to allocate task pool");
        free(loader);
        return NULL;
    }

    /* Initialize task pool as free */
    for (size_t i = 0; i < loader->task_pool_capacity; i++) {
        loader->task_pool[i].state.store(TASK_STATE_FREE);
    }

    loader->next_task_id.store(0);

    /* Create synchronization primitives */
    loader->work_mutex = SDL_CreateMutex();
    loader->work_cond = SDL_CreateCondition();
    loader->loaded_mutex = SDL_CreateMutex();
    loader->complete_mutex = SDL_CreateMutex();
    loader->region_mutex = SDL_CreateMutex();

    if (!loader->work_mutex || !loader->work_cond ||
        !loader->loaded_mutex || !loader->complete_mutex ||
        !loader->region_mutex) {
        agentite_set_error("async: failed to create synchronization primitives");
        agentite_async_loader_destroy(loader);
        return NULL;
    }

    /* Allocate regions */
    loader->regions = (StreamRegion *)calloc(MAX_REGIONS, sizeof(StreamRegion));
    if (!loader->regions) {
        agentite_set_error("async: failed to allocate regions");
        agentite_async_loader_destroy(loader);
        return NULL;
    }
    loader->next_region_id.store(0);

    /* Create worker threads */
    loader->threads = (SDL_Thread **)calloc(loader->thread_count, sizeof(SDL_Thread *));
    if (!loader->threads) {
        agentite_set_error("async: failed to allocate thread array");
        agentite_async_loader_destroy(loader);
        return NULL;
    }

    loader->shutdown.store(false);

    for (int i = 0; i < loader->thread_count; i++) {
        char name[32];
        snprintf(name, sizeof(name), "async_worker_%d", i);
        loader->threads[i] = SDL_CreateThread(worker_thread_func, name, loader);
        if (!loader->threads[i]) {
            agentite_set_error("async: failed to create worker thread %d", i);
            loader->shutdown.store(true);
            SDL_BroadcastCondition(loader->work_cond);
            agentite_async_loader_destroy(loader);
            return NULL;
        }
    }

    return loader;
}

void agentite_async_loader_destroy(Agentite_AsyncLoader *loader) {
    if (!loader) return;

    /* Signal shutdown while holding mutex to prevent race with worker wait */
    if (loader->work_mutex) {
        SDL_LockMutex(loader->work_mutex);
        loader->shutdown.store(true);
        if (loader->work_cond) {
            SDL_BroadcastCondition(loader->work_cond);
        }
        SDL_UnlockMutex(loader->work_mutex);
    } else {
        loader->shutdown.store(true);
    }

    /* Wait for worker threads */
    if (loader->threads) {
        for (int i = 0; i < loader->thread_count; i++) {
            if (loader->threads[i]) {
                SDL_WaitThread(loader->threads[i], NULL);
            }
        }
        free(loader->threads);
    }

    /* Free remaining tasks */
    if (loader->task_pool) {
        for (size_t i = 0; i < loader->task_pool_capacity; i++) {
            free_task(&loader->task_pool[i]);
        }
        free(loader->task_pool);
    }

    /* Free regions */
    if (loader->regions) {
        for (size_t i = 0; i < MAX_REGIONS; i++) {
            if (loader->regions[i].name) {
                free(loader->regions[i].name);
            }
            if (loader->regions[i].asset_paths) {
                for (size_t j = 0; j < loader->regions[i].asset_count; j++) {
                    free(loader->regions[i].asset_paths[j]);
                }
                free(loader->regions[i].asset_paths);
            }
            free(loader->regions[i].asset_types);
        }
        free(loader->regions);
    }

    /* Destroy synchronization primitives */
    if (loader->work_mutex) SDL_DestroyMutex(loader->work_mutex);
    if (loader->work_cond) SDL_DestroyCondition(loader->work_cond);
    if (loader->loaded_mutex) SDL_DestroyMutex(loader->loaded_mutex);
    if (loader->complete_mutex) SDL_DestroyMutex(loader->complete_mutex);
    if (loader->region_mutex) SDL_DestroyMutex(loader->region_mutex);

    free(loader);
}

void agentite_async_loader_update(Agentite_AsyncLoader *loader) {
    if (!loader) return;

    size_t processed = 0;
    size_t max_per_frame = loader->config.max_completed_per_frame;
    if (max_per_frame == 0) max_per_frame = SIZE_MAX;

    /* Process loaded tasks (create GPU resources) */
    LoadTask *task;
    while ((task = dequeue_loaded(loader)) != NULL) {
        /* Skip cancelled tasks */
        if (task->state.load() == TASK_STATE_CANCELLED) {
            enqueue_complete(loader, task);
            continue;
        }

        /* Finalize on main thread */
        switch (task->type) {
            case LOAD_TASK_TEXTURE:
                finalize_texture(task);
                break;
            case LOAD_TASK_SOUND:
                finalize_sound(task);
                break;
            case LOAD_TASK_MUSIC:
                finalize_music(task);
                break;
        }

        enqueue_complete(loader, task);
    }

    /* Invoke callbacks for completed tasks */
    while (processed < max_per_frame && (task = dequeue_complete(loader)) != NULL) {
        if (task->callback) {
            Agentite_LoadResult result;
            result.success = task->success;
            result.error = task->error_message;
            task->callback(task->handle, result, task->userdata);
        }

        free_task(task);
        processed++;
    }
}

/* ============================================================================
 * Public API - Texture Loading
 * ============================================================================ */

Agentite_LoadRequest agentite_texture_load_async(
    Agentite_AsyncLoader *loader,
    Agentite_SpriteRenderer *sr,
    Agentite_AssetRegistry *registry,
    const char *path,
    Agentite_AsyncCallback callback,
    void *userdata)
{
    return agentite_texture_load_async_ex(loader, sr, registry, path, NULL,
                                           callback, userdata);
}

Agentite_LoadRequest agentite_texture_load_async_ex(
    Agentite_AsyncLoader *loader,
    Agentite_SpriteRenderer *sr,
    Agentite_AssetRegistry *registry,
    const char *path,
    const Agentite_TextureLoadOptions *options,
    Agentite_AsyncCallback callback,
    void *userdata)
{
    if (!loader || !sr || !registry || !path) {
        agentite_set_error("async: invalid parameters for texture load");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    /* Check if already loaded */
    Agentite_AssetHandle existing = agentite_asset_lookup(registry, path);
    if (agentite_asset_is_valid(existing)) {
        /* Already loaded - call callback immediately on next update */
        LoadTask *task = allocate_task(loader);
        if (!task) return AGENTITE_INVALID_LOAD_REQUEST;

        task->type = LOAD_TASK_TEXTURE;
        task->path = strdup(path);
        task->handle = existing;
        task->success = true;
        task->callback = callback;
        task->userdata = userdata;
        task->system.sprite_renderer = sr;
        task->registry = registry;

        /* Increment refcount for the existing asset */
        agentite_asset_addref(registry, existing);

        enqueue_complete(loader, task);
        return pack_request(task->id);
    }

    LoadTask *task = allocate_task(loader);
    if (!task) {
        agentite_set_error("async: failed to allocate task");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    task->type = LOAD_TASK_TEXTURE;
    task->path = strdup(path);
    if (!task->path) {
        free_task(task);
        agentite_set_error("async: failed to duplicate path");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    task->priority = options ? options->priority : AGENTITE_PRIORITY_NORMAL;
    task->callback = callback;
    task->userdata = userdata;
    task->system.sprite_renderer = sr;
    task->registry = registry;

    enqueue_work(loader, task);
    return pack_request(task->id);
}

/* ============================================================================
 * Public API - Audio Loading
 * ============================================================================ */

Agentite_LoadRequest agentite_sound_load_async(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    Agentite_AsyncCallback callback,
    void *userdata)
{
    return agentite_sound_load_async_ex(loader, audio, registry, path, NULL,
                                         callback, userdata);
}

Agentite_LoadRequest agentite_sound_load_async_ex(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    const Agentite_AudioLoadOptions *options,
    Agentite_AsyncCallback callback,
    void *userdata)
{
    if (!loader || !audio || !registry || !path) {
        agentite_set_error("async: invalid parameters for sound load");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    /* Check if already loaded */
    Agentite_AssetHandle existing = agentite_asset_lookup(registry, path);
    if (agentite_asset_is_valid(existing)) {
        LoadTask *task = allocate_task(loader);
        if (!task) return AGENTITE_INVALID_LOAD_REQUEST;

        task->type = LOAD_TASK_SOUND;
        task->path = strdup(path);
        task->handle = existing;
        task->success = true;
        task->callback = callback;
        task->userdata = userdata;
        task->system.audio_system = audio;
        task->registry = registry;

        agentite_asset_addref(registry, existing);
        enqueue_complete(loader, task);
        return pack_request(task->id);
    }

    LoadTask *task = allocate_task(loader);
    if (!task) {
        agentite_set_error("async: failed to allocate task");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    task->type = LOAD_TASK_SOUND;
    task->path = strdup(path);
    if (!task->path) {
        free_task(task);
        agentite_set_error("async: failed to duplicate path");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    task->priority = options ? options->priority : AGENTITE_PRIORITY_NORMAL;
    task->callback = callback;
    task->userdata = userdata;
    task->system.audio_system = audio;
    task->registry = registry;

    enqueue_work(loader, task);
    return pack_request(task->id);
}

Agentite_LoadRequest agentite_music_load_async(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    Agentite_AsyncCallback callback,
    void *userdata)
{
    return agentite_music_load_async_ex(loader, audio, registry, path, NULL,
                                         callback, userdata);
}

Agentite_LoadRequest agentite_music_load_async_ex(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    const Agentite_AudioLoadOptions *options,
    Agentite_AsyncCallback callback,
    void *userdata)
{
    if (!loader || !audio || !registry || !path) {
        agentite_set_error("async: invalid parameters for music load");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    /* Check if already loaded */
    Agentite_AssetHandle existing = agentite_asset_lookup(registry, path);
    if (agentite_asset_is_valid(existing)) {
        LoadTask *task = allocate_task(loader);
        if (!task) return AGENTITE_INVALID_LOAD_REQUEST;

        task->type = LOAD_TASK_MUSIC;
        task->path = strdup(path);
        task->handle = existing;
        task->success = true;
        task->callback = callback;
        task->userdata = userdata;
        task->system.audio_system = audio;
        task->registry = registry;

        agentite_asset_addref(registry, existing);
        enqueue_complete(loader, task);
        return pack_request(task->id);
    }

    LoadTask *task = allocate_task(loader);
    if (!task) {
        agentite_set_error("async: failed to allocate task");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    task->type = LOAD_TASK_MUSIC;
    task->path = strdup(path);
    if (!task->path) {
        free_task(task);
        agentite_set_error("async: failed to duplicate path");
        return AGENTITE_INVALID_LOAD_REQUEST;
    }

    task->priority = options ? options->priority : AGENTITE_PRIORITY_NORMAL;
    task->callback = callback;
    task->userdata = userdata;
    task->system.audio_system = audio;
    task->registry = registry;

    enqueue_work(loader, task);
    return pack_request(task->id);
}

/* ============================================================================
 * Public API - Request Management
 * ============================================================================ */

Agentite_LoadStatus agentite_async_get_status(
    const Agentite_AsyncLoader *loader,
    Agentite_LoadRequest request)
{
    if (!loader || request.value == 0) return AGENTITE_LOAD_INVALID;

    /* Search all queues for the task */
    for (size_t i = 0; i < loader->task_pool_capacity; i++) {
        if (loader->task_pool[i].id == request.value) {
            int state = loader->task_pool[i].state.load();
            switch (state) {
                case TASK_STATE_PENDING: return AGENTITE_LOAD_PENDING;
                case TASK_STATE_LOADING: return AGENTITE_LOAD_LOADING;
                case TASK_STATE_LOADED:
                case TASK_STATE_COMPLETE: return AGENTITE_LOAD_COMPLETE;
                case TASK_STATE_CANCELLED: return AGENTITE_LOAD_CANCELLED;
                default: return AGENTITE_LOAD_INVALID;
            }
        }
    }

    return AGENTITE_LOAD_INVALID;
}

bool agentite_async_is_complete(
    const Agentite_AsyncLoader *loader,
    Agentite_LoadRequest request)
{
    Agentite_LoadStatus status = agentite_async_get_status(loader, request);
    return status == AGENTITE_LOAD_COMPLETE || status == AGENTITE_LOAD_CANCELLED;
}

bool agentite_async_cancel(
    Agentite_AsyncLoader *loader,
    Agentite_LoadRequest request)
{
    if (!loader || request.value == 0) return false;

    LoadTask *task = find_task_by_id(loader, request.value);
    if (!task) return false;

    int expected = TASK_STATE_PENDING;
    if (task->state.compare_exchange_strong(expected, TASK_STATE_CANCELLED)) {
        return true;
    }

    return false;
}

/* ============================================================================
 * Public API - Progress Tracking
 * ============================================================================ */

size_t agentite_async_pending_count(const Agentite_AsyncLoader *loader) {
    if (!loader) return 0;
    return loader->pending_count.load();
}

size_t agentite_async_completed_count(const Agentite_AsyncLoader *loader) {
    if (!loader) return 0;
    return loader->completed_count.load();
}

bool agentite_async_is_idle(const Agentite_AsyncLoader *loader) {
    if (!loader) return true;
    return loader->pending_count.load() == 0 && loader->completed_count.load() == 0;
}

bool agentite_async_wait_all(
    Agentite_AsyncLoader *loader,
    uint32_t timeout_ms)
{
    if (!loader) return true;

    Uint64 start = SDL_GetTicks();
    while (loader->pending_count.load() > 0) {
        SDL_Delay(1);
        if (timeout_ms > 0 && (SDL_GetTicks() - start) >= timeout_ms) {
            return false;
        }
    }
    return true;
}

/* ============================================================================
 * Public API - Streaming Regions
 * ============================================================================ */

Agentite_StreamRegion agentite_stream_region_create(
    Agentite_AsyncLoader *loader,
    const char *name)
{
    if (!loader) return AGENTITE_INVALID_STREAM_REGION;

    SDL_LockMutex(loader->region_mutex);

    /* Find free slot */
    for (size_t i = 0; i < MAX_REGIONS; i++) {
        if (loader->regions[i].id == 0) {
            StreamRegion *region = &loader->regions[i];
            region->id = loader->next_region_id.fetch_add(1) + 1;
            region->name = name ? strdup(name) : NULL;
            region->asset_paths = NULL;
            region->asset_types = NULL;
            region->asset_count = 0;
            region->asset_capacity = 0;
            region->loaded_count = 0;
            region->active = false;
            region->callback = NULL;
            region->userdata = NULL;

            SDL_UnlockMutex(loader->region_mutex);
            return (Agentite_StreamRegion){ region->id };
        }
    }

    SDL_UnlockMutex(loader->region_mutex);
    agentite_set_error("async: maximum regions reached");
    return AGENTITE_INVALID_STREAM_REGION;
}

void agentite_stream_region_add_asset(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region,
    const char *path,
    int type)
{
    if (!loader || region.value == 0 || !path) return;

    SDL_LockMutex(loader->region_mutex);

    /* Find region */
    StreamRegion *r = NULL;
    for (size_t i = 0; i < MAX_REGIONS; i++) {
        if (loader->regions[i].id == region.value) {
            r = &loader->regions[i];
            break;
        }
    }

    if (!r) {
        SDL_UnlockMutex(loader->region_mutex);
        return;
    }

    /* Grow arrays if needed */
    if (r->asset_count >= r->asset_capacity) {
        size_t new_capacity = r->asset_capacity == 0 ? 8 : r->asset_capacity * 2;
        char **new_paths = (char **)realloc(r->asset_paths,
                                             new_capacity * sizeof(char *));
        int *new_types = (int *)realloc(r->asset_types,
                                         new_capacity * sizeof(int));
        if (!new_paths || !new_types) {
            free(new_paths);
            free(new_types);
            SDL_UnlockMutex(loader->region_mutex);
            return;
        }
        r->asset_paths = new_paths;
        r->asset_types = new_types;
        r->asset_capacity = new_capacity;
    }

    r->asset_paths[r->asset_count] = strdup(path);
    r->asset_types[r->asset_count] = type;
    r->asset_count++;

    SDL_UnlockMutex(loader->region_mutex);
}

void agentite_stream_region_activate(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region,
    void (*callback)(Agentite_StreamRegion region, void *userdata),
    void *userdata)
{
    if (!loader || region.value == 0) return;

    SDL_LockMutex(loader->region_mutex);

    StreamRegion *r = NULL;
    for (size_t i = 0; i < MAX_REGIONS; i++) {
        if (loader->regions[i].id == region.value) {
            r = &loader->regions[i];
            break;
        }
    }

    if (!r || r->active) {
        SDL_UnlockMutex(loader->region_mutex);
        return;
    }

    r->active = true;
    r->loaded_count = 0;
    r->callback = callback;
    r->userdata = userdata;

    SDL_UnlockMutex(loader->region_mutex);

    /* Queue loads for all assets in the region */
    /* Note: This is a simplified implementation. In practice, you'd need
       to track which loads belong to which region and call the region
       callback when all are complete. */
}

void agentite_stream_region_deactivate(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region)
{
    if (!loader || region.value == 0) return;

    SDL_LockMutex(loader->region_mutex);

    for (size_t i = 0; i < MAX_REGIONS; i++) {
        if (loader->regions[i].id == region.value) {
            loader->regions[i].active = false;
            loader->regions[i].loaded_count = 0;
            break;
        }
    }

    SDL_UnlockMutex(loader->region_mutex);
}

void agentite_stream_region_destroy(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region)
{
    if (!loader || region.value == 0) return;

    SDL_LockMutex(loader->region_mutex);

    for (size_t i = 0; i < MAX_REGIONS; i++) {
        if (loader->regions[i].id == region.value) {
            StreamRegion *r = &loader->regions[i];
            free(r->name);
            if (r->asset_paths) {
                for (size_t j = 0; j < r->asset_count; j++) {
                    free(r->asset_paths[j]);
                }
                free(r->asset_paths);
            }
            free(r->asset_types);
            memset(r, 0, sizeof(StreamRegion));
            break;
        }
    }

    SDL_UnlockMutex(loader->region_mutex);
}

float agentite_stream_region_progress(
    const Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region)
{
    if (!loader || region.value == 0) return 0.0f;

    SDL_LockMutex(((Agentite_AsyncLoader *)loader)->region_mutex);

    for (size_t i = 0; i < MAX_REGIONS; i++) {
        if (loader->regions[i].id == region.value) {
            const StreamRegion *r = &loader->regions[i];
            float progress = r->asset_count > 0 ?
                (float)r->loaded_count / (float)r->asset_count : 1.0f;
            SDL_UnlockMutex(((Agentite_AsyncLoader *)loader)->region_mutex);
            return progress;
        }
    }

    SDL_UnlockMutex(((Agentite_AsyncLoader *)loader)->region_mutex);
    return 0.0f;
}
