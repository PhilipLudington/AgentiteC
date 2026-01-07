/*
 * Agentite Sprite Tests
 *
 * Tests for sprite struct operations that can be tested without GPU.
 * Note: Functions requiring Agentite_SpriteRenderer (GPU-dependent) cannot
 * be tested without actual GPU/window initialization.
 */

#include "catch_amalgamated.hpp"
#include "agentite/sprite.h"
#include <cstring>

/* ============================================================================
 * Sprite Struct Tests
 * ============================================================================ */

TEST_CASE("Sprite struct layout", "[sprite][struct]") {
    SECTION("Zero-initialized sprite has expected values") {
        Agentite_Sprite sprite = {};

        REQUIRE(sprite.texture == nullptr);
        REQUIRE(sprite.src_x == 0.0f);
        REQUIRE(sprite.src_y == 0.0f);
        REQUIRE(sprite.src_w == 0.0f);
        REQUIRE(sprite.src_h == 0.0f);
        REQUIRE(sprite.origin_x == 0.0f);
        REQUIRE(sprite.origin_y == 0.0f);
    }

    SECTION("Sprite struct size is reasonable") {
        // Sprite should be relatively small (pointer + 6 floats)
        REQUIRE(sizeof(Agentite_Sprite) >= sizeof(void*) + 6 * sizeof(float));
        REQUIRE(sizeof(Agentite_Sprite) <= 64);  // Reasonable upper bound
    }
}

TEST_CASE("Sprite vertex struct layout", "[sprite][vertex]") {
    SECTION("Zero-initialized vertex") {
        Agentite_SpriteVertex vertex = {};

        REQUIRE(vertex.pos[0] == 0.0f);
        REQUIRE(vertex.pos[1] == 0.0f);
        REQUIRE(vertex.uv[0] == 0.0f);
        REQUIRE(vertex.uv[1] == 0.0f);
        REQUIRE(vertex.color[0] == 0.0f);
        REQUIRE(vertex.color[1] == 0.0f);
        REQUIRE(vertex.color[2] == 0.0f);
        REQUIRE(vertex.color[3] == 0.0f);
    }

    SECTION("Vertex field assignment") {
        Agentite_SpriteVertex vertex = {};
        vertex.pos[0] = 100.0f;
        vertex.pos[1] = 200.0f;
        vertex.uv[0] = 0.5f;
        vertex.uv[1] = 0.75f;
        vertex.color[0] = 1.0f;
        vertex.color[1] = 0.5f;
        vertex.color[2] = 0.25f;
        vertex.color[3] = 1.0f;

        REQUIRE(vertex.pos[0] == 100.0f);
        REQUIRE(vertex.pos[1] == 200.0f);
        REQUIRE(vertex.uv[0] == 0.5f);
        REQUIRE(vertex.uv[1] == 0.75f);
        REQUIRE(vertex.color[0] == 1.0f);
        REQUIRE(vertex.color[1] == 0.5f);
        REQUIRE(vertex.color[2] == 0.25f);
        REQUIRE(vertex.color[3] == 1.0f);
    }

    SECTION("Vertex struct is POD-like") {
        // Should be copyable via memcpy
        Agentite_SpriteVertex v1 = {};
        v1.pos[0] = 10.0f;
        v1.pos[1] = 20.0f;
        v1.color[3] = 1.0f;

        Agentite_SpriteVertex v2;
        std::memcpy(&v2, &v1, sizeof(Agentite_SpriteVertex));

        REQUIRE(v2.pos[0] == 10.0f);
        REQUIRE(v2.pos[1] == 20.0f);
        REQUIRE(v2.color[3] == 1.0f);
    }
}

/* ============================================================================
 * Sprite Creation Tests (with NULL texture)
 * ============================================================================ */

