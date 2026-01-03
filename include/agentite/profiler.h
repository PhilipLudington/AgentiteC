/**
 * @file profiler.h
 * @brief Performance Profiling System
 *
 * Provides frame time tracking, scope-based profiling, draw call statistics,
 * and memory allocation monitoring for performance analysis.
 *
 * Features:
 * - Frame time tracking (update, render, present phases)
 * - Scope-based profiling with AGENTITE_PROFILE_SCOPE macro
 * - Draw call, batch, and vertex count tracking
 * - Entity count monitoring
 * - Memory allocation tracking
 * - Rolling frame time history for graphs
 * - CSV/JSON export for external analysis
 *
 * Usage:
 *   // Create profiler
 *   Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
 *   Agentite_Profiler *profiler = agentite_profiler_create(&config);
 *
 *   // Frame tracking
 *   agentite_profiler_begin_frame(profiler);
 *
 *   agentite_profiler_begin_scope(profiler, "update");
 *   // ... game update ...
 *   agentite_profiler_end_scope(profiler);
 *
 *   agentite_profiler_begin_scope(profiler, "render");
 *   // ... render ...
 *   agentite_profiler_end_scope(profiler);
 *
 *   agentite_profiler_end_frame(profiler);
 *
 *   // Get statistics
 *   const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
 *   printf("Frame time: %.2f ms\n", stats->frame_time_ms);
 *
 *   // Cleanup
 *   agentite_profiler_destroy(profiler);
 *
 * Scope-based profiling (C++ only):
 *   void MyFunction() {
 *       AGENTITE_PROFILE_SCOPE(profiler, "MyFunction");
 *       // ... function body automatically profiled ...
 *   }
 */

#ifndef AGENTITE_PROFILER_H
#define AGENTITE_PROFILER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

/** Maximum name length for profile scopes */
#define AGENTITE_PROFILER_MAX_SCOPE_NAME 64

/** Maximum number of concurrent nested scopes */
#define AGENTITE_PROFILER_MAX_SCOPE_DEPTH 32

/** Default frame history size for rolling average */
#define AGENTITE_PROFILER_DEFAULT_HISTORY_SIZE 128

/** Maximum named scopes that can be tracked */
#define AGENTITE_PROFILER_MAX_NAMED_SCOPES 64

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_Profiler Agentite_Profiler;

/* ============================================================================
 * Data Structures
 * ============================================================================ */

/** Timing data for a single profile scope */
typedef struct Agentite_ScopeStats {
    char name[AGENTITE_PROFILER_MAX_SCOPE_NAME];  /**< Scope name */
    double total_time_ms;    /**< Total time in this scope (current frame) */
    double avg_time_ms;      /**< Rolling average time */
    double min_time_ms;      /**< Minimum time recorded */
    double max_time_ms;      /**< Maximum time recorded */
    uint32_t call_count;     /**< Number of times entered this frame */
} Agentite_ScopeStats;

/** Memory allocation statistics */
typedef struct Agentite_MemoryStats {
    size_t current_bytes;    /**< Currently allocated bytes (tracked) */
    size_t peak_bytes;       /**< Peak allocation */
    size_t total_allocations;/**< Total number of allocations */
    size_t total_frees;      /**< Total number of frees */
    size_t allocation_count; /**< Current number of live allocations */
} Agentite_MemoryStats;

/** Rendering statistics (must be reported by renderer) */
typedef struct Agentite_RenderStats {
    uint32_t draw_calls;     /**< Number of draw calls this frame */
    uint32_t batch_count;    /**< Number of batches */
    uint32_t vertex_count;   /**< Number of vertices submitted */
    uint32_t index_count;    /**< Number of indices submitted */
    uint32_t texture_binds;  /**< Number of texture bind changes */
    uint32_t shader_binds;   /**< Number of shader bind changes */
} Agentite_RenderStats;

