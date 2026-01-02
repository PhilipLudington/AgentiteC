/*
 * Agentite Procedural Noise System
 *
 * Provides various noise generation algorithms for procedural content generation,
 * including terrain, biomes, textures, and resource distribution.
 *
 * Features:
 * - Perlin noise 2D/3D
 * - Simplex noise 2D/3D
 * - Worley (cellular) noise
 * - Fractal Brownian motion (fBm)
 * - Ridged multifractal noise
 * - Turbulence
 * - Domain warping
 * - Tilemap and heightmap generation utilities
 *
 * Usage:
 *   // Create a noise generator with a seed
 *   Agentite_Noise *noise = agentite_noise_create(12345);
 *
 *   // Sample 2D Perlin noise
 *   float value = agentite_noise_perlin2d(noise, x * 0.1f, y * 0.1f);
 *
 *   // Use fractal noise for terrain
 *   Agentite_NoiseFractalConfig fbm = AGENTITE_NOISE_FRACTAL_DEFAULT;
 *   fbm.octaves = 6;
 *   fbm.persistence = 0.5f;
 *   float terrain = agentite_noise_fbm2d(noise, x, y, &fbm);
 *
 *   // Generate a heightmap
 *   Agentite_HeightmapConfig hconfig = AGENTITE_HEIGHTMAP_DEFAULT;
 *   float *heightmap = agentite_noise_heightmap_create(noise, 256, 256, &hconfig);
 *   // ... use heightmap ...
 *   agentite_noise_heightmap_destroy(heightmap);
 *
 *   // Cleanup
 *   agentite_noise_destroy(noise);
 */

#ifndef AGENTITE_NOISE_H
#define AGENTITE_NOISE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct Agentite_Noise Agentite_Noise;

/* ============================================================================
 * Enumerations
 * ============================================================================ */

/** Noise algorithm type */
typedef enum Agentite_NoiseType {
    AGENTITE_NOISE_PERLIN,        /**< Classic Perlin noise */
    AGENTITE_NOISE_SIMPLEX,       /**< Simplex noise (faster, no grid artifacts) */
    AGENTITE_NOISE_WORLEY,        /**< Worley/cellular noise */
    AGENTITE_NOISE_VALUE          /**< Value noise (interpolated random values) */
} Agentite_NoiseType;

/** Worley noise distance function */
typedef enum Agentite_WorleyDistance {
    AGENTITE_WORLEY_EUCLIDEAN,    /**< Euclidean distance (circular cells) */
    AGENTITE_WORLEY_MANHATTAN,    /**< Manhattan distance (diamond cells) */
    AGENTITE_WORLEY_CHEBYSHEV     /**< Chebyshev distance (square cells) */
} Agentite_WorleyDistance;

/** Worley noise return value type */
typedef enum Agentite_WorleyReturn {
    AGENTITE_WORLEY_F1,           /**< Distance to nearest point */
    AGENTITE_WORLEY_F2,           /**< Distance to second nearest point */
    AGENTITE_WORLEY_F2_F1,        /**< F2 - F1 (cell edges) */
    AGENTITE_WORLEY_F1_F2         /**< F1 + F2 combined */
} Agentite_WorleyReturn;

/** Fractal noise combination method */
typedef enum Agentite_FractalType {
    AGENTITE_FRACTAL_FBM,         /**< Standard fBm (additive) */
    AGENTITE_FRACTAL_RIDGED,      /**< Ridged multifractal */
    AGENTITE_FRACTAL_BILLOW,      /**< Billow (abs of noise) */
    AGENTITE_FRACTAL_TURBULENCE   /**< Turbulence (abs fBm) */
} Agentite_FractalType;

/* ============================================================================
 * Configuration Structures
 * ============================================================================ */

/** Configuration for Worley noise */
typedef struct Agentite_NoiseWorleyConfig {
    Agentite_WorleyDistance distance;  /**< Distance function */
    Agentite_WorleyReturn return_type; /**< Which distance to return */
    float jitter;                      /**< Cell point jitter (0-1, default 1.0) */
} Agentite_NoiseWorleyConfig;