TEST_CASE("Sprite creation with NULL texture", "[sprite][create]") {
    SECTION("agentite_sprite_from_texture with NULL") {
        Agentite_Sprite sprite = agentite_sprite_from_texture(nullptr);

        // Should return zeroed sprite
        REQUIRE(sprite.texture == nullptr);
        REQUIRE(sprite.src_x == 0.0f);
        REQUIRE(sprite.src_y == 0.0f);
        REQUIRE(sprite.src_w == 0.0f);
        REQUIRE(sprite.src_h == 0.0f);
        // Origin should be at default 0 (not 0.5) since texture is NULL
        REQUIRE(sprite.origin_x == 0.0f);
        REQUIRE(sprite.origin_y == 0.0f);
    }

    SECTION("agentite_sprite_create with NULL texture") {
        Agentite_Sprite sprite = agentite_sprite_create(nullptr, 10.0f, 20.0f, 64.0f, 64.0f);

        // Should return zeroed sprite when texture is NULL
        REQUIRE(sprite.texture == nullptr);
        // Source rectangle values are not set when texture is NULL
        REQUIRE(sprite.src_x == 0.0f);
        REQUIRE(sprite.src_y == 0.0f);
        REQUIRE(sprite.src_w == 0.0f);
        REQUIRE(sprite.src_h == 0.0f);
    }
}

/* ============================================================================
 * Sprite Origin Tests
 * ============================================================================ */

TEST_CASE("Sprite origin operations", "[sprite][origin]") {
    SECTION("Set origin on sprite struct") {
        Agentite_Sprite sprite = {};
        sprite.texture = nullptr;  // No texture needed for origin test
        sprite.src_w = 64.0f;
        sprite.src_h = 64.0f;

        agentite_sprite_set_origin(&sprite, 0.5f, 0.5f);
        REQUIRE(sprite.origin_x == 0.5f);
        REQUIRE(sprite.origin_y == 0.5f);
    }

    SECTION("Set origin to top-left (0, 0)") {
        Agentite_Sprite sprite = {};
        agentite_sprite_set_origin(&sprite, 0.0f, 0.0f);
        REQUIRE(sprite.origin_x == 0.0f);
        REQUIRE(sprite.origin_y == 0.0f);
    }

    SECTION("Set origin to bottom-right (1, 1)") {
        Agentite_Sprite sprite = {};
        agentite_sprite_set_origin(&sprite, 1.0f, 1.0f);
        REQUIRE(sprite.origin_x == 1.0f);
        REQUIRE(sprite.origin_y == 1.0f);
    }

    SECTION("Set origin outside 0-1 range (allowed)") {
        Agentite_Sprite sprite = {};
        agentite_sprite_set_origin(&sprite, -0.5f, 1.5f);
        // Values outside 0-1 should be accepted (for custom pivot points)
        REQUIRE(sprite.origin_x == -0.5f);
        REQUIRE(sprite.origin_y == 1.5f);
    }

    SECTION("Set origin with NULL sprite is safe") {
        // Should not crash
        agentite_sprite_set_origin(nullptr, 0.5f, 0.5f);
    }
}

/* ============================================================================
 * Scale Mode Enum Tests
 * ============================================================================ */

TEST_CASE("Scale mode enum values", "[sprite][enum]") {
    SECTION("Scale modes are distinct") {
        REQUIRE(AGENTITE_SCALEMODE_NEAREST != AGENTITE_SCALEMODE_LINEAR);
        REQUIRE(AGENTITE_SCALEMODE_NEAREST != AGENTITE_SCALEMODE_PIXELART);
        REQUIRE(AGENTITE_SCALEMODE_LINEAR != AGENTITE_SCALEMODE_PIXELART);
    }

    SECTION("Default scale mode value") {
        // NEAREST should be 0 (first enum value, common default for pixel art)
        REQUIRE(static_cast<int>(AGENTITE_SCALEMODE_NEAREST) == 0);
    }
}

TEST_CASE("Address mode enum values", "[sprite][enum]") {
    SECTION("Address modes are distinct") {
        REQUIRE(AGENTITE_ADDRESSMODE_CLAMP != AGENTITE_ADDRESSMODE_REPEAT);
        REQUIRE(AGENTITE_ADDRESSMODE_CLAMP != AGENTITE_ADDRESSMODE_MIRROR);
        REQUIRE(AGENTITE_ADDRESSMODE_REPEAT != AGENTITE_ADDRESSMODE_MIRROR);
    }

    SECTION("Default address mode value") {
        // CLAMP should be 0 (first enum value, common default)
        REQUIRE(static_cast<int>(AGENTITE_ADDRESSMODE_CLAMP) == 0);
    }
}

