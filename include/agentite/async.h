/**
 * Agentite Engine - Async Asset Loading System
 *
 * Provides background loading of assets with completion callbacks.
 * Handles the SDL3 requirement that GPU resources must be created on the main thread
 * by splitting work: I/O on background threads, GPU upload on main thread.
 *
 * Usage:
 *   Agentite_AsyncLoader *loader = agentite_async_loader_create(2);  // 2 worker threads
 *
 *   // Queue async texture load
 *   agentite_texture_load_async(loader, sprite_renderer, registry,
 *       "sprites/player.png", on_texture_loaded, userdata);
 *
 *   // In game loop - MUST be called each frame on main thread
 *   while (running) {
 *       agentite_async_loader_update(loader);  // Processes completed loads
 *       // ... rest of frame
 *   }
 *
 *   agentite_async_loader_destroy(loader);
 *
 * Thread Safety:
 *   - agentite_async_loader_create/destroy: NOT thread-safe, main thread only
 *   - agentite_async_loader_update: NOT thread-safe, main thread only
 *   - agentite_*_load_async: Thread-safe, can be called from any thread
 *   - agentite_async_cancel: Thread-safe
 *   - agentite_async_is_complete: Thread-safe
 *   - Callbacks are always invoked on the main thread during update()
 */

#ifndef AGENTITE_ASYNC_H
#define AGENTITE_ASYNC_H

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
 * Opaque async loader handle.
 * Manages a thread pool for background I/O and a queue for main-thread callbacks.
 */
typedef struct Agentite_AsyncLoader Agentite_AsyncLoader;

/**
 * Load request handle.
 * Used to track, query, or cancel pending loads.
 */
typedef struct Agentite_LoadRequest {
    uint32_t value;  /* Packed request ID (0 = invalid) */
} Agentite_LoadRequest;

/** Invalid request constant */
#define AGENTITE_INVALID_LOAD_REQUEST ((Agentite_LoadRequest){ 0 })

/**
 * Check if load request handle is valid (not null).
 * Does NOT indicate whether the load completed or succeeded.
 */
static inline bool agentite_load_request_is_valid(Agentite_LoadRequest request) {
    return request.value != 0;
}

/**
 * Load result passed to completion callback.
 */
typedef struct Agentite_LoadResult {
    bool success;                    /* True if load succeeded */
    const char *error;               /* Error message if failed (NULL on success) */
} Agentite_LoadResult;

/**
 * Forward declarations for systems that integrate with async loading.
 */
typedef struct Agentite_AssetRegistry Agentite_AssetRegistry;
typedef struct Agentite_AssetHandle Agentite_AssetHandle;
typedef struct Agentite_SpriteRenderer Agentite_SpriteRenderer;
typedef struct Agentite_Audio Agentite_Audio;

/**
 * Async load completion callback.
 * Called on the main thread during agentite_async_loader_update().
 *
 * @param handle   Asset handle (AGENTITE_INVALID_ASSET_HANDLE if load failed)
 * @param result   Load result with success flag and error message
 * @param userdata User-provided context from the load request
 */
typedef void (*Agentite_AsyncCallback)(Agentite_AssetHandle handle,
                                        Agentite_LoadResult result,
                                        void *userdata);

/**
 * Load priority levels.
 * Higher priority loads are processed first.
 */
typedef enum Agentite_LoadPriority {
    AGENTITE_PRIORITY_LOW = 0,       /* Background preloading */
    AGENTITE_PRIORITY_NORMAL = 1,    /* Standard loading */
    AGENTITE_PRIORITY_HIGH = 2,      /* Immediate need */
    AGENTITE_PRIORITY_CRITICAL = 3   /* Required for current frame */
} Agentite_LoadPriority;

/**
 * Async loader configuration.
 */
typedef struct Agentite_AsyncLoaderConfig {
    int num_threads;                 /* Worker thread count (0 = auto-detect CPU cores) */
    size_t max_pending;              /* Maximum pending requests (0 = unlimited) */
    size_t max_completed_per_frame;  /* Max callbacks per update() call (0 = unlimited) */
} Agentite_AsyncLoaderConfig;

/** Default configuration */
#define AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT \
    ((Agentite_AsyncLoaderConfig){ .num_threads = 0, .max_pending = 0, .max_completed_per_frame = 0 })

/* ============================================================================
 * Loader Lifecycle
 * ============================================================================ */

/**
 * Create an async loader with the given configuration.
 * Caller OWNS the returned pointer and MUST call agentite_async_loader_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return New loader, or NULL on failure
 */
Agentite_AsyncLoader *agentite_async_loader_create(const Agentite_AsyncLoaderConfig *config);

/**
 * Destroy an async loader.
 * Waits for pending loads to complete and calls their callbacks with failure.
 * Safe to pass NULL.
 *
 * @param loader Loader to destroy
 */
