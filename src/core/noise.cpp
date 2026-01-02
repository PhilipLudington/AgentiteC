/*
 * Agentite Procedural Noise System Implementation
 *
 * Implements Perlin, Simplex, Worley noise and fractal variations.
 * Based on standard noise algorithms with optimizations for game use.
 */

#include "agentite/noise.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Constants
 * ============================================================================ */

#define PERM_SIZE 256
#define PERM_MASK 255

/* Simplex noise skew factors */
static const float F2 = 0.3660254037844386f;  /* (sqrt(3) - 1) / 2 */
static const float G2 = 0.21132486540518713f; /* (3 - sqrt(3)) / 6 */
static const float F3 = 1.0f / 3.0f;
static const float G3 = 1.0f / 6.0f;

/* ============================================================================
 * Internal Types
 * ============================================================================ */

struct Agentite_Noise {
    uint64_t seed;
    uint8_t perm[PERM_SIZE * 2];     /* Permutation table (doubled to avoid modulo) */
    float grad2[PERM_SIZE][2];       /* 2D gradient vectors */
    float grad3[PERM_SIZE][3];       /* 3D gradient vectors */
};

/* ============================================================================
 * Internal Helpers
 * ============================================================================ */

static int noise_fastfloor(float x) {
    int xi = (int)x;
    return x < xi ? xi - 1 : xi;
}

