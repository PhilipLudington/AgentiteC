/*
 * Agentite Engine - Asset Handle System Tests
 *
 * Tests for asset registry, handle management, and reference counting.
 */

#include "catch_amalgamated.hpp"
#include "agentite/asset.h"
#include <cstring>

/* ============================================================================
 * Test Helpers
 * ============================================================================ */

/* Counter for destructor calls */
static int g_destructor_calls = 0;
static void *g_last_destroyed_data = nullptr;
static Agentite_AssetType g_last_destroyed_type = AGENTITE_ASSET_UNKNOWN;

static void test_destructor(void *data, Agentite_AssetType type, void *userdata) {
    (void)userdata;
    g_destructor_calls++;
    g_last_destroyed_data = data;
    g_last_destroyed_type = type;
}

static void reset_destructor_state() {
    g_destructor_calls = 0;
    g_last_destroyed_data = nullptr;
    g_last_destroyed_type = AGENTITE_ASSET_UNKNOWN;
}

/* Dummy data pointers for testing */
static int g_dummy_texture = 1;
static int g_dummy_sound = 2;
static int g_dummy_music = 3;

/* ============================================================================
 * Registry Lifecycle Tests
 * ============================================================================ */

TEST_CASE("Asset registry lifecycle", "[asset][lifecycle]") {
    SECTION("Create and destroy registry") {
        Agentite_AssetRegistry *registry = agentite_asset_registry_create();
        REQUIRE(registry != nullptr);
        REQUIRE(agentite_asset_count(registry) == 0);

        agentite_asset_registry_destroy(registry);
    }

    SECTION("Destroy NULL registry is safe") {
        agentite_asset_registry_destroy(nullptr);
        // Should not crash
    }
}

/* ============================================================================
 * Handle Validity Tests
 * ============================================================================ */

TEST_CASE("Asset handle validity", "[asset][handle]") {
    SECTION("Invalid handle is not valid") {
        REQUIRE_FALSE(agentite_asset_is_valid(AGENTITE_INVALID_ASSET_HANDLE));
    }

    SECTION("Handle comparison") {
        Agentite_AssetHandle a = {123};
        Agentite_AssetHandle b = {123};
        Agentite_AssetHandle c = {456};

        REQUIRE(agentite_asset_handle_equals(a, b));
        REQUIRE_FALSE(agentite_asset_handle_equals(a, c));
        REQUIRE(agentite_asset_handle_equals(AGENTITE_INVALID_ASSET_HANDLE,
                                              AGENTITE_INVALID_ASSET_HANDLE));
    }
}

/* ============================================================================
 * Registration Tests
 * ============================================================================ */

