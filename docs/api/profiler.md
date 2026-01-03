# Performance Profiler

The Agentite Profiler provides comprehensive performance monitoring for debugging and optimization:
- Frame time tracking (update, render, present phases)
- Scope-based profiling with RAII support
- Draw call, batch, and vertex count statistics
- Memory allocation tracking
- Rolling frame history for graphs
- CSV/JSON export for external analysis

## Header

```c
#include "agentite/profiler.h"
```

## Quick Start

```c
// Create profiler with default config
Agentite_Profiler *profiler = agentite_profiler_create(NULL);

// In your game loop:
while (running) {
    agentite_profiler_begin_frame(profiler);

    // Track update phase
    agentite_profiler_begin_update(profiler);
    game_update(delta_time);
    agentite_profiler_end_update(profiler);

    // Track render phase
    agentite_profiler_begin_render(profiler);
    game_render();
    agentite_profiler_end_render(profiler);

    // Report entity count
    agentite_profiler_report_entity_count(profiler, entity_count);

    agentite_profiler_end_frame(profiler);

    // Access statistics
    const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
    printf("FPS: %.1f  Frame: %.2f ms\n", stats->fps, stats->frame_time_ms);
}

// Export data for analysis
agentite_profiler_export_json(profiler, "profile.json");

// Cleanup
agentite_profiler_destroy(profiler);
```

## Configuration

```c
Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
config.history_size = 256;       // Frame history for rolling average
config.track_scopes = true;      // Enable scope-based profiling
config.track_memory = true;      // Enable memory tracking
config.enabled = true;           // Master enable switch

Agentite_Profiler *profiler = agentite_profiler_create(&config);
```

## Scope-Based Profiling

### Manual Scopes

```c
void game_update(float dt) {
    agentite_profiler_begin_scope(profiler, "physics");
    physics_update(dt);
    agentite_profiler_end_scope(profiler);

    agentite_profiler_begin_scope(profiler, "ai");
    ai_update(dt);
    agentite_profiler_end_scope(profiler);

    agentite_profiler_begin_scope(profiler, "animation");
    animation_update(dt);
    agentite_profiler_end_scope(profiler);
}
```

### RAII Scopes (C++)

```cpp
void MySystem::update(float dt) {
    AGENTITE_PROFILE_SCOPE(profiler, "MySystem::update");
    // Scope automatically ends when function returns

    {
        AGENTITE_PROFILE_SCOPE(profiler, "expensive_calculation");
        // Nested scopes work too
    }
}

void game_update(float dt) {
    AGENTITE_PROFILE_FUNCTION(profiler);  // Uses __func__ as scope name
    // ...
}
```

## Render Statistics Reporting

Call these functions from your renderer to track GPU activity:

```c
// In your sprite renderer
void sprite_renderer_draw_batch(SpriteRenderer *sr) {
    agentite_profiler_report_batch(profiler, vertex_count, index_count);
    agentite_profiler_report_draw_call(profiler);
    // ... actual draw call ...
}

// On texture or shader change
agentite_profiler_report_texture_bind(profiler);
agentite_profiler_report_shader_bind(profiler);

// Or report all at once
Agentite_RenderStats stats = {
    .draw_calls = 10,
    .batch_count = 5,
    .vertex_count = 1000,
    .index_count = 1500,
    .texture_binds = 3,
    .shader_binds = 2
};
agentite_profiler_report_render_stats(profiler, &stats);
```

## Memory Tracking

```c
// Track allocations (call from your allocator wrapper)
void *my_alloc(size_t size) {
    void *ptr = malloc(size);
    if (ptr) {
        agentite_profiler_report_alloc(profiler, size);
    }
    return ptr;
}

void my_free(void *ptr, size_t size) {
    free(ptr);
    agentite_profiler_report_free(profiler, size);
}

// Get memory statistics
Agentite_MemoryStats mem;
agentite_profiler_get_memory_stats(profiler, &mem);
printf("Memory: %zu KB (peak: %zu KB)\n",
       mem.current_bytes / 1024,
       mem.peak_bytes / 1024);
```

