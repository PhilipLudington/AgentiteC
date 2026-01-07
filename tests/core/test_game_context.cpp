/*
 * Agentite Game Context Tests
 *
 * Tests for the unified game context system.
 * Note: Full context creation tests require a display and GPU, which
 * may not be available in CI environments.
 */

#include "catch_amalgamated.hpp"
#include "agentite/game_context.h"
#include "agentite/error.h"
#include <cstring>

/* ============================================================================
 * NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Game context NULL safety", "[game_context][null]") {
    SECTION("agentite_game_context_destroy with NULL") {
        // Should not crash
        agentite_game_context_destroy(nullptr);
    }

    SECTION("agentite_game_context_begin_frame with NULL") {
        // Should not crash
        agentite_game_context_begin_frame(nullptr);
    }

    SECTION("agentite_game_context_poll_events with NULL") {
        // Should not crash
        agentite_game_context_poll_events(nullptr);
    }

    SECTION("agentite_game_context_end_frame with NULL") {
        // Should not crash
        agentite_game_context_end_frame(nullptr);
    }

    SECTION("agentite_game_context_begin_render with NULL") {
        SDL_GPUCommandBuffer *cmd = agentite_game_context_begin_render(nullptr);
        REQUIRE(cmd == nullptr);
    }

    SECTION("agentite_game_context_begin_render_pass with NULL") {
        bool result = agentite_game_context_begin_render_pass(nullptr, 0, 0, 0, 1);
        REQUIRE_FALSE(result);
    }

    SECTION("agentite_game_context_begin_render_pass_no_clear with NULL") {
        bool result = agentite_game_context_begin_render_pass_no_clear(nullptr);
        REQUIRE_FALSE(result);
    }

    SECTION("agentite_game_context_end_render_pass_no_submit with NULL") {
        // Should not crash
        agentite_game_context_end_render_pass_no_submit(nullptr);
    }

    SECTION("agentite_game_context_end_render_pass with NULL") {
        // Should not crash
        agentite_game_context_end_render_pass(nullptr);
    }

    SECTION("agentite_game_context_is_running with NULL") {
        REQUIRE_FALSE(agentite_game_context_is_running(nullptr));
    }

    SECTION("agentite_game_context_quit with NULL") {
        // Should not crash
        agentite_game_context_quit(nullptr);
    }
}

/* ============================================================================
 * Default Config Tests
 * ============================================================================ */

TEST_CASE("Default game context config", "[game_context][config]") {
    Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;

    SECTION("Window settings have sensible defaults") {
        REQUIRE(config.window_title != nullptr);
        REQUIRE(strlen(config.window_title) > 0);
        REQUIRE(config.window_width > 0);
        REQUIRE(config.window_height > 0);
        REQUIRE_FALSE(config.fullscreen);
        REQUIRE(config.vsync);
    }

    SECTION("Default dimensions are reasonable") {
        REQUIRE(config.window_width >= 640);
        REQUIRE(config.window_width <= 7680);
        REQUIRE(config.window_height >= 480);
        REQUIRE(config.window_height <= 4320);
    }

    SECTION("Font settings default to NULL") {
        REQUIRE(config.font_path == nullptr);
        REQUIRE(config.ui_font_path == nullptr);
        REQUIRE(config.sdf_font_atlas == nullptr);
        REQUIRE(config.sdf_font_json == nullptr);
    }

    SECTION("Font sizes have sensible defaults") {
        REQUIRE(config.font_size > 0);
        REQUIRE(config.font_size < 200);
        REQUIRE(config.ui_font_size > 0);
        REQUIRE(config.ui_font_size < 200);
    }

    SECTION("Feature flags have sensible defaults") {
        REQUIRE(config.enable_ecs);
        REQUIRE(config.enable_audio);
        REQUIRE(config.enable_ui);
        REQUIRE_FALSE(config.enable_hot_reload);
        REQUIRE_FALSE(config.enable_mods);
    }

    SECTION("Hot reload defaults to disabled") {
        REQUIRE_FALSE(config.enable_hot_reload);
        REQUIRE(config.watch_paths == nullptr);
        REQUIRE(config.watch_path_count == 0);
    }

    SECTION("Mod system defaults to disabled") {
        REQUIRE_FALSE(config.enable_mods);
        REQUIRE(config.mod_paths == nullptr);
        REQUIRE(config.mod_path_count == 0);
        REQUIRE(config.allow_mod_overrides);  // Default to allowing overrides
    }
}

/* ============================================================================
 * Config Customization Tests
 * ============================================================================ */