/** Default Worley configuration */
#define AGENTITE_NOISE_WORLEY_DEFAULT { \
    .distance = AGENTITE_WORLEY_EUCLIDEAN, \
    .return_type = AGENTITE_WORLEY_F1, \
    .jitter = 1.0f \
}

/** Configuration for fractal noise */
typedef struct Agentite_NoiseFractalConfig {
    Agentite_FractalType type;    /**< Fractal combination method */
    int octaves;                  /**< Number of noise layers (1-16) */
    float frequency;              /**< Initial frequency (default 1.0) */
    float lacunarity;             /**< Frequency multiplier per octave (default 2.0) */
    float persistence;            /**< Amplitude multiplier per octave (default 0.5) */
    float gain;                   /**< Gain for ridged noise (default 2.0) */
    float offset;                 /**< Offset for ridged noise (default 1.0) */
    float weighted_strength;      /**< Weighted strength for ridged (default 0.0) */
} Agentite_NoiseFractalConfig;

/** Default fractal configuration */
#define AGENTITE_NOISE_FRACTAL_DEFAULT { \
    .type = AGENTITE_FRACTAL_FBM, \
    .octaves = 4, \
    .frequency = 1.0f, \
    .lacunarity = 2.0f, \
    .persistence = 0.5f, \
    .gain = 2.0f, \
    .offset = 1.0f, \
    .weighted_strength = 0.0f \
}

/** Configuration for domain warping */
typedef struct Agentite_NoiseDomainWarpConfig {
    Agentite_NoiseType noise_type; /**< Type of noise for warping */
    float amplitude;               /**< Warp strength (default 1.0) */
    float frequency;               /**< Warp noise frequency (default 1.0) */
    int octaves;                   /**< Fractal octaves for warp (default 1) */
    float lacunarity;              /**< Frequency multiplier (default 2.0) */
    float persistence;             /**< Amplitude multiplier (default 0.5) */
} Agentite_NoiseDomainWarpConfig;

/** Default domain warp configuration */
#define AGENTITE_NOISE_DOMAIN_WARP_DEFAULT { \
    .noise_type = AGENTITE_NOISE_SIMPLEX, \
    .amplitude = 1.0f, \
    .frequency = 1.0f, \
    .octaves = 1, \
    .lacunarity = 2.0f, \
    .persistence = 0.5f \
}

/** Configuration for heightmap generation */
typedef struct Agentite_HeightmapConfig {
    Agentite_NoiseType noise_type; /**< Base noise algorithm */
    Agentite_NoiseFractalConfig fractal; /**< Fractal settings */
    float scale;                   /**< World-space scale (default 1.0) */
    float offset_x;                /**< X offset for sampling */
    float offset_y;                /**< Y offset for sampling */
    bool normalize;                /**< Normalize output to 0-1 (default true) */
    bool apply_erosion;            /**< Apply simple erosion simulation */
    int erosion_iterations;        /**< Erosion iterations (default 10) */
} Agentite_HeightmapConfig;

/** Default heightmap configuration */
#define AGENTITE_HEIGHTMAP_DEFAULT { \
    .noise_type = AGENTITE_NOISE_SIMPLEX, \
    .fractal = AGENTITE_NOISE_FRACTAL_DEFAULT, \
    .scale = 0.01f, \
    .offset_x = 0.0f, \
    .offset_y = 0.0f, \
    .normalize = true, \
    .apply_erosion = false, \
    .erosion_iterations = 10 \
}

/** Configuration for tilemap noise generation */
typedef struct Agentite_NoiseTilemapConfig {
    int tile_types;                /**< Number of tile types to distribute */
    float *thresholds;             /**< Array of (tile_types-1) threshold values */
    Agentite_NoiseType noise_type; /**< Base noise algorithm */
    Agentite_NoiseFractalConfig fractal; /**< Fractal settings */
    float scale;                   /**< Noise scale (default 0.1) */
} Agentite_NoiseTilemapConfig;