## Statistics Access

```c
const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);

// Frame timing
printf("Frame: %.2f ms (avg: %.2f, min: %.2f, max: %.2f)\n",
       stats->frame_time_ms,
       stats->avg_frame_time_ms,
       stats->min_frame_time_ms,
       stats->max_frame_time_ms);
printf("FPS: %.1f\n", stats->fps);

// Phase breakdown
printf("Update: %.2f ms, Render: %.2f ms, Present: %.2f ms\n",
       stats->update_time_ms,
       stats->render_time_ms,
       stats->present_time_ms);

// Render stats
printf("Draw calls: %u, Batches: %u, Vertices: %u\n",
       stats->render.draw_calls,
       stats->render.batch_count,
       stats->render.vertex_count);

// Scopes
for (uint32_t i = 0; i < stats->scope_count; i++) {
    printf("  %s: %.2f ms (%u calls)\n",
           stats->scopes[i].name,
           stats->scopes[i].total_time_ms,
           stats->scopes[i].call_count);
}
```

## Frame History

Access rolling frame history for graphs:

```c
uint32_t history_size = agentite_profiler_get_history_size(profiler);
float *history = malloc(history_size * sizeof(float));
uint32_t count, newest_index;

agentite_profiler_get_frame_history(profiler, history, &count, &newest_index);

// Draw graph from oldest to newest
for (uint32_t i = 0; i < count; i++) {
    // history[i] contains frame time in ms
}

free(history);
```

## Data Export

```c
// Export current stats to CSV
agentite_profiler_export_csv(profiler, "profile_stats.csv");

// Export to JSON for tools
agentite_profiler_export_json(profiler, "profile_stats.json");

// Export frame history for external graphing
agentite_profiler_export_frame_history_csv(profiler, "frame_times.csv");
```

### JSON Output Format

```json
{
  "frame": {
    "time_ms": 16.67,
    "fps": 60.0,
    "avg_time_ms": 16.5,
    "min_time_ms": 15.0,
    "max_time_ms": 18.0,
    "count": 1000
  },
  "phases": {
    "update_ms": 5.0,
    "render_ms": 8.0,
    "present_ms": 3.0
  },
  "render": {
    "draw_calls": 50,
    "batch_count": 10,
    "vertex_count": 5000,
    "index_count": 7500,
    "texture_binds": 5,
    "shader_binds": 2
  },
  "memory": {
    "current_bytes": 10485760,
    "peak_bytes": 15728640,
    "total_allocations": 500,
    "allocation_count": 200
  },
  "entity_count": 150,
  "scopes": [
    {
      "name": "physics",
      "total_ms": 2.5,
      "avg_ms": 2.3,
      "min_ms": 1.8,
      "max_ms": 3.0,
      "call_count": 1
    }
  ]
}
```

## UI Integration

The profiler includes placeholder functions for UI overlay rendering (requires Agentite UI system):

```c
// Draw FPS/frame time overlay
agentite_profiler_draw_overlay(profiler, ui, 10, 10);

// Draw frame time graph
agentite_profiler_draw_graph(profiler, ui, 10, 50, 200, 60);

// Draw detailed stats panel
agentite_profiler_draw_panel(profiler, ui, 10, 120, 300, 400);
```

## Best Practices

### 1. Disable in Release Builds

```c
#ifdef DEBUG
    Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
    g_profiler = agentite_profiler_create(&config);
#else
    g_profiler = NULL;  // All profiler functions handle NULL safely
#endif
```

### 2. Profile Hot Paths

```c
void particle_system_update(ParticleSystem *ps, float dt) {
    AGENTITE_PROFILE_SCOPE(profiler, "particle_system");

    // Only profile expensive operations
    for (int i = 0; i < ps->emitter_count; i++) {
        update_emitter(&ps->emitters[i], dt);
    }
}
```

### 3. Use Scopes for System-Level Profiling