static float noise_fade(float t) {
    /* 6t^5 - 15t^4 + 10t^3 (improved Perlin fade) */
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

static float noise_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

static float noise_dot2(const float *g, float x, float y) {
    return g[0] * x + g[1] * y;
}

static float noise_dot3(const float *g, float x, float y, float z) {
    return g[0] * x + g[1] * y + g[2] * z;
}

/* Simple hash function for seeding */
static uint32_t noise_hash(uint32_t x) {
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = ((x >> 16) ^ x) * 0x45d9f3b;
    x = (x >> 16) ^ x;
    return x;
}

/* Initialize permutation table and gradients from seed */
static void noise_init_tables(Agentite_Noise *noise) {
    /* Initialize permutation table with identity */
    for (int i = 0; i < PERM_SIZE; i++) {
        noise->perm[i] = (uint8_t)i;
    }

    /* Fisher-Yates shuffle using seed */
    uint32_t rng = (uint32_t)(noise->seed ^ (noise->seed >> 32));
    for (int i = PERM_SIZE - 1; i > 0; i--) {
        rng = noise_hash(rng);
        int j = rng % (i + 1);
        uint8_t tmp = noise->perm[i];
        noise->perm[i] = noise->perm[j];
        noise->perm[j] = tmp;
    }

    /* Double the permutation table to avoid modulo */
    for (int i = 0; i < PERM_SIZE; i++) {
        noise->perm[PERM_SIZE + i] = noise->perm[i];
    }

    /* Generate 2D gradient vectors (unit vectors on circle) */
    for (int i = 0; i < PERM_SIZE; i++) {
        float angle = (float)i * (2.0f * (float)M_PI / (float)PERM_SIZE);
        noise->grad2[i][0] = cosf(angle);
        noise->grad2[i][1] = sinf(angle);
    }

    /* Generate 3D gradient vectors (12 edges of cube + 4 corners = 16 vectors) */
    static const float grad3_template[16][3] = {
        {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0},
        {1, 0, 1}, {-1, 0, 1}, {1, 0, -1}, {-1, 0, -1},
        {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
        {1, 1, 0}, {-1, 1, 0}, {0, -1, 1}, {0, -1, -1}
    };
    for (int i = 0; i < PERM_SIZE; i++) {
        int idx = i & 15;
        noise->grad3[i][0] = grad3_template[idx][0];
        noise->grad3[i][1] = grad3_template[idx][1];
        noise->grad3[i][2] = grad3_template[idx][2];
    }
}

/* ============================================================================
 * Noise Generator Lifecycle
 * ============================================================================ */

Agentite_Noise *agentite_noise_create(uint64_t seed) {
    Agentite_Noise *noise = (Agentite_Noise *)calloc(1, sizeof(Agentite_Noise));
    if (!noise) {
        agentite_set_error("noise: failed to allocate noise generator");
        return NULL;
    }

    noise->seed = seed;
    noise_init_tables(noise);

    return noise;
}

void agentite_noise_destroy(Agentite_Noise *noise) {
    free(noise);
}

void agentite_noise_reseed(Agentite_Noise *noise, uint64_t seed) {
    if (!noise) return;
    noise->seed = seed;
    noise_init_tables(noise);
}

uint64_t agentite_noise_get_seed(const Agentite_Noise *noise) {
    return noise ? noise->seed : 0;
}

/* ============================================================================
 * Perlin Noise
 * ============================================================================ */

float agentite_noise_perlin2d(const Agentite_Noise *noise, float x, float y) {
    if (!noise) return 0.0f;

    /* Find unit grid cell containing point */
    int X = noise_fastfloor(x);
    int Y = noise_fastfloor(y);

    /* Get relative xy coordinates of point within cell */
    x -= (float)X;
    y -= (float)Y;

    /* Wrap cell coordinates */
    X &= PERM_MASK;
    Y &= PERM_MASK;

    /* Compute fade curves for x and y */
    float u = noise_fade(x);
    float v = noise_fade(y);

    /* Hash coordinates of the 4 square corners */
    int aa = noise->perm[X + noise->perm[Y]];
    int ab = noise->perm[X + noise->perm[Y + 1]];
    int ba = noise->perm[X + 1 + noise->perm[Y]];
    int bb = noise->perm[X + 1 + noise->perm[Y + 1]];

    /* Add blended results from 4 corners of square */
    float res = noise_lerp(
        noise_lerp(noise_dot2(noise->grad2[aa], x, y),
                   noise_dot2(noise->grad2[ba], x - 1, y), u),
        noise_lerp(noise_dot2(noise->grad2[ab], x, y - 1),
                   noise_dot2(noise->grad2[bb], x - 1, y - 1), u),
        v);

    /* Scale to [-1, 1] */
    return res * 1.4142135623730951f;
}

float agentite_noise_perlin3d(const Agentite_Noise *noise, float x, float y, float z) {
    if (!noise) return 0.0f;

    /* Find unit cube containing point */
    int X = noise_fastfloor(x);
    int Y = noise_fastfloor(y);
    int Z = noise_fastfloor(z);

    /* Get relative xyz coordinates of point within cube */
    x -= (float)X;
    y -= (float)Y;
    z -= (float)Z;

    /* Wrap coordinates */
    X &= PERM_MASK;
    Y &= PERM_MASK;
    Z &= PERM_MASK;

    /* Compute fade curves */
    float u = noise_fade(x);
    float v = noise_fade(y);
    float w = noise_fade(z);

    /* Hash coordinates of 8 cube corners */
    int aaa = noise->perm[X + noise->perm[Y + noise->perm[Z]]];
    int aab = noise->perm[X + noise->perm[Y + noise->perm[Z + 1]]];
    int aba = noise->perm[X + noise->perm[Y + 1 + noise->perm[Z]]];
    int abb = noise->perm[X + noise->perm[Y + 1 + noise->perm[Z + 1]]];
    int baa = noise->perm[X + 1 + noise->perm[Y + noise->perm[Z]]];
    int bab = noise->perm[X + 1 + noise->perm[Y + noise->perm[Z + 1]]];
    int bba = noise->perm[X + 1 + noise->perm[Y + 1 + noise->perm[Z]]];
    int bbb = noise->perm[X + 1 + noise->perm[Y + 1 + noise->perm[Z + 1]]];

    /* Blend results from 8 corners */
    float res = noise_lerp(
        noise_lerp(
            noise_lerp(noise_dot3(noise->grad3[aaa], x, y, z),
                       noise_dot3(noise->grad3[baa], x - 1, y, z), u),
            noise_lerp(noise_dot3(noise->grad3[aba], x, y - 1, z),
                       noise_dot3(noise->grad3[bba], x - 1, y - 1, z), u),
            v),
        noise_lerp(
            noise_lerp(noise_dot3(noise->grad3[aab], x, y, z - 1),
                       noise_dot3(noise->grad3[bab], x - 1, y, z - 1), u),
            noise_lerp(noise_dot3(noise->grad3[abb], x, y - 1, z - 1),
                       noise_dot3(noise->grad3[bbb], x - 1, y - 1, z - 1), u),
            v),
        w);

    return res;
}

/* ============================================================================
 * Simplex Noise
 * ============================================================================ */

float agentite_noise_simplex2d(const Agentite_Noise *noise, float x, float y) {
    if (!noise) return 0.0f;

    /* Skew input space to determine which simplex cell we're in */
    float s = (x + y) * F2;
    int i = noise_fastfloor(x + s);
    int j = noise_fastfloor(y + s);

    /* Unskew back to (x,y) space */
    float t = (float)(i + j) * G2;
    float X0 = (float)i - t;
    float Y0 = (float)j - t;
    float x0 = x - X0;
    float y0 = y - Y0;

    /* Determine which simplex we're in */
    int i1, j1;
    if (x0 > y0) {
        i1 = 1; j1 = 0;
    } else {
        i1 = 0; j1 = 1;
    }

    /* Offsets for middle and last corners */
    float x1 = x0 - (float)i1 + G2;
    float y1 = y0 - (float)j1 + G2;
    float x2 = x0 - 1.0f + 2.0f * G2;
    float y2 = y0 - 1.0f + 2.0f * G2;

    /* Wrap indices */
    int ii = i & PERM_MASK;
    int jj = j & PERM_MASK;

    /* Calculate contribution from three corners */
    float n0, n1, n2;

    float t0 = 0.5f - x0 * x0 - y0 * y0;
    if (t0 < 0.0f) {
        n0 = 0.0f;
    } else {
        t0 *= t0;
        int gi0 = noise->perm[ii + noise->perm[jj]];
        n0 = t0 * t0 * noise_dot2(noise->grad2[gi0], x0, y0);
    }

    float t1 = 0.5f - x1 * x1 - y1 * y1;
    if (t1 < 0.0f) {
        n1 = 0.0f;
    } else {
        t1 *= t1;
        int gi1 = noise->perm[ii + i1 + noise->perm[jj + j1]];
        n1 = t1 * t1 * noise_dot2(noise->grad2[gi1], x1, y1);
    }

    float t2 = 0.5f - x2 * x2 - y2 * y2;
    if (t2 < 0.0f) {
        n2 = 0.0f;
    } else {
        t2 *= t2;
        int gi2 = noise->perm[ii + 1 + noise->perm[jj + 1]];
        n2 = t2 * t2 * noise_dot2(noise->grad2[gi2], x2, y2);
    }

    /* Scale result to [-1, 1] */
    return 70.0f * (n0 + n1 + n2);
}

float agentite_noise_simplex3d(const Agentite_Noise *noise, float x, float y, float z) {
    if (!noise) return 0.0f;

    /* Skew input space */
    float s = (x + y + z) * F3;
    int i = noise_fastfloor(x + s);
    int j = noise_fastfloor(y + s);
    int k = noise_fastfloor(z + s);

    /* Unskew */
    float t = (float)(i + j + k) * G3;
    float X0 = (float)i - t;
    float Y0 = (float)j - t;
    float Z0 = (float)k - t;
    float x0 = x - X0;
    float y0 = y - Y0;
    float z0 = z - Z0;

    /* Determine simplex */
    int i1, j1, k1, i2, j2, k2;
    if (x0 >= y0) {
        if (y0 >= z0) {
            i1 = 1; j1 = 0; k1 = 0;
            i2 = 1; j2 = 1; k2 = 0;
        } else if (x0 >= z0) {
            i1 = 1; j1 = 0; k1 = 0;
            i2 = 1; j2 = 0; k2 = 1;
        } else {
            i1 = 0; j1 = 0; k1 = 1;
            i2 = 1; j2 = 0; k2 = 1;
        }
    } else {
        if (y0 < z0) {
            i1 = 0; j1 = 0; k1 = 1;
            i2 = 0; j2 = 1; k2 = 1;
        } else if (x0 < z0) {
            i1 = 0; j1 = 1; k1 = 0;
            i2 = 0; j2 = 1; k2 = 1;
        } else {
            i1 = 0; j1 = 1; k1 = 0;
            i2 = 1; j2 = 1; k2 = 0;
        }
    }

    /* Offsets for remaining corners */
    float x1 = x0 - (float)i1 + G3;
    float y1 = y0 - (float)j1 + G3;
    float z1 = z0 - (float)k1 + G3;
    float x2 = x0 - (float)i2 + 2.0f * G3;
    float y2 = y0 - (float)j2 + 2.0f * G3;
    float z2 = z0 - (float)k2 + 2.0f * G3;
    float x3 = x0 - 1.0f + 3.0f * G3;
    float y3 = y0 - 1.0f + 3.0f * G3;
    float z3 = z0 - 1.0f + 3.0f * G3;

    /* Wrap indices */
    int ii = i & PERM_MASK;
    int jj = j & PERM_MASK;
    int kk = k & PERM_MASK;

    /* Calculate contributions from four corners */
    float n0, n1, n2, n3;

    float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0;
    if (t0 < 0.0f) {
        n0 = 0.0f;
    } else {
        t0 *= t0;
        int gi0 = noise->perm[ii + noise->perm[jj + noise->perm[kk]]];
        n0 = t0 * t0 * noise_dot3(noise->grad3[gi0], x0, y0, z0);
    }

    float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1;
    if (t1 < 0.0f) {
        n1 = 0.0f;
    } else {
        t1 *= t1;
        int gi1 = noise->perm[ii + i1 + noise->perm[jj + j1 + noise->perm[kk + k1]]];
        n1 = t1 * t1 * noise_dot3(noise->grad3[gi1], x1, y1, z1);
    }

    float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2;
    if (t2 < 0.0f) {
        n2 = 0.0f;
    } else {
        t2 *= t2;
        int gi2 = noise->perm[ii + i2 + noise->perm[jj + j2 + noise->perm[kk + k2]]];
        n2 = t2 * t2 * noise_dot3(noise->grad3[gi2], x2, y2, z2);
    }

    float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3;
    if (t3 < 0.0f) {
        n3 = 0.0f;
    } else {
        t3 *= t3;
        int gi3 = noise->perm[ii + 1 + noise->perm[jj + 1 + noise->perm[kk + 1]]];
        n3 = t3 * t3 * noise_dot3(noise->grad3[gi3], x3, y3, z3);
    }

    /* Scale result to [-1, 1] */
    return 32.0f * (n0 + n1 + n2 + n3);
}

/* ============================================================================
 * Worley (Cellular) Noise
 * ============================================================================ */

/* Get hash value for cell point position */
static void worley_cell_point(const Agentite_Noise *noise, int xi, int yi,
                              float jitter, float *px, float *py) {
    uint32_t h = noise_hash((uint32_t)(xi * 73856093 ^ yi * 19349663 ^ noise->seed));
    float fx = (float)(h & 0xFFFF) / 65535.0f;
    float fy = (float)((h >> 16) & 0xFFFF) / 65535.0f;
    *px = (float)xi + 0.5f + (fx - 0.5f) * jitter;
    *py = (float)yi + 0.5f + (fy - 0.5f) * jitter;
}

static void worley_cell_point_3d(const Agentite_Noise *noise, int xi, int yi, int zi,
                                  float jitter, float *px, float *py, float *pz) {
    uint32_t h1 = noise_hash((uint32_t)(xi * 73856093 ^ yi * 19349663 ^ zi * 83492791 ^ (uint32_t)noise->seed));
    uint32_t h2 = noise_hash(h1);
    float fx = (float)(h1 & 0xFFFF) / 65535.0f;
    float fy = (float)((h1 >> 16) & 0xFFFF) / 65535.0f;
    float fz = (float)(h2 & 0xFFFF) / 65535.0f;
    *px = (float)xi + 0.5f + (fx - 0.5f) * jitter;
    *py = (float)yi + 0.5f + (fy - 0.5f) * jitter;
    *pz = (float)zi + 0.5f + (fz - 0.5f) * jitter;
}

static float worley_distance(float dx, float dy, Agentite_WorleyDistance type) {
    switch (type) {
        case AGENTITE_WORLEY_EUCLIDEAN:
            return sqrtf(dx * dx + dy * dy);
        case AGENTITE_WORLEY_MANHATTAN:
            return fabsf(dx) + fabsf(dy);
        case AGENTITE_WORLEY_CHEBYSHEV:
            return fmaxf(fabsf(dx), fabsf(dy));
        default:
            return sqrtf(dx * dx + dy * dy);
    }
}

static float worley_distance_3d(float dx, float dy, float dz, Agentite_WorleyDistance type) {
    switch (type) {
        case AGENTITE_WORLEY_EUCLIDEAN:
            return sqrtf(dx * dx + dy * dy + dz * dz);
        case AGENTITE_WORLEY_MANHATTAN:
            return fabsf(dx) + fabsf(dy) + fabsf(dz);
        case AGENTITE_WORLEY_CHEBYSHEV:
            return fmaxf(fmaxf(fabsf(dx), fabsf(dy)), fabsf(dz));
        default:
            return sqrtf(dx * dx + dy * dy + dz * dz);
    }
}

float agentite_noise_worley2d(const Agentite_Noise *noise, float x, float y) {
    Agentite_NoiseWorleyConfig config = AGENTITE_NOISE_WORLEY_DEFAULT;
    return agentite_noise_worley2d_ex(noise, x, y, &config);
}

float agentite_noise_worley2d_ex(const Agentite_Noise *noise, float x, float y,
                                  const Agentite_NoiseWorleyConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseWorleyConfig cfg = config ? *config : (Agentite_NoiseWorleyConfig)AGENTITE_NOISE_WORLEY_DEFAULT;

    int xi = noise_fastfloor(x);
    int yi = noise_fastfloor(y);

    float f1 = 999999.0f;
    float f2 = 999999.0f;

    /* Check 3x3 neighborhood of cells */
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            float px, py;
            worley_cell_point(noise, xi + dx, yi + dy, cfg.jitter, &px, &py);

            float dist = worley_distance(x - px, y - py, cfg.distance);

            if (dist < f1) {
                f2 = f1;
                f1 = dist;
            } else if (dist < f2) {
                f2 = dist;
            }
        }
    }

    /* Return requested value type */
    switch (cfg.return_type) {
        case AGENTITE_WORLEY_F1:
            return f1;
        case AGENTITE_WORLEY_F2:
            return f2;
        case AGENTITE_WORLEY_F2_F1:
            return f2 - f1;
        case AGENTITE_WORLEY_F1_F2:
            return (f1 + f2) * 0.5f;
        default:
            return f1;
    }
}