/** Biome distribution configuration */
typedef struct Agentite_BiomeConfig {
    int biome_count;               /**< Number of distinct biomes */
    float temperature_scale;       /**< Scale for temperature noise (default 0.005) */
    float moisture_scale;          /**< Scale for moisture noise (default 0.007) */
    float elevation_influence;     /**< How much elevation affects temperature (default 0.3) */
    float *temperature_ranges;     /**< Biome temp thresholds (biome_count-1 values) */
    float *moisture_ranges;        /**< Biome moisture thresholds (biome_count-1 values) */
    Agentite_NoiseFractalConfig temp_fractal; /**< Temperature noise settings */
    Agentite_NoiseFractalConfig moist_fractal; /**< Moisture noise settings */
} Agentite_BiomeConfig;

/** Resource distribution configuration */
typedef struct Agentite_ResourceConfig {
    float density;                 /**< Base spawn density (0-1, default 0.1) */
    float cluster_scale;           /**< Clustering noise scale (default 0.05) */
    float cluster_threshold;       /**< Threshold for spawning (default 0.6) */
    int *allowed_biomes;           /**< Array of biome indices where resource spawns */
    int allowed_biome_count;       /**< Number of allowed biomes */
    float richness_scale;          /**< Scale for richness variation (default 0.1) */
    Agentite_NoiseFractalConfig fractal; /**< Noise fractal settings */
} Agentite_ResourceConfig;

/* ============================================================================
 * Noise Generator Lifecycle
 * ============================================================================ */

/**
 * Create a noise generator with a seed.
 * Caller OWNS the returned pointer and MUST call agentite_noise_destroy().
 *
 * @param seed Random seed for reproducible noise
 * @return Noise generator, or NULL on failure
 *
 * Thread Safety: Thread-safe (no shared state during creation)
 */
Agentite_Noise *agentite_noise_create(uint64_t seed);

/**
 * Destroy a noise generator.
 * Safe to call with NULL.
 *
 * @param noise Noise generator to destroy
 *
 * Thread Safety: NOT thread-safe (don't destroy while sampling)
 */
void agentite_noise_destroy(Agentite_Noise *noise);

/**
 * Reseed the noise generator.
 *
 * @param noise Noise generator
 * @param seed New random seed
 *
 * Thread Safety: NOT thread-safe
 */
void agentite_noise_reseed(Agentite_Noise *noise, uint64_t seed);

/**
 * Get the current seed.
 *
 * @param noise Noise generator
 * @return Current seed
 *
 * Thread Safety: Thread-safe (read-only)
 */
uint64_t agentite_noise_get_seed(const Agentite_Noise *noise);

/* ============================================================================
 * Perlin Noise
 * ============================================================================ */

/**
 * Sample 2D Perlin noise.
 * Returns value in range [-1, 1].
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @return Noise value in [-1, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_perlin2d(const Agentite_Noise *noise, float x, float y);

/**
 * Sample 3D Perlin noise.
 * Returns value in range [-1, 1].
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @return Noise value in [-1, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_perlin3d(const Agentite_Noise *noise, float x, float y, float z);

/* ============================================================================
 * Simplex Noise
 * ============================================================================ */

/**
 * Sample 2D Simplex noise.
 * Returns value in range [-1, 1]. Faster than Perlin with fewer artifacts.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @return Noise value in [-1, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_simplex2d(const Agentite_Noise *noise, float x, float y);

/**
 * Sample 3D Simplex noise.
 * Returns value in range [-1, 1].
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @return Noise value in [-1, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_simplex3d(const Agentite_Noise *noise, float x, float y, float z);

/* ============================================================================
 * Worley (Cellular) Noise
 * ============================================================================ */

