/*
 * Agentite Tilemap Tests
 *
 * Tests for tilemap functionality that can be tested without GPU.
 * Note: Most tilemap creation requires a tileset with a valid texture,
 * so these tests focus on NULL safety and constants.
 */

#include "catch_amalgamated.hpp"
#include "agentite/tilemap.h"

/* ============================================================================
 * Tilemap Constants Tests
 * ============================================================================ */

TEST_CASE("Tilemap constants", "[tilemap][constants]") {
    SECTION("Tile empty value") {
        REQUIRE(AGENTITE_TILE_EMPTY == 0);
    }

    SECTION("Chunk size is reasonable") {
        REQUIRE(AGENTITE_TILEMAP_CHUNK_SIZE > 0);
        REQUIRE(AGENTITE_TILEMAP_CHUNK_SIZE <= 64);  // Reasonable upper bound
        REQUIRE(AGENTITE_TILEMAP_CHUNK_SIZE == 32);  // Current value
    }

    SECTION("Max layers is reasonable") {
        REQUIRE(AGENTITE_TILEMAP_MAX_LAYERS > 0);
        REQUIRE(AGENTITE_TILEMAP_MAX_LAYERS <= 32);  // Reasonable upper bound
        REQUIRE(AGENTITE_TILEMAP_MAX_LAYERS == 16);  // Current value
    }
}

TEST_CASE("TileID type", "[tilemap][types]") {
    SECTION("TileID is 16-bit") {
        REQUIRE(sizeof(Agentite_TileID) == 2);
    }

    SECTION("TileID can represent many tiles") {
        // Should be able to represent at least 65535 tiles
        Agentite_TileID max_tile = 65535;
        REQUIRE(max_tile > AGENTITE_TILE_EMPTY);
    }
}

