/**
 * @file profiler.cpp
 * @brief Performance Profiling System Implementation
 */

#include "agentite/profiler.h"
#include "agentite/error.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <cstdio>
#include <cmath>
#include <algorithm>

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/** Active scope entry in the scope stack */
struct ScopeEntry {
    char name[AGENTITE_PROFILER_MAX_SCOPE_NAME];
    uint64_t start_time;
};

/** Named scope tracking data */
struct NamedScope {
    char name[AGENTITE_PROFILER_MAX_SCOPE_NAME];
    double total_time_ms;       /* Total time this frame */
    double accumulated_ms;      /* Accumulated for rolling average */
    double min_time_ms;
    double max_time_ms;
    uint32_t call_count;        /* Calls this frame */
    uint32_t sample_count;      /* Samples for average */
    bool active;
};

/** Profiler internal state */
struct Agentite_Profiler {
    /* Configuration */
    Agentite_ProfilerConfig config;

    /* Frame timing */
    uint64_t frame_start_time;
    uint64_t frame_count;
    double last_frame_time_ms;

    /* Phase timing */
    uint64_t update_start;
    uint64_t render_start;
    uint64_t present_start;
    double update_time_ms;
    double render_time_ms;
    double present_time_ms;

    /* Frame history (ring buffer) */
    float *frame_history;
    uint32_t history_index;
    uint32_t history_count;

    /* Rolling statistics */
    double avg_frame_time_ms;
    double min_frame_time_ms;
    double max_frame_time_ms;

    /* Scope stack */
    ScopeEntry scope_stack[AGENTITE_PROFILER_MAX_SCOPE_DEPTH];
    uint32_t scope_depth;

    /* Named scopes */
    NamedScope named_scopes[AGENTITE_PROFILER_MAX_NAMED_SCOPES];
    uint32_t named_scope_count;

    /* Render statistics (reset each frame) */
    Agentite_RenderStats render_stats;

    /* Memory statistics (cumulative) */
    Agentite_MemoryStats memory_stats;

    /* Entity count */
    uint32_t entity_count;

    /* Cached stats snapshot */
    Agentite_ProfilerStats cached_stats;

    /* Performance counter frequency */
    uint64_t perf_freq;
};

/* ============================================================================
 * Helper Functions
 * ============================================================================ */

static double ticks_to_ms(uint64_t ticks, uint64_t freq) {
    return (double)ticks * 1000.0 / (double)freq;
}

static NamedScope *find_named_scope(Agentite_Profiler *profiler, const char *name) {
    for (uint32_t i = 0; i < profiler->named_scope_count; i++) {
        if (profiler->named_scopes[i].active &&
            strcmp(profiler->named_scopes[i].name, name) == 0) {
            return &profiler->named_scopes[i];
        }
    }
    return nullptr;
}

static NamedScope *get_or_create_named_scope(Agentite_Profiler *profiler, const char *name) {
    /* Find existing */
    NamedScope *scope = find_named_scope(profiler, name);
    if (scope) {
        return scope;
    }

    /* Create new if space available */
    if (profiler->named_scope_count >= AGENTITE_PROFILER_MAX_NAMED_SCOPES) {
        return nullptr;
    }

    scope = &profiler->named_scopes[profiler->named_scope_count++];
    memset(scope, 0, sizeof(*scope));
    strncpy(scope->name, name, AGENTITE_PROFILER_MAX_SCOPE_NAME - 1);
    scope->name[AGENTITE_PROFILER_MAX_SCOPE_NAME - 1] = '\0';
    scope->active = true;
    scope->min_time_ms = 1e9;  /* Start high for min tracking */
    scope->max_time_ms = 0.0;

    return scope;
}

/* ============================================================================
 * Profiler Lifecycle
 * ============================================================================ */