TEST_CASE("Game context config customization", "[game_context][config]") {
    SECTION("Custom window settings") {
        Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
        config.window_title = "Test Game";
        config.window_width = 1920;
        config.window_height = 1080;
        config.fullscreen = true;
        config.vsync = false;

        REQUIRE(strcmp(config.window_title, "Test Game") == 0);
        REQUIRE(config.window_width == 1920);
        REQUIRE(config.window_height == 1080);
        REQUIRE(config.fullscreen);
        REQUIRE_FALSE(config.vsync);
    }

    SECTION("Custom font settings") {
        Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
        config.font_path = "assets/fonts/main.ttf";
        config.font_size = 24.0f;
        config.ui_font_path = "assets/fonts/ui.ttf";
        config.ui_font_size = 18.0f;

        REQUIRE(strcmp(config.font_path, "assets/fonts/main.ttf") == 0);
        REQUIRE(config.font_size == 24.0f);
        REQUIRE(strcmp(config.ui_font_path, "assets/fonts/ui.ttf") == 0);
        REQUIRE(config.ui_font_size == 18.0f);
    }

    SECTION("Custom SDF font settings") {
        Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
        config.sdf_font_atlas = "assets/fonts/sdf_atlas.png";
        config.sdf_font_json = "assets/fonts/sdf_metrics.json";

        REQUIRE(strcmp(config.sdf_font_atlas, "assets/fonts/sdf_atlas.png") == 0);
        REQUIRE(strcmp(config.sdf_font_json, "assets/fonts/sdf_metrics.json") == 0);
    }

    SECTION("Disable optional features") {
        Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
        config.enable_ecs = false;
        config.enable_audio = false;
        config.enable_ui = false;

        REQUIRE_FALSE(config.enable_ecs);
        REQUIRE_FALSE(config.enable_audio);
        REQUIRE_FALSE(config.enable_ui);
    }

    SECTION("Enable hot reload") {
        Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
        const char *paths[] = {"assets/", "shaders/"};
        config.enable_hot_reload = true;
        config.watch_paths = paths;
        config.watch_path_count = 2;

        REQUIRE(config.enable_hot_reload);
        REQUIRE(config.watch_paths != nullptr);
        REQUIRE(config.watch_path_count == 2);
        REQUIRE(strcmp(config.watch_paths[0], "assets/") == 0);
        REQUIRE(strcmp(config.watch_paths[1], "shaders/") == 0);
    }

    SECTION("Enable mods") {
        Agentite_GameContextConfig config = AGENTITE_GAME_CONTEXT_DEFAULT;
        const char *mod_paths[] = {"mods/"};
        config.enable_mods = true;
        config.mod_paths = mod_paths;
        config.mod_path_count = 1;
        config.allow_mod_overrides = false;

        REQUIRE(config.enable_mods);
        REQUIRE(config.mod_paths != nullptr);
        REQUIRE(config.mod_path_count == 1);
        REQUIRE_FALSE(config.allow_mod_overrides);
    }
}

/* ============================================================================
 * Struct Layout Tests
 * ============================================================================ */

TEST_CASE("Game context struct layout", "[game_context][struct]") {
    SECTION("Context struct has expected pointer members") {
        // Verify that the struct layout is correct by checking offsets
        // This is a compile-time sanity check - if the struct changes
        // incompatibly, this test will fail to compile or assert

        Agentite_GameContext ctx = {};

        // All pointers should initialize to NULL
        REQUIRE(ctx.engine == nullptr);
        REQUIRE(ctx.sprites == nullptr);
        REQUIRE(ctx.text == nullptr);
        REQUIRE(ctx.camera == nullptr);
        REQUIRE(ctx.input == nullptr);
        REQUIRE(ctx.audio == nullptr);
        REQUIRE(ctx.ecs == nullptr);
        REQUIRE(ctx.ui == nullptr);
        REQUIRE(ctx.font == nullptr);
        REQUIRE(ctx.sdf_font == nullptr);
        REQUIRE(ctx.watcher == nullptr);
        REQUIRE(ctx.hotreload == nullptr);
        REQUIRE(ctx.mods == nullptr);
    }

    SECTION("Context struct numeric fields initialize to zero") {
        Agentite_GameContext ctx = {};

        REQUIRE(ctx.delta_time == 0.0f);
        REQUIRE(ctx.frame_count == 0);
        REQUIRE(ctx.window_width == 0);
        REQUIRE(ctx.window_height == 0);
    }
}

/* ============================================================================
 * Config Struct Layout Tests
 * ============================================================================ */

TEST_CASE("Game context config struct layout", "[game_context][config][struct]") {
    SECTION("Config struct zero-initialization") {
        Agentite_GameContextConfig config = {};

        REQUIRE(config.window_title == nullptr);
        REQUIRE(config.window_width == 0);
        REQUIRE(config.window_height == 0);
        REQUIRE_FALSE(config.fullscreen);
        REQUIRE_FALSE(config.vsync);
        REQUIRE(config.font_path == nullptr);
        REQUIRE(config.font_size == 0.0f);
        REQUIRE_FALSE(config.enable_ecs);
        REQUIRE_FALSE(config.enable_audio);
        REQUIRE_FALSE(config.enable_ui);
        REQUIRE_FALSE(config.enable_hot_reload);
        REQUIRE_FALSE(config.enable_mods);
    }
}