/** Complete profiler statistics snapshot */
typedef struct Agentite_ProfilerStats {
    /* Frame timing */
    double frame_time_ms;    /**< Total frame time in milliseconds */
    double fps;              /**< Frames per second (1000 / frame_time_ms) */
    double avg_frame_time_ms;/**< Rolling average frame time */
    double min_frame_time_ms;/**< Minimum frame time in history */
    double max_frame_time_ms;/**< Maximum frame time in history */

    /* Phase timing (if using built-in phase tracking) */
    double update_time_ms;   /**< Time spent in update phase */
    double render_time_ms;   /**< Time spent in render phase */
    double present_time_ms;  /**< Time spent in present/vsync */

    /* Counters */
    uint64_t frame_count;    /**< Total frames since profiler creation */
    uint32_t entity_count;   /**< Entity count (must be reported) */

    /* Render stats */
    Agentite_RenderStats render; /**< Rendering statistics */

    /* Memory stats */
    Agentite_MemoryStats memory; /**< Memory statistics */

    /* Named scopes */
    Agentite_ScopeStats scopes[AGENTITE_PROFILER_MAX_NAMED_SCOPES];
    uint32_t scope_count;    /**< Number of active named scopes */
} Agentite_ProfilerStats;

/* ============================================================================
 * Configuration
 * ============================================================================ */

/** Profiler configuration */
typedef struct Agentite_ProfilerConfig {
    uint32_t history_size;   /**< Frame history size (default: 128) */
    bool track_memory;       /**< Enable memory allocation tracking */
    bool track_scopes;       /**< Enable scope-based profiling */
    bool enabled;            /**< Master enable switch */
} Agentite_ProfilerConfig;

/** Default profiler configuration */
#define AGENTITE_PROFILER_DEFAULT { \
    .history_size = AGENTITE_PROFILER_DEFAULT_HISTORY_SIZE, \
    .track_memory = false, \
    .track_scopes = true, \
    .enabled = true \
}

/* ============================================================================
 * Profiler Lifecycle
 * ============================================================================ */

/**
 * Create a profiler instance.
 * Caller OWNS the returned pointer and MUST call agentite_profiler_destroy().
 *
 * @param config Configuration (NULL for defaults)
 * @return Profiler instance, or NULL on failure
 *
 * Thread Safety: NOT thread-safe
 */
Agentite_Profiler *agentite_profiler_create(const Agentite_ProfilerConfig *config);

/**
 * Destroy profiler instance.
 * Safe to call with NULL.
 *
 * @param profiler Profiler to destroy
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_destroy(Agentite_Profiler *profiler);

/**
 * Enable or disable the profiler.
 * When disabled, all profiling functions become no-ops.
 *
 * @param profiler Profiler instance
 * @param enabled true to enable, false to disable
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_set_enabled(Agentite_Profiler *profiler, bool enabled);

/**
 * Check if profiler is enabled.
 *
 * @param profiler Profiler instance
 * @return true if enabled
 *
 * Thread Safety: Thread-safe (read-only)
 */
bool agentite_profiler_is_enabled(const Agentite_Profiler *profiler);

/**
 * Reset all profiler statistics.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_reset(Agentite_Profiler *profiler);

/* ============================================================================
 * Frame Timing
 * ============================================================================ */

/**
 * Begin a new frame.
 * Call at the start of each frame, before any profiled work.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_begin_frame(Agentite_Profiler *profiler);

/**
 * End the current frame.
 * Call at the end of each frame, after all profiled work.
 * This calculates frame time and updates rolling averages.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_end_frame(Agentite_Profiler *profiler);

/* ============================================================================
 * Phase Timing
 * ============================================================================ */

/**
 * Mark the start of the update phase.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_begin_update(Agentite_Profiler *profiler);

/**
 * Mark the end of the update phase.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_end_update(Agentite_Profiler *profiler);

/**
 * Mark the start of the render phase.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_begin_render(Agentite_Profiler *profiler);

/**
 * Mark the end of the render phase.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_end_render(Agentite_Profiler *profiler);

/**
 * Mark the start of the present phase.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_begin_present(Agentite_Profiler *profiler);

/**
 * Mark the end of the present phase.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_end_present(Agentite_Profiler *profiler);

/* ============================================================================
 * Scope-Based Profiling
 * ============================================================================ */