Agentite_Profiler *agentite_profiler_create(const Agentite_ProfilerConfig *config) {
    Agentite_ProfilerConfig default_config = AGENTITE_PROFILER_DEFAULT;
    if (!config) {
        config = &default_config;
    }

    Agentite_Profiler *profiler = (Agentite_Profiler *)calloc(1, sizeof(Agentite_Profiler));
    if (!profiler) {
        agentite_set_error("Failed to allocate profiler");
        return nullptr;
    }

    profiler->config = *config;
    profiler->perf_freq = SDL_GetPerformanceFrequency();

    /* Allocate frame history buffer */
    profiler->frame_history = (float *)calloc(config->history_size, sizeof(float));
    if (!profiler->frame_history) {
        agentite_set_error("Failed to allocate frame history buffer");
        free(profiler);
        return nullptr;
    }

    /* Initialize rolling stats */
    profiler->min_frame_time_ms = 1e9;
    profiler->max_frame_time_ms = 0.0;

    SDL_Log("Profiler created (history_size=%u, scopes=%s, memory=%s)",
            config->history_size,
            config->track_scopes ? "enabled" : "disabled",
            config->track_memory ? "enabled" : "disabled");

    return profiler;
}

void agentite_profiler_destroy(Agentite_Profiler *profiler) {
    if (!profiler) return;

    free(profiler->frame_history);
    free(profiler);
}

void agentite_profiler_set_enabled(Agentite_Profiler *profiler, bool enabled) {
    if (profiler) {
        profiler->config.enabled = enabled;
    }
}

bool agentite_profiler_is_enabled(const Agentite_Profiler *profiler) {
    return profiler && profiler->config.enabled;
}

void agentite_profiler_reset(Agentite_Profiler *profiler) {
    if (!profiler) return;

    profiler->frame_count = 0;
    profiler->history_index = 0;
    profiler->history_count = 0;
    profiler->avg_frame_time_ms = 0.0;
    profiler->min_frame_time_ms = 1e9;
    profiler->max_frame_time_ms = 0.0;
    profiler->scope_depth = 0;
    profiler->named_scope_count = 0;

    memset(profiler->frame_history, 0, profiler->config.history_size * sizeof(float));
    memset(&profiler->render_stats, 0, sizeof(profiler->render_stats));
    memset(&profiler->memory_stats, 0, sizeof(profiler->memory_stats));
}

/* ============================================================================
 * Frame Timing
 * ============================================================================ */

void agentite_profiler_begin_frame(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;

    profiler->frame_start_time = SDL_GetPerformanceCounter();

    /* Reset per-frame counters */
    memset(&profiler->render_stats, 0, sizeof(profiler->render_stats));
    profiler->update_time_ms = 0.0;
    profiler->render_time_ms = 0.0;
    profiler->present_time_ms = 0.0;

    /* Reset per-frame scope data */
    for (uint32_t i = 0; i < profiler->named_scope_count; i++) {
        profiler->named_scopes[i].total_time_ms = 0.0;
        profiler->named_scopes[i].call_count = 0;
    }
}

void agentite_profiler_end_frame(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;

    uint64_t end_time = SDL_GetPerformanceCounter();
    uint64_t elapsed = end_time - profiler->frame_start_time;
    double frame_time_ms = ticks_to_ms(elapsed, profiler->perf_freq);

    profiler->last_frame_time_ms = frame_time_ms;
    profiler->frame_count++;

    /* Add to history ring buffer */
    profiler->frame_history[profiler->history_index] = (float)frame_time_ms;
    profiler->history_index = (profiler->history_index + 1) % profiler->config.history_size;
    if (profiler->history_count < profiler->config.history_size) {
        profiler->history_count++;
    }

    /* Calculate rolling average from history */
    double sum = 0.0;
    double min_val = 1e9;
    double max_val = 0.0;
    for (uint32_t i = 0; i < profiler->history_count; i++) {
        double t = profiler->frame_history[i];
        sum += t;
        if (t < min_val) min_val = t;
        if (t > max_val) max_val = t;
    }
    profiler->avg_frame_time_ms = sum / profiler->history_count;
    profiler->min_frame_time_ms = min_val;
    profiler->max_frame_time_ms = max_val;

    /* Update named scope averages */
    for (uint32_t i = 0; i < profiler->named_scope_count; i++) {
        NamedScope *scope = &profiler->named_scopes[i];
        if (scope->call_count > 0) {
            scope->accumulated_ms += scope->total_time_ms;
            scope->sample_count++;
            if (scope->total_time_ms < scope->min_time_ms) {
                scope->min_time_ms = scope->total_time_ms;
            }
            if (scope->total_time_ms > scope->max_time_ms) {
                scope->max_time_ms = scope->total_time_ms;
            }
        }
    }
}