TEST_CASE("Asset registration", "[asset][register]") {
    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);

    SECTION("Register single asset") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "textures/player.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);

        REQUIRE(agentite_asset_is_valid(h));
        REQUIRE(agentite_asset_is_live(registry, h));
        REQUIRE(agentite_asset_count(registry) == 1);
    }

    SECTION("Register multiple assets") {
        Agentite_AssetHandle h1 = agentite_asset_register(
            registry, "tex1.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        Agentite_AssetHandle h2 = agentite_asset_register(
            registry, "sound.wav", AGENTITE_ASSET_SOUND, &g_dummy_sound);
        Agentite_AssetHandle h3 = agentite_asset_register(
            registry, "music.ogg", AGENTITE_ASSET_MUSIC, &g_dummy_music);

        REQUIRE(agentite_asset_is_valid(h1));
        REQUIRE(agentite_asset_is_valid(h2));
        REQUIRE(agentite_asset_is_valid(h3));
        REQUIRE(agentite_asset_count(registry) == 3);

        // All handles should be distinct
        REQUIRE_FALSE(agentite_asset_handle_equals(h1, h2));
        REQUIRE_FALSE(agentite_asset_handle_equals(h2, h3));
        REQUIRE_FALSE(agentite_asset_handle_equals(h1, h3));
    }

    SECTION("Re-register same path returns existing handle with incremented refcount") {
        Agentite_AssetHandle h1 = agentite_asset_register(
            registry, "shared.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        REQUIRE(agentite_asset_get_refcount(registry, h1) == 1);

        Agentite_AssetHandle h2 = agentite_asset_register(
            registry, "shared.png", AGENTITE_ASSET_TEXTURE, nullptr);  // data ignored

        REQUIRE(agentite_asset_handle_equals(h1, h2));
        REQUIRE(agentite_asset_get_refcount(registry, h1) == 2);
        REQUIRE(agentite_asset_count(registry) == 1);  // Still just one asset
    }

    SECTION("Register with NULL registry fails") {
        Agentite_AssetHandle h = agentite_asset_register(
            nullptr, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        REQUIRE_FALSE(agentite_asset_is_valid(h));
    }

    SECTION("Register with NULL path fails") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, nullptr, AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        REQUIRE_FALSE(agentite_asset_is_valid(h));
    }

    SECTION("Register with empty path fails") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        REQUIRE_FALSE(agentite_asset_is_valid(h));
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Lookup Tests
 * ============================================================================ */

TEST_CASE("Asset lookup", "[asset][lookup]") {
    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);

    Agentite_AssetHandle h1 = agentite_asset_register(
        registry, "player.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
    Agentite_AssetHandle h2 = agentite_asset_register(
        registry, "enemy.png", AGENTITE_ASSET_TEXTURE, &g_dummy_sound);

    SECTION("Lookup existing asset by path") {
        Agentite_AssetHandle found = agentite_asset_lookup(registry, "player.png");
        REQUIRE(agentite_asset_is_valid(found));
        REQUIRE(agentite_asset_handle_equals(found, h1));
    }

    SECTION("Lookup non-existent asset returns invalid handle") {
        Agentite_AssetHandle found = agentite_asset_lookup(registry, "missing.png");
        REQUIRE_FALSE(agentite_asset_is_valid(found));
    }

    SECTION("Lookup with NULL path returns invalid handle") {
        Agentite_AssetHandle found = agentite_asset_lookup(registry, nullptr);
        REQUIRE_FALSE(agentite_asset_is_valid(found));
    }

    SECTION("Lookup with NULL registry returns invalid handle") {
        Agentite_AssetHandle found = agentite_asset_lookup(nullptr, "player.png");
        REQUIRE_FALSE(agentite_asset_is_valid(found));
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Data Access Tests
 * ============================================================================ */

TEST_CASE("Asset data access", "[asset][data]") {
    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);

    Agentite_AssetHandle h = agentite_asset_register(
        registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);

    SECTION("Get data pointer") {
        void *data = agentite_asset_get_data(registry, h);
        REQUIRE(data == &g_dummy_texture);
    }

    SECTION("Get asset type") {
        Agentite_AssetType type = agentite_asset_get_type(registry, h);
        REQUIRE(type == AGENTITE_ASSET_TEXTURE);
    }

    SECTION("Get asset path") {
        const char *path = agentite_asset_get_path(registry, h);
        REQUIRE(path != nullptr);
        REQUIRE(strcmp(path, "test.png") == 0);
    }

    SECTION("Invalid handle returns NULL/unknown") {
        REQUIRE(agentite_asset_get_data(registry, AGENTITE_INVALID_ASSET_HANDLE) == nullptr);
        REQUIRE(agentite_asset_get_type(registry, AGENTITE_INVALID_ASSET_HANDLE) == AGENTITE_ASSET_UNKNOWN);
        REQUIRE(agentite_asset_get_path(registry, AGENTITE_INVALID_ASSET_HANDLE) == nullptr);
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Reference Counting Tests
 * ============================================================================ */

TEST_CASE("Asset reference counting", "[asset][refcount]") {
    reset_destructor_state();

    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);
    agentite_asset_set_destructor(registry, test_destructor, nullptr);

    SECTION("Initial refcount is 1") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        REQUIRE(agentite_asset_get_refcount(registry, h) == 1);
    }

    SECTION("AddRef increments refcount") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);

        REQUIRE(agentite_asset_addref(registry, h));
        REQUIRE(agentite_asset_get_refcount(registry, h) == 2);

        REQUIRE(agentite_asset_addref(registry, h));
        REQUIRE(agentite_asset_get_refcount(registry, h) == 3);
    }

    SECTION("Release decrements refcount") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_addref(registry, h);  // refcount = 2

        REQUIRE(agentite_asset_release(registry, h));
        REQUIRE(agentite_asset_get_refcount(registry, h) == 1);
        REQUIRE(agentite_asset_is_live(registry, h));
    }

    SECTION("Release to zero destroys asset") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);

        REQUIRE(agentite_asset_release(registry, h));

        // Handle should now be stale
        REQUIRE_FALSE(agentite_asset_is_live(registry, h));
        REQUIRE(agentite_asset_count(registry) == 0);

        // Destructor should have been called
        REQUIRE(g_destructor_calls == 1);
        REQUIRE(g_last_destroyed_data == &g_dummy_texture);
        REQUIRE(g_last_destroyed_type == AGENTITE_ASSET_TEXTURE);
    }

    SECTION("AddRef/Release with invalid handle returns false") {
        REQUIRE_FALSE(agentite_asset_addref(registry, AGENTITE_INVALID_ASSET_HANDLE));
        REQUIRE_FALSE(agentite_asset_release(registry, AGENTITE_INVALID_ASSET_HANDLE));
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Stale Handle Detection Tests
 * ============================================================================ */

TEST_CASE("Stale handle detection", "[asset][stale]") {
    reset_destructor_state();

    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);
    agentite_asset_set_destructor(registry, test_destructor, nullptr);

    SECTION("Handle becomes stale after release") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        REQUIRE(agentite_asset_is_live(registry, h));

        agentite_asset_release(registry, h);

        // Handle is now stale
        REQUIRE_FALSE(agentite_asset_is_live(registry, h));
        REQUIRE(agentite_asset_get_data(registry, h) == nullptr);
    }

    SECTION("New asset at same slot has different handle") {
        Agentite_AssetHandle h1 = agentite_asset_register(
            registry, "first.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_release(registry, h1);

        // Register new asset - may reuse same slot
        Agentite_AssetHandle h2 = agentite_asset_register(
            registry, "second.png", AGENTITE_ASSET_TEXTURE, &g_dummy_sound);

        // Old handle should still be invalid even if slot was reused
        REQUIRE_FALSE(agentite_asset_is_live(registry, h1));
        REQUIRE(agentite_asset_is_live(registry, h2));
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Iteration Tests
 * ============================================================================ */

TEST_CASE("Asset iteration", "[asset][iteration]") {
    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);

    SECTION("Empty registry returns zero count") {
        REQUIRE(agentite_asset_count(registry) == 0);

        Agentite_AssetHandle handles[10];
        size_t count = agentite_asset_get_all(registry, handles, 10);
        REQUIRE(count == 0);
    }

    SECTION("Get all handles") {
        agentite_asset_register(registry, "a.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_register(registry, "b.wav", AGENTITE_ASSET_SOUND, &g_dummy_sound);
        agentite_asset_register(registry, "c.ogg", AGENTITE_ASSET_MUSIC, &g_dummy_music);

        REQUIRE(agentite_asset_count(registry) == 3);

        Agentite_AssetHandle handles[10];
        size_t count = agentite_asset_get_all(registry, handles, 10);
        REQUIRE(count == 3);

        // All handles should be valid
        for (size_t i = 0; i < count; i++) {
            REQUIRE(agentite_asset_is_live(registry, handles[i]));
        }
    }

    SECTION("Limited output array") {
        agentite_asset_register(registry, "a.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_register(registry, "b.wav", AGENTITE_ASSET_SOUND, &g_dummy_sound);
        agentite_asset_register(registry, "c.ogg", AGENTITE_ASSET_MUSIC, &g_dummy_music);

        Agentite_AssetHandle handles[2];
        size_t count = agentite_asset_get_all(registry, handles, 2);
        REQUIRE(count == 2);  // Limited by max_count
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Serialization Helper Tests
 * ============================================================================ */

TEST_CASE("Asset type names", "[asset][serialization]") {
    SECTION("Type to name") {
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_TEXTURE), "texture") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_SOUND), "sound") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_MUSIC), "music") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_FONT), "font") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_PREFAB), "prefab") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_SCENE), "scene") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_DATA), "data") == 0);
        REQUIRE(strcmp(agentite_asset_type_name(AGENTITE_ASSET_UNKNOWN), "unknown") == 0);
    }

    SECTION("Name to type") {
        REQUIRE(agentite_asset_type_from_name("texture") == AGENTITE_ASSET_TEXTURE);
        REQUIRE(agentite_asset_type_from_name("TEXTURE") == AGENTITE_ASSET_TEXTURE);
        REQUIRE(agentite_asset_type_from_name("Texture") == AGENTITE_ASSET_TEXTURE);
        REQUIRE(agentite_asset_type_from_name("sound") == AGENTITE_ASSET_SOUND);
        REQUIRE(agentite_asset_type_from_name("music") == AGENTITE_ASSET_MUSIC);
        REQUIRE(agentite_asset_type_from_name("font") == AGENTITE_ASSET_FONT);
        REQUIRE(agentite_asset_type_from_name("prefab") == AGENTITE_ASSET_PREFAB);
        REQUIRE(agentite_asset_type_from_name("scene") == AGENTITE_ASSET_SCENE);
        REQUIRE(agentite_asset_type_from_name("data") == AGENTITE_ASSET_DATA);
    }

    SECTION("Invalid name returns unknown") {
        REQUIRE(agentite_asset_type_from_name("invalid") == AGENTITE_ASSET_UNKNOWN);
        REQUIRE(agentite_asset_type_from_name("") == AGENTITE_ASSET_UNKNOWN);
        REQUIRE(agentite_asset_type_from_name(nullptr) == AGENTITE_ASSET_UNKNOWN);
    }
}