/**
 * Sample 2D Worley noise with default settings (F1, Euclidean).
 * Returns value in range [0, 1].
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_worley2d(const Agentite_Noise *noise, float x, float y);

/**
 * Sample 2D Worley noise with custom configuration.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param config Worley noise configuration
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_worley2d_ex(const Agentite_Noise *noise, float x, float y,
                                  const Agentite_NoiseWorleyConfig *config);

/**
 * Sample 3D Worley noise.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param config Worley noise configuration (NULL for defaults)
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_worley3d(const Agentite_Noise *noise, float x, float y, float z,
                               const Agentite_NoiseWorleyConfig *config);

/* ============================================================================
 * Value Noise
 * ============================================================================ */

/**
 * Sample 2D value noise.
 * Interpolates between random values at integer coordinates.
 * Returns value in range [-1, 1].
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @return Noise value in [-1, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_value2d(const Agentite_Noise *noise, float x, float y);

/**
 * Sample 3D value noise.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @return Noise value in [-1, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_value3d(const Agentite_Noise *noise, float x, float y, float z);

/* ============================================================================
 * Fractal Noise
 * ============================================================================ */

/**
 * Sample 2D fractal Brownian motion noise.
 * Combines multiple octaves of noise for natural-looking variation.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param config Fractal configuration (NULL for defaults)
 * @return Noise value (range depends on configuration)
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_fbm2d(const Agentite_Noise *noise, float x, float y,
                           const Agentite_NoiseFractalConfig *config);

/**
 * Sample 3D fractal Brownian motion noise.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param config Fractal configuration (NULL for defaults)
 * @return Noise value
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_fbm3d(const Agentite_Noise *noise, float x, float y, float z,
                           const Agentite_NoiseFractalConfig *config);

/**
 * Sample 2D ridged multifractal noise.
 * Creates ridge-like features (good for mountains, veins).
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param config Fractal configuration (NULL for defaults)
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_ridged2d(const Agentite_Noise *noise, float x, float y,
                              const Agentite_NoiseFractalConfig *config);

/**
 * Sample 3D ridged multifractal noise.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param config Fractal configuration (NULL for defaults)
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_ridged3d(const Agentite_Noise *noise, float x, float y, float z,
                              const Agentite_NoiseFractalConfig *config);

/**
 * Sample 2D turbulence noise.
 * Uses absolute value of noise for billowy/cloudy effects.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param config Fractal configuration (NULL for defaults)
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_turbulence2d(const Agentite_Noise *noise, float x, float y,
                                  const Agentite_NoiseFractalConfig *config);

/**
 * Sample 3D turbulence noise.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param z Z coordinate
 * @param config Fractal configuration (NULL for defaults)
 * @return Noise value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only after creation)
 */
float agentite_noise_turbulence3d(const Agentite_Noise *noise, float x, float y, float z,
                                  const Agentite_NoiseFractalConfig *config);

/* ============================================================================
 * Domain Warping
 * ============================================================================ */

/**
 * Apply domain warping to 2D coordinates.
 * Modifies (x, y) coordinates based on noise-driven displacement.
 *
 * @param noise Noise generator
 * @param x Pointer to X coordinate (modified in place)
 * @param y Pointer to Y coordinate (modified in place)
 * @param config Domain warp configuration (NULL for defaults)
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
void agentite_noise_domain_warp2d(const Agentite_Noise *noise, float *x, float *y,
                                  const Agentite_NoiseDomainWarpConfig *config);

/**
 * Apply domain warping to 3D coordinates.
 *
 * @param noise Noise generator
 * @param x Pointer to X coordinate (modified in place)
 * @param y Pointer to Y coordinate (modified in place)
 * @param z Pointer to Z coordinate (modified in place)
 * @param config Domain warp configuration (NULL for defaults)
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
void agentite_noise_domain_warp3d(const Agentite_Noise *noise, float *x, float *y, float *z,
                                  const Agentite_NoiseDomainWarpConfig *config);

/**
 * Sample 2D noise with domain warping applied.
 * Convenience function combining warp + sample in one call.
 *
 * @param noise Noise generator
 * @param x X coordinate
 * @param y Y coordinate
 * @param warp_config Domain warp configuration
 * @param fractal_config Fractal configuration for final sample
 * @return Warped noise value
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
float agentite_noise_warped2d(const Agentite_Noise *noise, float x, float y,
                              const Agentite_NoiseDomainWarpConfig *warp_config,
                              const Agentite_NoiseFractalConfig *fractal_config);

/* ============================================================================
 * Heightmap Generation
 * ============================================================================ */