float agentite_noise_worley3d(const Agentite_Noise *noise, float x, float y, float z,
                               const Agentite_NoiseWorleyConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseWorleyConfig cfg = config ? *config : (Agentite_NoiseWorleyConfig)AGENTITE_NOISE_WORLEY_DEFAULT;

    int xi = noise_fastfloor(x);
    int yi = noise_fastfloor(y);
    int zi = noise_fastfloor(z);

    float f1 = 999999.0f;
    float f2 = 999999.0f;

    /* Check 3x3x3 neighborhood of cells */
    for (int dz = -1; dz <= 1; dz++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                float px, py, pz;
                worley_cell_point_3d(noise, xi + dx, yi + dy, zi + dz,
                                     cfg.jitter, &px, &py, &pz);

                float dist = worley_distance_3d(x - px, y - py, z - pz, cfg.distance);

                if (dist < f1) {
                    f2 = f1;
                    f1 = dist;
                } else if (dist < f2) {
                    f2 = dist;
                }
            }
        }
    }

    switch (cfg.return_type) {
        case AGENTITE_WORLEY_F1:
            return f1;
        case AGENTITE_WORLEY_F2:
            return f2;
        case AGENTITE_WORLEY_F2_F1:
            return f2 - f1;
        case AGENTITE_WORLEY_F1_F2:
            return (f1 + f2) * 0.5f;
        default:
            return f1;
    }
}

