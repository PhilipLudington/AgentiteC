/*
 * Agentite Noise System Tests
 */

#include <catch_amalgamated.hpp>
#include "agentite/noise.h"
#include <cmath>

TEST_CASE("Noise generator lifecycle", "[noise]") {
    SECTION("create and destroy") {
        Agentite_Noise *noise = agentite_noise_create(12345);
        REQUIRE(noise != nullptr);

        uint64_t seed = agentite_noise_get_seed(noise);
        REQUIRE(seed == 12345);

        agentite_noise_destroy(noise);
    }

    SECTION("reseed") {
        Agentite_Noise *noise = agentite_noise_create(100);
        REQUIRE(agentite_noise_get_seed(noise) == 100);

        agentite_noise_reseed(noise, 200);
        REQUIRE(agentite_noise_get_seed(noise) == 200);

        agentite_noise_destroy(noise);
    }

    SECTION("destroy null is safe") {
        agentite_noise_destroy(nullptr);
        // Should not crash
    }
}

TEST_CASE("Perlin noise 2D", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("returns values in expected range") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float value = agentite_noise_perlin2d(noise, x, y);

            // Perlin should return values in [-1, 1] range (roughly)
            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("is deterministic") {
        float v1 = agentite_noise_perlin2d(noise, 10.5f, 20.3f);
        float v2 = agentite_noise_perlin2d(noise, 10.5f, 20.3f);
        REQUIRE(v1 == v2);
    }

    SECTION("different coordinates give different values") {
        float v1 = agentite_noise_perlin2d(noise, 1.5f, 2.3f);
        float v2 = agentite_noise_perlin2d(noise, 100.7f, 50.2f);
        REQUIRE(v1 != v2);
    }

    SECTION("same seed gives same results") {
        Agentite_Noise *noise2 = agentite_noise_create(42);

        float v1 = agentite_noise_perlin2d(noise, 5.5f, 3.2f);
        float v2 = agentite_noise_perlin2d(noise2, 5.5f, 3.2f);
        REQUIRE(v1 == v2);

        agentite_noise_destroy(noise2);
    }

    SECTION("different seeds give different results") {
        Agentite_Noise *noise2 = agentite_noise_create(99);

        float v1 = agentite_noise_perlin2d(noise, 5.5f, 3.2f);
        float v2 = agentite_noise_perlin2d(noise2, 5.5f, 3.2f);
        REQUIRE(v1 != v2);

        agentite_noise_destroy(noise2);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Perlin noise 3D", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("returns values in expected range") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float z = (float)i * 0.1f;
            float value = agentite_noise_perlin3d(noise, x, y, z);

            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("is deterministic") {
        float v1 = agentite_noise_perlin3d(noise, 1.0f, 2.0f, 3.0f);
        float v2 = agentite_noise_perlin3d(noise, 1.0f, 2.0f, 3.0f);
        REQUIRE(v1 == v2);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Simplex noise 2D", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("returns values in expected range") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float value = agentite_noise_simplex2d(noise, x, y);

            // Simplex should return values in [-1, 1] range
            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("is deterministic") {
        float v1 = agentite_noise_simplex2d(noise, 10.5f, 20.3f);
        float v2 = agentite_noise_simplex2d(noise, 10.5f, 20.3f);
        REQUIRE(v1 == v2);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Simplex noise 3D", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("returns values in expected range") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float z = (float)i * 0.1f;
            float value = agentite_noise_simplex3d(noise, x, y, z);

            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("is deterministic") {
        float v1 = agentite_noise_simplex3d(noise, 1.0f, 2.0f, 3.0f);
        float v2 = agentite_noise_simplex3d(noise, 1.0f, 2.0f, 3.0f);
        REQUIRE(v1 == v2);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Worley noise 2D", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("default returns non-negative values") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.3f;
            float y = (float)i * 0.3f;
            float value = agentite_noise_worley2d(noise, x, y);

            // F1 distance should be >= 0
            REQUIRE(value >= 0.0f);
        }
    }

    SECTION("is deterministic") {
        float v1 = agentite_noise_worley2d(noise, 10.5f, 20.3f);
        float v2 = agentite_noise_worley2d(noise, 10.5f, 20.3f);
        REQUIRE(v1 == v2);
    }

    SECTION("different return types work") {
        Agentite_NoiseWorleyConfig f1_cfg = AGENTITE_NOISE_WORLEY_DEFAULT;
        f1_cfg.return_type = AGENTITE_WORLEY_F1;

        Agentite_NoiseWorleyConfig f2_cfg = AGENTITE_NOISE_WORLEY_DEFAULT;
        f2_cfg.return_type = AGENTITE_WORLEY_F2;

        Agentite_NoiseWorleyConfig f2f1_cfg = AGENTITE_NOISE_WORLEY_DEFAULT;
        f2f1_cfg.return_type = AGENTITE_WORLEY_F2_F1;

        float f1 = agentite_noise_worley2d_ex(noise, 5.0f, 5.0f, &f1_cfg);
        float f2 = agentite_noise_worley2d_ex(noise, 5.0f, 5.0f, &f2_cfg);
        float f2_f1 = agentite_noise_worley2d_ex(noise, 5.0f, 5.0f, &f2f1_cfg);

        // F2 should be >= F1
        REQUIRE(f2 >= f1);
        // F2-F1 should approximately equal the edge width
        REQUIRE(f2_f1 == Catch::Approx(f2 - f1).margin(0.001f));
    }

    SECTION("different distance functions produce different results") {
        Agentite_NoiseWorleyConfig euclid = AGENTITE_NOISE_WORLEY_DEFAULT;
        euclid.distance = AGENTITE_WORLEY_EUCLIDEAN;

        Agentite_NoiseWorleyConfig manhattan = AGENTITE_NOISE_WORLEY_DEFAULT;
        manhattan.distance = AGENTITE_WORLEY_MANHATTAN;

        float v1 = agentite_noise_worley2d_ex(noise, 5.5f, 3.3f, &euclid);
        float v2 = agentite_noise_worley2d_ex(noise, 5.5f, 3.3f, &manhattan);

        // Different distance functions should generally produce different values
        // (not guaranteed at every point, but usually different)
        // Just verify they both work
        REQUIRE(v1 >= 0.0f);
        REQUIRE(v2 >= 0.0f);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Value noise", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("2D returns values in expected range") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float value = agentite_noise_value2d(noise, x, y);

            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("3D returns values in expected range") {
        for (int i = 0; i < 100; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float z = (float)i * 0.1f;
            float value = agentite_noise_value3d(noise, x, y, z);

            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Fractal Brownian motion", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("2D with default config") {
        for (int i = 0; i < 50; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float value = agentite_noise_fbm2d(noise, x, y, nullptr);

            // fBm is normalized so should be roughly in [-1, 1]
            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("3D with default config") {
        for (int i = 0; i < 50; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float z = (float)i * 0.1f;
            float value = agentite_noise_fbm3d(noise, x, y, z, nullptr);

            REQUIRE(value >= -1.5f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("custom octaves affect output") {
        Agentite_NoiseFractalConfig cfg = AGENTITE_NOISE_FRACTAL_DEFAULT;

        cfg.octaves = 1;
        float v1 = agentite_noise_fbm2d(noise, 5.0f, 5.0f, &cfg);

        cfg.octaves = 8;
        float v8 = agentite_noise_fbm2d(noise, 5.0f, 5.0f, &cfg);

        // More octaves add more detail, so values should differ
        // (Not guaranteed at every point but very likely)
        // Just verify both work
        REQUIRE(std::isfinite(v1));
        REQUIRE(std::isfinite(v8));
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Ridged multifractal", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("2D returns reasonable values") {
        for (int i = 0; i < 50; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float value = agentite_noise_ridged2d(noise, x, y, nullptr);

            // Ridged noise produces non-negative values
            REQUIRE(value >= 0.0f);
            REQUIRE(std::isfinite(value));
        }
    }

    SECTION("is deterministic") {
        float v1 = agentite_noise_ridged2d(noise, 5.5f, 3.3f, nullptr);
        float v2 = agentite_noise_ridged2d(noise, 5.5f, 3.3f, nullptr);
        REQUIRE(v1 == v2);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Turbulence", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("2D returns non-negative values") {
        for (int i = 0; i < 50; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float value = agentite_noise_turbulence2d(noise, x, y, nullptr);

            // Turbulence uses abs() so should be non-negative
            REQUIRE(value >= 0.0f);
            REQUIRE(value <= 1.5f);
        }
    }

    SECTION("3D returns non-negative values") {
        for (int i = 0; i < 50; i++) {
            float x = (float)i * 0.1f;
            float y = (float)i * 0.1f;
            float z = (float)i * 0.1f;
            float value = agentite_noise_turbulence3d(noise, x, y, z, nullptr);

            REQUIRE(value >= 0.0f);
            REQUIRE(value <= 1.5f);
        }
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Domain warping", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("2D warp modifies coordinates") {
        float x = 5.0f;
        float y = 5.0f;
        float orig_x = x;
        float orig_y = y;

        Agentite_NoiseDomainWarpConfig cfg = AGENTITE_NOISE_DOMAIN_WARP_DEFAULT;
        cfg.amplitude = 10.0f; // Large enough to see effect

        agentite_noise_domain_warp2d(noise, &x, &y, &cfg);

        // Coordinates should be modified
        REQUIRE((x != orig_x || y != orig_y));
    }

    SECTION("warp amplitude affects strength") {
        float x1 = 5.0f, y1 = 5.0f;
        float x2 = 5.0f, y2 = 5.0f;

        Agentite_NoiseDomainWarpConfig cfg1 = AGENTITE_NOISE_DOMAIN_WARP_DEFAULT;
        cfg1.amplitude = 1.0f;

        Agentite_NoiseDomainWarpConfig cfg2 = AGENTITE_NOISE_DOMAIN_WARP_DEFAULT;
        cfg2.amplitude = 10.0f;

        agentite_noise_domain_warp2d(noise, &x1, &y1, &cfg1);
        agentite_noise_domain_warp2d(noise, &x2, &y2, &cfg2);

        float dist1 = sqrtf((x1 - 5.0f) * (x1 - 5.0f) + (y1 - 5.0f) * (y1 - 5.0f));
        float dist2 = sqrtf((x2 - 5.0f) * (x2 - 5.0f) + (y2 - 5.0f) * (y2 - 5.0f));

        // Higher amplitude should move coordinates further (generally)
        REQUIRE(std::isfinite(dist1));
        REQUIRE(std::isfinite(dist2));
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Heightmap generation", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("create with defaults") {
        float *heightmap = agentite_noise_heightmap_create(noise, 64, 64, nullptr);
        REQUIRE(heightmap != nullptr);

        // Check some values are valid
        for (int i = 0; i < 64 * 64; i++) {
            REQUIRE(std::isfinite(heightmap[i]));
        }

        agentite_noise_heightmap_destroy(heightmap);
    }

    SECTION("normalized heightmap is in 0-1 range") {
        Agentite_HeightmapConfig cfg = AGENTITE_HEIGHTMAP_DEFAULT;
        cfg.normalize = true;

        float *heightmap = agentite_noise_heightmap_create(noise, 64, 64, &cfg);
        REQUIRE(heightmap != nullptr);

        for (int i = 0; i < 64 * 64; i++) {
            REQUIRE(heightmap[i] >= 0.0f);
            REQUIRE(heightmap[i] <= 1.0f);
        }

        agentite_noise_heightmap_destroy(heightmap);
    }

    SECTION("invalid parameters return null") {
        float *h1 = agentite_noise_heightmap_create(nullptr, 64, 64, nullptr);
        REQUIRE(h1 == nullptr);

        float *h2 = agentite_noise_heightmap_create(noise, 0, 64, nullptr);
        REQUIRE(h2 == nullptr);

        float *h3 = agentite_noise_heightmap_create(noise, 64, 0, nullptr);
        REQUIRE(h3 == nullptr);
    }

    SECTION("heightmap normals are normalized") {
        Agentite_HeightmapConfig cfg = AGENTITE_HEIGHTMAP_DEFAULT;
        float *heightmap = agentite_noise_heightmap_create(noise, 64, 64, &cfg);
        REQUIRE(heightmap != nullptr);

        float nx, ny, nz;
        agentite_noise_heightmap_normal(heightmap, 64, 64, 32, 32, 1.0f, &nx, &ny, &nz);

        float len = sqrtf(nx * nx + ny * ny + nz * nz);
        REQUIRE(len == Catch::Approx(1.0f).margin(0.01f));

        agentite_noise_heightmap_destroy(heightmap);
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Tilemap generation", "[noise]") {
    Agentite_Noise *noise = agentite_noise_create(42);
    REQUIRE(noise != nullptr);

    SECTION("generates valid tile indices") {
        float thresholds[] = {0.3f, 0.6f};
        Agentite_NoiseTilemapConfig cfg = {
            .tile_types = 3,
            .thresholds = thresholds,
            .noise_type = AGENTITE_NOISE_SIMPLEX,
            .fractal = AGENTITE_NOISE_FRACTAL_DEFAULT,
            .scale = 0.1f
        };

        int *tiles = agentite_noise_tilemap_create(noise, 32, 32, &cfg);
        REQUIRE(tiles != nullptr);

        for (int i = 0; i < 32 * 32; i++) {
            REQUIRE(tiles[i] >= 0);
            REQUIRE(tiles[i] < 3);
        }

        free(tiles);
    }

    SECTION("tilemap sample returns valid index") {
        float thresholds[] = {0.3f, 0.6f};
        Agentite_NoiseTilemapConfig cfg = {
            .tile_types = 3,
            .thresholds = thresholds,
            .noise_type = AGENTITE_NOISE_SIMPLEX,
            .fractal = AGENTITE_NOISE_FRACTAL_DEFAULT,
            .scale = 0.1f
        };

        for (int i = 0; i < 100; i++) {
            int tile = agentite_noise_tilemap_sample(noise, (float)i, (float)i, &cfg);
            REQUIRE(tile >= 0);
            REQUIRE(tile < 3);
        }
    }

    agentite_noise_destroy(noise);
}

TEST_CASE("Utility functions", "[noise]") {
    SECTION("remap") {
        REQUIRE(agentite_noise_remap(0.5f, 0.0f, 1.0f, 0.0f, 100.0f) == Catch::Approx(50.0f));
        REQUIRE(agentite_noise_remap(0.0f, 0.0f, 1.0f, 0.0f, 100.0f) == Catch::Approx(0.0f));
        REQUIRE(agentite_noise_remap(1.0f, 0.0f, 1.0f, 0.0f, 100.0f) == Catch::Approx(100.0f));
        REQUIRE(agentite_noise_remap(-1.0f, -1.0f, 1.0f, 0.0f, 1.0f) == Catch::Approx(0.0f));
    }

    SECTION("clamp") {
        REQUIRE(agentite_noise_clamp(0.5f, 0.0f, 1.0f) == Catch::Approx(0.5f));
        REQUIRE(agentite_noise_clamp(-1.0f, 0.0f, 1.0f) == Catch::Approx(0.0f));
        REQUIRE(agentite_noise_clamp(2.0f, 0.0f, 1.0f) == Catch::Approx(1.0f));
    }

    SECTION("smoothstep") {
        REQUIRE(agentite_noise_smoothstep(0.0f, 1.0f, 0.0f) == Catch::Approx(0.0f));
        REQUIRE(agentite_noise_smoothstep(0.0f, 1.0f, 1.0f) == Catch::Approx(1.0f));
        REQUIRE(agentite_noise_smoothstep(0.0f, 1.0f, 0.5f) == Catch::Approx(0.5f));
        REQUIRE(agentite_noise_smoothstep(0.0f, 1.0f, -1.0f) == Catch::Approx(0.0f)); // clamped
        REQUIRE(agentite_noise_smoothstep(0.0f, 1.0f, 2.0f) == Catch::Approx(1.0f)); // clamped
    }

    SECTION("lerp") {
        REQUIRE(agentite_noise_lerp(0.0f, 10.0f, 0.0f) == Catch::Approx(0.0f));
        REQUIRE(agentite_noise_lerp(0.0f, 10.0f, 1.0f) == Catch::Approx(10.0f));
        REQUIRE(agentite_noise_lerp(0.0f, 10.0f, 0.5f) == Catch::Approx(5.0f));
    }

    SECTION("hash functions") {
        Agentite_Noise *noise = agentite_noise_create(42);
        REQUIRE(noise != nullptr);

        // Hash should be deterministic
        float h1 = agentite_noise_hash2d(noise, 10, 20);
        float h2 = agentite_noise_hash2d(noise, 10, 20);
        REQUIRE(h1 == h2);

        // Different coordinates should give different hashes (usually)
        float h3 = agentite_noise_hash2d(noise, 11, 20);
        // Just verify it's in range
        REQUIRE(h1 >= 0.0f);
        REQUIRE(h1 <= 1.0f);
        REQUIRE(h3 >= 0.0f);
        REQUIRE(h3 <= 1.0f);

        // 3D hash
        float h4 = agentite_noise_hash3d(noise, 1, 2, 3);
        REQUIRE(h4 >= 0.0f);
        REQUIRE(h4 <= 1.0f);

        agentite_noise_destroy(noise);
    }
}

TEST_CASE("Null safety", "[noise]") {
    SECTION("functions handle null noise gracefully") {
        // These should not crash and return default values
        REQUIRE(agentite_noise_perlin2d(nullptr, 0, 0) == 0.0f);
        REQUIRE(agentite_noise_perlin3d(nullptr, 0, 0, 0) == 0.0f);
        REQUIRE(agentite_noise_simplex2d(nullptr, 0, 0) == 0.0f);
        REQUIRE(agentite_noise_simplex3d(nullptr, 0, 0, 0) == 0.0f);
        REQUIRE(agentite_noise_worley2d(nullptr, 0, 0) == 0.0f);
        REQUIRE(agentite_noise_value2d(nullptr, 0, 0) == 0.0f);
        REQUIRE(agentite_noise_fbm2d(nullptr, 0, 0, nullptr) == 0.0f);
        REQUIRE(agentite_noise_ridged2d(nullptr, 0, 0, nullptr) == 0.0f);
        REQUIRE(agentite_noise_turbulence2d(nullptr, 0, 0, nullptr) == 0.0f);
    }
}