```c
void game_update(float dt) {
    agentite_profiler_begin_update(profiler);

    AGENTITE_PROFILE_SCOPE(profiler, "input");
    input_update();

    AGENTITE_PROFILE_SCOPE(profiler, "ai");
    ai_update(dt);

    AGENTITE_PROFILE_SCOPE(profiler, "physics");
    physics_update(dt);

    AGENTITE_PROFILE_SCOPE(profiler, "animation");
    animation_update(dt);

    agentite_profiler_end_update(profiler);
}
```

### 4. Export for Analysis

Use the JSON/CSV export to analyze performance in external tools:

```bash
# Import frame history into spreadsheet for analysis
# Or use Python/matplotlib to visualize
python analyze_profile.py profile_stats.json
```

## API Reference

### Lifecycle

| Function | Description |
|----------|-------------|
| `agentite_profiler_create(config)` | Create profiler instance |
| `agentite_profiler_destroy(profiler)` | Destroy profiler |
| `agentite_profiler_set_enabled(profiler, enabled)` | Enable/disable |
| `agentite_profiler_is_enabled(profiler)` | Check if enabled |
| `agentite_profiler_reset(profiler)` | Reset all statistics |

### Frame Timing

| Function | Description |
|----------|-------------|
| `agentite_profiler_begin_frame(profiler)` | Start frame timing |
| `agentite_profiler_end_frame(profiler)` | End frame, update stats |

### Phase Timing

| Function | Description |
|----------|-------------|
| `agentite_profiler_begin_update(profiler)` | Start update phase |
| `agentite_profiler_end_update(profiler)` | End update phase |
| `agentite_profiler_begin_render(profiler)` | Start render phase |
| `agentite_profiler_end_render(profiler)` | End render phase |
| `agentite_profiler_begin_present(profiler)` | Start present phase |
| `agentite_profiler_end_present(profiler)` | End present phase |

### Scopes

| Function | Description |
|----------|-------------|
| `agentite_profiler_begin_scope(profiler, name)` | Start named scope |
| `agentite_profiler_end_scope(profiler)` | End current scope |
| `agentite_profiler_get_scope(profiler, name)` | Get scope stats |

### Statistics Reporting

| Function | Description |
|----------|-------------|
| `agentite_profiler_report_draw_call(profiler)` | Count a draw call |
| `agentite_profiler_report_batch(profiler, verts, indices)` | Report batch |
| `agentite_profiler_report_texture_bind(profiler)` | Count texture bind |
| `agentite_profiler_report_shader_bind(profiler)` | Count shader bind |
| `agentite_profiler_report_entity_count(profiler, count)` | Set entity count |
| `agentite_profiler_report_render_stats(profiler, stats)` | Report all render stats |

### Memory Tracking

| Function | Description |
|----------|-------------|
| `agentite_profiler_report_alloc(profiler, bytes)` | Track allocation |
| `agentite_profiler_report_free(profiler, bytes)` | Track free |
| `agentite_profiler_get_memory_stats(profiler, out)` | Get memory stats |

### Statistics Access

| Function | Description |
|----------|-------------|
| `agentite_profiler_get_stats(profiler)` | Get all statistics |
| `agentite_profiler_get_frame_history(...)` | Get frame time history |
| `agentite_profiler_get_history_size(profiler)` | Get history buffer size |

### Export

| Function | Description |
|----------|-------------|
| `agentite_profiler_export_csv(profiler, path)` | Export stats to CSV |
| `agentite_profiler_export_json(profiler, path)` | Export stats to JSON |
| `agentite_profiler_export_frame_history_csv(...)` | Export frame history |

## Thread Safety

All profiler functions are **NOT thread-safe**. The profiler is designed for single-threaded use in the main game loop. If profiling multi-threaded code, create separate profilers per thread or synchronize access.

## See Also

- [UI System](ui.md) - For displaying profiler overlays
- [ECS](ecs.md) - Entity count reporting from Flecs
