/*
 * Agentite Profiler Tests
 *
 * Tests for the performance profiling system.
 */

#include "catch_amalgamated.hpp"
#include "agentite/profiler.h"
#include <cstring>
#include <thread>
#include <chrono>

/* ============================================================================
 * Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Profiler lifecycle", "[profiler][lifecycle]") {

    SECTION("Create with default config") {
        Agentite_Profiler *profiler = agentite_profiler_create(nullptr);
        REQUIRE(profiler != nullptr);
        REQUIRE(agentite_profiler_is_enabled(profiler));
        agentite_profiler_destroy(profiler);
    }

    SECTION("Create with custom config") {
        Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
        config.history_size = 64;
        config.track_scopes = true;
        config.track_memory = true;

        Agentite_Profiler *profiler = agentite_profiler_create(&config);
        REQUIRE(profiler != nullptr);
        REQUIRE(agentite_profiler_get_history_size(profiler) == 64);
        agentite_profiler_destroy(profiler);
    }

    SECTION("Destroy NULL is safe") {
        agentite_profiler_destroy(nullptr);  // Should not crash
    }

    SECTION("Enable/disable") {
        Agentite_Profiler *profiler = agentite_profiler_create(nullptr);
        REQUIRE(agentite_profiler_is_enabled(profiler));

        agentite_profiler_set_enabled(profiler, false);
        REQUIRE_FALSE(agentite_profiler_is_enabled(profiler));

        agentite_profiler_set_enabled(profiler, true);
        REQUIRE(agentite_profiler_is_enabled(profiler));

        agentite_profiler_destroy(profiler);
    }

    SECTION("Reset clears state") {
        Agentite_Profiler *profiler = agentite_profiler_create(nullptr);

        // Record some frames
        for (int i = 0; i < 10; i++) {
            agentite_profiler_begin_frame(profiler);
            agentite_profiler_end_frame(profiler);
        }

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->frame_count == 10);

        agentite_profiler_reset(profiler);
        stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->frame_count == 0);

        agentite_profiler_destroy(profiler);
    }
}

/* ============================================================================
 * Frame Timing Tests
 * ============================================================================ */

TEST_CASE("Profiler frame timing", "[profiler][timing]") {
    Agentite_Profiler *profiler = agentite_profiler_create(nullptr);

    SECTION("Frame count increments") {
        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->frame_count == 0);

        agentite_profiler_begin_frame(profiler);
        agentite_profiler_end_frame(profiler);

        stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->frame_count == 1);

        agentite_profiler_begin_frame(profiler);
        agentite_profiler_end_frame(profiler);

        stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->frame_count == 2);
    }

    SECTION("Frame time is positive") {
        agentite_profiler_begin_frame(profiler);
        // Simulate some work
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->frame_time_ms > 0.0);
    }

    SECTION("FPS is calculated") {
        agentite_profiler_begin_frame(profiler);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->fps > 0.0);
        REQUIRE(stats->fps < 200.0);  // Reasonable upper bound
    }

    SECTION("Rolling average calculated over multiple frames") {
        for (int i = 0; i < 20; i++) {
            agentite_profiler_begin_frame(profiler);
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            agentite_profiler_end_frame(profiler);
        }

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->avg_frame_time_ms > 0.0);
        REQUIRE(stats->min_frame_time_ms > 0.0);
        REQUIRE(stats->max_frame_time_ms >= stats->min_frame_time_ms);
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Phase Timing Tests
 * ============================================================================ */

TEST_CASE("Profiler phase timing", "[profiler][phases]") {
    Agentite_Profiler *profiler = agentite_profiler_create(nullptr);

    SECTION("Update phase timing") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_begin_update(profiler);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        agentite_profiler_end_update(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->update_time_ms > 0.0);
    }

    SECTION("Render phase timing") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_begin_render(profiler);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        agentite_profiler_end_render(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->render_time_ms > 0.0);
    }

    SECTION("Present phase timing") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_begin_present(profiler);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        agentite_profiler_end_present(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->present_time_ms > 0.0);
    }

    SECTION("All phases together") {
        agentite_profiler_begin_frame(profiler);

        agentite_profiler_begin_update(profiler);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        agentite_profiler_end_update(profiler);

        agentite_profiler_begin_render(profiler);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        agentite_profiler_end_render(profiler);

        agentite_profiler_begin_present(profiler);
        std::this_thread::sleep_for(std::chrono::microseconds(500));
        agentite_profiler_end_present(profiler);

        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->update_time_ms > 0.0);
        REQUIRE(stats->render_time_ms > 0.0);
        REQUIRE(stats->present_time_ms > 0.0);
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Scope-Based Profiling Tests
 * ============================================================================ */