/* ============================================================================
 * Value Noise
 * ============================================================================ */

float agentite_noise_value2d(const Agentite_Noise *noise, float x, float y) {
    if (!noise) return 0.0f;

    int X = noise_fastfloor(x);
    int Y = noise_fastfloor(y);

    x -= (float)X;
    y -= (float)Y;

    X &= PERM_MASK;
    Y &= PERM_MASK;

    float u = noise_fade(x);
    float v = noise_fade(y);

    /* Get random values at corners */
    float n00 = (float)noise->perm[X + noise->perm[Y]] / 127.5f - 1.0f;
    float n01 = (float)noise->perm[X + noise->perm[Y + 1]] / 127.5f - 1.0f;
    float n10 = (float)noise->perm[X + 1 + noise->perm[Y]] / 127.5f - 1.0f;
    float n11 = (float)noise->perm[X + 1 + noise->perm[Y + 1]] / 127.5f - 1.0f;

    return noise_lerp(
        noise_lerp(n00, n10, u),
        noise_lerp(n01, n11, u),
        v);
}

float agentite_noise_value3d(const Agentite_Noise *noise, float x, float y, float z) {
    if (!noise) return 0.0f;

    int X = noise_fastfloor(x);
    int Y = noise_fastfloor(y);
    int Z = noise_fastfloor(z);

    x -= (float)X;
    y -= (float)Y;
    z -= (float)Z;

    X &= PERM_MASK;
    Y &= PERM_MASK;
    Z &= PERM_MASK;

    float u = noise_fade(x);
    float v = noise_fade(y);
    float w = noise_fade(z);

    /* Get random values at cube corners */
    float n000 = (float)noise->perm[X + noise->perm[Y + noise->perm[Z]]] / 127.5f - 1.0f;
    float n001 = (float)noise->perm[X + noise->perm[Y + noise->perm[Z + 1]]] / 127.5f - 1.0f;
    float n010 = (float)noise->perm[X + noise->perm[Y + 1 + noise->perm[Z]]] / 127.5f - 1.0f;
    float n011 = (float)noise->perm[X + noise->perm[Y + 1 + noise->perm[Z + 1]]] / 127.5f - 1.0f;
    float n100 = (float)noise->perm[X + 1 + noise->perm[Y + noise->perm[Z]]] / 127.5f - 1.0f;
    float n101 = (float)noise->perm[X + 1 + noise->perm[Y + noise->perm[Z + 1]]] / 127.5f - 1.0f;
    float n110 = (float)noise->perm[X + 1 + noise->perm[Y + 1 + noise->perm[Z]]] / 127.5f - 1.0f;
    float n111 = (float)noise->perm[X + 1 + noise->perm[Y + 1 + noise->perm[Z + 1]]] / 127.5f - 1.0f;

    return noise_lerp(
        noise_lerp(
            noise_lerp(n000, n100, u),
            noise_lerp(n010, n110, u),
            v),
        noise_lerp(
            noise_lerp(n001, n101, u),
            noise_lerp(n011, n111, u),
            v),
        w);
}

