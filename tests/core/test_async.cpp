/**
 * Agentite Engine - Async Asset Loading Tests
 */

#include <catch_amalgamated.hpp>
#include "agentite/async.h"
#include "agentite/asset.h"
#include "agentite/error.h"

#include <SDL3/SDL.h>
#include <atomic>
#include <thread>
#include <chrono>

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class AsyncLoaderFixture {
public:
    Agentite_AsyncLoader *loader = nullptr;
    Agentite_AssetRegistry *registry = nullptr;

    AsyncLoaderFixture() {
        /* Ensure SDL is initialized for threading primitives */
        if (!SDL_WasInit(SDL_INIT_EVENTS)) {
            SDL_Init(SDL_INIT_EVENTS);
        }

        Agentite_AsyncLoaderConfig config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
        config.num_threads = 2;
        loader = agentite_async_loader_create(&config);
        registry = agentite_asset_registry_create();
    }

    ~AsyncLoaderFixture() {
        agentite_async_loader_destroy(loader);
        agentite_asset_registry_destroy(registry);
    }
};

/* ============================================================================
 * Basic Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Async loader creation and destruction", "[async]") {
    /* Ensure SDL is initialized */
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    SECTION("Create with default config") {
        Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
        REQUIRE(loader != nullptr);
        agentite_async_loader_destroy(loader);
    }

    SECTION("Create with custom config") {
        Agentite_AsyncLoaderConfig config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
        config.num_threads = 4;
        config.max_pending = 100;
        config.max_completed_per_frame = 10;

        Agentite_AsyncLoader *loader = agentite_async_loader_create(&config);
        REQUIRE(loader != nullptr);
        agentite_async_loader_destroy(loader);
    }

    SECTION("Destroy NULL is safe") {
        agentite_async_loader_destroy(NULL);  /* Should not crash */
    }

    SECTION("Update NULL is safe") {
        agentite_async_loader_update(NULL);  /* Should not crash */
    }
}

/* ============================================================================
 * Load Request Handle Tests
 * ============================================================================ */

TEST_CASE("Load request handle validation", "[async]") {
    SECTION("Invalid request constant") {
        Agentite_LoadRequest request = AGENTITE_INVALID_LOAD_REQUEST;
        REQUIRE_FALSE(agentite_load_request_is_valid(request));
    }

    SECTION("Valid request from non-zero value") {
        Agentite_LoadRequest request = { 42 };
        REQUIRE(agentite_load_request_is_valid(request));
    }
}

/* ============================================================================
 * Status Query Tests
 * ============================================================================ */

TEST_CASE("Load status queries", "[async]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
    REQUIRE(loader != nullptr);

    SECTION("Invalid request returns INVALID status") {
        Agentite_LoadStatus status = agentite_async_get_status(
            loader, AGENTITE_INVALID_LOAD_REQUEST);
        REQUIRE(status == AGENTITE_LOAD_INVALID);
    }

    SECTION("NULL loader returns INVALID status") {
        Agentite_LoadRequest request = { 1 };
        Agentite_LoadStatus status = agentite_async_get_status(NULL, request);
        REQUIRE(status == AGENTITE_LOAD_INVALID);
    }

    SECTION("Non-existent request returns INVALID") {
        Agentite_LoadRequest request = { 9999 };
        Agentite_LoadStatus status = agentite_async_get_status(loader, request);
        REQUIRE(status == AGENTITE_LOAD_INVALID);
    }

    agentite_async_loader_destroy(loader);
}

/* ============================================================================
 * Progress Tracking Tests
 * ============================================================================ */

TEST_CASE("Progress tracking functions", "[async]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
    REQUIRE(loader != nullptr);

    SECTION("Initial state is idle") {
        REQUIRE(agentite_async_is_idle(loader) == true);
        REQUIRE(agentite_async_pending_count(loader) == 0);
        REQUIRE(agentite_async_completed_count(loader) == 0);
    }

    SECTION("NULL loader returns safe values") {
        REQUIRE(agentite_async_is_idle(NULL) == true);
        REQUIRE(agentite_async_pending_count(NULL) == 0);
        REQUIRE(agentite_async_completed_count(NULL) == 0);
    }

    agentite_async_loader_destroy(loader);
}

/* ============================================================================
 * Cancellation Tests
 * ============================================================================ */