/* ============================================================================
 * Destructor Callback Tests
 * ============================================================================ */

TEST_CASE("Asset destructor callbacks", "[asset][destructor]") {
    reset_destructor_state();

    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);

    SECTION("Destructor called on release to zero") {
        agentite_asset_set_destructor(registry, test_destructor, nullptr);

        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_release(registry, h);

        REQUIRE(g_destructor_calls == 1);
        REQUIRE(g_last_destroyed_data == &g_dummy_texture);
    }

    SECTION("Destructor called on registry destroy") {
        agentite_asset_set_destructor(registry, test_destructor, nullptr);

        agentite_asset_register(registry, "a.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_register(registry, "b.wav", AGENTITE_ASSET_SOUND, &g_dummy_sound);

        agentite_asset_registry_destroy(registry);
        registry = nullptr;  // Prevent double-destroy

        REQUIRE(g_destructor_calls == 2);
    }

    SECTION("No destructor means no callback") {
        // Don't set destructor
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_release(registry, h);

        REQUIRE(g_destructor_calls == 0);
    }

    if (registry) {
        agentite_asset_registry_destroy(registry);
    }
}

/* ============================================================================
 * Unregister Tests
 * ============================================================================ */

TEST_CASE("Asset unregister", "[asset][unregister]") {
    reset_destructor_state();

    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);
    agentite_asset_set_destructor(registry, test_destructor, nullptr);

    SECTION("Unregister decrements refcount") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        agentite_asset_addref(registry, h);  // refcount = 2

        agentite_asset_unregister(registry, h);
        REQUIRE(agentite_asset_get_refcount(registry, h) == 1);
        REQUIRE(agentite_asset_is_live(registry, h));
    }

    SECTION("Unregister to zero destroys asset") {
        Agentite_AssetHandle h = agentite_asset_register(
            registry, "test.png", AGENTITE_ASSET_TEXTURE, &g_dummy_texture);

        agentite_asset_unregister(registry, h);

        REQUIRE_FALSE(agentite_asset_is_live(registry, h));
        REQUIRE(g_destructor_calls == 1);
    }

    SECTION("Unregister with invalid handle is safe") {
        agentite_asset_unregister(registry, AGENTITE_INVALID_ASSET_HANDLE);
        // Should not crash
    }

    agentite_asset_registry_destroy(registry);
}