/* ============================================================================
 * Fractal Noise
 * ============================================================================ */

/* Get base noise sample based on type (internal) */
static float noise_sample_2d(const Agentite_Noise *noise, Agentite_NoiseType type,
                              float x, float y) {
    switch (type) {
        case AGENTITE_NOISE_PERLIN:
            return agentite_noise_perlin2d(noise, x, y);
        case AGENTITE_NOISE_SIMPLEX:
            return agentite_noise_simplex2d(noise, x, y);
        case AGENTITE_NOISE_WORLEY:
            return agentite_noise_worley2d(noise, x, y) * 2.0f - 1.0f;
        case AGENTITE_NOISE_VALUE:
            return agentite_noise_value2d(noise, x, y);
        default:
            return agentite_noise_simplex2d(noise, x, y);
    }
}

static float noise_sample_3d(const Agentite_Noise *noise, Agentite_NoiseType type,
                              float x, float y, float z) {
    switch (type) {
        case AGENTITE_NOISE_PERLIN:
            return agentite_noise_perlin3d(noise, x, y, z);
        case AGENTITE_NOISE_SIMPLEX:
            return agentite_noise_simplex3d(noise, x, y, z);
        case AGENTITE_NOISE_WORLEY:
            return agentite_noise_worley3d(noise, x, y, z, NULL) * 2.0f - 1.0f;
        case AGENTITE_NOISE_VALUE:
            return agentite_noise_value3d(noise, x, y, z);
        default:
            return agentite_noise_simplex3d(noise, x, y, z);
    }
}

float agentite_noise_fbm2d(const Agentite_Noise *noise, float x, float y,
                           const Agentite_NoiseFractalConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseFractalConfig cfg = config ? *config : (Agentite_NoiseFractalConfig)AGENTITE_NOISE_FRACTAL_DEFAULT;

    int octaves = cfg.octaves > 16 ? 16 : (cfg.octaves < 1 ? 1 : cfg.octaves);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = cfg.frequency;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        sum += agentite_noise_simplex2d(noise, x * frequency, y * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= cfg.persistence;
        frequency *= cfg.lacunarity;
    }

    return sum / max_value;
}

float agentite_noise_fbm3d(const Agentite_Noise *noise, float x, float y, float z,
                           const Agentite_NoiseFractalConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseFractalConfig cfg = config ? *config : (Agentite_NoiseFractalConfig)AGENTITE_NOISE_FRACTAL_DEFAULT;

    int octaves = cfg.octaves > 16 ? 16 : (cfg.octaves < 1 ? 1 : cfg.octaves);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = cfg.frequency;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        sum += agentite_noise_simplex3d(noise, x * frequency, y * frequency, z * frequency) * amplitude;
        max_value += amplitude;
        amplitude *= cfg.persistence;
        frequency *= cfg.lacunarity;
    }

    return sum / max_value;
}

float agentite_noise_ridged2d(const Agentite_Noise *noise, float x, float y,
                              const Agentite_NoiseFractalConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseFractalConfig cfg = config ? *config : (Agentite_NoiseFractalConfig)AGENTITE_NOISE_FRACTAL_DEFAULT;

    int octaves = cfg.octaves > 16 ? 16 : (cfg.octaves < 1 ? 1 : cfg.octaves);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = cfg.frequency;
    float weight = 1.0f;

    for (int i = 0; i < octaves; i++) {
        float n = agentite_noise_simplex2d(noise, x * frequency, y * frequency);
        n = cfg.offset - fabsf(n);
        n *= n;
        n *= weight;
        weight = n * cfg.gain;
        if (weight > 1.0f) weight = 1.0f;
        if (weight < 0.0f) weight = 0.0f;

        sum += n * amplitude;
        frequency *= cfg.lacunarity;
        amplitude *= cfg.persistence;
    }

    return sum;
}

float agentite_noise_ridged3d(const Agentite_Noise *noise, float x, float y, float z,
                              const Agentite_NoiseFractalConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseFractalConfig cfg = config ? *config : (Agentite_NoiseFractalConfig)AGENTITE_NOISE_FRACTAL_DEFAULT;

    int octaves = cfg.octaves > 16 ? 16 : (cfg.octaves < 1 ? 1 : cfg.octaves);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = cfg.frequency;
    float weight = 1.0f;

    for (int i = 0; i < octaves; i++) {
        float n = agentite_noise_simplex3d(noise, x * frequency, y * frequency, z * frequency);
        n = cfg.offset - fabsf(n);
        n *= n;
        n *= weight;
        weight = n * cfg.gain;
        if (weight > 1.0f) weight = 1.0f;
        if (weight < 0.0f) weight = 0.0f;

        sum += n * amplitude;
        frequency *= cfg.lacunarity;
        amplitude *= cfg.persistence;
    }

    return sum;
}