TEST_CASE("Load cancellation", "[async]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
    REQUIRE(loader != nullptr);

    SECTION("Cancel invalid request returns false") {
        bool cancelled = agentite_async_cancel(loader, AGENTITE_INVALID_LOAD_REQUEST);
        REQUIRE_FALSE(cancelled);
    }

    SECTION("Cancel non-existent request returns false") {
        Agentite_LoadRequest request = { 9999 };
        bool cancelled = agentite_async_cancel(loader, request);
        REQUIRE_FALSE(cancelled);
    }

    SECTION("Cancel with NULL loader returns false") {
        Agentite_LoadRequest request = { 1 };
        bool cancelled = agentite_async_cancel(NULL, request);
        REQUIRE_FALSE(cancelled);
    }

    agentite_async_loader_destroy(loader);
}

/* ============================================================================
 * Wait All Tests
 * ============================================================================ */

TEST_CASE("Wait for completion", "[async]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
    REQUIRE(loader != nullptr);

    SECTION("Wait on empty queue returns immediately") {
        bool completed = agentite_async_wait_all(loader, 100);
        REQUIRE(completed == true);
    }

    SECTION("Wait with NULL loader returns true") {
        bool completed = agentite_async_wait_all(NULL, 100);
        REQUIRE(completed == true);
    }

    agentite_async_loader_destroy(loader);
}

/* ============================================================================
 * Streaming Region Tests
 * ============================================================================ */

TEST_CASE("Streaming regions", "[async][streaming]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
    REQUIRE(loader != nullptr);

    SECTION("Create region with name") {
        Agentite_StreamRegion region = agentite_stream_region_create(loader, "test_region");
        REQUIRE(region.value != 0);
        agentite_stream_region_destroy(loader, region);
    }

    SECTION("Create region with NULL name") {
        Agentite_StreamRegion region = agentite_stream_region_create(loader, NULL);
        REQUIRE(region.value != 0);
        agentite_stream_region_destroy(loader, region);
    }

    SECTION("Create multiple regions") {
        Agentite_StreamRegion r1 = agentite_stream_region_create(loader, "region1");
        Agentite_StreamRegion r2 = agentite_stream_region_create(loader, "region2");
        Agentite_StreamRegion r3 = agentite_stream_region_create(loader, "region3");

        REQUIRE(r1.value != 0);
        REQUIRE(r2.value != 0);
        REQUIRE(r3.value != 0);
        REQUIRE(r1.value != r2.value);
        REQUIRE(r2.value != r3.value);

        agentite_stream_region_destroy(loader, r1);
        agentite_stream_region_destroy(loader, r2);
        agentite_stream_region_destroy(loader, r3);
    }

    SECTION("Add assets to region") {
        Agentite_StreamRegion region = agentite_stream_region_create(loader, "test");

        agentite_stream_region_add_asset(loader, region, "asset1.png", 0);
        agentite_stream_region_add_asset(loader, region, "asset2.png", 0);
        agentite_stream_region_add_asset(loader, region, "asset3.wav", 0);

        /* Progress should be 0 until activated */
        float progress = agentite_stream_region_progress(loader, region);
        REQUIRE(progress == 0.0f);

        agentite_stream_region_destroy(loader, region);
    }

    SECTION("Invalid region constant") {
        REQUIRE(AGENTITE_INVALID_STREAM_REGION.value == 0);
    }

    SECTION("Destroy invalid region is safe") {
        agentite_stream_region_destroy(loader, AGENTITE_INVALID_STREAM_REGION);
    }

    SECTION("Create with NULL loader") {
        Agentite_StreamRegion region = agentite_stream_region_create(NULL, "test");
        REQUIRE(region.value == 0);
    }

    agentite_async_loader_destroy(loader);
}

/* ============================================================================
 * Load Request Validation (without actual file loading)
 * ============================================================================ */

TEST_CASE("Async load parameter validation", "[async]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(loader != nullptr);
    REQUIRE(registry != nullptr);

    SECTION("Texture load with NULL loader fails") {
        Agentite_LoadRequest req = agentite_texture_load_async(
            NULL, nullptr, registry, "test.png", nullptr, nullptr);
        REQUIRE_FALSE(agentite_load_request_is_valid(req));
    }

    SECTION("Texture load with NULL path fails") {
        Agentite_LoadRequest req = agentite_texture_load_async(
            loader, nullptr, registry, NULL, nullptr, nullptr);
        REQUIRE_FALSE(agentite_load_request_is_valid(req));
    }

    SECTION("Texture load with NULL registry fails") {
        Agentite_LoadRequest req = agentite_texture_load_async(
            loader, nullptr, NULL, "test.png", nullptr, nullptr);
        REQUIRE_FALSE(agentite_load_request_is_valid(req));
    }

    SECTION("Sound load with NULL loader fails") {
        Agentite_LoadRequest req = agentite_sound_load_async(
            NULL, nullptr, registry, "test.wav", nullptr, nullptr);
        REQUIRE_FALSE(agentite_load_request_is_valid(req));
    }

    SECTION("Music load with NULL loader fails") {
        Agentite_LoadRequest req = agentite_music_load_async(
            NULL, nullptr, registry, "test.ogg", nullptr, nullptr);
        REQUIRE_FALSE(agentite_load_request_is_valid(req));
    }

    agentite_asset_registry_destroy(registry);
    agentite_async_loader_destroy(loader);
}