/**
 * Begin a named profiling scope.
 * Scopes can be nested. Each scope tracks its own timing statistics.
 *
 * @param profiler Profiler instance
 * @param name Scope name (max AGENTITE_PROFILER_MAX_SCOPE_NAME chars)
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_begin_scope(Agentite_Profiler *profiler, const char *name);

/**
 * End the current profiling scope.
 * Must be paired with a corresponding begin_scope call.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_end_scope(Agentite_Profiler *profiler);

/**
 * Get statistics for a named scope.
 *
 * @param profiler Profiler instance
 * @param name Scope name
 * @return Scope statistics, or NULL if scope not found
 *
 * Thread Safety: NOT thread-safe
 */
const Agentite_ScopeStats *agentite_profiler_get_scope(
    const Agentite_Profiler *profiler, const char *name);

/* ============================================================================
 * Statistics Reporting (call these to update counters)
 * ============================================================================ */

/**
 * Report a draw call.
 * Call this from your renderer each time you issue a draw call.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_draw_call(Agentite_Profiler *profiler);

/**
 * Report a batch.
 *
 * @param profiler Profiler instance
 * @param vertex_count Number of vertices in the batch
 * @param index_count Number of indices in the batch
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_batch(
    Agentite_Profiler *profiler, uint32_t vertex_count, uint32_t index_count);

/**
 * Report a texture bind.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_texture_bind(Agentite_Profiler *profiler);

/**
 * Report a shader bind.
 *
 * @param profiler Profiler instance
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_shader_bind(Agentite_Profiler *profiler);

/**
 * Report entity count.
 * Call this once per frame with your current entity count.
 *
 * @param profiler Profiler instance
 * @param count Current entity count
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_entity_count(Agentite_Profiler *profiler, uint32_t count);

/**
 * Report render statistics directly.
 * Alternative to calling individual report functions.
 *
 * @param profiler Profiler instance
 * @param stats Render statistics to merge
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_render_stats(
    Agentite_Profiler *profiler, const Agentite_RenderStats *stats);

/* ============================================================================
 * Memory Tracking
 * ============================================================================ */

/**
 * Report a memory allocation.
 * Call this when allocating memory you want tracked.
 *
 * @param profiler Profiler instance
 * @param bytes Number of bytes allocated
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_alloc(Agentite_Profiler *profiler, size_t bytes);

/**
 * Report a memory free.
 * Call this when freeing tracked memory.
 *
 * @param profiler Profiler instance
 * @param bytes Number of bytes freed
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_report_free(Agentite_Profiler *profiler, size_t bytes);

/**
 * Get current memory statistics.
 *
 * @param profiler Profiler instance
 * @param out_stats Output memory statistics
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_get_memory_stats(
    const Agentite_Profiler *profiler, Agentite_MemoryStats *out_stats);

/* ============================================================================
 * Statistics Access
 * ============================================================================ */

/**
 * Get complete profiler statistics snapshot.
 * The returned pointer is valid until the next call to begin_frame or destroy.
 *
 * @param profiler Profiler instance
 * @return Statistics snapshot (do not free)
 *
 * Thread Safety: NOT thread-safe
 */
const Agentite_ProfilerStats *agentite_profiler_get_stats(
    const Agentite_Profiler *profiler);

/**
 * Get frame time history for graphing.
 * Returns a ring buffer of recent frame times.
 *
 * @param profiler Profiler instance
 * @param out_times Output array (caller must allocate history_size floats)
 * @param out_count Output: number of valid entries
 * @param out_index Output: index of most recent entry
 * @return true on success
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_profiler_get_frame_history(
    const Agentite_Profiler *profiler,
    float *out_times, uint32_t *out_count, uint32_t *out_index);

/**
 * Get the configured history size.
 *
 * @param profiler Profiler instance
 * @return History buffer size
 *
 * Thread Safety: Thread-safe (read-only)
 */