float agentite_noise_turbulence2d(const Agentite_Noise *noise, float x, float y,
                                  const Agentite_NoiseFractalConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseFractalConfig cfg = config ? *config : (Agentite_NoiseFractalConfig)AGENTITE_NOISE_FRACTAL_DEFAULT;

    int octaves = cfg.octaves > 16 ? 16 : (cfg.octaves < 1 ? 1 : cfg.octaves);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = cfg.frequency;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        sum += fabsf(agentite_noise_simplex2d(noise, x * frequency, y * frequency)) * amplitude;
        max_value += amplitude;
        amplitude *= cfg.persistence;
        frequency *= cfg.lacunarity;
    }

    return sum / max_value;
}

float agentite_noise_turbulence3d(const Agentite_Noise *noise, float x, float y, float z,
                                  const Agentite_NoiseFractalConfig *config) {
    if (!noise) return 0.0f;

    Agentite_NoiseFractalConfig cfg = config ? *config : (Agentite_NoiseFractalConfig)AGENTITE_NOISE_FRACTAL_DEFAULT;

    int octaves = cfg.octaves > 16 ? 16 : (cfg.octaves < 1 ? 1 : cfg.octaves);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float frequency = cfg.frequency;
    float max_value = 0.0f;

    for (int i = 0; i < octaves; i++) {
        sum += fabsf(agentite_noise_simplex3d(noise, x * frequency, y * frequency, z * frequency)) * amplitude;
        max_value += amplitude;
        amplitude *= cfg.persistence;
        frequency *= cfg.lacunarity;
    }

    return sum / max_value;
}

/* ============================================================================
 * Domain Warping
 * ============================================================================ */

void agentite_noise_domain_warp2d(const Agentite_Noise *noise, float *x, float *y,
                                  const Agentite_NoiseDomainWarpConfig *config) {
    if (!noise || !x || !y) return;

    Agentite_NoiseDomainWarpConfig cfg = config ? *config : (Agentite_NoiseDomainWarpConfig)AGENTITE_NOISE_DOMAIN_WARP_DEFAULT;

    /* Sample warp noise at offset positions */
    float warp_x = noise_sample_2d(noise, cfg.noise_type, *x * cfg.frequency, *y * cfg.frequency);
    float warp_y = noise_sample_2d(noise, cfg.noise_type, *x * cfg.frequency + 100.0f, *y * cfg.frequency + 100.0f);

    /* Apply warp with additional octaves if requested */
    if (cfg.octaves > 1) {
        Agentite_NoiseFractalConfig fractal = {
            .type = AGENTITE_FRACTAL_FBM,
            .octaves = cfg.octaves,
            .frequency = 1.0f,
            .lacunarity = cfg.lacunarity,
            .persistence = cfg.persistence,
            .gain = 2.0f,
            .offset = 1.0f,
            .weighted_strength = 0.0f
        };
        warp_x = agentite_noise_fbm2d(noise, *x * cfg.frequency, *y * cfg.frequency, &fractal);
        warp_y = agentite_noise_fbm2d(noise, *x * cfg.frequency + 100.0f, *y * cfg.frequency + 100.0f, &fractal);
    }

    *x += warp_x * cfg.amplitude;
    *y += warp_y * cfg.amplitude;
}

void agentite_noise_domain_warp3d(const Agentite_Noise *noise, float *x, float *y, float *z,
                                  const Agentite_NoiseDomainWarpConfig *config) {
    if (!noise || !x || !y || !z) return;

    Agentite_NoiseDomainWarpConfig cfg = config ? *config : (Agentite_NoiseDomainWarpConfig)AGENTITE_NOISE_DOMAIN_WARP_DEFAULT;

    float warp_x = noise_sample_3d(noise, cfg.noise_type, *x * cfg.frequency, *y * cfg.frequency, *z * cfg.frequency);
    float warp_y = noise_sample_3d(noise, cfg.noise_type, *x * cfg.frequency + 100.0f, *y * cfg.frequency + 100.0f, *z * cfg.frequency + 100.0f);
    float warp_z = noise_sample_3d(noise, cfg.noise_type, *x * cfg.frequency + 200.0f, *y * cfg.frequency + 200.0f, *z * cfg.frequency + 200.0f);

    *x += warp_x * cfg.amplitude;
    *y += warp_y * cfg.amplitude;
    *z += warp_z * cfg.amplitude;
}

float agentite_noise_warped2d(const Agentite_Noise *noise, float x, float y,
                              const Agentite_NoiseDomainWarpConfig *warp_config,
                              const Agentite_NoiseFractalConfig *fractal_config) {
    if (!noise) return 0.0f;

    agentite_noise_domain_warp2d(noise, &x, &y, warp_config);
    return agentite_noise_fbm2d(noise, x, y, fractal_config);
}

/* ============================================================================
 * Heightmap Generation
 * ============================================================================ */

float *agentite_noise_heightmap_create(const Agentite_Noise *noise,
                                       int width, int height,
                                       const Agentite_HeightmapConfig *config) {
    if (!noise || width <= 0 || height <= 0) {
        agentite_set_error("noise: invalid heightmap parameters");
        return NULL;
    }

    Agentite_HeightmapConfig cfg = config ? *config : (Agentite_HeightmapConfig)AGENTITE_HEIGHTMAP_DEFAULT;

    float *heightmap = (float *)calloc((size_t)width * (size_t)height, sizeof(float));
    if (!heightmap) {
        agentite_set_error("noise: failed to allocate heightmap");
        return NULL;
    }

    float min_val = 999999.0f;
    float max_val = -999999.0f;

    /* Generate noise values */
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            float nx = (cfg.offset_x + (float)x) * cfg.scale;
            float ny = (cfg.offset_y + (float)y) * cfg.scale;

            float value;
            switch (cfg.fractal.type) {
                case AGENTITE_FRACTAL_RIDGED:
                    value = agentite_noise_ridged2d(noise, nx, ny, &cfg.fractal);
                    break;
                case AGENTITE_FRACTAL_TURBULENCE:
                    value = agentite_noise_turbulence2d(noise, nx, ny, &cfg.fractal);
                    break;
                default:
                    value = agentite_noise_fbm2d(noise, nx, ny, &cfg.fractal);
                    break;
            }

            heightmap[y * width + x] = value;

            if (value < min_val) min_val = value;
            if (value > max_val) max_val = value;
        }
    }

    /* Normalize to 0-1 if requested */
    if (cfg.normalize && max_val > min_val) {
        float range = max_val - min_val;
        for (int i = 0; i < width * height; i++) {
            heightmap[i] = (heightmap[i] - min_val) / range;
        }
    }

    /* Apply erosion if requested */
    if (cfg.apply_erosion && cfg.erosion_iterations > 0) {
        agentite_noise_heightmap_erode(heightmap, width, height,
                                       cfg.erosion_iterations, 0.1f, 0.1f);
    }

    return heightmap;
}