/* ============================================================================
 * Phase Timing
 * ============================================================================ */

void agentite_profiler_begin_update(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->update_start = SDL_GetPerformanceCounter();
}

void agentite_profiler_end_update(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    uint64_t elapsed = SDL_GetPerformanceCounter() - profiler->update_start;
    profiler->update_time_ms = ticks_to_ms(elapsed, profiler->perf_freq);
}

void agentite_profiler_begin_render(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->render_start = SDL_GetPerformanceCounter();
}

void agentite_profiler_end_render(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    uint64_t elapsed = SDL_GetPerformanceCounter() - profiler->render_start;
    profiler->render_time_ms = ticks_to_ms(elapsed, profiler->perf_freq);
}

void agentite_profiler_begin_present(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->present_start = SDL_GetPerformanceCounter();
}

void agentite_profiler_end_present(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    uint64_t elapsed = SDL_GetPerformanceCounter() - profiler->present_start;
    profiler->present_time_ms = ticks_to_ms(elapsed, profiler->perf_freq);
}

/* ============================================================================
 * Scope-Based Profiling
 * ============================================================================ */

void agentite_profiler_begin_scope(Agentite_Profiler *profiler, const char *name) {
    if (!profiler || !profiler->config.enabled || !profiler->config.track_scopes) return;
    if (!name) return;

    if (profiler->scope_depth >= AGENTITE_PROFILER_MAX_SCOPE_DEPTH) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Profiler scope depth exceeded (max %d)",
                    AGENTITE_PROFILER_MAX_SCOPE_DEPTH);
        return;
    }

    ScopeEntry *entry = &profiler->scope_stack[profiler->scope_depth++];
    strncpy(entry->name, name, AGENTITE_PROFILER_MAX_SCOPE_NAME - 1);
    entry->name[AGENTITE_PROFILER_MAX_SCOPE_NAME - 1] = '\0';
    entry->start_time = SDL_GetPerformanceCounter();
}

void agentite_profiler_end_scope(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled || !profiler->config.track_scopes) return;

    if (profiler->scope_depth == 0) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                    "Profiler end_scope called without matching begin_scope");
        return;
    }

    profiler->scope_depth--;
    ScopeEntry *entry = &profiler->scope_stack[profiler->scope_depth];

    uint64_t elapsed = SDL_GetPerformanceCounter() - entry->start_time;
    double elapsed_ms = ticks_to_ms(elapsed, profiler->perf_freq);

    /* Update named scope stats */
    NamedScope *scope = get_or_create_named_scope(profiler, entry->name);
    if (scope) {
        scope->total_time_ms += elapsed_ms;
        scope->call_count++;
    }
}

const Agentite_ScopeStats *agentite_profiler_get_scope(
    const Agentite_Profiler *profiler, const char *name) {
    if (!profiler || !name) return nullptr;

    /* Find the scope and fill cached stats - non-const cast for internal lookup */
    Agentite_Profiler *p = const_cast<Agentite_Profiler *>(profiler);
    NamedScope *scope = find_named_scope(p, name);
    if (!scope) return nullptr;

    /* Find or use first available slot in cached_stats.scopes */
    /* For simplicity, just scan the cached stats */
    for (uint32_t i = 0; i < profiler->cached_stats.scope_count; i++) {
        if (strcmp(profiler->cached_stats.scopes[i].name, name) == 0) {
            return &profiler->cached_stats.scopes[i];
        }
    }

    return nullptr;
}

/* ============================================================================
 * Statistics Reporting
 * ============================================================================ */

void agentite_profiler_report_draw_call(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->render_stats.draw_calls++;
}

void agentite_profiler_report_batch(
    Agentite_Profiler *profiler, uint32_t vertex_count, uint32_t index_count) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->render_stats.batch_count++;
    profiler->render_stats.vertex_count += vertex_count;
    profiler->render_stats.index_count += index_count;
}

void agentite_profiler_report_texture_bind(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->render_stats.texture_binds++;
}

void agentite_profiler_report_shader_bind(Agentite_Profiler *profiler) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->render_stats.shader_binds++;
}