void agentite_async_loader_destroy(Agentite_AsyncLoader *loader);

/**
 * Process completed loads and invoke callbacks.
 * MUST be called on the main thread each frame.
 * GPU resources are created here (not in background threads).
 *
 * @param loader Async loader
 */
void agentite_async_loader_update(Agentite_AsyncLoader *loader);

/* ============================================================================
 * Texture Async Loading
 * ============================================================================ */

/**
 * Extended options for async texture loading.
 */
typedef struct Agentite_TextureLoadOptions {
    Agentite_LoadPriority priority;  /* Load priority */
} Agentite_TextureLoadOptions;

/** Default texture load options */
#define AGENTITE_TEXTURE_LOAD_OPTIONS_DEFAULT \
    ((Agentite_TextureLoadOptions){ .priority = AGENTITE_PRIORITY_NORMAL })

/**
 * Queue an async texture load.
 * The image file is read in a background thread, then the GPU texture is
 * created on the main thread during the next update() call.
 *
 * @param loader    Async loader
 * @param sr        Sprite renderer (for GPU texture creation)
 * @param registry  Asset registry (for handle management)
 * @param path      File path to load
 * @param callback  Completion callback (called on main thread)
 * @param userdata  User context passed to callback
 * @return Load request handle, or AGENTITE_INVALID_LOAD_REQUEST on failure
 */
Agentite_LoadRequest agentite_texture_load_async(
    Agentite_AsyncLoader *loader,
    Agentite_SpriteRenderer *sr,
    Agentite_AssetRegistry *registry,
    const char *path,
    Agentite_AsyncCallback callback,
    void *userdata);

/**
 * Queue an async texture load with options.
 *
 * @param loader    Async loader
 * @param sr        Sprite renderer
 * @param registry  Asset registry
 * @param path      File path
 * @param options   Load options (NULL for defaults)
 * @param callback  Completion callback
 * @param userdata  User context
 * @return Load request handle
 */
Agentite_LoadRequest agentite_texture_load_async_ex(
    Agentite_AsyncLoader *loader,
    Agentite_SpriteRenderer *sr,
    Agentite_AssetRegistry *registry,
    const char *path,
    const Agentite_TextureLoadOptions *options,
    Agentite_AsyncCallback callback,
    void *userdata);

/* ============================================================================
 * Audio Async Loading
 * ============================================================================ */

/**
 * Extended options for async audio loading.
 */
typedef struct Agentite_AudioLoadOptions {
    Agentite_LoadPriority priority;  /* Load priority */
} Agentite_AudioLoadOptions;

/** Default audio load options */
#define AGENTITE_AUDIO_LOAD_OPTIONS_DEFAULT \
    ((Agentite_AudioLoadOptions){ .priority = AGENTITE_PRIORITY_NORMAL })

/**
 * Queue an async sound load.
 * Sound files are fully loaded into memory in a background thread.
 *
 * @param loader    Async loader
 * @param audio     Audio system
 * @param registry  Asset registry
 * @param path      File path
 * @param callback  Completion callback
 * @param userdata  User context
 * @return Load request handle
 */
Agentite_LoadRequest agentite_sound_load_async(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    Agentite_AsyncCallback callback,
    void *userdata);

/**
 * Queue an async sound load with options.
 */
Agentite_LoadRequest agentite_sound_load_async_ex(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    const Agentite_AudioLoadOptions *options,
    Agentite_AsyncCallback callback,
    void *userdata);

/**
 * Queue an async music load.
 * Music metadata is loaded in background; streaming setup is main-thread.
 *
 * @param loader    Async loader
 * @param audio     Audio system
 * @param registry  Asset registry
 * @param path      File path
 * @param callback  Completion callback
 * @param userdata  User context
 * @return Load request handle
 */
Agentite_LoadRequest agentite_music_load_async(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    Agentite_AsyncCallback callback,
    void *userdata);

/**
 * Queue an async music load with options.
 */
Agentite_LoadRequest agentite_music_load_async_ex(
    Agentite_AsyncLoader *loader,
    Agentite_Audio *audio,
    Agentite_AssetRegistry *registry,
    const char *path,
    const Agentite_AudioLoadOptions *options,
    Agentite_AsyncCallback callback,
    void *userdata);

/* ============================================================================
 * Request Management
 * ============================================================================ */

/**
 * Load request status.
 */
typedef enum Agentite_LoadStatus {
    AGENTITE_LOAD_INVALID = 0,   /* Invalid or expired request */
    AGENTITE_LOAD_PENDING,       /* Waiting in queue */
    AGENTITE_LOAD_LOADING,       /* Currently being processed */
    AGENTITE_LOAD_COMPLETE,      /* Completed (callback not yet called) */
    AGENTITE_LOAD_CANCELLED      /* Cancelled by user */
} Agentite_LoadStatus;