/**
 * Generate a 2D heightmap array.
 * Caller OWNS the returned array and MUST call agentite_noise_heightmap_destroy().
 *
 * @param noise Noise generator
 * @param width Heightmap width in samples
 * @param height Heightmap height in samples
 * @param config Heightmap configuration (NULL for defaults)
 * @return Float array of size (width * height), or NULL on failure.
 *         Access with: heightmap[y * width + x]
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
float *agentite_noise_heightmap_create(const Agentite_Noise *noise,
                                       int width, int height,
                                       const Agentite_HeightmapConfig *config);

/**
 * Destroy a heightmap array.
 * Safe to call with NULL.
 *
 * @param heightmap Heightmap to destroy
 *
 * Thread Safety: Thread-safe
 */
void agentite_noise_heightmap_destroy(float *heightmap);

/**
 * Apply simple hydraulic erosion to a heightmap.
 * Modifies the heightmap in place.
 *
 * @param heightmap Heightmap array
 * @param width Heightmap width
 * @param height Heightmap height
 * @param iterations Number of erosion iterations
 * @param erosion_rate How much material is eroded per iteration (default 0.1)
 * @param deposition_rate How much material is deposited (default 0.1)
 *
 * Thread Safety: NOT thread-safe (modifies heightmap)
 */
void agentite_noise_heightmap_erode(float *heightmap, int width, int height,
                                    int iterations, float erosion_rate, float deposition_rate);

/**
 * Calculate the normal vector at a heightmap point.
 *
 * @param heightmap Heightmap array
 * @param width Heightmap width
 * @param height Heightmap height
 * @param x X coordinate (0 to width-1)
 * @param y Y coordinate (0 to height-1)
 * @param scale Height scale factor
 * @param out_nx Output normal X (can be NULL)
 * @param out_ny Output normal Y (can be NULL)
 * @param out_nz Output normal Z (can be NULL)
 *
 * Thread Safety: Thread-safe (read-only)
 */
void agentite_noise_heightmap_normal(const float *heightmap, int width, int height,
                                     int x, int y, float scale,
                                     float *out_nx, float *out_ny, float *out_nz);

/* ============================================================================
 * Tilemap Generation
 * ============================================================================ */

/**
 * Generate tile indices based on noise thresholds.
 * Caller OWNS the returned array and MUST call free().
 *
 * @param noise Noise generator
 * @param width Tilemap width in tiles
 * @param height Tilemap height in tiles
 * @param config Tilemap noise configuration
 * @return Int array of tile indices, or NULL on failure.
 *         Access with: tiles[y * width + x]
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
int *agentite_noise_tilemap_create(const Agentite_Noise *noise,
                                   int width, int height,
                                   const Agentite_NoiseTilemapConfig *config);

/**
 * Sample noise to determine tile type at a position.
 *
 * @param noise Noise generator
 * @param x World X coordinate
 * @param y World Y coordinate
 * @param config Tilemap noise configuration
 * @return Tile type index (0 to tile_types-1)
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
int agentite_noise_tilemap_sample(const Agentite_Noise *noise, float x, float y,
                                  const Agentite_NoiseTilemapConfig *config);

/* ============================================================================
 * Biome Distribution
 * ============================================================================ */