uint32_t agentite_profiler_get_history_size(const Agentite_Profiler *profiler);

/* ============================================================================
 * Export Functions
 * ============================================================================ */

/**
 * Export current statistics to CSV format.
 * Writes a summary of current statistics.
 *
 * @param profiler Profiler instance
 * @param path File path to write (will be overwritten)
 * @return true on success
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_profiler_export_csv(
    const Agentite_Profiler *profiler, const char *path);

/**
 * Export current statistics to JSON format.
 *
 * @param profiler Profiler instance
 * @param path File path to write (will be overwritten)
 * @return true on success
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_profiler_export_json(
    const Agentite_Profiler *profiler, const char *path);

/**
 * Export frame history to CSV (for graphing in external tools).
 *
 * @param profiler Profiler instance
 * @param path File path to write
 * @return true on success
 *
 * Thread Safety: NOT thread-safe
 */
bool agentite_profiler_export_frame_history_csv(
    const Agentite_Profiler *profiler, const char *path);

/* ============================================================================
 * UI Integration (requires UI system)
 * ============================================================================ */

/* Forward declarations for UI types */
struct Agentite_UI;

/**
 * Draw profiler overlay widget.
 * Displays frame time, FPS, and key statistics.
 *
 * @param profiler Profiler instance
 * @param ui UI context
 * @param x X position
 * @param y Y position
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_draw_overlay(
    const Agentite_Profiler *profiler,
    struct Agentite_UI *ui,
    float x, float y);

/**
 * Draw frame time graph widget.
 * Shows rolling frame time history as a line graph.
 *
 * @param profiler Profiler instance
 * @param ui UI context
 * @param x X position
 * @param y Y position
 * @param width Graph width
 * @param height Graph height
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_draw_graph(
    const Agentite_Profiler *profiler,
    struct Agentite_UI *ui,
    float x, float y, float width, float height);

/**
 * Draw detailed profiler panel.
 * Shows all statistics including scopes, memory, and render stats.
 *
 * @param profiler Profiler instance
 * @param ui UI context
 * @param x X position
 * @param y Y position
 * @param width Panel width
 * @param height Panel height
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_profiler_draw_panel(
    const Agentite_Profiler *profiler,
    struct Agentite_UI *ui,
    float x, float y, float width, float height);

#ifdef __cplusplus
}

/* ============================================================================
 * C++ RAII Scope Helper
 * ============================================================================ */

/**
 * RAII helper for automatic scope profiling.
 * Usage: AGENTITE_PROFILE_SCOPE(profiler, "MyFunction");
 */
class Agentite_ProfileScope {
public:
    Agentite_ProfileScope(Agentite_Profiler *profiler, const char *name)
        : m_profiler(profiler) {
        if (m_profiler) {
            agentite_profiler_begin_scope(m_profiler, name);
        }
    }

    ~Agentite_ProfileScope() {
        if (m_profiler) {
            agentite_profiler_end_scope(m_profiler);
        }
    }

    Agentite_ProfileScope(const Agentite_ProfileScope&) = delete;
    Agentite_ProfileScope& operator=(const Agentite_ProfileScope&) = delete;

private:
    Agentite_Profiler *m_profiler;
};

/** Macro for automatic scope profiling (C++ only) */
#define AGENTITE_PROFILE_SCOPE(profiler, name) \
    Agentite_ProfileScope _agentite_profile_scope_##__LINE__((profiler), (name))

/** Macro for profiling a function (uses function name as scope name) */
#define AGENTITE_PROFILE_FUNCTION(profiler) \
    AGENTITE_PROFILE_SCOPE((profiler), __func__)

#endif /* __cplusplus */

#endif /* AGENTITE_PROFILER_H */