void agentite_profiler_report_entity_count(Agentite_Profiler *profiler, uint32_t count) {
    if (!profiler || !profiler->config.enabled) return;
    profiler->entity_count = count;
}

void agentite_profiler_report_render_stats(
    Agentite_Profiler *profiler, const Agentite_RenderStats *stats) {
    if (!profiler || !profiler->config.enabled || !stats) return;
    profiler->render_stats.draw_calls += stats->draw_calls;
    profiler->render_stats.batch_count += stats->batch_count;
    profiler->render_stats.vertex_count += stats->vertex_count;
    profiler->render_stats.index_count += stats->index_count;
    profiler->render_stats.texture_binds += stats->texture_binds;
    profiler->render_stats.shader_binds += stats->shader_binds;
}

/* ============================================================================
 * Memory Tracking
 * ============================================================================ */

void agentite_profiler_report_alloc(Agentite_Profiler *profiler, size_t bytes) {
    if (!profiler || !profiler->config.enabled || !profiler->config.track_memory) return;

    profiler->memory_stats.current_bytes += bytes;
    profiler->memory_stats.total_allocations++;
    profiler->memory_stats.allocation_count++;

    if (profiler->memory_stats.current_bytes > profiler->memory_stats.peak_bytes) {
        profiler->memory_stats.peak_bytes = profiler->memory_stats.current_bytes;
    }
}

void agentite_profiler_report_free(Agentite_Profiler *profiler, size_t bytes) {
    if (!profiler || !profiler->config.enabled || !profiler->config.track_memory) return;

    if (bytes <= profiler->memory_stats.current_bytes) {
        profiler->memory_stats.current_bytes -= bytes;
    } else {
        profiler->memory_stats.current_bytes = 0;
    }
    profiler->memory_stats.total_frees++;
    if (profiler->memory_stats.allocation_count > 0) {
        profiler->memory_stats.allocation_count--;
    }
}

void agentite_profiler_get_memory_stats(
    const Agentite_Profiler *profiler, Agentite_MemoryStats *out_stats) {
    if (!profiler || !out_stats) return;
    *out_stats = profiler->memory_stats;
}

/* ============================================================================
 * Statistics Access
 * ============================================================================ */

const Agentite_ProfilerStats *agentite_profiler_get_stats(
    const Agentite_Profiler *profiler) {
    if (!profiler) return nullptr;

    /* Update cached stats (const_cast for internal caching) */
    Agentite_Profiler *p = const_cast<Agentite_Profiler *>(profiler);
    Agentite_ProfilerStats *stats = &p->cached_stats;

    /* Frame timing */
    stats->frame_time_ms = profiler->last_frame_time_ms;
    stats->fps = (profiler->last_frame_time_ms > 0.0)
        ? 1000.0 / profiler->last_frame_time_ms
        : 0.0;
    stats->avg_frame_time_ms = profiler->avg_frame_time_ms;
    stats->min_frame_time_ms = profiler->min_frame_time_ms;
    stats->max_frame_time_ms = profiler->max_frame_time_ms;

    /* Phase timing */
    stats->update_time_ms = profiler->update_time_ms;
    stats->render_time_ms = profiler->render_time_ms;
    stats->present_time_ms = profiler->present_time_ms;

    /* Counters */
    stats->frame_count = profiler->frame_count;
    stats->entity_count = profiler->entity_count;

    /* Render stats */
    stats->render = profiler->render_stats;

    /* Memory stats */
    stats->memory = profiler->memory_stats;

    /* Named scopes */
    stats->scope_count = 0;
    for (uint32_t i = 0; i < profiler->named_scope_count && i < AGENTITE_PROFILER_MAX_NAMED_SCOPES; i++) {
        const NamedScope *src = &profiler->named_scopes[i];
        Agentite_ScopeStats *dst = &stats->scopes[stats->scope_count];

        strncpy(dst->name, src->name, AGENTITE_PROFILER_MAX_SCOPE_NAME - 1);
        dst->name[AGENTITE_PROFILER_MAX_SCOPE_NAME - 1] = '\0';
        dst->total_time_ms = src->total_time_ms;
        dst->avg_time_ms = (src->sample_count > 0)
            ? src->accumulated_ms / src->sample_count
            : 0.0;
        dst->min_time_ms = src->min_time_ms;
        dst->max_time_ms = src->max_time_ms;
        dst->call_count = src->call_count;

        stats->scope_count++;
    }

    return stats;
}