/* ============================================================================
 * Texture NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Texture NULL safety for getters", "[sprite][texture][null]") {
    SECTION("agentite_texture_get_size with NULL texture") {
        int width = -1, height = -1;
        agentite_texture_get_size(nullptr, &width, &height);
        // Should handle NULL gracefully (values depend on implementation)
        // At minimum, should not crash
    }

    SECTION("agentite_texture_get_scale_mode with NULL texture") {
        // Should return a safe default or handle NULL
        Agentite_ScaleMode mode = agentite_texture_get_scale_mode(nullptr);
        // Verify it doesn't crash and returns something (likely NEAREST or 0)
        REQUIRE(static_cast<int>(mode) >= 0);
        REQUIRE(static_cast<int>(mode) <= 2);
    }

    SECTION("agentite_texture_get_address_mode with NULL texture") {
        Agentite_TextureAddressMode mode = agentite_texture_get_address_mode(nullptr);
        // Should return a safe default
        REQUIRE(static_cast<int>(mode) >= 0);
        REQUIRE(static_cast<int>(mode) <= 2);
    }

    SECTION("agentite_texture_set_scale_mode with NULL texture") {
        // Should not crash
        agentite_texture_set_scale_mode(nullptr, AGENTITE_SCALEMODE_LINEAR);
    }

    SECTION("agentite_texture_set_address_mode with NULL texture") {
        // Should not crash
        agentite_texture_set_address_mode(nullptr, AGENTITE_ADDRESSMODE_REPEAT);
    }
}

/* ============================================================================
 * Renderer NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Sprite renderer NULL safety", "[sprite][renderer][null]") {
    SECTION("agentite_sprite_shutdown with NULL") {
        // Should not crash
        agentite_sprite_shutdown(nullptr);
    }

    SECTION("agentite_sprite_set_screen_size with NULL") {
        // Should not crash
        agentite_sprite_set_screen_size(nullptr, 1920, 1080);
    }

    SECTION("agentite_sprite_set_camera with NULL renderer") {
        // Should not crash
        agentite_sprite_set_camera(nullptr, nullptr);
    }

    SECTION("agentite_sprite_get_camera with NULL renderer") {
        Agentite_Camera *cam = agentite_sprite_get_camera(nullptr);
        REQUIRE(cam == nullptr);
    }

    SECTION("agentite_sprite_has_vignette with NULL") {
        bool has = agentite_sprite_has_vignette(nullptr);
        REQUIRE_FALSE(has);
    }
}

/* ============================================================================
 * Sprite Batch NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Sprite batch NULL safety", "[sprite][batch][null]") {
    SECTION("agentite_sprite_begin with NULL renderer") {
        // Should not crash
        agentite_sprite_begin(nullptr, nullptr);
    }

    SECTION("agentite_sprite_draw with NULL renderer") {
        Agentite_Sprite sprite = {};
        // Should not crash
        agentite_sprite_draw(nullptr, &sprite, 100.0f, 200.0f);
    }

    SECTION("agentite_sprite_draw with NULL sprite") {
        // Should not crash (renderer is also NULL, but sprite NULL check should come first)
        agentite_sprite_draw(nullptr, nullptr, 100.0f, 200.0f);
    }

    SECTION("agentite_sprite_draw_scaled with NULL renderer") {
        Agentite_Sprite sprite = {};
        // Should not crash
        agentite_sprite_draw_scaled(nullptr, &sprite, 100.0f, 200.0f, 2.0f, 2.0f);
    }

    SECTION("agentite_sprite_draw_ex with NULL renderer") {
        Agentite_Sprite sprite = {};
        // Should not crash
        agentite_sprite_draw_ex(nullptr, &sprite, 100.0f, 200.0f, 1.0f, 1.0f, 45.0f, 0.5f, 0.5f);
    }

    SECTION("agentite_sprite_draw_tinted with NULL renderer") {
        Agentite_Sprite sprite = {};
        // Should not crash
        agentite_sprite_draw_tinted(nullptr, &sprite, 100.0f, 200.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    }

    SECTION("agentite_sprite_draw_full with NULL renderer") {
        Agentite_Sprite sprite = {};
        // Should not crash
        agentite_sprite_draw_full(nullptr, &sprite, 100.0f, 200.0f, 1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f);
    }

    SECTION("agentite_sprite_upload with NULL renderer") {
        // Should not crash
        agentite_sprite_upload(nullptr, nullptr);
    }

    SECTION("agentite_sprite_render with NULL renderer") {
        // Should not crash
        agentite_sprite_render(nullptr, nullptr, nullptr);
    }

    SECTION("agentite_sprite_flush with NULL renderer") {
        // Should not crash
        agentite_sprite_flush(nullptr, nullptr, nullptr);
    }
}

/* ============================================================================
 * Texture Loading NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Texture loading NULL safety", "[sprite][texture][null]") {
    SECTION("agentite_texture_load with NULL renderer") {
        Agentite_Texture *tex = agentite_texture_load(nullptr, "test.png");
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_texture_load with NULL path") {
        Agentite_Texture *tex = agentite_texture_load(nullptr, nullptr);
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_texture_load_memory with NULL renderer") {
        const char data[] = "fake data";
        Agentite_Texture *tex = agentite_texture_load_memory(nullptr, data, sizeof(data));
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_texture_load_memory with NULL data") {
        Agentite_Texture *tex = agentite_texture_load_memory(nullptr, nullptr, 100);
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_texture_create with NULL renderer") {
        const unsigned char pixels[16] = {0};
        Agentite_Texture *tex = agentite_texture_create(nullptr, 2, 2, pixels);
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_texture_create with NULL pixels") {
        Agentite_Texture *tex = agentite_texture_create(nullptr, 2, 2, nullptr);
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_texture_destroy with NULL texture") {
        // Should not crash
        agentite_texture_destroy(nullptr, nullptr);
    }

    SECTION("agentite_texture_reload with NULL renderer") {
        bool result = agentite_texture_reload(nullptr, nullptr, "test.png");
        REQUIRE_FALSE(result);
    }
}

/* ============================================================================
 * Render Target NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Render target NULL safety", "[sprite][rendertarget][null]") {
    SECTION("agentite_texture_create_render_target with NULL renderer") {
        Agentite_Texture *tex = agentite_texture_create_render_target(nullptr, 256, 256);
        REQUIRE(tex == nullptr);
    }

    SECTION("agentite_sprite_begin_render_to_texture with NULL renderer") {
        SDL_GPURenderPass *pass = agentite_sprite_begin_render_to_texture(
            nullptr, nullptr, nullptr, 0.0f, 0.0f, 0.0f, 1.0f);
        REQUIRE(pass == nullptr);
    }

    SECTION("agentite_sprite_render_to_texture with NULL renderer") {
        // Should not crash
        agentite_sprite_render_to_texture(nullptr, nullptr, nullptr);
    }

    SECTION("agentite_sprite_end_render_to_texture with NULL pass") {
        // Should not crash
        agentite_sprite_end_render_to_texture(nullptr);
    }
}

/* ============================================================================
 * Vignette NULL Safety Tests
 * ============================================================================ */

TEST_CASE("Vignette NULL safety", "[sprite][vignette][null]") {
    SECTION("agentite_sprite_render_vignette with NULL renderer") {
        // Should not crash
        agentite_sprite_render_vignette(nullptr, nullptr, nullptr, nullptr);
    }

    SECTION("agentite_sprite_prepare_fullscreen_quad with NULL renderer") {
        // Should not crash
        agentite_sprite_prepare_fullscreen_quad(nullptr);
    }

    SECTION("agentite_sprite_upload_fullscreen_quad with NULL renderer") {
        // Should not crash
        agentite_sprite_upload_fullscreen_quad(nullptr, nullptr);
    }
}