/**
 * Get the status of a load request.
 * Thread-safe.
 *
 * @param loader  Async loader
 * @param request Load request handle
 * @return Current status
 */
Agentite_LoadStatus agentite_async_get_status(
    const Agentite_AsyncLoader *loader,
    Agentite_LoadRequest request);

/**
 * Check if a load request has completed (successfully or not).
 * Thread-safe.
 *
 * @param loader  Async loader
 * @param request Load request handle
 * @return true if complete or cancelled
 */
bool agentite_async_is_complete(
    const Agentite_AsyncLoader *loader,
    Agentite_LoadRequest request);

/**
 * Cancel a pending load request.
 * If the load is already in progress or complete, this has no effect.
 * The callback will still be called with success=false.
 * Thread-safe.
 *
 * @param loader  Async loader
 * @param request Load request handle
 * @return true if successfully cancelled, false if already complete/invalid
 */
bool agentite_async_cancel(
    Agentite_AsyncLoader *loader,
    Agentite_LoadRequest request);

/* ============================================================================
 * Progress Tracking
 * ============================================================================ */

/**
 * Get the number of pending load requests.
 * Thread-safe.
 *
 * @param loader Async loader
 * @return Number of pending (queued + in-progress) loads
 */
size_t agentite_async_pending_count(const Agentite_AsyncLoader *loader);

/**
 * Get the number of completed loads waiting for callback.
 * Thread-safe.
 *
 * @param loader Async loader
 * @return Number of completed loads not yet processed by update()
 */
size_t agentite_async_completed_count(const Agentite_AsyncLoader *loader);

/**
 * Check if all loads are complete.
 * Thread-safe.
 *
 * @param loader Async loader
 * @return true if no pending or completed loads
 */
bool agentite_async_is_idle(const Agentite_AsyncLoader *loader);

/**
 * Wait for all pending loads to complete.
 * Blocks the calling thread. Callbacks are NOT invoked during this wait.
 * Call update() after this to process callbacks.
 *
 * WARNING: Can block indefinitely if loads are slow or stuck.
 *
 * @param loader      Async loader
 * @param timeout_ms  Maximum wait time in milliseconds (0 = wait forever)
 * @return true if all loads completed, false on timeout
 */
bool agentite_async_wait_all(
    Agentite_AsyncLoader *loader,
    uint32_t timeout_ms);

/* ============================================================================
 * Asset Streaming (Region-Based Loading)
 * ============================================================================ */

/**
 * Streaming region handle.
 * Used to manage assets that should be loaded/unloaded together.
 */
typedef struct Agentite_StreamRegion {
    uint32_t value;
} Agentite_StreamRegion;

/** Invalid region constant */
#define AGENTITE_INVALID_STREAM_REGION ((Agentite_StreamRegion){ 0 })

/**
 * Create a streaming region.
 * Regions group assets that should be loaded/unloaded together (e.g., level chunks).
 *
 * @param loader Async loader
 * @param name   Region name for debugging (can be NULL)
 * @return Region handle
 */
Agentite_StreamRegion agentite_stream_region_create(
    Agentite_AsyncLoader *loader,
    const char *name);

/**
 * Add an asset path to a streaming region.
 * The asset will be loaded when the region is activated and unloaded when deactivated.
 *
 * @param loader   Async loader
 * @param region   Region handle
 * @param path     Asset path
 * @param type     Asset type hint (AGENTITE_ASSET_UNKNOWN to auto-detect)
 */
void agentite_stream_region_add_asset(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region,
    const char *path,
    int type);

/**
 * Activate a streaming region (start loading its assets).
 *
 * @param loader   Async loader
 * @param region   Region handle
 * @param callback Called when all region assets are loaded
 * @param userdata User context
 */
void agentite_stream_region_activate(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region,
    void (*callback)(Agentite_StreamRegion region, void *userdata),
    void *userdata);

/**
 * Deactivate a streaming region (unload its assets).
 * Assets are unloaded when their reference count reaches zero.
 *
 * @param loader Async loader
 * @param region Region handle
 */
void agentite_stream_region_deactivate(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region);

/**
 * Destroy a streaming region.
 *
 * @param loader Async loader
 * @param region Region handle
 */
void agentite_stream_region_destroy(
    Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region);

/**
 * Get loading progress for a region.
 *
 * @param loader Async loader
 * @param region Region handle
 * @return Progress from 0.0 (nothing loaded) to 1.0 (fully loaded)
 */
float agentite_stream_region_progress(
    const Agentite_AsyncLoader *loader,
    Agentite_StreamRegion region);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_ASYNC_H */