bool agentite_profiler_get_frame_history(
    const Agentite_Profiler *profiler,
    float *out_times, uint32_t *out_count, uint32_t *out_index) {
    if (!profiler || !out_times || !out_count || !out_index) return false;

    memcpy(out_times, profiler->frame_history,
           profiler->config.history_size * sizeof(float));
    *out_count = profiler->history_count;
    *out_index = (profiler->history_index > 0)
        ? profiler->history_index - 1
        : (profiler->history_count > 0 ? profiler->config.history_size - 1 : 0);

    return true;
}

uint32_t agentite_profiler_get_history_size(const Agentite_Profiler *profiler) {
    return profiler ? profiler->config.history_size : 0;
}

/* ============================================================================
 * Export Functions
 * ============================================================================ */

bool agentite_profiler_export_csv(
    const Agentite_Profiler *profiler, const char *path) {
    if (!profiler || !path) {
        agentite_set_error("Invalid profiler or path");
        return false;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        agentite_set_error("Failed to open file for writing: %s", path);
        return false;
    }

    const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);

    /* Header */
    fprintf(f, "metric,value\n");

    /* Frame timing */
    fprintf(f, "frame_time_ms,%.4f\n", stats->frame_time_ms);
    fprintf(f, "fps,%.2f\n", stats->fps);
    fprintf(f, "avg_frame_time_ms,%.4f\n", stats->avg_frame_time_ms);
    fprintf(f, "min_frame_time_ms,%.4f\n", stats->min_frame_time_ms);
    fprintf(f, "max_frame_time_ms,%.4f\n", stats->max_frame_time_ms);
    fprintf(f, "update_time_ms,%.4f\n", stats->update_time_ms);
    fprintf(f, "render_time_ms,%.4f\n", stats->render_time_ms);
    fprintf(f, "present_time_ms,%.4f\n", stats->present_time_ms);

    /* Counters */
    fprintf(f, "frame_count,%llu\n", (unsigned long long)stats->frame_count);
    fprintf(f, "entity_count,%u\n", stats->entity_count);

    /* Render stats */
    fprintf(f, "draw_calls,%u\n", stats->render.draw_calls);
    fprintf(f, "batch_count,%u\n", stats->render.batch_count);
    fprintf(f, "vertex_count,%u\n", stats->render.vertex_count);
    fprintf(f, "index_count,%u\n", stats->render.index_count);
    fprintf(f, "texture_binds,%u\n", stats->render.texture_binds);
    fprintf(f, "shader_binds,%u\n", stats->render.shader_binds);

    /* Memory stats */
    fprintf(f, "memory_current_bytes,%zu\n", stats->memory.current_bytes);
    fprintf(f, "memory_peak_bytes,%zu\n", stats->memory.peak_bytes);
    fprintf(f, "memory_total_allocations,%zu\n", stats->memory.total_allocations);
    fprintf(f, "memory_allocation_count,%zu\n", stats->memory.allocation_count);

    fclose(f);
    return true;
}