void agentite_noise_heightmap_destroy(float *heightmap) {
    free(heightmap);
}

void agentite_noise_heightmap_erode(float *heightmap, int width, int height,
                                    int iterations, float erosion_rate, float deposition_rate) {
    if (!heightmap || width <= 0 || height <= 0 || iterations <= 0) return;

    /* Simple thermal erosion simulation */
    float *temp = (float *)malloc((size_t)width * (size_t)height * sizeof(float));
    if (!temp) return;

    float talus_angle = 4.0f / (float)width; /* Angle of repose */

    for (int iter = 0; iter < iterations; iter++) {
        memcpy(temp, heightmap, (size_t)width * (size_t)height * sizeof(float));

        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {
                int idx = y * width + x;
                float h = heightmap[idx];

                /* Find steepest downhill neighbor */
                float max_diff = 0.0f;
                int max_nx = x, max_ny = y;

                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (dx == 0 && dy == 0) continue;
                        int nidx = (y + dy) * width + (x + dx);
                        float nh = heightmap[nidx];
                        float diff = h - nh;
                        if (diff > max_diff) {
                            max_diff = diff;
                            max_nx = x + dx;
                            max_ny = y + dy;
                        }
                    }
                }

                /* Erode if slope exceeds talus angle */
                if (max_diff > talus_angle) {
                    float amount = (max_diff - talus_angle) * erosion_rate;
                    temp[idx] -= amount;
                    temp[max_ny * width + max_nx] += amount * deposition_rate;
                }
            }
        }

        memcpy(heightmap, temp, (size_t)width * (size_t)height * sizeof(float));
    }

    free(temp);
}

void agentite_noise_heightmap_normal(const float *heightmap, int width, int height,
                                     int x, int y, float scale,
                                     float *out_nx, float *out_ny, float *out_nz) {
    if (!heightmap || width <= 0 || height <= 0) {
        if (out_nx) *out_nx = 0.0f;
        if (out_ny) *out_ny = 1.0f;
        if (out_nz) *out_nz = 0.0f;
        return;
    }

    /* Clamp coordinates */
    int x0 = (x > 0) ? x - 1 : 0;
    int x1 = (x < width - 1) ? x + 1 : width - 1;
    int y0 = (y > 0) ? y - 1 : 0;
    int y1 = (y < height - 1) ? y + 1 : height - 1;

    /* Sample heights */
    float hL = heightmap[y * width + x0];
    float hR = heightmap[y * width + x1];
    float hD = heightmap[y0 * width + x];
    float hU = heightmap[y1 * width + x];

    /* Calculate normal using central differences */
    float nx = (hL - hR) * scale;
    float ny = 2.0f;
    float nz = (hD - hU) * scale;

    /* Normalize */
    float len = sqrtf(nx * nx + ny * ny + nz * nz);
    if (len > 0.0001f) {
        nx /= len;
        ny /= len;
        nz /= len;
    }

    if (out_nx) *out_nx = nx;
    if (out_ny) *out_ny = ny;
    if (out_nz) *out_nz = nz;
}

/* ============================================================================
 * Tilemap Generation
 * ============================================================================ */

int *agentite_noise_tilemap_create(const Agentite_Noise *noise,
                                   int width, int height,
                                   const Agentite_NoiseTilemapConfig *config) {
    if (!noise || !config || width <= 0 || height <= 0 || config->tile_types < 2) {
        agentite_set_error("noise: invalid tilemap parameters");
        return NULL;
    }

    int *tiles = (int *)calloc((size_t)width * (size_t)height, sizeof(int));
    if (!tiles) {
        agentite_set_error("noise: failed to allocate tilemap");
        return NULL;
    }

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            tiles[y * width + x] = agentite_noise_tilemap_sample(noise, (float)x, (float)y, config);
        }
    }

    return tiles;
}

int agentite_noise_tilemap_sample(const Agentite_Noise *noise, float x, float y,
                                  const Agentite_NoiseTilemapConfig *config) {
    if (!noise || !config || config->tile_types < 1) return 0;

    float nx = x * config->scale;
    float ny = y * config->scale;

    float value;
    switch (config->noise_type) {
        case AGENTITE_NOISE_PERLIN:
            value = agentite_noise_perlin2d(noise, nx, ny);
            break;
        case AGENTITE_NOISE_WORLEY:
            value = agentite_noise_worley2d(noise, nx, ny) * 2.0f - 1.0f;
            break;
        case AGENTITE_NOISE_VALUE:
            value = agentite_noise_value2d(noise, nx, ny);
            break;
        default:
            value = agentite_noise_simplex2d(noise, nx, ny);
            break;
    }

    /* Apply fractal if configured */
    if (config->fractal.octaves > 1) {
        value = agentite_noise_fbm2d(noise, nx, ny, &config->fractal);
    }

    /* Normalize to 0-1 */
    value = (value + 1.0f) * 0.5f;
    if (value < 0.0f) value = 0.0f;
    if (value > 1.0f) value = 1.0f;

    /* Find tile type based on thresholds */
    if (!config->thresholds) {
        /* Even distribution */
        return (int)(value * (float)config->tile_types) % config->tile_types;
    }

    for (int i = 0; i < config->tile_types - 1; i++) {
        if (value < config->thresholds[i]) {
            return i;
        }
    }
    return config->tile_types - 1;
}