/* ============================================================================
 * Thread Pool Stress Test
 * ============================================================================ */

TEST_CASE("Thread pool stress test", "[async][stress]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    SECTION("Rapid create/destroy cycles") {
        for (int i = 0; i < 10; i++) {
            Agentite_AsyncLoaderConfig config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
            config.num_threads = 2;
            Agentite_AsyncLoader *loader = agentite_async_loader_create(&config);
            REQUIRE(loader != nullptr);
            agentite_async_loader_destroy(loader);
        }
    }

    SECTION("Update with no work") {
        Agentite_AsyncLoader *loader = agentite_async_loader_create(NULL);
        REQUIRE(loader != nullptr);

        /* Call update many times with no work */
        for (int i = 0; i < 100; i++) {
            agentite_async_loader_update(loader);
        }

        REQUIRE(agentite_async_is_idle(loader));
        agentite_async_loader_destroy(loader);
    }
}

/* ============================================================================
 * Configuration Edge Cases
 * ============================================================================ */

TEST_CASE("Configuration edge cases", "[async]") {
    if (!SDL_WasInit(SDL_INIT_EVENTS)) {
        SDL_Init(SDL_INIT_EVENTS);
    }

    SECTION("Single thread") {
        Agentite_AsyncLoaderConfig config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
        config.num_threads = 1;
        Agentite_AsyncLoader *loader = agentite_async_loader_create(&config);
        REQUIRE(loader != nullptr);
        agentite_async_loader_destroy(loader);
    }

    SECTION("Many threads") {
        Agentite_AsyncLoaderConfig config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
        config.num_threads = 8;
        Agentite_AsyncLoader *loader = agentite_async_loader_create(&config);
        REQUIRE(loader != nullptr);
        agentite_async_loader_destroy(loader);
    }

    SECTION("Limited callbacks per frame") {
        Agentite_AsyncLoaderConfig config = AGENTITE_ASYNC_LOADER_CONFIG_DEFAULT;
        config.max_completed_per_frame = 1;
        Agentite_AsyncLoader *loader = agentite_async_loader_create(&config);
        REQUIRE(loader != nullptr);
        agentite_async_loader_destroy(loader);
    }
}

/* ============================================================================
 * Priority Tests
 * ============================================================================ */

TEST_CASE("Load priority constants", "[async]") {
    /* Verify priority ordering */
    REQUIRE(AGENTITE_PRIORITY_LOW < AGENTITE_PRIORITY_NORMAL);
    REQUIRE(AGENTITE_PRIORITY_NORMAL < AGENTITE_PRIORITY_HIGH);
    REQUIRE(AGENTITE_PRIORITY_HIGH < AGENTITE_PRIORITY_CRITICAL);
}

/* ============================================================================
 * Load Result Structure Tests
 * ============================================================================ */

TEST_CASE("Load result structure", "[async]") {
    SECTION("Success result") {
        Agentite_LoadResult result;
        result.success = true;
        result.error = NULL;

        REQUIRE(result.success == true);
        REQUIRE(result.error == nullptr);
    }

    SECTION("Failure result") {
        Agentite_LoadResult result;
        result.success = false;
        result.error = "Test error message";

        REQUIRE(result.success == false);
        REQUIRE(result.error != nullptr);
        REQUIRE(strcmp(result.error, "Test error message") == 0);
    }
}

/* ============================================================================
 * Load Options Tests
 * ============================================================================ */

TEST_CASE("Load options defaults", "[async]") {
    SECTION("Texture load options default") {
        Agentite_TextureLoadOptions options = AGENTITE_TEXTURE_LOAD_OPTIONS_DEFAULT;
        REQUIRE(options.priority == AGENTITE_PRIORITY_NORMAL);
    }

    SECTION("Audio load options default") {
        Agentite_AudioLoadOptions options = AGENTITE_AUDIO_LOAD_OPTIONS_DEFAULT;
        REQUIRE(options.priority == AGENTITE_PRIORITY_NORMAL);
    }
}