TEST_CASE("Profiler scope tracking", "[profiler][scopes]") {
    Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
    config.track_scopes = true;
    Agentite_Profiler *profiler = agentite_profiler_create(&config);

    SECTION("Simple scope") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_begin_scope(profiler, "test_scope");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        agentite_profiler_end_scope(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->scope_count > 0);

        // Find the test_scope
        bool found = false;
        for (uint32_t i = 0; i < stats->scope_count; i++) {
            if (strcmp(stats->scopes[i].name, "test_scope") == 0) {
                found = true;
                REQUIRE(stats->scopes[i].total_time_ms > 0.0);
                REQUIRE(stats->scopes[i].call_count == 1);
                break;
            }
        }
        REQUIRE(found);
    }

    SECTION("Multiple calls to same scope") {
        agentite_profiler_begin_frame(profiler);
        for (int i = 0; i < 5; i++) {
            agentite_profiler_begin_scope(profiler, "repeated_scope");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            agentite_profiler_end_scope(profiler);
        }
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        for (uint32_t i = 0; i < stats->scope_count; i++) {
            if (strcmp(stats->scopes[i].name, "repeated_scope") == 0) {
                REQUIRE(stats->scopes[i].call_count == 5);
                break;
            }
        }
    }

    SECTION("Nested scopes") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_begin_scope(profiler, "outer");
        agentite_profiler_begin_scope(profiler, "inner");
        std::this_thread::sleep_for(std::chrono::microseconds(100));
        agentite_profiler_end_scope(profiler);  // end inner
        agentite_profiler_end_scope(profiler);  // end outer
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->scope_count >= 2);
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Render Statistics Tests
 * ============================================================================ */

TEST_CASE("Profiler render statistics", "[profiler][render]") {
    Agentite_Profiler *profiler = agentite_profiler_create(nullptr);

    SECTION("Draw call counting") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_report_draw_call(profiler);
        agentite_profiler_report_draw_call(profiler);
        agentite_profiler_report_draw_call(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->render.draw_calls == 3);
    }

    SECTION("Batch reporting") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_report_batch(profiler, 100, 150);
        agentite_profiler_report_batch(profiler, 200, 300);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->render.batch_count == 2);
        REQUIRE(stats->render.vertex_count == 300);
        REQUIRE(stats->render.index_count == 450);
    }

    SECTION("Texture and shader binds") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_report_texture_bind(profiler);
        agentite_profiler_report_texture_bind(profiler);
        agentite_profiler_report_shader_bind(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->render.texture_binds == 2);
        REQUIRE(stats->render.shader_binds == 1);
    }

    SECTION("Counters reset each frame") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_report_draw_call(profiler);
        agentite_profiler_end_frame(profiler);

        agentite_profiler_begin_frame(profiler);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->render.draw_calls == 0);
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Memory Statistics Tests
 * ============================================================================ */