bool agentite_profiler_export_json(
    const Agentite_Profiler *profiler, const char *path) {
    if (!profiler || !path) {
        agentite_set_error("Invalid profiler or path");
        return false;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        agentite_set_error("Failed to open file for writing: %s", path);
        return false;
    }

    const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);

    fprintf(f, "{\n");
    fprintf(f, "  \"frame\": {\n");
    fprintf(f, "    \"time_ms\": %.4f,\n", stats->frame_time_ms);
    fprintf(f, "    \"fps\": %.2f,\n", stats->fps);
    fprintf(f, "    \"avg_time_ms\": %.4f,\n", stats->avg_frame_time_ms);
    fprintf(f, "    \"min_time_ms\": %.4f,\n", stats->min_frame_time_ms);
    fprintf(f, "    \"max_time_ms\": %.4f,\n", stats->max_frame_time_ms);
    fprintf(f, "    \"count\": %llu\n", (unsigned long long)stats->frame_count);
    fprintf(f, "  },\n");

    fprintf(f, "  \"phases\": {\n");
    fprintf(f, "    \"update_ms\": %.4f,\n", stats->update_time_ms);
    fprintf(f, "    \"render_ms\": %.4f,\n", stats->render_time_ms);
    fprintf(f, "    \"present_ms\": %.4f\n", stats->present_time_ms);
    fprintf(f, "  },\n");

    fprintf(f, "  \"render\": {\n");
    fprintf(f, "    \"draw_calls\": %u,\n", stats->render.draw_calls);
    fprintf(f, "    \"batch_count\": %u,\n", stats->render.batch_count);
    fprintf(f, "    \"vertex_count\": %u,\n", stats->render.vertex_count);
    fprintf(f, "    \"index_count\": %u,\n", stats->render.index_count);
    fprintf(f, "    \"texture_binds\": %u,\n", stats->render.texture_binds);
    fprintf(f, "    \"shader_binds\": %u\n", stats->render.shader_binds);
    fprintf(f, "  },\n");

    fprintf(f, "  \"memory\": {\n");
    fprintf(f, "    \"current_bytes\": %zu,\n", stats->memory.current_bytes);
    fprintf(f, "    \"peak_bytes\": %zu,\n", stats->memory.peak_bytes);
    fprintf(f, "    \"total_allocations\": %zu,\n", stats->memory.total_allocations);
    fprintf(f, "    \"allocation_count\": %zu\n", stats->memory.allocation_count);
    fprintf(f, "  },\n");

    fprintf(f, "  \"entity_count\": %u,\n", stats->entity_count);

    fprintf(f, "  \"scopes\": [\n");
    for (uint32_t i = 0; i < stats->scope_count; i++) {
        const Agentite_ScopeStats *scope = &stats->scopes[i];
        fprintf(f, "    {\n");
        fprintf(f, "      \"name\": \"%s\",\n", scope->name);
        fprintf(f, "      \"total_ms\": %.4f,\n", scope->total_time_ms);
        fprintf(f, "      \"avg_ms\": %.4f,\n", scope->avg_time_ms);
        fprintf(f, "      \"min_ms\": %.4f,\n", scope->min_time_ms);
        fprintf(f, "      \"max_ms\": %.4f,\n", scope->max_time_ms);
        fprintf(f, "      \"call_count\": %u\n", scope->call_count);
        fprintf(f, "    }%s\n", (i < stats->scope_count - 1) ? "," : "");
    }
    fprintf(f, "  ]\n");

    fprintf(f, "}\n");

    fclose(f);
    return true;
}

bool agentite_profiler_export_frame_history_csv(
    const Agentite_Profiler *profiler, const char *path) {
    if (!profiler || !path) {
        agentite_set_error("Invalid profiler or path");
        return false;
    }

    FILE *f = fopen(path, "w");
    if (!f) {
        agentite_set_error("Failed to open file for writing: %s", path);
        return false;
    }

    fprintf(f, "frame,time_ms\n");

    /* Output in chronological order */
    uint32_t start = (profiler->history_count < profiler->config.history_size)
        ? 0
        : profiler->history_index;

    for (uint32_t i = 0; i < profiler->history_count; i++) {
        uint32_t idx = (start + i) % profiler->config.history_size;
        fprintf(f, "%u,%.4f\n", i, profiler->frame_history[idx]);
    }

    fclose(f);
    return true;
}

/* ============================================================================
 * UI Integration
 * ============================================================================ */

void agentite_profiler_draw_overlay(
    const Agentite_Profiler *profiler,
    struct Agentite_UI *ui,
    float x, float y) {
    /* UI implementation will be added when integrating with the UI system */
    (void)profiler;
    (void)ui;
    (void)x;
    (void)y;
}

void agentite_profiler_draw_graph(
    const Agentite_Profiler *profiler,
    struct Agentite_UI *ui,
    float x, float y, float width, float height) {
    /* UI implementation will be added when integrating with the UI system */
    (void)profiler;
    (void)ui;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}

void agentite_profiler_draw_panel(
    const Agentite_Profiler *profiler,
    struct Agentite_UI *ui,
    float x, float y, float width, float height) {
    /* UI implementation will be added when integrating with the UI system */
    (void)profiler;
    (void)ui;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
}