/* ============================================================================
 * Biome Distribution
 * ============================================================================ */

int agentite_noise_biome_sample(const Agentite_Noise *noise, float x, float y,
                                float elevation, const Agentite_BiomeConfig *config) {
    if (!noise || !config || config->biome_count < 1) return 0;

    float temp = agentite_noise_biome_temperature(noise, x, y, config);
    float moist = agentite_noise_biome_moisture(noise, x, y, config);

    /* Modify temperature based on elevation */
    if (elevation >= 0.0f && config->elevation_influence > 0.0f) {
        temp -= elevation * config->elevation_influence;
        if (temp < 0.0f) temp = 0.0f;
    }

    /* Simple biome selection based on temperature and moisture */
    /* This is a simplified 2D lookup - real games would use a biome table */
    if (!config->temperature_ranges || !config->moisture_ranges) {
        /* Default: use combined value */
        float combined = (temp + moist) * 0.5f;
        return (int)(combined * (float)config->biome_count) % config->biome_count;
    }

    /* Find temperature bracket */
    int temp_idx = config->biome_count - 1;
    for (int i = 0; i < config->biome_count - 1; i++) {
        if (temp < config->temperature_ranges[i]) {
            temp_idx = i;
            break;
        }
    }

    /* Find moisture bracket */
    int moist_idx = config->biome_count - 1;
    for (int i = 0; i < config->biome_count - 1; i++) {
        if (moist < config->moisture_ranges[i]) {
            moist_idx = i;
            break;
        }
    }

    /* Combine into biome index (simple approach) */
    return (temp_idx + moist_idx) % config->biome_count;
}

float agentite_noise_biome_temperature(const Agentite_Noise *noise, float x, float y,
                                       const Agentite_BiomeConfig *config) {
    if (!noise || !config) return 0.5f;

    float nx = x * config->temperature_scale;
    float ny = y * config->temperature_scale;

    float temp = agentite_noise_fbm2d(noise, nx, ny, &config->temp_fractal);
    return (temp + 1.0f) * 0.5f; /* Normalize to 0-1 */
}

float agentite_noise_biome_moisture(const Agentite_Noise *noise, float x, float y,
                                    const Agentite_BiomeConfig *config) {
    if (!noise || !config) return 0.5f;

    float nx = x * config->moisture_scale + 1000.0f; /* Offset to get different pattern */
    float ny = y * config->moisture_scale + 1000.0f;

    float moist = agentite_noise_fbm2d(noise, nx, ny, &config->moist_fractal);
    return (moist + 1.0f) * 0.5f; /* Normalize to 0-1 */
}

/* ============================================================================
 * Resource Distribution
 * ============================================================================ */

bool agentite_noise_resource_check(const Agentite_Noise *noise, float x, float y,
                                   int biome, const Agentite_ResourceConfig *config) {
    if (!noise || !config) return false;

    /* Check if biome is allowed */
    if (config->allowed_biomes && config->allowed_biome_count > 0) {
        bool allowed = false;
        for (int i = 0; i < config->allowed_biome_count; i++) {
            if (config->allowed_biomes[i] == biome) {
                allowed = true;
                break;
            }
        }
        if (!allowed) return false;
    }

    /* Sample clustering noise */
    float nx = x * config->cluster_scale;
    float ny = y * config->cluster_scale;

    float cluster_value = agentite_noise_fbm2d(noise, nx, ny, &config->fractal);
    cluster_value = (cluster_value + 1.0f) * 0.5f;

    /* Check against threshold */
    return cluster_value > config->cluster_threshold;
}

float agentite_noise_resource_richness(const Agentite_Noise *noise, float x, float y,
                                       const Agentite_ResourceConfig *config) {
    if (!noise || !config) return 0.0f;

    float nx = x * config->richness_scale + 500.0f;
    float ny = y * config->richness_scale + 500.0f;

    float richness = agentite_noise_fbm2d(noise, nx, ny, &config->fractal);
    richness = (richness + 1.0f) * 0.5f;

    return richness * config->density;
}

/* ============================================================================
 * Utility Functions
 * ============================================================================ */

float agentite_noise_remap(float value, float in_min, float in_max,
                           float out_min, float out_max) {
    float t = (value - in_min) / (in_max - in_min);
    return out_min + t * (out_max - out_min);
}

float agentite_noise_clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

float agentite_noise_smoothstep(float edge0, float edge1, float x) {
    float t = agentite_noise_clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float agentite_noise_lerp(float a, float b, float t) {
    return a + t * (b - a);
}

float agentite_noise_hash2d(const Agentite_Noise *noise, int x, int y) {
    if (!noise) return 0.0f;
    uint32_t h = noise_hash((uint32_t)(x * 73856093 ^ y * 19349663 ^ noise->seed));
    return (float)h / (float)UINT32_MAX;
}

float agentite_noise_hash3d(const Agentite_Noise *noise, int x, int y, int z) {
    if (!noise) return 0.0f;
    uint32_t h = noise_hash((uint32_t)(x * 73856093 ^ y * 19349663 ^ z * 83492791 ^ noise->seed));
    return (float)h / (float)UINT32_MAX;
}