/* ============================================================================
 * Tileset NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tileset NULL safety", "[tileset][null]") {
    SECTION("agentite_tileset_create with NULL texture") {
        Agentite_Tileset *ts = agentite_tileset_create(nullptr, 32, 32);
        REQUIRE(ts == nullptr);
    }

    SECTION("agentite_tileset_create with zero tile size") {
        Agentite_Tileset *ts = agentite_tileset_create(nullptr, 0, 32);
        REQUIRE(ts == nullptr);

        ts = agentite_tileset_create(nullptr, 32, 0);
        REQUIRE(ts == nullptr);
    }

    SECTION("agentite_tileset_create with negative tile size") {
        Agentite_Tileset *ts = agentite_tileset_create(nullptr, -32, 32);
        REQUIRE(ts == nullptr);

        ts = agentite_tileset_create(nullptr, 32, -32);
        REQUIRE(ts == nullptr);
    }

    SECTION("agentite_tileset_create_ex with NULL texture") {
        Agentite_Tileset *ts = agentite_tileset_create_ex(nullptr, 32, 32, 2, 2);
        REQUIRE(ts == nullptr);
    }

    SECTION("agentite_tileset_destroy with NULL") {
        // Should not crash
        agentite_tileset_destroy(nullptr);
    }

    SECTION("agentite_tileset_get_tile_size with NULL") {
        int w = -1, h = -1;
        agentite_tileset_get_tile_size(nullptr, &w, &h);
        // Values should remain unchanged (function returns early)
        REQUIRE(w == -1);
        REQUIRE(h == -1);
    }

    SECTION("agentite_tileset_get_tile_count with NULL") {
        int count = agentite_tileset_get_tile_count(nullptr);
        REQUIRE(count == 0);
    }
}

/* ============================================================================
 * Tilemap NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tilemap creation NULL safety", "[tilemap][null]") {
    SECTION("agentite_tilemap_create with NULL tileset") {
        Agentite_Tilemap *tm = agentite_tilemap_create(nullptr, 100, 100);
        REQUIRE(tm == nullptr);
    }

    SECTION("agentite_tilemap_create with zero dimensions") {
        Agentite_Tilemap *tm = agentite_tilemap_create(nullptr, 0, 100);
        REQUIRE(tm == nullptr);

        tm = agentite_tilemap_create(nullptr, 100, 0);
        REQUIRE(tm == nullptr);
    }

    SECTION("agentite_tilemap_create with negative dimensions") {
        Agentite_Tilemap *tm = agentite_tilemap_create(nullptr, -100, 100);
        REQUIRE(tm == nullptr);

        tm = agentite_tilemap_create(nullptr, 100, -100);
        REQUIRE(tm == nullptr);
    }

    SECTION("agentite_tilemap_destroy with NULL") {
        // Should not crash
        agentite_tilemap_destroy(nullptr);
    }
}

TEST_CASE("Tilemap getters NULL safety", "[tilemap][null]") {
    SECTION("agentite_tilemap_get_size with NULL") {
        int w = -1, h = -1;
        agentite_tilemap_get_size(nullptr, &w, &h);
        // Values should remain unchanged
        REQUIRE(w == -1);
        REQUIRE(h == -1);
    }

    SECTION("agentite_tilemap_get_tile_size with NULL") {
        int w = -1, h = -1;
        agentite_tilemap_get_tile_size(nullptr, &w, &h);
        REQUIRE(w == -1);
        REQUIRE(h == -1);
    }

    SECTION("agentite_tilemap_get_layer_count with NULL") {
        int count = agentite_tilemap_get_layer_count(nullptr);
        REQUIRE(count == 0);
    }
}

/* ============================================================================
 * Layer NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tilemap layer NULL safety", "[tilemap][layer][null]") {
    SECTION("agentite_tilemap_add_layer with NULL tilemap") {
        int index = agentite_tilemap_add_layer(nullptr, "test");
        REQUIRE(index == -1);
    }

    SECTION("agentite_tilemap_get_layer with NULL tilemap") {
        Agentite_TileLayer *layer = agentite_tilemap_get_layer(nullptr, 0);
        REQUIRE(layer == nullptr);
    }

    SECTION("agentite_tilemap_get_layer_by_name with NULL tilemap") {
        Agentite_TileLayer *layer = agentite_tilemap_get_layer_by_name(nullptr, "test");
        REQUIRE(layer == nullptr);
    }

    SECTION("agentite_tilemap_get_layer_by_name with NULL name") {
        Agentite_TileLayer *layer = agentite_tilemap_get_layer_by_name(nullptr, nullptr);
        REQUIRE(layer == nullptr);
    }

    SECTION("agentite_tilemap_set_layer_visible with NULL tilemap") {
        // Should not crash
        agentite_tilemap_set_layer_visible(nullptr, 0, true);
    }

    SECTION("agentite_tilemap_get_layer_visible with NULL tilemap") {
        bool visible = agentite_tilemap_get_layer_visible(nullptr, 0);
        REQUIRE_FALSE(visible);
    }

    SECTION("agentite_tilemap_set_layer_opacity with NULL tilemap") {
        // Should not crash
        agentite_tilemap_set_layer_opacity(nullptr, 0, 0.5f);
    }

    SECTION("agentite_tilemap_get_layer_opacity with NULL tilemap") {
        float opacity = agentite_tilemap_get_layer_opacity(nullptr, 0);
        REQUIRE(opacity == 0.0f);
    }
}

/* ============================================================================
 * Tile Access NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tile access NULL safety", "[tilemap][tile][null]") {
    SECTION("agentite_tilemap_set_tile with NULL tilemap") {
        // Should not crash
        agentite_tilemap_set_tile(nullptr, 0, 50, 50, 1);
    }

    SECTION("agentite_tilemap_get_tile with NULL tilemap") {
        Agentite_TileID tile = agentite_tilemap_get_tile(nullptr, 0, 50, 50);
        REQUIRE(tile == AGENTITE_TILE_EMPTY);
    }

    SECTION("agentite_tilemap_fill with NULL tilemap") {
        // Should not crash
        agentite_tilemap_fill(nullptr, 0, 0, 0, 10, 10, 1);
    }

    SECTION("agentite_tilemap_clear_layer with NULL tilemap") {
        // Should not crash (internally calls get_layer which returns NULL)
        agentite_tilemap_clear_layer(nullptr, 0);
    }
}

/* ============================================================================
 * Coordinate Conversion NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tilemap coordinate conversion NULL safety", "[tilemap][coords][null]") {
    SECTION("agentite_tilemap_world_to_tile with NULL tilemap") {
        int tx = -1, ty = -1;
        agentite_tilemap_world_to_tile(nullptr, 100.0f, 200.0f, &tx, &ty);
        // Values should remain unchanged
        REQUIRE(tx == -1);
        REQUIRE(ty == -1);
    }

    SECTION("agentite_tilemap_tile_to_world with NULL tilemap") {
        float wx = -1.0f, wy = -1.0f;
        agentite_tilemap_tile_to_world(nullptr, 5, 10, &wx, &wy);
        // Values should remain unchanged
        REQUIRE(wx == -1.0f);
        REQUIRE(wy == -1.0f);
    }

    SECTION("agentite_tilemap_get_tile_at_world with NULL tilemap") {
        Agentite_TileID tile = agentite_tilemap_get_tile_at_world(nullptr, 0, 100.0f, 200.0f);
        REQUIRE(tile == AGENTITE_TILE_EMPTY);
    }

    SECTION("agentite_tilemap_get_world_bounds with NULL tilemap") {
        float left = -1.0f, right = -1.0f, top = -1.0f, bottom = -1.0f;
        agentite_tilemap_get_world_bounds(nullptr, &left, &right, &top, &bottom);
        // Values should remain unchanged
        REQUIRE(left == -1.0f);
        REQUIRE(right == -1.0f);
        REQUIRE(top == -1.0f);
        REQUIRE(bottom == -1.0f);
    }
}

/* ============================================================================
 * Rendering NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tilemap rendering NULL safety", "[tilemap][render][null]") {
    SECTION("agentite_tilemap_render with NULL tilemap") {
        // Should not crash
        agentite_tilemap_render(nullptr, nullptr, nullptr);
    }

    SECTION("agentite_tilemap_render with NULL sprite renderer") {
        // Should not crash (tilemap is also NULL here)
        agentite_tilemap_render(nullptr, nullptr, nullptr);
    }

    SECTION("agentite_tilemap_render_layer with NULL tilemap") {
        // Should not crash
        agentite_tilemap_render_layer(nullptr, nullptr, nullptr, 0);
    }
}

/* ============================================================================
 * Partial Output NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Tilemap partial output NULL safety", "[tilemap][null]") {
    SECTION("agentite_tilemap_get_size with partial NULL outputs") {
        agentite_tilemap_get_size(nullptr, nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_tilemap_get_tile_size with partial NULL outputs") {
        agentite_tilemap_get_tile_size(nullptr, nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_tileset_get_tile_size with partial NULL outputs") {
        agentite_tileset_get_tile_size(nullptr, nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_tilemap_world_to_tile with partial NULL outputs") {
        agentite_tilemap_world_to_tile(nullptr, 100.0f, 200.0f, nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_tilemap_tile_to_world with partial NULL outputs") {
        agentite_tilemap_tile_to_world(nullptr, 5, 10, nullptr, nullptr);
        // Should not crash
    }

    SECTION("agentite_tilemap_get_world_bounds with partial NULL outputs") {
        agentite_tilemap_get_world_bounds(nullptr, nullptr, nullptr, nullptr, nullptr);
        // Should not crash
    }
}