TEST_CASE("Profiler memory statistics", "[profiler][memory]") {
    Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
    config.track_memory = true;
    Agentite_Profiler *profiler = agentite_profiler_create(&config);

    SECTION("Allocation tracking") {
        agentite_profiler_report_alloc(profiler, 1024);
        agentite_profiler_report_alloc(profiler, 2048);

        Agentite_MemoryStats stats;
        agentite_profiler_get_memory_stats(profiler, &stats);

        REQUIRE(stats.current_bytes == 3072);
        REQUIRE(stats.total_allocations == 2);
        REQUIRE(stats.allocation_count == 2);
    }

    SECTION("Free tracking") {
        agentite_profiler_report_alloc(profiler, 1024);
        agentite_profiler_report_alloc(profiler, 2048);
        agentite_profiler_report_free(profiler, 1024);

        Agentite_MemoryStats stats;
        agentite_profiler_get_memory_stats(profiler, &stats);

        REQUIRE(stats.current_bytes == 2048);
        REQUIRE(stats.total_frees == 1);
        REQUIRE(stats.allocation_count == 1);
    }

    SECTION("Peak tracking") {
        agentite_profiler_report_alloc(profiler, 1024);
        agentite_profiler_report_alloc(profiler, 2048);  // Peak at 3072
        agentite_profiler_report_free(profiler, 2048);

        Agentite_MemoryStats stats;
        agentite_profiler_get_memory_stats(profiler, &stats);

        REQUIRE(stats.current_bytes == 1024);
        REQUIRE(stats.peak_bytes == 3072);
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Entity Count Tests
 * ============================================================================ */

TEST_CASE("Profiler entity count", "[profiler][entities]") {
    Agentite_Profiler *profiler = agentite_profiler_create(nullptr);

    SECTION("Report entity count") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_report_entity_count(profiler, 42);
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->entity_count == 42);
    }

    SECTION("Entity count updates") {
        agentite_profiler_begin_frame(profiler);
        agentite_profiler_report_entity_count(profiler, 10);
        agentite_profiler_report_entity_count(profiler, 20);  // Overwrites
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        REQUIRE(stats->entity_count == 20);
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Frame History Tests
 * ============================================================================ */

TEST_CASE("Profiler frame history", "[profiler][history]") {
    Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
    config.history_size = 32;
    Agentite_Profiler *profiler = agentite_profiler_create(&config);

    SECTION("History fills up") {
        for (int i = 0; i < 20; i++) {
            agentite_profiler_begin_frame(profiler);
            agentite_profiler_end_frame(profiler);
        }

        float history[32];
        uint32_t count, index;
        bool ok = agentite_profiler_get_frame_history(profiler, history, &count, &index);
        REQUIRE(ok);
        REQUIRE(count == 20);
    }

    SECTION("History wraps around") {
        for (int i = 0; i < 50; i++) {
            agentite_profiler_begin_frame(profiler);
            agentite_profiler_end_frame(profiler);
        }

        float history[32];
        uint32_t count, index;
        bool ok = agentite_profiler_get_frame_history(profiler, history, &count, &index);
        REQUIRE(ok);
        REQUIRE(count == 32);  // Capped at history size
    }

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * Disabled Profiler Tests
 * ============================================================================ */

TEST_CASE("Disabled profiler is no-op", "[profiler][disabled]") {
    Agentite_Profiler *profiler = agentite_profiler_create(nullptr);
    agentite_profiler_set_enabled(profiler, false);

    // All these should be no-ops and not crash
    agentite_profiler_begin_frame(profiler);
    agentite_profiler_begin_update(profiler);
    agentite_profiler_end_update(profiler);
    agentite_profiler_begin_render(profiler);
    agentite_profiler_end_render(profiler);
    agentite_profiler_begin_scope(profiler, "test");
    agentite_profiler_end_scope(profiler);
    agentite_profiler_report_draw_call(profiler);
    agentite_profiler_report_batch(profiler, 100, 100);
    agentite_profiler_report_alloc(profiler, 1024);
    agentite_profiler_report_free(profiler, 1024);
    agentite_profiler_end_frame(profiler);

    // Stats should show no activity
    const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
    REQUIRE(stats->frame_count == 0);

    agentite_profiler_destroy(profiler);
}

/* ============================================================================
 * C++ RAII Scope Helper Tests
 * ============================================================================ */

TEST_CASE("C++ scope helper", "[profiler][cpp]") {
    Agentite_ProfilerConfig config = AGENTITE_PROFILER_DEFAULT;
    config.track_scopes = true;
    Agentite_Profiler *profiler = agentite_profiler_create(&config);

    SECTION("AGENTITE_PROFILE_SCOPE macro") {
        agentite_profiler_begin_frame(profiler);
        {
            AGENTITE_PROFILE_SCOPE(profiler, "raii_scope");
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        agentite_profiler_end_frame(profiler);

        const Agentite_ProfilerStats *stats = agentite_profiler_get_stats(profiler);
        bool found = false;
        for (uint32_t i = 0; i < stats->scope_count; i++) {
            if (strcmp(stats->scopes[i].name, "raii_scope") == 0) {
                found = true;
                REQUIRE(stats->scopes[i].total_time_ms > 0.0);
                break;
            }
        }
        REQUIRE(found);
    }

    agentite_profiler_destroy(profiler);
}