/**
 * Sample biome at a position based on temperature and moisture.
 *
 * @param noise Noise generator
 * @param x World X coordinate
 * @param y World Y coordinate
 * @param elevation Optional elevation value (0-1), or pass -1 to ignore
 * @param config Biome configuration
 * @return Biome index (0 to biome_count-1)
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
int agentite_noise_biome_sample(const Agentite_Noise *noise, float x, float y,
                                float elevation, const Agentite_BiomeConfig *config);

/**
 * Get temperature value at a position.
 *
 * @param noise Noise generator
 * @param x World X coordinate
 * @param y World Y coordinate
 * @param config Biome configuration
 * @return Temperature in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
float agentite_noise_biome_temperature(const Agentite_Noise *noise, float x, float y,
                                       const Agentite_BiomeConfig *config);

/**
 * Get moisture value at a position.
 *
 * @param noise Noise generator
 * @param x World X coordinate
 * @param y World Y coordinate
 * @param config Biome configuration
 * @return Moisture in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
float agentite_noise_biome_moisture(const Agentite_Noise *noise, float x, float y,
                                    const Agentite_BiomeConfig *config);

/* ============================================================================
 * Resource Distribution
 * ============================================================================ */

/**
 * Check if a resource should spawn at a position.
 *
 * @param noise Noise generator
 * @param x World X coordinate
 * @param y World Y coordinate
 * @param biome Current biome index at this position
 * @param config Resource configuration
 * @return true if resource should spawn here
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
bool agentite_noise_resource_check(const Agentite_Noise *noise, float x, float y,
                                   int biome, const Agentite_ResourceConfig *config);

/**
 * Get resource richness/quantity at a position.
 *
 * @param noise Noise generator
 * @param x World X coordinate
 * @param y World Y coordinate
 * @param config Resource configuration
 * @return Richness value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only noise state)
 */
float agentite_noise_resource_richness(const Agentite_Noise *noise, float x, float y,
                                       const Agentite_ResourceConfig *config);

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

/**
 * Remap a noise value from one range to another.
 *
 * @param value Input value
 * @param in_min Input range minimum
 * @param in_max Input range maximum
 * @param out_min Output range minimum
 * @param out_max Output range maximum
 * @return Remapped value
 *
 * Thread Safety: Thread-safe (pure function)
 */
float agentite_noise_remap(float value, float in_min, float in_max,
                           float out_min, float out_max);

/**
 * Clamp a value to a range.
 *
 * @param value Input value
 * @param min Minimum value
 * @param max Maximum value
 * @return Clamped value
 *
 * Thread Safety: Thread-safe (pure function)
 */
float agentite_noise_clamp(float value, float min, float max);

/**
 * Smooth interpolation (smoothstep).
 *
 * @param edge0 Lower edge
 * @param edge1 Upper edge
 * @param x Value to interpolate
 * @return Smoothly interpolated value in [0, 1]
 *
 * Thread Safety: Thread-safe (pure function)
 */
float agentite_noise_smoothstep(float edge0, float edge1, float x);

/**
 * Linear interpolation.
 *
 * @param a Start value
 * @param b End value
 * @param t Interpolation factor (0-1)
 * @return Interpolated value
 *
 * Thread Safety: Thread-safe (pure function)
 */
float agentite_noise_lerp(float a, float b, float t);

/**
 * Hash function for coordinate-based random values.
 * Useful for consistent per-tile/per-cell randomization.
 *
 * @param noise Noise generator (for seed)
 * @param x Integer X coordinate
 * @param y Integer Y coordinate
 * @return Pseudo-random value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only)
 */
float agentite_noise_hash2d(const Agentite_Noise *noise, int x, int y);

/**
 * 3D hash function.
 *
 * @param noise Noise generator (for seed)
 * @param x Integer X coordinate
 * @param y Integer Y coordinate
 * @param z Integer Z coordinate
 * @return Pseudo-random value in [0, 1]
 *
 * Thread Safety: Thread-safe (read-only)
 */
float agentite_noise_hash3d(const Agentite_Noise *noise, int x, int y, int z);

#ifdef __cplusplus
}
#endif

#endif /* AGENTITE_NOISE_H */