/* ============================================================================
 * Hash Collision / Stress Tests
 * ============================================================================ */

TEST_CASE("Asset registry stress", "[asset][stress]") {
    Agentite_AssetRegistry *registry = agentite_asset_registry_create();
    REQUIRE(registry != nullptr);

    SECTION("Many assets with similar paths") {
        // Register many assets to trigger hash table growth
        char path[64];
        for (int i = 0; i < 200; i++) {
            snprintf(path, sizeof(path), "asset_%03d.png", i);
            Agentite_AssetHandle h = agentite_asset_register(
                registry, path, AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
            REQUIRE(agentite_asset_is_valid(h));
        }

        REQUIRE(agentite_asset_count(registry) == 200);

        // Verify all can be looked up
        for (int i = 0; i < 200; i++) {
            snprintf(path, sizeof(path), "asset_%03d.png", i);
            Agentite_AssetHandle h = agentite_asset_lookup(registry, path);
            REQUIRE(agentite_asset_is_live(registry, h));
        }
    }

    SECTION("Register and release many assets") {
        char path[64];
        Agentite_AssetHandle handles[100];

        // Register 100 assets
        for (int i = 0; i < 100; i++) {
            snprintf(path, sizeof(path), "temp_%d.png", i);
            handles[i] = agentite_asset_register(
                registry, path, AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
        }
        REQUIRE(agentite_asset_count(registry) == 100);

        // Release all
        for (int i = 0; i < 100; i++) {
            agentite_asset_release(registry, handles[i]);
        }
        REQUIRE(agentite_asset_count(registry) == 0);

        // Re-register - slots should be reused
        for (int i = 0; i < 100; i++) {
            snprintf(path, sizeof(path), "new_%d.png", i);
            Agentite_AssetHandle h = agentite_asset_register(
                registry, path, AGENTITE_ASSET_TEXTURE, &g_dummy_texture);
            REQUIRE(agentite_asset_is_valid(h));
        }
        REQUIRE(agentite_asset_count(registry) == 100);
    }

    agentite_asset_registry_destroy(registry);
}
